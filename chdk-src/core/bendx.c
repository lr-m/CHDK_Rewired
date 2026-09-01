//-------------------------------------------------------------------
// Experimental profiles - see include/bendx.h and docs/EXPERIMENTAL_EFFECTS.md
//
// One named failure of the path between the ADC and the JPEG encoder, applied
// to the frame buffer after the routing matrix in bend.c has finished with the
// bus. No CHDK dependencies, so tools/bendx_selftest.c can compile it on the
// host and prove the bounds - which matters more here than it does for bend.c,
// because these effects address the buffer themselves instead of being handed
// one pixel at a time.
//
// Every write in this file lands inside buf->base[0 .. rows*rowlen). Every
// read of a foreign buffer lands inside the length the caller declared for it.
// Those two sentences are the whole safety argument, and the host test exists
// to keep them true.
//-------------------------------------------------------------------

#include "camera.h"
#include "bendx.h"

#ifdef CAM_BEND_EXPERIMENTAL

//-------------------------------------------------------------------

const char * const bendx_mix_names[BXMIX_COUNT] = {
    "Replace", "XOR", "OR", "AND"
};

// At most 12 characters - the picker column is 20 and has to hold a marker,
// the name and the amount.
static const char * const bx_names[BX_COUNT] = {
    "Off",
    "Byte order",       // BX_ENDIAN
    "Bit slip",         // BX_SLIP
    "Sync tear",        // BX_TEAR
    "Row addr",         // BX_ADDR
    "Col addr",         // BX_COLADDR
    "DMA stride",       // BX_STRIDE
    "Refresh",          // BX_REFRESH
    "Bus echo",         // BX_ECHO
    "Line mem",         // BX_LINEMEM
    "Field flip",       // BX_FIELD
    "ROM bus",          // BX_ROMBUS
    "Clock slip",       // BX_CLOCK
    "CFA phase",        // BX_CFA
    "Clamp run",        // BX_CLAMP
    "Bloom",            // BX_BLOOM
    "Pixel sort",       // BX_SORT
    "Lock loss",        // BX_LOCK
    "JPEG bus",         // BX_JPGBUS
};

// One line each, for the menu. What the hardware fault is, not what the code
// does - the code is the imitation and the fault is the thing.
static const char * const bx_descs[BX_COUNT] = {
    "No experimental profile",
    "Bytes latched in the wrong order",
    "Row read back off its bit boundary",
    "Bit slip growing down the frame",
    "A row address line stuck",
    "A column address line stuck",
    "DMA stride mis-programmed",
    "DRAM refresh missed - bits decay",
    "The bus reflected back on itself",
    "Line memory holding the last row",
    "Field phase inverted",
    "Program ROM on the data lanes",
    "Pixel clock at the wrong rate",
    "Demosaic on the wrong Bayer phase",
    "Black level clamp loop unlocked",
    "Saturation into the V register",
    "Runs sorted along the row",
    "Timing losing lock and re-finding",
    "The JPEG buffer on the data lanes",
};

//-------------------------------------------------------------------

