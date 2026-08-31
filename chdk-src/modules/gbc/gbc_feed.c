//-------------------------------------------------------------------
// The Canon sensor feeding the Nintendo sensor.
//
// gbc_cam.c asks for a 128x120 plane of 8-bit luminance whenever the ROM
// trips the shutter; this fills it from the A480's live viewport.
//
// Format. The A480 viewport is UYVYYY - YUV 4:1:1, six bytes per four pixels,
// 720x480 for a 0x7e900 frame (docs/LIVEVIEW_NOTES.md). Luma sits at byte
// offsets 1, 3, 4 and 5 of each six-byte group:
//
//      byte   0   1   2   3   4   5
//             U   Y0  V   Y1  Y2  Y3
//
// Only luma is read. The Game Boy Camera is a greyscale device and chroma
// would be thrown away by the dither stage anyway, so this never pays for a
// colour conversion.
//
// Aspect. The viewport is 4:3 and the sensor plane is 128x120. Sampling the
// full frame would squash faces horizontally, which is exactly the sort of
// thing that looks like a bug rather than a filter, so this centre-crops the
// source to the sensor's aspect and scales that.
//-------------------------------------------------------------------

#include "gbc_cam.h"
#include "viewport.h"
#include "camera_info.h"

// Ceilings for the geometry the viewport accessors report. The A480's live
// buffer is 720x480 (docs/LIVEVIEW_NOTES.md, 0x7e900 = 720*480*3/2), so these
// are generous for this body and still small enough that a nonsense value
// cannot turn into an out-of-bounds read.
#define GBC_FEED_MAX_STRIDE     4096
#define GBC_FEED_MAX_ROWS       1024

//-------------------------------------------------------------------
// Luma byte offset within a 6-byte UYVYYY group, indexed by pixel-in-group.
static const unsigned char uyvyyy_luma_off[4] = { 1, 3, 4, 5 };

// Why the last capture returned what it did, for the HUD. A cartridge that is
// photographing flat grey and one that is photographing the room look
// identical from the emulator's side, so this is the only way to tell whether
// the sensor is actually feeding it.
int gbc_feed_status = GBC_FEED_NEVER;

//-------------------------------------------------------------------

