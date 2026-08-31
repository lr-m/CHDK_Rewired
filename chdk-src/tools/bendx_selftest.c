// Host-side checks for the experimental profile engine (core/bendx.c).
//
// bend_selftest.c exists because getting the packed fast path wrong is a
// reflash to find out. This one exists for a harder reason: every effect in
// bendx.c addresses the frame buffer itself - it picks rows, it picks byte
// offsets, it wraps, it reads a second buffer that lives somewhere else
// entirely - and an off-by-one there is not a wrong picture, it is a write
// into whatever the imaging pipeline had parked next to fifteen megabytes of
// sensor data. So the whole buffer is fenced, every knob of every effect is
// swept, and the fences are checked after each one.
//
// Also checked: bendx_get/bendx_set against the real accessors in core/raw.c
// for both depths and every phase, and the handful of effects whose result is
// exactly predictable, because "it stayed in bounds" is not the same claim as
// "it did the thing".
//
//   gcc -O2 -Wall -Wextra -I../include -o bendx_selftest bendx_selftest.c ../core/bendx.c
//   ./bendx_selftest
//
// Exits non-zero on any mismatch.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bendx.h"

static int failures;

static void fail(const char *what)
{
    printf("FAIL: %s\n", what);
    failures++;
}

//-------------------------------------------------------------------
// verbatim from core/raw.c, made row-relative

