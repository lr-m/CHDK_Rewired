//-------------------------------------------------------------------
// Bend presets on the card - see include/bend_store.h and BENDING_DESIGN.md.
//
// One file per preset, named BENDNN.BND, in A/CHDK/BENDS. Numbered rather than
// named because the only text entry this camera has is the four arrows and a
// SET key, and a preset you have to spell out is a preset you do not save.
//-------------------------------------------------------------------

#include "platform.h"
#include "stdlib.h"
#include "string.h"
#include "dirent.h"
#include "bend_store.h"
#include "conf.h"

//-------------------------------------------------------------------
// File layout. Fixed size, so a short read is a corrupt file and not a
// negotiation.
//
//   0  magic  'B','N','D','1'
//   4  ver    2 bytes, little endian
//   6  size   2 bytes, sizeof(bend_t) as written
//   8  the bend_t itself, byte for byte
//   +  optional bend_store_info_t (version 2)
//
// The size field is checked exactly rather than tolerantly. bend_t is a packed
// blob with no internal length markers, so a file written by a build with a
// different struct cannot be read half-way - and bend_sanitize() only rescues
// values that are out of range, not fields that have moved. Refusing is the
// only honest option, and it costs nothing: presets are cheap to make again.

#define BS_HDR      8
#define BS_VER      3

static const char bs_magic[4] = { 'B', 'N', 'D', '1' };

//-------------------------------------------------------------------

static unsigned char bs_slot[BEND_STORE_MAX];
static int bs_count = -1;       // -1 = the directory has not been read yet
static int bs_cur   = -1;

//-------------------------------------------------------------------

