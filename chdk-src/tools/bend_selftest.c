// Host-side checks for the bend engine (core/bend.c) and for the packed 10bpp
// fast path in core/raw.c.
//
// Two things are worth proving off-camera, because getting either wrong is a
// reflash to find out:
//
//   1. the fast path packs and unpacks exactly the way CHDK's own
//      get_raw_pixel/set_raw_pixel do - it exists only because those recompute
//      an address and run an 8-way switch 7.4 million times a shot, and it is
//      worth having only if it is bit-for-bit identical to what it replaces
//
//   2. the routing matrix reproduces the six hardcoded modes it replaced. The
//      matrix is a strictly larger model, but "strictly larger" is a claim,
//      and a bend that used to work has to still work.
//
//   gcc -O2 -Wall -I../include -o bend_selftest bend_selftest.c ../core/bend.c
//   ./bend_selftest
//
// Exits non-zero on any mismatch.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bend.h"

#define NBITS   10
#define ROWPIX  3152                    // CAM_RAW_ROWPIX for the a470
#define ROWLEN  ((ROWPIX * 10) / 8)
#define BB_MAX  1023

static unsigned char  buf[ROWLEN];
static unsigned char  ref[ROWLEN];
static unsigned short lut[1024];
static unsigned short lut8[256];

//-------------------------------------------------------------------
// verbatim from core/raw.c (CAM_SENSOR_BITS_PER_PIXEL == 10)

static unsigned short get_raw_pixel(unsigned char *rawadr, unsigned int x)
{
    unsigned char *addr = rawadr + (x / 8) * 10;
    switch (x % 8) {
        case 0: return ((0x3fc&(((unsigned short)addr[1])<<2)) | (addr[0] >> 6));
        case 1: return ((0x3f0&(((unsigned short)addr[0])<<4)) | (addr[3] >> 4));
        case 2: return ((0x3c0&(((unsigned short)addr[3])<<6)) | (addr[2] >> 2));
        case 3: return ((0x300&(((unsigned short)addr[2])<<8)) | (addr[5]));
        case 4: return ((0x3fc&(((unsigned short)addr[4])<<2)) | (addr[7] >> 6));
        case 5: return ((0x3f0&(((unsigned short)addr[7])<<4)) | (addr[6] >> 4));
        case 6: return ((0x3c0&(((unsigned short)addr[6])<<6)) | (addr[9] >> 2));
        case 7: return ((0x300&(((unsigned short)addr[9])<<8)) | (addr[8]));
    }
    return 0;
}

