// Host-side checks for the multiple exposure maths (core/mexp.c).
//
// Three things are worth proving off-camera, because getting any of them wrong
// costs a reflash and a sequence of shots to find out:
//
//   1. the packed row helpers round-trip, and agree with what
//      get_raw_pixel/set_raw_pixel in core/raw.c do to the same bytes. They
//      exist only to avoid those two, and are worth having only if they are
//      bit-for-bit what they replace. A pack that is one bit out does not look
//      like a bug in the packing - it looks like a bent photograph, on a
//      camera whose whole purpose is bending photographs.
//
//   2. the row blend agrees with the per-pixel blend, for every mode. The row
//      version hoists the mode test out of the loop and so is a second copy of
//      the arithmetic; two copies drift.
//
//   3. no mode can leave the sensor's range, at any frame count, from any pair
//      of inputs. Everything downstream - the packers here, Canon's JPEG
//      developer, the DNG writer - assumes the buffer holds values that fit
//      the sensor's bit depth.
//
//   gcc -O2 -Wall -Wextra -I../include -o mexp_selftest mexp_selftest.c ../core/mexp.c
//   ./mexp_selftest
//
// Exits non-zero on any mismatch.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mexp.h"

static int failures = 0;

static void fail(const char *what, long a, long b)
{
    printf("FAIL %-52s %ld != %ld\n", what, a, b);
    failures++;
}

static void ok(const char *what)
{
    printf("ok   %s\n", what);
}

//-------------------------------------------------------------------
// The accessors from core/raw.c, copied verbatim so the helpers can be checked
// against the real thing rather than against themselves. rowlen is a whole
// number of groups in every case here.

static unsigned short ref_get(int bits, const unsigned char *base, unsigned x)
{
    if (bits == 10)
    {
        const unsigned char *addr = base + (x/8)*10;
        switch (x%8) {
            case 0: return ((0x3fc&(((unsigned short)addr[1])<<2)) | (addr[0] >> 6));
            case 1: return ((0x3f0&(((unsigned short)addr[0])<<4)) | (addr[3] >> 4));
            case 2: return ((0x3c0&(((unsigned short)addr[3])<<6)) | (addr[2] >> 2));
            case 3: return ((0x300&(((unsigned short)addr[2])<<8)) | (addr[5]));
            case 4: return ((0x3fc&(((unsigned short)addr[4])<<2)) | (addr[7] >> 6));
            case 5: return ((0x3f0&(((unsigned short)addr[7])<<4)) | (addr[6] >> 4));
            case 6: return ((0x3c0&(((unsigned short)addr[6])<<6)) | (addr[9] >> 2));
            case 7: return ((0x300&(((unsigned short)addr[9])<<8)) | (addr[8]));
        }
    }
    else if (bits == 12)
    {
        const unsigned char *addr = base + (x/4)*6;
        switch (x%4) {
            case 0: return ((unsigned short)(addr[1])        << 4) | (addr[0] >> 4);
            case 1: return ((unsigned short)(addr[0] & 0x0F) << 8) | (addr[3]);
            case 2: return ((unsigned short)(addr[2])        << 4) | (addr[5] >> 4);
            case 3: return ((unsigned short)(addr[5] & 0x0F) << 8) | (addr[4]);
        }
    }
    else
    {
        const unsigned char *addr = base + (x/8)*14;
        switch (x%8) {
            case 0: return ((unsigned short)(addr[ 1])        <<  6) | (addr[ 0] >> 2);
            case 1: return ((unsigned short)(addr[ 0] & 0x03) << 12) | (addr[ 3] << 4) | (addr[ 2] >> 4);
            case 2: return ((unsigned short)(addr[ 2] & 0x0F) << 10) | (addr[ 5] << 2) | (addr[ 4] >> 6);
            case 3: return ((unsigned short)(addr[ 4] & 0x3F) <<  8) | (addr[ 7]);
            case 4: return ((unsigned short)(addr[ 6])        <<  6) | (addr[ 9] >> 2);
            case 5: return ((unsigned short)(addr[ 9] & 0x03) << 12) | (addr[ 8] << 4) | (addr[11] >> 4);
            case 6: return ((unsigned short)(addr[11] & 0x0F) << 10) | (addr[10] << 2) | (addr[13] >> 6);
            case 7: return ((unsigned short)(addr[13] & 0x3F) <<  8) | (addr[12]);
        }
    }
    return 0;
}