static char bs_up(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

// Slot number for a directory entry, or -1 if the name is not one of ours.
// Indexing up to [10] unconditionally is safe whatever the string length,
// because d_name is a fixed 100 byte array and the comparisons short-circuit
// on the first mismatch - a name shorter than the pattern fails on its
// terminator.
static int bs_parse(const char *name)
{
    int d0, d1;

    if (bs_up(name[0]) != 'B' || bs_up(name[1]) != 'E' ||
        bs_up(name[2]) != 'N' || bs_up(name[3]) != 'D') return -1;

    d0 = name[4];
    d1 = name[5];
    if (d0 < '0' || d0 > '9' || d1 < '0' || d1 > '9') return -1;

    if (name[6] != '.') return -1;
    if (bs_up(name[7]) != 'B' || bs_up(name[8]) != 'N' ||
        bs_up(name[9]) != 'D') return -1;
    if (name[10] != 0) return -1;

    return (d0 - '0') * 10 + (d1 - '0');
}

static void bs_path(int slot, char *buf)
{
    // sprintf with %02d would do, but this is called from a redraw path on a
    // camera where sprintf pulls in the whole formatter for two digits.
    strcpy(buf, BEND_STORE_DIR "/BEND00.BND");
    buf[sizeof(BEND_STORE_DIR "/BEND") - 1]     = (char)('0' + (slot / 10) % 10);
    buf[sizeof(BEND_STORE_DIR "/BEND") - 1 + 1] = (char)('0' + slot % 10);
}

static void bs_info_clean(bend_store_info_t *dst, const bend_store_info_t *src)
{
    memset(dst, 0, sizeof(*dst));
    if (!src) return;
    strncpy(dst->name, src->name, BEND_STORE_NAME_MAX);
    strncpy(dst->description, src->description, BEND_STORE_DESC_MAX);
}

//-------------------------------------------------------------------

int bend_store_rescan(void)
{
    DIR *d;
    struct dirent *e;
    int prev_slot = (bs_cur >= 0 && bs_cur < bs_count) ? bs_slot[bs_cur] : -1;
    int i;

    bs_count = 0;

    d = opendir(BEND_STORE_DIR);
    if (d)
    {
        while ((e = readdir(d)) != 0)
        {
            int slot = bs_parse(e->d_name);
            int j;

            if (slot < 0) continue;
            if (bs_count >= BEND_STORE_MAX) break;

            // Insertion sort. The list is walked with the arrows, so the order
            // has to be the order of the numbers on screen and not whatever
            // order the FAT driver happens to hand back.
            for (j = bs_count; j > 0 && bs_slot[j-1] > (unsigned char)slot; j--)
                bs_slot[j] = bs_slot[j-1];
            bs_slot[j] = (unsigned char)slot;
            bs_count++;
        }
        closedir(d);
    }

    // Follow the preset that was loaded to wherever it moved to, so saving or
    // deleting does not silently repoint the cursor at a different bend.
    bs_cur = -1;
    if (prev_slot >= 0)
        for (i = 0; i < bs_count; i++)
            if (bs_slot[i] == (unsigned char)prev_slot) { bs_cur = i; break; }

    return bs_count;
}

int bend_store_count(void)
{
    if (bs_count < 0) bend_store_rescan();
    return bs_count;
}

int bend_store_slot_at(int i)
{
    if (bs_count < 0) bend_store_rescan();
    if (i < 0 || i >= bs_count) return -1;
    return bs_slot[i];
}

// The other direction: where a slot number sits in the list. The segment rows
// hold slot numbers rather than list positions, because the list is rebuilt on
// every rescan and a position taken before a delete points at the wrong file
// afterwards, while a slot number either still exists or does not.
int bend_store_index_of(int slot)
{
    int i;

    if (bs_count < 0) bend_store_rescan();
    for (i = 0; i < bs_count; i++)
        if (bs_slot[i] == slot) return i;
    return -1;
}

int bend_store_cur(void)
{
    return bs_cur;
}

void bend_store_detach(void)
{
    bs_cur = -1;
}

//-------------------------------------------------------------------

static int bs_read_slot(int slot, bend_store_recipe_t *r, bend_store_info_t *info)
{
    char path[sizeof(BEND_STORE_DIR "/BEND00.BND")];
    unsigned char hdr[BS_HDR];
    bend_store_info_t skip;
    int fd, ver, ok = 0;

    memset(r, 0, sizeof(*r));
    if (info) memset(info, 0, sizeof(*info));
    bs_path(slot, path);
    fd = open(path, O_RDONLY, 0777);
    if (fd < 0) return 0;

    if (read(fd, hdr, BS_HDR) == BS_HDR &&
        memcmp(hdr, bs_magic, 4) == 0 &&
        (hdr[6] | (hdr[7] << 8)) == (int)sizeof(bend_t))
    {
        ver = hdr[4] | (hdr[5] << 8);
        if (ver >= 1 && ver <= BS_VER &&
            read(fd, &r->bend, sizeof(r->bend)) == (int)sizeof(r->bend))
        {
            r->bend_on = 1;
            ok = 1;
            if (ver >= 2)
                ok = read(fd, info ? info : &skip, sizeof(skip)) == (int)sizeof(skip);
            if (ok && ver >= 3)
                ok = read(fd, &r->bendx, sizeof(r->bendx)) == (int)sizeof(r->bendx)
                  && read(fd, &r->segs, sizeof(r->segs)) == (int)sizeof(r->segs)
                  && read(fd, &r->bend_on, 4) == 4;
        }
    }
    close(fd);
    if (ok)
    {
        bend_sanitize(&r->bend, CAM_SENSOR_BITS_PER_PIXEL);
        bend_seg_sanitize(&r->segs, CAM_SENSOR_BITS_PER_PIXEL);
#ifdef CAM_BEND_EXPERIMENTAL
        bendx_chain_sanitize(&r->bendx);
#endif
        if (info)
        {
            info->name[BEND_STORE_NAME_MAX] = 0;
            info->description[BEND_STORE_DESC_MAX] = 0;
        }
    }
    return ok;
}

static int bs_load_slot(int slot, bend_t *b)
{
    bend_store_recipe_t r;
    if (!bs_read_slot(slot, &r, 0)) return 0;
    *b = r.bend;
    return 1;
}

static int bs_write_slot(int slot, const bend_store_recipe_t *r,
                         const bend_store_info_t *info)
{
    char path[sizeof(BEND_STORE_DIR "/BEND00.BND")];
    unsigned char hdr[BS_HDR];
    bend_store_info_t clean;
    int fd, ok;

    memcpy(hdr, bs_magic, 4);
    hdr[4] = BS_VER & 0xff;
    hdr[5] = (BS_VER >> 8) & 0xff;
    hdr[6] = sizeof(bend_t) & 0xff;
    hdr[7] = (sizeof(bend_t) >> 8) & 0xff;
    bs_info_clean(&clean, info);
    bs_path(slot, path);
    fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0777);
    if (fd < 0) return 0;
    ok = write(fd, hdr, BS_HDR) == BS_HDR
      && write(fd, &r->bend, sizeof(r->bend)) == (int)sizeof(r->bend)
      && write(fd, &clean, sizeof(clean)) == (int)sizeof(clean)
      && write(fd, &r->bendx, sizeof(r->bendx)) == (int)sizeof(r->bendx)
      && write(fd, &r->segs, sizeof(r->segs)) == (int)sizeof(r->segs)
      && write(fd, &r->bend_on, 4) == 4;
    close(fd);
    if (!ok) remove(path);
    return ok;
}

int bend_store_load_at(int i, bend_t *b)
{
    int n = bend_store_count();

    if (i < 0 || i >= n) return 0;
    // The cursor moves even if the file turns out to be unreadable, so walking
    // past a corrupt preset does not strand the cursor behind it.
    bs_cur = i;
    return bs_load_slot(bs_slot[i], b);
}

