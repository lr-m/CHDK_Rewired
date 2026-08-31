//-------------------------------------------------------------------
// Cartridge RAM persistence and photo export - see gbc_save.h.
//-------------------------------------------------------------------

#include "gbc_save.h"

#ifdef GBC_HOST_TEST
/* CHDK's mkdir takes a path only; POSIX wants a mode as well. */
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#define mkdir(p)    mkdir((p), 0777)
#else
#include "stdlib.h"
#include "dirent.h"       // opendir/readdir, for numbering the DCIM copies
#endif

#define GBC_SRAM_BYTES  (GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE)

// Both live with the export code below; gbc_save_load() needs them to mark the
// album it just read as already exported.
static int slot_is_blank(const unsigned char *tiles);
static unsigned short slot_checksum(const unsigned char *tiles);

//-------------------------------------------------------------------
// The Game Boy's four tones, lightest first.
//
// One ramp per screen tint, so the file on the card is the picture that was on
// the screen. The tint is a viewing choice and the sensor data is the same
// either way, but a photograph that comes out green when the camera was set to
// red is simply the wrong photograph.
//
// Green is the real DMG ramp and stays exactly as it was. The rest follow the
// same shape - a pale tone, the hue itself, a dark one, and near-black - to
// match the blended palette the screen builds them from (gbc_emu.c). Written
// as B,G,R because that is the order a BMP pixel wants, and in the same order
// as gbc_hues[] there.
#define GBC_PHOTO_HUES  6

static const unsigned char gbc_photo_pal[GBC_PHOTO_HUES][4][3] = {
    { { 0x0f, 0xbc, 0x9b }, { 0x0f, 0xac, 0x8b },
      { 0x30, 0x62, 0x30 }, { 0x0f, 0x38, 0x0f } },   /* Green - DMG */
    { { 0xe0, 0xe0, 0xe0 }, { 0xa8, 0xa8, 0xa8 },
      { 0x58, 0x58, 0x58 }, { 0x10, 0x10, 0x10 } },   /* Grey - Pocket */
    { { 0xa0, 0xa0, 0xe8 }, { 0x50, 0x50, 0xc0 },
      { 0x20, 0x20, 0x80 }, { 0x08, 0x08, 0x20 } },   /* Red */
    { { 0xa0, 0xe0, 0xf0 }, { 0x40, 0xb8, 0xd0 },
      { 0x18, 0x68, 0x80 }, { 0x08, 0x1a, 0x20 } },   /* Yellow */
    { { 0xe8, 0xc0, 0xa8 }, { 0xc0, 0x70, 0x48 },
      { 0x70, 0x30, 0x20 }, { 0x1f, 0x10, 0x08 } },   /* Blue */
    { { 0xa0, 0xc8, 0xf0 }, { 0x30, 0x88, 0xd0 },
      { 0x10, 0x48, 0x88 }, { 0x04, 0x10, 0x20 } },   /* Orange */
};

// Which ramp the exporter uses. Set from gbc_set_hue() rather than read from
// conf here, so this file stays free of CHDK headers and the host self-tests
// go on building.
static int gbc_photo_hue = 0;

void gbc_photo_set_hue(int h)
{
    gbc_photo_hue = (h >= 0 && h < GBC_PHOTO_HUES) ? h : 0;
}

//-------------------------------------------------------------------
// Path building. No snprintf in this port (gbc_port.h), so by hand.

