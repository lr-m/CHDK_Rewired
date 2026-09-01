//-------------------------------------------------------------------
// The recipe inside the photograph - see include/bend_tag.h.
//-------------------------------------------------------------------

#include "platform.h"
#include "stdlib.h"
#include "string.h"
#include "bend_tag.h"
#include "bend_shot.h"

//-------------------------------------------------------------------
// Timing.
//
// The queue is filled in the raw hook, which runs before Canon has developed
// the JPEG, so the file named there does not exist yet. Rather than guess how
// long that takes on a given card, the service waits for the file to be there
// *and* to end in EOI - a JPEG that has been closed ends FFD9 and one that is
// still being written does not - and gives up after a while so a shot that was
// discarded, or written somewhere else, cannot leave a job pending forever.

#define BT_FIRST_MS     800         // before the first look
#define BT_GIVEUP_MS    20000       // before a pending job is abandoned
#define BT_COPY_BUF     8192        // rewrite chunk

// Enough for the comment, which is fixed-shape text plus the hex record. The
// record is a hundred-odd bytes and hex doubles it; the rest is six short
// lines. Static rather than malloc'd because it is filled from the raw hook,
// on a path that has just handed the heap a raw-sized buffer.
#define BT_TEXT_MAX     1024

static char bt_path[BEND_TAG_PATHLEN];
static char bt_text[BT_TEXT_MAX];
static int  bt_len;
static int  bt_at;              // tick the job was queued
static int  bt_pending;

//-------------------------------------------------------------------
// Building the comment.

static const char bt_hexd[] = "0123456789abcdef";

static int bt_str(char *p, int at, int max, const char *s)
{
    while (*s && at < max - 1) p[at++] = *s++;
    return at;
}

static int bt_num(char *p, int at, int max, int v)
{
    char tmp[12];
    int n = 0;

    if (v < 0) { if (at < max - 1) p[at++] = '-'; v = -v; }
    do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v && n < (int)sizeof(tmp));
    while (n-- > 0 && at < max - 1) p[at++] = tmp[n];
    return at;
}

// The human half. Written as the strip reads it - pin 9 first - because the
// person reading this later is the person who was looking at that strip.
static int bt_describe(char *p, int at, int max,
                       const bend_t *b, int bend_on,
                       const bendx_chain_t *c, int bendx_on,
                       const bend_segs_t *s)
{
    char name[8];
    int i;

    at = bt_str(p, at, max, BEND_TAG_MARK "\n");

    if (bend_on)
    {
        at = bt_str(p, at, max, "bend");
        for (i = CAM_SENSOR_BITS_PER_PIXEL - 1; i >= 0; i--)
        {
            bend_src_name(b->route[i], name);
            at = bt_str(p, at, max, " ");
            at = bt_num(p, at, max, i);
            at = bt_str(p, at, max, ":");
            at = bt_str(p, at, max, name);
        }
        at = bt_str(p, at, max, "\ndepth ");
        at = bt_num(p, at, max, b->depth);
        at = bt_str(p, at, max, "  trash ");
        at = bt_str(p, at, max, bend_trash_names[b->trash_type]);
        at = bt_str(p, at, max, "\n");
    }

    if (s->layout != BSEG_OFF)
    {
        int n = bend_seg_count(s->layout);
        at = bt_str(p, at, max, "seg ");
        at = bt_str(p, at, max, bend_seg_name(s->layout));
        for (i = 0; i < n; i++)
        {
            at = bt_str(p, at, max, "  ");
            at = bt_num(p, at, max, i + 1);
            at = bt_str(p, at, max, ":");
            // The machine record below contains the complete matrix for every
            // region. A BENDnn name is only a reference to a mutable file and
            // becomes false when that preset is deleted, so the human summary
            // names the self-contained matrices instead.
            at = bt_str(p, at, max, "bend");
            at = bt_num(p, at, max, i + 1);
        }
        at = bt_str(p, at, max, "\n");
    }

#ifdef CAM_BEND_EXPERIMENTAL
    if (bendx_on)
    {
        int n = c->n;
        at = bt_str(p, at, max, "x");
        for (i = 0; i < n && i < BENDX_CHAIN_MAX; i++)
        {
            char amt[16];
            if (i) at = bt_str(p, at, max, " >");
            at = bt_str(p, at, max, " ");
            at = bt_str(p, at, max, bendx_name(c->item[i].kind));
            bendx_label(&c->item[i], amt);
            if (amt[0])
            {
                at = bt_str(p, at, max, " ");
                at = bt_str(p, at, max, amt);
            }
        }
        at = bt_str(p, at, max, "\n");
    }
#else
    (void)c; (void)bendx_on;
#endif
    return at;
}