static void ref_set(int bits, unsigned char *base, unsigned x, unsigned short value)
{
    if (bits == 10)
    {
        unsigned char *addr = base + (x/8)*10;
        switch (x%8) {
            case 0: addr[0]=(addr[0]&0x3F)|(value<<6); addr[1]=value>>2;                  break;
            case 1: addr[0]=(addr[0]&0xC0)|(value>>4); addr[3]=(addr[3]&0x0F)|(value<<4); break;
            case 2: addr[2]=(addr[2]&0x03)|(value<<2); addr[3]=(addr[3]&0xF0)|(value>>6); break;
            case 3: addr[2]=(addr[2]&0xFC)|(value>>8); addr[5]=value;                     break;
            case 4: addr[4]=value>>2;                  addr[7]=(addr[7]&0x3F)|(value<<6); break;
            case 5: addr[6]=(addr[6]&0x0F)|(value<<4); addr[7]=(addr[7]&0xC0)|(value>>4); break;
            case 6: addr[6]=(addr[6]&0xF0)|(value>>6); addr[9]=(addr[9]&0x03)|(value<<2); break;
            case 7: addr[8]=value;                     addr[9]=(addr[9]&0xFC)|(value>>8); break;
        }
    }
    else if (bits == 12)
    {
        unsigned char *addr = base + (x/4)*6;
        switch (x%4) {
            case 0: addr[0] = (addr[0]&0x0F) | (unsigned char)(value << 4);  addr[1] = (unsigned char)(value >> 4);  break;
            case 1: addr[0] = (addr[0]&0xF0) | (unsigned char)(value >> 8);  addr[3] = (unsigned char)value;         break;
            case 2: addr[2] = (unsigned char)(value >> 4);  addr[5] = (addr[5]&0x0F) | (unsigned char)(value << 4);  break;
            case 3: addr[4] = (unsigned char)value; addr[5] = (addr[5]&0xF0) | (unsigned char)(value >> 8);  break;
        }
    }
    else
    {
        unsigned char *addr = base + (x/8)*14;
        switch (x%8) {
            case 0: addr[ 0]=(addr[0]&0x03)|(value<< 2); addr[ 1]=value>>6;                                                         break;
            case 1: addr[ 0]=(addr[0]&0xFC)|(value>>12); addr[ 2]=(addr[ 2]&0x0F)|(value<< 4); addr[ 3]=value>>4;                   break;
            case 2: addr[ 2]=(addr[2]&0xF0)|(value>>10); addr[ 4]=(addr[ 4]&0x3F)|(value<< 6); addr[ 5]=value>>2;                   break;
            case 3: addr[ 4]=(addr[4]&0xC0)|(value>> 8); addr[ 7]=value;                                                            break;
            case 4: addr[ 6]=value>>6;                   addr[ 9]=(addr[ 9]&0x03)|(value<< 2);                                      break;
            case 5: addr[ 8]=value>>4;                   addr[ 9]=(addr[ 9]&0xFC)|(value>>12); addr[11]=(addr[11]&0x0F)|(value<<4); break;
            case 6: addr[10]=value>>2;                   addr[11]=(addr[11]&0xF0)|(value>>10); addr[13]=(addr[13]&0x3F)|(value<<6); break;
            case 7: addr[12]=value;                      addr[13]=(addr[13]&0xC0)|(value>> 8);                                      break;
        }
    }
}

//-------------------------------------------------------------------

#define ROWPIX  1024        // a whole number of groups at all three depths

static void test_pack(int bits)
{
    unsigned max = (1u << bits) - 1;
    unsigned rowlen = (ROWPIX * bits) / 8;
    unsigned char *packed = malloc(rowlen);
    unsigned char *refbuf = malloc(rowlen);
    unsigned short *v = malloc(ROWPIX * sizeof(unsigned short));
    unsigned short *back = malloc(ROWPIX * sizeof(unsigned short));
    char name[64];
    unsigned i;
    int bad;

    srand(1234 + bits);
    for (i = 0; i < ROWPIX; i++)
        v[i] = (unsigned short)(rand() & max);

    // Corners as well as noise: a group whose pixels are all zero and one whose
    // pixels are all saturated are the two patterns most likely to hide a
    // mask that is one bit wide in the wrong direction.
    for (i = 0; i < 8; i++)          v[i] = 0;
    for (i = 8; i < 16; i++)         v[i] = (unsigned short)max;

    // pack, then read back with the real accessor
    memset(packed, 0xa5, rowlen);
    mexp_row_pack(bits, v, packed, ROWPIX);

    bad = -1;
    for (i = 0; i < ROWPIX; i++)
        if (ref_get(bits, packed, i) != v[i]) { bad = (int)i; break; }
    sprintf(name, "%2dbpp mexp_row_pack matches set/get_raw_pixel", bits);
    if (bad >= 0) fail(name, ref_get(bits, packed, bad), v[bad]); else ok(name);

    // and the other way: lay a row down with the real accessor, unpack it
    memset(refbuf, 0, rowlen);
    for (i = 0; i < ROWPIX; i++)
        ref_set(bits, refbuf, i, v[i]);
    mexp_row_unpack(bits, refbuf, back, ROWPIX);

    bad = -1;
    for (i = 0; i < ROWPIX; i++)
        if (back[i] != v[i]) { bad = (int)i; break; }
    sprintf(name, "%2dbpp mexp_row_unpack matches set/get_raw_pixel", bits);
    if (bad >= 0) fail(name, back[bad], v[bad]); else ok(name);

    // round trip
    mexp_row_unpack(bits, packed, back, ROWPIX);
    bad = -1;
    for (i = 0; i < ROWPIX; i++)
        if (back[i] != v[i]) { bad = (int)i; break; }
    sprintf(name, "%2dbpp pack/unpack round trip", bits);
    if (bad >= 0) fail(name, back[bad], v[bad]); else ok(name);

    // and that packing wrote every byte of the row - a group loop that stops
    // one group short leaves the 0xa5 fill behind and nothing above would
    // notice, because the pixels it did write are all correct
    sprintf(name, "%2dbpp pack covers the whole row", bits);
    {
        int touched = 1;
        for (i = 0; i < rowlen; i++)
            if (packed[i] != refbuf[i]) { touched = 0; break; }
        if (!touched) fail(name, packed[i], refbuf[i]); else ok(name);
    }

    free(packed); free(refbuf); free(v); free(back);
}