static void set_raw_pixel(unsigned char *rawadr, unsigned int x, unsigned short value)
{
    unsigned char *addr = rawadr + (x / 8) * 10;
    switch (x % 8) {
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

static void bb_row_fast(unsigned char *p, unsigned int rowpix)
{
    unsigned int groups = rowpix >> 3;
    unsigned int i;

    for (i = 0; i < groups; i++, p += 10)
    {
        unsigned int p0 = ((0x3fc & (p[1] << 2)) | (p[0] >> 6));
        unsigned int p1 = ((0x3f0 & (p[0] << 4)) | (p[3] >> 4));
        unsigned int p2 = ((0x3c0 & (p[3] << 6)) | (p[2] >> 2));
        unsigned int p3 = ((0x300 & (p[2] << 8)) | (p[5]));
        unsigned int p4 = ((0x3fc & (p[4] << 2)) | (p[7] >> 6));
        unsigned int p5 = ((0x3f0 & (p[7] << 4)) | (p[6] >> 4));
        unsigned int p6 = ((0x3c0 & (p[6] << 6)) | (p[9] >> 2));
        unsigned int p7 = ((0x300 & (p[9] << 8)) | (p[8]));

        p0 = lut[p0]; p1 = lut[p1]; p2 = lut[p2]; p3 = lut[p3];
        p4 = lut[p4]; p5 = lut[p5]; p6 = lut[p6]; p7 = lut[p7];

        p[0] = (unsigned char)(((p0 & 0x003) << 6) | ((p1 >> 4) & 0x3f));
        p[1] = (unsigned char)((p0 >> 2) & 0xff);
        p[2] = (unsigned char)(((p2 & 0x03f) << 2) | ((p3 >> 8) & 0x03));
        p[3] = (unsigned char)(((p1 & 0x00f) << 4) | ((p2 >> 6) & 0x0f));
        p[4] = (unsigned char)((p4 >> 2) & 0xff);
        p[5] = (unsigned char)(p3 & 0xff);
        p[6] = (unsigned char)(((p5 & 0x00f) << 4) | ((p6 >> 6) & 0x0f));
        p[7] = (unsigned char)(((p4 & 0x003) << 6) | ((p5 >> 4) & 0x3f));
        p[8] = (unsigned char)(p7 & 0xff);
        p[9] = (unsigned char)(((p6 & 0x03f) << 2) | ((p7 >> 8) & 0x03));
    }
}

//-------------------------------------------------------------------
// verbatim from core/raw.c (CAM_SENSOR_BITS_PER_PIXEL == 12) - the a480.
// Six packed bytes hold four pixels instead of ten holding eight.

#define ROWPIX12 3720                   // CAM_RAW_ROWPIX for the a480
#define ROWLEN12 ((ROWPIX12 * 12) / 8)
#define BB_MAX12 4095

static unsigned char  buf12[ROWLEN12];
static unsigned char  ref12[ROWLEN12];
static unsigned short lut12[4096];

static unsigned short get_raw_pixel12(unsigned char *rawadr, unsigned int x)
{
    unsigned char *addr = rawadr + (x / 4) * 6;
    switch (x % 4) {
        case 0: return ((unsigned short)(addr[1])        << 4) | (addr[0] >> 4);
        case 1: return ((unsigned short)(addr[0] & 0x0F) << 8) | (addr[3]);
        case 2: return ((unsigned short)(addr[2])        << 4) | (addr[5] >> 4);
        case 3: return ((unsigned short)(addr[5] & 0x0F) << 8) | (addr[4]);
    }
    return 0;
}

static void set_raw_pixel12(unsigned char *rawadr, unsigned int x, unsigned short value)
{
    unsigned char *addr = rawadr + (x / 4) * 6;
    switch (x % 4) {
        case 0: addr[0] = (addr[0]&0x0F) | (unsigned char)(value << 4); addr[1] = (unsigned char)(value >> 4); break;
        case 1: addr[0] = (addr[0]&0xF0) | (unsigned char)(value >> 8); addr[3] = (unsigned char)value;        break;
        case 2: addr[2] = (unsigned char)(value >> 4); addr[5] = (addr[5]&0x0F) | (unsigned char)(value << 4); break;
        case 3: addr[4] = (unsigned char)value;        addr[5] = (addr[5]&0xF0) | (unsigned char)(value >> 8); break;
    }
}

// ---- verbatim from core/raw.c ----
static void bb_row_fast12(unsigned char *p, unsigned int rowpix)
{
    unsigned int groups = rowpix >> 2;
    unsigned int i;

    for (i = 0; i < groups; i++, p += 6)
    {
        unsigned int p0 = ((unsigned int)p[1] << 4) | (p[0] >> 4);
        unsigned int p1 = ((unsigned int)(p[0] & 0x0f) << 8) | p[3];
        unsigned int p2 = ((unsigned int)p[2] << 4) | (p[5] >> 4);
        unsigned int p3 = ((unsigned int)(p[5] & 0x0f) << 8) | p[4];

        p0 = lut12[p0]; p1 = lut12[p1]; p2 = lut12[p2]; p3 = lut12[p3];

        p[0] = (unsigned char)(((p0 & 0x00f) << 4) | ((p1 >> 8) & 0x0f));
        p[1] = (unsigned char)((p0 >> 4) & 0xff);
        p[2] = (unsigned char)((p2 >> 4) & 0xff);
        p[3] = (unsigned char)(p1 & 0xff);
        p[4] = (unsigned char)(p3 & 0xff);
        p[5] = (unsigned char)(((p2 & 0x00f) << 4) | ((p3 >> 8) & 0x0f));
    }
}

//-------------------------------------------------------------------
// The six modes the matrix replaced, transcribed from the version of
// core/raw.c that shipped before it. These are the regression oracle.

static unsigned old_swap(unsigned v, int a, int b)
{
    unsigned va, vb;
    if (a == b) return v;
    va = (v >> a) & 1;
    vb = (v >> b) & 1;
    return (v & ~((1u << a) | (1u << b))) | (va << b) | (vb << a);
}
static unsigned old_rotate(unsigned v, int a)
{
    if (!a) return v;
    return ((v << a) | (v >> (NBITS - a))) & BB_MAX;
}
static unsigned old_reverse(unsigned v)
{
    unsigned o = 0;
    int i;
    for (i = 0; i < NBITS; i++)
        if (v & (1u << i)) o |= 1u << (NBITS - 1 - i);
    return o;
}
static unsigned old_xor(unsigned v, unsigned m)  { return v ^ (m & BB_MAX); }
static unsigned old_kill(unsigned v, int a)      { return v & ~(1u << a); }
static unsigned old_stuck(unsigned v, int a)     { return v | (1u << a); }

//-------------------------------------------------------------------

static int fail;

static void check(const char *what, int ok)
{
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fail = 1;
}

static void mkbend(bend_t *b) { bend_reset(b, NBITS); }

static void compile10(const bend_t *b, bend_cc_t *cc) { bend_compile(b, cc, lut, NBITS); }

//-------------------------------------------------------------------
// 1. Packing

static int test_roundtrip_identity(void)
{
    unsigned int x;
    int i;
    for (i = 0; i < 1024; i++) lut[i] = i;

    srand(1);
    for (i = 0; i < ROWLEN; i++) buf[i] = rand() & 0xff;
    memcpy(ref, buf, ROWLEN);

    bb_row_fast(buf, ROWPIX);
    if (memcmp(buf, ref, ROWLEN) != 0) return 0;

    for (x = 0; x < ROWPIX; x++)
        if (get_raw_pixel(buf, x) != get_raw_pixel(ref, x)) return 0;
    return 1;
}

// The whole point of the fast path: for an arbitrary compiled table it must
// produce byte-for-byte what a get/modify/set loop through the reference
// accessors produces.
static int test_fastpath_matches_accessors(void)
{
    int trial;

    for (trial = 0; trial < 200; trial++)
    {
        bend_t b;
        bend_cc_t cc;
        unsigned int x;

        mkbend(&b);
        bend_random(&b, 0x1000u + trial * 2654435761u, 1 + (trial % NBITS), 1);
        // The fast path only ever runs for pure bends; bussed ones take the
        // per-pixel path in raw.c, which uses the accessors directly.
        compile10(&b, &cc);
        if (!cc.is_pure) continue;

        srand(trial + 7);
        for (x = 0; x < ROWLEN; x++) buf[x] = rand() & 0xff;
        memcpy(ref, buf, ROWLEN);

        bb_row_fast(buf, ROWPIX);
        for (x = 0; x < ROWPIX; x++)
            set_raw_pixel(ref, x, lut[get_raw_pixel(ref, x) & BB_MAX]);

        if (memcmp(buf, ref, ROWLEN) != 0) return 0;
    }
    return 1;
}

//-------------------------------------------------------------------
// 2. The matrix reproduces the modes it replaced

static int test_preset_swap(void)
{
    int a, c;
    for (a = 0; a < NBITS; a++)
        for (c = 0; c < NBITS; c++)
        {
            bend_t b; bend_cc_t cc; unsigned v;
            mkbend(&b);
            bend_preset_swap(&b, a, c);
            compile10(&b, &cc);
            for (v = 0; v < 1024; v++)
                if (cc.lut[v] != old_swap(v, a, c)) return 0;
        }
    return 1;
}

static int test_preset_rotate(void)
{
    int k;
    for (k = 0; k < NBITS; k++)
    {
        bend_t b; bend_cc_t cc; unsigned v;
        mkbend(&b);
        bend_preset_rotate(&b, k);
        compile10(&b, &cc);
        for (v = 0; v < 1024; v++)
            if (cc.lut[v] != old_rotate(v, k)) return 0;
    }
    return 1;
}

static int test_preset_reverse(void)
{
    bend_t b; bend_cc_t cc; unsigned v;
    mkbend(&b);
    bend_preset_reverse(&b);
    compile10(&b, &cc);
    for (v = 0; v < 1024; v++)
        if (cc.lut[v] != old_reverse(v)) return 0;
    return 1;
}

static int test_preset_xor(void)
{
    unsigned m;
    for (m = 0; m < 1024; m += 7)
    {
        bend_t b; bend_cc_t cc; unsigned v;
        mkbend(&b);
        bend_preset_xor(&b, m);
        compile10(&b, &cc);
        for (v = 0; v < 1024; v++)
            if (cc.lut[v] != old_xor(v, m)) return 0;
    }
    return 1;
}

static int test_preset_tie(void)
{
    int a;
    for (a = 0; a < NBITS; a++)
    {
        bend_t b; bend_cc_t cc; unsigned v;

        mkbend(&b);
        bend_preset_tie(&b, a, 0);
        compile10(&b, &cc);
        for (v = 0; v < 1024; v++)
            if (cc.lut[v] != old_kill(v, a)) return 0;

        mkbend(&b);
        bend_preset_tie(&b, a, 1);
        compile10(&b, &cc);
        for (v = 0; v < 1024; v++)
            if (cc.lut[v] != old_stuck(v, a)) return 0;
    }
    return 1;
}

// Composition is the thing the old model could not do at all: two clip leads
// at once. A swap followed by a tie must give exactly the tie applied to the
// swapped word, not one or the other.
static int test_composition(void)
{
    bend_t b; bend_cc_t cc; unsigned v;
    mkbend(&b);
    bend_preset_swap(&b, 9, 0);
    bend_preset_tie(&b, 4, 0);
    bend_preset_xor(&b, 1u << 7);
    compile10(&b, &cc);

    for (v = 0; v < 1024; v++)
    {
        unsigned want = old_swap(v, 9, 0);
        want = old_kill(want, 4);
        want ^= (1u << 7);
        if (cc.lut[v] != (want & BB_MAX)) return 0;
    }
    return 1;
}

//-------------------------------------------------------------------
// 3. Bit depth remapping
//
// A matrix authored at 10 bits has to behave the same way when it is applied
// to the 8 bit live view. Bits are mapped by distance from the top, so "bit 9"
// stays the most significant bit in both domains.

static int test_remap_8bit(void)
{
    bend_t b;
    bend_cc_t cc8;
    unsigned v;

    // Top bit fed from the bit one below it: at 10 bits that is 9 <- 8, so at
    // 8 bits it must be 7 <- 6.
    mkbend(&b);
    b.route[9] = BSRC_DATA(8);
    bend_compile(&b, &cc8, lut8, 8);
    for (v = 0; v < 256; v++)
    {
        unsigned want = (v & ~0x80u) | (((v >> 6) & 1u) << 7);
        if (cc8.lut[v] != want) return 0;
    }

    // Tying the top line low must tie the top line low in either domain.
    mkbend(&b);
    bend_preset_tie(&b, 9, 0);
    bend_compile(&b, &cc8, lut8, 8);
    for (v = 0; v < 256; v++)
        if (cc8.lut[v] != (v & 0x7fu)) return 0;

    // Inversion of the top line, likewise.
    mkbend(&b);
    b.route[9] = BSRC_NDATA(9);
    bend_compile(&b, &cc8, lut8, 8);
    for (v = 0; v < 256; v++)
        if (cc8.lut[v] != (v ^ 0x80u)) return 0;

    return 1;
}

//-------------------------------------------------------------------
// 4. Non-data buses

static int bus_case(int bus, unsigned v, int x, int y, const bend_src_t *s, unsigned want_bit)
{
    bend_t b;
    bend_cc_t cc;
    unsigned got;

    mkbend(&b);
    b.route[0] = BSRC_BUS(bus);
    b.hdiv = 3;
    b.vdiv = 3;
    compile10(&b, &cc);

    if (cc.is_pure) return 0;                   // a bus must mark the bend bussed
    got = bend_eval(&cc, v, x, y, s);

    // Every other bit must be untouched, and bit 0 must be the bus value.
    if ((got & ~1u) != (v & ~1u)) return 0;
    return (got & 1u) == (want_bit & 1u);
}

static int test_buses(void)
{
    bend_src_t s;
    memset(&s, 0, sizeof(s));
    s.prev  = 0x155;    // bit 0 set
    s.above = 0x2aa;    // bit 0 clear
    s.ob    = 0x001;
    s.tap   = 0x000;
    s.noise = 0x001;
    s.frame = 1;

    if (!bus_case(BUS_HCLK,   0x2aa, 3, 0, &s, 1)) return 0;   // x odd
    if (!bus_case(BUS_HCLK,   0x2aa, 4, 0, &s, 0)) return 0;   // x even
    if (!bus_case(BUS_HCLKN,  0x2aa, 8, 0, &s, 1)) return 0;   // (8>>3)&1
    if (!bus_case(BUS_HCLKN,  0x2aa, 4, 0, &s, 0)) return 0;
    if (!bus_case(BUS_VCLK,   0x2aa, 0, 5, &s, 1)) return 0;
    if (!bus_case(BUS_VCLK,   0x2aa, 0, 6, &s, 0)) return 0;
    if (!bus_case(BUS_VCLKN,  0x2aa, 0, 8, &s, 1)) return 0;
    if (!bus_case(BUS_FRAME,  0x2aa, 0, 0, &s, 1)) return 0;
    if (!bus_case(BUS_DIAG,   0x2aa, 1, 0, &s, 1)) return 0;
    if (!bus_case(BUS_DIAG,   0x2aa, 1, 1, &s, 0)) return 0;
    if (!bus_case(BUS_OB,     0x2aa, 0, 0, &s, 1)) return 0;
    if (!bus_case(BUS_NOISE,  0x2aa, 0, 0, &s, 1)) return 0;
    if (!bus_case(BUS_HOLD,   0x2aa, 0, 0, &s, 1)) return 0;
    if (!bus_case(BUS_VHOLD,  0x2aa, 0, 0, &s, 0)) return 0;
    if (!bus_case(BUS_TAP,    0x2aa, 0, 0, &s, 0)) return 0;

    // Parity of the input word, not of the output. 0x2aa sets bits 1,3,5,7,9,
    // so odd parity; 0x2ab adds bit 0 and makes it even.
    if (!bus_case(BUS_PARITY, 0x2aa, 0, 0, &s, 1)) return 0;
    if (!bus_case(BUS_PARITY, 0x0aa, 0, 0, &s, 0)) return 0;   // bits 1,3,5,7
    return 1;
}

// An inverted bus is the same signal read on the wrong polarity, so it must be
// the exact complement and nothing else.
static int test_bus_inversion(void)
{
    bend_t b, ib;
    bend_cc_t cc, icc;
    static unsigned short ilut[1024];
    bend_src_t s;
    int x, y;

    memset(&s, 0, sizeof(s));
    s.noise = 0x3ff;

    mkbend(&b);  b.route[5] = BSRC_BUS(BUS_DIAG);
    mkbend(&ib); ib.route[5] = BSRC_NBUS(BUS_DIAG);
    bend_compile(&b,  &cc,  lut,  NBITS);
    bend_compile(&ib, &icc, ilut, NBITS);

    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
        {
            unsigned v;
            for (v = 0; v < 1024; v += 13)
                if ((bend_eval(&cc, v, x, y, &s) ^ bend_eval(&icc, v, x, y, &s)) != (1u << 5))
                    return 0;
        }
    return 1;
}

//-------------------------------------------------------------------
// 5. Robustness and the random generator

static int test_sanitize(void)
{
    bend_t b;
    int i;

    // A zeroed struct is what config_load_defaults hands us, and it is not the
    // identity bend - every output bit would come from pin 0.
    memset(&b, 0, sizeof(b));
    bend_sanitize(&b, NBITS);
    if (!bend_is_identity(&b)) return 0;

    // Garbage that would index off the end of the LUT or name a bus that does
    // not exist has to be pulled back into range, not merely tolerated.
    memset(&b, 0xff, sizeof(b));
    b.nbits = NBITS;
    bend_sanitize(&b, NBITS);
    for (i = 0; i < NBITS; i++)
    {
        unsigned char src = b.route[i];
        switch (BSRC_CLASS(src))
        {
        case BSRC_C_DATA:
        case BSRC_C_NDATA:  if (BSRC_IDX(src) >= NBITS) return 0; break;
        case BSRC_C_TIE:    break;
        case BSRC_C_BUS:
        case BSRC_C_NBUS:   if (BSRC_IDX(src) >= BUS_COUNT) return 0; break;
        default:            return 0;
        }
    }
    if (b.bayer > 4 || b.trash_type >= TRASH_COUNT) return 0;
    return 1;
}

// Every bend produced by the generator has to compile without indexing out of
// range, and none of them may come out as the identity - a random press that
// does nothing reads as a broken button.
static int test_random_wellformed(void)
{
    int trial;
    for (trial = 0; trial < 2000; trial++)
    {
        bend_t b;
        bend_cc_t cc;
        int depth = 1 + (trial % NBITS);

        mkbend(&b);
        bend_random(&b, trial * 2654435761u + 1u, depth, 1);
        if (bend_is_identity(&b)) return 0;

        bend_sanitize(&b, NBITS);
        compile10(&b, &cc);
        if (cc.mask != BB_MAX || cc.nbits != NBITS) return 0;
    }
    return 1;
}

// Depth is meant to be "how many clip leads you have". A swap moves two pins,
// everything else moves one, so at most 2*depth routes may differ.
static int test_random_depth(void)
{
    int trial;
    for (trial = 0; trial < 2000; trial++)
    {
        bend_t b;
        int depth = 1 + (trial % NBITS);
        int i, changed = 0;

        mkbend(&b);
        bend_random(&b, trial * 40503u + 3u, depth, 1);
        for (i = 0; i < NBITS; i++)
            if (b.route[i] != (unsigned char)BSRC_DATA(i)) changed++;
        if (changed > 2 * depth) return 0;
    }
    return 1;
}

// A saved bend that came out different every time it was loaded would be
// worthless, so the generators must be a pure function of the seed.
static int test_reproducible(void)
{
    bend_t a, b;
    unsigned sa, sb, i;

    mkbend(&a); mkbend(&b);
    bend_random(&a, 0xc0ffeeu, 6, 1);
    bend_random(&b, 0xc0ffeeu, 6, 1);
    if (memcmp(&a, &b, sizeof(bend_t)) != 0) return 0;

    sa = bend_trash_seed(&a, 42, 0x123);
    sb = bend_trash_seed(&b, 42, 0x123);
    for (i = 0; i < 500; i++)
        if (bend_trash_step(&a, &sa, i) != bend_trash_step(&b, &sb, i)) return 0;

    // and the noise must actually move, or the bus is dead
    sa = bend_trash_seed(&a, 1, 0);
    a.trash_type = TRASH_LFSR;
    {
        unsigned first = bend_trash_step(&a, &sa, 0);
        int moved = 0;
        for (i = 1; i < 64; i++)
            if (bend_trash_step(&a, &sa, i) != first) { moved = 1; break; }
        if (!moved) return 0;
    }
    return 1;
}

//-------------------------------------------------------------------
// 5a. Simple-signals mode
//
// The switch exists for one reason: a bend with no bus in it compiles to a
// pure lookup, and a pure lookup is the difference between a shot that saves
// and a shot you wait out. So the property worth proving is not "buses are
// gone" but "cc.is_pure comes out true" - that is the flag both appliers
// branch on, and it is the one that has to hold for every reachable matrix.

static int test_simplify_is_pure(void)
{
    int trial;

    for (trial = 0; trial < 4000; trial++)
    {
        bend_t b;
        bend_cc_t cc;
        int i;

        // Deliberately bus-heavy input: random bends with buses allowed, plus
        // a hand-patched bus on the top pin so the trials that happen to come
        // out clean still exercise the rewrite.
        mkbend(&b);
        bend_random(&b, trial * 2654435761u + 7u, 1 + (trial % NBITS), 1);
        b.route[NBITS - 1] = (unsigned char)((trial & 1)
                                ? BSRC_BUS(trial % BUS_COUNT)
                                : BSRC_NBUS(trial % BUS_COUNT));
        bend_sanitize(&b, NBITS);

        bend_simplify(&b);

        for (i = 0; i < NBITS; i++)
        {
            int cls = BSRC_CLASS(b.route[i]);
            if (cls == BSRC_C_BUS || cls == BSRC_C_NBUS) return 0;
            // and it must not have landed on a pin that does not exist
            if ((cls == BSRC_C_DATA || cls == BSRC_C_NDATA) &&
                BSRC_IDX(b.route[i]) >= NBITS) return 0;
        }

        compile10(&b, &cc);
        if (!cc.is_pure || cc.dyn_mask != 0 || cc.bus_used != 0) return 0;
    }
    return 1;
}

// Simplifying twice must be the same as simplifying once, or the switch would
// keep changing the bend every time something touched it - and it is touched
// once per shot and once per live-view frame.
static int test_simplify_idempotent(void)
{
    int trial;

    for (trial = 0; trial < 2000; trial++)
    {
        bend_t a, b;

        mkbend(&a);
        bend_random(&a, trial * 40503u + 11u, 1 + (trial % NBITS), 1);
        bend_sanitize(&a, NBITS);

        bend_simplify(&a);
        b = a;
        if (bend_simplify(&b) != 0) return 0;       // nothing left to change
        if (memcmp(&a, &b, sizeof(bend_t)) != 0) return 0;
    }
    return 1;
}

// With buses off the generator must spend the whole budget on edges that
// survive - a reroll that lands on a bus and gets rewritten back to a straight
// line is a wasted lead, and at depth 1 it is the whole press.
static int test_random_no_bus(void)
{
    int trial, identities = 0;

    for (trial = 0; trial < 4000; trial++)
    {
        bend_t b;
        bend_cc_t cc;
        int i;

        mkbend(&b);
        bend_random(&b, trial * 2654435761u + 13u, 1 + (trial % NBITS), 0);

        for (i = 0; i < NBITS; i++)
        {
            int cls = BSRC_CLASS(b.route[i]);
            if (cls == BSRC_C_BUS || cls == BSRC_C_NBUS) return 0;
        }
        if (bend_is_identity(&b)) identities++;

        bend_sanitize(&b, NBITS);
        compile10(&b, &cc);
        if (!cc.is_pure) return 0;
    }
    return identities == 0;
}

static int test_mutate_no_bus(void)
{
    int trial;

    for (trial = 0; trial < 4000; trial++)
    {
        bend_t b;
        int i;

        mkbend(&b);
        bend_random(&b, trial * 40503u + 17u, 4, 0);
        bend_mutate(&b, trial * 2654435761u + 19u, 0);

        for (i = 0; i < NBITS; i++)
        {
            int cls = BSRC_CLASS(b.route[i]);
            if (cls == BSRC_C_BUS || cls == BSRC_C_NBUS) return 0;
        }
    }
    return 1;
}

//-------------------------------------------------------------------
// 6. The a480's 12 bit packing
//
// Same claim as the 10bpp path, checked the same way. This one matters more:
// the a480 is 3720x2772, so the fast path runs 10.3 million times a shot and
// the per-pixel accessors are not a usable fallback.

static int test12_roundtrip_identity(void)
{
    unsigned int x;
    int i;
    for (i = 0; i < 4096; i++) lut12[i] = i;

    srand(11);
    for (i = 0; i < ROWLEN12; i++) buf12[i] = rand() & 0xff;
    memcpy(ref12, buf12, ROWLEN12);

    bb_row_fast12(buf12, ROWPIX12);
    if (memcmp(buf12, ref12, ROWLEN12) != 0) return 0;

    for (x = 0; x < ROWPIX12; x++)
        if (get_raw_pixel12(buf12, x) != get_raw_pixel12(ref12, x)) return 0;
    return 1;
}

static int test12_fastpath_matches_accessors(void)
{
    int trial;

    for (trial = 0; trial < 200; trial++)
    {
        bend_t b;
        bend_cc_t cc;
        unsigned int x;

        bend_reset(&b, 12);
        bend_random(&b, 0x2000u + trial * 2654435761u, 1 + (trial % 12), 1);
        bend_compile(&b, &cc, lut12, 12);
        if (!cc.is_pure) continue;

        srand(trial + 101);
        for (x = 0; x < ROWLEN12; x++) buf12[x] = rand() & 0xff;
        memcpy(ref12, buf12, ROWLEN12);

        bb_row_fast12(buf12, ROWPIX12);
        for (x = 0; x < ROWPIX12; x++)
            set_raw_pixel12(ref12, x, lut12[get_raw_pixel12(ref12, x) & BB_MAX12]);

        if (memcmp(buf12, ref12, ROWLEN12) != 0) return 0;
    }
    return 1;
}

// A matrix authored on the a470 and carried to the a480 must keep meaning the
// same thing: bit 9 is the top bit at 10 bits, bit 11 is the top bit at 12.
static int test12_remap_from_10bit(void)
{
    bend_t b;
    bend_cc_t cc;
    unsigned v;

    bend_reset(&b, NBITS);
    bend_preset_tie(&b, 9, 0);          // tie the top line low at 10 bits
    bend_compile(&b, &cc, lut12, 12);
    for (v = 0; v < 4096; v++)
        if (cc.lut[v] != (v & 0x7ffu)) return 0;

    bend_reset(&b, NBITS);
    b.route[9] = BSRC_NDATA(9);         // invert the top line
    bend_compile(&b, &cc, lut12, 12);
    for (v = 0; v < 4096; v++)
        if (cc.lut[v] != (v ^ 0x800u)) return 0;

    return 1;
}

//-------------------------------------------------------------------

//-------------------------------------------------------------------
// Segments.
//
// The applier trusts the spans completely: it walks them, and for each one it
// belongs to it writes every pixel from where the last span ended to where
// this one does. So the only thing that keeps a segmented bend inside the
// frame buffer is that the spans of a row tile [0, w) exactly - no gap, no
// overlap, nothing past the end - and that is what these prove, over every
// layout, every size and a frame geometry from each of the two bodies.

#define SEG_W   3720                    // a480
#define SEG_H   2772
#define SEG_W2  3152                    // a470
#define SEG_H2  2346

static int seg_tiles_row(int layout, int size, int y, int w, int h)
{
    unsigned char  seg[BEND_SEG_SPANS];
    unsigned short xend[BEND_SEG_SPANS];
    int n = bend_seg_spans(layout, size, y, w, h, seg, xend);
    int i, last = 0;

    if (n < 1 || n > BEND_SEG_SPANS) return 0;
    for (i = 0; i < n; i++)
    {
        if ((int)xend[i] <= last) return 0;              // empty or backwards
        if ((int)xend[i] > w)     return 0;              // past the end
        if (seg[i] >= bend_seg_count(layout)) return 0;  // region that has no
        last = xend[i];                                  // matrix behind it
    }
    return last == w;                                    // covers the whole row
}

static int test_seg_spans_tile(void)
{
    int lay, size, y;

    for (lay = 0; lay < BSEG_COUNT; lay++)
        for (size = 1; size <= 100; size++)
        {
            // Every row of the smaller frame, and the same sweep on the larger
            // one - the circles are the only thing here that is not row
            // invariant, and every row of them is a different chord.
            for (y = 0; y < SEG_H2; y++)
                if (!seg_tiles_row(lay, size, y, SEG_W2, SEG_H2)) return 0;
            for (y = 0; y < SEG_H; y++)
                if (!seg_tiles_row(lay, size, y, SEG_W, SEG_H)) return 0;
        }
    return 1;
}

// Rows outside the frame and degenerate geometry. The applier never asks for
// these, but the sanitizer is the only thing standing between a corrupt config
// block and this function, so it must not be the thing that has to be right.
static int test_seg_spans_edges(void)
{
    unsigned char  seg[BEND_SEG_SPANS];
    unsigned short xend[BEND_SEG_SPANS];

    if (!seg_tiles_row(BSEG_CIRCLE, 100, -5, SEG_W, SEG_H)) return 0;
    if (!seg_tiles_row(BSEG_CIRCLE, 100, SEG_H + 5, SEG_W, SEG_H)) return 0;
    if (!seg_tiles_row(BSEG_RINGS,  100, SEG_H / 2, SEG_W, SEG_H)) return 0;
    if (!seg_tiles_row(BSEG_QUAD,   50,  0, 1, 1)) return 0;

    // Out of range layout and size are the two fields that index something,
    // and both have to land on the layout that cannot go wrong.
    if (bend_seg_spans(99, 0, 0, SEG_W, SEG_H, seg, xend) != 1) return 0;
    if (xend[0] != SEG_W || seg[0] != 0) return 0;

    // A zero width row cannot be tiled and must still return a usable span.
    if (bend_seg_spans(BSEG_QUAD, 50, 0, 0, SEG_H, seg, xend) != 1) return 0;
    return 1;
}

// bend_seg_at() is the readable statement of the geometry and the spans are
// the fast one. They are two descriptions of the same thing, so they have to
// agree everywhere, and a disagreement means the applier is bending pixels the
// UI thinks belong to another region.
static int test_seg_at_matches_spans(void)
{
    int lay, y, x;

    for (lay = 0; lay < BSEG_COUNT; lay++)
        for (y = 0; y < SEG_H; y += 37)
        {
            unsigned char  seg[BEND_SEG_SPANS];
            unsigned short xend[BEND_SEG_SPANS];
            int n = bend_seg_spans(lay, 60, y, SEG_W, SEG_H, seg, xend);
            int i = 0;

            for (x = 0; x < SEG_W; x++)
            {
                while (i < n - 1 && x >= (int)xend[i]) i++;
                if (bend_seg_at(lay, 60, x, y, SEG_W, SEG_H) != seg[i]) return 0;
            }
        }
    return 1;
}

// The shapes are meant to be shapes. A layout whose regions all landed on the
// same pixels would tile perfectly and be useless, so check the middle of the
// frame is in a different region from the corner for each of them, and that
// the circle grows with its knob rather than shrinking or standing still.
static int test_seg_shapes(void)
{
    int size, last = -1;

    if (bend_seg_at(BSEG_HALF_H, 50, SEG_W/2, 10, SEG_W, SEG_H) ==
        bend_seg_at(BSEG_HALF_H, 50, SEG_W/2, SEG_H-10, SEG_W, SEG_H)) return 0;
    if (bend_seg_at(BSEG_HALF_V, 50, 10, SEG_H/2, SEG_W, SEG_H) ==
        bend_seg_at(BSEG_HALF_V, 50, SEG_W-10, SEG_H/2, SEG_W, SEG_H)) return 0;
    if (bend_seg_at(BSEG_QUAD, 50, 10, 10, SEG_W, SEG_H) != 0) return 0;
    if (bend_seg_at(BSEG_QUAD, 50, SEG_W-10, SEG_H-10, SEG_W, SEG_H) != 3) return 0;
    if (bend_seg_at(BSEG_CIRCLE, 50, SEG_W/2, SEG_H/2, SEG_W, SEG_H) != 0) return 0;
    if (bend_seg_at(BSEG_CIRCLE, 50, 0, 0, SEG_W, SEG_H) != 1) return 0;
    if (bend_seg_at(BSEG_RINGS, 80, SEG_W/2, SEG_H/2, SEG_W, SEG_H) != 0) return 0;
    if (bend_seg_at(BSEG_RINGS, 80, 0, 0, SEG_W, SEG_H) != 2) return 0;

    // The diagonal splits the two corners it is named for, and the wedges put
    // the four edge midpoints in four different regions - which is the thing
    // that makes them wedges and not quarters.
    if (bend_seg_at(BSEG_DIAG, 50, SEG_W-10, 10, SEG_W, SEG_H) != 0) return 0;
    if (bend_seg_at(BSEG_DIAG, 50, 10, SEG_H-10, SEG_W, SEG_H) != 1) return 0;
    if (bend_seg_at(BSEG_WEDGE, 50, SEG_W/2, 10,        SEG_W, SEG_H) != 0) return 0;
    if (bend_seg_at(BSEG_WEDGE, 50, SEG_W-10, SEG_H/2,  SEG_W, SEG_H) != 1) return 0;
    if (bend_seg_at(BSEG_WEDGE, 50, SEG_W/2, SEG_H-10,  SEG_W, SEG_H) != 2) return 0;
    if (bend_seg_at(BSEG_WEDGE, 50, 10, SEG_H/2,        SEG_W, SEG_H) != 3) return 0;

    // The repeating layouts have to repeat: every region appears, and the ones
    // that cycle along a row put more than one region in that row. A cell held
    // so large that the pattern degenerated into one block would tile
    // perfectly and be indistinguishable from Off.
    {
        int lay, seen, k;
        static const int rep[3] = { BSEG_BANDS, BSEG_BARS, BSEG_CHECKER };

        for (k = 0; k < 3; k++)
        {
            lay  = rep[k];
            seen = 0;
            for (size = 5; size <= 100; size += 5)
            {
                int y;
                for (y = 0; y < SEG_H; y += 7)
                {
                    int x;
                    for (x = 0; x < SEG_W; x += 7)
                        seen |= 1 << bend_seg_at(lay, size, x, y, SEG_W, SEG_H);
                }
            }
            if (seen != (1 << BEND_SEG_MAX) - 1) return 0;
        }

        // The pattern layouts are two-region, and both regions have to be worth
        // having. A pattern that came out all one value would still tile, still
        // re-roll and still look like a working layout from every other test
        // here - it would just quietly apply one bend to the whole frame.
        //
        // The bar is deliberately higher than "both appear": a pattern where
        // one region is a handful of pixels is the same failure, slower. Each
        // has to put at least a tenth of the frame in its minority region.
        for (lay = BSEG_WARP; lay < BSEG_COUNT; lay++)
        {
            if (bend_seg_count(lay) != 2) return 0;

            for (size = 5; size <= 100; size += 5)
            {
                int y, n0 = 0, n1 = 0;
                for (y = 0; y < SEG_H; y += 7)
                {
                    int x;
                    for (x = 0; x < SEG_W; x += 7)
                    {
                        if (bend_seg_at(lay, size, x, y, SEG_W, SEG_H)) n1++;
                        else                                            n0++;
                    }
                }
                if (n0 + n1 == 0) return 0;
                if (n0 * 10 < (n0 + n1)) return 0;
                if (n1 * 10 < (n0 + n1)) return 0;
            }
        }

        // Bands is the one that must not cut a row, because the applier keeps
        // its whole-row fast path on the strength of exactly that.
        for (size = 5; size <= 100; size += 5)
        {
            int y;
            for (y = 0; y < SEG_H; y += 3)
            {
                unsigned char  seg[BEND_SEG_SPANS];
                unsigned short xend[BEND_SEG_SPANS];
                if (bend_seg_spans(BSEG_BANDS, size, y, SEG_W, SEG_H, seg, xend) != 1)
                    return 0;
            }
        }
        if (bend_seg_cuts_rows(BSEG_BANDS)) return 0;
        if (!bend_seg_cuts_rows(BSEG_BARS)) return 0;
    }

    // Monotonic in the knob, measured along the centre row.
    for (size = 5; size <= 100; size += 5)
    {
        unsigned char  seg[BEND_SEG_SPANS];
        unsigned short xend[BEND_SEG_SPANS];
        int n = bend_seg_spans(BSEG_CIRCLE, size, SEG_H/2, SEG_W, SEG_H, seg, xend);
        int wide = 0, i, prev = 0;

        for (i = 0; i < n; i++)
        {
            if (seg[i] == 0) wide = (int)xend[i] - prev;
            prev = xend[i];
        }
        if (wide <= last) return 0;
        last = wide;
    }
    return 1;
}

// The gate. A zeroed struct is what a config block from a build without
// segments looks like, and it has to come out as the camera that was there
// before - off, with a radius that is worth looking at rather than zero.
static int test_seg_sanitize(void)
{
    bend_segs_t s;
    int i;

    memset(&s, 0, sizeof(s));
    bend_seg_sanitize(&s, NBITS);
    if (s.layout != BSEG_OFF || s.active != 0) return 0;
    if (s.size < 1 || s.size > 100) return 0;
    for (i = 0; i < BEND_SEG_MAX - 1; i++)
        if (!bend_is_identity(&s.b[i])) return 0;

    // Slots are stored plus one, so zero fill has to mean "no preset here" -
    // otherwise every region of an upgraded camera would claim BEND00.
    for (i = 0; i < BEND_SEG_MAX; i++) if (s.slot[i] != 0) return 0;

    memset(&s, 0xff, sizeof(s));
    bend_seg_sanitize(&s, NBITS);
    if (s.layout >= BSEG_COUNT) return 0;
    if (s.active >= bend_seg_count(s.layout)) return 0;
    if (s.size < 1 || s.size > 100) return 0;
    for (i = 0; i < BEND_SEG_MAX; i++) if (s.slot[i] > 100) return 0;

    // Every layout has as many labels as it has regions, and none of them are
    // the empty string - the pickers print these unconditionally.
    for (i = 0; i < BSEG_COUNT; i++)
    {
        int j, n = bend_seg_count(i);
        if (n < 1 || n > BEND_SEG_MAX) return 0;
        if (!bend_seg_name(i) || !bend_seg_name(i)[0]) return 0;
        for (j = 0; j < n; j++)
            if (!bend_seg_label(i, j) || !bend_seg_label(i, j)[0]) return 0;
    }
    return 1;
}

// The re-roll has to actually re-roll, and has to leave the geometry alone.
//
// Both halves matter. A seed that did nothing would leave every shot identical,
// which is the thing being fixed; a seed that reached the geometric layouts
// would move the centre circle, which would read as a bug rather than a
// feature. And the default has to stay fixed, because every assertion in
// test_seg_shapes() above is written against it.
static int test_seg_reroll(void)
{
    // A 32x32 grid over the frame - 1024 samples, sized from the step rather
    // than guessed, because guessing it is how the first version of this test
    // ran off the end of its buffer.
    #define RR_N    32
    #define RR_DX   (SEG_W / RR_N)
    #define RR_DY   (SEG_H / RR_N)

    int lay, x, y, size = 60;
    int moved, same;

    // Sampled on a grid rather than at one point - two seeds can of course
    // agree about any single pixel.
    #define SAMPLE(dst)                                                     \
        do {                                                                \
            int i = 0;                                                      \
            for (y = 0; y + RR_DY <= SEG_H; y += RR_DY)                     \
                for (x = 0; x + RR_DX <= SEG_W; x += RR_DX)                 \
                    (dst)[i++] = (char)bend_seg_at(lay, size, x, y,         \
                                                   SEG_W, SEG_H);           \
        } while (0)

    for (lay = 0; lay < BSEG_COUNT; lay++)
    {
        char a[RR_N * RR_N], b[RR_N * RR_N], c[RR_N * RR_N];

        bend_seg_set_seed(0x11111111u); SAMPLE(a);
        bend_seg_set_seed(0x22222222u); SAMPLE(b);
        bend_seg_set_seed(0x11111111u); SAMPLE(c);

        moved = memcmp(a, b, sizeof a) != 0;
        same  = memcmp(a, c, sizeof a) == 0;

        // Same seed, same picture - otherwise the pattern would crawl between
        // the four segment passes of a single shot and none of them would line
        // up with the others.
        if (!same) return 0;

        if (BSEG_IS_PATTERN(lay)) { if (!moved) return 0; }
        else                      { if (moved)  return 0; }
    }
    #undef SAMPLE
    #undef RR_DY
    #undef RR_DX
    #undef RR_N

    // Put the default back, or every test that runs after this one is looking
    // at a different pattern from the one it was written against.
    bend_seg_set_seed(0);
    return 1;
}

int main(void)
{
    printf("bend engine self-test\n\n");

    check("packing: identity table round trips",         test_roundtrip_identity());
    check("packing: fast path == reference accessors",   test_fastpath_matches_accessors());

    check("matrix reproduces old mode: swap",            test_preset_swap());
    check("matrix reproduces old mode: rotate",          test_preset_rotate());
    check("matrix reproduces old mode: reverse",         test_preset_reverse());
    check("matrix reproduces old mode: xor",             test_preset_xor());
    check("matrix reproduces old mode: tie low/high",    test_preset_tie());
    check("matrix composes swap + tie + invert",         test_composition());

    check("bit depth remap 10 -> 8 preserves top bits",  test_remap_8bit());

    check("non-data buses evaluate as documented",       test_buses());
    check("inverted bus is the exact complement",        test_bus_inversion());

    check("sanitize repairs zeroed and garbage bends",   test_sanitize());
    check("random bends are well formed and non-trivial",test_random_wellformed());
    check("random respects the depth bound",             test_random_depth());
    check("generators are reproducible from the seed",   test_reproducible());

    check("simplify leaves a bend that compiles pure",   test_simplify_is_pure());
    check("simplify is idempotent",                      test_simplify_idempotent());
    check("random without buses stays pure and useful",  test_random_no_bus());
    check("mutate without buses cannot patch one in",    test_mutate_no_bus());

    check("12bpp: identity table round trips",           test12_roundtrip_identity());
    check("12bpp: fast path == reference accessors",     test12_fastpath_matches_accessors());
    check("12bpp: a470 matrix means the same on a480",   test12_remap_from_10bit());

    check("segments: spans tile every row exactly",      test_seg_spans_tile());
    check("segments: degenerate geometry stays in range",test_seg_spans_edges());
    check("segments: bend_seg_at agrees with the spans", test_seg_at_matches_spans());
    check("segments: the layouts are the shapes claimed",test_seg_shapes());
    check("segments: sanitize repairs a zeroed struct",  test_seg_sanitize());
    check("segments: patterns re-roll, shapes do not",   test_seg_reroll());

    printf("\n%s\n", fail ? "FAILED" : "all passed");
    return fail;
}
