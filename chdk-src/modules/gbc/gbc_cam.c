//-------------------------------------------------------------------
// MAC-GBD mapper + M64282FP sensor - see gbc_cam.h.
//
// Ported from AntonioND's reference implementation (GPL-3). Two deliberate
// departures from it, both forced by the target:
//
//   int -> short   The reference works in int[128][120] buffers. Three of
//                  those is 184K of stack; the A480 GUI task does not have it.
//                  Every intermediate here is clamped to -128..127 or 0..255
//                  before it is stored, and the widest transient is
//                  4*255+255 scaled by 5 = ~6.4K, so short is provably enough.
//
//   float -> 1/4s  The edge-enhancement ratios are all multiples of 0.25
//                  ({0.5, 0.75, 1, 1.25, 2, 3, 4, 5}), so the LUT holds
//                  quarters and the multiply stays integer. DIGIC III has no
//                  FPU and softfloat in a per-pixel loop is not worth it.
//-------------------------------------------------------------------

#include "gbc_cam.h"

#define BIT(n) (1u << (n))

#define SENSOR_PIXELS   (GBCAM_SENSOR_W * GBCAM_SENSOR_H)
#define SIDX(x,y)       ((y) * GBCAM_SENSOR_W + (x))

//-------------------------------------------------------------------

