// Host-side checks for the shot sidecar (core/bend_shot.c).
//
// This one runs inside the capture hook, once per bent frame, on a card full
// of photographs that matter. Three things are worth proving off-camera:
//
//   1. what is written comes back identical, including which engines were on -
//      a recipe that does not reproduce the frame is worse than no recipe
//
//   2. a file that is truncated, empty, or written by a build with different
//      structs is refused rather than half read, because half a bend_t is a
//      bend nobody chose
//
//   3. the path arithmetic stays inside its buffer for every shape of name it
//      can be handed, including the ones the file browser can produce
//
// core/bend_shot.c is compiled against tools/teststub/, which redirects the
// CHDK file calls into the memory below.
//
//   gcc -O2 -Wall -Wextra -Iteststub -I../include -o bend_shot_selftest
//       bend_shot_selftest.c ../core/bend_shot.c ../core/bend.c ../core/bendx.c
//   ./bend_shot_selftest
//
// Exits non-zero on any mismatch.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"       // the stub in teststub/ - O_* and the file macros
#include "bend.h"
#include "bendx.h"
#include "bend_seg.h"
#include "bend_shot.h"

static int failures;

static void fail(const char *what)
{
    printf("FAIL: %s\n", what);
    failures++;
}

//-------------------------------------------------------------------
// One file, in memory. Enough for a test that never has two open at once.

#define STUB_MAX    512

static char          stub_name[128];
static unsigned char stub_data[STUB_MAX];
static int           stub_len;
static int           stub_pos;
static int           stub_open_flags;
static int           stub_exists;
// Set to fail the n'th write, to exercise the half-written-file path.
static int           stub_fail_write_at = -1;
static int           stub_write_count;

int stub_open(const char *name, int flags, int mode)
{
    (void)mode;
    if (!(flags & O_CREAT) && !stub_exists) return -1;
    if (!(flags & O_CREAT) && strcmp(name, stub_name) != 0) return -1;

    stub_open_flags = flags;
    stub_pos = 0;
    if (flags & O_TRUNC) { stub_len = 0; stub_write_count = 0; }
    if (flags & O_CREAT) { strcpy(stub_name, name); stub_exists = 1; }
    return 3;
}

int stub_read(int fd, void *buf, int n)
{
    int avail = stub_len - stub_pos;
    (void)fd;
    if (n > avail) n = avail;
    memcpy(buf, stub_data + stub_pos, n);
    stub_pos += n;
    return n;
}

int stub_write(int fd, const void *buf, int n)
{
    (void)fd;
    if (stub_write_count++ == stub_fail_write_at) return -1;
    if (stub_pos + n > STUB_MAX) return -1;
    memcpy(stub_data + stub_pos, buf, n);
    stub_pos += n;
    if (stub_pos > stub_len) stub_len = stub_pos;
    return n;
}

int stub_close(int fd) { (void)fd; return 0; }

int stub_remove(const char *name)
{
    if (stub_exists && strcmp(name, stub_name) == 0)
    { stub_exists = 0; stub_len = 0; return 0; }
    return -1;
}

static void stub_reset(void)
{
    stub_name[0] = 0;
    stub_len = stub_pos = 0;
    stub_exists = 0;
    stub_fail_write_at = -1;
    stub_write_count = 0;
}

//-------------------------------------------------------------------
// Something distinctive to write, so a field that silently fails to survive
// the trip shows up as a mismatch rather than as a zero that looks plausible.

static void make_state(bend_t *b, bendx_chain_t *c, unsigned salt)
{
    int i;

    bend_reset(b, 12);
    for (i = 0; i < 12; i++)
        b->route[i] = (unsigned char)((i + salt) % 12);
    b->route[3]  = (unsigned char)BSRC_HIGH;
    b->route[7]  = (unsigned char)BSRC_BUS(BUS_NOISE);
    b->depth     = 5;
    b->trash_type = TRASH_BURST;
    b->tap_dx    = -21;
    b->tap_dy    = 3;
    b->seed      = 0xdecafbadu ^ salt;
    bend_sanitize(b, 12);

    bendx_chain_reset(c);
    bendx_set_kind(bendx_chain_live(c), BX_SLIP);
    bendx_chain_live(c)->amount = 9;
    bendx_chain_lock(c);
    bendx_set_kind(bendx_chain_live(c), BX_BLOOM);
    bendx_chain_live(c)->amount = 4;
    bendx_chain_live(c)->reach  = 11;
    bendx_chain_lock(c);
    bendx_set_kind(bendx_chain_live(c), BX_SORT);
    bendx_chain_live(c)->amount = 7;
    bendx_chain_sanitize(c);
}