static unsigned xs32(unsigned s)
{
    if (!s) s = 0x9e3779b9u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// Bytes per packed group, and pixels in it. The frame buffer is not a linear
// bit stream: eight pixels share ten bytes at 10bpp and four share six at
// 12bpp, in an order that is not ascending. That is why the byte-domain
// effects below scramble rather than shift, and why they look like a memory
// fault rather than like a filter.
static unsigned bx_gsz(int nbits)  { return (nbits == 10) ? 10u : 6u; }
static unsigned bx_gpix(int nbits) { return (nbits == 10) ?  8u : 4u; }

// Pixels the pixel-domain effects may address.
//
// A packed group is indivisible: writing any pixel of it touches all of its
// bytes, so a row whose pixel count is not a whole number of groups has a tail
// that cannot be written without reaching past the end of the row - and on the
// last row of the frame, past the end of the buffer. Both cameras here divide
// exactly (3152/8 and 3720/4) so nothing is lost, but rowpix is a per-camera
// constant and the next port may not be so tidy. The tail keeps its own value,
// which is at most three pixels of the far right edge.
static unsigned bx_wholepix(const bendx_buf_t *b)
{
    return b->rowpix - (b->rowpix % bx_gpix(b->nbits));
}

//-------------------------------------------------------------------
// Packed accessors. Lifted from get_raw_pixel/set_raw_pixel in core/raw.c and
// made row-relative; tools/bendx_selftest.c checks them against the originals
// for every phase at both depths.

unsigned bendx_get(const unsigned char *p, unsigned x, int nbits)
{
    if (nbits == 10)
    {
        const unsigned char *a = p + (x / 8) * 10;
        switch (x % 8) {
            case 0: return ((0x3fc & (((unsigned)a[1]) << 2)) | (a[0] >> 6));
            case 1: return ((0x3f0 & (((unsigned)a[0]) << 4)) | (a[3] >> 4));
            case 2: return ((0x3c0 & (((unsigned)a[3]) << 6)) | (a[2] >> 2));
            case 3: return ((0x300 & (((unsigned)a[2]) << 8)) | (a[5]));
            case 4: return ((0x3fc & (((unsigned)a[4]) << 2)) | (a[7] >> 6));
            case 5: return ((0x3f0 & (((unsigned)a[7]) << 4)) | (a[6] >> 4));
            case 6: return ((0x3c0 & (((unsigned)a[6]) << 6)) | (a[9] >> 2));
            case 7: return ((0x300 & (((unsigned)a[9]) << 8)) | (a[8]));
        }
    }
    else
    {
        const unsigned char *a = p + (x / 4) * 6;
        switch (x % 4) {
            case 0: return (((unsigned)a[1])        << 4) | (a[0] >> 4);
            case 1: return (((unsigned)(a[0]&0x0f)) << 8) | (a[3]);
            case 2: return (((unsigned)a[2])        << 4) | (a[5] >> 4);
            case 3: return (((unsigned)(a[5]&0x0f)) << 8) | (a[4]);
        }
    }
    return 0;
}

void bendx_set(unsigned char *p, unsigned x, unsigned v, int nbits)
{
    if (nbits == 10)
    {
        unsigned char *a = p + (x / 8) * 10;
        switch (x % 8) {
            case 0: a[0]=(a[0]&0x3F)|(unsigned char)(v<<6); a[1]=(unsigned char)(v>>2);                       break;
            case 1: a[0]=(a[0]&0xC0)|(unsigned char)(v>>4); a[3]=(a[3]&0x0F)|(unsigned char)(v<<4);           break;
            case 2: a[2]=(a[2]&0x03)|(unsigned char)(v<<2); a[3]=(a[3]&0xF0)|(unsigned char)(v>>6);           break;
            case 3: a[2]=(a[2]&0xFC)|(unsigned char)(v>>8); a[5]=(unsigned char)v;                            break;
            case 4: a[4]=(unsigned char)(v>>2);             a[7]=(a[7]&0x3F)|(unsigned char)(v<<6);           break;
            case 5: a[6]=(a[6]&0x0F)|(unsigned char)(v<<4); a[7]=(a[7]&0xC0)|(unsigned char)(v>>4);           break;
            case 6: a[6]=(a[6]&0xF0)|(unsigned char)(v>>6); a[9]=(a[9]&0x03)|(unsigned char)(v<<2);           break;
            case 7: a[8]=(unsigned char)v;                  a[9]=(a[9]&0xFC)|(unsigned char)(v>>8);           break;
        }
    }
    else
    {
        unsigned char *a = p + (x / 4) * 6;
        switch (x % 4) {
            case 0: a[0]=(a[0]&0x0F)|(unsigned char)(v<<4); a[1]=(unsigned char)(v>>4);                       break;
            case 1: a[0]=(a[0]&0xF0)|(unsigned char)(v>>8); a[3]=(unsigned char)v;                            break;
            case 2: a[2]=(unsigned char)(v>>4);             a[5]=(a[5]&0x0F)|(unsigned char)(v<<4);           break;
            case 3: a[4]=(unsigned char)v;                  a[5]=(a[5]&0xF0)|(unsigned char)(v>>8);           break;
        }
    }
}

//-------------------------------------------------------------------
// Lanes and mixing.
//
// lanes is a mask over the eight data lanes of the frame buffer's memory bus,
// not over the ADC bits - after packing those two are not the same thing, and
// pretending they were would make the mask do something different on every
// pixel phase. In the pixel-domain effects at the bottom of the file it is
// used as a bit-plane mask instead, over the top eight planes, because there
// the value is unpacked and planes are what exist.

static unsigned char bx_lanes(const bendx_t *x)
{
    return x->lanes ? x->lanes : 0xff;
}

static unsigned char bx_mixb(unsigned char old, unsigned char nw, int mix)
{
    switch (mix)
    {
    case BXMIX_XOR: return (unsigned char)(old ^ nw);
    case BXMIX_OR:  return (unsigned char)(old | nw);
    case BXMIX_AND: return (unsigned char)(old & nw);
    default:        return nw;
    }
}

static unsigned char bx_merge(unsigned char old, unsigned char nw,
                              unsigned char lanes, int mix)
{
    unsigned char v = bx_mixb(old, nw, mix);
    return (unsigned char)((old & ~lanes) | (v & lanes));
}

// Same, in the value domain.
static unsigned bx_mixv(unsigned old, unsigned nw, unsigned mask, int mix)
{
    unsigned v;
    switch (mix)
    {
    case BXMIX_XOR: v = old ^ nw; break;
    case BXMIX_OR:  v = old | nw; break;
    case BXMIX_AND: v = old & nw; break;
    default:        v = nw;       break;
    }
    return (old & ~mask) | (v & mask);
}

// Plane mask for the pixel-domain effects. 0 means every plane, which is the
// default and the only setting most profiles ever want.
static unsigned bx_planes(const bendx_t *x, int nbits)
{
    unsigned all = (1u << nbits) - 1u;
    if (!x->lanes) return all;
    return ((unsigned)x->lanes << (nbits - 8)) & all;
}

// Row banding. rowmod 0 and 1 mean every row; otherwise the effect is on for
// rowmod rows and off for rowmod rows, so a profile can be left visibly
// interleaved with the clean picture instead of covering it.
static int bx_on(const bendx_t *x, unsigned y)
{
    unsigned m = x->rowmod;
    if (m < 2) return 1;
    return (y % (m * 2u)) < m;
}

//-------------------------------------------------------------------
// Ranges. Both inclusive, and 0 means the knob does nothing for this effect.

int bendx_amount_max(int kind)
{
    switch (kind)
    {
    case BX_ENDIAN:   return 4;     // which reordering
    case BX_SLIP:     return 15;    // bits, minus one
    case BX_TEAR:     return 15;    // bits per row, minus one
    case BX_ADDR:     return 11;    // which row address line
    case BX_COLADDR:  return 11;    // which column address line
    case BX_STRIDE:   return 63;    // bytes of stride error, minus one
    case BX_REFRESH:  return 15;    // severity
    case BX_ECHO:     return 63;    // delay in bytes, minus one
    case BX_LINEMEM:  return 63;    // period, minus two
    case BX_FIELD:    return 63;    // block height, minus two
    case BX_ROMBUS:   return 7;     // texture scale, as a shift
    case BX_CLOCK:    return 15;    // rate error
    case BX_CFA:      return 7;     // pixels across
    case BX_CLAMP:    return 15;    // clamp gain
    case BX_BLOOM:    return 15;    // how low the overflow threshold sits
    case BX_SORT:     return 15;    // threshold the runs are taken above
    case BX_LOCK:     return 15;    // how often lock is lost
    case BX_JPGBUS:   return 7;     // texture scale, as a shift
    }
    return 0;
}

int bendx_reach_max(int kind)
{
    switch (kind)
    {
    case BX_SLIP:     return 32;    // whole bytes on top of the bit rotation
    case BX_TEAR:     return 32;    // starting offset
    case BX_STRIDE:   return 15;    // coarse multiplier on the stride error
    case BX_ECHO:     return 15;    // coarse multiplier on the delay
    case BX_LINEMEM:  return 15;    // how many rows are held
    case BX_ROMBUS:   return 15;    // how fast the ROM window walks
    case BX_CLOCK:    return 7;     // wobble period, as a shift
    case BX_CFA:      return 7;     // rows down
    case BX_CLAMP:    return 15;    // how far away the wrong OB row is
    case BX_BLOOM:    return 15;    // how far the smear carries down
    case BX_SORT:     return 15;    // run length, and which way up - see below
    case BX_LOCK:     return 15;    // how far it jumps when it slips
    case BX_JPGBUS:   return 15;    // how fast the window walks
    }
    return 0;
}

int bendx_is_slow(int kind)
{
    switch (kind)
    {
    case BX_CLOCK: case BX_CFA:   case BX_CLAMP:
    case BX_BLOOM: case BX_SORT:
        return 1;
    }
    return 0;
}

int bendx_needs_env(int kind)
{
    return (kind == BX_ROMBUS || kind == BX_JPGBUS);
}

//-------------------------------------------------------------------
// How much working space an effect wants. Two rows unless it says otherwise.

#define BX_SORT_BUCKETS     4096u   // one per 12 bit value

unsigned bendx_scratch_bytes(int kind, unsigned rowlen)
{
    unsigned two = rowlen * BENDX_SCRATCH_ROWS;

    if (kind == BX_SORT)
    {
        // A counting sort over the whole value range. At 10bpp two rows are
        // 7880 bytes and this is 8192, so the default is not enough and the
        // caller has to be told - which is the entire reason this function
        // exists rather than a constant.
        unsigned hist = BX_SORT_BUCKETS * sizeof(unsigned short);
        return (hist > two) ? hist : two;
    }
    return two;
}

unsigned bendx_chain_scratch_bytes(const bendx_chain_t *c, unsigned rowlen)
{
    unsigned most = rowlen * BENDX_SCRATCH_ROWS;
    int i, n = bendx_chain_count(c);

    for (i = 0; i < n; i++)
    {
        unsigned want = bendx_scratch_bytes(c->item[i].kind, rowlen);
        if (want > most) most = want;
    }
    return most;
}

const char *bendx_name(int kind)
{
    if (kind < 0 || kind >= BX_COUNT) kind = BX_OFF;
    return bx_names[kind];
}

const char *bendx_desc(int kind)
{
    if (kind < 0 || kind >= BX_COUNT) kind = BX_OFF;
    return bx_descs[kind];
}

//-------------------------------------------------------------------

void bendx_reset(bendx_t *x)
{
    x->kind   = BX_OFF;
    x->amount = 0;
    x->reach  = 0;
    x->lanes  = 0;
    x->mix    = BXMIX_SET;
    x->rowmod = 0;
    x->pad[0] = 0;
    x->pad[1] = 0;
    x->seed   = 0xb1a5edu;
}

int bendx_is_off(const bendx_t *x)
{
    return x->kind == BX_OFF || x->kind >= BX_COUNT;
}

// Change the effect, and start its knobs somewhere worth looking at.
//
// The knobs have to be reset: they are one pair of bytes that means "eleven
// bits of slip" under one effect and "address line eleven" under the next, so
// carrying them over is a setting that changed itself. Both UIs go through
// here so that they cannot disagree about that.
//
// The mix does not reset to Replace for the foreign buses. At Replace and full
// lanes, ROM bus and the two display buses overwrite every byte of the frame -
// the photograph is not damaged, it is gone, and there is nothing on the back
// of the camera to suggest which knob to turn to get it back. XOR keeps the
// picture and puts the foreign buffer through it, which is the effect people
// mean when they ask for this one; Replace is still there, one item down the
// menu, for when it is what you want.
void bendx_set_kind(bendx_t *x, int kind)
{
    unsigned keep = x->seed;

    if (kind < 0 || kind >= BX_COUNT) kind = BX_OFF;

    bendx_reset(x);
    x->seed = keep ? keep : x->seed;
    x->kind = (unsigned char)kind;
    if (bendx_needs_env(kind)) x->mix = BXMIX_XOR;

    bendx_sanitize(x);
}

void bendx_sanitize(bendx_t *x)
{
    int m;

    if (x->kind >= BX_COUNT) { bendx_reset(x); return; }

    m = bendx_amount_max(x->kind);
    if (x->amount > m) x->amount = (unsigned char)m;
    m = bendx_reach_max(x->kind);
    if (x->reach > m) x->reach = (unsigned char)m;

    if (x->mix >= BXMIX_COUNT) x->mix = BXMIX_SET;
    if (x->rowmod > 64)        x->rowmod = 0;
    if (!x->seed)              x->seed = 0xb1a5edu;
}

// The amount in the effect's own units, because "7" means seven different
// things across this list and the picker has room to say which.
void bendx_label(const bendx_t *x, char *buf)
{
    static const char * const endian[] = { "byte", "word", "grp", "nib", "rev" };
    unsigned v, div;
    int i = 0;

    buf[0] = 0;
    switch (x->kind)
    {
    case BX_OFF:
        return;

    case BX_ENDIAN:
        {
            const char *s = endian[(x->amount <= 4) ? x->amount : 0];
            while (*s) buf[i++] = *s++;
            buf[i] = 0;
        }
        return;

    case BX_SLIP:     v = (unsigned)x->amount + 1u + (unsigned)x->reach * 8u;  break;
    case BX_TEAR:     v = (unsigned)x->amount + 1u;                            break;
    case BX_ADDR:
    case BX_COLADDR:  v = 1u << x->amount;                                     break;
    case BX_STRIDE:
    case BX_ECHO:     v = (unsigned)x->amount + 1u + (unsigned)x->reach * 64u; break;
    case BX_LINEMEM:
    case BX_FIELD:    v = (unsigned)x->amount + 2u;                            break;
    default:          v = (unsigned)x->amount;                                 break;
    }

    // No sprintf here: this file is shared with the host test and with a
    // redraw path, and four digits do not justify pulling in the formatter.
    for (div = 1u; v / div >= 10u; div *= 10u) ;
    for (; div; div /= 10u) buf[i++] = (char)('0' + (v / div) % 10u);
    buf[i] = 0;
}

//-------------------------------------------------------------------
// The chain. See the layout note in bendx.h: item[0..n-1] are locked and
// item[n] is live, so the two are one list and "lock it" is n++.

void bendx_chain_reset(bendx_chain_t *c)
{
    int i;
    for (i = 0; i < BENDX_CHAIN_MAX; i++) bendx_reset(&c->item[i]);
    c->n = 0;
    c->pad[0] = c->pad[1] = c->pad[2] = 0;
}

void bendx_chain_sanitize(bendx_chain_t *c)
{
    int i;

    if (c->n > BENDX_CHAIN_MAX) c->n = 0;
    for (i = 0; i < BENDX_CHAIN_MAX; i++) bendx_sanitize(&c->item[i]);

    // A locked slot holding Off is a hole, and a hole makes the position
    // markers in the picker lie about what runs when. Close them here rather
    // than defending against them at every use: this is the only place the
    // invariant "item[0..n-1] are all live effects" is established, and it is
    // reachable from a config block written by any earlier build.
    for (i = 0; i < (int)c->n; )
    {
        if (bendx_is_off(&c->item[i])) bendx_chain_remove(c, i);
        else                           i++;
    }
}

int bendx_chain_count(const bendx_chain_t *c)
{
    int n = c->n;
    if (n < BENDX_CHAIN_MAX && !bendx_is_off(&c->item[n])) n++;
    return n;
}

bendx_t *bendx_chain_live(bendx_chain_t *c)
{
    return (c->n < BENDX_CHAIN_MAX) ? &c->item[c->n] : 0;
}

const bendx_t *bendx_chain_live_const(const bendx_chain_t *c)
{
    return (c->n < BENDX_CHAIN_MAX) ? &c->item[c->n] : 0;
}

int bendx_chain_find(const bendx_chain_t *c, int kind)
{
    int i, n = bendx_chain_count(c);

    if (kind == BX_OFF) return -1;
    for (i = 0; i < n; i++)
        if (c->item[i].kind == kind) return i;
    return -1;
}

int bendx_chain_lock(bendx_chain_t *c)
{
    if (c->n >= BENDX_CHAIN_MAX)      return 0;     // nothing left to open
    if (bendx_is_off(&c->item[c->n])) return 0;     // nothing live to freeze

    c->n++;
    // The slot that just became live has whatever an earlier chain left in it.
    if (c->n < BENDX_CHAIN_MAX) bendx_reset(&c->item[c->n]);
    return 1;
}

int bendx_chain_remove(bendx_chain_t *c, int i)
{
    int k;

    if (i < 0 || i >= BENDX_CHAIN_MAX) return 0;

    if (i >= (int)c->n)
    {
        // The live slot. Nothing to close up - it just goes back to Off.
        if (i == (int)c->n) { bendx_reset(&c->item[i]); return 1; }
        return 0;
    }

    // Shift the rest down, live slot included, so tuning is not disturbed by
    // removing something above it.
    for (k = i; k < BENDX_CHAIN_MAX - 1; k++) c->item[k] = c->item[k + 1];
    bendx_reset(&c->item[BENDX_CHAIN_MAX - 1]);
    c->n--;
    return 1;
}

int bendx_chain_slow(const bendx_chain_t *c)
{
    int i, n = bendx_chain_count(c);
    for (i = 0; i < n; i++)
        if (bendx_is_slow(c->item[i].kind)) return 1;
    return 0;
}

//-------------------------------------------------------------------
// Reroll the knobs, never the effect. The effect is the thing you chose; the
// knobs are the thing you are hunting through.

unsigned bendx_random(bendx_t *x, unsigned seed)
{
    unsigned s = xs32(seed);
    int am = bendx_amount_max(x->kind);
    int rm = bendx_reach_max(x->kind);

    x->seed = s;

    if (am > 0) { s = xs32(s); x->amount = (unsigned char)(s % (unsigned)(am + 1)); }
    if (rm > 0) { s = xs32(s); x->reach  = (unsigned char)(s % (unsigned)(rm + 1)); }

    // Full lanes most of the time. A partial lane mask is a good effect and a
    // bad default: three quarters of the masks make the damage too quiet to
    // see on the back of the camera, which is where it gets judged.
    s = xs32(s);
    x->lanes = (s % 100u < 70u) ? 0 : (unsigned char)(s >> 8);

    s = xs32(s);
    if (bendx_needs_env(x->kind))
    {
        // Never Replace on a foreign bus, for the reason in bendx_set_kind():
        // it does not damage the photograph, it removes it. A reroll that can
        // land on "no picture at all" is a reroll people stop pressing.
        x->mix = (unsigned char)(BXMIX_XOR + s % (BXMIX_COUNT - 1));
    }
    else if (s % 100u < 55u) x->mix = BXMIX_SET;
    else                     x->mix = (unsigned char)(s % BXMIX_COUNT);

    s = xs32(s);
    x->rowmod = (s % 100u < 70u) ? 0 : (unsigned char)(1 + s % 32u);

    bendx_sanitize(x);
    return s;
}

//-------------------------------------------------------------------
// Byte-domain effects.
//
// All of these walk the packed bytes in place, one row at a time, with at most
// two rows of scratch. That is one linear pass over the buffer - the same
// order of work the pure path in raw.c does - which is why they are the ones
// that can be left on.

// Rotate a row left by n bits, wrapping. The unit of the fault is the bit,
// because that is what a read that started early recovers: the bytes are
// intact and the boundaries are not.
// Optional UI service callback, called from inside the per-row loops below.
//
// Each profile is one pass over the whole sensor and there was no service point
// anywhere inside it, so an experimental chain held the display task for the
// entire pass - measured on the A470 at ~14s, which is exactly how long the
// persistent overlay stayed off the screen after a shot. The bend engine in
// core/raw.c already services every 64 rows for the same reason; this gives the
// experimental engine the same courtesy.
//
// Kept as a setter rather than a member of bendx_env_t so that the host
// self-tests, which build their own env on the stack, cannot leave it holding a
// stack value. Null until a caller sets it, which is what the self-tests get.
static void (*bx_service)(void);

void bendx_set_service(void (*fn)(void))
{
    bx_service = fn;
}

// Cheap enough to sit in every row loop: one test against a null pointer, and
// the call itself only every 64th row.
#define BX_TICK(y) do { if (bx_service && (((y) & 0x3f) == 0)) bx_service(); } while (0)

static void bx_rot(unsigned char *dst, const unsigned char *src,
                   unsigned len, unsigned bits)
{
    unsigned bo, bi, i;

    if (!len) return;
    bits %= len * 8u;
    bo = bits >> 3;
    bi = bits & 7u;

    if (!bi)
    {
        for (i = 0; i < len; i++) dst[i] = src[(i + bo) % len];
        return;
    }
    for (i = 0; i < len; i++)
    {
        unsigned a = src[(i + bo) % len];
        unsigned b = src[(i + bo + 1u) % len];
        dst[i] = (unsigned char)(((a << bi) | (b >> (8u - bi))) & 0xff);
    }
}

static void bx_blend_row(unsigned char *row, const unsigned char *nw,
                         unsigned len, unsigned char lanes, int mix)
{
    unsigned i;
    for (i = 0; i < len; i++) row[i] = bx_merge(row[i], nw[i], lanes, mix);
}

static void bx_endian(const bendx_t *x, const bendx_buf_t *b,
                      unsigned char *s0, unsigned char *s1)
{
    unsigned char lanes = bx_lanes(x);
    unsigned gsz = bx_gsz(b->nbits);
    unsigned len = b->rowlen, y, i;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        if (!bx_on(x, y)) continue;

        for (i = 0; i < len; i++) s0[i] = row[i];

        switch (x->amount)
        {
        case 0:     // adjacent bytes swapped - a halfword latched backwards
            for (i = 0; i + 1 < len; i += 2)
            { s1[i] = s0[i+1]; s1[i+1] = s0[i]; }
            if (len & 1) s1[len-1] = s0[len-1];
            break;

        case 1:     // whole word reversed - the wrong endianness outright
            for (i = 0; i + 3 < len; i += 4)
            { s1[i]=s0[i+3]; s1[i+1]=s0[i+2]; s1[i+2]=s0[i+1]; s1[i+3]=s0[i]; }
            for (; i < len; i++) s1[i] = s0[i];
            break;

        case 2:     // the packed pixel group reversed, so the phases invert
            for (i = 0; i + gsz <= len; i += gsz)
            {
                unsigned k;
                for (k = 0; k < gsz; k++) s1[i+k] = s0[i + gsz - 1 - k];
            }
            for (; i < len; i++) s1[i] = s0[i];
            break;

        case 3:     // nibbles swapped - half a byte lane out of step
            for (i = 0; i < len; i++)
                s1[i] = (unsigned char)((s0[i] << 4) | (s0[i] >> 4));
            break;

        default:    // every byte bit-reversed - the lanes wired back to front
            for (i = 0; i < len; i++)
            {
                unsigned v = s0[i];
                v = ((v & 0xaa) >> 1) | ((v & 0x55) << 1);
                v = ((v & 0xcc) >> 2) | ((v & 0x33) << 2);
                v = ((v & 0xf0) >> 4) | ((v & 0x0f) << 4);
                s1[i] = (unsigned char)v;
            }
            break;
        }

        bx_blend_row(row, s1, len, lanes, x->mix);
    }
}