//-------------------------------------------------------------------

void bend_tag_queue(const char *dir, long num,
                    const bend_t *b, int bend_on,
                    const bendx_chain_t *c, int bendx_on,
                    const bend_segs_t *s)
{
    unsigned char rec[BEND_SHOT_REC_MAX];
    int at, n, i;

    if (!dir || !b || !c || !s) return;
    if ((int)strlen(dir) + 16 >= (int)sizeof(bt_path)) return;

    // The Canon file for this shot, the same way the sidecar names itself.
    sprintf(bt_path, "%s/IMG_%04d.JPG", dir, (int)num);

    at = bt_describe(bt_text, 0, BT_TEXT_MAX, b, bend_on, c, bendx_on, s);

    // The exact record a sidecar would hold, in hex so the whole comment stays
    // printable - a JPEG comment with raw bytes in it is legal but it is not
    // something a person can read out of `strings`, and half the point of
    // putting this in the picture is that it can be read anywhere.
    n = bend_shot_pack(rec, sizeof(rec), b, bend_on, c, bendx_on, s);
    if (n > 0 && at + n * 2 + 8 < BT_TEXT_MAX)
    {
        at = bt_str(bt_text, at, BT_TEXT_MAX, "BSH1:");
        for (i = 0; i < n; i++)
        {
            bt_text[at++] = bt_hexd[(rec[i] >> 4) & 0xf];
            bt_text[at++] = bt_hexd[rec[i] & 0xf];
        }
        bt_text[at++] = '\n';
    }
    bt_text[at] = 0;

    bt_len     = at;
    bt_at      = (int)get_tick_count();
    bt_pending = 1;
}

//-------------------------------------------------------------------
// Rewriting the file.
//
// SOI, our comment, then everything the original had after its SOI. Written
// beside the picture and moved over it at the end, so a failure anywhere -
// card full, card pulled - leaves the photograph exactly as Canon wrote it and
// costs nothing but a stray .TMP.

#ifndef CAM_BEND_TAG_INJECT
static int bt_tmp_path(const char *jpg, char *out, int outlen)
{
    int len = (int)strlen(jpg);

    if (len + 1 > outlen || len < 4) return 0;
    strcpy(out, jpg);
    strcpy(out + len - 4, ".TMP");
    return 1;
}

// Is the file a complete JPEG that we have not already tagged? Returns 1 to go
// ahead, 0 to wait, -1 to give up on it.
// Two ways of getting the recipe into the picture, and which one a body uses is
// decided by whether CHDK can see Canon's writes on it.
//
// CAM_BEND_TAG_INJECT - the good one. CHDK replaces Canon's file-write task, so
// fwt_write() sees every chunk on its way to the card and the comment is
// slipped in as the picture is written. One extra write of a few hundred bytes,
// no delay, no temp file. Needs DryOS (fwt_write is inside #ifdef CAM_DRYOS)
// *and* boot.c to actually install the replacement task: true on the a470 and
// a480, false on every VxWorks body here.
//
// Otherwise - the fallback below. Wait for the picture to be finished, then
// copy it to a .TMP with the comment spliced in at the front and rename over
// the original. Inserting at the front of a file means rewriting all of it:
// 13.6MB of I/O for a 6.8MB frame, several seconds, and it runs in spytask so
// it stalls everything CHDK draws for that long. That is the fault this whole
// mechanism was rebuilt to avoid - it is kept only because on those bodies the
// alternative is no tag at all.
// 1 ready, 0 not yet, -1 already ours.
static int bt_ready(const char *path)
{
    unsigned char head[4];
    int fd, ok = 0;

    fd = open(path, O_RDONLY, 0777);
    if (fd < 0) return 0;                       // not written yet

    if (read(fd, head, 2) == 2 && head[0] == 0xff && head[1] == 0xd8)
    {
        // Already ours? The next segment is a comment beginning with the mark.
        if (read(fd, head, 2) == 2 && head[0] == 0xff && head[1] == 0xfe)
        {
            char mark[sizeof(BEND_TAG_MARK)];
            if (read(fd, head, 2) == 2 &&
                read(fd, mark, sizeof(mark) - 1) == (int)sizeof(mark) - 1)
            {
                mark[sizeof(mark) - 1] = 0;
                if (strcmp(mark, BEND_TAG_MARK) == 0) { close(fd); return -1; }
            }
        }

        // Finished? A closed JPEG ends FFD9. One still being written does not.
        if (lseek(fd, -2, SEEK_END) >= 0 &&
            read(fd, head, 2) == 2 && head[0] == 0xff && head[1] == 0xd9)
            ok = 1;
    }
    close(fd);
    return ok;
}

