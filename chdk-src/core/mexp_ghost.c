// Multiple exposure - the ghost overlay.
//
// Between the exposures of a sequence, what has been captured so far is held
// over the live view so the next one can be lined up against it. See
// include/mexp_ghost.h.
//
// Three things decide the shape of this file, and all three are the screen:
//
//   the bitmap plane is 8 bits of palette index, and on these bodies the
//   palette is Canon's - CAM_LOAD_CUSTOM_COLORS is not set on any of the seven
//   ported cameras, so we get the entries Canon put there and no others. The
//   neutral ones are five: black, dark grey, grey, light grey, white. That is
//   why the ghost is a five-level monochrome image and not a colour one, even
//   though the Bayer pattern in the raw buffer would give us colour for free.
//
//   nothing in that palette is half transparent, so a 50% mix has to be made
//   out of coverage instead of opacity: every other pixel, in a checkerboard.
//   Half the ghost's pixels are drawn and the live view shows through the
//   other half, which is the same 50% the high-end bodies blend, arrived at
//   the only way an indexed plane can arrive at it.
//
//   the plane is not redrawn for us. The tile is painted once and stays until
//   something erases it, which is exactly how the persistent OSD works on
//   these cameras and why it can be this cheap - 21,600 draw_pixel calls when
//   it is painted, and none at all on the redraws in between.
//
// The tile is stored at half screen resolution, one byte a pixel, because a
// checkerboard at full resolution would carry two levels per 2x2 block that
// nothing could tell apart at this size - and 21KB of it is already more than
// this project usually asks the heap for.

#include "platform.h"
#include "camera_info.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "mexp.h"
#include "mexp_ghost.h"
#include "raw.h"

//-------------------------------------------------------------------

static unsigned char *tile = NULL;  // gw*gh, one mexp_ghost_level() per byte
static int gw, gh;                  // tile size - half the bitmap screen
static int tile_dirty = 0;          // tile holds something not yet painted
static int painted = 0;             // ...and something is on screen now

// The sensor area worth sampling. The masked border carries no picture, and on
// a body that defines its active area the ghost should be the picture, framed
// the way the JPEG will be.
#ifdef CAM_ACTIVE_AREA_X1
  #define GH_X1     CAM_ACTIVE_AREA_X1
  #define GH_Y1     CAM_ACTIVE_AREA_Y1
  #define GH_X2     CAM_ACTIVE_AREA_X2
  #define GH_Y2     CAM_ACTIVE_AREA_Y2
#else
  #define GH_X1     0
  #define GH_Y1     0
  #define GH_X2     ((int)camera_sensor.raw_rowpix)
  #define GH_Y2     ((int)camera_sensor.raw_rows)
#endif

//-------------------------------------------------------------------

static void ghost_free(void)
{
    if (tile)
    {
        free(tile);
        tile = NULL;
    }
    tile_dirty = 0;
}

//-------------------------------------------------------------------
// Sample the raw buffer down into the tile.
//
// One 2x2 Bayer block per tile pixel, all four photosites averaged. That is
// two greens, a red and a blue, which is a green-weighted luminance - near
// enough to what the eye is going to be lining the second exposure up against,
// and it costs no knowledge of which CFA phase this sensor starts on. Sampling
// a single green instead would halve the reads and double the noise; at this
// size the noise is what would show.