static void bx_slip(const bendx_t *x, const bendx_buf_t *b,
                    unsigned char *s0, unsigned char *s1)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen, y, i;
    unsigned bits = (unsigned)x->amount + 1u + (unsigned)x->reach * 8u;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        if (!bx_on(x, y)) continue;
        for (i = 0; i < len; i++) s0[i] = row[i];
        bx_rot(s1, s0, len, bits);
        bx_blend_row(row, s1, len, lanes, x->mix);
    }
}

static void bx_tear(const bendx_t *x, const bendx_buf_t *b,
                    unsigned char *s0, unsigned char *s1)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen, y, i;
    unsigned per = (unsigned)x->amount + 1u;
    unsigned bits = (unsigned)x->reach * 8u;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        bits += per;                    // grows whether or not the row is on,
                                        // so banding cuts the tear instead of
                                        // pausing it
        if (!bx_on(x, y)) continue;
        for (i = 0; i < len; i++) s0[i] = row[i];
        bx_rot(s1, s0, len, bits);
        bx_blend_row(row, s1, len, lanes, x->mix);
    }
}

// A stuck address line does not corrupt data, it fetches the wrong data: the
// two rows whose numbers differ only in that bit become the same row, or -
// since both halves of the pair are driven - trade places.
static void bx_addr(const bendx_t *x, const bendx_buf_t *b, unsigned char *s0)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned step = 1u << x->amount;
    unsigned y, i;

    if (step >= b->rows) return;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *ra, *rb;
        unsigned y2 = y | step;

        if (y & step) continue;         // handled as the low half of its pair
        if (y2 >= b->rows) continue;
        if (!bx_on(x, y)) continue;

        ra = b->base + (unsigned)y  * len;
        rb = b->base + (unsigned)y2 * len;
        for (i = 0; i < len; i++) s0[i] = ra[i];
        for (i = 0; i < len; i++)
        {
            unsigned char a = s0[i], c = rb[i];
            ra[i] = bx_merge(a, c, lanes, x->mix);
            rb[i] = bx_merge(c, a, lanes, x->mix);
        }
    }
}