static int bt_rewrite(const char *path, const char *text, int len)
{
    char tmp[BEND_TAG_PATHLEN];
    unsigned char hdr[4];
    unsigned char *buf;
    int src, dst, n, seg, ok = 0;

    // A comment segment's length field covers itself.
    seg = len + 2;
    if (len <= 0 || seg > 0xfffd) return 0;
    if (!bt_tmp_path(path, tmp, sizeof(tmp))) return 0;

    buf = malloc(BT_COPY_BUF);
    if (!buf) return 0;

    src = open(path, O_RDONLY, 0777);
    if (src < 0) { free(buf); return 0; }
    dst = open(tmp, O_WRONLY|O_CREAT|O_TRUNC, 0777);
    if (dst < 0) { close(src); free(buf); return 0; }

    hdr[0] = 0xff; hdr[1] = 0xd8;               // SOI
    hdr[2] = 0xff; hdr[3] = 0xfe;               // COM
    ok = (write(dst, hdr, 4) == 4);

    hdr[0] = (unsigned char)((seg >> 8) & 0xff);
    hdr[1] = (unsigned char)(seg & 0xff);
    ok = ok && (write(dst, hdr, 2) == 2)
            && (write(dst, text, len) == len);

    // Everything after the original's SOI, unchanged.
    if (ok && lseek(src, 2, SEEK_SET) >= 0)
    {
        while ((n = read(src, buf, BT_COPY_BUF)) > 0)
            if (write(dst, buf, n) != n) { ok = 0; break; }
        if (n < 0) ok = 0;
    }
    else
        ok = 0;

    close(dst);
    close(src);
    free(buf);

    if (!ok) { remove(tmp); return 0; }

    // Only now is the picture touched, and only by having the tagged copy put
    // in its place.
    remove(path);
    if (rename(tmp, path) != 0) return 0;
    return 1;
}

#endif // !CAM_BEND_TAG_INJECT

// Written into the picture as Canon writes it, not afterwards.
//
// **What this replaces, and why.** The tag is a JPEG comment segment that goes
// directly after the SOI, at the very front of the file. A filesystem cannot
// insert into the middle of a file - only overwrite or append - so putting ~500
// bytes at the front used to mean writing a whole new copy of the picture:
// 13.6MB of I/O and 1734 calls through Canon's FIO for a 6.8MB frame, several
// seconds, scaling with picture size. Worse, it ran in spytask, which is the
// task that draws, so it also cost the overlay for its duration; and being
// interrupted left a half-written .TMP beside an untagged JPEG.
//
// None of that was necessary. CHDK already replaces Canon's file-write task on
// these bodies (platform/<cam>/sub/<fw>/filewrite.c, installed from boot.c), so
// fwt_write() sees every chunk on the way to the card. The first chunk of a
// JPEG begins FFD8. Emit the SOI, then the comment, then the rest of the chunk,
// and the tag is in the file Canon is already writing - one extra write of a
// few hundred bytes, once, and nothing else changes.
//
// Two properties of this firmware make it safe, and both were checked:
//   - a COM inserted *before* the Exif APP1 does not disturb Exif, whose
//     internal offsets are relative to its own segment. That was already true
//     of the rewrite this replaces.
//   - neither CAM_FILEWRITETASK_SEEKS nor CAM_FILEWRITETASK_MULTIPASS is set
//     here, so Canon writes the file straight through and never seeks back to
//     patch a header at an offset our inserted bytes would have moved.
//
// Guards, because this is the photograph: inject only when a tag is pending,
// only into the file whose name we queued, only on that file's first chunk, and
// only if that chunk really does start FFD8. Anything else passes through
// untouched and the tag is simply not written.
static volatile int bt_arm;             // this file is ours; inject on chunk 1
static char bt_open_name[BEND_TAG_PATHLEN];
static unsigned char bt_hdr[4];         // FFFE + length; the text is written
                                        // straight from bt_text behind it

// Both of these run in Canon's file-write task. Copies and compares only.
void bend_tag_note_file_open(const char *name)
{
    int a = 0, b = 0, i;

    bt_arm = 0;
    if (!name || !bt_pending) return;

    for (i = 0; i < BEND_TAG_PATHLEN - 1 && name[i]; i++) bt_open_name[i] = name[i];
    bt_open_name[i] = 0;

    // Match on the "IMG_1234.JPG" tail: CHDK's path and Canon's are not spelt
    // identically, but that part identifies the file.
    while (bt_open_name[a]) a++;
    while (bt_path[b])      b++;
    if (a < 12 || b < 12) return;
    for (i = 1; i <= 12; i++)
        if (bt_open_name[a - i] != bt_path[b - i]) return;

    bt_arm = 1;
}

