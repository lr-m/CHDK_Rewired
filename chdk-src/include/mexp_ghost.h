#ifndef MEXP_GHOST_H
#define MEXP_GHOST_H

// The ghost - the exposures already taken, held over the live view while the
// next one is composed. What a body with a multiple exposure mode shows you,
// and the reason the feature is usable at all: without it the second frame is
// guesswork.
//
// Implementation and its limits are in core/mexp_ghost.c.

// Sample the composite out of the raw buffer into the ghost tile. Must be
// called from inside the raw hook, where get_raw_pixel() is valid.
void mexp_ghost_capture(void);

// Paint the tile over the live view. Called from gui_redraw() with the same
// enforce-redraw flag the grid gets; does nothing unless a sequence is part
// way through and the tile needs repainting.
void mexp_ghost_draw(int force);

// Drop the tile and ask for the screen to be repainted without it. Called when
// a sequence finishes or is cancelled.
void mexp_ghost_clear(void);

// Non-zero when there is a captured tile to paint. The counter in the OSD
// marks a sequence running without one, which is the difference between a
// ghost that was never captured - no heap for the tile, most likely - and one
// that is captured and then painted somewhere it cannot be seen. Those two
// look identical on the camera and are fixed in different files.
int mexp_ghost_have(void);

#endif
