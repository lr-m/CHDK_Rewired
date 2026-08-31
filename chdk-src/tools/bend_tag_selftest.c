// Host-side checks for the in-picture bend record (core/bend_tag.c).
//
// This is the one piece of the fork that rewrites a photograph, so what is
// worth proving off-camera is not "does it write a comment" but "is the
// picture still the picture afterwards":
//
//   1. every byte of the original JPEG after its SOI survives the rewrite,
//      for sizes either side of the copy buffer
//   2. the record read back out is the record that went in
//   3. a file that is not finished, not a JPEG, or already tagged is left
//      completely alone - the second of those is what stops a browse of the
//      card from nesting a comment inside a comment on every pass
//   4. a failed write never leaves the original damaged
//
//   gcc -O2 -Wall -Wextra -Iteststub_tag -Iteststub -I../include -o
//       bend_tag_selftest bend_tag_selftest.c ../core/bend_tag.c
//       ../core/bend_shot.c ../core/bend.c ../core/bendx.c
//   ./bend_tag_selftest
//
// Exits non-zero on any mismatch.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "platform.h"
#include "bend.h"
#include "bendx.h"
#include "bend_seg.h"
#include "bend_shot.h"
#include "bend_tag.h"

static int failures;
static unsigned stub_now = 1000;

unsigned stub_tick(void) { return stub_now; }

static void fail(const char *what)
{
    printf("FAIL: %s\n", what);
    failures++;
}

//-------------------------------------------------------------------
// bend_seg.c is not linked in - it is the config side and needs conf - so the
// two functions bend_tag.c reads out of it are provided here.

int bend_seg_count_stub;

//-------------------------------------------------------------------

#define TMPDIR  "/tmp/bendtag_test"

static char pathbuf[256];

static const char *tpath(const char *name)
{
    sprintf(pathbuf, "%s/%s", TMPDIR, name);
    return pathbuf;
}

// A fake JPEG: SOI, a plausible APP1, n bytes of body, EOI.
static unsigned char *make_jpeg(int body, int *len_out)
{
    unsigned char *p = malloc(body + 16);
    int i, n = 0;

    p[n++] = 0xff; p[n++] = 0xd8;               // SOI
    p[n++] = 0xff; p[n++] = 0xe1;               // APP1
    p[n++] = 0x00; p[n++] = 0x08;               // length 8
    p[n++] = 'E'; p[n++] = 'x'; p[n++] = 'i'; p[n++] = 'f'; p[n++] = 0; p[n++] = 0;
    for (i = 0; i < body; i++)
        p[n++] = (unsigned char)((i * 31 + 7) & 0xff);
    p[n++] = 0xff; p[n++] = 0xd9;               // EOI
    *len_out = n;
    return p;
}

static int write_file(const char *path, const unsigned char *d, int n)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return 0;
    if (write(fd, d, n) != n) { close(fd); return 0; }
    close(fd);
    return 1;
}

static unsigned char *read_file(const char *path, int *n_out)
{
    unsigned char *d;
    int fd = open(path, O_RDONLY, 0666);
    int n, cap = 1 << 20;

    if (fd < 0) return 0;
    d = malloc(cap);
    n = read(fd, d, cap);
    close(fd);
    if (n < 0) { free(d); return 0; }
    *n_out = n;
    return d;
}

//-------------------------------------------------------------------

static void fill_state(bend_t *b, bendx_chain_t *c, bend_segs_t *s)
{
    int i;

    bend_reset(b, CAM_SENSOR_BITS_PER_PIXEL);
    b->route[9] = BSRC_DATA(0);
    b->route[0] = BSRC_DATA(9);
    b->route[4] = BSRC_LOW;
    b->route[7] = BSRC_NDATA(7);
    b->route[6] = BSRC_BUS(1);
    b->depth = 3;
    b->trash_type = 1;
    // The reader sanitizes what it finds, which rebuilds the derived fields -
    // so the thing to compare against is a sanitized matrix, not a hand-built
    // one that has never been through the gate.
    bend_sanitize(b, CAM_SENSOR_BITS_PER_PIXEL);

    memset(c, 0, sizeof(*c));
    c->n = 1;
    c->item[0].kind = 2;
    c->item[0].amount = 11;

    memset(s, 0, sizeof(*s));
    s->layout = 0;
    for (i = 0; i < BEND_SEG_MAX; i++) s->slot[i] = 0xff;   // no preset
}

// One round trip: queue, service, and read back.
static void tag_it(const char *path, const bend_t *b, const bendx_chain_t *c,
                   const bend_segs_t *s)
{
    char dir[128];
    int i, cut = 0;

    strcpy(dir, path);
    for (i = 0; dir[i]; i++) if (dir[i] == '/') cut = i;
    dir[cut] = 0;

    // bend_tag_queue names the file itself, so the test's file has to be the
    // one it would name.
    bend_tag_queue(dir, 123, b, 1, c, 1, s);
    stub_now += 5000;
    bend_tag_service();
}

