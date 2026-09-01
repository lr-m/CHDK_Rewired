#include "camera_info.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_fselect.h"
#include "gui_mbox.h"
#include "module_def.h"
#include "module_load.h"
#include "simple_module.h"
#include "boot_screen.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"


// No image decoder here any more.
//
// This module used to embed stb_image and stb_image_write to convert arbitrary
// JPEG/PNG on the camera. That cost ~35 KB of module and, far worse, needed an
// 18 KB decoder context plus the full decoded frame plus a 320x240 RGB buffer
// plus the encoder - megabytes, on a body with a few hundred KB free. It is
// what made "Import JPG/PNG" crash.
//
// Conversion now happens on a desktop with tools/mkbootjpg.py, which produces a
// file already in the exact form the slot wants, and this module just copies
// bytes into DISKBOOT.BIN. See boot_ready_jpeg() for what it insists on.

static int running;
static char result_msg[160];





static const char *base_name(const char *path)
{
    const char *p, *base = path;
    for (p = path; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    return base;
}

static unsigned char diskboot_dance(unsigned char v, unsigned pos)
{
    // Canon's encoder restarts the position-dependent transform for each
    // 0x400-byte chunk (matching tools/dancingbits.c).
    pos &= 0x3ff;
    if ((pos % 3) != 0) return v ^ 0xff;
    if ((pos & 1) == 0) return v ^ 0xa0;
    return (unsigned char)((v >> 4) | (v << 4));
}

// How this camera's DISKBOOT.BIN is encoded, and which slot marker it carries.
//
// NEED_ENCODED_DISKBOOT in platform/<cam>/sub/<fw>/makefile.inc selects the
// permutation, and the value is the 1-based row of _chr_[] in
// tools/dancingbits.h. A camera that does not set it at all - every VxWorks
// body here - writes DISKBOOT.BIN as plain bytes, which is `enc_version 0`.
//
//   a430  (none)  plain          a460  (none)  plain
//   a470  1       original       a480  2       nacho cheese
//
// Modules cannot include camera.h, so this is selected from camera_info at
// runtime - the same pattern modules/bend_picture.c uses for its viewport
// addresses.
typedef struct {
    const char *platform;
    const char *tag;            // the 4 marker characters
    int enc_version;            // 0 = unencoded, else NEED_ENCODED_DISKBOOT
    unsigned capacity;          // bytes reserved for jpeg[] in that slot
} boot_slot_cfg_t;

// capacity must match what tools/mkbootslot.py reserved for that camera. It is
// read back out of the slot as well and the two are required to agree, so a
// header regenerated at a different size fails the import rather than writing
// past the slot into whatever follows it.
static const boot_slot_cfg_t boot_slot_cfgs[] = {
    { "a430", "A430", 0, 18426 },
    { "a460", "A460", 0, 18426 },
    { "a470", "A470", 1, 17500 },   // smaller: this core has no room for more
    { "a480", "A480", 2,  7168 },   // core slot, sized to AgentRAM budget
};

// _chr_[] from tools/dancingbits.h, rows 1 and 2 - the only ones any camera
// here uses. Indexed by enc_version-1.
static const unsigned char diskboot_perms[2][8] = {
    { 4,6,1,0,7,2,5,3 },        // version 1 - original flavor
    { 5,3,6,1,2,7,0,4 },        // version 2 - nacho cheese
};

static const boot_slot_cfg_t *boot_slot_cfg(void)
{
    unsigned i;
    for (i = 0; i < sizeof(boot_slot_cfgs)/sizeof(boot_slot_cfgs[0]); i++)
        if (!strcmp(camera_info.platform, boot_slot_cfgs[i].platform))
            return &boot_slot_cfgs[i];
    return 0;
}

// Decode/encode one raw eight-byte group. DISKBOOT has one unencoded prefix
// byte before group zero. `dance` is its own inverse, so decoding and encoding
// differ only in which side of the permutation is indexed.
static void diskboot_decode_group(const unsigned char *enc, unsigned char *raw,
                                  unsigned raw_pos, const unsigned char *perm)
{
    int i;
    for (i = 0; i < 8; i++) raw[i] = diskboot_dance(enc[perm[i]], raw_pos+i);
}

static void diskboot_encode_group(const unsigned char *raw, unsigned char *enc,
                                  unsigned raw_pos, const unsigned char *perm)
{
    int i;
    for (i = 0; i < 8; i++) enc[perm[i]] = diskboot_dance(raw[i], raw_pos+i);
}

// DISKBOOT.BIN is rewritten a chunk at a time, not a group at a time.
//
// The file is ~187 KB, which is 23398 eight-byte dancing-bits groups. Reading
// and writing one group per fread/fwrite meant ~47000 stdio calls to install an
// image, and the read-back verification below added another 23000 on top. On a
// DryOS FAT stream that is enough work to look like a hang. One 4 KB buffer
// turns the whole thing into ~140 calls.
//
// The transform is still per group: the permutation and the position-dependent
// dance both work on eight raw bytes at a time, and the position is the offset
// within the file's 0x400-byte chunk. Only the I/O is batched.
#define BOOT_IO_CHUNK 4096              /* multiple of 8 */
static unsigned char boot_io_buf[BOOT_IO_CHUNK];

// Transform one buffer of groups in place, and hand each raw byte to `visit`.
// Returns the number of whole groups processed.
typedef void (*boot_byte_fn)(unsigned char *b, void *ctx);

static int boot_walk_chunk(unsigned char *buf, int n, unsigned *raw_pos,
                           const unsigned char *perm, boot_byte_fn visit,
                           void *ctx)
{
    int g, i, groups = n / 8;
    for (g = 0; g < groups; g++) {
        unsigned char *e = buf + g * 8;
        unsigned char raw[8];
        if (perm) diskboot_decode_group(e, raw, *raw_pos, perm);
        else      memcpy(raw, e, 8);
        for (i = 0; i < 8; i++) visit(&raw[i], ctx);
        if (perm) diskboot_encode_group(raw, e, *raw_pos, perm);
        else      memcpy(e, raw, 8);
        *raw_pos += 8;
    }
    return groups;
}

// --- the install pass ------------------------------------------------------
typedef struct {
    const unsigned char *marker;
    const unsigned char *jpeg;
    unsigned jpeg_size, capacity;
    unsigned match, patch_pos, seen_capacity;
    int found;
} boot_patch_ctx;

static void boot_patch_byte(unsigned char *b, void *vctx)
{
    boot_patch_ctx *c = (boot_patch_ctx *)vctx;
    if (!c->found) {
        if (*b == c->marker[c->match]) c->match++;
        else c->match = (*b == c->marker[0]) ? 1 : 0;
        if (c->match == 16) c->found = 1;
    } else if (c->patch_pos < 8 + c->capacity) {
        unsigned k = c->patch_pos;
        if (k < 4)                                  // capacity: read, never rewrite
            c->seen_capacity |= ((unsigned)*b) << (k * 8);
        else if (k < 8)
            *b = (unsigned char)((c->jpeg_size >> ((k - 4) * 8)) & 0xff);
        else
            *b = (k - 8 < c->jpeg_size) ? c->jpeg[k - 8] : 0xff;
        c->patch_pos++;
    }
}

// --- the verification pass -------------------------------------------------
typedef struct {
    const unsigned char *marker;
    const unsigned char *jpeg;
    unsigned jpeg_size, capacity;
    unsigned match, got, size_seen, cap_seen;
    int found, bad;
} boot_check_ctx;

static void boot_check_byte(unsigned char *b, void *vctx)
{
    boot_check_ctx *c = (boot_check_ctx *)vctx;
    if (!c->found) {
        if (*b == c->marker[c->match]) c->match++;
        else c->match = (*b == c->marker[0]) ? 1 : 0;
        if (c->match == 16) c->found = 1;
    } else if (c->got < 8 + c->capacity) {
        unsigned k = c->got;
        if (k < 4)                    c->cap_seen  |= ((unsigned)*b) << (k * 8);
        else if (k < 8)               c->size_seen |= ((unsigned)*b) << ((k - 4) * 8);
        else if (k - 8 < c->jpeg_size && *b != c->jpeg[k - 8]) c->bad = 1;
        c->got++;
    }
}

// Read the slot back out of a written DISKBOOT and check it says what we meant
// it to say. Called on the temporary file *before* it replaces the real one.
//
// Worth the second pass: the alternative is discovering the write went wrong at
// the next power-on, by which point the working boot file has been renamed away.
// A short write, a full card or a bad sector all land here as "refused".
static int verify_slot_in_file(const char *path, const unsigned char *marker,
                               const unsigned char *jpeg, unsigned jpeg_size,
                               unsigned cfg_capacity, const unsigned char *perm)
{
    FILE *f = fopen(path, "rb");
    boot_check_ctx c;
    unsigned raw_pos = 0;
    unsigned char prefix;
    int n, ok = 0;

    if (!f) return 0;
    memset(&c, 0, sizeof(c));
    c.marker = marker; c.jpeg = jpeg;
    c.jpeg_size = jpeg_size; c.capacity = cfg_capacity;

    if (perm && fread(&prefix, 1, 1, f) != 1) goto out;
    while ((n = fread(boot_io_buf, 1, BOOT_IO_CHUNK, f)) > 0) {
        boot_walk_chunk(boot_io_buf, n, &raw_pos, perm, boot_check_byte, &c);
        if (c.bad) goto out;
    }
    ok = c.found && !c.bad && c.got == 8 + cfg_capacity &&
         c.cap_seen == cfg_capacity && c.size_seen == jpeg_size;
out:
    fclose(f);
    return ok;
}

static int install_early_jpeg(const unsigned char *jpeg, unsigned jpeg_size,
                              unsigned cfg_capacity)
{
    static const char original[] = "A/DISKBOOT.BIN";
    static const char temporary[] = "A/DISKBNEW.BIN";
    static const char backup[] = "A/DISKBOOT.BAK";
    const boot_slot_cfg_t *cfg = boot_slot_cfg();
    const unsigned char *perm;
    unsigned char marker[16];
    FILE *src = 0, *dst = 0;
    boot_patch_ctx pc;
    unsigned raw_pos = 0;
    int found = 0, ok = 0, n, j;
    unsigned char prefix;

    if (!jpeg || jpeg_size < 4 || jpeg_size > cfg_capacity) return 0;
    if (!cfg) return 0;

    // "BOOTSLOT" <tag> "IMG1" - the same 16 bytes tools/mkbootslot.py writes and
    // include/boot_screen.h checks. Built here rather than stored so the
    // per-camera tag is the only thing that varies.
    memcpy(marker, "BOOTSLOT", 8);
    for (j = 0; j < 4; j++) marker[8+j] = (unsigned char)cfg->tag[j];
    memcpy(marker+12, "IMG1", 4);

    perm = cfg->enc_version ? diskboot_perms[cfg->enc_version - 1] : 0;

    remove(temporary);
    src = fopen(original, "rb");
    dst = fopen(temporary, "wb");
    if (!src || !dst) goto done;

    // Rewrite the whole file sequentially rather than seeking to the slot:
    // random read/write seeks on this FAT stream can silently leave the write
    // cursor thousands of bytes from the requested location.
    //
    // On an encoded DISKBOOT the first byte is not encoded and every following
    // group of eight is. On an unencoded one (the VxWorks bodies) there is no
    // prefix and no transform - only the transform is switched off, so the same
    // buffered loop serves both.
    if (perm) {
        if (fread(&prefix, 1, 1, src) != 1 ||
            fwrite(&prefix, 1, 1, dst) != 1) goto done;
    }
    memset(&pc, 0, sizeof(pc));
    pc.marker = marker; pc.jpeg = jpeg;
    pc.jpeg_size = jpeg_size; pc.capacity = cfg_capacity;

    while ((n = fread(boot_io_buf, 1, BOOT_IO_CHUNK, src)) > 0) {
        boot_walk_chunk(boot_io_buf, n, &raw_pos, perm, boot_patch_byte, &pc);
        // Any tail shorter than a group is carried through untouched; the slot
        // is far from the end, so nothing patchable can live there.
        if ((int)fwrite(boot_io_buf, 1, n, dst) != n) goto done;
    }
    found = pc.found;

    if (!found || pc.patch_pos != 8 + cfg_capacity) goto done;
    if (pc.seen_capacity != cfg_capacity) goto done;
    fclose(src); src = 0;
    if (fclose(dst) != 0) { dst = 0; goto done; }
    dst = 0;

    // Read the replacement back before trusting it with the boot file. A short
    // write, a full card or a bad sector otherwise only shows up at the next
    // power-on, after the working DISKBOOT.BIN has already been renamed away.
    if (!verify_slot_in_file(temporary, marker, jpeg, jpeg_size,
                             cfg_capacity, perm))
        goto done;

    // Keep the previous boot file recoverable until the replacement is in
    // place. If the second rename fails, restore it immediately.
    remove(backup);
    if (rename(original, backup) != 0) goto done;
    if (rename(temporary, original) != 0) {
        rename(backup, original);
        goto done;
    }
    ok = 1;
done:
    if (src) fclose(src);
    if (dst) fclose(dst);
    if (!ok) remove(temporary);
    return ok;
}

// Is this file already exactly what the slot wants?
//
// A boot-ready JPEG is 320x240 baseline, 3 components, standard Huffman tables,
// and small enough for this camera's slot - which is what tools/mkbootjpg.py
// produces. When a file is already that, importing it is a byte copy: no
// decode, no resize, no re-encode, no megabyte buffers.
//
// That is now the only path. The importer used to convert arbitrary images on
// the camera, and it is what made this feature unreliable - stb's decoder alone
// wants an 18 KB context plus the full RGB frame, on a body with a few hundred
// KB free. Converting on a desktop, where memory is free, and leaving the camera
// to copy bytes, removes the whole failure surface.
//
// Checked, not assumed, because the firmware decoder renders anything it cannot
// parse as a black screen with no way back except pulling the card.
static int boot_ready_jpeg(const unsigned char *d, unsigned n, char *why)
{
    unsigned i = 2;

    if (n < 4 || d[0] != 0xff || d[1] != 0xd8) {
        strcpy(why, "not a JPEG");
        return 0;
    }
    if (d[n-2] != 0xff || d[n-1] != 0xd9) {
        strcpy(why, "JPEG is truncated");
        return 0;
    }
    while (i + 3 < n) {
        unsigned char m;
        unsigned len;
        if (d[i] != 0xff) { strcpy(why, "corrupt JPEG structure"); return 0; }
        m = d[i+1];
        if (m == 0xd8 || m == 0x01 || (m >= 0xd0 && m <= 0xd7)) { i += 2; continue; }
        len = ((unsigned)d[i+2] << 8) | d[i+3];
        if (len < 2 || i + 2 + len > n) { strcpy(why, "corrupt JPEG structure"); return 0; }
        if (m == 0xc2) { strcpy(why, "progressive JPEG - must be baseline"); return 0; }
        if (m == 0xc0) {                       // baseline SOF
            unsigned h, w;
            if (len < 8) { strcpy(why, "corrupt JPEG header"); return 0; }
            h = ((unsigned)d[i+5] << 8) | d[i+6];
            w = ((unsigned)d[i+7] << 8) | d[i+8];
            if (w != BOOT_SCREEN_WIDTH || h != BOOT_SCREEN_HEIGHT) {
                sprintf(why, "is %ux%u, needs %dx%d", w, h,
                        BOOT_SCREEN_WIDTH, BOOT_SCREEN_HEIGHT);
                return 0;
            }
            if (d[i+9] != 3) { strcpy(why, "not a colour JPEG"); return 0; }
            return 1;                          // everything we can check, checked
        }
        if (m == 0xda) break;                  // SOS before SOF: no dimensions
        i += 2 + len;
    }
    strcpy(why, "no image header found");
    return 0;
}

static int import_image(const char *name)
{
    struct stat st;
    FILE *in = 0;
    unsigned char *data = 0;
    char why[64];
    int ok = 0;
    const boot_slot_cfg_t *cfg = boot_slot_cfg();

    if (!cfg) {
        strcpy(result_msg, "This camera has no boot screen slot");
        goto done;
    }
    if (!name || stat(name, &st) != 0 || st.st_size <= 0) {
        strcpy(result_msg, "Could not read that file");
        goto done;
    }
    if ((unsigned)st.st_size > cfg->capacity) {
        sprintf(result_msg,
                "Too big: %d bytes, slot holds %u.\nConvert it first - see readme.txt",
                (int)st.st_size, cfg->capacity);
        goto done;
    }

    data = malloc(st.st_size);
    in = fopen(name, "rb");
    if (!data || !in || fread(data, st.st_size, 1, in) != 1) {
        strcpy(result_msg, "Could not read that file");
        goto done;
    }
    fclose(in); in = 0;

    if (!boot_ready_jpeg(data, (unsigned)st.st_size, why)) {
        sprintf(result_msg, "%s: %s.\nConvert it first - see readme.txt",
                base_name(name), why);
        goto done;
    }

    draw_progress_bar("Import boot screen", 50);
    if (!install_early_jpeg(data, (unsigned)st.st_size, cfg->capacity)) {
        strcpy(result_msg, "Could not update DISKBOOT.BIN - boot file unchanged");
        goto done;
    }
    draw_progress_bar("Import boot screen", 100);
    sprintf(result_msg, "Installed for next boot:\n%s (%d bytes)",
            base_name(name), (int)st.st_size);
    ok = 1;
done:
    if (in) fclose(in);
    if (data) free(data);
    return ok;
}

static void dialog_done(unsigned button)
{
    (void)button;
    running = 0;
}

static void selected(const char *name)
{
    if (!name) { running = 0; return; }
    import_image(name);
    gui_mbox_init((int)"Boot screen", (int)result_msg,
                  MBOX_BTN_OK | MBOX_TEXT_CENTER | MBOX_FUNC_RESTORE, dialog_done);
}

int _run(void)
{
    running = 1;
    libfselect->file_select((int)"Import JPG or PNG", "A", "A", selected);
    return 0;
}
int _module_can_unload(void) { return !running; }
int _module_exit_alt(void) { running = 0; return 0; }

libsimple_sym _libbootimage = {{ 0, 0, _module_can_unload, _module_exit_alt, _run }};
ModuleInfo _module_info = {
    MODULEINFO_V1_MAGICNUM, sizeof(ModuleInfo), {1,0},
    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE, ANY_PLATFORM_ALLOWED,
    (int32_t)"Boot image importer", MTYPE_TOOL, &_libbootimage.base,
    ANY_VERSION, CAM_SCREEN_VERSION, ANY_VERSION, ANY_VERSION, 0
};