// The same fault on the other axis. The unit is the packed group rather than
// the byte, so the blocks that trade places land on pixel boundaries and the
// result reads as displaced image rather than as noise.
static void bx_coladdr(const bendx_t *x, const bendx_buf_t *b)
{
    unsigned char lanes = bx_lanes(x);
    unsigned gsz = bx_gsz(b->nbits);
    unsigned len = b->rowlen;
    unsigned ngroups = len / gsz;
    unsigned step = 1u << x->amount;
    unsigned y, g, k;

    if (step >= ngroups) return;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        if (!bx_on(x, y)) continue;

        for (g = 0; g < ngroups; g++)
        {
            unsigned char *ga, *gb;
            unsigned g2 = g | step;

            if (g & step) continue;
            if (g2 >= ngroups) continue;

            ga = row + g  * gsz;
            gb = row + g2 * gsz;
            for (k = 0; k < gsz; k++)
            {
                unsigned char a = ga[k], c = gb[k];
                ga[k] = bx_merge(a, c, lanes, x->mix);
                gb[k] = bx_merge(c, a, lanes, x->mix);
            }
        }
    }
}

// Stride is the one number the DMA engine needs that the picture does not
// contain. Get it wrong by d bytes and row y is fetched from d*y bytes into
// the frame - the image shears, and keeps shearing, and eventually wraps.
//
// Rows are rewritten in ascending order and every source lies at or after the
// row being written, so the source is still untouched when it is read. Once
// the offset wraps past the end the source is a row this pass already bent,
// which is the feedback a real runaway stride has and is wanted.
static void bx_stride(const bendx_t *x, const bendx_buf_t *b, unsigned char *s0)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned total = len * b->rows;
    unsigned d = (unsigned)x->amount + 1u + (unsigned)x->reach * 64u;
    unsigned y, i;

    if (!total) return;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        // y * (len + d) cannot overflow 32 bits: the largest sensor here is
        // 2772 rows of 5580 bytes and d tops out at 1024.
        unsigned src = (y * (len + d)) % total;

        if (!bx_on(x, y)) continue;

        if (src + len <= total)
            for (i = 0; i < len; i++) s0[i] = b->base[src + i];
        else
        {
            unsigned head = total - src;
            for (i = 0; i < head; i++)     s0[i] = b->base[src + i];
            for (; i < len; i++)           s0[i] = b->base[i - head];
        }
        bx_blend_row(row, s0, len, lanes, x->mix);
    }
}

