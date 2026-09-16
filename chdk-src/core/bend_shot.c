//-------------------------------------------------------------------
// What a picture was taken with - see include/bend_shot.h.
//
// Separate from bend_store.c because that file owns the numbered preset
// directory and its cursor, and this owns one file per photograph with no
// list, no cursor and no directory of its own. They share nothing but the
// extension.
//-------------------------------------------------------------------

#include "platform.h"
#include "stdlib.h"
#include "string.h"
#include "bend_shot.h"
#include "conf.h"
#include "gui.h"
#include "bend_store.h"
#include "bend_seg.h"
#include "camera_info.h"
#include "dirent.h"

//-------------------------------------------------------------------

#define BS_HDR      16
#define BS_HDR_V1   12
#define BS_VER      3

static const char bsh_magic[4] = { 'B', 'S', 'H', '1' };

//-------------------------------------------------------------------

static void bsh_put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

static unsigned bsh_get16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

// Versions 1 and 2 stored the original 24-entry experimental list. Preserve
// surviving recipes when reading them and turn the five retired effects off.
#ifdef CAM_BEND_EXPERIMENTAL
static void bsh_upgrade_bendx(bendx_chain_t *c, int ver)
{
    int i;
    if (ver >= 3) return;

    for (i = 0; i < BENDX_CHAIN_MAX; i++)
    {
        unsigned k = c->item[i].kind;
        if (k == 12 || k == 13 || k == 17 || k == 19 || k == 20)
            c->item[i].kind = BX_OFF;
        else if (k >= 21 && k <= 23)
            c->item[i].kind = (unsigned char)(k - 5);
        else if (k >= 18)
            c->item[i].kind = (unsigned char)(k - 3);
        else if (k >= 14)
            c->item[i].kind = (unsigned char)(k - 2);
    }
}
#endif

//-------------------------------------------------------------------

int bend_shot_pack(unsigned char *out, int outlen,
                   const bend_t *b, int bend_on,
                   const bendx_chain_t *c, int bendx_on,
                   const bend_segs_t *s)
{
    int n = 0;

    if (!out || !b || !c || !s) return 0;
    if (outlen < (int)BEND_SHOT_REC_MAX) return 0;

    memcpy(out, bsh_magic, 4);
    bsh_put16(out + 4,  BS_VER);
    bsh_put16(out + 6,  sizeof(bend_t));
    bsh_put16(out + 8,  sizeof(bendx_chain_t));
    bsh_put16(out + 10, (unsigned)((bend_on  ? BEND_SHOT_F_BEND  : 0) |
                                   (bendx_on ? BEND_SHOT_F_BENDX : 0)));
    bsh_put16(out + 12, sizeof(bend_segs_t));
    bsh_put16(out + 14, 0);
    n = BS_HDR;

    memcpy(out + n, b, sizeof(bend_t));         n += sizeof(bend_t);
    memcpy(out + n, c, sizeof(bendx_chain_t));  n += sizeof(bendx_chain_t);
    memcpy(out + n, s, sizeof(bend_segs_t));    n += sizeof(bend_segs_t);
    return n;
}