//-------------------------------------------------------------------

static void test_blend_agrees(void)
{
    static const unsigned depths[3] = { 10, 12, 14 };
    unsigned d, n;
    int mode;
    int bad = 0;

    for (d = 0; d < 3; d++)
    {
        unsigned white = (1u << depths[d]) - 1;
        unsigned black = (1u << (depths[d] - 5)) - 1;

        for (mode = 0; mode < MEXP_MODE_COUNT; mode++)
        {
            for (n = 1; n <= MEXP_FRAMES_MAX; n++)
            {
                unsigned short a[9], b[9], r[9];
                unsigned i;

                a[0] = 0;               b[0] = 0;
                a[1] = (unsigned short)white; b[1] = (unsigned short)white;
                a[2] = 0;               b[2] = (unsigned short)white;
                a[3] = (unsigned short)white; b[3] = 0;
                a[4] = (unsigned short)black; b[4] = (unsigned short)black;
                a[5] = (unsigned short)(white/2); b[5] = (unsigned short)(white/3);
                a[6] = 1;               b[6] = 2;
                a[7] = (unsigned short)(white-1); b[7] = (unsigned short)(white-2);
                a[8] = (unsigned short)black; b[8] = (unsigned short)white;

                for (i = 0; i < 9; i++)
                    r[i] = mexp_blend_px(mode, a[i], b[i], n, black, white);

                mexp_row_blend(mode, a, b, 9, n, black, white);

                for (i = 0; i < 9; i++)
                {
                    if (a[i] != r[i])
                    {
                        char name[80];
                        sprintf(name, "%2ubpp mode %d n=%u row blend == per-pixel blend",
                                depths[d], mode, n);
                        fail(name, a[i], r[i]);
                        bad = 1;
                        break;
                    }
                    if (a[i] > white)
                    {
                        char name[80];
                        sprintf(name, "%2ubpp mode %d n=%u stays inside the sensor range",
                                depths[d], mode, n);
                        fail(name, a[i], white);
                        bad = 1;
                        break;
                    }
                }
                if (bad) return;
            }
        }
    }
    ok("row blend == per-pixel blend, every mode, every depth, every count");
    ok("no mode leaves the sensor range");
}

//-------------------------------------------------------------------
// What the modes are supposed to mean, stated as tests rather than as a
// comment, because "additive" and "average" are the two that a reader could
// reasonably expect to be the other one.