// Refresh is the only thing keeping a DRAM cell at the value it was written.
// Miss it and cells drift to whichever rail their sense amp favours - and the
// longer since the write, the more of them have gone, which is why this gets
// worse down the frame rather than being uniform.
static void bx_refresh(const bendx_t *x, const bendx_buf_t *b)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned y, i;
    unsigned sev = (unsigned)x->amount + 1u;
    int to_one = (x->mix == BXMIX_OR);

    if (!b->rows) return;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        unsigned st = xs32(x->seed ^ (y * 0x9e3779b9u));
        // 0 at the top of the frame, sev/16 of all cells by the bottom.
        // Divided before the multiply so the whole thing stays in 32 bits.
        unsigned thr = (sev * ((y * 0x10000u) / b->rows)) / 16u;

        if (!bx_on(x, y)) continue;

        for (i = 0; i < len; i++)
        {
            st = xs32(st);
            if ((st & 0xffffu) >= thr) continue;
            row[i] = to_one ? (unsigned char)(row[i] | lanes)
                            : (unsigned char)(row[i] & ~lanes);
        }
    }
}

// A bus that is not terminated reflects, and what comes back arrives late and
// lands on top of what is being driven now. The delay reaches back past the
// start of the row into the one before it, because the scan does not stop at
// the row boundary and neither does the reflection.
static void bx_echo(const bendx_t *x, const bendx_buf_t *b,
                    unsigned char *s0, unsigned char *s1)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned d = (unsigned)x->amount + 1u + (unsigned)x->reach * 64u;
    unsigned char *cur = s0, *prev = s1;
    int have_prev = 0;
    unsigned y, i;

    if (d > len) d = len;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        unsigned char *t;

        for (i = 0; i < len; i++) cur[i] = row[i];

        if (bx_on(x, y))
        {
            for (i = 0; i < len; i++)
            {
                unsigned char src;
                if (i >= d)            src = cur[i - d];
                else if (have_prev)    src = prev[len + i - d];
                else                   continue;   // nothing has been driven yet
                row[i] = bx_merge(row[i], src, lanes, x->mix);
            }
        }

        // cur holds this row as it arrived, which is what the next row's
        // reflection has to see - not the version we just wrote over it.
        t = prev; prev = cur; cur = t;
        have_prev = 1;
    }
}

