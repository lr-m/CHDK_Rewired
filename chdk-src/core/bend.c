//-------------------------------------------------------------------
// Bend engine - see BENDING_DESIGN.md and include/bend.h
//
// This file holds the model and the maths only. It has no CHDK dependencies
// so that tools/bend_selftest.c can compile it on the host and check the
// camera-side fast paths in raw.c against bend_eval() here. The appliers -
// the code that walks the raw buffer and the live viewport - live in raw.c
// where the buffer accessors are.
//-------------------------------------------------------------------

#include "bend.h"

//-------------------------------------------------------------------

const char * const bend_bus_names[BUS_COUNT] = {
    "Pixel clock",      // BUS_HCLK
    "Pixel clock /N",   // BUS_HCLKN
    "Line clock",       // BUS_VCLK
    "Line clock /N",    // BUS_VCLKN
    "Frame",            // BUS_FRAME
    "Diagonal",         // BUS_DIAG
    "Black level",      // BUS_OB
    "Noise",            // BUS_NOISE
    "Sample+hold",      // BUS_HOLD
    "Line memory",      // BUS_VHOLD
    "Bus parity",       // BUS_PARITY
    "Tap",              // BUS_TAP
};

// Two characters, because the pin strip in bend mode has ten columns across a
// 360px screen and there is no room for more.
static const char * const bus_short[BUS_COUNT] = {
    "HC", "Hc", "VC", "Vc", "FR", "DG", "OB", "NZ", "SH", "VH", "PY", "TP"
};

const char * const bend_trash_names[TRASH_COUNT] = {
    "White",            // TRASH_LFSR
    "Blocky",           // TRASH_LFSRN
    "Ramp",             // TRASH_RAMP
    "Burst",            // TRASH_BURST
    "Sensor seed",      // TRASH_SEED
};

//-------------------------------------------------------------------

