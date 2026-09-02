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
// Two ways of getting the recipe into the picture, and which one a body uses is
// decided by whether CHDK can see Canon's writes on it.
//
// CAM_BEND_TAG_INJECT - the good one. CHDK replaces Canon's file-write task, so
// fwt_close() appends the comment as the picture is finished. One extra write
// of a few hundred bytes, inside the save Canon is already doing. Needs DryOS
// (the fwt_ hooks are inside #ifdef CAM_DRYOS) *and* boot.c to actually install
// the replacement task: true on the a470 and a480, false on every VxWorks body
// here.
//
// Otherwise - the fallback below. Wait for the picture to be finished, then
// open it and append the same segment. This runs in spytask, so what it costs
// is what the display stalls for, which is why it is an append and not the
// rewrite it used to be: putting the comment at the *front* means rewriting the
// whole file, 13.6MB of I/O for a 6.8MB frame and several seconds of frozen
// overlay. Appending is one open, one write of a few hundred bytes and one
// close, whatever the picture weighs. Both paths therefore put the tag in the
// same place - after the EOI - and bend_tag_read() has one way to find it.
//
// Is the file a complete JPEG that we have not already tagged?
// 1 ready, 0 not yet, -1 already ours.
static int bt_ready(const char *path)
{
    unsigned char head[4];
    char *tail;
    long size;
    int fd, n, i, win, ok = 0;

    fd = open(path, O_RDONLY, 0777);
    if (fd < 0) return 0;                       // not written yet

    if (read(fd, head, 2) != 2 || head[0] != 0xff || head[1] != 0xd8)
    {
        close(fd);
        return 0;
    }

    // Finished? A closed, untagged JPEG ends FFD9. One still being written does
    // not - and neither does one we have already tagged, since our segment goes
    // after that EOI. So the cheap test answers the common case, and only when
    // it fails do we pay for a look at the tail to tell those two apart.
    if (lseek(fd, -2, SEEK_END) >= 0 &&
        read(fd, head, 2) == 2 && head[0] == 0xff && head[1] == 0xd9)
    {
        close(fd);
        return 1;
    }

    size = lseek(fd, 0, SEEK_END);
    win  = BT_TEXT_MAX + 4;
    if (size < (long)win) win = (int)size;
    if (win < (int)sizeof(BEND_TAG_MARK) || lseek(fd, size - win, SEEK_SET) < 0)
    {
        close(fd);
        return 0;
    }

    tail = malloc(win + 1);
    if (!tail) { close(fd); return 0; }
    n = read(fd, tail, win);
    close(fd);

    if (n == win)
    {
        tail[win] = 0;
        for (i = 0; i + (int)sizeof(BEND_TAG_MARK) - 1 <= win; i++)
            if (strncmp(tail + i, BEND_TAG_MARK,
                        sizeof(BEND_TAG_MARK) - 1) == 0)
            {
                ok = -1;                        // already ours
                break;
            }
    }
    free(tail);
    return ok;
}

// The tag, appended after the picture's EOI.
//
// Nothing before the end of the file is touched, so there is no copy, no .TMP
// and no rename: the photograph on the card is the one Canon wrote, with our
// segment behind it. A decoder stops at the EOI and never reads this; anything
// looking for the tag reads the tail (bend_tag_read).
static int bt_append(const char *path, const char *text, int len)
{
    unsigned char hdr[4];
    int fd, seg, ok;

    // A comment segment's length field covers itself.
    seg = len + 2;
    if (len <= 0 || seg > 0xfffd) return 0;

    fd = open(path, O_WRONLY, 0777);
    if (fd < 0) return 0;

    if (lseek(fd, 0, SEEK_END) < 0) { close(fd); return 0; }

    hdr[0] = 0xff; hdr[1] = 0xfe;               // COM
    hdr[2] = (unsigned char)((seg >> 8) & 0xff);
    hdr[3] = (unsigned char)(seg & 0xff);

    ok = (write(fd, hdr, 4) == 4) && (write(fd, text, len) == len);
    close(fd);

    // A short write leaves trailing bytes that carry no mark, which reads back
    // the same as an untagged picture. The picture itself was never opened for
    // anything but appending, so it cannot have been damaged.
    return ok;
}

#endif // !CAM_BEND_TAG_INJECT

// Written into the picture as Canon writes it, not afterwards.
//
// **What this replaces, and why.** The tag is a JPEG comment segment appended
// after the EOI. It used to go in at the front, after the SOI, and a filesystem
// cannot insert into the middle of a file - only overwrite or append - so
// putting ~500 bytes there meant writing a whole new copy of the picture:
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
// One property of this firmware makes it safe, and it was checked: neither
// CAM_FILEWRITETASK_SEEKS nor CAM_FILEWRITETASK_MULTIPASS is set here, so Canon
// writes the file straight through and never seeks back to patch a header at an
// offset our appended bytes would have moved.
//
// What was *not* safe was doing this at the front of the file. It parsed
// correctly - the file on the card was valid and opened normally after a power
// cycle - but see fwt_close(): the picture is longer than Canon recorded, and
// only bytes past the EOI can be lost to that without breaking playback.
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

// The fallback: poll, then append. See the note above bt_ready(). The append
// itself is a few hundred bytes and returns immediately; what this still costs
// spytask is the poll, which is why it does not start until BT_FIRST_MS.
void bend_tag_service(void)
{
    int t;

    if (!bt_pending) return;

    t = (int)get_tick_count();
    if ((t - bt_at) < BT_FIRST_MS) return;

    switch (bt_ready(bt_path))
    {
    case 1:
        bt_append(bt_path, bt_text, bt_len);
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
    unsigned char rec[BEND_SHOT_REC_MAX];
    char *text, *mark;
    long size;
    int fd, seg, n, i, at, ok = 0;

    if (!picture) return 0;

    fd = open(picture, O_RDONLY, 0777);
    if (fd < 0) return 0;

    // The tag sits past the EOI on every body - appended by fwt_close() where
    // CHDK owns the write path, and by bt_append() where it does not - so it is
    // read from the tail. One read of at most a kilobyte, not a scan of the
    // file: the tag is the last thing written, so it is inside the last
    // segment-sized window or it is not there at all.
    size = lseek(fd, 0, SEEK_END);
    seg  = BT_TEXT_MAX + 4;
    if (size < (long)seg) seg = (int)size;
    if (seg < (int)sizeof(BEND_TAG_MARK) ||
        lseek(fd, size - seg, SEEK_SET) < 0)
    {
        close(fd);
        return 0;
    }

    text = malloc(seg + 1);
    if (!text) { close(fd); return 0; }
    n = read(fd, text, seg);
    close(fd);

    if (n == seg)
    {
        // Terminated here so that everything from the mark onwards - which is
        // our own text, since nothing follows it in the file - can be treated
        // as a string. The bytes before it are image data and are only ever
        // compared, never scanned as one.
        text[seg] = 0;
        mark = 0;
        for (i = 0; i + (int)sizeof(BEND_TAG_MARK) - 1 <= seg; i++)
            if (strncmp(text + i, BEND_TAG_MARK,
                        sizeof(BEND_TAG_MARK) - 1) == 0)
            {
                mark = text + i;
                break;
            }
        if (mark)
        {
            // The machine half, which is the only part read back - the lines
            // above it are prose and the record is the truth.
            char *p = strstr(mark, "BSH1:");
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