void mexp_ghost_capture(void)
{
    int tx, ty;
    int x1 = GH_X1, y1 = GH_Y1;
    int aw = GH_X2 - GH_X1, ah = GH_Y2 - GH_Y1;
    unsigned black = camera_sensor.black_level;
    unsigned white = camera_sensor.white_level;

    // Turned off part way through a sequence: take what is already on screen
    // down with it, rather than freezing the last ghost over the live view.
    if (!conf.mexp_ghost) { mexp_ghost_clear(); return; }

    gw = camera_screen.width / 2;
    gh = camera_screen.height / 2;

    if (gw <= 0 || gh <= 0 || aw <= 1 || ah <= 1) return;

    if (!tile)
    {
        tile = malloc(gw * gh);
        if (!tile) return;      // no ghost, but the exposures still combine
    }

    for (ty = 0; ty < gh; ty++)
    {
        // & ~1 so the pair of rows sampled is always an even/odd pair, which
        // keeps every block one whole Bayer quad rather than half of each of
        // two. Without it the ghost picks up a green/magenta shimmer from
        // blocks that straddle the pattern.
        unsigned int ry = (unsigned int)(y1 + (ty * ah) / gh) & ~1u;
        unsigned char *row = tile + ty * gw;

        for (tx = 0; tx < gw; tx++)
        {
            unsigned int rx = (unsigned int)(x1 + (tx * aw) / gw) & ~1u;
            unsigned int v = (unsigned int)get_raw_pixel(rx,   ry)
                           + (unsigned int)get_raw_pixel(rx+1, ry)
                           + (unsigned int)get_raw_pixel(rx,   ry+1)
                           + (unsigned int)get_raw_pixel(rx+1, ry+1);

            row[tx] = (unsigned char)mexp_ghost_level(v >> 2, black, white);
        }
    }

    tile_dirty = 1;
}

//-------------------------------------------------------------------
// Paint it.

// Painted in slices, and the reason is the same one the persistent OSD gives
// for repairing its text every 200ms: Canon paints over this buffer. A tile
// drawn once and left alone is eaten a piece at a time, which is what the
// first version of this did - captured, allocated, painted, gone.
//
// So it is repainted continuously, a slice per repair tick, cycling. A slice
// rather than the whole tile because the whole tile is 21,600 draw_pixel calls
// and the OSD's own note about the a480 tearing is about repainting far less
// than that far less often. One slice is a few thousand, in the same order as
// a plate row, and the whole ghost comes round every MEXP_GHOST_SLICES ticks.
//
// The exception is a forced pass - a screen restore, or a tile just captured.
// There the whole thing goes down at once, because a ghost that fades in over
// the best part of a second after each exposure would read as a fault.
#define MEXP_GHOST_SLICES 4

static int slice = 0;

void mexp_ghost_draw(int force)
{
    int tx, ty, y0, y1;
    color grey[MEXP_GHOST_LEVELS];

    if (!tile || !mexp_in_progress()) return;

    if (force || tile_dirty)
    {
        y0 = 0;
        y1 = gh;
        slice = 0;
    }
    else
    {
        int h = (gh + MEXP_GHOST_SLICES - 1) / MEXP_GHOST_SLICES;
        y0 = slice * h;
        y1 = y0 + h;
        if (y1 > gh) y1 = gh;
        slice = (slice + 1) % MEXP_GHOST_SLICES;
        if (y0 >= gh) return;
    }

    // Resolved here rather than held in a table: chdk_colors is swapped
    // between the record and playback palettes by set_palette(), so a Canon
    // index cached at capture time is not the same colour by the time this
    // runs.
    grey[0] = COLOR_BLACK;
    grey[1] = COLOR_GREY_DK;
    grey[2] = COLOR_GREY;
    grey[3] = COLOR_GREY_LT;
    grey[4] = COLOR_WHITE;

    for (ty = y0; ty < y1; ty++)
    {
        const unsigned char *row = tile + ty * gw;
        int sy = ty * 2;
        // The checkerboard phase alternates by tile row so the drawn pixels
        // never line up into vertical stripes.
        int phase = ty & 1;

        for (tx = 0; tx < gw; tx++)
        {
            color cl = grey[row[tx]];
            int sx = tx * 2;

            draw_pixel(sx + phase,     sy,     cl);
            draw_pixel(sx + 1 - phase, sy + 1, cl);
        }
    }

    tile_dirty = 0;
    painted = 1;
}

//-------------------------------------------------------------------

int mexp_ghost_have(void)
{
    return tile != NULL;
}

//-------------------------------------------------------------------

void mexp_ghost_clear(void)
{
    ghost_free();

    // Erase by repaint rather than by drawing transparent over 21,600 pixels:
    // the plane holds the OSD and the grid as well, and half of a checkerboard
    // erased pixel by pixel would take out whatever was drawn under it. This
    // is the same route the overlay itself uses when it goes inactive.
    if (painted)
    {
        painted = 0;
        gui_set_need_restore();
    }
}