int bend_shot_unpack(const unsigned char *rec, int len,
                     bend_t *b, int *bend_on,
                     bendx_chain_t *c, int *bendx_on,
                     bend_segs_t *s)
{
    bend_t          rb;
    bendx_chain_t   rc;
    bend_segs_t     rs;
    int ver, at;

    // The version 1 header is the first twelve bytes of this one, so a record
    // that stops there is still a record - see the layout note in the header.
    if (!rec || len < BS_HDR_V1) return 0;
    if (memcmp(rec, bsh_magic, 4) != 0) return 0;
    if (bsh_get16(rec + 6) != sizeof(bend_t)) return 0;
    if (bsh_get16(rec + 8) != sizeof(bendx_chain_t)) return 0;

    ver = (int)bsh_get16(rec + 4);
    if (ver == 1)
    {
        at = BS_HDR_V1;
        if (len < at + (int)(sizeof(rb) + sizeof(rc))) return 0;
        // No segments in the record, and none in the camera that wrote it.
        // Off is not a fallback here, it is the truth about the picture.
        memset(&rs, 0, sizeof(rs));
    }
    else if (ver == 2 || ver == BS_VER)
    {
        at = BS_HDR;
        if (len < BS_HDR) return 0;
        if (bsh_get16(rec + 12) != sizeof(bend_segs_t)) return 0;
        if (len < at + (int)(sizeof(rb) + sizeof(rc) + sizeof(rs))) return 0;
    }
    else
        return 0;

    memcpy(&rb, rec + at, sizeof(rb)); at += sizeof(rb);
    memcpy(&rc, rec + at, sizeof(rc)); at += sizeof(rc);
    if (ver >= 2) memcpy(&rs, rec + at, sizeof(rs));

    // Straight off the card, and possibly written by another body or another
    // build, so all three go through the same gate everything else does before
    // anything is handed back to the caller.
    bend_sanitize(&rb, CAM_SENSOR_BITS_PER_PIXEL);
    bend_seg_sanitize(&rs, CAM_SENSOR_BITS_PER_PIXEL);
#ifdef CAM_BEND_EXPERIMENTAL
    bsh_upgrade_bendx(&rc, ver);
    bendx_chain_sanitize(&rc);
#endif

    if (b) *b = rb;
    if (c) *c = rc;
    if (s) *s = rs;
    {
        unsigned flags = bsh_get16(rec + 10);
        if (bend_on)  *bend_on  = (flags & BEND_SHOT_F_BEND)  ? 1 : 0;
        if (bendx_on) *bendx_on = (flags & BEND_SHOT_F_BENDX) ? 1 : 0;
    }
    return 1;
}

int bend_shot_save(const char *dir, long num,
                   const bend_t *b, int bend_on,
                   const bendx_chain_t *c, int bendx_on,
                   const bend_segs_t *s)
{
    char path[BEND_SHOT_PATHLEN];
    unsigned char hdr[BS_HDR];
    int fd, ok;

    if (!dir || !b || !c || !s) return 0;

    // The Canon file for this shot is <dir>/IMG_<num>.JPG, so the sidecar is
    // the same name with our extension. Built with sprintf rather than by
    // hand because unlike the preset path this one has a variable directory
    // in front of it and there is nothing to be saved by being clever.
    if ((int)strlen(dir) + 16 >= (int)sizeof(path)) return 0;
    sprintf(path, "%s/IMG_%04d" BEND_SHOT_EXT, dir, (int)num);

    memcpy(hdr, bsh_magic, 4);
    bsh_put16(hdr + 4,  BS_VER);
    bsh_put16(hdr + 6,  sizeof(bend_t));
    bsh_put16(hdr + 8,  sizeof(bendx_chain_t));
    bsh_put16(hdr + 10, (unsigned)((bend_on  ? BEND_SHOT_F_BEND  : 0) |
                                   (bendx_on ? BEND_SHOT_F_BENDX : 0)));
    bsh_put16(hdr + 12, sizeof(bend_segs_t));
    bsh_put16(hdr + 14, 0);

    fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0777);
    if (fd < 0) return 0;

    ok = (write(fd, hdr, BS_HDR) == BS_HDR) &&
         (write(fd, b, sizeof(bend_t)) == (int)sizeof(bend_t)) &&
         (write(fd, c, sizeof(bendx_chain_t)) == (int)sizeof(bendx_chain_t)) &&
         (write(fd, s, sizeof(bend_segs_t)) == (int)sizeof(bend_segs_t));
    close(fd);

    // A half written sidecar reads as a corrupt one every time the picture is
    // picked, which is worse than the picture having no record at all.
    if (!ok) remove(path);
    return ok;
}

//-------------------------------------------------------------------

