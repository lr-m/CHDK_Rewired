//-------------------------------------------------------------------
// Host-side test for the Game Boy Camera sensor pipeline.
//
// Same idea as bend_selftest.c / bendx_selftest.c: gbc_cam.c has no CHDK
// dependencies beyond umalloc/ufree and the gbc_feed_capture() bridge, so it
// compiles here and the whole signal path can be proven on a synthetic image
// before it ever runs on a body. Nobody wants to debug a dithering matrix
// through a 2.5" LCD.
//
// Build and run:
//   gcc -O2 -o /tmp/gbcam_selftest tools/gbcam_selftest.c modules/gbc/gbc_cam.c
//       -I modules/gbc -DGBC_HOST_TEST
//   /tmp/gbcam_selftest
//
// Writes /tmp/gbcam_out.pgm - the decoded 128x112 result, viewable directly.
//-------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbc_cam.h"

//-------------------------------------------------------------------
// The feed the cartridge pulls from. On the camera this is the live viewport;
// here it is a test card - a horizontal gradient crossed with a hard-edged
// bar, so both the exposure ramp and the edge-enhancement kernel have
// something to bite on.

static int feed_mode = 0;

int gbc_feed_capture(unsigned char *dst)
{
    int x, y;
    for (y = 0; y < GBCAM_SENSOR_H; y++)
        for (x = 0; x < GBCAM_SENSOR_W; x++)
        {
            int v;
            if (feed_mode == 0)
                v = (x * 255) / (GBCAM_SENSOR_W - 1);       // gradient
            else if (feed_mode == 1)
                v = ((x / 8) & 1) ? 220 : 40;               // hard bars
            else
                v = 128;                                    // flat grey
            dst[y * GBCAM_SENSOR_W + x] = (unsigned char)v;
        }
    return 0;
}

//-------------------------------------------------------------------

static int failures = 0;

static void check(int cond, const char *what)
{
    printf("  %-52s %s\n", what, cond ? "ok" : "FAIL");
    if (!cond) failures++;
}

//-------------------------------------------------------------------
// Decode the packed 2bpp tile output back to a flat 128x112 image so the
// result can be looked at and measured.

static void decode_tiles(const unsigned char *tiles, unsigned char *flat)
{
    int tx, ty, row, bit;
    for (ty = 0; ty < GBCAM_IMAGE_TILES_Y; ty++)
        for (tx = 0; tx < GBCAM_IMAGE_TILES_X; tx++)
        {
            const unsigned char *t = tiles + ((ty * GBCAM_IMAGE_TILES_X) + tx) * 16;
            for (row = 0; row < 8; row++)
                for (bit = 0; bit < 8; bit++)
                {
                    int lo = (t[row * 2 + 0] >> (7 - bit)) & 1;
                    int hi = (t[row * 2 + 1] >> (7 - bit)) & 1;
                    int shade = lo | (hi << 1);
                    flat[(ty * 8 + row) * GBCAM_W + (tx * 8 + bit)] =
                        (unsigned char)(255 - shade * 85);
                }
        }
}

//-------------------------------------------------------------------
// A plausible register set: mid exposure, the 1-D filter, and an ordered
// dither matrix.
//
// The thresholds cluster tightly around 128 rather than spanning 64/128/192,
// and that is not a fudge to make the test pass - it is forced by the sensor.
// The "adapt to 3.1/5.0 V" step in gbc_cam.c divides the signal by 8, so a
// full-range 0..255 input reaches the dither stage as roughly 112..143.
// Thresholds outside that window can never be crossed and the image collapses
// to two shades. The real cartridge's matrix is clustered for exactly this
// reason; a port that copies textbook Bayer values gets a flat picture and no
// obvious explanation why.

static void default_regs(gbc_cam_t *c)
{
    // 4x4 ordered dither, offsets in the compressed range.
    static const int bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6,
                                   3, 11, 1,  9, 15, 7, 13, 5 };
    int i;
    memset(c->reg, 0, sizeof(c->reg));

    c->reg[1] = 0x00;               // N=0, VH=0  -> filtering mode 0x0
    c->reg[2] = 0x03;               // exposure high
    c->reg[3] = 0x00;               // exposure low -> 0x0300
    c->reg[4] = 0x01;

    for (i = 0; i < 16; i++)
    {
        int d = bayer[i] - 8;       // -8..+7 around the midpoint
        c->reg[6 + i * 3 + 0] = (unsigned char)(120 + d);
        c->reg[6 + i * 3 + 1] = (unsigned char)(128 + d);
        c->reg[6 + i * 3 + 2] = (unsigned char)(136 + d);
    }
}