static char *sv_str(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

// "A/CHDK/GBC/GBCAMERA.GB" -> "GBCAMERA"
static char *sv_stem(const char *rom_path, char *out, int max)
{
    const char *base = rom_path, *q;
    int n = 0;

    for (q = rom_path; *q; q++)
        if (*q == '/' || *q == '\\')
            base = q + 1;

    for (q = base; *q && *q != '.' && n < max - 1; q++)
        out[n++] = *q;
    out[n] = 0;
    return out;
}

void gbc_save_path(const char *rom_path, char *out)
{
    char stem[32];
    char *p = out;

    sv_stem(rom_path, stem, sizeof(stem));
    p = sv_str(p, GBC_SAVE_DIR);
    p = sv_str(p, "/");
    p = sv_str(p, stem);
    p = sv_str(p, ".SAV");
    *p = 0;
}

//-------------------------------------------------------------------
// Write the album now, because the ROM just said it had finished writing.
//
// Called from the mapper (gbc_cam.c) on the RAM-disable that a cartridge uses
// as its commit signal, so a photograph is on the card seconds after it is
// taken rather than only if the player exits cleanly. Guarded by `dirty` so a
// ROM that toggles the RAM enable without writing anything - which the camera
// does constantly - does not trigger a 128K card write each time.

static int write_sav(gbc_cam_t *c)
{
    int fd, n;

    mkdir(GBC_SAVE_DIR);

    fd = open(c->save_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
        return -1;                  // leave dirty set; try again next time

    n = write(fd, c->sram, GBC_SRAM_BYTES);
    close(fd);

    return (n == GBC_SRAM_BYTES) ? 0 : -1;
}

//-------------------------------------------------------------------
// Ordinary cartridge battery RAM - see the note in gbc_save.h.

int gbc_save_ram_read(const char *save_path, unsigned char *ram, int len)
{
    int fd, n;

    if (!save_path || !save_path[0] || !ram || len <= 0)
        return -1;

    fd = open(save_path, O_RDONLY, 0777);
    if (fd < 0)
        return -1;                  // never saved; the RAM stays as it was

    n = read(fd, ram, len);
    close(fd);

    // A short file is honoured as far as it goes rather than refused. Cartridge
    // RAM sizes are declared by the ROM header and a .SAV from another emulator
    // may have been written for a different declared size; the overlap is still
    // the player's save.
    return (n > 0) ? 0 : -1;
}

int gbc_save_ram_write(const char *save_path, const unsigned char *ram, int len)
{
    int fd, n;

    if (!save_path || !save_path[0] || !ram || len <= 0)
        return -1;

    mkdir(GBC_SAVE_DIR);

    fd = open(save_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
        return -1;

    n = write(fd, ram, len);
    close(fd);

    return (n == len) ? 0 : -1;
}

//-------------------------------------------------------------------
// Handing slots back - see the note in gbc_save.h.

// One line per recycle in A/CHDK/LOGS/GBCALBUM.TXT.
//
// Worth keeping because the interesting number - whether the working copy in
// WRAM was found - cannot be seen from the card afterwards. A recycle that
// frees slots but patches nothing means the search below stopped matching, and
// the counter on screen will be going down again.
static void album_log(int used, int freed, int hits)
{
#ifndef GBC_HOST_TEST
    char line[64];
    int fd;

    mkdir("A/CHDK/LOGS");
    fd = open("A/CHDK/LOGS/GBCALBUM.TXT", O_WRONLY|O_CREAT|O_APPEND, 0777);
    if (fd < 0) return;
    sprintf(line, "used %d freed %d wram %d\n", used, freed, hits);
    write(fd, line, strlen(line));
    close(fd);
#else
    (void)used; (void)freed; (void)hits;
#endif
}

// Patch the ROM's working copy of the album index.
//
// The cartridge copy is only half of it. The ROM reads its index into work RAM
// when it boots and drives the "pictures remaining" count from there, writing
// it back to the cartridge when it saves - so a trim that only touches SRAM is
// invisible until the next power-on, and the counter keeps walking down.
//
// There is no documented address for the copy, so it is found by its contents:
// the exact thirty bytes that were in the cartridge a moment ago. Thirty bytes
// ending in a run of 0xff do not occur by accident, and if no match turns up
// nothing is written. Returns how many copies were patched.
static int album_patch_wram(gbc_cam_t *c, const unsigned char *before,
                            const unsigned char *after, int used)
{
    int i, j, hits = 0, limit;

    if (!c->wram || c->wram_bytes < GBC_ALBUM_LEN) return 0;
    limit = c->wram_bytes - GBC_ALBUM_LEN;

    for (i = 0; i <= limit; i++)
        if (memcmp(c->wram + i, before, GBC_ALBUM_LEN) == 0)
        {
            memcpy(c->wram + i, after, GBC_ALBUM_LEN);
            hits++;
            i += GBC_ALBUM_LEN - 1;
        }
    if (hits) return hits;

    // Failing that, the ROM may hold only the entries themselves with no 0xff
    // padding after them. Match just the used run - still a specific thing to
    // find at five entries or more - and pad the replacement to the same
    // length so nothing beyond it moves.
    if (used < 5) return 0;
    limit = c->wram_bytes - used;

    for (i = 0; i <= limit; i++)
        if (memcmp(c->wram + i, before, used) == 0)
        {
            for (j = 0; j < used; j++)
                c->wram[i + j] = after[j];
            hits++;
            i += used - 1;
        }
    return hits;
}

int gbc_album_recycle(gbc_cam_t *c)
{
    unsigned char *tab, keep[GBC_ALBUM_LEN];
    unsigned char before[GBC_ALBUM_LEN], after[GBC_ALBUM_LEN];
    int i, n = 0, drop, freed = 0;

    if (!c || !c->sram) return 0;
    if (GBC_ALBUM_TAB2 + GBC_ALBUM_LEN > GBC_SRAM_BYTES) return 0;

    tab = c->sram + GBC_ALBUM_TAB;
    memcpy(before, tab, GBC_ALBUM_LEN);

    // What is in the album now, oldest first.
    for (i = 0; i < GBC_ALBUM_LEN; i++)
        if (tab[i] != 0xff && tab[i] < GBC_PHOTO_SLOTS)
            keep[n++] = tab[i];

    if (n <= GBC_ALBUM_KEEP) return 0;
    drop = n - GBC_ALBUM_KEEP;

    // Only let go of pictures that are already on the card. slot_sum is set
    // when a slot is exported, so a zero there means this session has never
    // written that photograph out and freeing it would lose it.
    while (drop > 0 && c->slot_sum[keep[freed]] != 0)
    {
        freed++;
        drop--;
    }
    if (freed == 0) return 0;

    // Compacted, the way the table already reads: the survivors in order and
    // 0xff for the rest. Both copies, because the cartridge keeps two.
    for (i = 0; i < GBC_ALBUM_LEN; i++)
    {
        unsigned char v = (i < n - freed) ? keep[freed + i] : 0xff;
        after[i] = v;
        c->sram[GBC_ALBUM_TAB  + i] = v;
        c->sram[GBC_ALBUM_TAB2 + i] = v;
    }

    // And the copy the ROM is actually counting from.
    c->wram_hits = album_patch_wram(c, before, after, n);
    album_log(n, freed, c->wram_hits);

    // The freed slots are about to be shot over, and their checksums must not
    // claim the new picture is the old one.
    for (i = 0; i < freed; i++)
        c->slot_sum[keep[i]] = 0;

    c->dirty = 1;
    return freed;
}

int gbc_save_service(gbc_cam_t *c)
{
    if (!c || !c->sram || !c->save_path[0] || !c->commit_pending)
        return 0;

    // Step one: the album itself. This is the artifact that matters, so it
    // goes first - if the body loses power midway through the photo exports,
    // the pictures are still safe in the .SAV.
    if (c->dirty)
    {
        if (write_sav(c) == 0)
            c->dirty = 0;
        else
            c->commit_pending = 0;  // card refused; do not spin on it
        return c->commit_pending;
    }

    // Then one photo per call. Thirty 43K files in a single callback is
    // exactly the kind of stall that took the camera down; one at a time keeps
    // each frame's I/O bounded and the display fed.
    while (c->export_slot < GBC_PHOTO_SLOTS)
    {
        int slot = c->export_slot++;
        if (gbc_save_export_slot(c, slot))
            return 1;               // wrote one; more may remain
    }

    // Everything is on the card, so the oldest of it no longer needs a slot.
    // Leaves the album dirty and the commit open, so the next call writes the
    // .SAV with the shorter list in it.
    if (gbc_album_recycle(c))
        return 1;

    c->commit_pending = 0;
    return 0;
}

void gbc_save_flush(gbc_cam_t *c)
{
    int guard = GBC_PHOTO_SLOTS + 4;    // bounded, so a failing card cannot hang
    while (guard-- > 0 && gbc_save_service(c))
        ;
}

//-------------------------------------------------------------------

void gbc_save_load(gbc_cam_t *c, const char *rom_path)
{
    char path[128];
    int fd, bytes_read;

    if (!c || !c->sram)
        return;

    gbc_save_path(rom_path, path);

    fd = open(path, O_RDONLY, 0777);
    if (fd < 0)
        return;                     // first run, empty cartridge - not an error

    // A short read leaves the rest of SRAM as the zeros gbc_cam_init() left,
    // which is what an unwritten album looks like anyway.
    bytes_read = read(fd, c->sram, GBC_SRAM_BYTES);
    close(fd);
    (void)bytes_read;

    // Everything in the album as it arrives has already been photographed, and
    // in an earlier session it was already copied into DCIM. Without this the
    // first commit of every session would see thirty slots it has no checksum
    // for, decide they are all new, and write the whole album into DCIM again -
    // a fresh set of duplicates every time the emulator is opened.
    {
        int slot;
        for (slot = 0; slot < GBC_PHOTO_SLOTS; slot++)
        {
            const unsigned char *tiles = c->sram + GBC_PHOTO_SLOT_BASE
                                       + slot * GBC_PHOTO_SLOT_STRIDE;
            if (GBC_PHOTO_SLOT_BASE + slot * GBC_PHOTO_SLOT_STRIDE
                    + GBCAM_IMAGE_BYTES > GBC_SRAM_BYTES)
                break;
            c->slot_sum[slot] = slot_is_blank(tiles) ? 0 : slot_checksum(tiles);
        }
    }
}

int gbc_save_store(gbc_cam_t *c, const char *rom_path)
{
    char path[128];
    int fd, n;

    if (!c || !c->sram)
        return -1;

    // Ignore the result: it fails when the directory already exists, which is
    // the normal case.
    mkdir(GBC_SAVE_DIR);

    gbc_save_path(rom_path, path);

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
        return -1;

    n = write(fd, c->sram, GBC_SRAM_BYTES);
    close(fd);

    return (n == GBC_SRAM_BYTES) ? 0 : -1;
}

//-------------------------------------------------------------------
// 2bpp tile data to one byte per pixel.
//
// Standard Game Boy packing: two bytes per tile row, one bit of each per
// pixel, low plane first, leftmost pixel in bit 7. Tiles run in reading order,
// 16 across. Identical to what gbc_cam.c writes for the live capture, so this
// decodes either.

void gbc_photo_decode(const unsigned char *tiles, unsigned char *out)
{
    int ty, tx, py, px;

    for (ty = 0; ty < GBCAM_IMAGE_TILES_Y; ty++)
        for (tx = 0; tx < GBCAM_IMAGE_TILES_X; tx++)
        {
            const unsigned char *t =
                tiles + (ty * GBCAM_IMAGE_TILES_X + tx) * 16;

            for (py = 0; py < 8; py++)
            {
                unsigned b1 = t[py * 2];
                unsigned b2 = t[py * 2 + 1];
                unsigned char *o = out + (ty * 8 + py) * GBCAM_W + tx * 8;

                for (px = 0; px < 8; px++)
                {
                    unsigned shift = 7 - px;
                    o[px] = (unsigned char)(((b1 >> shift) & 1) |
                                           (((b2 >> shift) & 1) << 1));
                }
            }
        }
}

//-------------------------------------------------------------------
// 24-bit BMP. Uncompressed and bottom-up, which is the form every viewer
// accepts without argument. 128*3 is already a multiple of 4, so no row
// padding is needed - asserted by the host test rather than assumed.

static unsigned char *bmp_u16(unsigned char *p, unsigned v)
{
    *p++ = (unsigned char)(v & 0xff);
    *p++ = (unsigned char)((v >> 8) & 0xff);
    return p;
}

static unsigned char *bmp_u32(unsigned char *p, unsigned v)
{
    *p++ = (unsigned char)(v & 0xff);
    *p++ = (unsigned char)((v >> 8) & 0xff);
    *p++ = (unsigned char)((v >> 16) & 0xff);
    *p++ = (unsigned char)((v >> 24) & 0xff);
    return p;
}

int gbc_photo_to_bmp(const unsigned char *shades, unsigned char *buf)
{
    unsigned pixels = GBCAM_W * GBCAM_H * 3;
    unsigned char *p = buf;
    int y, x;

    *p++ = 'B'; *p++ = 'M';
    p = bmp_u32(p, GBC_BMP_HEADER + pixels);    /* file size   */
    p = bmp_u16(p, 0);
    p = bmp_u16(p, 0);
    p = bmp_u32(p, GBC_BMP_HEADER);             /* pixel offset */

    p = bmp_u32(p, 40);                         /* DIB header size */
    p = bmp_u32(p, GBCAM_W);
    p = bmp_u32(p, GBCAM_H);                    /* positive = bottom-up */
    p = bmp_u16(p, 1);                          /* planes */
    p = bmp_u16(p, 24);                         /* bits per pixel */
    p = bmp_u32(p, 0);                          /* BI_RGB, no compression */
    p = bmp_u32(p, pixels);
    p = bmp_u32(p, 2835);                       /* ~72 dpi, x */
    p = bmp_u32(p, 2835);                       /* ~72 dpi, y */
    p = bmp_u32(p, 0);
    p = bmp_u32(p, 0);

    for (y = GBCAM_H - 1; y >= 0; y--)          /* bottom row first */
    {
        const unsigned char *row = shades + y * GBCAM_W;
        for (x = 0; x < GBCAM_W; x++)
        {
            const unsigned char *c = gbc_photo_pal[gbc_photo_hue][row[x] & 3];
            *p++ = c[0];
            *p++ = c[1];
            *p++ = c[2];
        }
    }

    return (int)(p - buf);
}

//-------------------------------------------------------------------

static int slot_is_blank(const unsigned char *tiles)
{
    int i;
    unsigned char first = tiles[0];

    // A slot that was never written is uniform - all 0x00 on a fresh cartridge.
    // Anything with variation in it is a picture.
    for (i = 1; i < GBCAM_IMAGE_BYTES; i++)
        if (tiles[i] != first)
            return 0;
    return 1;
}

// One slot. Returns 1 if a file was written, 0 if the slot was empty or the
// card refused it.
//
// Heap, not static, for the two buffers: they are 14K and 43K, and a CHDK
// module's static data is part of an image loaded into a small arena. They are
// also allocated and freed per slot rather than held, so the emulator gets the
// memory back between frames - this runs while a game is resident.
// Cheap checksum over a slot's tile data. Only has to answer "is this the same
// picture as last time", so a Fletcher-style sum is plenty and it costs one
// pass over 3.5K rather than a decode.
static unsigned short slot_checksum(const unsigned char *tiles)
{
    unsigned a = 1, b = 0;
    int i;

    for (i = 0; i < GBCAM_IMAGE_BYTES; i++)
    {
        a += tiles[i];
        b += a;
    }
    a = (unsigned short)((b << 8) ^ a ^ 0x1234);
    // Zero means "never exported", so a real picture must not hash to it.
    return (unsigned short)(a ? a : 1);
}

// The next unused number in GBC_DCIM_DIR.
//
// Found once per session by walking the directory, then counted up from there.
// Walking it every time would be a directory scan per photograph; assuming it
// starts at zero would overwrite the last session's pictures, which is the
// whole thing this directory exists to stop.
static int gbc_dcim_next = -1;

static void gbc_dcim_scan(void)
{
    DIR *d;
    struct dirent *e;
    int hi = -1;

    gbc_dcim_next = 0;

    d = opendir(GBC_DCIM_DIR);
    if (!d) return;

    while ((e = readdir(d)) != 0)
    {
        const char *n = e->d_name;
        int v = 0, i;

        // GBC_nnnn.BMP, and nothing else in here is ours.
        if (n[0] != 'G' || n[1] != 'B' || n[2] != 'C' || n[3] != '_') continue;
        for (i = 4; i < 8; i++)
        {
            if (n[i] < '0' || n[i] > '9') { v = -1; break; }
            v = v * 10 + (n[i] - '0');
        }
        if (v > hi) hi = v;
    }
    closedir(d);

    gbc_dcim_next = hi + 1;
}

// Write the same BMP a second time under a number that is never reused.
static void gbc_dcim_write(const unsigned char *bmp, int len)
{
    char path[128], *p;
    int fd, n;

    if (gbc_dcim_next < 0) gbc_dcim_scan();
    if (gbc_dcim_next < 0 || gbc_dcim_next > 9999) return;

    mkdir("A/DCIM");
    mkdir(GBC_DCIM_DIR);

    n = gbc_dcim_next;
    p = sv_str(path, GBC_DCIM_DIR);
    p = sv_str(p, "/GBC_");
    *p++ = (char)('0' + (n / 1000) % 10);
    *p++ = (char)('0' + (n / 100) % 10);
    *p++ = (char)('0' + (n / 10) % 10);
    *p++ = (char)('0' + n % 10);
    p = sv_str(p, ".BMP");
    *p = 0;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0) return;
    if (write(fd, bmp, len) == len) gbc_dcim_next++;
    close(fd);
}

int gbc_save_export_slot(gbc_cam_t *c, int slot)
{
    const unsigned char *tiles;
    unsigned char *shades, *bmp;
    char path[128], *p;
    int fd, len, ok = 0;
    unsigned short sum;

    if (!c || !c->sram || slot < 0 || slot >= GBC_PHOTO_SLOTS)
        return 0;

    if (GBC_PHOTO_SLOT_BASE + slot * GBC_PHOTO_SLOT_STRIDE
            + GBCAM_IMAGE_BYTES > GBC_SRAM_BYTES)
        return 0;

    tiles = c->sram + GBC_PHOTO_SLOT_BASE + slot * GBC_PHOTO_SLOT_STRIDE;
    if (slot_is_blank(tiles))
        return 0;

    // Unchanged since the last export: not a new photograph, and writing it
    // again would put a duplicate in DCIM every time any other slot moved.
    sum = slot_checksum(tiles);
    if (c->slot_sum[slot] == sum)
        return 0;

    shades = (unsigned char *)malloc(GBCAM_W * GBCAM_H);
    bmp    = (unsigned char *)malloc(GBC_BMP_BYTES);
    if (!shades || !bmp)
    {
        if (shades) free(shades);
        if (bmp)    free(bmp);
        return 0;
    }

    gbc_photo_decode(tiles, shades);
    len = gbc_photo_to_bmp(shades, bmp);

    mkdir(GBC_PHOTO_DIR);

    /* GBC_PHOTO_DIR/GB_00.BMP - named by slot, so re-exporting an album
     * overwrites rather than accumulating duplicates. */
    p = sv_str(path, GBC_PHOTO_DIR);
    p = sv_str(p, "/GB_");
    *p++ = (char)('0' + (slot / 10));
    *p++ = (char)('0' + (slot % 10));
    p = sv_str(p, ".BMP");
    *p = 0;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd >= 0)
    {
        ok = (write(fd, bmp, len) == len);
        close(fd);
    }

    // The copy that outlives the album. Written after the slot-named one so a
    // card that fills up loses the duplicate rather than the original.
    if (ok)
    {
        gbc_dcim_write(bmp, len);
        c->slot_sum[slot] = sum;
    }

    free(shades);
    free(bmp);
    return ok;
}
