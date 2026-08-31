//-------------------------------------------------------------------
// Host harness that boots a real ROM through the real core.
//
// The camera-side failure ("hangs, then the body resets, no HUD ever
// appears") gives almost nothing to work with: there is no console, and the
// symptom of a wedged emulator and the symptom of a very slow one are the
// same picture. This runs the identical code off-camera, where a hang is a
// backtrace and an out-of-bounds write is an ASan report.
//
// Build and run:
//   gcc -g -O1 -fsanitize=address -o /tmp/gbc_boot tools/gbc_boot_selftest.c \
//       modules/gbc/gbc_rom.c modules/gbc/gbc_cam.c \
//       modules/gbc/core/gbccore_unity.c \
//       -I modules/gbc -I modules/gbc/core \
//       -DGBC_HOST_TEST -DGBC_CHDK_PORT -include modules/gbc/gbc_port.h
//   /tmp/gbc_boot <rom> [frames]
//-------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbc_rom.h"
#include "gbc_cam.h"
#include "core/types.h"
#include "core/hwdefs.h"
#include "core/state.h"
#include "core/cpu.h"
#include "core/lcd.h"
#include "core/mmu.h"
#include "core/player_input.h"
#include "gbc_hooks.h"

//-------------------------------------------------------------------
// The bits of gbc_emu.c that are not CHDK-specific, restated.

char gbc_panic_msg[64];
int  gbc_panicked = 0;
int  gbc_render_enabled = 1;

void gbc_panic(const char *what)
{
    if (!gbc_panicked)
        snprintf(gbc_panic_msg, sizeof(gbc_panic_msg), "%s", what);
    gbc_panicked = 1;
}

int  gbc_rom_memo_bank = -1;
unsigned char *gbc_rom_memo_win = 0;
void gbc_rom_memo_flush(void) { gbc_rom_memo_bank = -1; gbc_rom_memo_win = 0; }

static gbc_cam_t *the_cart = 0;

// Counters for what the ROM actually does with the cartridge, so "the picture
// never reaches VRAM" can be split into "the ROM never read it", "it read it
// and got 0xff" and "it read the right bytes and did something else with them".
static unsigned long cam_reads_img = 0;    // inside the captured-image window
static unsigned long cam_reads_ff  = 0;    // of those, ones that returned 0xff
static unsigned long cam_reads_reg = 0;

unsigned char gbc_hook_cam_read(struct gb_state *s, unsigned a)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    unsigned char v;

    if (!c) return 0xff;
    v = gbc_cam_read(c, (unsigned short)a);

    if (c->regs_selected)
        cam_reads_reg++;
    else if (a >= 0xa000 + GBCAM_IMAGE_OFFSET &&
             a <  0xa000 + GBCAM_IMAGE_OFFSET + GBCAM_IMAGE_BYTES)
    {
        cam_reads_img++;
        if (v == 0xff) cam_reads_ff++;
    }
    return v;
}

void gbc_hook_cam_write(struct gb_state *s, unsigned a, unsigned char v)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    if (!c) return;

    /* Trace transitions of the RAM-enable latch in order, capped so the log
     * stays readable. Ordering is the thing in question, not which values
     * appear. */
    if (a < 0x2000)
    {
        static int n = 0;
        int before = c->sram_enabled;
        gbc_cam_write(c, (unsigned short)a, v);
        if (c->sram_enabled != before && n++ < 12)
            printf("    [enable] wrote %02x to %04x: sram_enabled %d -> %d "
                   "(image reads so far %lu)\n",
                   v, a, before, c->sram_enabled, cam_reads_img);
        if (a < 0x6000)
            gbc_mmu_remap(s);
        return;
    }

    gbc_cam_write(c, (unsigned short)a, v);
    if (a < 0x6000)
        gbc_mmu_remap(s);
}

int gbc_hook_cam_rom_bank(struct gb_state *s)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    return c ? c->rom_bank : 1;
}

int  dbg_run_debugger(struct gb_state *s)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "core fault pc=%04x", s ? s->pc : 0);
    gbc_panic(buf);
    return 1;
}
void dbg_print_regs(struct gb_state *s) { (void)s; }

int gui_lcd_init(int w, int h, int z, char *t)
{ (void)w;(void)h;(void)z;(void)t; return 0; }

// Never called: the core rasterises into emu_state->lcd_pixbuf and this port's
// gbc_emu_frame() reads that buffer itself once per slice, rather than letting
// lcd.c push. The harness does the same - see frame_histogram().
void gui_lcd_render_frame(char use_colors, uint16_t *pixbuf)
{ (void)use_colors; (void)pixbuf; }

static const uint16_t *last_pixbuf = 0;
static int             last_use_colors = 0;
static int             pixbuf_seen = 0;

