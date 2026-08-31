#ifndef BENDX_H
#define BENDX_H

//-------------------------------------------------------------------
// Experimental profiles - see docs/EXPERIMENTAL_EFFECTS.md
//
// The bend engine in bend.h models one thing: the ADC data bus, rewired one
// output bit at a time. That is the right model for the wires between the
// sensor and the processor, and it is the wrong model for everything that
// happens after them - because once the pixels are in the frame buffer they
// are not a bus any more, they are memory, and memory fails differently.
//
// This is the second engine. A profile is not a routing matrix; it is one
// named failure of the path between the ADC and the JPEG encoder - a stuck
// address line, a mis-programmed DMA stride, a lost pixel clock, a refresh
// that did not happen, a foreign buffer shorted onto the data lanes. One at a
// time, because that is how a fault presents on a bench.
//
// The two compose. bend.h runs first and rewires the bus; this runs second and
// breaks the memory the bus wrote into. That is also the physical order.
//
// Deliberately free of CHDK types, for the same reason bend.c is: tools/
// bendx_selftest.c compiles it on the host and checks the packed accessors and
// the bounds of every effect against a reference, because every one of these
// writes into a fifteen megabyte buffer and an off-by-one is a reflash to find
// out about.
//-------------------------------------------------------------------

//-------------------------------------------------------------------
// Effects.
//
// Grouped by which part of the path they break, which is also the order they
// appear in the picker. The numbers are stored in the config block, so they
// are append-only - a new effect goes at the end of its group only if that
// does not renumber the ones after it, and otherwise at the end of the list.

#define BX_OFF          0

// Memory and bus. These work on the packed bytes as they lie in the frame
// buffer, so they are fast - one pass, no unpacking - and they scramble rather
// than shift, because the packing is not linear. See the note on BX_SLIP.
#define BX_ENDIAN       1   // byte/nibble/bit order within the packed group
#define BX_SLIP         2   // the whole row rotated by a number of bits
#define BX_TEAR         3   // that rotation growing down the frame
#define BX_ADDR         4   // one row address line stuck
#define BX_COLADDR      5   // one column address line stuck
#define BX_STRIDE       6   // DMA stride mis-programmed
#define BX_REFRESH      7   // DRAM refresh missed, bits decaying
#define BX_ECHO         8   // delay line - the bus reflected back onto itself
#define BX_LINEMEM      9   // line memory stuck, holding the last row
#define BX_FIELD        10  // VD phase inverted, fields in the wrong order

// Foreign buses. Some other buffer in the camera shorted onto the frame
// buffer's data lanes. Read-only on the far side, always.
#define BX_ROMBUS       11  // the program ROM

// Pixel domain. These have to unpack, so they cost what the bussed path in
// raw.c costs. bendx_is_slow() says so and the UI marks them.
#define BX_CLOCK        12  // pixel clock running at the wrong rate
#define BX_CFA          13  // demosaic reading the wrong Bayer phase
#define BX_CLAMP        14  // optical black clamp loop unlocked

// Sensor physics. Not a fault of the memory or the timing but of the CCD
// itself.
#define BX_BLOOM        15  // saturation overflowing the vertical register

// Process. This is not a failure of anything - see the note in
// docs/EXPERIMENTAL_EFFECTS.md. They are here because they look good, and
// saying so is better than inventing hardware that would do them.
#define BX_SORT         16  // threshold runs sorted along the row

// Late additions to the earlier families.
#define BX_LOCK         17  // PLL losing lock and re-acquiring, per row
#define BX_JPGBUS       18  // the JPEG encoder's buffer on the data lanes

#define BX_COUNT        19

//-------------------------------------------------------------------
// How the effect's output is put back. For the foreign buses this is the
// difference between "the ROM replaced the picture" and "the ROM is faintly
// visible in the low bits of it", which is most of the range of the effect.

#define BXMIX_SET       0
#define BXMIX_XOR       1
#define BXMIX_OR        2
#define BXMIX_AND       3
#define BXMIX_COUNT     4

//-------------------------------------------------------------------
// A profile. POD, 12 bytes, so it stores with CONF_INT_PTR exactly as bend_t
// does.

typedef struct
{
    unsigned char kind;         // BX_*
    unsigned char amount;       // the main knob, 0 .. bendx_amount_max()
    unsigned char reach;        // the second knob, meaning per effect
    unsigned char lanes;        // data bus lane mask, 0 = all eight
    unsigned char mix;          // BXMIX_*
    unsigned char rowmod;       // effect on for bands of N rows, 0/1 = always
    unsigned char pad[2];
    unsigned      seed;
} bendx_t;

//-------------------------------------------------------------------
// The buffer being bent. Supplied by the caller because the engine has no way
// to ask - and because the host test drives it over a synthetic one.

typedef struct
{
    unsigned char *base;        // packed sensor data, rows * rowlen bytes
    unsigned       rowlen;      // bytes per row. No padding - confirmed
                                // against the ROM's own CRAW_BUFF_SIZE, see
                                // docs/EXPERIMENTAL_EFFECTS.md
    unsigned       rowpix;      // pixels per row
    unsigned       rows;
    unsigned       ob_x;        // a masked column, for the clamp effect
    int            nbits;       // 10 or 12
} bendx_buf_t;

//-------------------------------------------------------------------
// Everything the engine cannot obtain for itself: the foreign buffers, and the
// two sensor constants the clamp effect needs. Any of the pointers may be
// null, and an effect that needs a null one does nothing rather than guessing.
//
// The lengths are the contract. The engine never reads outside base[0..len),
// so a caller that gets a length wrong is the only way a foreign read can go
// out of bounds - which is why raw.c derives all three from accessors the port
// already has rather than from constants written here.

