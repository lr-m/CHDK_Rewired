// Multiple exposure - the maths.
//
// See include/mexp.h for what this is and for where the rest of it lives. This
// file holds no camera address, no CHDK header and no state that outlives a
// sequence, which is what lets tools/mexp_selftest.c run it on the host. The
// same split the bend engine uses: core/bend.c is the model, core/raw.c is the
// applier.

#include "mexp.h"

//-------------------------------------------------------------------
// State

int mexp_count    = 0;
int mexp_target   = MEXP_FRAMES_MIN;
int mexp_mode_used = MEXP_AVERAGE;
int mexp_bend_each_used = 1;

void mexp_begin(int mode, int frames, int bend_each)
{
    if (mode < 0 || mode >= MEXP_MODE_COUNT) mode = MEXP_AVERAGE;
    if (frames < MEXP_FRAMES_MIN) frames = MEXP_FRAMES_MIN;
    if (frames > MEXP_FRAMES_MAX) frames = MEXP_FRAMES_MAX;

    // The mode and the frame count are copied out of the config here and read
    // from these copies for the rest of the sequence. Changing either half way
    // through would otherwise mean an accumulator built one way and finished
    // another, and for the average mode it would mean dividing by a count the
    // pixels in the file were never scaled for.
    mexp_mode_used = mode;
    mexp_bend_each_used = !!bend_each;
    mexp_target    = frames;
    mexp_count     = 0;
}

void mexp_reset(void)
{
    mexp_count = 0;
}

int mexp_in_progress(void)
{
    return mexp_count > 0 && mexp_count < mexp_target;
}

//-------------------------------------------------------------------
// Blending

unsigned short mexp_blend_px(int mode, unsigned int acc, unsigned int v,
                             unsigned int n, unsigned int black, unsigned int white)
{
    unsigned int r;

    switch (mode)
    {
    case MEXP_LIGHTEN:
        r = (v > acc) ? v : acc;
        break;

    case MEXP_DARKEN:
        r = (v < acc) ? v : acc;
        break;

    case MEXP_ADD:
        // Sum of the light, not of the numbers. A sensor reading is the light
        // plus a black-level pedestal that every exposure carries, so adding
        // two readings adds the pedestal twice; subtracting one back is what
        // makes two exposures of a dark scene stay dark instead of drifting
        // grey. The same correction raw_merge.c makes when it sums files.
        r = acc + v;
        r = (r > black) ? (r - black) : 0;
        break;

    case MEXP_AVERAGE:
    default:
        // acc already holds the mean of n exposures, so the mean of n+1 is
        // (acc*n + v)/(n+1). Running it this way rather than summing and
        // dividing at the end is what keeps the accumulator in the sensor's
        // own packed format and the file the same size as a raw - a sum of
        // nine 12-bit exposures needs 16 bits a pixel and a third more card.
        //
        // The cost is that each step rounds. The error is under half an LSB
        // per exposure and does not compound - the +((n+1)/2) rounds to
        // nearest rather than towards zero, so the drift has no sign to build
        // up in. MEXP_FRAMES_MAX is where that stops being true enough to
        // ignore, not where the storage runs out.
        r = (acc * n + v + ((n + 1) / 2)) / (n + 1);
        break;
    }

    if (r > white) r = white;
    return (unsigned short)r;
}

void mexp_row_blend(int mode, unsigned short *acc, const unsigned short *v,
                    unsigned int rowpix, unsigned int n,
                    unsigned int black, unsigned int white)
{
    unsigned int i;

    // The mode test is hoisted out of the pixel loop. It is constant for a
    // whole sequence, and this loop runs once per sensor pixel per exposure -
    // ten million times a shot on the a480, on a core with no branch
    // prediction. The bodies are what mexp_blend_px does, kept in step with
    // it by tools/mexp_selftest.c, which checks every mode against it.
    switch (mode)
    {
    case MEXP_LIGHTEN:
        for (i = 0; i < rowpix; i++)
            if (v[i] > acc[i]) acc[i] = v[i];
        break;

    case MEXP_DARKEN:
        for (i = 0; i < rowpix; i++)
            if (v[i] < acc[i]) acc[i] = v[i];
        break;

    case MEXP_ADD:
        for (i = 0; i < rowpix; i++)
        {
            unsigned int r = (unsigned int)acc[i] + (unsigned int)v[i];
            r = (r > black) ? (r - black) : 0;
            acc[i] = (unsigned short)((r > white) ? white : r);
        }
        break;

    case MEXP_AVERAGE:
    default:
        {
            unsigned int half = (n + 1) / 2;
            for (i = 0; i < rowpix; i++)
            {
                unsigned int r = ((unsigned int)acc[i] * n + (unsigned int)v[i] + half) / (n + 1);
                acc[i] = (unsigned short)((r > white) ? white : r);
            }
        }
        break;
    }
}

