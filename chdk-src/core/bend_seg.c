//-------------------------------------------------------------------
// Segments, config side - see include/bend_seg.h.
//
// Separate from bend.c because that file has no CHDK dependencies and must
// keep none: the host test compiles it to prove the span geometry, and the
// moment it reads conf it cannot be built off-camera any more.
//-------------------------------------------------------------------

#include "platform.h"
#include "conf.h"
#include "bend_seg.h"

//-------------------------------------------------------------------

bend_t *bend_seg_conf(int seg)
{
    if (seg <= 0 || seg >= BEND_SEG_MAX) return &conf.bitbend;
    return &conf.bend_segs.b[seg - 1];
}

int bend_seg_n(void)
{
    return bend_seg_count(conf.bend_segs.layout);
}

int bend_seg_active(void)
{
    int a = conf.bend_segs.active;
    // Clamped on read as well as on write. The layout can change from the
    // menu, from a preset, or from a picture's sidecar, and not all of those
    // go through bend_seg_set_active() - but every one of them is followed by
    // something asking which matrix to draw.
    return (a >= 0 && a < bend_seg_n()) ? a : 0;
}

void bend_seg_set_active(int seg)
{
    int n = bend_seg_n();
    if (seg < 0)  seg = 0;
    if (seg >= n) seg = n - 1;
    conf.bend_segs.active = (unsigned char)seg;
}

bend_t *bend_seg_cur(void)
{
    return bend_seg_conf(bend_seg_active());
}

int bend_seg_slot(int seg)
{
    int v;
    if (seg < 0 || seg >= BEND_SEG_MAX) return -1;
    v = conf.bend_segs.slot[seg];
    return (v > 0 && v <= 100) ? (v - 1) : -1;
}

void bend_seg_set_slot(int seg, int slot)
{
    if (seg < 0 || seg >= BEND_SEG_MAX) return;
    conf.bend_segs.slot[seg] = (slot >= 0 && slot <= 99)
                             ? (unsigned char)(slot + 1) : 0;
}

// The region being edited no longer holds what it was loaded with. Paired with
// bend_store_detach() at every call site rather than folded into it, because
// that one owns the card's cursor and this one owns four bytes of config, and
// keeping them separate is what lets bend_store.c stay unaware of segments.
void bend_seg_detach(void)
{
    bend_seg_set_slot(bend_seg_active(), -1);
}

void bend_seg_prep(int nbits)
{
    int i, n;

    bend_seg_sanitize(&conf.bend_segs, nbits);
    bend_sanitize(&conf.bitbend, nbits);

    // Only the regions the layout actually has. A matrix in a slot no layout
    // reaches is left exactly as it was, so switching to Quarters, tuning the
    // fourth corner, switching to Off and back finds it still there.
    n = bend_seg_n();
    if (conf.bitbend_simple)
        for (i = 0; i < n; i++) bend_simplify(bend_seg_conf(i));
}

int bend_seg_any(void)
{
    int i, n = bend_seg_n();

    for (i = 0; i < n; i++)
        if (!bend_is_identity(bend_seg_conf(i))) return 1;
    return 0;
}