typedef struct
{
    const unsigned char *rom;   unsigned rom_len;
    const unsigned char *jpg;   unsigned jpg_len;
    unsigned frame;             // shot counter, for the effects that drift
    unsigned black;             // camera_sensor.black_level
    unsigned white;             // camera_sensor.white_level
} bendx_env_t;

//-------------------------------------------------------------------
// A chain of them.
//
// One profile is one fault. A camera with two faults is not exotic, and the
// interesting images are mostly the ones where a clean geometric failure has
// something dirty happening underneath it - a stuck address line under a
// refresh that is decaying, say. So profiles stack, and they are applied in
// order, each one working on what the last one left.
//
// The layout is one list, not a list plus a variable:
//
//   item[0 .. n-1]   locked. Frozen, applied in order, knobs no longer move.
//   item[n]          live. The one the rocker is turning, applied last.
//
// so "lock it and start another" is n++, and the live slot is wherever the
// locked ones stopped. When n reaches BENDX_CHAIN_MAX there is no live slot -
// every slot is spoken for - and the UI says so rather than silently dropping
// the next selection.
//
// Four, because the cost is additive and three slow effects in a row is
// already a shot you wait out. It is also as many markers as fit in the
// picker's left column.

#define BENDX_CHAIN_MAX     4

typedef struct
{
    bendx_t       item[BENDX_CHAIN_MAX];
    unsigned char n;            // locked count; item[n] is the live one
    unsigned char pad[3];
} bendx_chain_t;

void bendx_chain_reset(bendx_chain_t *c);
void bendx_chain_sanitize(bendx_chain_t *c);

// Profiles that will actually be applied - the locked ones plus the live one
// if it is not Off. This is what the UI counts and what raw.c walks.
int  bendx_chain_count(const bendx_chain_t *c);

// The slot the rocker turns, or null when every slot is locked.
bendx_t       *bendx_chain_live(bendx_chain_t *c);
const bendx_t *bendx_chain_live_const(const bendx_chain_t *c);

// Position of the first entry using this effect, 0-based over the whole list
// including the live slot, or -1. The picker marks rows with it.
int  bendx_chain_find(const bendx_chain_t *c, int kind);

// Freeze the live slot and open the next one. Returns 0 if there was nothing
// live to lock, or if the chain is full.
int  bendx_chain_lock(bendx_chain_t *c);

// Drop entry i, closing the gap. The live slot moves down with everything
// else, so removing a locked profile does not disturb the one being tuned.
int  bendx_chain_remove(bendx_chain_t *c, int i);

// Whether anything in the chain has to unpack the buffer. One is slow; three
// is slow three times over, which is the thing worth warning about.
int  bendx_chain_slow(const bendx_chain_t *c);

//-------------------------------------------------------------------
// Scratch.
//
// bendx_apply() will not allocate - it is called from the capture path with
// the buffer already committed - so the caller provides working space and the
// engine says how much it needs.
//
// Most effects want two rows. The sort wants a 4096 entry histogram, which at
// 10bpp is larger than two rows are, so a fixed row count cannot express it
// and the caller has to ask. Getting this wrong is not a wrong picture, it is
// a write past the end of the allocation - which is why the host test fences
// the scratch as tightly as it fences the frame buffer.

#define BENDX_SCRATCH_ROWS  2       // the default, in rows

unsigned bendx_scratch_bytes(int kind, unsigned rowlen);
unsigned bendx_chain_scratch_bytes(const bendx_chain_t *c, unsigned rowlen);

//-------------------------------------------------------------------

void bendx_reset(bendx_t *x);
int  bendx_is_off(const bendx_t *x);

// Change the effect and start its knobs somewhere worth looking at. Both UIs
// go through this rather than resetting by hand - see the definition for why
// the knobs must not carry over, and why the foreign buses do not start on
// BXMIX_SET.
void bendx_set_kind(bendx_t *x, int kind);

// Clamp every field into range. Called before applying, because a profile can
// arrive from a zeroed config block or from a build with a shorter effect
// list, and an out of range kind indexes a jump table.
void bendx_sanitize(bendx_t *x);

// The knobs. amount is 0..bendx_amount_max(kind) and reach is
// 0..bendx_reach_max(kind); both are inclusive, and both are meaningless for
// effects that return 0.
int  bendx_amount_max(int kind);
int  bendx_reach_max(int kind);

// Whether this effect has to unpack the buffer. The UI marks these because on
// these bodies it is the difference between a shot that saves and a shot you
// wait out - the same distinction bend_simplify() draws for the buses.
int  bendx_is_slow(int kind);

// Whether this effect reads a foreign buffer, and so does nothing if the port
// does not supply one.
int  bendx_needs_env(int kind);

// Names. bendx_name() is at most 12 characters, for the picker; bendx_label()
// writes the amount in the effect's own units into buf (>= 8 bytes).
const char *bendx_name(int kind);
const char *bendx_desc(int kind);
void        bendx_label(const bendx_t *x, char *buf);

// Reroll every knob but the effect itself, so a profile you like can be walked
// without losing it. Returns the seed used.
unsigned bendx_random(bendx_t *x, unsigned seed);

// Do the work. buf->base is written; env's buffers are only ever read.
void bendx_apply(const bendx_t *x, const bendx_buf_t *buf,
                 const bendx_env_t *env, unsigned char *scratch);

// Packed accessors, exposed only so the host test can check them against the
// ones in raw.c. Row-relative: p points at the first byte of the row.
unsigned bendx_get(const unsigned char *p, unsigned x, int nbits);
void     bendx_set(unsigned char *p, unsigned x, unsigned v, int nbits);

extern const char * const bendx_mix_names[BXMIX_COUNT];

#endif