// Line memory is one row deep and it is what everything downstream reads. Stop
// clocking it and it keeps presenting the last row it latched, for as long as
// the fault lasts.
static void bx_linemem(const bendx_t *x, const bendx_buf_t *b)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned period = (unsigned)x->amount + 2u;
    unsigned hold   = (unsigned)x->reach + 1u;
    unsigned y, i;

    if (hold >= period) hold = period - 1u;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row, *src;
        unsigned pos = y % period;
        unsigned sy;

        if (pos < period - hold) continue;      // still clocking normally
        if (!bx_on(x, y)) continue;

        sy = y - pos + (period - hold) - 1u;    // the last row it latched
        row = b->base + (unsigned)y  * len;
        src = b->base + (unsigned)sy * len;
        for (i = 0; i < len; i++) row[i] = bx_merge(row[i], src[i], lanes, x->mix);
    }
}

// The two fields of an interlaced read arrive in an order the timing generator
// decides. Invert the phase and the block comes back bottom-up; at a block of
// two that is the classic odd/even swap, and at larger blocks it is what a
// field buffer read backwards looks like.
static void bx_field(const bendx_t *x, const bendx_buf_t *b, unsigned char *s0)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned blk = (unsigned)x->amount + 2u;
    unsigned y0, k, i;

    for (y0 = 0; y0 < b->rows; y0 += blk)
    {
        unsigned n = (y0 + blk <= b->rows) ? blk : (b->rows - y0);

        if (!bx_on(x, y0)) continue;

        for (k = 0; k < n / 2u; k++)
        {
            unsigned char *ra = b->base + (unsigned)(y0 + k)         * len;
            unsigned char *rb = b->base + (unsigned)(y0 + n - 1 - k) * len;
            for (i = 0; i < len; i++) s0[i] = ra[i];
            for (i = 0; i < len; i++)
            {
                unsigned char a = s0[i], c = rb[i];
                ra[i] = bx_merge(a, c, lanes, x->mix);
                rb[i] = bx_merge(c, a, lanes, x->mix);
            }
        }
    }
}

//-------------------------------------------------------------------
// Foreign buses.
//
// Something else in the camera driving the frame buffer's data lanes. On a
// bench this is a probe left on the wrong pad; here it is a second buffer read
// straight through, at whatever address it happens to live.
//
// Read-only on the far side, always, and bounded by the length the caller
// declared. raw.c derives all three lengths from accessors the port already
// has, so this file never contains an address.

// A flat slab of memory read straight through. Used for the program ROM and
// for the JPEG encoder's buffer, which want identical treatment and produce
// completely different pictures: one is ARM code and Canon's string tables,
// the other is entropy-coded image data, and they look nothing like each other
// on the data lanes.
static void bx_membus(const bendx_t *x, const bendx_buf_t *b,
                      const unsigned char *mem, unsigned mem_len)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned shift = x->amount;             // repeat each byte 2^shift
    unsigned span  = (len >> shift) + 2u;   // source bytes one row consumes
    unsigned walk  = ((unsigned)x->reach + 1u) * 61u;
    unsigned window, off, y, i;

    if (!mem || mem_len <= span) return;

    window = mem_len - span;
    off = x->seed % window;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        const unsigned char *r = mem + (off + (unsigned)y * walk) % window;

        if (!bx_on(x, y)) continue;
        for (i = 0; i < len; i++)
            row[i] = bx_merge(row[i], r[i >> shift], lanes, x->mix);
    }
}

//-------------------------------------------------------------------
// Pixel-domain effects.
//
// These unpack, so they cost what the bussed path in raw.c costs - which on
// these bodies is the difference between a shot that saves and a shot you wait
// out. bendx_is_slow() reports it and the picker marks it; none of them can be
// expressed on the packed bytes, because the packing is not in scan order.

// The pixel clock is what says where one pixel ends and the next begins. Run
// it at the wrong rate against a fixed line length and the row stretches or
// compresses, and since the rate is never wrong by a constant, it wobbles.
static void bx_clock(const bendx_t *x, const bendx_buf_t *b, unsigned char *s0)
{
    unsigned mask = bx_planes(x, b->nbits);
    unsigned len = b->rowlen;
    unsigned err = ((unsigned)x->amount + 1u) * 128u;
    unsigned period = 1u << (x->reach + 2);
    unsigned npix = bx_wholepix(b);
    unsigned y, i;

    if (!npix) return;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        unsigned ratio;
        unsigned pos;

        if (!bx_on(x, y)) continue;

        // Triangle over `period` rows, so the rate drifts one way and back
        // rather than jumping - a PLL hunting, not a PLL switched. period is
        // 1 << (reach + 2), so its half is never zero.
        pos = y % period;
        if (pos >= period / 2u) pos = period - pos - 1u;
        ratio = 65536u + (err * pos * 2u) / (period / 2u);

        for (i = 0; i < len; i++) s0[i] = row[i];

        for (i = 0; i < npix; i++)
        {
            // i * ratio stays inside 32 bits: 3720 pixels by at most 67584.
            unsigned sx = (i * ratio) >> 16;
            unsigned v;
            sx %= npix;                 // the row folds back rather than
                                        // smearing its last pixel
            v = bendx_get(s0, sx, b->nbits);
            bendx_set(row, i, bx_mixv(bendx_get(s0, i, b->nbits), v, mask, x->mix),
                      b->nbits);
        }
    }
}

