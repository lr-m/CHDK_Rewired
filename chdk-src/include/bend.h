#ifndef BEND_H
#define BEND_H

//-------------------------------------------------------------------
// Bend engine - see BENDING_DESIGN.md
//
// A bend is a routing matrix: for every output bit, name where its value comes
// from. That is what a patchbay is, and it covers the whole family of "swap
// these two lines / tie that one low / feed this one from the clock" bends in
// a single model, instead of one hardcoded mode per idea.
//
// The model is deliberately free of CHDK types so that tools/bend_selftest.c
// can compile the engine on the host and check the fast paths against a
// reference implementation.
//-------------------------------------------------------------------

#define BEND_MAX_BITS       16      // room for 12 and 14 bit sensors

//-------------------------------------------------------------------
// Source encoding, one byte per output bit.
//
//   0x00 | n    data pin n
//   0x10 | n    data pin n, inverted
//   0x20        tie low   - line shorted to ground
//   0x21        tie high  - line shorted to the rail
//   0x30 | k    non-data bus k
//   0x40 | k    non-data bus k, inverted
//
// Inversion is a flag rather than a source of its own because on hardware any
// of these can be read on the wrong polarity or pulled through an inverter, so
// it doubles the palette for one bit of storage.

#define BSRC_DATA(n)        (0x00 | (n))
#define BSRC_NDATA(n)       (0x10 | (n))
#define BSRC_LOW            0x20
#define BSRC_HIGH           0x21
#define BSRC_BUS(k)         (0x30 | (k))
#define BSRC_NBUS(k)        (0x40 | (k))

#define BSRC_CLASS(s)       ((s) & 0xf0)
#define BSRC_IDX(s)         ((s) & 0x0f)

#define BSRC_C_DATA         0x00
#define BSRC_C_NDATA        0x10
#define BSRC_C_TIE          0x20
#define BSRC_C_BUS          0x30
#define BSRC_C_NBUS         0x40

//-------------------------------------------------------------------
// Non-data buses. Every one of these is a signal you could clip a lead onto in
// hardware, and every one has an exact equivalent at the point we intercept,
// because the raw buffer still carries the scan geometry.

#define BUS_HCLK        0   // pixel clock            x & 1
#define BUS_HCLKN       1   // divided pixel clock    (x >> hdiv) & 1
#define BUS_VCLK        2   // line clock             y & 1
#define BUS_VCLKN       3   // divided line clock     (y >> vdiv) & 1
#define BUS_FRAME       4   // VD / field select      flips per frame
#define BUS_DIAG        5   // two clocks shorted     (x ^ y) & 1
#define BUS_OB          6   // optical black clamp    masked column, same row
#define BUS_NOISE       7   // floating line          LFSR
#define BUS_HOLD        8   // sample and hold cap    previous pixel
#define BUS_VHOLD       9   // line memory            pixel one row up
#define BUS_PARITY      10  // wire-OR of the bus     parity of the input word
#define BUS_TAP         11  // lead clipped elsewhere pixel at (x+dx, y+dy)
#define BUS_COUNT       12

//-------------------------------------------------------------------
// Trash generator types, feeding BUS_NOISE.

#define TRASH_LFSR      0   // clocked every pixel        - white noise
#define TRASH_LFSRN     1   // clocked every 2^rate       - blocky noise
#define TRASH_RAMP      2   // free running counter bit   - sawtooth
#define TRASH_BURST     3   // LFSR gated by a slow count - dropouts
#define TRASH_SEED      4   // reseeded per row from OB   - tracks the sensor
#define TRASH_COUNT     5

//-------------------------------------------------------------------
// A complete bend. POD and a multiple of 4 bytes, so it can be stored with
// CONF_INT_PTR and written to a preset file as-is.

typedef struct
{
    unsigned char route[BEND_MAX_BITS]; // source per output bit
    unsigned char nbits;                // bit depth this was authored for
    unsigned char bayer;                // scope: 0 = all, 1..4 = Bayer position
    unsigned char row_period;           // scope: every Nth row, 0/1 = all
    unsigned char hdiv;                 // BUS_HCLKN shift
    unsigned char vdiv;                 // BUS_VCLKN shift
    unsigned char trash_type;
    unsigned char trash_rate;
    unsigned char depth;                // edges used by the random generator
    signed   char tap_dx;
    signed   char tap_dy;
    unsigned char pad[2];
    unsigned int  seed;                 // makes a saved bend reproducible
} bend_t;