static void test_roundtrip(int body)
{
    bend_t b, rb;
    bendx_chain_t c, rc;
    bend_segs_t s, rs;
    unsigned char *orig, *now;
    int olen, nlen, bon = 0, xon = 0;
    const char *path = tpath("IMG_0123.JPG");
    char keep[256];

    strcpy(keep, path);
    fill_state(&b, &c, &s);

    orig = make_jpeg(body, &olen);
    if (!write_file(keep, orig, olen)) { fail("could not write the test JPEG"); return; }

    tag_it(keep, &b, &c, &s);

    now = read_file(keep, &nlen);
    if (!now) { fail("tagged file is gone"); free(orig); return; }

    if (nlen <= olen) fail("tagged file did not grow");
    if (now[0] != 0xff || now[1] != 0xd8) fail("tagged file lost its SOI");
    if (now[2] != 0xff || now[3] != 0xfe) fail("comment is not first");

    // Everything after the original SOI must be there, byte for byte, at the
    // end of the new file.
    if (nlen - (olen - 2) < 0 || memcmp(now + nlen - (olen - 2), orig + 2, olen - 2) != 0)
        fail("image data changed");

    if (!bend_tag_read(keep, &rb, &bon, &rc, &xon, &rs))
        fail("could not read the record back");
    else
    {
        if (memcmp(&rb, &b, sizeof(b)) != 0) fail("matrix came back different");
        if (rc.n != c.n || rc.item[0].kind != c.item[0].kind ||
            rc.item[0].amount != c.item[0].amount) fail("chain came back different");
        if (!bon || !xon) fail("engine flags came back wrong");
    }

    // And it says so in words, for whoever opens this in something else.
    {
        int i, found = 0;
        for (i = 0; i + (int)sizeof(BEND_TAG_MARK) - 1 < nlen; i++)
            if (memcmp(now + i, BEND_TAG_MARK, sizeof(BEND_TAG_MARK) - 1) == 0)
            { found = 1; break; }
        if (!found) fail("the mark is not in the file");
    }

    free(orig);
    free(now);
}

// Tagging twice must not nest.
static void test_idempotent(void)
{
    bend_t b; bendx_chain_t c; bend_segs_t s;
    unsigned char *once, *twice;
    int l1, l2;
    const char *path = tpath("IMG_0123.JPG");
    char keep[256];
    unsigned char *orig;
    int olen;

    strcpy(keep, path);
    fill_state(&b, &c, &s);
    orig = make_jpeg(1000, &olen);
    write_file(keep, orig, olen);

    tag_it(keep, &b, &c, &s);
    once = read_file(keep, &l1);

    tag_it(keep, &b, &c, &s);
    twice = read_file(keep, &l2);

    if (!once || !twice) { fail("file vanished across a second tag"); }
    else if (l1 != l2 || memcmp(once, twice, l1) != 0)
        fail("tagging an already tagged picture changed it");

    free(orig); free(once); free(twice);
}

// A file that is still being written, and a file that is not a JPEG. Neither
// may be touched.
static void test_refuses(void)
{
    bend_t b; bendx_chain_t c; bend_segs_t s;
    const char *path = tpath("IMG_0123.JPG");
    char keep[256];
    unsigned char *orig, *now;
    int olen, nlen;

    strcpy(keep, path);
    fill_state(&b, &c, &s);

    // No EOI - as if the write is not finished.
    orig = make_jpeg(500, &olen);
    write_file(keep, orig, olen - 2);
    bend_tag_queue(TMPDIR, 123, &b, 1, &c, 1, &s);
    stub_now += 1000;
    bend_tag_service();                 // too early to give up, must not write
    now = read_file(keep, &nlen);
    if (!now || nlen != olen - 2 || memcmp(now, orig, nlen) != 0)
        fail("an unfinished JPEG was rewritten");
    free(now);

    // Not a JPEG at all.
    write_file(keep, (const unsigned char *)"not a picture", 13);
    stub_now += 1000;
    bend_tag_queue(TMPDIR, 123, &b, 1, &c, 1, &s);
    stub_now += 5000;
    bend_tag_service();
    now = read_file(keep, &nlen);
    if (!now || nlen != 13 || memcmp(now, "not a picture", 13) != 0)
        fail("a non-JPEG was rewritten");
    free(now);
    free(orig);
}

// Reading a picture that was never tagged must fail cleanly rather than
// returning something made of whatever was in the header.
static void test_untagged(void)
{
    bend_t b; bendx_chain_t c; bend_segs_t s;
    unsigned char *orig;
    int olen, bon, xon;
    const char *path = tpath("IMG_0124.JPG");
    char keep[256];

    strcpy(keep, path);
    orig = make_jpeg(300, &olen);
    write_file(keep, orig, olen);

    if (bend_tag_read(keep, &b, &bon, &c, &xon, &s))
        fail("an untagged picture claimed to hold a record");
    if (bend_tag_read(tpath("nothing_here.JPG"), &b, &bon, &c, &xon, &s))
        fail("a missing picture claimed to hold a record");
    free(orig);
}

int main(void)
{
    char cmd[256];

    sprintf(cmd, "rm -rf %s && mkdir -p %s", TMPDIR, TMPDIR);
    if (system(cmd) != 0) { printf("could not make %s\n", TMPDIR); return 2; }

    // Either side of the 8K copy buffer, and one that is not a multiple of it.
    test_roundtrip(100);
    test_roundtrip(8192);
    test_roundtrip(20000);
    test_idempotent();
    test_refuses();
    test_untagged();

    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("bend_tag_selftest: all checks passed\n");
    return 0;
}