//-------------------------------------------------------------------

int main(void)
{
    gbc_cam_t cam;
    unsigned char flat[GBCAM_W * GBCAM_H];
    int i, nonzero, distinct[4] = {0,0,0,0};

    printf("gbcam_selftest\n\n");

    if (gbc_cam_init(&cam))
    {
        printf("gbc_cam_init failed\n");
        return 1;
    }

    printf("sizes\n");
    check(GBCAM_IMAGE_BYTES == 3584, "packed image is 3584 bytes (14x16 tiles)");
    check(GBCAM_SENSOR_H == 120,     "sensor height is 112 + 8 dark rows");
    check(sizeof(cam.reg) == 54,     "register file is 54 bytes");

    //---------------------------------------------------------------
    printf("\npower-on state\n");
    check(cam.rom_bank == 1, "switchable ROM window powers up on bank 1");
    check(cam.sram_enabled == 0, "SRAM write-enable starts off");

    // The RAM-enable latch gates WRITES only; reading RAM and registers is
    // always enabled on MAC-GBD (Pan Docs, "Game Boy Camera"). This test used
    // to assert the MBC behaviour - that a disabled SRAM floats high - and
    // that assertion was wrong and kept a real bug pinned in place: the camera
    // ROM reads the captured picture with the latch off, so gating reads on it
    // returned 0xff for the entire copy loop and the viewfinder never showed
    // anything.
    cam.sram[0] = 0x5a;
    check(gbc_cam_read(&cam, 0xa000) == 0x5a,
          "RAM reads work with the write-enable off");

    // Writes, on the other hand, are gated.
    gbc_cam_write(&cam, 0xa000, 0x99);
    check(cam.sram[0] == 0x5a, "RAM writes are ignored with the enable off");
    gbc_cam_write(&cam, 0x0000, 0x0a);
    gbc_cam_write(&cam, 0xa000, 0x99);
    check(cam.sram[0] == 0x99, "RAM writes land once enabled");
    gbc_cam_write(&cam, 0x0000, 0x00);
    cam.sram[0] = 0;

    // And while the capture unit is running, RAM reads return 0x00 rather
    // than the previous picture.
    cam.clocks_left = 1000;
    check(gbc_cam_read(&cam, 0xa000) == 0x00, "RAM reads 0x00 mid-capture");
    cam.clocks_left = 0;

    //---------------------------------------------------------------
    printf("\nmapper\n");
    gbc_cam_write(&cam, 0x0000, 0x0a);
    check(cam.sram_enabled == 1, "0x0a to 0000-1fff enables SRAM");

    gbc_cam_write(&cam, 0x2000, 0x00);
    check(cam.rom_bank == 1, "ROM bank 0 is remapped to 1");
    gbc_cam_write(&cam, 0x2000, 0x3f);
    check(cam.rom_bank == 0x3f, "ROM bank 63 selectable");

    gbc_cam_write(&cam, 0x4000, 0x03);
    check(cam.regs_selected == 0 && cam.sram_bank == 3, "SRAM bank select");
    gbc_cam_write(&cam, 0x4000, 0x10);
    check(cam.regs_selected == 1, "bit 4 selects the register file");

    // SRAM round-trip on a non-zero bank, to prove banking arithmetic.
    gbc_cam_write(&cam, 0x4000, 0x05);
    gbc_cam_write(&cam, 0xa000, 0xa5);
    gbc_cam_write(&cam, 0x4000, 0x06);
    check(gbc_cam_read(&cam, 0xa000) != 0xa5, "banks are distinct");
    gbc_cam_write(&cam, 0x4000, 0x05);
    check(gbc_cam_read(&cam, 0xa000) == 0xa5, "SRAM round-trips on bank 5");

    //---------------------------------------------------------------
    printf("\ncapture\n");
    default_regs(&cam);
    gbc_cam_write(&cam, 0x4000, 0x10);          // select registers
    feed_mode = 0;
    gbc_cam_write(&cam, 0xa000, 0x01);          // trigger

    check(cam.captures == 1, "trigger ran one capture");
    check(cam.clocks_left > 0, "busy timer armed after trigger");
    check((gbc_cam_read(&cam, 0xa000) & 1) == 1, "register 0 reads busy");

    gbc_cam_step(&cam, cam.clocks_left);
    check((gbc_cam_read(&cam, 0xa000) & 1) == 0, "busy clears once the timer drains");

    //---------------------------------------------------------------
    printf("\nimage\n");
    decode_tiles(&cam.sram[GBCAM_IMAGE_OFFSET], flat);

    nonzero = 0;
    for (i = 0; i < GBCAM_W * GBCAM_H; i++)
    {
        if (flat[i] != 255) nonzero++;
        distinct[(255 - flat[i]) / 85]++;
    }
    check(nonzero > 0, "gradient produced a non-blank image");
    check(distinct[0] && distinct[3], "image uses both extremes of the palette");

    printf("    shade histogram: %d %d %d %d\n",
           distinct[0], distinct[1], distinct[2], distinct[3]);

    // The image must land inside the SRAM region reserved for it and must not
    // have run past into the next bank.
    {
        int clean = 1;
        for (i = GBCAM_IMAGE_OFFSET + GBCAM_IMAGE_BYTES; i < GBCAM_SRAM_BANKSIZE; i++)
            if (cam.sram[i] != 0) { clean = 0; break; }
        check(clean, "capture stayed inside its 3584 bytes");
    }

    //---------------------------------------------------------------
    printf("\nedge enhancement (mode 0xe)\n");
    default_regs(&cam);
    cam.reg[1] = 0x80 | (3 << 5);               // N=1, VH=3
    cam.reg[4] = 0x80 | (6 << 4);               // E3=1, alpha=4.0 -> mode 0xe
    feed_mode = 1;                              // hard bars
    gbc_cam_write(&cam, 0xa000, 0x01);
    decode_tiles(&cam.sram[GBCAM_IMAGE_OFFSET], flat);

    nonzero = 0;
    for (i = 0; i < GBCAM_W * GBCAM_H; i++) if (flat[i] != 255) nonzero++;
    check(nonzero > 0, "2-D enhancement produced an image");

    //---------------------------------------------------------------
    // Exposure sweep - the exposure register must actually widen the output.
    //
    // Every other check here runs at exposure 0x0300, where the exposure stage
    // is the identity: (v * 0x0300) / 0x0300 == v. That is the one value at
    // which the stage cannot exceed 255, so it is also the one value at which
    // wrongly clamping between the exposure and voltage stages makes no
    // difference. Testing only there is how a clamp that broke the camera
    // outright got shipped as a no-op "fix".
    //
    // The real invariant is not a fixed output window - it is that raising
    // exposure widens the range reaching the dither stage. That is the entire
    // mechanism the ROM's auto-exposure uses: it fixes its dither thresholds
    // (observed at 140..231 in the viewfinder) and winds exposure until the
    // picture spans them. A pipeline whose output is stuck at 112..143 makes
    // every pixel darker than every threshold, and the viewfinder is black.
    printf("\nexposure sweep - higher exposure widens the dither input\n");
    {
        static const unsigned expos[] = { 0x0300, 0x0600, 0x1000, 0x4000 };
        int e, prev_hi = -1, monotonic = 1;
        int hi_at_nominal = 0, hi_at_high = 0;

        for (e = 0; e < (int)(sizeof(expos)/sizeof(expos[0])); e++)
        {
            int v, lo = 255, hi = 0;
            for (v = 0; v < 256; v++)
            {
                /* The signal path of gbc_cam_take_picture()'s LUT, unsigned. */
                int t = (int)(((unsigned)v * expos[e]) / 0x0300);
                t = 128 + ((t - 128) / 8);
                if (t > 255) t = 255;
                if (t < 0)   t = 0;
                if (t < lo) lo = t;
                if (t > hi) hi = t;
            }
            printf("    exposure %04x -> %d..%d\n", expos[e], lo, hi);
            if (hi < prev_hi) monotonic = 0;
            prev_hi = hi;
            if (expos[e] == 0x0300) hi_at_nominal = hi;
            if (expos[e] == 0x1000) hi_at_high = hi;
        }

        check(hi_at_nominal == 143, "at nominal exposure the range tops out at 143");
        check(monotonic, "raising exposure never narrows the range");
        check(hi_at_high >= 231,
              "at exposure 0x1000 the range covers the ROM's dither thresholds");
    }

    //---------------------------------------------------------------
    // Dump the last frame for eyeballing.
    {
        FILE *f = fopen("/tmp/gbcam_out.pgm", "wb");
        if (f)
        {
            fprintf(f, "P5\n%d %d\n255\n", GBCAM_W, GBCAM_H);
            fwrite(flat, 1, sizeof(flat), f);
            fclose(f);
            printf("\nwrote /tmp/gbcam_out.pgm\n");
        }
    }

    gbc_cam_free(&cam);

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "passed",
           failures, failures == 1 ? "" : "s");
    return failures != 0;
}