//-------------------------------------------------------------------
// Segments.
//
// One bend is one way of wiring the bus, and the whole frame comes off the
// same bus - so a single matrix over the whole picture is the honest default.
// But the frame is not read out all at once either, and a routing that changes
// with where the scan has got to is exactly what a bend that drifts with
// temperature or supply does. That is the licence for this: the array split
// into regions, each with its own matrix, applied to its own pixels.
//
// Only the matrix is segmented. The experimental profiles in bendx.h are not,
// deliberately: they model the frame buffer downstream of the bus, and that is
// one buffer with one fault. They run after all the segments, over everything.
//
// The geometry lives here rather than in the applier so that the host test can
// prove the one property that matters - that the spans of a row tile it
// exactly, with no gap and no overlap and nothing past the end - which is the
// difference between a segmented bend and a write outside the frame buffer.

#define BSEG_OFF        0   // one region: the whole frame
#define BSEG_HALF_H     1   // top, bottom
#define BSEG_HALF_V     2   // left, right
#define BSEG_QUAD       3   // four corners
#define BSEG_CIRCLE     4   // a disc in the middle, and everything else
#define BSEG_RINGS      5   // disc, annulus, outside
#define BSEG_DIAG       6   // split corner to corner
#define BSEG_WEDGE      7   // both diagonals - four triangles about the centre
#define BSEG_BANDS      8   // horizontal bands, cycling down the frame
#define BSEG_BARS       9   // vertical bars, cycling across it
#define BSEG_CHECKER    10  // squares, cycling both ways
// The pattern layouts. All six are two-region - black is one bend, white is
// the other - because that is what a shader of this kind actually produces: a
// figure and a ground. Cycling four regions through them turned every edge into
// a different pair of bends and read as noise rather than as the pattern.
//
// They are also the only layouts that move between shots. See bend_seg_set_seed().
#define BSEG_WARP       11  // noise-warped stripe field
#define BSEG_ZIGZAG     12  // mirrored tiles, zigzag fill
#define BSEG_TRUCHET    13  // rotated half-tiles
#define BSEG_LINEWAVE   14  // wave-displaced lines
#define BSEG_CHECKS     15  // rotated boxes on a grid
#define BSEG_MIRROR     16  // mirrored tiles, sine fill
#define BSEG_COUNT      17

#define BSEG_IS_PATTERN(l)  ((l) >= BSEG_WARP && (l) < BSEG_COUNT)

// Four, matching the chain in bendx.h and for the same reason: it is as many
// as the picker can mark and as many as anyone can hold in their head while
// tuning them one at a time.
#define BEND_SEG_MAX    4

// Most spans any layout puts in one row.
//
// This was 20, set by the repeating layouts: their cell is held at or above an
// eighteenth of the row, so a row could not need more. BSEG_WARP broke that
// assumption - it is a stripe field with an arbitrary rotation, so a row cut
// across the stripes rather than along them needs one span per stripe, and the
// stripe count is a knob.
//
// Measured rather than reasoned, and re-measured every time the pattern set
// grew. A host sweep over all six pattern layouts, every knob setting from 1 to
// 100, forty seeds and three frame geometries reports a worst row of 119 -
// BSEG_WARP at size 58 on the smallest frame, which is the UI preview rather
// than a sensor. 128 would have left seven percent in hand, which is not a
// budget; 192 leaves a third.
//
// Small frames are the worst case, not the sensor: the sampling step clamps to
// one pixel there, so every transition in a pattern finer than the frame gets
// its own span.
//
// Overrunning is safe rather than fatal in any case - bseg_emit() extends the
// last span instead of writing past the array - but a row that overruns loses
// the right-hand end of its pattern, so the budget is set to make it not
// happen rather than to survive it.
//
// Cost is 3 bytes a span at each of the two stack sites that hold a row
// (core/raw.c and bend_seg_at()), so this is 576 bytes rather than 60.
#define BEND_SEG_SPANS  192

// Smallest cell a repeating layout may use, as a fraction of the row. This used
// to be the same decision as BEND_SEG_SPANS written twice; it is now just the
// floor that keeps Bars and Checkerboard from offering a stripe too narrow to
// apply, and the two numbers move independently.
#define BEND_SEG_MINCELL 18