static unsigned xs32(unsigned s)
{
    // xorshift32. Never let it reach zero - it is an absorbing state and the
    // noise bus would silently die halfway down a frame.
    if (!s) s = 0x9e3779b9u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// Bit numbers are mapped by distance from the top rather than by value, so
// "bit 9" stays the most significant bit whether the matrix is applied to the
// 10 bit raw domain or the 8 bit live view, and turning the knob feels the
// same in both. When the target is narrower the low bits simply fall off the
// bottom, which is what happens to them in the pipeline anyway.
// Returns -1 when the bit has no counterpart in the target domain rather than
// clamping. Which of those two the caller wants differs by direction, and
// getting it wrong is silent: see the two call sites in bend_compile.
static int bend_remap_raw(int i, int from, int to)
{
    int d = from - 1 - i;
    int r = to - 1 - d;
    if (r < 0 || r > to - 1) return -1;
    return r;
}

static int bend_remap(int i, int from, int to)
{
    int r = bend_remap_raw(i, from, to);
    if (r < 0) return (from > to) ? 0 : to - 1;
    return r;
}

//-------------------------------------------------------------------

void bend_reset(bend_t *b, int nbits)
{
    int i;
    for (i = 0; i < BEND_MAX_BITS; i++) b->route[i] = (unsigned char)BSRC_DATA(i);
    b->nbits      = (unsigned char)nbits;
    b->bayer      = 0;
    b->row_period = 0;
    b->hdiv       = 3;
    b->vdiv       = 3;
    b->trash_type = TRASH_LFSR;
    b->trash_rate = 0;
    b->depth      = 3;
    b->tap_dx     = 17;
    b->tap_dy     = -3;
    b->pad[0]     = 0;
    b->pad[1]     = 0;
    b->seed       = 0x5eed1e55u;
}

int bend_is_identity(const bend_t *b)
{
    int i;
    for (i = 0; i < b->nbits && i < BEND_MAX_BITS; i++)
        if (b->route[i] != (unsigned char)BSRC_DATA(i)) return 0;
    return 1;
}

void bend_sanitize(bend_t *b, int nbits)
{
    int i;

    // A zeroed config block, a preset from a camera with a different sensor,
    // or a hand-edited file all arrive here. A bend that is merely strange is
    // fine - that is the point - but one that indexes off the end of the LUT
    // or names a bus that does not exist is not.
    if (b->nbits < 2 || b->nbits > BEND_MAX_BITS)
    {
        bend_reset(b, nbits);
        return;
    }

    for (i = 0; i < BEND_MAX_BITS; i++)
    {
        unsigned char src = b->route[i];
        int idx = BSRC_IDX(src);

        switch (BSRC_CLASS(src))
        {
        case BSRC_C_DATA:
        case BSRC_C_NDATA:
            if (idx >= b->nbits) b->route[i] = (unsigned char)(BSRC_CLASS(src) | (b->nbits - 1));
            break;
        case BSRC_C_TIE:
            b->route[i] = (unsigned char)(idx & 1 ? BSRC_HIGH : BSRC_LOW);
            break;
        case BSRC_C_BUS:
        case BSRC_C_NBUS:
            if (idx >= BUS_COUNT) b->route[i] = (unsigned char)(BSRC_CLASS(src) | BUS_NOISE);
            break;
        default:
            b->route[i] = (unsigned char)BSRC_DATA(i < b->nbits ? i : b->nbits - 1);
            break;
        }
    }

    if (b->bayer > 4)                 b->bayer = 0;
    if (b->hdiv > 12)                 b->hdiv = 3;
    if (b->vdiv > 12)                 b->vdiv = 3;
    if (b->trash_type >= TRASH_COUNT) b->trash_type = TRASH_LFSR;
    if (b->trash_rate > 12)           b->trash_rate = 0;
    if (b->depth < 1 || b->depth > BEND_MAX_BITS) b->depth = 3;
    if (!b->seed)                     b->seed = 0x5eed1e55u;
}

//-------------------------------------------------------------------

void bend_compile(const bend_t *b, bend_cc_t *cc, unsigned short *lut, int nbits)
{
    int from = b->nbits ? b->nbits : nbits;
    unsigned v, size = 1u << nbits;
    int i, k;

    // Per output bit, resolved into the target bit domain.
    unsigned char pin[BEND_MAX_BITS];   // source pin, for the data classes
    unsigned char inv[BEND_MAX_BITS];   // read on the wrong polarity
    unsigned tie_high = 0, tie_mask = 0, data_mask = 0;

    cc->lut      = lut;
    cc->nbits    = nbits;
    cc->mask     = size - 1;
    cc->dyn_mask = 0;
    cc->bus_inv  = 0;
    cc->bus_used = 0;
    cc->hdiv     = b->hdiv;
    cc->vdiv     = b->vdiv;
    for (k = 0; k < BUS_COUNT; k++) cc->bus_bits[k] = 0;
    for (i = 0; i < BEND_MAX_BITS; i++) { pin[i] = (unsigned char)i; inv[i] = 0; }

    for (i = 0; i < nbits; i++)
    {
        unsigned      obit = 1u << i;
        int           ai   = bend_remap_raw(i, nbits, from);
        unsigned char src;

        if (ai < 0)
        {
            // This output bit has no counterpart in the authored matrix - it is
            // one of the extra low bits you get carrying a 10 bit bend to a 12
            // bit sensor. The matrix says nothing about it, so leave the line
            // straight through. Clamping it onto the lowest authored bit
            // instead would quietly wire it to a pin two places up.
            pin[i] = (unsigned char)i;
            data_mask |= obit;
            continue;
        }
        src = b->route[ai];

        switch (BSRC_CLASS(src))
        {
        case BSRC_C_NDATA:
            inv[i] = 1;
            /* fall through */
        case BSRC_C_DATA:
            pin[i] = (unsigned char)bend_remap(BSRC_IDX(src), from, nbits);
            data_mask |= obit;
            break;

        case BSRC_C_TIE:
            tie_mask |= obit;
            if (BSRC_IDX(src) & 1) tie_high |= obit;
            break;

        case BSRC_C_NBUS:
            cc->bus_inv |= obit;
            /* fall through */
        case BSRC_C_BUS:
        {
            int bus = BSRC_IDX(src);
            if (bus >= BUS_COUNT) bus = BUS_NOISE;
            cc->dyn_mask     |= obit;
            cc->bus_bits[bus]|= obit;
            cc->bus_used     |= 1u << bus;
            break;
        }

        default:
            pin[i] = (unsigned char)i;
            data_mask |= obit;
            break;
        }
    }

    // The pure part. Bus-fed bits are left zero here because bend_eval and the
    // appliers mask them out before the bus value is merged in, so whatever
    // the table holds for them never reaches the output.
    for (v = 0; v < size; v++)
    {
        unsigned o = tie_high;
        for (i = 0; i < nbits; i++)
        {
            unsigned obit = 1u << i;
            unsigned bit;
            if (!(data_mask & obit)) continue;
            bit = (v >> pin[i]) & 1;
            if (inv[i]) bit ^= 1;
            if (bit) o |= obit;
        }
        lut[v] = (unsigned short)(o & cc->mask);
    }

    cc->is_pure = (cc->dyn_mask == 0);
    cc->is_identity = 0;
    if (cc->is_pure && !tie_mask)
    {
        int same = 1;
        for (i = 0; i < nbits; i++)
            if (pin[i] != i || inv[i]) { same = 0; break; }
        cc->is_identity = same;
    }
}

//-------------------------------------------------------------------

static unsigned bend_parity(unsigned v)
{
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1;
}

// Value of every bus-fed output bit, before inversion. Written so that the
// buses a bend does not use cost one predictable branch each: bus_used is
// constant for the whole shot, so this collapses to just the live ones.
unsigned bend_busval(const bend_cc_t *cc, unsigned v, int x, int y, const bend_src_t *s)
{
    unsigned r = 0;
    unsigned used = cc->bus_used;

    if (used & (1u << BUS_HCLK))   { if (x & 1)                    r |= cc->bus_bits[BUS_HCLK]; }
    if (used & (1u << BUS_HCLKN))  { if ((x >> cc->hdiv) & 1)      r |= cc->bus_bits[BUS_HCLKN]; }
    if (used & (1u << BUS_VCLK))   { if (y & 1)                    r |= cc->bus_bits[BUS_VCLK]; }
    if (used & (1u << BUS_VCLKN))  { if ((y >> cc->vdiv) & 1)      r |= cc->bus_bits[BUS_VCLKN]; }
    if (used & (1u << BUS_FRAME))  { if (s->frame & 1)             r |= cc->bus_bits[BUS_FRAME]; }
    if (used & (1u << BUS_DIAG))   { if ((x ^ y) & 1)              r |= cc->bus_bits[BUS_DIAG]; }
    if (used & (1u << BUS_PARITY)) { if (bend_parity(v & cc->mask))r |= cc->bus_bits[BUS_PARITY]; }

    // These four carry a value per bit rather than one bit for the whole bus,
    // so they are masked rather than tested. That is what makes them smear:
    // bit 7 of the held pixel lands on bit 7 of the output.
    if (used & (1u << BUS_OB))     r |= s->ob    & cc->bus_bits[BUS_OB];
    if (used & (1u << BUS_NOISE))  r |= s->noise & cc->bus_bits[BUS_NOISE];
    if (used & (1u << BUS_HOLD))   r |= s->prev  & cc->bus_bits[BUS_HOLD];
    if (used & (1u << BUS_VHOLD))  r |= s->above & cc->bus_bits[BUS_VHOLD];
    if (used & (1u << BUS_TAP))    r |= s->tap   & cc->bus_bits[BUS_TAP];

    return r;
}

unsigned bend_eval(const bend_cc_t *cc, unsigned v, int x, int y, const bend_src_t *s)
{
    unsigned o = cc->lut[v & cc->mask];
    if (cc->dyn_mask)
    {
        unsigned bv = bend_busval(cc, v, x, y, s) ^ cc->bus_inv;
        o = (o & ~cc->dyn_mask) | (bv & cc->dyn_mask);
    }
    return o & cc->mask;
}

//-------------------------------------------------------------------
// Trash generators

unsigned bend_trash_seed(const bend_t *b, unsigned row, unsigned ob)
{
    if (b->trash_type == TRASH_SEED)
    {
        // Reseeded from the optical black value of this row, so the noise
        // tracks the sensor's own dark current instead of running free.
        return xs32(b->seed ^ (row * 0x9e3779b9u) ^ (ob << 16) ^ ob);
    }
    return xs32(b->seed ^ (row * 0x9e3779b9u));
}

unsigned bend_trash_step(const bend_t *b, unsigned *state, unsigned pos)
{
    unsigned rate = b->trash_rate & 15;

    switch (b->trash_type)
    {
    case TRASH_LFSRN:
        if ((pos & ((1u << rate) - 1u)) == 0) *state = xs32(*state);
        return *state;

    case TRASH_RAMP:
        // A counter, not noise - interferes with the image as a gradient
        // rather than as static.
        return pos >> rate;

    case TRASH_BURST:
        *state = xs32(*state);
        // Gated by a slow counter, so the damage arrives in bursts with clean
        // stretches between them.
        return ((pos >> (rate + 5)) & 1) ? *state : 0u;

    case TRASH_SEED:
    case TRASH_LFSR:
    default:
        *state = xs32(*state);
        return *state;
    }
}

//-------------------------------------------------------------------
// Preset generators. These fill the matrix from the older single-idea modes,
// so nothing that worked before this change is lost.

void bend_preset_swap(bend_t *b, int a, int c)
{
    unsigned char t;
    int n = b->nbits;
    if (a < 0 || c < 0 || a >= n || c >= n || a == c) return;
    t = b->route[a]; b->route[a] = b->route[c]; b->route[c] = t;
}

void bend_preset_rotate(bend_t *b, int k)
{
    unsigned char old[BEND_MAX_BITS];
    int n = b->nbits, i;
    for (i = 0; i < n; i++) old[i] = b->route[i];
    for (i = 0; i < n; i++) b->route[i] = old[((i - k) % n + n) % n];
}

void bend_preset_reverse(bend_t *b)
{
    unsigned char old[BEND_MAX_BITS];
    int n = b->nbits, i;
    for (i = 0; i < n; i++) old[i] = b->route[i];
    for (i = 0; i < n; i++) b->route[i] = old[n - 1 - i];
}

// Read this source on the wrong polarity.
//
// Not "^= 0x10". That works for the two data classes, which is all it was ever
// tried on, and is wrong for every other one: a pin tied high is 0x21, and
// 0x21 ^ 0x10 is 0x31 - the pixel clock bus. So inverting a tied pin used to
// silently patch a clock onto it, which is a different bend, and under "simple
// signals only" it is a bend that is not supposed to be reachable at all.
// 0x30 ^ 0x10 lands on the tie class the same way round.
unsigned char bend_src_invert(unsigned char src)
{
    int idx = BSRC_IDX(src);

    switch (BSRC_CLASS(src))
    {
    case BSRC_C_DATA:  return (unsigned char)BSRC_NDATA(idx);
    case BSRC_C_NDATA: return (unsigned char)BSRC_DATA(idx);
    // Inverting a line shorted to ground gives a line shorted to the rail.
    case BSRC_C_TIE:   return (unsigned char)((idx & 1) ? BSRC_LOW : BSRC_HIGH);
    case BSRC_C_BUS:   return (unsigned char)BSRC_NBUS(idx);
    case BSRC_C_NBUS:  return (unsigned char)BSRC_BUS(idx);
    }
    return src;
}

void bend_preset_xor(bend_t *b, unsigned mask)
{
    int n = b->nbits, i;
    for (i = 0; i < n; i++)
    {
        if (!(mask & (1u << i))) continue;
        // Inverting flips the polarity of whatever is already routed there
        // rather than overwriting it, so XOR composes with a swap.
        b->route[i] = bend_src_invert(b->route[i]);
    }
}

void bend_preset_tie(bend_t *b, int a, int high)
{
    if (a < 0 || a >= b->nbits) return;
    b->route[a] = (unsigned char)(high ? BSRC_HIGH : BSRC_LOW);
}

int bend_simplify(bend_t *b)
{
    int n = (b->nbits >= 2 && b->nbits <= BEND_MAX_BITS) ? b->nbits : BEND_MAX_BITS;
    int i, changed = 0;

    for (i = 0; i < BEND_MAX_BITS; i++)
    {
        unsigned char src = b->route[i];
        int cls = BSRC_CLASS(src);
        int pin;

        if (cls != BSRC_C_BUS && cls != BSRC_C_NBUS) continue;

        // Back onto its own line, not onto the pin whose number matches the bus
        // number. The two share the low nibble of the encoding and mean nothing
        // to each other, so reading one as the other would turn "turn the extra
        // signals off" into a silent extra rewire - the bend would keep
        // changing when the whole point of the switch is that it stops.
        pin = (i < n) ? i : n - 1;
        b->route[i] = (unsigned char)((cls == BSRC_C_NBUS) ? BSRC_NDATA(pin) : BSRC_DATA(pin));
        changed++;
    }
    return changed;
}

//-------------------------------------------------------------------
// Constrained random.
//
// A uniformly random matrix is almost always garbage: with ten bits, most
// routings wreck the top of the word and you get noise instead of an image.
// The weights below keep the picture legible at low depth and let it fall
// apart at high depth, so the knob is aggression rather than reroll-until-
// something-works.

unsigned bend_random(bend_t *b, unsigned seed, int depth, int allow_bus)
{
    unsigned s = seed ? seed : 0x1234567u;
    int n = b->nbits, i, guard;

    if (n < 2) return s;
    if (depth < 1) depth = 1;
    if (depth > n) depth = n;

    for (guard = 0; guard < 8; guard++)
    {
        bend_reset(b, n);
        b->nbits = (unsigned char)n;
        b->seed  = s;
        b->depth = (unsigned char)depth;

        for (i = 0; i < depth; i++)
        {
            int bit, roll;

            s = xs32(s);
            bit = (int)(s % (unsigned)n);
            // The top two bits carry the picture. Touch them sometimes, but
            // not one time in five like a uniform pick would.
            if (bit >= n - 2)
            {
                s = xs32(s);
                if (s & 3) bit = (int)(s % (unsigned)(n - 2));
            }

            s = xs32(s);
            roll = (int)(s % 100u);

            // Buses occupy the top fifth of the roll. With them off, the roll
            // is squeezed into what is left rather than rerolled, so the three
            // cheap edges keep the same proportions to each other and a bend
            // at a given depth still feels like the same knob.
            if (!allow_bus) roll = roll * 80 / 100;

            if (roll < 45)                      // swap - the commonest bend
            {
                int o;
                unsigned char t;
                s = xs32(s);
                o = (int)(s % (unsigned)n);
                t = b->route[bit]; b->route[bit] = b->route[o]; b->route[o] = t;
            }
            else if (roll < 63)                 // invert
            {
                b->route[bit] = bend_src_invert(b->route[bit]);
            }
            else if (roll < 80)                 // tie to a rail
            {
                s = xs32(s);
                b->route[bit] = (unsigned char)((s & 1) ? BSRC_HIGH : BSRC_LOW);
            }
            else if (roll < 94)                 // a clock or the black level
            {
                static const unsigned char tame[] = {
                    BUS_HCLK, BUS_HCLKN, BUS_VCLK, BUS_VCLKN,
                    BUS_DIAG, BUS_OB, BUS_HOLD, BUS_VHOLD
                };
                s = xs32(s);
                b->route[bit] = (unsigned char)BSRC_BUS(tame[s % (sizeof(tame)/sizeof(tame[0]))]);
            }
            else                                // trash - rarest, loudest
            {
                s = xs32(s);
                b->route[bit] = (unsigned char)BSRC_BUS((s & 1) ? BUS_NOISE : BUS_TAP);
                s = xs32(s);
                b->trash_type = (unsigned char)(s % TRASH_COUNT);
                s = xs32(s);
                b->trash_rate = (unsigned char)(s % 6);
            }
        }

        s = xs32(s);
        b->hdiv = (unsigned char)(1 + (s % 6));
        s = xs32(s);
        b->vdiv = (unsigned char)(1 + (s % 6));
        s = xs32(s);
        b->tap_dx = (signed char)((int)(s % 64) - 32);
        s = xs32(s);
        b->tap_dy = (signed char)((int)(s % 8) - 4);

        // A random bend that turns out to be the identity is a wasted press.
        if (!bend_is_identity(b)) break;
        s = xs32(s);
    }

    return s;
}

void bend_mutate(bend_t *b, unsigned seed, int allow_bus)
{
    // One edge moved, everything else held - so a look you like can be walked
    // rather than rerolled.
    unsigned s = xs32(seed);
    int n = b->nbits;
    int bit, roll;

    if (n < 2) return;
    bit = (int)(s % (unsigned)n);
    s = xs32(s);
    roll = (int)(s % 100u);
    if (!allow_bus) roll = roll * 90 / 100;     // the bus branch is the top tenth

    if (roll < 50)
    {
        int o;
        unsigned char t;
        s = xs32(s);
        o = (int)(s % (unsigned)n);
        t = b->route[bit]; b->route[bit] = b->route[o]; b->route[o] = t;
    }
    else if (roll < 75)
    {
        b->route[bit] = bend_src_invert(b->route[bit]);
    }
    else if (roll < 90)
    {
        b->route[bit] = (unsigned char)BSRC_DATA(bit);
    }
    else
    {
        s = xs32(s);
        b->route[bit] = (unsigned char)BSRC_BUS(s % BUS_COUNT);
    }
}

//-------------------------------------------------------------------

void bend_src_name(unsigned char src, char *buf)
{
    int idx = BSRC_IDX(src);

    switch (BSRC_CLASS(src))
    {
    case BSRC_C_DATA:
        buf[0] = 'd';
        buf[1] = (char)((idx < 10) ? ('0' + idx) : ('a' + idx - 10));
        buf[2] = 0;
        return;

    case BSRC_C_NDATA:
        buf[0] = '~';
        buf[1] = (char)((idx < 10) ? ('0' + idx) : ('a' + idx - 10));
        buf[2] = 0;
        return;

    case BSRC_C_TIE:
        buf[0] = (idx & 1) ? 'H' : 'L';
        buf[1] = (idx & 1) ? 'I' : 'O';
        buf[2] = 0;
        return;

    case BSRC_C_BUS:
    case BSRC_C_NBUS:
    {
        const char *s = bus_short[(idx < BUS_COUNT) ? idx : BUS_NOISE];
        int o = 0;
        if (BSRC_CLASS(src) == BSRC_C_NBUS) buf[o++] = '~';
        buf[o++] = s[0];
        buf[o++] = s[1];
        buf[o]   = 0;
        return;
    }
    }

    buf[0] = '?'; buf[1] = 0;
}

//-------------------------------------------------------------------
// Segments - see the note in bend.h.
//
// Everything here is integer and allocation-free, because it is called once
// per row of a fifteen megabyte buffer from inside the capture path.

#define BSEG_SIZE_DEF   50      // radius as a percentage of the short side

static const char * const bseg_names[BSEG_COUNT] = {
    "Off",              // BSEG_OFF
    "Top / bottom",     // BSEG_HALF_H
    "Left / right",     // BSEG_HALF_V
    "Quarters",         // BSEG_QUAD
    "Centre circle",    // BSEG_CIRCLE
    "Rings",            // BSEG_RINGS
    "Diagonal",         // BSEG_DIAG
    "Wedges - X",       // BSEG_WEDGE
    "Bands ---",        // BSEG_BANDS
    "Bars |||",         // BSEG_BARS
    "Checkerboard",     // BSEG_CHECKER
    "Warped lines",     // BSEG_WARP
    "Zigzag",           // BSEG_ZIGZAG
    "Truchet",          // BSEG_TRUCHET
    "Wave lines",       // BSEG_LINEWAVE
    "Checks",           // BSEG_CHECKS
    "Mirror tiles",     // BSEG_MIRROR
};

// Region names, four per layout, padded with the empty string. Indexed rather
// than switched so that adding a layout is one row of table and one case in
// bend_seg_spans() and cannot get out of step with the count.
static const char * const bseg_labels[BSEG_COUNT][BEND_SEG_MAX] = {
    { "whole frame", "",           "",             ""             },
    { "top",         "bottom",     "",             ""             },
    { "left",        "right",      "",             ""             },
    { "top left",    "top right",  "bottom left",  "bottom right" },
    { "circle",      "around it",  "",             ""             },
    { "centre",      "ring",       "outside",      ""             },
    { "upper right", "lower left", "",             ""             },
    { "top",         "right",      "bottom",       "left"         },
    { "band 1",      "band 2",     "band 3",       "band 4"       },
    { "bar 1",       "bar 2",      "bar 3",        "bar 4"        },
    { "square 1",    "square 2",   "square 3",     "square 4"     },
    { "black",       "white",      "",             ""             },
    { "black",       "white",      "",             ""             },
    { "black",       "white",      "",             ""             },
    { "black",       "white",      "",             ""             },
    { "black",       "white",      "",             ""             },
    { "black",       "white",      "",             ""             },
};

static const unsigned char bseg_counts[BSEG_COUNT] = { 1, 2, 2, 4, 2, 3,
                                                       2, 4, 4, 4, 4,
                                                       2, 2, 2, 2, 2, 2 };

int bend_seg_has_size(int layout)
{
    return layout == BSEG_CIRCLE || layout == BSEG_RINGS ||
           layout == BSEG_BANDS  || layout == BSEG_BARS  ||
           layout == BSEG_CHECKER || BSEG_IS_PATTERN(layout);
}

int bend_seg_cuts_rows(int layout)
{
    // Everything except the two that only ever change region between rows.
    // Bands is the interesting one: it is the only repeating layout that keeps
    // every row whole, which makes it the cheap way to have four bends in one
    // frame and the one to reach for when the shot has to be quick.
    return !(layout == BSEG_OFF || layout == BSEG_HALF_H ||
             layout == BSEG_BANDS);
}

int bend_seg_count(int layout)
{
    if (layout < 0 || layout >= BSEG_COUNT) return 1;
    return bseg_counts[layout];
}

const char *bend_seg_name(int layout)
{
    if (layout < 0 || layout >= BSEG_COUNT) layout = BSEG_OFF;
    return bseg_names[layout];
}

const char *bend_seg_label(int layout, int seg)
{
    if (layout < 0 || layout >= BSEG_COUNT) layout = BSEG_OFF;
    if (seg < 0 || seg >= BEND_SEG_MAX)     seg = 0;
    return bseg_labels[layout][seg];
}

void bend_seg_sanitize(bend_segs_t *s, int nbits)
{
    int i, n;

    if (s->layout >= BSEG_COUNT) s->layout = BSEG_OFF;

    // Zero means the field has never been written - a config block from a
    // build without segments, or a factory default. A disc of no radius is a
    // layout that appears to do nothing, which is indistinguishable from a
    // broken one, so it starts where it is worth looking at instead.
    if (s->size == 0)   s->size = BSEG_SIZE_DEF;
    if (s->size > 100)  s->size = 100;

    n = bend_seg_count(s->layout);
    if (s->active >= n) s->active = 0;

    // Preset numbers are two digits on the card, and these are stored plus
    // one, so anything above a hundred is not a preset this camera could ever
    // open. A region claiming to hold BEND99 when the card has eight files
    // would put a name on a row that no press could reach.
    for (i = 0; i < BEND_SEG_MAX; i++)
        if (s->slot[i] > 100) s->slot[i] = 0;

    // Every matrix goes through the same gate the camera's own does. They are
    // stored in the config block and restored by size, so one of them arriving
    // as zeroes is the ordinary case rather than the corrupt one.
    for (i = 0; i < BEND_SEG_MAX - 1; i++) bend_sanitize(&s->b[i], nbits);
}

//-------------------------------------------------------------------
// Integer square root, for the circular layouts. Newton would need a divide
// per iteration and this body has no hardware divider; the bit-at-a-time
// method is shifts and compares only, and it runs at most sixteen times.

static unsigned bseg_isqrt(unsigned v)
{
    unsigned rem = v, root = 0, bit = 1u << 30;

    while (bit > rem) bit >>= 2;
    while (bit)
    {
        if (rem >= root + bit)
        {
            rem  -= root + bit;
            root  = (root >> 1) + bit;
        }
        else
            root >>= 1;
        bit >>= 2;
    }
    return root;
}

// Half the width of the chord a circle of radius r cuts at a vertical distance
// dy from its centre, or 0 if the row misses the circle entirely.
static int bseg_chord(int r, int dy)
{
    if (dy < 0) dy = -dy;
    if (r <= 0 || dy >= r) return 0;
    return (int)bseg_isqrt((unsigned)(r * r - dy * dy));
}

//-------------------------------------------------------------------
// Noise-warped stripes, for BSEG_WARP.
//
// A transcription of the standard "domain warp" fragment shader - value noise
// used as a rotation angle, applied to the coordinate before a stripe function
// samples it - into what the span model can carry.
//
// The span model is the whole design constraint. A shader returns a value per
// pixel; a layout here returns runs, so the pattern has to be sampled along the
// row and run-length encoded. Two consequences worth knowing before reading the
// code: the sampling step is what decides how fine a stripe can survive, and
// the stripe count is what decides how many spans a row needs. They pull
// against each other and against BEND_SEG_SPANS.
//
// The coordinate swap is load-bearing. The reference builds pos from st.yx, so
// the stripe axis is the *row* - stripes run across the frame, and a scanline
// mostly travels along a stripe rather than across it. That is what makes the
// pattern affordable here: a row needs few spans everywhere except where the
// warp has rotated the stripes to vertical, which is exactly the small vortex
// regions in the reference image.
//
// Fixed point throughout, 8-bit fraction. There is no FPU on any of these
// bodies and this runs once per row of a fifteen megabyte buffer inside the
// capture path.
//
// One octave of noise rather than the usual stack of them. Summed octaves pull
// towards the middle of the range, which here would flatten the warp into a
// uniform shear.

// Integer hash. Deterministic and platform-independent - the host selftest and
// the camera have to agree about which region a pixel is in, so this cannot be
// rand() and cannot depend on anything that varies per build.
static unsigned bseg_hash(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (h ^ (h >> 16)) & 0xFFu;
}

// Value noise at (px,py), both 8.8 fixed point. Returns 0..255.
//
// The lattice corners are hashed and bilinearly mixed, with the same
// smoothstep the reference shaders use - f*f*(3-2f), written here as
// (f*f*(768-2f))>>16 to stay in integers. Peak intermediate is 255*255*258,
// about 16.7M, so it stays inside a signed 32-bit int with room to spare.
static int bseg_vnoise(int px, int py)
{
    int i  = px >> 8,     j  = py >> 8;
    int fx = px & 0xFF,   fy = py & 0xFF;
    int ux = (fx * fx * (768 - 2 * fx)) >> 16;
    int uy = (fy * fy * (768 - 2 * fy)) >> 16;

    int a = (int)bseg_hash(i,     j);
    int b = (int)bseg_hash(i + 1, j);
    int c = (int)bseg_hash(i,     j + 1);
    int d = (int)bseg_hash(i + 1, j + 1);

    int ab = a + (((b - a) * ux) >> 8);
    int cd = c + (((d - c) * ux) >> 8);

    return ab + (((cd - ab) * uy) >> 8);
}

// sin(2*pi*a/256), returned in 8.8 fixed point so the range is -256..256.
//
// The parabola 4x(1-|x|) over a half turn, which is the standard cheap sine and
// is accurate to about 6% at its worst - well inside what a pattern that ends
// up quantised to four regions can notice. A table would be more accurate and
// would cost a cache line on every call from the row loop, which this does not.
static int bseg_sin(int a)
{
    int x = (a & 255) * 2;              // 0..510, a half turn per 256
    if (x >= 256) x -= 512;             // fold to -256..254
    return (x * (256 - ((x < 0) ? -x : x))) / 64;
}

static int bseg_cos(int a)
{
    return bseg_sin(a + 64);
}

// How far the noise is allowed to rotate the stripe field. The reference feeds
// noise straight in as radians, so about a sixth of a turn; 64 is a quarter,
// which bends the stripes further and gives the closed vortices rather than a
// gentle wave.
#define BSEG_WARP_TURN  64

// Extent of the pattern coordinate down the frame, in 8.8 - the reference's
// vec2(10.,3.) scaling, so pux runs 0..10.0 and puy 0..3.0. The stripe count is
// divided by this rather than shifted by 8, which is the whole difference
// between "n stripes across the frame" and "n stripes per pattern unit" - ten
// times too many, which aliases into hash at any sane sampling step.
#define BSEG_WARP_SPAN_X  (10 * 256)
#define BSEG_WARP_SPAN_Y  (3 * 256)

// 2*pi in the same 8.8 the coordinates use, for turning radians - which is what
// the references are written in - into the turn units bseg_sin() takes.
#define BSEG_TWO_PI       1608

//-------------------------------------------------------------------
// The pattern layouts.
//
// Six fragment shaders from The Book of Shaders chapter 9, transcribed into
// fixed point and reduced to a two-region mask. They share everything below
// this line: one coordinate convention, one per-shot re-roll, one run-length
// encoder in bend_seg_spans().
//
// Coordinates are 8.8 fixed point with 256 meaning 1.0, so the frame is 0..256
// on both axes whatever the sensor actually is. fract() is & 255 and works on
// negatives as it stands, because two's complement makes the mask the positive
// residue - which the mirrored layouts depend on when they compute 1.0 - y.
//
// Every one of them ends in a comparison rather than a smoothstep. The
// references anti-alias their edges; there is nothing here to anti-alias into,
// because the output is which bend a pixel gets and there is no blending
// between two bends. A hard edge is the honest form of the same pattern.

// Per-shot re-roll. Held as derived values rather than re-mixed per sample -
// this is called once per pixel sampled, and the mixing is not free.
static unsigned bseg_seed     = 0x5eed1e55u;
static unsigned bseg_seed_cur = 0;      // 0 is not a legal seed, so this forces
static int bseg_ox, bseg_oy, bseg_inv, bseg_nx, bseg_ny;

static unsigned bseg_mix(unsigned s, unsigned k)
{
    s ^= k * 2654435761u;
    s ^= s >> 15;
    s *= 2246822519u;
    s ^= s >> 13;
    return s;
}

void bend_seg_set_seed(unsigned s)
{
    // Zero is reserved as "never prepared", so a caller handing us a zero tick
    // count would otherwise re-roll on every row for the rest of the shot.
    bseg_seed = s ? s : 0x5eed1e55u;
}

static void bseg_pat_prep(void)
{
    if (bseg_seed_cur == bseg_seed) return;
    bseg_seed_cur = bseg_seed;

    // Translation is the re-roll that suits every one of these: they are all
    // tilings, so moving the origin lands the tile boundaries somewhere else
    // entirely without changing what the pattern is.
    bseg_ox  = (int)(bseg_mix(bseg_seed, 1) & 255);
    bseg_oy  = (int)(bseg_mix(bseg_seed, 2) & 255);
    // Which region is figure and which is ground. Free, and it is the
    // difference that reads most immediately between two shots.
    bseg_inv = (int)(bseg_mix(bseg_seed, 3) & 1);
    // A different part of the noise field for the warp, which is a stronger
    // change than moving it - the vortices land in new places rather than the
    // same ones shifted.
    bseg_nx  = (int)(bseg_mix(bseg_seed, 4) & 1023);
    bseg_ny  = (int)(bseg_mix(bseg_seed, 5) & 1023);
}

// Tiles across the frame, from the size knob. 2 to 12.
static int bseg_pat_zoom(int size)
{
    return 2 + (size * 10) / 100;
}

// Stripes across the frame for the warp, from the same knob. 8 to 64.
static int bseg_pat_nstr(int size)
{
    return 8 + (size * 56) / 100;
}

// Sampling step, in pixels.
//
// A constant is only ever right for one frame width - eight pixels is five
// samples per stripe on a 2664px sensor row and less than one on a 512px
// preview, which is what turned the first version of the warp into aliased
// hash. Derived from the finest feature the layout can be asked for, so the
// sampling density is the same whatever the frame is.
static int bseg_pat_step(int layout, int size, int w)
{
    int feats = (layout == BSEG_WARP) ? (bseg_pat_nstr(size) * 4)
                                      : (bseg_pat_zoom(size) * 16);
    int s = (feats > 0) ? (w / feats) : 1;
    return (s < 1) ? 1 : s;
}

// Which of the two regions (x,y) falls in.
static int bseg_pat_at(int layout, int x, int y, int w, int h, int size)
{
    int sx   = (w > 0) ? (((x * 256) / w) + bseg_ox) : 0;
    int sy   = (h > 0) ? (((y * 256) / h) + bseg_oy) : 0;
    int zoom = bseg_pat_zoom(size);
    int v    = 0;

    switch (layout)
    {
    case BSEG_WARP:
    {
        int nstr = bseg_pat_nstr(size);
        int pux  = (sy * BSEG_WARP_SPAN_X) / 256;
        int puy  = (sx * BSEG_WARP_SPAN_Y) / 256;
        int ang  = (bseg_vnoise(pux + bseg_nx, puy + bseg_ny) * BSEG_WARP_TURN) >> 8;
        int rx   = ((pux * bseg_cos(ang)) - (puy * bseg_sin(ang))) >> 8;
        // Masked rather than compared, so a negative rx still lands on a real
        // region - two's complement makes -1 & 1 come out as 1.
        v = ((rx * nstr) / BSEG_WARP_SPAN_X) & 1;
        break;
    }

    case BSEG_ZIGZAG:
    {
        // mirrorTile(st*vec2(1,2), zoom), then a fill whose threshold ramps
        // linearly between 0 and 1 across each unit of x - which is the zigzag.
        int tx = sx * zoom;
        int ty = sy * 2 * zoom;
        int fx, fy, x2, a, f, pct;

        // fract(_st.y*0.5) > 0.5 is true exactly when the integer part of the
        // scaled y is odd, which is cheaper to ask directly.
        if ((ty >> 8) & 1) { tx += 128; ty = 256 - ty; }
        fx = tx & 255;
        fy = ty & 255;

        x2  = fx * 2;
        // floor(1.0 + sin(x*PI)) is 1 where the sine is positive and 0 where it
        // is not, so it is the parity of the integer part rather than a sine.
        a   = ((x2 >> 8) & 1) ? 0 : 1;
        f   = x2 & 255;
        // mix(a, b, f) with b = 1-a, so it ramps one way or the other.
        pct = a ? (256 - f) : f;
        v   = (fy > pct);
        break;
    }

    case BSEG_TRUCHET:
    {
        // Each tile split into four, each quarter rotated by its index, then a
        // diagonal drawn in it. The rotations are all multiples of 90 degrees,
        // so they are coordinate swaps and do not need the sine.
        int tx  = (sx * zoom) & 255;
        int ty  = (sy * zoom) & 255;
        int tx2 = tx * 2, ty2 = ty * 2;
        int idx = ((tx2 >> 8) & 1) + (((ty2 >> 8) & 1) << 1);
        int fx  = tx2 & 255, fy = ty2 & 255;
        int rx, ry;

        switch (idx)
        {
        case 1:  rx = 256 - fy; ry = fx;       break;   //  90
        case 2:  rx = fy;       ry = 256 - fx; break;   // -90
        case 3:  rx = 256 - fx; ry = 256 - fy; break;   // 180
        default: rx = fx;       ry = fy;       break;
        }
        v = (ry >= rx);
        break;
    }

    case BSEG_LINEWAVE:
    {
        // Lines displaced by a cosine of x. The aspect correction is the
        // reference's st.x *= u_resolution.x/u_resolution.y, and it matters
        // here because the sensor is a long way from square.
        int sxa = (h > 0) ? ((sx * w) / h) : sx;
        int X   = sxa * zoom;
        int Y   = sy * zoom;
        int a, u;

        // cos(X*3) - radians into turn units, where 256 is a full turn and
        // BSEG_TWO_PI is 2*pi in the same 8.8 the coordinates use.
        a  = ((X * 3) * 256) / BSEG_TWO_PI;
        Y += bseg_cos(a);

        // step(0.5, 1 - smoothstep(0,1,|sin(Y*PI)|)) reduces to |sin| <= 0.5,
        // because smoothstep is 0.5 exactly at its own midpoint. sin(Y*PI) is
        // half a turn per unit of Y, so the angle is Y/2.
        u = bseg_sin(Y >> 1);
        if (u < 0) u = -u;
        v = (u <= 128);
        break;
    }

    case BSEG_CHECKS:
    {
        // A box on each tile of a grid, with the tile rotated 45 degrees under
        // it. 181 is cos(45)*256, and it is both terms because sin and cos are
        // equal there.
        int tx = (sx * zoom) & 255;
        int ty = (sy * zoom) & 255;
        int dx = tx - 128, dy = ty - 128;
        int rx = 128 + ((dx * 181) - (dy * 181)) / 256;
        int ry = 128 + ((dx * 181) + (dy * 181)) / 256;

        // box(st, vec2(0.7)) keeps 0.15..0.85 on each axis.
        v = (rx > 38 && rx < 218 && ry > 38 && ry < 218);
        break;
    }

    case BSEG_MIRROR:
    default:
    {
        // As the zigzag, but mirrored in y only and filled against a sine
        // rather than a ramp.
        int tx = sx * zoom;
        int ty = sy * zoom;
        int fx, fy, pct;

        if ((ty >> 8) & 1) ty = 256 - ty;
        fx  = tx & 255;
        fy  = ty & 255;
        // sin(st.x*PI*2) is one full turn per unit of x, so fx is already the
        // angle. 115 is the reference's 0.45 amplitude.
        pct = 128 + (bseg_sin(fx) * 115) / 256;
        v   = (fy > pct);
        break;
    }
    }

    return bseg_inv ? !v : v;
}

// Append one span ending at x. Spans arrive left to right, so this is where
// every degenerate case is dealt with once: a span that ends where the last
// one did is empty and is dropped, a span abutting one of the same region is
// merged into it, and nothing is ever written past the end of the row. That
// is what lets the layouts below be written as a list of boundaries without
// each of them having to handle a circle wider than the frame.
static int bseg_emit(unsigned char *seg, unsigned short *xend, int n,
                     int s, int x, int w)
{
    int last = (n > 0) ? (int)xend[n - 1] : 0;

    if (x > w) x = w;
    if (x <= last) return n;
    if (n > 0 && seg[n - 1] == (unsigned char)s) { xend[n - 1] = (unsigned short)x; return n; }
    if (n >= BEND_SEG_SPANS) { xend[n - 1] = (unsigned short)x; return n; }

    seg[n]  = (unsigned char)s;
    xend[n] = (unsigned short)x;
    return n + 1;
}

int bend_seg_spans(int layout, int size, int y, int w, int h,
                   unsigned char *seg, unsigned short *xend)
{
    int n = 0;
    int cx = w / 2, cy = h / 2;
    int rmax = ((w < h) ? w : h) / 2;
    int r, c;

    if (w <= 0) { seg[0] = 0; xend[0] = 0; return 1; }
    if (layout < 0 || layout >= BSEG_COUNT) layout = BSEG_OFF;
    if (size < 1)   size = 1;
    if (size > 100) size = 100;

    switch (layout)
    {
    case BSEG_HALF_H:
        n = bseg_emit(seg, xend, n, (y < cy) ? 0 : 1, w, w);
        break;

    case BSEG_HALF_V:
        n = bseg_emit(seg, xend, n, 0, cx, w);
        n = bseg_emit(seg, xend, n, 1, w,  w);
        break;

    case BSEG_QUAD:
    {
        // 0 1
        // 2 3 - reading order, so the labels and the numbers agree.
        int top = (y < cy) ? 0 : 2;
        n = bseg_emit(seg, xend, n, top,     cx, w);
        n = bseg_emit(seg, xend, n, top + 1, w,  w);
        break;
    }

    case BSEG_CIRCLE:
        r = rmax * size / 100;
        c = bseg_chord(r, y - cy);
        n = bseg_emit(seg, xend, n, 1, cx - c, w);
        n = bseg_emit(seg, xend, n, 0, cx + c, w);
        n = bseg_emit(seg, xend, n, 1, w,      w);
        break;

    case BSEG_RINGS:
    {
        // The knob is the outer boundary and the inner one is half of it, so
        // one control moves the whole figure and the ring never inverts.
        int ro = rmax * size / 100;
        int ri = ro / 2;
        int co = bseg_chord(ro, y - cy);
        int ci = bseg_chord(ri, y - cy);

        n = bseg_emit(seg, xend, n, 2, cx - co, w);
        n = bseg_emit(seg, xend, n, 1, cx - ci, w);
        n = bseg_emit(seg, xend, n, 0, cx + ci, w);
        n = bseg_emit(seg, xend, n, 1, cx + co, w);
        n = bseg_emit(seg, xend, n, 2, w,       w);
        break;
    }

    case BSEG_DIAG:
    {
        // Corner to corner, top left to bottom right. The boundary is where
        // the scan has got to as a fraction of the frame, which is the same
        // number in both axes - a fault that drifts at exactly the rate the
        // readout advances draws this line and no other.
        int xb = (int)(((long)w * y) / (h > 0 ? h : 1));
        n = bseg_emit(seg, xend, n, 1, xb, w);
        n = bseg_emit(seg, xend, n, 0, w,  w);
        break;
    }

    case BSEG_WEDGE:
    {
        // Both diagonals. Four triangles meeting at the centre, so the regions
        // are top, right, bottom and left rather than corners - which is what
        // makes it different from Quarters and not just Quarters rotated.
        int d1 = (int)(((long)w * y) / (h > 0 ? h : 1));     // '\'
        int d2 = w - d1;                                     // '/'
        int lo = (d1 < d2) ? d1 : d2;
        int hi = (d1 < d2) ? d2 : d1;
        int mid = (y < cy) ? 0 : 2;                          // top or bottom

        n = bseg_emit(seg, xend, n, 3,   lo, w);             // left
        n = bseg_emit(seg, xend, n, mid, hi, w);
        n = bseg_emit(seg, xend, n, 1,   w,  w);             // right
        break;
    }

    case BSEG_BANDS:
    {
        // The one repeating layout that leaves every row whole, so it keeps
        // the packed fast path. The knob is the band height: at 100 the four
        // regions divide the frame once, and below that they cycle.
        int band = (h * size / 100) / BEND_SEG_MAX;
        // Rows outside the frame would divide to a negative band number, and
        // that indexes the region array. Nothing in the applier asks for one,
        // but this function is also the thing that decides what the applier is
        // allowed to be asked, so it holds the line rather than assuming.
        int yy = (y < 0) ? 0 : y;
        if (band < 1) band = 1;
        n = bseg_emit(seg, xend, n, (yy / band) % BEND_SEG_MAX, w, w);
        break;
    }

    case BSEG_BARS:
    {
        int bar = (w * size / 100) / BEND_SEG_MAX;
        int x;
        // Held at a cell that cannot overrun the span array - see the note on
        // BEND_SEG_SPANS. The knob stops being able to make them finer well
        // before it stops turning, which is the honest way round: a stripe
        // narrower than this could not be applied and should not be offered.
        if (bar < w / BEND_SEG_MINCELL) bar = w / BEND_SEG_MINCELL;
        if (bar < 1) bar = 1;
        for (x = 0; x < w; x += bar)
            n = bseg_emit(seg, xend, n, (x / bar) % BEND_SEG_MAX, x + bar, w);
        break;
    }

    case BSEG_CHECKER:
    {
        // Square cells in sensor pixels, so they are square in the picture -
        // measured off the short side for the same reason the circle is.
        int cell = (rmax * 2 * size / 100) / BEND_SEG_MAX;
        int row, x;
        if (cell < w / BEND_SEG_MINCELL) cell = w / BEND_SEG_MINCELL;
        if (cell < 1) cell = 1;
        row = ((y < 0) ? 0 : y) / cell;
        for (x = 0; x < w; x += cell)
            n = bseg_emit(seg, xend, n,
                          ((x / cell) + row) % BEND_SEG_MAX, x + cell, w);
        break;
    }

    case BSEG_WARP:
    case BSEG_ZIGZAG:
    case BSEG_TRUCHET:
    case BSEG_LINEWAVE:
    case BSEG_CHECKS:
    case BSEG_MIRROR:
    {
        // One encoder for all six. The pattern is sampled along the row and the
        // runs between changes become the spans - which is the whole bridge
        // between a shader, which answers per pixel, and this, which answers in
        // runs.
        int yy   = (y < 0) ? 0 : y;
        int step = bseg_pat_step(layout, size, w);
        int x, cur;

        bseg_pat_prep();

        cur = bseg_pat_at(layout, 0, yy, w, h, size);
        for (x = step; x < w; x += step)
        {
            int s = bseg_pat_at(layout, x, yy, w, h, size);
            if (s != cur)
            {
                n = bseg_emit(seg, xend, n, cur, x, w);
                cur = s;
            }
        }
        // Always closes at w, so the row tiles exactly however the sampling
        // came out and whatever bseg_emit did with the span budget.
        n = bseg_emit(seg, xend, n, cur, w, w);
        break;
    }

    case BSEG_OFF:
    default:
        n = bseg_emit(seg, xend, n, 0, w, w);
        break;
    }

    // A row of zero width is the only way to get here with nothing emitted,
    // and it is already handled above - but the callers loop over the result
    // without checking, so this cannot be allowed to return 0.
    if (n == 0) { seg[0] = 0; xend[0] = (unsigned short)w; n = 1; }
    return n;
}

int bend_seg_at(int layout, int size, int x, int y, int w, int h)
{
    unsigned char  seg[BEND_SEG_SPANS];
    unsigned short xend[BEND_SEG_SPANS];
    int n = bend_seg_spans(layout, size, y, w, h, seg, xend);
    int i;

    for (i = 0; i < n; i++)
        if (x < (int)xend[i]) return seg[i];
    return seg[n - 1];
}
