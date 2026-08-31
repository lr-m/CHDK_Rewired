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
#include "limits.h"
#include "meminfo.h"

#ifndef INT_MIN
#define INT_MIN (-2147483647-1)
#endif
#ifndef SHRT_MIN
#define SHRT_MIN (-32767-1)
#endif
#ifndef UINT_MAX
#define UINT_MAX 0xffffffffu
#endif

typedef struct { unsigned size; } boot_alloc_header_t;
static unsigned boot_alloc_used;
static unsigned boot_alloc_limit;

static void *boot_stbi_malloc(unsigned size)
{
    boot_alloc_header_t *h;
    if (!size || size > 0x7ffffff0u || size > boot_alloc_limit ||
        boot_alloc_used > boot_alloc_limit - size)
        return 0;
    h = malloc((long)(sizeof(*h) + size));
    if (!h) return 0;
    h->size = size;
    boot_alloc_used += size;
    return h + 1;
}

static void boot_stbi_free(void *p)
{
    boot_alloc_header_t *h;
    if (!p) return;
    h = (boot_alloc_header_t *)p - 1;
    if (boot_alloc_used >= h->size) boot_alloc_used -= h->size;
    free(h);
}

static void *boot_realloc_sized(void *p, unsigned old_size, unsigned new_size)
{
    void *q = boot_stbi_malloc(new_size);
    if (q && p) memcpy(q, p, old_size < new_size ? old_size : new_size);
    if (q && p) boot_stbi_free(p);
    return q;
}

static void *boot_memmove(void *dst, const void *src, unsigned n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 1600
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) boot_stbi_malloc(sz)
#define STBI_FREE(p) boot_stbi_free(p)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) boot_realloc_sized(p,oldsz,newsz)
#define __SYMBIAN32__ 1 /* stb fixed-width typedefs; CHDK builds with -nostdinc */
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(sz) boot_stbi_malloc(sz)
#define STBIW_FREE(p) boot_stbi_free(p)
#define STBIW_REALLOC_SIZED(p,oldsz,newsz) boot_realloc_sized(p,oldsz,newsz)
#define STBIW_MEMMOVE(a,b,sz) boot_memmove(a,b,sz)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb/stb_image_write.h"

static int running;
static char result_msg[160];

typedef struct { unsigned char *p; int n; int overflow; } jpg_sink_t;

static void jpg_write(void *ctx, void *data, int size)
{
    jpg_sink_t *s = (jpg_sink_t *)ctx;
    if (size < 0 || s->n + size > BOOT_SCREEN_MAX_JPEG) {
        s->overflow = 1;
        return;
    }
    memcpy(s->p + s->n, data, size);
    s->n += size;
}

static void resize_contain(const unsigned char *src, int sw, int sh, int comp,
                           unsigned char *dst)
{
    int dw, dh, ox, oy, x, y;
    if (sw * BOOT_SCREEN_HEIGHT > sh * BOOT_SCREEN_WIDTH) {
        dw = BOOT_SCREEN_WIDTH;
        dh = sh * BOOT_SCREEN_WIDTH / sw;
    } else {
        dh = BOOT_SCREEN_HEIGHT;
        dw = sw * BOOT_SCREEN_HEIGHT / sh;
    }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    ox = (BOOT_SCREEN_WIDTH - dw) / 2;
    oy = (BOOT_SCREEN_HEIGHT - dh) / 2;
    memset(dst, 0, BOOT_SCREEN_WIDTH * BOOT_SCREEN_HEIGHT * 3);
    for (y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        for (x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            const unsigned char *p = src + (sy * sw + sx) * comp;
            unsigned char *q = dst + ((y + oy) * BOOT_SCREEN_WIDTH + x + ox) * 3;
            q[0] = p[0]; q[1] = p[1]; q[2] = p[2];
        }
    }
}

static int encode_frame(const unsigned char *rgb, unsigned char *jpg)
{
    int quality;
    for (quality = 88; quality >= 28; quality -= 6) {
        jpg_sink_t s = { jpg, 0, 0 };
        if (stbi_write_jpg_to_func(jpg_write, &s, BOOT_SCREEN_WIDTH,
                                   BOOT_SCREEN_HEIGHT, 3, rgb, quality) &&
            !s.overflow && s.n > 0 && s.n <= BOOT_SCREEN_MAX_JPEG)
            return s.n;
    }
    return 0;
}

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

// A480 uses dancing-bits version 2. Decode/encode one raw eight-byte group;
// DISKBOOT has one unencoded prefix byte before group zero.
static void diskboot_decode_group(const unsigned char *enc, unsigned char *raw,
                                  unsigned raw_pos)
{
    static const unsigned char perm[8] = {5,3,6,1,2,7,0,4};
    int i;
    for (i = 0; i < 8; i++) raw[i] = diskboot_dance(enc[perm[i]], raw_pos+i);
}

static void diskboot_encode_group(const unsigned char *raw, unsigned char *enc,
                                  unsigned raw_pos)
{
    static const unsigned char perm[8] = {5,3,6,1,2,7,0,4};
    int i;
    for (i = 0; i < 8; i++) enc[perm[i]] = diskboot_dance(raw[i], raw_pos+i);
}