//-------------------------------------------------------------------
// The segment set.
//
// Segment 0's matrix is not in here - it is the camera's one bend, the one
// every other part of this feature already edits, saves and loads. Keeping it
// where it was is what makes segments an arrangement of bends rather than a
// second kind of bend: a preset file is still one matrix, the pin strip still
// edits one matrix, and switching the layout off leaves exactly the camera
// that was there before.

typedef struct
{
    unsigned char layout;               // BSEG_*
    unsigned char active;               // segment the UI is editing, 0..n-1
    unsigned char size;                 // radius, percent of the short side
    unsigned char pad;
    // Which preset each region was loaded from, as the preset number plus one,
    // so that zero means none. Plus one rather than a 0xff sentinel because a
    // zeroed config block is the ordinary way this arrives and BEND00 is a real
    // file - a sentinel that is not what zero-fill produces would have every
    // region claiming to hold preset zero on the first boot after an upgrade.
    //
    // Recorded per region because the saved list has one cursor for the whole
    // camera and four regions to describe, and "which bend is in the bottom
    // left" is the question the region list exists to answer. Cleared the
    // moment the matrix is edited, the same way the single cursor detaches.
    unsigned char slot[BEND_SEG_MAX];
    bend_t        b[BEND_SEG_MAX - 1];  // segments 1..3; 0 is the camera's own
} bend_segs_t;

// Regions in this layout. Always >= 1, so a caller can loop over it blind.
int  bend_seg_count(int layout);

// Names for the pickers. bend_seg_name() is the layout, at most 16 characters;
// bend_seg_label() is one region of it, at most 12.
const char *bend_seg_name(int layout);
const char *bend_seg_label(int layout, int seg);

// Whether the size knob means anything here - the round layouts and the
// repeating ones have a radius or a cell, the rest are cut by the frame's own
// geometry and have nothing to turn.
int  bend_seg_has_size(int layout);

// Whether this layout cuts rows across. The ones that do lose the packed
// whole-row fast path on every row they cut, which is the difference between
// a shot that saves and one you wait out - so the UI warns, the same way it
// warns about a slow experimental profile.
int  bend_seg_cuts_rows(int layout);

// Clamp every field into range, the same gate bend_sanitize() is - this can
// arrive from a zeroed config block, and layout indexes a jump table. A zero
// size means "never set" and becomes the default rather than a disc of no
// radius, which would make the circle layouts look broken out of the box.
void bend_seg_sanitize(bend_segs_t *s, int nbits);

// Cut one row into spans. Writes up to BEND_SEG_SPANS entries: seg[i] is the
// region and xend[i] is where it ends, so span i covers [i ? xend[i-1] : 0,
// xend[i]). The spans tile [0, w) exactly and xend is strictly increasing.
// Returns how many there are, always >= 1.
int bend_seg_spans(int layout, int size, int y, int w, int h,
                   unsigned char *seg, unsigned short *xend);

// The region one pixel is in. Written in terms of bend_seg_spans() so there is
// only one geometry, and not used per pixel by anything - the appliers walk
// the spans, which is the whole point of having them.
int bend_seg_at(int layout, int size, int x, int y, int w, int h);

// Re-roll the pattern layouts. Changes where the pattern sits, which way round
// its two regions are, and for BSEG_WARP which noise field it is warped by -
// so the same layout at the same knob setting gives a different picture every
// shot, which is the difference between a pattern and a watermark.
//
// The geometric layouts ignore it: a circle that moved would just be a circle
// somebody had mis-set.
//
// Called once per capture from raw_bitbend(). Left alone, the default seed is
// fixed, which is what lets the host test assert on exact geometry.
void bend_seg_set_seed(unsigned s);

//-------------------------------------------------------------------
// Per-pixel inputs the engine cannot obtain for itself. The applier owns the
// buffer, so it samples these and hands them in; that keeps the engine pure
// and testable on the host.

typedef struct
{
    unsigned prev;      // previous pixel value      - BUS_HOLD
    unsigned above;     // pixel one row up          - BUS_VHOLD
    unsigned ob;        // optical black, this row   - BUS_OB
    unsigned tap;       // pixel at the tap offset   - BUS_TAP
    unsigned noise;     // current LFSR output       - BUS_NOISE
    unsigned frame;     // frame counter             - BUS_FRAME
} bend_src_t;