// Reduce a frame the same way gbc_emu.c's blit does, and report the spread of
// shades. A Game Boy screen that is entirely one shade is the "white box"
// symptom; anything else means the rasteriser is producing a picture and the
// problem is downstream of it.
static void frame_histogram(int f, const char *tag)
{
    unsigned hist[4] = {0,0,0,0};
    int i;

    if (!pixbuf_seen) { printf("  %s frame %d: nothing rendered\n", tag, f); return; }

    for (i = 0; i < GB_LCD_W * GB_LCD_H; i++)
    {
        unsigned v = last_pixbuf[i], c;
        if (last_use_colors)
        {
            unsigned l = ((v & 0x1f) * 77 + ((v >> 5) & 0x1f) * 151 +
                          ((v >> 10) & 0x1f) * 28) >> 8;
            c = 3 - (l >> 3);
        }
        else
            c = v & 3;
        hist[c & 3]++;
    }
    printf("  %s frame %4d shades: %5u %5u %5u %5u%s\n", tag, f,
           hist[0], hist[1], hist[2], hist[3],
           (hist[0] == GB_LCD_W * GB_LCD_H || hist[3] == GB_LCD_W * GB_LCD_H)
               ? "   <- FLAT" : "");
}

// Dump the frame as a PGM so it can actually be looked at.
static void frame_dump(int f)
{
    char path[64];
    FILE *fp;
    int i;

    if (!pixbuf_seen) return;
    snprintf(path, sizeof(path), "/tmp/gbc_frame_%04d.pgm", f);
    fp = fopen(path, "wb");
    if (!fp) return;
    fprintf(fp, "P5\n%d %d\n255\n", GB_LCD_W, GB_LCD_H);
    for (i = 0; i < GB_LCD_W * GB_LCD_H; i++)
    {
        unsigned v = last_pixbuf[i], c;
        if (last_use_colors)
        {
            unsigned l = ((v & 0x1f) * 77 + ((v >> 5) & 0x1f) * 151 +
                          ((v >> 10) & 0x1f) * 28) >> 8;
            c = 3 - (l >> 3);
        }
        else
            c = v & 3;
        fputc((int)(255 - (c & 3) * 85), fp);
    }
    fclose(fp);
    printf("  wrote %s\n", path);
}
int gui_input_poll(struct player_input *in) { memset(in,0,sizeof(*in)); return 0; }
int gui_audio_init(int r, int c, size_t n, uint8_t *b)
{ (void)r;(void)c;(void)n;(void)b; return 0; }

//-------------------------------------------------------------------
// Stand-in for gbc_feed.c: a moving gradient, so the sensor path is exercised
// with something that is neither flat nor constant between exposures.

int gbc_feed_status = 0;
static int feed_calls = 0;

int gbc_feed_capture(unsigned char *dst)
{
    int x, y;
    for (y = 0; y < GBCAM_SENSOR_H; y++)
        for (x = 0; x < GBCAM_SENSOR_W; x++)
            // Deliberately static across captures. It used to include
            // feed_calls, which made every exposure different - and that
            // silently broke the "is the image in VRAM" search below, because
            // SRAM always held a newer frame than the copy the ROM had made.
            dst[y * GBCAM_SENSOR_W + x] = (unsigned char)(x * 2 + y);
    feed_calls++;
    gbc_feed_status = 1;
    return 0;
}

//-------------------------------------------------------------------

extern unsigned gbc_core_step_frame(struct gb_state *s, unsigned guard,
                                    int *hit_guard);

//-------------------------------------------------------------------
// Scripted input, so the harness can get past a menu.
//
// The camera ROM boots to a menu and only starts its viewfinder once SHOOT is
// chosen, so a run that never presses anything sits on the menu forever and
// says nothing about the mode that actually matters. This mashes A on a duty
// cycle, which is enough to walk the ROM into SHOOT.

static void apply_input(struct gb_state *s, int a, int start)
{
    /* Buttons are active-low: a set bit is released. */
    s->io_buttons_buttons = 0x0f;
    s->io_buttons_dirs    = 0x0f;
    if (a)     s->io_buttons_buttons &= ~(1 << 0);
    if (start) s->io_buttons_buttons &= ~(1 << 3);
}