int gbc_feed_capture(unsigned char *dst)
{
    const unsigned char *vid;
    int stride, rows, src_w;
    int crop_w, crop_h, crop_x, crop_y;
    int x, y;

    // Playback mode is not a live feed.
    //
    // vid_get_viewport_active_buffer() hands back vid_get_viewport_fb_d() when
    // the camera is in PLAY mode - the playback display buffer, whose size need
    // not match the geometry vid_get_viewport_byte_width()/height() report for
    // the live viewport. Indexing one with the other's dimensions walks off the
    // end of the allocation, and the generic wrapper also documents that it can
    // return NULL outright in playback. There is nothing to photograph in
    // playback anyway, so the sensor gets flat grey instead.
    if (camera_info.state.mode_play)
    {
        gbc_feed_status = GBC_FEED_PLAYMODE;
        return -1;
    }

    // vid_get_viewport_active_buffer() rather than vid_get_viewport_live_fb():
    // only the former is in modules/exportlist.inc, and a module that calls an
    // unexported symbol fails at elf2flt time rather than at compile time.
    vid = (const unsigned char *)vid_get_viewport_active_buffer();

    // Some ports never implemented this and return 0 - that was a real bug on
    // the a470 (LIVEVIEW_NOTES.md), so it is worth checking rather than
    // trusting.
    if (!vid)
    {
        gbc_feed_status = GBC_FEED_NOBUF;
        return -1;
    }

    stride = vid_get_viewport_byte_width();
    rows   = (int)vid_get_viewport_height() * vid_get_viewport_yscale();

    // Sanity-check the reported geometry before trusting it as a stride.
    // These are read from the port's own accessors, but this code runs inside
    // the emulated cartridge's shutter and a bad multiply here is an
    // out-of-bounds read of whatever follows the viewport in memory - which is
    // exactly the kind of thing that takes the camera down rather than
    // producing a wrong picture.
    if (stride < 6 || stride > GBC_FEED_MAX_STRIDE ||
        rows < 1 || rows > GBC_FEED_MAX_ROWS)
    {
        gbc_feed_status = GBC_FEED_BADGEOM;
        return -1;
    }

    // Four pixels per six bytes.
    src_w = (stride / 6) * 4;
    if (src_w < 4)
    {
        gbc_feed_status = GBC_FEED_BADGEOM;
        return -1;
    }

    //---------------------------------------------------------------
    // Centre-crop to the sensor's 128:120 aspect.

    crop_w = src_w;
    crop_h = (src_w * GBCAM_SENSOR_H) / GBCAM_SENSOR_W;
    if (crop_h > rows)
    {
        crop_h = rows;
        crop_w = (rows * GBCAM_SENSOR_W) / GBCAM_SENSOR_H;
        if (crop_w > src_w)
            crop_w = src_w;
    }
    // Correct for the viewport's non-square pixels.
    //
    // Everything above treats a viewport pixel as square. It is not. The A480
    // reports 720x480 (docs/LIVEVIEW_NOTES.md) - a 1.5:1 buffer - but what it
    // shows is a 4:3 scene, so each viewport pixel is 3/4 * 480/720 = 8/9 as
    // wide as it is tall.
    //
    // Ignoring that, a crop of 512x480 looks like the sensor's 128:120 on paper
    // but is really 0.948:1 once displayed. Resampling it into the square
    // 128x120 sensor plane stretches the scene horizontally by 9/8 - 12.5% -
    // which is subtle at a 160x144 blit and obvious once the screen is scaled
    // to full height.
    //
    // The horizontal correction is src_w*3 / (rows*4): the amount by which the
    // buffer is wider than the 4:3 image it represents. On this body that turns
    // the 512-wide crop into the 576 it should always have been.
    //
    // Derived from the geometry the port reports rather than hardcoded, so a
    // body with a square-pixel viewport gets a factor of 1 and is unaffected.
    {
        int corrected = (crop_w * src_w * 3) / (rows * 4);

        if (corrected > src_w)
        {
            // Cannot widen any further - narrow the height instead, so the
            // aspect is right and the crop still fits.
            crop_h = (crop_h * src_w) / corrected;
            corrected = src_w;
        }
        crop_w = corrected;
    }

    crop_x = (src_w - crop_w) / 2;
    crop_y = (rows  - crop_h) / 2;

    //---------------------------------------------------------------
    // Nearest-neighbour down-sample. The source is ~720x480 and the target is
    // 128x120, so this is a ~6:1 reduction; a box filter would be sharper but
    // this runs inside the ROM's capture wait and the sensor's own dithering
    // hides the difference.

    // Column byte offsets, computed once.
    //
    // This loop used to divide inside it - `(x * crop_w) / GBCAM_SENSOR_W` per
    // pixel, which is 128*120 = 15360 divisions per exposure. ARM946 has no
    // divide instruction, so every one was a call into __aeabi_uidiv, and the
    // Game Boy Camera's viewfinder takes exposures continuously. Hoisting it
    // into a 128-entry table costs 128 divisions instead of 15360.
    {
        static short col_off[GBCAM_SENSOR_W];
        static short row_off[GBCAM_SENSOR_H];

        for (x = 0; x < GBCAM_SENSOR_W; x++)
        {
            int sx  = crop_x + (x * crop_w) / GBCAM_SENSOR_W;
            int off = (sx >> 2) * 6 + uyvyyy_luma_off[sx & 3];
            if (off >= stride)          // cannot happen; cheap to be sure
                off = stride - 1;
            col_off[x] = (short)off;
        }

        for (y = 0; y < GBCAM_SENSOR_H; y++)
            row_off[y] = (short)(crop_y + (y * crop_h) / GBCAM_SENSOR_H);

        for (y = 0; y < GBCAM_SENSOR_H; y++)
        {
            const unsigned char *row = vid + (unsigned)row_off[y] * stride;
            unsigned char *out = dst + y * GBCAM_SENSOR_W;

            for (x = 0; x < GBCAM_SENSOR_W; x++)
                out[x] = row[col_off[x]];
        }
    }

    gbc_feed_status = GBC_FEED_OK;
    return 0;
}
