//-------------------------------------------------------------------
// Profiling harness for the emulator core.
//
// Separate from gbc_boot_selftest.c on purpose: that one is full of counters,
// VRAM scans and printf, all of which would show up in a profile as work the
// camera never does. This runs the same code path the A480 spends its time in
// - gbc_core_step_frame() plus the scanline rasteriser, one frame in
// GBC_RENDER_EVERY rendered, which is exactly what the HUD's "emu=" measures -
// and nothing else.
//
// What transfers from an x86 profile to ARM946 and what does not:
//
//   transfers      which functions dominate, how many times each runs, how
//                  many memory accesses and branches the interpreter executes
//                  per emulated instruction. For an interpreter this is the
//                  bulk of the answer.
//
//   does not       absolute time. The A480 has no branch prediction, a tiny
//                  cache and no divide instruction, so on ARM a mispredicted
//                  branch or a cache miss costs far more relative to an ALU op
//                  than it does here. Treat x86 time as a weak signal and
//                  instruction/call counts as the strong one.
//
// Build:
//   gcc -O2 -g -fno-omit-frame-pointer -o /tmp/gbc_bench tools/gbc_bench.c \
//       modules/gbc/gbc_rom.c modules/gbc/gbc_cam.c \
//       modules/gbc/core/gbccore_unity.c \
//       -I modules/gbc -I modules/gbc/core \
//       -DGBC_HOST_TEST -DGBC_CHDK_PORT -include modules/gbc/gbc_port.h
//
// Profile:
//   perf record -g --call-graph=dwarf /tmp/gbc_bench <rom> 3000
//   perf report --stdio
//-------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

#ifndef GBC_RENDER_EVERY
#define GBC_RENDER_EVERY 2
#endif

//-------------------------------------------------------------------

char gbc_panic_msg[64];
int  gbc_panicked = 0;
int  gbc_render_enabled = 1;
int  gbc_feed_status = 0;

void gbc_panic(const char *what) { (void)what; gbc_panicked = 1; }

int  gbc_rom_memo_bank = -1;
unsigned char *gbc_rom_memo_win = 0;
void gbc_rom_memo_flush(void) { gbc_rom_memo_bank = -1; gbc_rom_memo_win = 0; }

static gbc_cam_t *the_cart = 0;

unsigned char gbc_hook_cam_read(struct gb_state *s, unsigned a)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    return c ? gbc_cam_read(c, (unsigned short)a) : 0xff;
}

void gbc_hook_cam_write(struct gb_state *s, unsigned a, unsigned char v)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    if (!c) return;
    gbc_cam_write(c, (unsigned short)a, v);
    if (a < 0x6000) gbc_mmu_remap(s);
}

int gbc_hook_cam_rom_bank(struct gb_state *s)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    return c ? c->rom_bank : 1;
}

int  dbg_run_debugger(struct gb_state *s) { (void)s; gbc_panicked = 1; return 1; }
void dbg_print_regs(struct gb_state *s) { (void)s; }

int  gui_lcd_init(int w, int h, int z, char *t)
{ (void)w;(void)h;(void)z;(void)t; return 0; }
void gui_lcd_render_frame(char uc, uint16_t *pb) { (void)uc; (void)pb; }
int  gui_input_poll(struct player_input *in) { memset(in,0,sizeof(*in)); return 0; }
int  gui_audio_init(int r, int c, size_t n, uint8_t *b)
{ (void)r;(void)c;(void)n;(void)b; return 0; }

// Static, so successive exposures are comparable and the sensor path does a
// realistic amount of work without the result drifting.
int gbc_feed_capture(unsigned char *dst)
{
    int x, y;
    for (y = 0; y < GBCAM_SENSOR_H; y++)
        for (x = 0; x < GBCAM_SENSOR_W; x++)
            dst[y * GBCAM_SENSOR_W + x] = (unsigned char)(x * 2 + y);
    gbc_feed_status = 1;
    return 0;
}

extern unsigned gbc_core_step_frame(struct gb_state *s, unsigned guard,
                                    int *hit_guard);

//-------------------------------------------------------------------

int main(int argc, char **argv)
{
    struct gb_state *s;
    gbc_rom_t pager;
    unsigned char *bank0;
    int frames = (argc > 2) ? atoi(argv[2]) : 3000;
    int f, phase = 0;
    unsigned long long insns = 0;
    // Rendering changes cannot be validated by instruction count - the CPU runs
    // identically whatever the rasteriser draws. This hashes every rendered
    // frame so an "optimisation" that alters a single pixel is caught. Off by
    // default so it never pollutes a timing run.
    int do_check = (argc > 3 && !strcmp(argv[3], "--check"));
    unsigned long long fbhash = 1469598103934665603ULL;
    struct timespec t0, t1;
    double secs;

    if (argc < 2) { fprintf(stderr, "usage: gbc_bench <rom> [frames]\n"); return 2; }

    if (gbc_rom_open(&pager, argv[1])) { fprintf(stderr, "cannot open ROM\n"); return 1; }

    s = calloc(1, sizeof(struct gb_state));
    bank0 = gbc_rom_bank(&pager, 0);
    if (state_new_from_rom(s, bank0, (size_t)pager.size)) return 1;

    cpu_reset_state(s);
    s->gbc_pager = &pager;

    if (s->mbc == GBC_MBC_CAM)
    {
        the_cart = calloc(1, sizeof(gbc_cam_t));
        if (gbc_cam_init(the_cart)) return 1;
        s->gbc_cart = the_cart;
    }

    init_emu_state(s);
    cpu_init_emu_cpu_state(s);
    if (lcd_init(s)) return 1;
    gbc_mmu_remap(s);

    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (f = 0; f < frames; f++)
    {
        int hit_guard = 0;

        // Mash A past the boot sequence so the run spends its time in the
        // viewfinder, which is the mode that matters.
        s->io_buttons_buttons = 0x0f;
        s->io_buttons_dirs    = 0x0f;
        if (f > 180 && (f % 24) < 6) s->io_buttons_buttons &= ~(1 << 0);

        // Same rasteriser duty cycle as gbc_emu_frame().
        gbc_render_enabled = (++phase >= GBC_RENDER_EVERY);
        if (gbc_render_enabled) phase = 0;

        gbc_panicked = 0;
        insns += gbc_core_step_frame(s, 2000000, &hit_guard);
        if (gbc_panicked) { fprintf(stderr, "panic at frame %d\n", f); return 1; }
        if (the_cart) gbc_cam_step(the_cart, GB_FREQ / 60);

        if (do_check && gbc_render_enabled)
        {
            const uint16_t *pb = s->emu_state->lcd_pixbuf;
            int i;
            for (i = 0; i < GB_LCD_W * GB_LCD_H; i++)
            {
                fbhash ^= pb[i];
                fbhash *= 1099511628211ULL;
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    printf("%d frames, %llu instructions, %.3fs\n", frames, insns, secs);
    printf("%.1f ns per emulated instruction, %.2f Mframe-insn/s\n",
           secs * 1e9 / (double)insns, (double)insns / secs / 1e6);
    if (the_cart) printf("%lu exposures\n", the_cart->captures);
    if (do_check) printf("framebuffer hash %016llx\n", fbhash);
    return 0;
}