int bend_shot_path(const char *picture, char *out, int outlen)
{
    int i, dot = -1, len;

    if (!picture || !out) return 0;
    len = (int)strlen(picture);
    if (len <= 0) return 0;

    // Last dot after the last separator, so a directory with a dot in it does
    // not get mistaken for the extension.
    for (i = 0; i < len; i++)
    {
        if (picture[i] == '/' || picture[i] == '\\') dot = -1;
        else if (picture[i] == '.')                 dot = i;
    }
    if (dot < 0) dot = len;

    if (dot + (int)sizeof(BEND_SHOT_EXT) > outlen) return 0;

    for (i = 0; i < dot; i++) out[i] = picture[i];
    strcpy(out + dot, BEND_SHOT_EXT);
    return 1;
}

int bend_shot_load(const char *picture,
                   bend_t *b, int *bend_on,
                   bendx_chain_t *c, int *bendx_on,
                   bend_segs_t *s)
{
    char path[BEND_SHOT_PATHLEN];
    unsigned char hdr[BS_HDR];
    bend_t          rb;
    bendx_chain_t   rc;
    bend_segs_t     rs;
    int fd, ver, ok = 0;

    if (!bend_shot_path(picture, path, sizeof(path))) return 0;

    fd = open(path, O_RDONLY, 0777);
    if (fd < 0) return 0;

    // The version 1 header is the first twelve bytes of this one, so it is
    // read to that length first and the rest only when the version says there
    // is a rest. Reading sixteen unconditionally would fail on a short file
    // that is perfectly good.
    memset(hdr, 0, sizeof(hdr));
    if (read(fd, hdr, BS_HDR_V1) == BS_HDR_V1 &&
        memcmp(hdr, bsh_magic, 4) == 0 &&
        bsh_get16(hdr + 6) == sizeof(bend_t) &&
        bsh_get16(hdr + 8) == sizeof(bendx_chain_t))
    {
        ver = (int)bsh_get16(hdr + 4);
        if (ver == 1)
        {
            // No segments in the file, and none in the camera that wrote it.
            // Off is not a fallback here, it is the truth about the picture.
            memset(&rs, 0, sizeof(rs));
            ok = (read(fd, &rb, sizeof(rb)) == (int)sizeof(rb)) &&
                 (read(fd, &rc, sizeof(rc)) == (int)sizeof(rc));
        }
        else if (ver == 2 || ver == BS_VER)
        {
            ok = (read(fd, hdr + BS_HDR_V1, BS_HDR - BS_HDR_V1) == BS_HDR - BS_HDR_V1) &&
                 bsh_get16(hdr + 12) == sizeof(bend_segs_t) &&
                 (read(fd, &rb, sizeof(rb)) == (int)sizeof(rb)) &&
                 (read(fd, &rc, sizeof(rc)) == (int)sizeof(rc)) &&
                 (read(fd, &rs, sizeof(rs)) == (int)sizeof(rs));
        }
    }
    close(fd);

    if (!ok) return 0;

    // Straight off the card, and possibly written by another body or another
    // build, so all three go through the same gate everything else does before
    // anything is handed back to the caller.
    bend_sanitize(&rb, CAM_SENSOR_BITS_PER_PIXEL);
    bend_seg_sanitize(&rs, CAM_SENSOR_BITS_PER_PIXEL);
#ifdef CAM_BEND_EXPERIMENTAL
    bsh_upgrade_bendx(&rc, ver);
    bendx_chain_sanitize(&rc);
#endif

    if (b)        *b        = rb;
    if (c)        *c        = rc;
    if (s)        *s        = rs;
    {
        unsigned flags = bsh_get16(hdr + 10);
        if (bend_on)  *bend_on  = (flags & BEND_SHOT_F_BEND)  ? 1 : 0;
        if (bendx_on) *bendx_on = (flags & BEND_SHOT_F_BENDX) ? 1 : 0;
    }
    return 1;
}