static unsigned short raw_get10(const unsigned char *p, unsigned x)
{
    const unsigned char *addr = p + (x / 8) * 10;
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

static void raw_set10(unsigned char *p, unsigned x, unsigned short value)
{
    unsigned char *addr = p + (x / 8) * 10;
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

static unsigned short raw_get12(const unsigned char *p, unsigned x)
{
    const unsigned char *addr = p + (x / 4) * 6;
    switch (x % 4) {
        case 0: return ((unsigned short)(addr[1])        << 4) | (addr[0] >> 4);
        case 1: return ((unsigned short)(addr[0] & 0x0F) << 8) | (addr[3]);
        case 2: return ((unsigned short)(addr[2])        << 4) | (addr[5] >> 4);
        case 3: return ((unsigned short)(addr[5] & 0x0F) << 8) | (addr[4]);
    }
    return 0;
}

static void raw_set12(unsigned char *p, unsigned x, unsigned short value)
{
    unsigned char *addr = p + (x / 4) * 6;
    switch (x % 4) {
        case 0: addr[0] = (addr[0]&0x0F) | (unsigned char)(value << 4);  addr[1] = (unsigned char)(value >> 4);  break;
        case 1: addr[0] = (addr[0]&0xF0) | (unsigned char)(value >> 8);  addr[3] = (unsigned char)value;         break;
        case 2: addr[2] = (unsigned char)(value >> 4);  addr[5] = (addr[5]&0x0F) | (unsigned char)(value << 4);  break;
        case 3: addr[4] = (unsigned char)value; addr[5] = (addr[5]&0xF0) | (unsigned char)(value >> 8);  break;
    }
}

//-------------------------------------------------------------------

static void test_accessors(void)
{
    unsigned char a[64], b[64];
    unsigned x, v;

    // Every phase, both depths, against the real thing. Written into a buffer
    // that starts as noise, so a case that forgets to clear its field is
    // caught rather than hidden by zeroes.
    for (x = 0; x < 16; x++)
    {
        for (v = 0; v < 1024; v += 7)
        {
            memset(a, 0xa5, sizeof a);
            memset(b, 0xa5, sizeof b);
            raw_set10(a, x, (unsigned short)v);
            bendx_set(b, x, v, 10);
            if (memcmp(a, b, sizeof a)) { fail("bendx_set 10bpp differs from raw.c"); return; }
            if (bendx_get(b, x, 10) != raw_get10(a, x)) { fail("bendx_get 10bpp differs from raw.c"); return; }
            if (bendx_get(b, x, 10) != v) { fail("10bpp round trip"); return; }
        }
        for (v = 0; v < 4096; v += 13)
        {
            memset(a, 0x5a, sizeof a);
            memset(b, 0x5a, sizeof b);
            raw_set12(a, x, (unsigned short)v);
            bendx_set(b, x, v, 12);
            if (memcmp(a, b, sizeof a)) { fail("bendx_set 12bpp differs from raw.c"); return; }
            if (bendx_get(b, x, 12) != raw_get12(a, x)) { fail("bendx_get 12bpp differs from raw.c"); return; }
            if (bendx_get(b, x, 12) != v) { fail("12bpp round trip"); return; }
        }
    }
    printf("  accessors: ok\n");
}

//-------------------------------------------------------------------
// The fenced buffer. Effects get base; everything outside it is checked byte
// for byte after every call.

#define FENCE   4096
#define FENCEB  0xDB

typedef struct
{
    unsigned char *mem;         // FENCE + size + FENCE
    unsigned char *base;
    unsigned size;
} fenced_t;

static void fence_alloc(fenced_t *f, unsigned size)
{
    f->size = size;
    f->mem  = malloc(FENCE + size + FENCE);
    memset(f->mem, FENCEB, FENCE + size + FENCE);
    f->base = f->mem + FENCE;
}

static void fence_fill(fenced_t *f, unsigned seed)
{
    unsigned i, s = seed | 1;
    for (i = 0; i < f->size; i++)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        f->base[i] = (unsigned char)s;
    }
}

static int fence_check(fenced_t *f, const char *what)
{
    unsigned i;
    for (i = 0; i < FENCE; i++)
    {
        if (f->mem[i] != FENCEB)                    { fail(what); printf("    underrun at -%u\n", FENCE - i); return 0; }
        if (f->mem[FENCE + f->size + i] != FENCEB)  { fail(what); printf("    overrun at +%u\n", i); return 0; }
    }
    return 1;
}

static void fence_free(fenced_t *f) { free(f->mem); }

//-------------------------------------------------------------------
// One sweep: every effect, every amount, a spread of every other knob, at both
// depths, checking every buffer fence each time.

static void sweep(int nbits, unsigned rowpix, unsigned rows)
{
    unsigned rowlen = (rowpix * (unsigned)nbits) / 8u;
    fenced_t buf, scratch, rom, jpg;
    bendx_buf_t bb;
    bendx_env_t env;
    int kind, checked = 0;

    fence_alloc(&buf,     rowlen * rows);
    fence_alloc(&rom,     64 * 1024);
    fence_alloc(&jpg,     48 * 1024);
    fence_fill(&rom, 0x1234);
    fence_fill(&jpg, 0x4567);

    memset(&bb, 0, sizeof bb);
    bb.base   = buf.base;
    bb.rowlen = rowlen;
    bb.rowpix = rowpix;
    bb.rows   = rows;
    bb.ob_x   = 2;
    bb.nbits  = nbits;

    memset(&env, 0, sizeof env);
    env.rom = rom.base;         env.rom_len    = rom.size;
    env.jpg = jpg.base;         env.jpg_len    = jpg.size;
    env.frame = 7;
    env.black = (1u << (nbits - 5)) - 1u;
    env.white = (1u << nbits) - 1u;

    for (kind = 0; kind < BX_COUNT; kind++)
    {
        int amax = bendx_amount_max(kind);
        int rmax = bendx_reach_max(kind);
        int a, r, n = 0;

        // Fenced to exactly what this effect says it needs, not to the largest
        // any effect needs. Sizing it generously would let an effect overrun
        // its own declaration and still land inside the allocation, which is
        // the bug this test exists to catch - raw.c allocates per chain from
        // the same function, so an under-declaration is a heap overrun there.
        fence_alloc(&scratch, bendx_scratch_bytes(kind, rowlen));

        // Every amount against every reach, because those two are what reach
        // the index arithmetic and they are the pair an off-by-one hides in.
        // The other three knobs only ever mask or gate the result, so they are
        // cycled rather than crossed - the full product is thirty-six times
        // the work for a case distinction the code does not make.
        for (a = 0; a <= amax; a++)
        for (r = 0; r <= rmax; r++, n++)
        {
            static const unsigned char lane_set[4]   = { 0, 0x0f, 0x81, 0xff };
            static const unsigned char rowmod_set[3] = { 0, 1, 7 };
            bendx_t x;

            bendx_reset(&x);
            x.kind   = (unsigned char)kind;
            x.amount = (unsigned char)a;
            x.reach  = (unsigned char)r;
            x.mix    = (unsigned char)(n % BXMIX_COUNT);
            x.lanes  = lane_set[n % 4];
            x.rowmod = rowmod_set[n % 3];
            x.seed   = 0x9e3779b9u ^ (unsigned)(kind * 7 + a * 13 + r);

            bendx_sanitize(&x);
            if (x.kind != kind || x.amount != a || x.reach != r)
                { fail("bendx_sanitize rejected an in-range profile"); return; }

            fence_fill(&buf, 0xbeef ^ (unsigned)(kind * 31 + a));
            memset(scratch.base, 0, scratch.size);

            bendx_apply(&x, &bb, &env, scratch.base);
            checked++;

            if (!fence_check(&buf, "effect wrote outside the frame buffer"))
                { printf("    kind %d (%s) amount %d reach %d\n",
                         kind, bendx_name(kind), a, r); return; }
            if (!fence_check(&scratch, "effect wrote outside the scratch rows"))
                { printf("    kind %d (%s) amount %d reach %d\n", kind, bendx_name(kind), a, r); return; }
            if (!fence_check(&rom, "effect wrote into the ROM window"))    return;
            if (!fence_check(&jpg, "effect wrote into the JPEG buffer"))   return;
        }
        fence_free(&scratch);
    }

    printf("  %dbpp %ux%u: %d profiles, fences intact\n", nbits, rowpix, rows, checked);

    fence_free(&buf);
    fence_free(&rom); fence_free(&jpg);
}

//-------------------------------------------------------------------
// A profile that is off must not touch a byte, and neither must one whose
// foreign buffer the port does not supply. Both are relied on: the first is
// how the picker means "Off", and the second is how a camera with no reversed
// viewport address gets a list with three dead entries rather than a crash.

static void test_inert(void)
{
    unsigned rowpix = 400, rows = 16, rowlen = (rowpix * 12) / 8;
    fenced_t buf, scratch;
    unsigned char *ref = malloc(rowlen * rows);
    bendx_buf_t bb;
    bendx_env_t env;
    bendx_t x;
    int kind;

    fence_alloc(&buf, rowlen * rows);
    fence_alloc(&scratch, bendx_scratch_bytes(BX_SORT, rowlen));

    memset(&bb, 0, sizeof bb);
    bb.base = buf.base; bb.rowlen = rowlen; bb.rowpix = rowpix;
    bb.rows = rows; bb.ob_x = 2; bb.nbits = 12;
    memset(&env, 0, sizeof env);        // no foreign buffers at all
    env.white = 4095;

    bendx_reset(&x);
    fence_fill(&buf, 0x77);
    memcpy(ref, buf.base, rowlen * rows);
    bendx_apply(&x, &bb, &env, scratch.base);
    if (memcmp(ref, buf.base, rowlen * rows)) fail("BX_OFF changed the buffer");

    for (kind = 0; kind < BX_COUNT; kind++)
    {
        if (!bendx_needs_env(kind)) continue;
        bendx_reset(&x);
        x.kind = (unsigned char)kind;
        fence_fill(&buf, 0x88);
        memcpy(ref, buf.base, rowlen * rows);
        bendx_apply(&x, &bb, &env, scratch.base);
        if (memcmp(ref, buf.base, rowlen * rows))
        { fail("a foreign bus ran with no buffer supplied"); printf("    %s\n", bendx_name(kind)); }
    }

    printf("  inert cases: ok\n");
    free(ref); fence_free(&buf); fence_free(&scratch);
}

//-------------------------------------------------------------------
// The effects whose result is exactly predictable. "It stayed in bounds" is a
// weaker claim than anyone wants to rest a shot on.

static void test_semantics(void)
{
    unsigned rowpix = 64, rows = 8, rowlen = (rowpix * 12) / 8;   // 96 bytes
    unsigned char *mem = malloc(rowlen * rows);
    unsigned char *ref = malloc(rowlen * rows);
    unsigned char *scratch = malloc(rowlen * BENDX_SCRATCH_ROWS);
    bendx_buf_t bb;
    bendx_env_t env;
    bendx_t x;
    unsigned i, y;

    memset(&bb, 0, sizeof bb);
    bb.base = mem; bb.rowlen = rowlen; bb.rowpix = rowpix;
    bb.rows = rows; bb.ob_x = 2; bb.nbits = 12;
    memset(&env, 0, sizeof env);
    env.white = 4095;

    #define SETUP()                                                     \
        do {                                                            \
            unsigned k, s = 0x1234567u;                                 \
            for (k = 0; k < rowlen * rows; k++) {                       \
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;                \
                mem[k] = (unsigned char)s;                              \
            }                                                           \
            memcpy(ref, mem, rowlen * rows);                            \
            bendx_reset(&x);                                            \
        } while (0)

    // Byte order, adjacent pair: every even byte trades with its neighbour.
    SETUP();
    x.kind = BX_ENDIAN; x.amount = 0;
    bendx_apply(&x, &bb, &env, scratch);
    for (y = 0; y < rows; y++)
        for (i = 0; i + 1 < rowlen; i += 2)
            if (mem[y*rowlen + i] != ref[y*rowlen + i + 1] ||
                mem[y*rowlen + i + 1] != ref[y*rowlen + i])
            { fail("BX_ENDIAN byte pair"); goto done_endian; }
done_endian:

    // Bit slip of a whole byte is a pure byte rotation of the row.
    SETUP();
    x.kind = BX_SLIP; x.amount = 7; x.reach = 0;    // 7 + 1 = 8 bits
    bendx_apply(&x, &bb, &env, scratch);
    for (y = 0; y < rows; y++)
        for (i = 0; i < rowlen; i++)
            if (mem[y*rowlen + i] != ref[y*rowlen + (i + 1) % rowlen])
            { fail("BX_SLIP whole-byte rotation"); goto done_slip; }
done_slip:

    // A slip of the whole row is the identity, which is the wrap working.
    SETUP();
    x.kind = BX_SLIP; x.amount = 7; x.reach = 0;
    { unsigned n; for (n = 0; n < rowlen; n++) bendx_apply(&x, &bb, &env, scratch); }
    if (memcmp(mem, ref, rowlen * rows)) fail("BX_SLIP does not wrap to the identity");

    // Row address line 0 stuck: rows 0 and 1 trade, 2 and 3 trade, and so on.
    SETUP();
    x.kind = BX_ADDR; x.amount = 0;
    bendx_apply(&x, &bb, &env, scratch);
    for (y = 0; y + 1 < rows; y += 2)
        if (memcmp(mem + y*rowlen, ref + (y+1)*rowlen, rowlen) ||
            memcmp(mem + (y+1)*rowlen, ref + y*rowlen, rowlen))
        { fail("BX_ADDR row pair swap"); break; }

    // Field flip at a block of two is the same swap seen from the other side,
    // so the two must agree. They are different code paths and both ship.
    SETUP();
    x.kind = BX_FIELD; x.amount = 0;    // block = 2
    bendx_apply(&x, &bb, &env, scratch);
    for (y = 0; y + 1 < rows; y += 2)
        if (memcmp(mem + y*rowlen, ref + (y+1)*rowlen, rowlen))
        { fail("BX_FIELD block of two"); break; }

    // Line memory: with period 2 and hold 1, every odd row becomes the even
    // row above it.
    SETUP();
    x.kind = BX_LINEMEM; x.amount = 0; x.reach = 0;
    bendx_apply(&x, &bb, &env, scratch);
    for (y = 1; y < rows; y += 2)
        if (memcmp(mem + y*rowlen, ref + (y-1)*rowlen, rowlen))
        { fail("BX_LINEMEM hold"); break; }
    for (y = 0; y < rows; y += 2)
        if (memcmp(mem + y*rowlen, ref + y*rowlen, rowlen))
        { fail("BX_LINEMEM touched a row it was still clocking"); break; }

    // Row banding: with rowmod 1 every row is in scope; with rowmod 2 rows 2
    // and 3 of every four are untouched.
    SETUP();
    x.kind = BX_ENDIAN; x.amount = 3; x.rowmod = 2;
    bendx_apply(&x, &bb, &env, scratch);
    for (y = 0; y < rows; y++)
    {
        int same = (memcmp(mem + y*rowlen, ref + y*rowlen, rowlen) == 0);
        int want_touched = ((y % 4) < 2);
        if (want_touched == same) { fail("rowmod banding"); break; }
    }

    // A lane mask means the lanes outside it keep their value, whatever the
    // effect did. Checked on the loudest effect there is.
    SETUP();
    x.kind = BX_ENDIAN; x.amount = 4; x.lanes = 0x0f;
    bendx_apply(&x, &bb, &env, scratch);
    for (i = 0; i < rowlen * rows; i++)
        if ((mem[i] & 0xf0) != (ref[i] & 0xf0))
        { fail("lane mask leaked into a masked lane"); break; }

    // CFA phase by one pixel is a one-pixel horizontal roll of every row, in
    // the pixel domain rather than the byte domain.
    SETUP();
    x.kind = BX_CFA; x.amount = 1; x.reach = 0;
    bendx_apply(&x, &bb, &env, scratch);
    for (y = 0; y < rows; y++)
        for (i = 0; i < rowpix; i++)
            if (bendx_get(mem + y*rowlen, i, 12) !=
                bendx_get(ref + y*rowlen, (i + 1) % rowpix, 12))
            { fail("BX_CFA one-pixel roll"); goto done_cfa; }
done_cfa:

    printf("  semantics: ok\n");
    #undef SETUP
    free(mem); free(ref); free(scratch);
}

//-------------------------------------------------------------------
// bendx_random has to produce something sanitize() accepts, every time, for
// every effect - it is wired to a button and a profile it cannot represent is
// a profile that silently resets under the user's finger.

static void test_random(void)
{
    int kind;
    unsigned s = 12345;

    for (kind = 1; kind < BX_COUNT; kind++)
    {
        int n;
        for (n = 0; n < 500; n++)
        {
            bendx_t x, before;
            bendx_reset(&x);
            x.kind = (unsigned char)kind;
            s = bendx_random(&x, s);
            before = x;
            bendx_sanitize(&x);
            if (memcmp(&before, &x, sizeof x))
            { fail("bendx_random produced a profile sanitize() had to fix"); printf("    %s\n", bendx_name(kind)); return; }
            if (x.kind != kind)
            { fail("bendx_random changed the effect"); return; }
            // A foreign bus at Replace and full lanes does not damage the
            // photograph, it removes it - so neither the reroll nor the effect
            // switch is allowed to land there on its own.
            if (bendx_needs_env(kind) && x.mix == BXMIX_SET)
            { fail("bendx_random put a foreign bus on Replace"); printf("    %s\n", bendx_name(kind)); return; }
        }
    }

    for (kind = 0; kind < BX_COUNT; kind++)
    {
        bendx_t x, after;
        bendx_reset(&x);
        x.amount = 200; x.reach = 200; x.mix = BXMIX_AND; x.rowmod = 9;
        bendx_set_kind(&x, kind);
        if (x.kind != kind)      { fail("bendx_set_kind did not set the effect"); return; }
        if (x.amount || x.reach) { fail("bendx_set_kind carried a knob over"); printf("    %s\n", bendx_name(kind)); return; }
        if (bendx_needs_env(kind) && x.mix == BXMIX_SET)
        { fail("bendx_set_kind started a foreign bus on Replace"); return; }
        after = x;
        bendx_sanitize(&x);
        if (memcmp(&after, &x, sizeof x))
        { fail("bendx_set_kind produced something sanitize() had to fix"); return; }
    }
    printf("  random and set_kind: ok\n");
}

//-------------------------------------------------------------------
// Everything the UI reads has to exist for every effect, or the picker draws a
// null pointer.

static void test_strings(void)
{
    int kind;
    for (kind = 0; kind < BX_COUNT; kind++)
    {
        char label[16];
        bendx_t x;
        const char *n = bendx_name(kind);
        const char *d = bendx_desc(kind);

        if (!n || !d)            { fail("missing name or description"); return; }
        if (strlen(n) > 12)      { fail("effect name too long for the picker"); printf("    %s\n", n); }
        if (strlen(d) > 36)      { fail("effect description too long for the menu"); printf("    %s\n", d); }

        bendx_reset(&x);
        x.kind   = (unsigned char)kind;
        x.amount = (unsigned char)bendx_amount_max(kind);
        x.reach  = (unsigned char)bendx_reach_max(kind);
        memset(label, 0xcc, sizeof label);
        bendx_label(&x, label);
        if (strlen(label) > 7)   { fail("amount label too long"); printf("    %s -> %s\n", n, label); }
    }
    printf("  strings: ok\n");
}

//-------------------------------------------------------------------
// The chain. Its invariant - item[0..n-1] locked, item[n] live - is what every
// marker in the picker is read off, so a hole or an off-by-one there is a UI
// that lies about what will run.

static void test_chain(void)
{
    bendx_chain_t c;
    int i;

    bendx_chain_reset(&c);
    if (bendx_chain_count(&c) != 0)      { fail("fresh chain is not empty"); return; }
    if (!bendx_chain_live(&c))           { fail("fresh chain has no live slot"); return; }
    if (bendx_chain_lock(&c))            { fail("locked an Off live slot"); return; }

    // Fill it. Each lock must freeze what was live and open a clean slot.
    for (i = 0; i < BENDX_CHAIN_MAX; i++)
    {
        bendx_t *live = bendx_chain_live(&c);
        if (!live) { fail("ran out of live slots early"); return; }
        bendx_set_kind(live, BX_SLIP + i);
        live->amount = (unsigned char)(i + 1);
        if (bendx_chain_count(&c) != i + 1)
        { fail("count wrong with a live entry"); return; }
        if (!bendx_chain_lock(&c)) { fail("could not lock"); return; }
        if (bendx_chain_count(&c) != i + 1)
        { fail("locking changed the count"); return; }
    }

    if (bendx_chain_live(&c))  { fail("full chain still offers a live slot"); return; }
    if (bendx_chain_lock(&c))  { fail("locked past the end"); return; }
    if (bendx_chain_count(&c) != BENDX_CHAIN_MAX) { fail("full count wrong"); return; }

    // Order and knobs must have survived, because that is the whole promise of
    // locking: what you tuned is what runs, in the order you committed it.
    for (i = 0; i < BENDX_CHAIN_MAX; i++)
    {
        if (c.item[i].kind != BX_SLIP + i)      { fail("chain order lost"); return; }
        if (c.item[i].amount != i + 1)          { fail("locked knob changed"); return; }
        if (bendx_chain_find(&c, BX_SLIP + i) != i) { fail("chain_find wrong"); return; }
    }

    // Removing from the middle closes the gap and keeps the rest in order.
    bendx_chain_remove(&c, 1);
    if (bendx_chain_count(&c) != BENDX_CHAIN_MAX - 1) { fail("remove did not shrink"); return; }
    if (c.item[0].kind != BX_SLIP)     { fail("remove disturbed an earlier entry"); return; }
    if (c.item[1].kind != BX_SLIP + 2) { fail("remove did not close the gap"); return; }
    if (c.item[2].kind != BX_SLIP + 3) { fail("remove did not close the gap"); return; }
    if (!bendx_chain_live(&c))         { fail("remove did not free a live slot"); return; }

    // A hole in the locked run has to be closed by sanitize, whatever put it
    // there - an older config block, a shorter effect list on an older build.
    bendx_chain_reset(&c);
    bendx_set_kind(&c.item[0], BX_SLIP);
    bendx_set_kind(&c.item[1], BX_OFF);
    bendx_set_kind(&c.item[2], BX_TEAR);
    c.n = 3;
    bendx_chain_sanitize(&c);
    if (c.n != 2)                   { fail("sanitize left a hole in the chain"); return; }
    if (c.item[0].kind != BX_SLIP ||
        c.item[1].kind != BX_TEAR)  { fail("sanitize closed the hole wrongly"); return; }

    // Garbage n from a config block written by anything at all.
    bendx_chain_reset(&c);
    c.n = 200;
    bendx_chain_sanitize(&c);
    if (c.n > BENDX_CHAIN_MAX)      { fail("sanitize accepted an impossible n"); return; }

    // Slow is any-of, because the cost is additive and one slow entry in four
    // is still a slow shot.
    bendx_chain_reset(&c);
    bendx_set_kind(bendx_chain_live(&c), BX_SLIP);
    bendx_chain_lock(&c);
    if (bendx_chain_slow(&c)) { fail("fast chain reported slow"); return; }
    bendx_set_kind(bendx_chain_live(&c), BX_CFA);
    if (!bendx_chain_slow(&c)) { fail("slow live entry not reported"); return; }

    printf("  chain: ok\n");
}

//-------------------------------------------------------------------
// A chain applied end to end must stay inside the same fences one profile does
// - the effects run back to back over a buffer each of them has already been
// writing into, which is exactly where a wrap assumption goes wrong.

static void sweep_chain(int nbits, unsigned rowpix, unsigned rows)
{
    unsigned rowlen = (rowpix * (unsigned)nbits) / 8u;
    fenced_t buf, scratch, rom, jpg;
    bendx_buf_t bb;
    bendx_env_t env;
    bendx_chain_t c;
    int a, b, n = 0, checked = 0;

    fence_alloc(&buf,     rowlen * rows);
    fence_alloc(&rom,     64 * 1024);
    fence_alloc(&jpg,     48 * 1024);
    fence_fill(&rom, 0x1234); fence_fill(&jpg, 0x4567);

    memset(&bb, 0, sizeof bb);
    bb.base = buf.base; bb.rowlen = rowlen; bb.rowpix = rowpix;
    bb.rows = rows; bb.ob_x = 2; bb.nbits = nbits;

    memset(&env, 0, sizeof env);
    env.rom = rom.base; env.rom_len = rom.size;
    env.jpg = jpg.base; env.jpg_len = jpg.size;
    env.black = (1u << (nbits - 5)) - 1u;
    env.white = (1u << nbits) - 1u;

    // Every ordered pair of effects, then those pairs padded out to a full
    // chain. Pairs rather than all four-deep combinations: the effects do not
    // interact except through the buffer, so what is being proved is "runs
    // after anything, over anything" and that is a pair property.
    for (a = 1; a < BX_COUNT; a++)
    for (b = 1; b < BX_COUNT; b++, n++)
    {
        int i;

        bendx_chain_reset(&c);
        bendx_set_kind(&c.item[0], a);
        bendx_set_kind(&c.item[1], b);
        c.item[0].amount = (unsigned char)(n % (bendx_amount_max(a) + 1));
        c.item[1].amount = (unsigned char)(n % (bendx_amount_max(b) + 1));
        c.item[0].reach  = (unsigned char)(n % (bendx_reach_max(a) + 1));
        c.item[1].reach  = (unsigned char)(n % (bendx_reach_max(b) + 1));
        c.n = 2;
        bendx_chain_sanitize(&c);

        // Sized exactly the way raw.c sizes it - from the chain, not from the
        // largest effect that exists. If bendx_chain_scratch_bytes() ever
        // under-reports for a pair, this fence is what catches it.
        fence_alloc(&scratch, bendx_chain_scratch_bytes(&c, rowlen));

        fence_fill(&buf, 0xfeed ^ (unsigned)n);
        memset(scratch.base, 0, scratch.size);

        for (i = 0; i < bendx_chain_count(&c); i++)
            bendx_apply(&c.item[i], &bb, &env, scratch.base);
        checked++;

        if (!fence_check(&buf, "a chained effect wrote outside the frame buffer"))
        { printf("    %s then %s\n", bendx_name(a), bendx_name(b)); return; }
        if (!fence_check(&scratch, "a chained effect wrote outside the scratch rows"))
        { printf("    %s then %s\n", bendx_name(a), bendx_name(b)); return; }
        if (!fence_check(&rom, "a chained effect wrote into the ROM window")) return;
        if (!fence_check(&jpg, "a chained effect wrote into the JPEG buffer")) return;
        fence_free(&scratch);
    }

    printf("  %dbpp %ux%u: %d chained pairs, fences intact\n", nbits, rowpix, rows, checked);

    fence_free(&buf);
    fence_free(&rom); fence_free(&jpg);
}

//-------------------------------------------------------------------

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);   // so a hang says where it hung
    printf("bendx selftest\n");

    test_accessors();
    test_strings();
    test_random();
    test_chain();
    test_inert();
    test_semantics();

    // The two real geometries, at a fraction of the row count - the effects
    // that key off the frame height are all proportional, and the fences care
    // about the row length.
    sweep(10, 3152, 24);        // a470
    sweep(12, 3720, 24);        // a480

    // And two that divide badly, because a future port's row length will not
    // be a multiple of the packed group and the tail handling has to hold.
    sweep(10, 3160, 9);
    sweep(12, 3722, 9);

    // And every ordered pair of effects run back to back, because a chained
    // effect works on a buffer the one before it has already rewritten.
    sweep_chain(12, 3720, 12);
    sweep_chain(10, 3160, 9);

    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}