//-------------------------------------------------------------------
// Packed rows
//
// The bit layouts are get_raw_pixel()/set_raw_pixel() in core/raw.c, taken a
// group at a time. A group is the smallest run of pixels that occupies a whole
// number of bytes: eight pixels in ten bytes at 10bpp, four in six at 12, and
// eight in fourteen at 14.

void mexp_row_unpack(int bits, const unsigned char *src, unsigned short *dst, unsigned int rowpix)
{
    unsigned int i;

    if (bits == 10)
    {
        unsigned int groups = rowpix >> 3;
        for (i = 0; i < groups; i++, src += 10, dst += 8)
        {
            dst[0] = (unsigned short)((0x3fc & (src[1] << 2)) | (src[0] >> 6));
            dst[1] = (unsigned short)((0x3f0 & (src[0] << 4)) | (src[3] >> 4));
            dst[2] = (unsigned short)((0x3c0 & (src[3] << 6)) | (src[2] >> 2));
            dst[3] = (unsigned short)((0x300 & (src[2] << 8)) | (src[5]));
            dst[4] = (unsigned short)((0x3fc & (src[4] << 2)) | (src[7] >> 6));
            dst[5] = (unsigned short)((0x3f0 & (src[7] << 4)) | (src[6] >> 4));
            dst[6] = (unsigned short)((0x3c0 & (src[6] << 6)) | (src[9] >> 2));
            dst[7] = (unsigned short)((0x300 & (src[9] << 8)) | (src[8]));
        }
    }
    else if (bits == 12)
    {
        unsigned int groups = rowpix >> 2;
        for (i = 0; i < groups; i++, src += 6, dst += 4)
        {
            dst[0] = (unsigned short)(((unsigned)src[1] << 4) | (src[0] >> 4));
            dst[1] = (unsigned short)(((unsigned)(src[0] & 0x0f) << 8) | src[3]);
            dst[2] = (unsigned short)(((unsigned)src[2] << 4) | (src[5] >> 4));
            dst[3] = (unsigned short)(((unsigned)(src[5] & 0x0f) << 8) | src[4]);
        }
    }
    else if (bits == 14)
    {
        unsigned int groups = rowpix >> 3;
        for (i = 0; i < groups; i++, src += 14, dst += 8)
        {
            dst[0] = (unsigned short)(((unsigned)src[ 1] << 6) | (src[ 0] >> 2));
            dst[1] = (unsigned short)(((unsigned)(src[ 0] & 0x03) << 12) | ((unsigned)src[ 3] << 4) | (src[ 2] >> 4));
            dst[2] = (unsigned short)(((unsigned)(src[ 2] & 0x0f) << 10) | ((unsigned)src[ 5] << 2) | (src[ 4] >> 6));
            dst[3] = (unsigned short)(((unsigned)(src[ 4] & 0x3f) <<  8) | (src[ 7]));
            dst[4] = (unsigned short)(((unsigned)src[ 6] << 6) | (src[ 9] >> 2));
            dst[5] = (unsigned short)(((unsigned)(src[ 9] & 0x03) << 12) | ((unsigned)src[ 8] << 4) | (src[11] >> 4));
            dst[6] = (unsigned short)(((unsigned)(src[11] & 0x0f) << 10) | ((unsigned)src[10] << 2) | (src[13] >> 6));
            dst[7] = (unsigned short)(((unsigned)(src[13] & 0x3f) <<  8) | (src[12]));
        }
    }
}