//-------------------------------------------------------------------
// Make a recipe the live bend.
//
// One function because there were two, and they had drifted. core/gui.c's
// playback path and modules/bend_picture.c's browser both assigned the same
// fields, but only the first resynced the menu's shadow ints - and
// bend_ui_push() runs at the top of every gui_redraw(), writing those shadows
// straight back into the current segment. A bend loaded from the browser
// therefore had nine of its fields (trash type and rate, hdiv, vdiv, both
// taps, bayer, row period, depth) overwritten with whatever was last left in
// the menu, on the very next redraw. It looked like the load had half worked.
//
// Order matters and is the order a preset goes through: assign, then prep,
// then sanitize - simplify reads nbits - then resync the UI, then detach.
void bend_shot_apply(const bend_t *b, int bend_on,
                     const bendx_chain_t *c, int bendx_on,
                     const bend_segs_t *sg)
{
    int i;

    if (b) { conf.bitbend = *b; conf.bitbend_enable = bend_on; }

    // The layout comes back with the matrices, because on a segmented frame
    // neither half is the record on its own - the same four bends under a
    // different layout are a different picture.
    if (sg) conf.bend_segs = *sg;

#ifdef CAM_BEND_EXPERIMENTAL
    if (c) { conf.bendx = *c; conf.bendx_enable = bendx_on; }
#else
    (void)c; (void)bendx_on;
#endif

    // A recipe carries every matrix itself. Preset slot numbers came from
    // whatever card wrote it and may have since been deleted or mean something
    // else here, so they are never restored.
    for (i = 0; i < BEND_SEG_MAX; i++) conf.bend_segs.slot[i] = 0;

    bend_seg_prep(camera_sensor.bits_per_pixel);
#ifdef CAM_BEND_EXPERIMENTAL
    bendx_chain_sanitize(&conf.bendx);
#endif

    // The half the browser was missing. Without it the next redraw pushes the
    // stale menu shadows back over what was just loaded.
    bend_ui_resync();

    // It did not come from a preset on the card, so nothing in the saved list
    // should still be claiming to be what is loaded.
    bend_store_detach();
}

//-------------------------------------------------------------------
// Which picture Canon is showing in playback, as a directory and file number.
// See the declaration in bend_shot.h, and tools/newport.py's
// find_playback_image() for where the four constants come from.
//
// Canon identifies the displayed picture by a packed handle held in the
// playback controller's state. The ID it draws in its own corner - the
// "100-0042" readout - is built by pulling the directory and file numbers out
// of that handle with two tiny leaf functions, and those leaves are pure: no
// memory access, no calls, no side effects. So the port supplies the handle's
// address and the two (mask, shift) pairs, and the arithmetic happens here
// rather than by calling into ROM. One word read and two shifts cannot disturb
// Canon, which matters because this runs while Canon owns the screen.
//
// This deliberately stops at the two numbers and does not go on to a path.
// Turning them into a filename means walking A/DCIM, and that walk plus the
// message it feeds is around a kilobyte - which the a480 does not have. It runs
// CHDK out of a 0x32000 ARAM region with a few hundred bytes spare, so the walk
// lives in modules/bend_picture.c, which is a .flt loaded from the card and
// costs the core image nothing. What has to be here is only the part that needs
// camera.h: modules cannot include it, so they cannot see these four constants.
//
// Defined on every build rather than under the #ifdef, because the module that
// calls it is built once for all bodies and cannot test the macro. On a port
// without the constants this is two instructions returning 0, and the caller
// falls back to asking which picture was meant.
//
// Derived and hardware-tested on the a430. Every other body's constants come
// from the same detector, which reproduces the a430's exactly.
int playback_current_image_id(int *dir, int *file)
{
#ifdef CAM_PLAYBACK_CURRENT_IMAGE
    int h = *(volatile int*)CAM_PB_IMAGE_HANDLE;
    int dn, fn;

    if (!h) return 0;
    dn = (h & CAM_PB_IMAGE_DIR_MASK)  >> CAM_PB_IMAGE_DIR_SHIFT;
    fn = (h & CAM_PB_IMAGE_FILE_MASK) >> CAM_PB_IMAGE_FILE_SHIFT;

    // Canon's own limits, which its file-spec parser enforces too. Outside
    // them this is not a still image - playback has not settled yet, or it is
    // showing a movie or a sound file.
    if (dn < 100 || dn > 999 || fn < 1 || fn > 9999) return 0;
    *dir = dn;
    *file = fn;
    return 1;
#else
    (void)dir; (void)file;
    return 0;
#endif
}