static void test_semantics(void)
{
    unsigned white = 4095, black = 127;
    unsigned i;

    if (mexp_blend_px(MEXP_LIGHTEN, 100, 900, 1, black, white) != 900)
        fail("lighten keeps the brighter pixel", mexp_blend_px(MEXP_LIGHTEN,100,900,1,black,white), 900);
    else ok("lighten keeps the brighter pixel");

    if (mexp_blend_px(MEXP_DARKEN, 100, 900, 1, black, white) != 100)
        fail("darken keeps the darker pixel", mexp_blend_px(MEXP_DARKEN,100,900,1,black,white), 100);
    else ok("darken keeps the darker pixel");

    // Two exposures of the same black frame stay black rather than becoming
    // two pedestals of grey. This is the whole reason for the black-level term.
    if (mexp_blend_px(MEXP_ADD, black, black, 1, black, white) != black)
        fail("additive: black + black = black", mexp_blend_px(MEXP_ADD,black,black,1,black,white), black);
    else ok("additive: black + black = black");

    if (mexp_blend_px(MEXP_ADD, 3000, 3000, 1, black, white) != white)
        fail("additive clips at saturation", mexp_blend_px(MEXP_ADD,3000,3000,1,black,white), white);
    else ok("additive clips at saturation");

    if (mexp_blend_px(MEXP_AVERAGE, 1000, 2000, 1, black, white) != 1500)
        fail("average of two", mexp_blend_px(MEXP_AVERAGE,1000,2000,1,black,white), 1500);
    else ok("average of two");

    // The running average is the point of the storage format, so check it
    // against a real mean over the whole frame count rather than at one step.
    // Under one LSB of drift per exposure is the claim in mexp.c.
    {
        unsigned short acc = 100;
        unsigned sum = 100;
        int worst = 0;
        for (i = 1; i <= MEXP_FRAMES_MAX - 1u; i++)
        {
            unsigned v = 200 + i * 373;
            if (v > white) v = white;
            sum += v;
            acc = mexp_blend_px(MEXP_AVERAGE, acc, v, i, black, white);
            {
                int exact = (int)((sum + (i+1)/2) / (i+1));
                int err = (int)acc - exact;
                if (err < 0) err = -err;
                if (err > worst) worst = err;
            }
        }
        if (worst > 1) fail("running average stays within 1 LSB of the true mean", worst, 1);
        else ok("running average stays within 1 LSB of the true mean");
    }
}

//-------------------------------------------------------------------

static void test_ghost_levels(void)
{
    unsigned white = 4095, black = 127;
    int lo = mexp_ghost_level(black, black, white);
    int hi = mexp_ghost_level(white, black, white);
    int mid = mexp_ghost_level((white + black) / 2, black, white);
    unsigned v;
    int last = 0, monotonic = 1;

    if (lo != 0) fail("ghost: black maps to the darkest level", lo, 0); else ok("ghost: black maps to the darkest level");
    if (hi != MEXP_GHOST_LEVELS-1) fail("ghost: saturation maps to the lightest level", hi, MEXP_GHOST_LEVELS-1);
    else ok("ghost: saturation maps to the lightest level");

    // Mid grey has to land in the middle of the ramp, not at the bottom. This
    // is what the gamma is for, and a linear mapping fails it.
    if (mid < 2) fail("ghost: mid scene grey is not crushed to black", mid, 2);
    else ok("ghost: mid scene grey is not crushed to black");

    for (v = black; v <= white; v += 7)
    {
        int l = mexp_ghost_level(v, black, white);
        if (l < last) { monotonic = 0; break; }
        last = l;
    }
    if (!monotonic) fail("ghost: levels never go backwards", v, 0);
    else ok("ghost: levels never go backwards");

    // Degenerate sensor config must not divide by zero
    if (mexp_ghost_level(500, 500, 500) != 0) fail("ghost: white == black is survivable", 1, 0);
    else ok("ghost: white == black is survivable");
}

//-------------------------------------------------------------------

static void test_state(void)
{
    mexp_begin(MEXP_ADD, 3, 0);
    if (mexp_count != 0 || mexp_target != 3 || mexp_mode_used != MEXP_ADD || mexp_bend_each_used != 0)
        fail("begin sets mode and target and clears the count", mexp_target, 3);
    else ok("begin sets mode and target and clears the count");

    if (mexp_in_progress()) fail("a sequence with no exposures is not in progress", 1, 0);
    else ok("a sequence with no exposures is not in progress");

    mexp_count = 1;
    if (!mexp_in_progress()) fail("a part-finished sequence is in progress", 0, 1);
    else ok("a part-finished sequence is in progress");

    mexp_count = 3;
    if (mexp_in_progress()) fail("a finished sequence is not in progress", 1, 0);
    else ok("a finished sequence is not in progress");

    // Out of range settings are clamped rather than trusted: these come from a
    // config file on the card, which a build with different limits may have
    // written.
    mexp_begin(-1, 99, 1);
    if (mexp_mode_used != MEXP_AVERAGE || mexp_target != MEXP_FRAMES_MAX)
        fail("begin clamps a config from another build", mexp_target, MEXP_FRAMES_MAX);
    else ok("begin clamps a config from another build");

    mexp_begin(MEXP_MODE_COUNT, 0, 1);
    if (mexp_mode_used != MEXP_AVERAGE || mexp_target != MEXP_FRAMES_MIN)
        fail("begin clamps the other way", mexp_target, MEXP_FRAMES_MIN);
    else ok("begin clamps the other way");
}

//-------------------------------------------------------------------

int main(void)
{
    test_pack(10);
    test_pack(12);
    test_pack(14);
    test_blend_agrees();
    test_semantics();
    test_ghost_levels();
    test_state();

    if (failures)
    {
        printf("\n%d failure(s)\n", failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