// Canon's demosaic assumes a fixed Bayer phase. Move the image under it by an
// odd number of pixels or rows and every red site is read as green - the
// geometry is untouched and the colour is destroyed, which is a thing only a
// CFA can do.
static void bx_cfa(const bendx_t *x, const bendx_buf_t *b, unsigned char *s0)
{
    unsigned mask = bx_planes(x, b->nbits);
    unsigned len = b->rowlen;
    unsigned dx = x->amount, dy = x->reach;
    unsigned npix = bx_wholepix(b);
    unsigned y, i;

    if (!dx && !dy) dx = 1;             // a phase shift of nothing is not one
    if (!npix || !b->rows) return;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        unsigned sy = y + dy;
        const unsigned char *src;

        if (!bx_on(x, y)) continue;

        // Ascending, and the source is always at or below us, so it has not
        // been rewritten yet. Past the last row there is nothing to read, so
        // the bottom dy rows keep their own phase.
        if (sy >= b->rows) continue;
        if (dy == 0)
        {
            for (i = 0; i < len; i++) s0[i] = row[i];
            src = s0;
        }
        else
            src = b->base + (unsigned)sy * len;

        for (i = 0; i < npix; i++)
        {
            unsigned sx = (i + dx) % npix;
            unsigned v = bendx_get(src, sx, b->nbits);
            bendx_set(row, i, bx_mixv(bendx_get(row, i, b->nbits), v, mask, x->mix),
                      b->nbits);
        }
    }
}

// The clamp loop measures the masked columns at the start of each row and
// subtracts what it finds, which is how a CCD's dark current is kept out of
// the picture. Feed it the measurement from a different row and it corrects
// for a row that is not there - so the correction becomes the signal, and it
// bands, and the banding tracks real dark current rather than a pattern.
static void bx_clamp(const bendx_t *x, const bendx_buf_t *b,
                     const bendx_env_t *env)
{
    unsigned mask = bx_planes(x, b->nbits);
    unsigned len = b->rowlen;
    unsigned vmax = (1u << b->nbits) - 1u;
    unsigned white = env->white ? env->white : vmax;
    unsigned gain = (unsigned)x->amount + 1u;
    unsigned away = ((unsigned)x->reach + 1u) * 8u;
    unsigned npix = bx_wholepix(b);
    unsigned y, i;

    if (b->ob_x >= npix || !b->rows) return;
    if (white > vmax) white = vmax;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;
        unsigned oy = (y + away) % b->rows;
        unsigned ob = bendx_get(b->base + (unsigned)oy * len, b->ob_x, b->nbits);
        int corr;

        if (!bx_on(x, y)) continue;

        corr = (int)((ob * gain) >> 3) - (int)env->black;

        for (i = 0; i < npix; i++)
        {
            unsigned v = bendx_get(row, i, b->nbits);
            int d = (int)v - corr;
            if (d < 0) d = 0;
            if (d > (int)white) d = (int)white;
            bendx_set(row, i, bx_mixv(v, (unsigned)d, mask, x->mix), b->nbits);
        }
    }
}

//-------------------------------------------------------------------
// Sensor physics.
//
// Everything above this point breaks something between the sensor and the
// JPEG. Bloom breaks the sensor and is what you see on an old CCD pointed at
// something bright.

// Blooming.
//
// A well that fills past capacity spills into the vertical transfer register,
// and everything clocked through that register afterwards carries the spill.
// On a CCD that is the white column standing out of every specular highlight -
// the single most recognisable "this is not a CMOS sensor" artefact there is.
//
// One accumulator per column, carried down the frame, which fits in the two
// rows of scratch every effect already gets: a column short is two bytes and
// there are fewer pixels in a row than there are bytes in two of them.
static void bx_bloom(const bendx_t *x, const bendx_buf_t *b, unsigned char *s)
{
    unsigned mask = bx_planes(x, b->nbits);
    unsigned len = b->rowlen;
    unsigned npix = bx_wholepix(b);
    unsigned vmax = (1u << b->nbits) - 1u;
    unsigned short *acc = (unsigned short *)(void *)s;
    // Threshold walks down from the top of the range, so a low amount spills
    // only from what is genuinely clipped and a high one from anything bright.
    unsigned thr = vmax - ((vmax * (unsigned)x->amount) / 20u);
    unsigned decay = 200u + (unsigned)x->reach * 3u;    // /256 kept per row
    unsigned y, i;

    if (!npix) return;
    if (decay > 255u) decay = 255u;
    for (i = 0; i < npix; i++) acc[i] = 0;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;

        for (i = 0; i < npix; i++)
        {
            unsigned v = bendx_get(row, i, b->nbits);
            unsigned a = acc[i];
            unsigned o;

            if (v > thr) a += (v - thr);
            else         a = (a * decay) >> 8;
            if (a > vmax) a = vmax;
            acc[i] = (unsigned short)a;

            // The accumulator is carried whether or not this row is in scope,
            // so banding cuts holes in the smear rather than resetting it.
            if (!bx_on(x, y)) continue;

            o = v + a;
            if (o > vmax) o = vmax;
            bendx_set(row, i, bx_mixv(v, o, mask, x->mix), b->nbits);
        }
    }
}

//-------------------------------------------------------------------
// Processes.
//
// Pixel sorting is not a hardware failure. It is here because it looks good,
// and the honest place to say so is here rather than in a hardware story
// invented to justify it.
//
// Both are Bayer-aware in a way the fault effects are not, and for a reason
// that is not aesthetic: a fault moves charge or bits, and the mosaic goes
// along for the ride. A process moves *pixels*, and moving a pixel one place
// puts a red value on a green site - so Canon's demosaic, which runs after us
// and assumes a fixed phase, turns the result into colour mud rather than into
// the effect. It therefore moves in steps of two, which is the distance
// between neighbours of the same colour.

