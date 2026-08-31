// Host-side check that the bit bending fast path in core/raw.c packs and
// unpacks 10bpp raw exactly the way CHDK's own get_raw_pixel/set_raw_pixel do.
//
// The fast path exists because the reference accessors recompute an address and
// run an 8-way switch for every single pixel, which is a lot to pay 7.4 million
// times on a 2008 camera. It is only worth having if it is bit-for-bit identical
// to the thing it replaces, so this checks that directly rather than by eye.
//
//   gcc -O2 -Wall -o bitbend_selftest tools/bitbend_selftest.c && ./bitbend_selftest
//
// Exits non-zero on any mismatch.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWPIX  3152                    // CAM_RAW_ROWPIX for the a470
#define ROWLEN  ((ROWPIX * 10) / 8)
#define BB_MAX  1023

static unsigned char buf[ROWLEN];
static unsigned char ref[ROWLEN];
static unsigned short lut[1024];

// ---- verbatim from core/raw.c (CAM_SENSOR_BITS_PER_PIXEL == 10) ----

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

// ---- verbatim from core/raw.c ----

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

// ---- the checks ----

static int fail;

static void check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fail = 1;
}

// Every pixel value must survive an unpack/repack round trip untouched.
static int test_roundtrip_identity(void)
{
    unsigned int x;
    int i;
    for (i = 0; i < 1024; i++) lut[i] = i;          // identity table

    srand(1);
    for (i = 0; i < ROWLEN; i++) buf[i] = rand() & 0xff;
    memcpy(ref, buf, ROWLEN);

    bb_row_fast(buf, ROWPIX);

    if (memcmp(buf, ref, ROWLEN) != 0) return 0;

    // and the accessors agree about what is in there
    for (x = 0; x < ROWPIX; x++)
        if (get_raw_pixel(buf, x) != get_raw_pixel(ref, x)) return 0;
    return 1;
}

// The fast path must produce byte-identical output to the reference accessors
// for an arbitrary table. Run it for a table that maps every value to something
// different, so a dropped or duplicated pixel cannot pass by luck.
static int test_matches_reference(unsigned short (*fn)(unsigned short))
{
    unsigned int x;
    int i;
    for (i = 0; i < 1024; i++) lut[i] = fn(i) & BB_MAX;

    srand(7);
    for (i = 0; i < ROWLEN; i++) buf[i] = rand() & 0xff;
    memcpy(ref, buf, ROWLEN);

    bb_row_fast(buf, ROWPIX);
    for (x = 0; x < ROWPIX; x++)
        set_raw_pixel(ref, x, lut[get_raw_pixel(ref, x) & BB_MAX]);

    return memcmp(buf, ref, ROWLEN) == 0;
}

static unsigned short f_swap_9_0(unsigned short v)
{
    unsigned a = (v >> 9) & 1, b = v & 1;
    return (v & ~((1u << 9) | 1u)) | (a << 0) | (b << 9);
}
static unsigned short f_reverse(unsigned short v)
{
    unsigned short o = 0; int i;
    for (i = 0; i < 10; i++) if (v & (1 << i)) o |= 1 << (9 - i);
    return o;
}
static unsigned short f_xor(unsigned short v)      { return v ^ 0x2aa; }
static unsigned short f_rot3(unsigned short v)     { return ((v << 3) | (v >> 7)) & BB_MAX; }
static unsigned short f_kill7(unsigned short v)    { return v & ~(1 << 7); }
static unsigned short f_stuck2(unsigned short v)   { return v | (1 << 2); }

// Each of the 8 phases in a group must be hit exactly once - catches a swapped
// or omitted pixel slot in the unpack, which a uniform table would hide.
static int test_all_phases_distinct(void)
{
    unsigned int x;
    int i;
    for (i = 0; i < 1024; i++) lut[i] = 0;
    memset(buf, 0, ROWLEN);

    // write a known ramp through the reference accessors, read back via fast
    // path with an identity table, and confirm every position survives
    for (i = 0; i < 1024; i++) lut[i] = i;
    for (x = 0; x < ROWPIX; x++) set_raw_pixel(buf, x, (x * 7 + 13) & BB_MAX);
    memcpy(ref, buf, ROWLEN);
    bb_row_fast(buf, ROWPIX);
    if (memcmp(buf, ref, ROWLEN) != 0) return 0;

    for (x = 0; x < ROWPIX; x++)
        if (get_raw_pixel(buf, x) != (unsigned short)((x * 7 + 13) & BB_MAX)) return 0;
    return 1;
}

int main(void)
{
    printf("bit bend fast path self test (10bpp, rowpix=%d, rowlen=%d)\n\n",
           ROWPIX, ROWLEN);

    check("identity table leaves the row untouched",   test_roundtrip_identity());
    check("all 8 group phases addressed correctly",    test_all_phases_distinct());
    check("swap bits 9<->0 matches reference",         test_matches_reference(f_swap_9_0));
    check("reverse bit order matches reference",       test_matches_reference(f_reverse));
    check("xor 0x2aa matches reference",               test_matches_reference(f_xor));
    check("rotate left 3 matches reference",           test_matches_reference(f_rot3));
    check("kill bit 7 matches reference",              test_matches_reference(f_kill7));
    check("stuck bit 2 matches reference",             test_matches_reference(f_stuck2));

    printf("\n%s\n", fail ? "FAILED" : "all passed");
    return fail;
}