void mexp_row_pack(int bits, const unsigned short *src, unsigned char *dst, unsigned int rowpix)
{
    unsigned int i;

    if (bits == 10)
    {
        unsigned int groups = rowpix >> 3;
        for (i = 0; i < groups; i++, src += 8, dst += 10)
        {
            dst[0] = (unsigned char)(((src[0] & 0x003) << 6) | ((src[1] >> 4) & 0x3f));
            dst[1] = (unsigned char)((src[0] >> 2) & 0xff);
            dst[2] = (unsigned char)(((src[2] & 0x03f) << 2) | ((src[3] >> 8) & 0x03));
            dst[3] = (unsigned char)(((src[1] & 0x00f) << 4) | ((src[2] >> 6) & 0x0f));
            dst[4] = (unsigned char)((src[4] >> 2) & 0xff);
            dst[5] = (unsigned char)(src[3] & 0xff);
            dst[6] = (unsigned char)(((src[5] & 0x00f) << 4) | ((src[6] >> 6) & 0x0f));
            dst[7] = (unsigned char)(((src[4] & 0x003) << 6) | ((src[5] >> 4) & 0x3f));
            dst[8] = (unsigned char)(src[7] & 0xff);
            dst[9] = (unsigned char)(((src[6] & 0x03f) << 2) | ((src[7] >> 8) & 0x03));
        }
    }
    else if (bits == 12)
    {
        unsigned int groups = rowpix >> 2;
        for (i = 0; i < groups; i++, src += 4, dst += 6)
        {
            dst[0] = (unsigned char)(((src[0] & 0x00f) << 4) | ((src[1] >> 8) & 0x0f));
            dst[1] = (unsigned char)((src[0] >> 4) & 0xff);
            dst[2] = (unsigned char)((src[2] >> 4) & 0xff);
            dst[3] = (unsigned char)(src[1] & 0xff);
            dst[4] = (unsigned char)(src[3] & 0xff);
            dst[5] = (unsigned char)(((src[2] & 0x00f) << 4) | ((src[3] >> 8) & 0x0f));
        }
    }
    else if (bits == 14)
    {
        unsigned int groups = rowpix >> 3;
        for (i = 0; i < groups; i++, src += 8, dst += 14)
        {
            dst[ 0] = (unsigned char)(((src[0] & 0x03f) << 2) | ((src[1] >> 12) & 0x03));
            dst[ 1] = (unsigned char)((src[0] >> 6) & 0xff);
            dst[ 2] = (unsigned char)(((src[1] & 0x00f) << 4) | ((src[2] >> 10) & 0x0f));
            dst[ 3] = (unsigned char)((src[1] >> 4) & 0xff);
            dst[ 4] = (unsigned char)(((src[2] & 0x003) << 6) | ((src[3] >>  8) & 0x3f));
            dst[ 5] = (unsigned char)((src[2] >> 2) & 0xff);
            dst[ 6] = (unsigned char)((src[4] >> 6) & 0xff);
            dst[ 7] = (unsigned char)(src[3] & 0xff);
            dst[ 8] = (unsigned char)((src[5] >> 4) & 0xff);
            dst[ 9] = (unsigned char)(((src[4] & 0x03f) << 2) | ((src[5] >> 12) & 0x03));
            dst[10] = (unsigned char)((src[6] >> 2) & 0xff);
            dst[11] = (unsigned char)(((src[5] & 0x00f) << 4) | ((src[6] >> 10) & 0x0f));
            dst[12] = (unsigned char)(src[7] & 0xff);
            dst[13] = (unsigned char)(((src[6] & 0x003) << 6) | ((src[7] >>  8) & 0x3f));
        }
    }
}

//-------------------------------------------------------------------
// Ghost tone mapping

int mexp_ghost_level(unsigned int v, unsigned int black, unsigned int white)
{
    unsigned int range, t, i;

    // Level boundaries, in thousandths of the range, at the midpoints between
    // the five display greys after a gamma of 2 - ((i-0.5)/4)^2. Straight
    // linear thresholds put four fifths of an ordinary daylight frame in the
    // bottom level, which draws the ghost as a black silhouette and makes the
    // overlay useless for lining a second exposure up, which is the one job
    // it has.
    static const unsigned short bound[MEXP_GHOST_LEVELS - 1] = { 16, 141, 391, 766 };

    if (white <= black) return 0;
    range = white - black;

    if (v <= black) return 0;
    v -= black;
    if (v >= range) return MEXP_GHOST_LEVELS - 1;

    t = (v * 1000) / range;

    for (i = 0; i < MEXP_GHOST_LEVELS - 1; i++)
        if (t < bound[i]) return (int)i;

    return MEXP_GHOST_LEVELS - 1;
}