int main(int argc, char **argv)
{
    struct gb_state *s;
    gbc_rom_t pager;
    unsigned char *bank0;
    int frames = (argc > 2) ? atoi(argv[2]) : 600;
    int every  = (argc > 3) ? atoi(argv[3]) : 60;
    int f;

    if (argc < 2) { fprintf(stderr, "usage: gbc_boot <rom> [frames]\n"); return 2; }

    printf("gbc_boot_selftest: %s\n\n", argv[1]);

    if (gbc_rom_open(&pager, argv[1])) { printf("cannot open ROM\n"); return 1; }

    s = calloc(1, sizeof(struct gb_state));
    bank0 = gbc_rom_bank(&pager, 0);

    if (state_new_from_rom(s, bank0, (size_t)pager.size))
    { printf("state_new_from_rom failed\n"); return 1; }

    printf("gb_type=%d mbc=%02x rom_banks=%d extram_banks=%d\n",
           s->gb_type, s->mbc, s->mem_num_banks_rom, s->mem_num_banks_extram);

    cpu_reset_state(s);
    s->gbc_pager = &pager;

    if (s->mbc == GBC_MBC_CAM)
    {
        the_cart = calloc(1, sizeof(gbc_cam_t));
        if (gbc_cam_init(the_cart)) { printf("cam init failed\n"); return 1; }
        s->gbc_cart = the_cart;
    }

    init_emu_state(s);
    cpu_init_emu_cpu_state(s);
    if (lcd_init(s)) { printf("lcd_init failed\n"); return 1; }
    gbc_mmu_remap(s);

    printf("boot ok, running %d frames\n\n", frames);

    for (f = 0; f < frames; f++)
    {
        int hit_guard = 0;
        unsigned n;

        // Mash A once the boot sequence is over: 6 frames held, 18 released.
        // Long enough that the ROM's own key-repeat filter registers it.
        apply_input(s, (f > 180) && ((f % 24) < 6), 0);

        gbc_panicked = 0;
        n = gbc_core_step_frame(s, 2000000, &hit_guard);

        if (gbc_panicked)
        {
            printf("frame %d: PANIC after %u insns: %s (pc=%04x)\n",
                   f, n, gbc_panic_msg, s->pc);
            return 1;
        }
        if (hit_guard)
        {
            printf("frame %d: GUARD - vblank never reached in %u insns "
                   "(pc=%04x LCDC=%02x)\n", f, n, s->pc, s->io_lcd_LCDC);
            return 1;
        }
        // Whenever the cartridge finishes an exposure, report what it actually
        // produced and the register set the ROM asked for. The emulated screen
        // being black and the cartridge handing over a black picture look
        // identical from the pixbuf; this separates them.
        if (the_cart)
        {
            static unsigned long last_cap = 0;
            if (the_cart->captures != last_cap)
            {
                unsigned hist[4] = {0,0,0,0};
                const unsigned char *img = &the_cart->sram[GBCAM_IMAGE_OFFSET];
                int tx, ty, py, px, lo = 255, hi = 0, k;

                for (ty = 0; ty < GBCAM_IMAGE_TILES_Y; ty++)
                    for (tx = 0; tx < GBCAM_IMAGE_TILES_X; tx++)
                    {
                        const unsigned char *t =
                            img + (ty * GBCAM_IMAGE_TILES_X + tx) * 16;
                        for (py = 0; py < 8; py++)
                        {
                            unsigned b1 = t[py*2], b2 = t[py*2+1];
                            for (px = 0; px < 8; px++)
                            {
                                unsigned sh = ((b1 >> (7-px)) & 1) |
                                             (((b2 >> (7-px)) & 1) << 1);
                                hist[sh]++;
                            }
                        }
                    }

                /* Spread of the ROM's 48 dither thresholds. */
                for (k = 6; k < GBCAM_NUM_REGS; k++)
                {
                    if (the_cart->reg[k] < lo) lo = the_cart->reg[k];
                    if (the_cart->reg[k] > hi) hi = the_cart->reg[k];
                }

                if (last_cap == 0 || (the_cart->captures % 20) == 0)
                    printf("  [cap %lu] cart image shades %5u %5u %5u %5u | "
                           "exposure=%04x reg0=%02x reg1=%02x reg4=%02x "
                           "dither %d..%d\n",
                           the_cart->captures, hist[0], hist[1], hist[2], hist[3],
                           the_cart->reg[3] | (the_cart->reg[2] << 8),
                           the_cart->reg[0], the_cart->reg[1], the_cart->reg[4],
                           lo, hi);
                last_cap = the_cart->captures;
            }
            gbc_cam_step(the_cart, GB_FREQ / 60);
        }

        // The first frame that is not a flat expanse of one shade is the one
        // worth knowing: everything before it is the ROM's own boot sequence
        // with the LCD off, which on a real Game Boy is a blank white screen
        // and on this port looks exactly like a broken blitter.
        {
            const uint16_t *pb = s->emu_state->lcd_pixbuf;
            static int announced = 0;
            if (pb && !announced)
            {
                int i, flat = 1;
                for (i = 1; i < GB_LCD_W * GB_LCD_H; i++)
                    if ((pb[i] & 3) != (pb[0] & 3)) { flat = 0; break; }
                if (!flat)
                {
                    printf("  >> first non-flat frame: %d (LCDC=%02x)\n",
                           f, s->io_lcd_LCDC);
                    announced = 1;
                }
            }
        }

        /* Pager traffic, reported per interval rather than cumulatively.
         *
         * This is the one cost a host profile cannot see. Here a miss is a read
         * from a file in page cache; on the A480 it is a 16K read off an SD
         * card through DryOS, which is many orders of magnitude slower and does
         * not appear in any x86 measurement at all. If the boot sequence sweeps
         * banks, that is the real startup cost and nothing in gprof, perf or
         * callgrind will ever say so. */
        {
            static unsigned long prev_miss = 0, prev_hit = 0;
            if ((f % every) == 0)
            {
                printf("  [pager] f=%4d  misses %lu (+%lu)  hits %lu (+%lu)\n",
                       f, pager.misses, pager.misses - prev_miss,
                       pager.hits, pager.hits - prev_hit);
                prev_miss = pager.misses;
                prev_hit  = pager.hits;
            }
        }

        if ((f % every) == 0)
        {
            printf("  frame %4d  insns=%6u pc=%04x bank=%d cap=%lu feed=%d "
                   "LCDC=%02x BGP=%02x SCX=%d SCY=%d\n",
                   f, n, s->pc,
                   the_cart ? the_cart->rom_bank : s->mem_bank_rom,
                   the_cart ? the_cart->captures : 0, gbc_feed_status,
                   s->io_lcd_LCDC, s->io_lcd_BGP,
                   s->io_lcd_SCX, s->io_lcd_SCY);
            // Where did the ROM actually put the camera image?
            //
            // The cartridge deposits 224 tiles of 2bpp data in SRAM; the ROM
            // then copies them into VRAM itself. If the BG cannot address
            // wherever it put them, the viewfinder is blank while sprites -
            // which always fetch from 0x8000 regardless of LCDC bit 4 - can
            // still show fragments of it. So find the block rather than
            // reason about it.
            if (the_cart && the_cart->captures > 0)
            {
                const unsigned char *img = &the_cart->sram[GBCAM_IMAGE_OFFSET];
                int t, off, present = 0, lo = 0x2000, hi = -1;
                int lcdc_unsigned = (s->io_lcd_LCDC & (1 << 4)) ? 1 : 0;

                /* Per tile rather than one contiguous block: the ROM is free to
                 * rearrange them, and an exact prefix match cannot survive
                 * that. A tile of all-identical bytes is skipped - flat tiles
                 * match everywhere and would inflate the count. */
                for (t = 0; t < GBCAM_IMAGE_TILES_X * GBCAM_IMAGE_TILES_Y; t++)
                {
                    const unsigned char *tile = img + t * 16;
                    int k, flat = 1;
                    for (k = 1; k < 16; k++)
                        if (tile[k] != tile[0]) { flat = 0; break; }
                    if (flat) continue;

                    for (off = 0; off + 16 <= VRAM_BANKSIZE; off += 16)
                        if (!memcmp(s->mem_VRAM + off, tile, 16))
                        {
                            present++;
                            if (off < lo) lo = off;
                            if (off > hi) hi = off;
                            break;
                        }
                }

                if (present)
                    printf("  camera tiles in VRAM: %d, spanning 0x%04x..0x%04x "
                           "| LCDC bit4=%d (%s), BG window 0x%04x..0x%04x\n",
                           present, 0x8000 + lo, 0x8000 + hi, lcdc_unsigned,
                           lcdc_unsigned ? "unsigned" : "signed",
                           lcdc_unsigned ? 0x8000 : 0x8800,
                           lcdc_unsigned ? 0x8fff : 0x97ff);
                else
                    printf("  no camera tiles in VRAM at all\n");

                printf("    mapper: sram_enabled=%d regs_selected=%d "
                       "sram_bank=%d | reads: image=%lu (0xff:%lu) regs=%lu\n",
                       the_cart->sram_enabled, the_cart->regs_selected,
                       the_cart->sram_bank,
                       cam_reads_img, cam_reads_ff, cam_reads_reg);
            }

            last_pixbuf     = s->emu_state->lcd_pixbuf;
            last_use_colors = (s->gb_type == GB_TYPE_CGB);
            pixbuf_seen     = (last_pixbuf != 0);
            frame_histogram(f, "");
            frame_dump(f);
        }
    }

    printf("\nran %d frames clean", frames);
    if (the_cart) printf(", %lu exposures", the_cart->captures);
    printf("\n");
    return 0;
}