// Pixel sorting.
//
// Runs of pixels above a threshold, put in order along the row. Two passes per
// row, one per Bayer phase, stepping two at a time so red only ever sorts with
// red - see the note above.
//
// Counting sort, not insertion sort. Insertion is what every pixel sort is
// written with and it is quadratic; on a 3720 pixel row over 2772 rows that is
// thirteen billion comparisons, which is not slow, it is never. Counting sort
// is one pass to tally and one to write back, and the value range is only 4096
// wide because that is what the sensor produces.
static void bx_sort(const bendx_t *x, const bendx_buf_t *b, unsigned char *s)
{
    unsigned mask = bx_planes(x, b->nbits);
    unsigned len = b->rowlen;
    unsigned npix = bx_wholepix(b);
    unsigned vmax = (1u << b->nbits) - 1u;
    unsigned short *hist = (unsigned short *)(void *)s;
    unsigned thr = (vmax * (unsigned)x->amount) / 18u;
    // The low three bits set the shortest run worth sorting; the top bit turns
    // the sort over. One byte, two knobs, because there is no third field and
    // a sort that only runs one way up is half an effect.
    unsigned minrun = (((unsigned)x->reach & 7u) + 1u) * 8u;
    int down = (x->reach >= 8);
    unsigned phase, y, i;

    if (npix < 4) return;
    for (i = 0; i < BX_SORT_BUCKETS; i++) hist[i] = 0;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;

        if (!bx_on(x, y)) continue;

        for (phase = 0; phase < 2; phase++)
        {
            unsigned i0 = phase;

            while (i0 < npix)
            {
                unsigned i1, n, lo, hi, v, w;

                // Find a run of same-phase pixels that are all above the
                // threshold. That mask is where nearly all of the look comes
                // from - sorting everything just gives a gradient.
                while (i0 < npix && bendx_get(row, i0, b->nbits) <= thr) i0 += 2;
                if (i0 >= npix) break;
                for (i1 = i0; i1 < npix && bendx_get(row, i1, b->nbits) > thr; i1 += 2) ;

                n = (i1 - i0) / 2u;
                if (n < minrun) { i0 = i1 + 2; continue; }

                lo = vmax; hi = 0;
                for (i = i0; i < i1; i += 2)
                {
                    v = bendx_get(row, i, b->nbits);
                    hist[v]++;
                    if (v < lo) lo = v;
                    if (v > hi) hi = v;
                }

                // Write back in order, and clear only the buckets this run
                // touched - clearing all 4096 per run is what would make this
                // quadratic again by the back door.
                w = i0;
                if (!down)
                {
                    for (v = lo; v <= hi; v++)
                        while (hist[v]--)
                        { unsigned o = bendx_get(row, w, b->nbits);
                          bendx_set(row, w, bx_mixv(o, v, mask, x->mix), b->nbits); w += 2; }
                }
                else
                {
                    for (v = hi; ; v--)
                    {
                        while (hist[v]--)
                        { unsigned o = bendx_get(row, w, b->nbits);
                          bendx_set(row, w, bx_mixv(o, v, mask, x->mix), b->nbits); w += 2; }
                        if (v == lo) break;
                    }
                }
                // hist[v]-- leaves every touched bucket at 0xffff, not 0.
                for (v = lo; v <= hi; v++) hist[v] = 0;

                i0 = i1 + 2;
            }
        }
    }
}

//-------------------------------------------------------------------
// Timing losing lock.
//
// BX_SLIP is a constant offset and BX_TEAR is one that grows. This is the
// third thing a clock recovery loop does when it cannot hold: it sits locked
// for a while, slips to somewhere else, holds there, slips again. Discrete,
// unpredictable, and stable in between - which reads completely differently
// from either of the other two even though all three rotate a row.
static void bx_lock(const bendx_t *x, const bendx_buf_t *b,
                    unsigned char *s0, unsigned char *s1)
{
    unsigned char lanes = bx_lanes(x);
    unsigned len = b->rowlen;
    unsigned rate = (unsigned)x->amount + 1u;       // slips per hundred rows
    unsigned span = ((unsigned)x->reach + 1u) * 8u; // how far it lands
    unsigned st = xs32(x->seed);
    unsigned bits = 0;
    unsigned y, i;

    for (y = 0; y < b->rows; y++)
    {
        BX_TICK(y);
        unsigned char *row = b->base + (unsigned)y * len;

        st = xs32(st);
        if (st % 100u < rate)
        {
            st = xs32(st);
            bits = st % (span + 1u);
        }

        if (!bx_on(x, y) || !bits) continue;
        for (i = 0; i < len; i++) s0[i] = row[i];
        bx_rot(s1, s0, len, bits);
        bx_blend_row(row, s1, len, lanes, x->mix);
    }
}

//-------------------------------------------------------------------

void bendx_apply(const bendx_t *x, const bendx_buf_t *buf,
                 const bendx_env_t *env, unsigned char *scratch)
{
    unsigned char *s0, *s1;

    if (!x || !buf || !env || !scratch) return;
    if (!buf->base || !buf->rowlen || !buf->rows) return;
    if (buf->nbits != 10 && buf->nbits != 12) return;
    if (bendx_is_off(x)) return;

    s0 = scratch;
    s1 = scratch + buf->rowlen;

    switch (x->kind)
    {
    case BX_ENDIAN:  bx_endian(x, buf, s0, s1);                             break;
    case BX_SLIP:    bx_slip(x, buf, s0, s1);                               break;
    case BX_TEAR:    bx_tear(x, buf, s0, s1);                               break;
    case BX_ADDR:    bx_addr(x, buf, s0);                                   break;
    case BX_COLADDR: bx_coladdr(x, buf);                                    break;
    case BX_STRIDE:  bx_stride(x, buf, s0);                                 break;
    case BX_REFRESH: bx_refresh(x, buf);                                    break;
    case BX_ECHO:    bx_echo(x, buf, s0, s1);                               break;
    case BX_LINEMEM: bx_linemem(x, buf);                                    break;
    case BX_FIELD:   bx_field(x, buf, s0);                                  break;
    case BX_ROMBUS:  bx_membus(x, buf, env->rom, env->rom_len);             break;
    case BX_CLOCK:   bx_clock(x, buf, s0);                                  break;
    case BX_CFA:     bx_cfa(x, buf, s0);                                    break;
    case BX_CLAMP:   bx_clamp(x, buf, env);                                 break;
    case BX_BLOOM:   bx_bloom(x, buf, s0);                                  break;
    case BX_SORT:    bx_sort(x, buf, s0);                                   break;
    case BX_LOCK:    bx_lock(x, buf, s0, s1);                               break;
    case BX_JPGBUS:  bx_membus(x, buf, env->jpg, env->jpg_len);             break;
    default: break;
    }
}

#else

// The image recipe module is shared by every camera and imports these four
// read-only helpers through the global module export table. Cameras without
// the experimental engine still need resolvable exports even though they can
// neither run nor restore a profile chain. Report the only state those builds
// support: experimental off.
const char *bendx_name(int kind)
{
    (void)kind;
    return "Off";
}

void bendx_label(const bendx_t *x, char *buf)
{
    (void)x;
    buf[0] = 0;
}

void bendx_chain_sanitize(bendx_chain_t *c)
{
    c->n = 0;
}

int bendx_chain_count(const bendx_chain_t *c)
{
    (void)c;
    return 0;
}

#endif // CAM_BEND_EXPERIMENTAL