//-------------------------------------------------------------------

static void test_roundtrip(void)
{
    static const int flag_cases[4][2] = { {1,1}, {1,0}, {0,1}, {0,0} };
    int k;

    for (k = 0; k < 4; k++)
    {
        bend_t        b,  rb;
        bendx_chain_t c,  rc;
        bend_segs_t   sg, rsg;
        int bon = flag_cases[k][0], xon = flag_cases[k][1];
        int rbon = -1, rxon = -1;

        stub_reset();
        make_state(&b, &c, (unsigned)k);

        if (!bend_shot_save("A/DCIM/100CANON", 123, &b, bon, &c, xon, &sg))
        { fail("bend_shot_save refused a good write"); return; }

        if (strcmp(stub_name, "A/DCIM/100CANON/IMG_0123.BND") != 0)
        { fail("sidecar landed on the wrong name"); printf("    %s\n", stub_name); return; }

        if (!bend_shot_load("A/DCIM/100CANON/IMG_0123.JPG", &rb, &rbon, &rc, &rxon, &rsg))
        { fail("could not read back what was just written"); return; }

        if (memcmp(&b, &rb, sizeof b))  { fail("the matrix did not survive the trip"); return; }
        if (memcmp(&c, &rc, sizeof c))  { fail("the chain did not survive the trip"); return; }
        if (rbon != bon || rxon != xon) { fail("the enable flags did not survive"); return; }
    }
    printf("  round trip: ok\n");
}

// The picture is picked in a file browser, so the name handed back can be the
// JPEG, the sidecar itself, or something with no extension at all.
static void test_load_by_any_name(void)
{
    static const char * const names[] = {
        "A/DCIM/100CANON/IMG_0123.JPG",
        "A/DCIM/100CANON/IMG_0123.BND",
        "A/DCIM/100CANON/IMG_0123.DNG",
        "A/DCIM/100CANON/IMG_0123",
    };
    bend_t b, rb;
    bendx_chain_t c, rc;
    bend_segs_t   sg, rsg;
    unsigned i;

    stub_reset();
    make_state(&b, &c, 7);
    bend_shot_save("A/DCIM/100CANON", 123, &b, 1, &c, 1, &sg);

    for (i = 0; i < sizeof(names)/sizeof(names[0]); i++)
        if (!bend_shot_load(names[i], &rb, 0, &rc, 0, &rsg))
        { fail("could not load from a valid name"); printf("    %s\n", names[i]); return; }

    // A different frame must not pick up this one's recipe.
    if (bend_shot_load("A/DCIM/100CANON/IMG_0124.JPG", &rb, 0, &rc, 0, &rsg))
    { fail("loaded a bend for the wrong picture"); return; }

    printf("  names: ok\n");
}