//-------------------------------------------------------------------
// Compiled form. Built once per shot by bend_compile(), then read per pixel.

typedef struct
{
    unsigned short *lut;                // [1 << nbits], the pure part
    unsigned    dyn_mask;               // output bits fed by a bus
    unsigned    bus_inv;                // of those, which are inverted
    unsigned    bus_bits[BUS_COUNT];    // output bits fed by each bus
    unsigned    bus_used;               // bitmap of buses in use
    unsigned    mask;                   // (1 << nbits) - 1
    int         nbits;
    int         hdiv, vdiv;
    int         is_pure;                // dyn_mask == 0: the fast path applies
    int         is_identity;            // no change at all: skip entirely
} bend_cc_t;

//-------------------------------------------------------------------

void bend_reset(bend_t *b, int nbits);
int  bend_is_identity(const bend_t *b);

// Clamp every field into range, and reset outright if the matrix is not usable
// at this bit depth. Called before compiling, because a bend can arrive from a
// zeroed config block, from a preset file written by another camera, or from a
// preset file someone edited by hand.
void bend_sanitize(bend_t *b, int nbits);

// Compile b into cc, writing the value table into lut (caller owns it, and it
// must hold 1 << nbits entries). nbits may differ from b->nbits: the matrix is
// remapped by distance from the top, so bend authored at 10 bits behaves the
// same way when applied to the 8 bit live view.
void bend_compile(const bend_t *b, bend_cc_t *cc, unsigned short *lut, int nbits);

// Reference evaluation. Correct but slow - the appliers use the compiled form
// directly. tools/bend_selftest.c checks the fast paths against this.
unsigned bend_eval(const bend_cc_t *cc, unsigned v, int x, int y, const bend_src_t *s);

// Value of every bus-fed output bit, before inversion. Exposed so an applier
// can hoist it out of an inner loop when it knows the buses in use are all
// constant over the span it is walking.
unsigned bend_busval(const bend_cc_t *cc, unsigned v, int x, int y, const bend_src_t *s);

// Trash generator. Advance once per pixel; returns the current noise word.
unsigned bend_trash_step(const bend_t *b, unsigned *state, unsigned pos);
unsigned bend_trash_seed(const bend_t *b, unsigned row, unsigned ob);

// Preset generators - fill a matrix from the older single-idea modes, so
// nothing that worked before is lost and the menu still has a way in.
void bend_preset_swap(bend_t *b, int a, int c);
void bend_preset_rotate(bend_t *b, int k);
void bend_preset_reverse(bend_t *b);
void bend_preset_xor(bend_t *b, unsigned mask);

// Read a source on the wrong polarity. Every class has a counterpart, so this
// is total - and it is not a bit flip, see the comment on the definition.
unsigned char bend_src_invert(unsigned char src);
void bend_preset_tie(bend_t *b, int a, int high);

// Constrained random. depth is how many edges to touch - the number of clip
// leads you have. Returns the seed actually used.
//
// allow_bus 0 spends the whole budget on the cheap edges, so a reroll cannot
// land on a bend that will not compile to a pure lookup. Filtering the result
// afterwards would not do: an edge that gets rewritten back to a straight line
// is a wasted lead, and at depth 3 losing one of them is the difference
// between a bend and nothing.
unsigned bend_random(bend_t *b, unsigned seed, int depth, int allow_bus);
void     bend_mutate(bend_t *b, unsigned seed, int allow_bus);

// Restrict the matrix to the sources that compile into a pure lookup table -
// data pins, inverted data pins and the two rails. Every pin patched to a
// non-data bus goes back onto its own line, keeping the inversion if the bus
// was inverted. Returns the number of pins changed.
//
// The buses are what makes a bend expensive. One bus-fed output bit sets
// dyn_mask, which drops both appliers off their fast paths onto a loop that
// unpacks, evaluates and repacks every pixel individually - and on the capture
// path that runs over the whole sensor, with a tap or a line-memory source
// re-reading the buffer per pixel on top. On these bodies that is the
// difference between a shot that saves and a shot you wait out.
int bend_simplify(bend_t *b);

// Display helper: writes a short label for a source into buf (>= 6 bytes).
void bend_src_name(unsigned char src, char *buf);

extern const char * const bend_bus_names[BUS_COUNT];
extern const char * const bend_trash_names[TRASH_COUNT];

#endif