static inline int clamp_int(int lo, int v, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int min_int(int a, int b) { return (a < b) ? a : b; }
static inline int max_int(int a, int b) { return (a > b) ? a : b; }

//-------------------------------------------------------------------

int gbc_cam_init(gbc_cam_t *c)
{
    memset(c, 0, sizeof(*c));

    // Not 0: bank 0 is fixed at 0000-3fff, so the switchable window powers up
    // showing bank 1. memset would have left this pointing at a mirror of the
    // header, and the ROM would run its own vectors as game code.
    c->rom_bank = 1;

    c->sram = (unsigned char *)malloc(GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE);
    if (!c->sram)
        return -1;
    memset(c->sram, 0, GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE);

    c->retina = (short *)malloc(SENSOR_PIXELS * sizeof(short));
    c->temp   = (short *)malloc(SENSOR_PIXELS * sizeof(short));
    if (!c->retina || !c->temp)
    {
        gbc_cam_free(c);
        return -1;
    }

    return 0;
}

void gbc_cam_free(gbc_cam_t *c)
{
    if (c->sram)   { free(c->sram);   c->sram = 0; }
    if (c->retina) { free(c->retina); c->retina = 0; }
    if (c->temp)   { free(c->temp);   c->temp = 0; }
}

//-------------------------------------------------------------------
// The dithering matrix. Three thresholds per position in a 4x4 tile, held in
// registers 6..53, turning a byte of luminance into one of four Game Boy
// shades. This is the register block the ROM's brightness and contrast
// sliders actually write to.

static inline unsigned matrix_process(unsigned value, unsigned x, unsigned y,
                                      const unsigned char *reg)
{
    int base = 6 + (((y & 3) * 4 + (x & 3)) * 3);

    unsigned r0 = reg[base + 0];
    unsigned r1 = reg[base + 1];
    unsigned r2 = reg[base + 2];

    if (value < r0) return 0x00;
    if (value < r1) return 0x40;
    if (value < r2) return 0x80;
    return 0xc0;
}

//-------------------------------------------------------------------
// One exposure. Everything from here to the memcpy at the bottom is the
// cartridge's own signal path, in the order the silicon does it.

static void gbc_cam_take_picture(gbc_cam_t *c)
{
    const unsigned char *reg = c->reg;
    short *retina = c->retina;
    short *temp   = c->temp;
    int i, j;

    // Edge ratios in quarters - see the header comment.
    static const int edge_ratio_q4[8] = { 2, 3, 4, 5, 8, 12, 16, 20 };

    //---------------------------------------------------------------
    // Pull a frame off the Canon sensor. It lands as 0..255 luminance in the
    // low byte of retina[]; the pipeline below widens it in place.

    {
        unsigned char *src = (unsigned char *)temp;   // borrow temp as a byte buffer
        if (gbc_feed_capture(src) != 0)
            memset(src, 128, SENSOR_PIXELS);
        for (i = SENSOR_PIXELS - 1; i >= 0; i--)
            retina[i] = (short)src[i];
    }

    //---------------------------------------------------------------
    // Register decode.

    unsigned P_bits = 0, M_bits = 0;
    switch ((reg[0] >> 1) & 3)
    {
        case 0:            P_bits = 0x00; M_bits = 0x01; break;
        case 1:            P_bits = 0x01; M_bits = 0x00; break;
        case 2: case 3:    P_bits = 0x01; M_bits = 0x02; break;
    }

    unsigned N_bit    = (reg[1] & BIT(7)) >> 7;
    unsigned VH_bits  = (reg[1] & (BIT(6) | BIT(5))) >> 5;
    unsigned exposure = reg[3] | (reg[2] << 8);

    int      edge_q4  = edge_ratio_q4[(reg[4] & 0x70) >> 4];
    unsigned E3_bit   = (reg[4] & BIT(7)) >> 7;
    unsigned I_bit    = (reg[4] & BIT(3)) >> 3;

    // How long the ROM will see the busy bit set.
    //
    // The reference reproduces the sensor's real timing so the game's own
    // "please wait" animation runs for about the right length. On a body that
    // emulates at a handful of frames a second, that is a bad trade: the wait
    // is a spin loop on the emulated CPU, and the exposure it is waiting for
    // has already happened - gbc_feed_capture() is synchronous.
    //
    // It matters most at boot. The ROM's sensor calibration takes 79 exposures
    // back to back with the LCD switched off, and at full timing that is ~205
    // emulated frames of pure spinning - about fifty seconds of blank white
    // screen on the A480, which is indistinguishable from a hung emulator and
    // was reported as one.
    //
    // The busy flag still sets and still clears in the right order, so ROMs
    // that poll it (all of them) behave identically; only the dead time
    // shrinks. GBCAM_BUSY_DIV is the one knob - raise it if a ROM turns out to
    // depend on the real duration.
    //
    // 4 rather than something larger because that is where the benefit stops:
    // measured with tools/gbc_boot_selftest.c, the first frame the camera ROM
    // renders anything into goes 244 -> 165 at DIV=4, and 8 and 16 are also
    // 165. The remaining 165 frames are not the sensor being waited on, so
    // there is nothing to buy by distorting the timing further. The exposure
    // count over the boot sequence is 79 at every divisor, which is the check
    // that the ROM's calibration loop still converges the same way.
#ifndef GBCAM_BUSY_DIV
#define GBCAM_BUSY_DIV  4
#endif
    c->clocks_left = (4 * (32446 + (N_bit ? 0 : 512) + 16 * (int)exposure))
                     / GBCAM_BUSY_DIV;

    //---------------------------------------------------------------
    // Exposure, supply-voltage range, inversion and the conversion to signed,
    // as a single 256-entry lookup.
    //
    // Upstream does these as four separate passes over all 15360 pixels, and
    // the first of them divides by 0x0300 - which on ARM946 is a call into
    // __aeabi_uidiv, 15360 times per exposure, for a viewfinder that takes
    // exposures continuously.
    //
    // Every one of those steps is a pure function of the pixel's own 0..255
    // value and constants fixed for this capture, so the whole chain folds
    // into a table built with 256 divisions and applied with a load. That
    // also collapses four passes over the buffer into one, which on a machine
    // this cache-poor matters about as much as the arithmetic.
    //
    // There is deliberately NO clamp between the exposure step and the voltage
    // step, and that is load-bearing. The reference clamps exactly once, after
    // the voltage adaptation - verified against AntonioND's sample_code.c, not
    // assumed.
    //
    // It is tempting to "restore" a clamp at the pass boundary. Doing so breaks
    // the camera completely, because the intermediate range is the entire
    // mechanism by which exposure works:
    //
    //   clamped:    the /8 always receives 0..255, so it always emits 112..143,
    //               whatever exposure the ROM asked for. The exposure register
    //               becomes a no-op.
    //   unclamped:  at exposure 0x1000 the /8 receives 0..1360 and emits
    //               112..255. Raising exposure widens the output range.
    //
    // The ROM's auto-exposure depends on the second. It sets its 48 dither
    // thresholds - observed at 140..231 in the viewfinder - and then winds
    // exposure until the picture spans them. Pin the range to 112..143 and
    // every pixel sits below every threshold; matrix_process() maps
    // "below all thresholds" to the darkest shade, so the viewfinder goes
    // black and the AE loop can never climb out. That is not a hypothetical:
    // it was measured at 69% black with tools/gbc_boot_selftest.c.

    {
        short lut[256];
        int v;

        for (v = 0; v < 256; v++)
        {
            int t = (v * (int)exposure) / 0x0300;
            t = 128 + ((t - 128) / 8);          // "adapt" to 3.1/5.0 V
            t = clamp_int(0, t, 255);           // the reference's only clamp
            if (I_bit)
                t = 255 - t;
            lut[v] = (short)(t - 128);          // signed, for the filter stage
        }

        for (i = 0; i < SENSOR_PIXELS; i++)
            retina[i] = lut[retina[i] & 0xff];
    }

    //---------------------------------------------------------------
    // Filtering. The mode is a 4-bit field assembled from three registers and
    // only four values of it are legal on this cartridge.

    unsigned mode = (N_bit << 3) | (VH_bits << 1) | E3_bit;

    switch (mode)
    {
    case 0x0:   // 1-D filtering
        memcpy(temp, retina, SENSOR_PIXELS * sizeof(short));
        for (j = 0; j < GBCAM_SENSOR_H; j++)
            for (i = 0; i < GBCAM_SENSOR_W; i++)
            {
                int ms = temp[SIDX(i, min_int(j + 1, GBCAM_SENSOR_H - 1))];
                int px = temp[SIDX(i, j)];
                int value = 0;
                if (P_bits & BIT(0)) value += px;
                if (P_bits & BIT(1)) value += ms;
                if (M_bits & BIT(0)) value -= px;
                if (M_bits & BIT(1)) value -= ms;
                retina[SIDX(i, j)] = (short)clamp_int(-128, value, 127);
            }
        break;

    case 0x2:   // 1-D filtering + horizontal enhancement: P + {2P-(MW+ME)}*a
        for (j = 0; j < GBCAM_SENSOR_H; j++)
            for (i = 0; i < GBCAM_SENSOR_W; i++)
            {
                int mw = retina[SIDX(max_int(0, i - 1), j)];
                int me = retina[SIDX(min_int(i + 1, GBCAM_SENSOR_W - 1), j)];
                int px = retina[SIDX(i, j)];
                temp[SIDX(i, j)] = (short)clamp_int(0,
                        px + (((2 * px - mw - me) * edge_q4) / 4), 255);
            }
        for (j = 0; j < GBCAM_SENSOR_H; j++)
            for (i = 0; i < GBCAM_SENSOR_W; i++)
            {
                int ms = temp[SIDX(i, min_int(j + 1, GBCAM_SENSOR_H - 1))];
                int px = temp[SIDX(i, j)];
                int value = 0;
                if (P_bits & BIT(0)) value += px;
                if (P_bits & BIT(1)) value += ms;
                if (M_bits & BIT(0)) value -= px;
                if (M_bits & BIT(1)) value -= ms;
                retina[SIDX(i, j)] = (short)clamp_int(-128, value, 127);
            }
        break;

    case 0xe:   // 2-D enhancement: P + {4P-(MN+MS+ME+MW)}*a
        for (j = 0; j < GBCAM_SENSOR_H; j++)
            for (i = 0; i < GBCAM_SENSOR_W; i++)
            {
                int ms = retina[SIDX(i, min_int(j + 1, GBCAM_SENSOR_H - 1))];
                int mn = retina[SIDX(i, max_int(0, j - 1))];
                int mw = retina[SIDX(max_int(0, i - 1), j)];
                int me = retina[SIDX(min_int(i + 1, GBCAM_SENSOR_W - 1), j)];
                int px = retina[SIDX(i, j)];
                temp[SIDX(i, j)] = (short)clamp_int(-128,
                        px + (((4 * px - mw - me - mn - ms) * edge_q4) / 4), 127);
            }
        memcpy(retina, temp, SENSOR_PIXELS * sizeof(short));
        break;

    case 0x1:
        // AntonioND's cartridge returns a flat colour here and the sensor
        // datasheet does not document the combination. Reproduced rather than
        // guessed at - a ROM that lands here gets what the hardware gives.
        memset(retina, 0, SENSOR_PIXELS * sizeof(short));
        break;

    default:
        // Unknown mode: pass the frame through unfiltered. The reference
        // printf()s and does the same.
        break;
    }

    // The "back to unsigned" pass that used to sit here - retina[i] += 128 over
    // all 15360 pixels - is folded into the dither loop below, which is the
    // only thing that reads the result and only reads the 112 visible rows.

    //---------------------------------------------------------------
    // Controller stage: dither to 2bpp and pack into Game Boy tiles.
    //
    // The sensor's extra dark rows are split top and bottom, so the visible
    // 112 lines start half of them in.

    unsigned char *out = &c->sram[GBCAM_IMAGE_OFFSET];
    memset(out, 0, GBCAM_IMAGE_BYTES);

    // Everything that depends only on the row is hoisted out of the inner loop:
    // the source row, the tile row within the packed output, and the row of the
    // 4x4 dither matrix. That leaves the inner loop with no multiplies at all -
    // upstream recomputed a SIDX, a tile address and the matrix base index per
    // pixel, three multiplies each time, 14336 times per exposure on a core
    // where the viewfinder captures continuously.
    for (j = 0; j < GBCAM_H; j++)
    {
        int sy = j + (GBCAM_EXTRA_LINES / 2);
        const short *srow = &retina[sy * GBCAM_SENSOR_W];

        // finalbuffer[j>>3][i>>3] in the reference: 16 tiles per row,
        // 16 bytes per tile, two bytes per tile line.
        unsigned char *trow = out + (j >> 3) * (GBCAM_IMAGE_TILES_X * 16)
                                  + (j & 7) * 2;

        // matrix_process()'s base is 6 + ((y&3)*4 + (x&3))*3; the y part is
        // fixed for the row.
        const unsigned char *mrow = &reg[6 + (j & 3) * 12];

        for (i = 0; i < GBCAM_W; i++)
        {
            int v = srow[i] + 128;              // the folded "back to unsigned"
            const unsigned char *m = mrow + (i & 3) * 3;
            unsigned char *tile = trow + (i >> 3) * 16;
            unsigned bit = 1u << (7 - (i & 7));
            unsigned outcolor;

            v = clamp_int(0, v, 255);

            // matrix_process() inlined, with its 0x00/0x40/0x80/0xc0 return and
            // the caller's 3 - (r >> 6) collapsed into the shade directly.
            if      ((unsigned)v < m[0]) outcolor = 3;
            else if ((unsigned)v < m[1]) outcolor = 2;
            else if ((unsigned)v < m[2]) outcolor = 1;
            else                         outcolor = 0;

            if (outcolor & 1) tile[0] |= bit;
            if (outcolor & 2) tile[1] |= bit;
        }
    }

    c->captures++;
}

//-------------------------------------------------------------------
// Mapper.

void gbc_cam_write(gbc_cam_t *c, unsigned short addr, unsigned char val)
{
    if (addr < 0x2000)
    {
        int was_enabled = c->sram_enabled;

        c->sram_enabled = ((val & 0x0f) == 0x0a);

        // Disabling cartridge RAM is how a real battery-backed cartridge is
        // told the write is finished and it is safe to commit. Games and the
        // camera ROM both do it immediately after saving, so it is the natural
        // moment to put the album on the card - and far more reliable than
        // waiting for a clean exit, which a player who simply switches the
        // camera off never performs.
        // Only raise the request. The work happens in gbc_save_service(), back
        // in the module's frame loop - see the note on commit_pending.
        if (was_enabled && !c->sram_enabled && c->dirty)
        {
            c->commit_pending = 1;
            c->export_slot = 0;
        }

        return;
    }

    if (addr < 0x4000)
    {
        // 6-bit ROM bank. Bank 0 is not selectable here; the hardware
        // substitutes 1, same as MBC1. The mmu reads this back and hands it
        // to the pager - the cartridge does not own the ROM window itself.
        int bank = val & 0x3f;
        c->rom_bank = bank ? bank : 1;
        return;
    }

    if (addr < 0x6000)
    {
        if (val & 0x10)
        {
            c->regs_selected = 1;
        }
        else
        {
            c->regs_selected = 0;
            c->sram_bank = val & 0x0f;
        }
        return;
    }

    if (addr >= 0xa000 && addr < 0xc000)
    {
        if (c->regs_selected)
        {
            unsigned idx = (addr - 0xa000) % GBCAM_REG_MIRROR;
            if (idx >= GBCAM_NUM_REGS)
                return;                          // mirrored gap, ignored

            c->reg[idx] = val;

            // Writing bit 0 of register 0 arms the shutter. Everything else
            // is configuration that only matters at that moment.
            if (idx == 0 && (val & BIT(0)))
                gbc_cam_take_picture(c);
            return;
        }

        if (c->sram_enabled)
        {
            c->sram[c->sram_bank * GBCAM_SRAM_BANKSIZE + (addr - 0xa000)] = val;
            c->dirty = 1;
        }
        return;
    }
}

//-------------------------------------------------------------------

unsigned char gbc_cam_read(gbc_cam_t *c, unsigned short addr)
{
    if (addr >= 0xa000 && addr < 0xc000)
    {
        if (c->regs_selected)
        {
            unsigned idx = (addr - 0xa000) % GBCAM_REG_MIRROR;
            if (idx != 0)
                return 0x00;                     // only register 0 reads back

            // Bit 0 is the busy flag. The ROM spins on this after triggering.
            return (unsigned char)(c->clocks_left ? 1 : 0);
        }

        // Reads are NOT gated by the RAM-enable latch.
        //
        // On MAC-GBD the 0x0a written to 0000-1fff enables *writing* to RAM;
        // reading RAM and registers is always enabled (Pan Docs, "Game Boy
        // Camera"). This is where the MBCs and this cartridge part company,
        // and porting the MBC habit across is an easy mistake: gating reads on
        // sram_enabled meant the camera ROM's own copy loop - which reads the
        // picture with the latch off, quite legitimately - got 0xff for every
        // one of 62720 reads, so nothing was ever copied into VRAM and the
        // viewfinder stayed black while the cartridge cheerfully went on
        // taking exposures.
        //
        // While the capture unit is running, RAM reads return 0x00 rather than
        // the previous picture.
        if (c->clocks_left)
            return 0x00;

        return c->sram[c->sram_bank * GBCAM_SRAM_BANKSIZE + (addr - 0xa000)];
    }

    return 0xff;
}

//-------------------------------------------------------------------

void gbc_cam_step(gbc_cam_t *c, int cycles)
{
    if (c->clocks_left > 0)
    {
        c->clocks_left -= cycles;
        if (c->clocks_left < 0)
            c->clocks_left = 0;
    }
}