static void test_rejects_damage(void)
{
    bend_t b, rb;
    bendx_chain_t c, rc;
    bend_segs_t   sg, rsg;
    int good_len;

    stub_reset();
    make_state(&b, &c, 3);
    bend_shot_save("A/DCIM/100CANON", 1, &b, 1, &c, 1, &sg);
    good_len = stub_len;

    // Truncated at every length short of complete. Every one must be refused:
    // the structs have no internal length markers, so a short read is a bend
    // nobody chose rather than a bend missing its tail.
    {
        int n;
        for (n = 0; n < good_len; n++)
        {
            stub_len = n;
            if (bend_shot_load("A/DCIM/100CANON/IMG_0001.JPG", &rb, 0, &rc, 0, &rsg))
            { fail("accepted a truncated sidecar"); printf("    at %d of %d bytes\n", n, good_len); return; }
        }
        stub_len = good_len;
    }

    // Wrong magic - a preset file from A/CHDK/BENDS dropped in here.
    stub_data[0] = 'B'; stub_data[1] = 'N'; stub_data[2] = 'D'; stub_data[3] = '1';
    if (bend_shot_load("A/DCIM/100CANON/IMG_0001.JPG", &rb, 0, &rc, 0, &rsg))
    { fail("accepted a preset file as a sidecar"); return; }
    stub_data[0] = 'B'; stub_data[1] = 'S'; stub_data[2] = 'H'; stub_data[3] = '1';

    // Wrong struct sizes - a build whose bend_t or chain moved.
    stub_data[6] ^= 0xff;
    if (bend_shot_load("A/DCIM/100CANON/IMG_0001.JPG", &rb, 0, &rc, 0, &rsg))
    { fail("accepted a sidecar with a different bend_t"); return; }
    stub_data[6] ^= 0xff;

    stub_data[8] ^= 0xff;
    if (bend_shot_load("A/DCIM/100CANON/IMG_0001.JPG", &rb, 0, &rc, 0, &rsg))
    { fail("accepted a sidecar with a different chain"); return; }
    stub_data[8] ^= 0xff;

    // And it still reads once the damage is undone, so the checks above are
    // rejecting the damage rather than everything.
    if (!bend_shot_load("A/DCIM/100CANON/IMG_0001.JPG", &rb, 0, &rc, 0, &rsg))
    { fail("refused a sidecar it had already accepted"); return; }

    // A write that fails part way must take the file with it.
    stub_reset();
    stub_fail_write_at = 1;             // header lands, the matrix does not
    if (bend_shot_save("A/DCIM/100CANON", 2, &b, 1, &c, 1, &sg))
    { fail("reported success on a failed write"); return; }
    if (stub_exists)
    { fail("left a half written sidecar on the card"); return; }

    printf("  damage: ok\n");
}

// The buffer is fixed and the name comes from a file browser, so every shape
// it can produce has to either fit or be refused - never write past the end.
static void test_path_bounds(void)
{
    char out[BEND_SHOT_PATHLEN + 8];
    char in[256];
    unsigned n;

    #define GUARD()  do {                                                     \
        unsigned g;                                                           \
        for (g = BEND_SHOT_PATHLEN; g < sizeof(out); g++)                     \
            if (out[g] != (char)0xAA)                                         \
            { fail("bend_shot_path wrote past the buffer"); return; }         \
    } while (0)

    // Exactly at, either side of, and far past the limit.
    for (n = 1; n < 200; n++)
    {
        unsigned i;
        for (i = 0; i < n; i++) in[i] = 'x';
        strcpy(in + n, ".JPG");

        memset(out, 0xAA, sizeof out);
        if (bend_shot_path(in, out, BEND_SHOT_PATHLEN))
        {
            if ((int)strlen(out) >= BEND_SHOT_PATHLEN)
            { fail("bend_shot_path returned an oversized path"); return; }
            if (strcmp(out + strlen(out) - 4, BEND_SHOT_EXT))
            { fail("bend_shot_path lost the extension"); return; }
        }
        GUARD();
    }

    // A dot in a directory name must not be mistaken for the extension.
    memset(out, 0xAA, sizeof out);
    if (!bend_shot_path("A/DIR.X/IMG_0001", out, BEND_SHOT_PATHLEN)) { fail("refused a valid path"); return; }
    if (strcmp(out, "A/DIR.X/IMG_0001" BEND_SHOT_EXT))
    { fail("bend_shot_path cut at a directory dot"); printf("    %s\n", out); return; }
    GUARD();

    // Degenerate inputs.
    memset(out, 0xAA, sizeof out);
    if (bend_shot_path("", out, BEND_SHOT_PATHLEN)) { fail("accepted an empty name"); return; }
    if (bend_shot_path(0, out, BEND_SHOT_PATHLEN))  { fail("accepted a null name"); return; }
    GUARD();
    #undef GUARD

    printf("  path bounds: ok\n");
}

//-------------------------------------------------------------------

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("bend_shot selftest\n");

    test_roundtrip();
    test_load_by_any_name();
    test_rejects_damage();
    test_path_bounds();

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all ok\n");
    return 0;
}