static int install_early_jpeg(const unsigned char *jpeg, unsigned jpeg_size)
{
    static const unsigned char marker[16] = {
        'B','O','O','T','S','L','O','T','A','4','8','0','I','M','G','1'
    };
    static const char original[] = "A/DISKBOOT.BIN";
    static const char temporary[] = "A/DISKBNEW.BIN";
    static const char backup[] = "A/DISKBOOT.BAK";
    FILE *src = 0, *dst = 0;
    unsigned char enc[8], raw[8];
    unsigned raw_pos = 0, match = 0, patch_pos = 0;
    unsigned patch_size = 4 + BOOT_SCREEN_MAX_JPEG;
    int found = 0, ok = 0;
    unsigned char prefix;

    if (!jpeg || jpeg_size < 4 || jpeg_size > BOOT_SCREEN_MAX_JPEG) return 0;
    remove(temporary);
    src = fopen(original, "rb");
    dst = fopen(temporary, "wb");
    if (!src || !dst) goto done;

    // The first DISKBOOT byte is not dancing-bits encoded. Rewrite the whole
    // file sequentially: random read/write seeks on this DryOS FAT stream can
    // silently leave the write cursor thousands of bytes from the requested
    // location.
    if (fread(&prefix, 1, 1, src) != 1 ||
        fwrite(&prefix, 1, 1, dst) != 1) goto done;
    while (fread(enc, sizeof(enc), 1, src) == 1) {
        int i;
        diskboot_decode_group(enc, raw, raw_pos);
        for (i = 0; i < 8; i++) {
            if (!found) {
                unsigned char b = raw[i];
                if (b == marker[match]) match++;
                else match = (b == marker[0]) ? 1 : 0;
                if (match == sizeof(marker)) found = 1;
            } else if (patch_pos < patch_size) {
                if (patch_pos < 4)
                    raw[i] = (jpeg_size >> (patch_pos * 8)) & 0xff;
                else {
                    unsigned j = patch_pos - 4;
                    raw[i] = (j < jpeg_size) ? jpeg[j] : 0xff;
                }
                patch_pos++;
            }
        }
        diskboot_encode_group(raw, enc, raw_pos);
        if (fwrite(enc, sizeof(enc), 1, dst) != 1) goto done;
        raw_pos += 8;
    }
    if (!found || patch_pos != patch_size) goto done;
    fclose(src); src = 0;
    if (fclose(dst) != 0) { dst = 0; goto done; }
    dst = 0;

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

static int import_image(const char *name)
{
    struct stat st;
    FILE *in = 0;
    unsigned char *file_data = 0, *decoded = 0, *rgb = 0, *jpg = 0;
    int w = 0, h = 0, comp = 0, jpg_size = 0, ok = 0;

    if (!name || stat(name, &st) != 0 || st.st_size <= 0 || st.st_size > 2*1024*1024) {
        strcpy(result_msg, "Choose a JPG/PNG smaller than 2 MB");
        goto done;
    }
    file_data = malloc(st.st_size);
    in = fopen(name, "rb");
    if (!file_data || !in || fread(file_data, st.st_size, 1, in) != 1) {
        strcpy(result_msg, "Could not read source image");
        goto done;
    }
    fclose(in); in = 0;

    if (st.st_size >= 6 && file_data[0] == 'G' && file_data[1] == 'I' && file_data[2] == 'F') {
        strcpy(result_msg, "GIF boot animations are no longer supported");
        goto done;
    }

    {
        cam_meminfo mi;
        GetMemInfo(&mi);
        boot_alloc_used = 0;
        // Keep room for the 320x240 RGB resize buffer, encoded JPEG and UI.
        boot_alloc_limit = (mi.free_block_max_size > 320*1024) ?
                           mi.free_block_max_size - 320*1024 : 0;
    }
    if (boot_alloc_limit > 4*1024*1024) boot_alloc_limit = 4*1024*1024;
    if (boot_alloc_limit < 128*1024) {
        strcpy(result_msg, "Not enough memory to import safely");
        goto done;
    }

    decoded = stbi_load_from_memory(file_data, st.st_size, &w, &h, &comp, 3);
    comp = 3;
    if (!decoded || w < 1 || h < 1) {
        strcpy(result_msg, "Unsupported or damaged JPG/PNG");
        goto done;
    }
    rgb = malloc(BOOT_SCREEN_WIDTH * BOOT_SCREEN_HEIGHT * 3);
    jpg = malloc(BOOT_SCREEN_MAX_JPEG);
    if (!rgb || !jpg) {
        strcpy(result_msg, "Not enough free camera memory");
        goto done;
    }

    draw_progress_bar("Import boot screen", 25);
    resize_contain(decoded, w, h, comp, rgb);
    jpg_size = encode_frame(rgb, jpg);
    if (!jpg_size) {
        strcpy(result_msg, "Image is too detailed for A480 boot JPEG");
        goto done;
    }
    draw_progress_bar("Import boot screen", 100);
    if (!install_early_jpeg(jpg, jpg_size)) {
        strcpy(result_msg, "Could not update DISKBOOT.BIN");
        goto done;
    }
    sprintf(result_msg, "Installed for next boot:\n%s", base_name(name));
    ok = 1;
done:
    if (in) fclose(in);
    if (file_data) free(file_data);
    if (decoded) stbi_image_free(decoded);
    if (rgb) free(rgb);
    if (jpg) free(jpg);
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