void bend_tag_note_file_closed(void)
{
    bt_arm = 0;
}

// The comment segment to insert, or 0. Handed back as its four-byte header
// plus a pointer to the text, rather than assembled into one buffer: the buffer
// was a 1032-byte .bss array to save one _Write call, on a body with under 2KB
// of core budget left. Two writes are free; the array was not.
int bend_tag_segment(const unsigned char **hdr, int *hdrlen,
                     const char **text, int *textlen)
{
    int seg;

    if (!bt_arm || !bt_pending || bt_len <= 0) return 0;
    seg = bt_len + 2;                   // the length field covers itself
    if (seg > 0xfffd) return 0;

    bt_hdr[0] = 0xff;
    bt_hdr[1] = 0xfe;
    bt_hdr[2] = (unsigned char)((seg >> 8) & 0xff);
    bt_hdr[3] = (unsigned char)(seg & 0xff);

    *hdr = bt_hdr;   *hdrlen  = 4;
    *text = bt_text; *textlen = bt_len;
    return 1;
}

// Called once the segment has actually reached the card.
void bend_tag_segment_written(void)
{
    bt_arm     = 0;
    bt_pending = 0;
}

#ifdef CAM_BEND_TAG_INJECT

// Nothing left for spytask to do - fwt_write() has it. Kept so the call site in
// core/main.c stays valid, and so a queued tag whose file never arrives is
// eventually dropped rather than held forever.
void bend_tag_service(void)
{
    if (!bt_pending) return;
    if (((int)get_tick_count() - bt_at) > BT_GIVEUP_MS) bt_pending = 0;
}

#else

// The fallback: poll, then rewrite. See the note above bt_ready(). This blocks
// spytask inside Canon's write and stalls the display for the length of the
// save; it is here because on these bodies there is no write to hook.
void bend_tag_service(void)
{
    int t;

    if (!bt_pending) return;

    t = (int)get_tick_count();
    if ((t - bt_at) < BT_FIRST_MS) return;

    switch (bt_ready(bt_path))
    {
    case 1:
        bt_rewrite(bt_path, bt_text, bt_len);
        bt_pending = 0;
        break;
    case -1:
        bt_pending = 0;
        break;
    default:
        if ((t - bt_at) > BT_GIVEUP_MS) bt_pending = 0;
        break;
    }
}

#endif // CAM_BEND_TAG_INJECT

//-------------------------------------------------------------------
// Reading one back.

static int bt_unhex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int bend_tag_read(const char *picture,
                  bend_t *b, int *bend_on,
                  bendx_chain_t *c, int *bendx_on,
                  bend_segs_t *s)
{
    unsigned char head[4];
    unsigned char rec[BEND_SHOT_REC_MAX];
    char *text;
    int fd, seg, n, i, at, ok = 0;

    if (!picture) return 0;

    fd = open(picture, O_RDONLY, 0777);
    if (fd < 0) return 0;

    // Only the segment we write ourselves, in the position we write it. A scan
    // of the whole header for any comment would also find Canon's, and a scan
    // of the whole file would read megabytes off the card to answer a question
    // about its first few hundred bytes.
    if (read(fd, head, 2) != 2 || head[0] != 0xff || head[1] != 0xd8 ||
        read(fd, head, 2) != 2 || head[0] != 0xff || head[1] != 0xfe ||
        read(fd, head, 2) != 2)
    {
        close(fd);
        return 0;
    }

    seg = ((int)head[0] << 8) | head[1];
    if (seg < 3 || seg > BT_TEXT_MAX + 2) { close(fd); return 0; }
    seg -= 2;

    text = malloc(seg + 1);
    if (!text) { close(fd); return 0; }
    n = read(fd, text, seg);
    close(fd);

    if (n == seg)
    {
        text[seg] = 0;
        if (strncmp(text, BEND_TAG_MARK, sizeof(BEND_TAG_MARK) - 1) == 0)
        {
            // The machine half, which is the only part read back - the lines
            // above it are prose and the record is the truth.
            char *p = strstr(text, "BSH1:");
            if (p)
            {
                p += 5;
                for (at = 0; at < (int)sizeof(rec); at++)
                {
                    int hi = bt_unhex(p[at * 2]);
                    int lo = (hi >= 0) ? bt_unhex(p[at * 2 + 1]) : -1;
                    if (lo < 0) break;
                    rec[at] = (unsigned char)((hi << 4) | lo);
                }
                ok = bend_shot_unpack(rec, at, b, bend_on, c, bendx_on, s);
            }
        }
    }
    free(text);
    (void)i;
    return ok;
}
