#ifndef MEXP_H
#define MEXP_H

// Multiple exposure - several shots combined into one frame, the way the
// high-end bodies do it: additive, lighten, darken, average.
//
// The combining happens in the raw buffer inside the raw hook, before Canon
// develops the JPEG from it, so the composite lands in the JPEG and in the DNG
// alike - the same place and for the same reason the bend engines run there.
//
// This file is the maths only: how two pixels combine, how a packed sensor row
// unpacks and repacks, and how a linear sensor value becomes one of the five
// grey levels the ghost overlay can draw. None of it touches a camera address
// or a CHDK header, so tools/mexp_selftest.c can run every line of it on the
// host. The applier - the card file, the row loop, the state machine - is in
// core/raw.c beside the bend appliers, and the overlay is in core/mexp_ghost.c.
//
// See docs/MULTI_EXPOSURE.md.

//-------------------------------------------------------------------
// Blend modes.
//
// Named for what they do to the light rather than for the arithmetic, because
// that is how the cameras that have this feature name them, and the point of
// the feature is to be the thing those cameras have.

#define MEXP_ADD        0       // sum the exposures - highlights build and clip
#define MEXP_LIGHTEN    1       // keep the brighter pixel - lights on black
#define MEXP_DARKEN     2       // keep the darker pixel - the inverse
#define MEXP_AVERAGE    3       // mean of the exposures - the classic ghost

#define MEXP_MODE_COUNT 4

// How many exposures a sequence may hold. Two is the double exposure this was
// asked for; the ceiling is where the average mode's accumulator stops being
// exact rather than any limit of the storage (see mexp_blend_px).
#define MEXP_FRAMES_MIN 2
#define MEXP_FRAMES_MAX 9

// Grey levels the ghost overlay quantises to. Five, because that is how many
// neutral entries the fixed Canon palette gives us on these bodies:
// black, dark grey, grey, light grey, white.
#define MEXP_GHOST_LEVELS 5

//-------------------------------------------------------------------
// Combine one pixel.
//
//   acc     the accumulator's value - what the first n exposures came to
//   v       the pixel just captured
//   n       how many exposures are already in acc (>= 1)
//   black   sensor black level, white the saturation point
//
// Returns the new accumulator value, always inside [0, white].
unsigned short mexp_blend_px(int mode, unsigned int acc, unsigned int v,
                             unsigned int n, unsigned int black, unsigned int white);

// The same over a whole row, acc[] updated in place.
void mexp_row_blend(int mode, unsigned short *acc, const unsigned short *v,
                    unsigned int rowpix, unsigned int n,
                    unsigned int black, unsigned int white);

//-------------------------------------------------------------------
// Packed sensor rows.
//
// Both directions for 10, 12 and 14 bits per pixel. The layouts are the ones
// get_raw_pixel() and set_raw_pixel() use in core/raw.c, lifted group at a
// time rather than pixel at a time for the same reason bb_row_fast() exists
// there: this runs over every pixel of every exposure, which is 10.3 million
// of them per shot on the a480.
//
// rowpix is rounded down to a whole group; a sensor row is a whole number of
// groups on every body here.
void mexp_row_unpack(int bits, const unsigned char *src, unsigned short *dst, unsigned int rowpix);
void mexp_row_pack(int bits, const unsigned short *src, unsigned char *dst, unsigned int rowpix);

//-------------------------------------------------------------------
// Ghost overlay tone mapping.
//
// Sensor values are linear and the eye is not, so a straight scaling of the
// range onto five levels puts almost the whole picture in the bottom two. This
// applies a gamma first, which is what makes the ghost look like the picture
// rather than like a silhouette.
int mexp_ghost_level(unsigned int v, unsigned int black, unsigned int white);

//-------------------------------------------------------------------
// Sequence state. Held here rather than in the applier so the menu and the
// overlay can read it without either of them including the other.

extern int mexp_count;      // exposures captured in the current sequence
extern int mexp_target;     // how many the sequence is going to take
extern int mexp_mode_used;  // the mode it started in - see mexp_begin()
extern int mexp_bend_each_used; // bend each exposure, or only the composite

// Start a sequence of `frames` exposures in `mode`. Clamps both.
void mexp_begin(int mode, int frames, int bend_each);

// Forget the sequence. Does not touch the card - the applier owns the file.
void mexp_reset(void);

// Non-zero while a sequence is part way through: at least one exposure in and
// the last one not yet taken. This is exactly the window in which the ghost is
// worth drawing, and in which cancelling means something.
int mexp_in_progress(void);

#endif
