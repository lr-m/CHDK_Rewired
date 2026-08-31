#include "platform.h"

char *hook_raw_image_addr()
{
    return (char*)0x105BA490;
}

void *vid_get_viewport_live_fb()
{
    return (void*)0;
}

void *vid_get_bitmap_fb()
{
    return (void*)(0x10361000);
}

void *vid_get_viewport_fb()
{
    return (void*)0x105F3170; //0x1055B370;
}

void *vid_get_viewport_fb_d()
{
    return (void*)(*(int*)0x65b2c);
}


extern int _GetVRAMHPixelsSize();
extern int _GetVRAMVPixelsSize();

long vid_get_viewport_height()
{
    if(camera_info.state.mode_play) {
        return 240;
    }
    return _GetVRAMVPixelsSize();
}

int vid_get_viewport_height_proper() {
    return vid_get_viewport_height();
}


int vid_get_viewport_width() {
    if(camera_info.state.mode_play) {
        return 360;
    }
    return _GetVRAMHPixelsSize() >> 1;
}


char *camera_jpeg_count_str()
{
    return (char*)0x71200;
}

// PTP display stuff
int vid_get_palette_type() { return 1; }
int vid_get_palette_size() { return 16*4; }

void *vid_get_bitmap_active_palette() {
    return (void *)0x65520; // GetPaletteFromPhysicalScreen -> sub_ffd30aec
}

void *vid_get_bitmap_active_buffer()
{
    return (void*)(*(int*)0x5fe4); // ffd308d4
}

int vid_get_viewport_fullscreen_height() {
    // except for stitch, always full screen
    if(!camera_info.state.mode_play && camera_info.state.mode_shooting == MODE_STITCH) {
        return 240;
    }
    return vid_get_viewport_height_proper();
}
int vid_get_viewport_fullscreen_width() {
    // except for stitch, always full screen
    if(!camera_info.state.mode_play && camera_info.state.mode_shooting == MODE_STITCH) {
        return 720;
    }
    return vid_get_viewport_width_proper();
}

int vid_get_viewport_display_xoffset() {
    if(camera_info.state.mode_play) {
        return 0;
    } else if(camera_info.state.mode_shooting == MODE_STITCH) {
        short dir=0;
        short seq=0;
        get_property_case(PROPCASE_STITCH_DIRECTION,&dir,sizeof(dir));
        get_property_case(PROPCASE_STITCH_SEQUENCE,&seq,sizeof(seq));
        // guessed based on D10
        if(dir==0) {
            return seq?135:45;
        } else {
            return seq?45:135;
        }
    }
    return 0;
}

int vid_get_viewport_display_yoffset() {
    if(camera_info.state.mode_play) {
        return 0;
    }
    return (camera_info.state.mode_shooting == MODE_STITCH)?60:0; // window is 120, centered in 240 screen
}