int bend_store_peek_recipe_at(int i, bend_store_recipe_t *r)
{
    if (!r) return 0;
    if (bs_count < 0) bend_store_rescan();
    if (i < 0 || i >= bs_count) return 0;
    return bs_read_slot(bs_slot[i], r, 0);
}

int bend_store_load_recipe_at(int i, bend_store_recipe_t *r)
{
    if (!bend_store_peek_recipe_at(i, r)) return 0;
    bs_cur = i;
    return 1;
}

int bend_store_step(bend_t *b, int delta)
{
    int n = bend_store_count();

    if (n <= 0) { bs_cur = -1; return 0; }

    if (bs_cur < 0) bs_cur = (delta >= 0) ? 0 : n - 1;
    else
    {
        bs_cur += delta;
        while (bs_cur < 0)  bs_cur += n;
        while (bs_cur >= n) bs_cur -= n;
    }

    return bs_load_slot(bs_slot[bs_cur], b);
}

//-------------------------------------------------------------------

int bend_store_save_recipe(const bend_store_recipe_t *r,
                           const bend_store_info_t *info)
{
    int slot, i, n;

    mkdir_if_not_exist(BEND_STORE_DIR);
    bend_store_rescan();

    // Lowest free number, so deleting a preset in the middle makes its slot
    // available again and the list stays short.
    for (slot = 0; slot < BEND_STORE_SLOTS; slot++)
    {
        int taken = 0;
        for (i = 0; i < bs_count; i++)
            if (bs_slot[i] == (unsigned char)slot) { taken = 1; break; }
        if (!taken) break;
    }
    if (slot >= BEND_STORE_SLOTS || bs_count >= BEND_STORE_MAX) return -1;

    if (!bs_write_slot(slot, r, info))
    {
        // A half-written preset reads as a corrupt one every time it is walked
        // past, which is worse than not having saved it.
        bend_store_rescan();
        return -1;
    }

    n = bend_store_rescan();
    for (i = 0; i < n; i++)
        if (bs_slot[i] == (unsigned char)slot) { bs_cur = i; return i; }
    return -1;
}

int bend_store_save_current(void)
{
    bend_store_recipe_t r;
    memset(&r, 0, sizeof(r));
    r.bend = conf.bitbend;
    r.bendx = conf.bendx;
    r.segs = conf.bend_segs;
    r.bend_on = conf.bitbend_enable;
    r.bendx_on = conf.bendx_enable;
    return bend_store_save_recipe(&r, 0);
}

int bend_store_get_info_at(int i, bend_store_info_t *info)
{
    bend_store_recipe_t r;

    if (!info) return 0;
    if (bs_count < 0) bend_store_rescan();
    if (i < 0 || i >= bs_count) return 0;
    return bs_read_slot(bs_slot[i], &r, info);
}

int bend_store_set_info_at(int i, const bend_store_info_t *info)
{
    bend_store_recipe_t r;
    int slot;
    if (bs_count < 0) bend_store_rescan();
    if (i < 0 || i >= bs_count) return 0;
    slot = bs_slot[i];
    if (!bend_store_peek_recipe_at(i, &r)) return 0;
    return bs_write_slot(slot, &r, info);
}

int bend_store_delete_at(int i)
{
    char path[sizeof(BEND_STORE_DIR "/BEND00.BND")];
    int slot;

    if (bs_count < 0) bend_store_rescan();
    if (i < 0 || i >= bs_count) return 0;

    slot = bs_slot[i];
    bs_path(slot, path);
    remove(path);

    bend_store_rescan();
    // rescan follows the loaded preset to wherever it moved, and clears the
    // cursor if it was the one that went. Nothing further to do here: unlike
    // the cursor-relative delete below, this one is not being called in a run
    // and has no position to keep warm.
    return 1;
}

int bend_store_delete_cur(void)
{
    char path[sizeof(BEND_STORE_DIR "/BEND00.BND")];
    int slot;

    if (bs_count < 0) bend_store_rescan();
    if (bs_cur < 0 || bs_cur >= bs_count) return 0;

    slot = bs_slot[bs_cur];
    bs_path(slot, path);
    remove(path);

    bend_store_rescan();
    // rescan clears the cursor when the preset it pointed at is gone. Land on
    // the one that took its place rather than on nothing, so a run of deletes
    // does not need the cursor re-established between each.
    if (bs_cur < 0 && bs_count > 0)
    {
        bs_cur = 0;
        while (bs_cur < bs_count - 1 && bs_slot[bs_cur] < (unsigned char)slot) bs_cur++;
    }
    return 1;
}
