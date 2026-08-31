#include "camera_info.h"
#include "conf.h"
#include "modes.h"
#include "viewport.h"
#include "ptp_chdk.h"
#include "live_view.h"

// Live View implementation for all cameras using YUV or 8bpp palette based bitmap buffer
// Viewport = YUV buffer
// Bitmap = 8bpp buffer or YUV buffer (Digic6)
// Palette = Color table (Non Digic 6)

typedef struct
{
    lv_data_header      lv;
    lv_framebuffer_desc vp;
    lv_framebuffer_desc bm;
#ifdef THUMB_FW
    lv_framebuffer_desc bmo;
#endif
} lv_hdr;

/*
send selected data for live view
returns 0 on error, total size on success
should only be called from ptp handler
*/
int live_view_get_data(ptp_data *data, int flags)
{
    lv_hdr lv;

    // Header
    lv.lv.version_major = LIVE_VIEW_VERSION_MAJOR;
    lv.lv.version_minor = LIVE_VIEW_VERSION_MINOR;

    lv.lv.lcd_aspect_ratio = vid_get_aspect_ratio();

    lv.lv.vp_desc_start = sizeof(lv_data_header);
    lv.lv.bm_desc_start = lv.lv.vp_desc_start+sizeof(lv_framebuffer_desc);
#ifdef THUMB_FW
    lv.lv.bmo_desc_start = lv.lv.bm_desc_start+sizeof(lv_framebuffer_desc);
#else
    lv.lv.bmo_desc_start = 0;
#endif

    // update camera_screen values to increase chance they will be consistent
    // with values obtained directly from fw functions
    mode_get();

    // Viewport
    lv.vp.fb_type = vid_get_viewport_type();

    lv.vp.buffer_width = vid_get_viewport_buffer_width_proper();

    lv.vp.visible_width = vid_get_viewport_width_proper();
    lv.vp.visible_height = vid_get_viewport_height_proper();

    lv.vp.margin_left = vid_get_viewport_display_xoffset_proper();
    lv.vp.margin_top = vid_get_viewport_display_yoffset_proper();

    // TODO returning margins from lib.c might be better
    // can end up with negative if size and offset sources don't update at exactly the same time
    lv.vp.margin_right = vid_get_viewport_fullscreen_width() - lv.vp.visible_width - lv.vp.margin_left;
    lv.vp.margin_bot = vid_get_viewport_fullscreen_height() - lv.vp.visible_height - lv.vp.margin_top;

    // Bitmap
    // TODO maybe should be function like viewport
#ifdef THUMB_FW
    lv.bm.fb_type = LV_FB_YUV8B;
#else
    lv.bm.fb_type = LV_FB_PAL8;
#endif

    lv.bm.buffer_width = CAMERA_SCREEN_BM_BUFFER_WIDTH;
    lv.bm.visible_width = CAMERA_SCREEN_BM_PHYS_WIDTH;
    lv.bm.visible_height = CAMERA_SCREEN_BM_HEIGHT;

    lv.bm.margin_left = lv.bm.margin_right = lv.bm.margin_top = lv.bm.margin_bot = 0;

#ifdef THUMB_FW
    // Bitmap opacity
    lv.bmo.fb_type = LV_FB_OPACITY8;
    lv.bmo.buffer_width = lv.bm.buffer_width;

    lv.bmo.visible_width = lv.bm.visible_width;
    lv.bmo.visible_height = lv.bm.visible_height;

    lv.bmo.margin_left = lv.bmo.margin_right = lv.bmo.margin_top = lv.bmo.margin_bot = 0;
#endif

    // Calc sizes and get buffer pointers
    int total_size = sizeof(lv_hdr);

#ifndef THUMB_FW
    int pal_size = 0;
    char *pal_data = NULL;
    // determine if we will send palette so it can go in one send
    if ( flags & LV_TFR_PALETTE ) // bitmap palette
    {
        // if no palette defined in port, size will be set to zero
        pal_size = vid_get_palette_size();
        if (pal_size)
        {
            // palette data may be NULL or a small offset while switching outputs
            // or due to port bugs or other reasons
            pal_data = vid_get_bitmap_active_palette();
            if(pal_data > (char *)0x1000) {
                lv.lv.palette_type = vid_get_palette_type();
                lv.lv.palette_data_start = total_size;
                total_size += pal_size;
            } else {
                pal_data = 0;
                pal_size = 0;
            }
        }
        if (!pal_size) {
            lv.lv.palette_data_start = lv.lv.palette_type = 0;
        }
    }
    else
    {
        lv.lv.palette_data_start = lv.lv.palette_type = 0;
    }
#else
    lv.lv.palette_data_start = lv.lv.palette_type = 0;
#endif

    int vp_size = 0;
    void *vp_fb = vid_get_viewport_active_buffer();
    // Add viewport details if requested, and not null
    if ( (flags & LV_TFR_VIEWPORT) && vp_fb) // live buffer
    {
        lv.vp.data_start = total_size;
#ifndef THUMB_FW
        vp_size = (lv.vp.buffer_width*lv.vp.visible_height*6)/4;
#else
        vp_size = (lv.vp.buffer_width*lv.vp.visible_height*4)/2;
#endif
        total_size += vp_size;
        // offset to start of actual data
        vp_fb += vid_get_viewport_image_offset();
    }
    else
    {
        lv.vp.data_start = 0;
    }

    int bm_size = 0;
    // Add bitmap details if requested
    if ( flags & LV_TFR_BITMAP ) // bitmap buffer
    {
        lv.bm.data_start = total_size;
#ifndef THUMB_FW
        bm_size = lv.bm.buffer_width*lv.bm.visible_height;
#else
        bm_size = (lv.bm.buffer_width*lv.bm.visible_height*4)/2;
#endif
        total_size += bm_size;
    }
    else
    {
        lv.bm.data_start = 0;
    }

#ifdef THUMB_FW
    int bmo_size = 0;
    // Add bitmap opacity if thumb2 and request
    if ( flags & LV_TFR_BITMAP_OPACITY ) // bitmap opacity buffer
    {
        lv.bmo.data_start = total_size;
        bmo_size = lv.bmo.buffer_width*lv.bmo.visible_height;
        total_size += bmo_size;
    }
    else
    {
        lv.bmo.data_start = 0;
    }
#endif

    // Send header structure (along with total size to be sent)
    data->send_data(data->handle,(char*)&lv,sizeof(lv_hdr),total_size,0,0,0);

#ifndef THUMB_FW
    // Send palette data if requested
    if (pal_size)
    {
        data->send_data(data->handle,pal_data,pal_size,0,0,0,0);
    }
#endif

    // Send viewport data if requested
    if ( vp_size )
    {
        data->send_data(data->handle,vp_fb,vp_size,0,0,0,0);
    }

    // Send bitmap data if requested
    if ( bm_size )
    {
        data->send_data(data->handle,vid_get_bitmap_active_buffer(),bm_size,0,0,0,0);
    }

#ifdef THUMB_FW
    // Send bitmap opacity data if requested
    if ( bmo_size )
    {
        data->send_data(data->handle,vid_get_opacity_active_buffer(),bmo_size,0,0,0,0);
    }
#endif

    return total_size;
}
