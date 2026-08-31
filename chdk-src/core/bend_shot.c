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
