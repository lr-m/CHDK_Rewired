#include "platform.h"

// Returning 0 here makes vid_get_viewport_active_buffer() fall back to the fixed
// address in vid_get_viewport_fb(). That is survivable for readers - the
// histogram and motion detect just get a frame that is sometimes one refresh
// stale - but it is fatal for anything that WRITES into the picture, because the
// live view cycles through several buffers and most of what you write is never
// displayed. Symptom is a picture that flickers at odd rates instead of holding.
//
// current_viewport_buffer (stubs_min.S, 0x3e2b4) is Canon's own record of which
// buffer is on screen: the accessor at 0xffcc24d0 in ImgDDev.c is just
// "return *(0x3e2b4)", and the firmware itself calls it and walks a table
// comparing against the result to recover the buffer index (loop at 0xffca0a68).
void *vid_get_viewport_live_fb()
{
    extern char *current_viewport_buffer;
    unsigned a = (unsigned)current_viewport_buffer;

    // Return 0 rather than a wild pointer if the display is not up yet - the
    // caller already knows how to fall back.
    if ((a & ~0x10000000u) < 0x00100000 || (a & ~0x10000000u) > 0x02000000)
        return (void*)0;

    // Force the uncached alias. Writes must reach memory for the display
    // controller to see them; a cached alias leaves them sitting in D-cache.
    return (void*)(a | 0x10000000u);
}

void *vid_get_viewport_fb_d()
{
    //return (void*)(*(int*)0x5228);  // same as 100e, eg FFC44B58
	// http://chdk.setepontos.com/index.php/topic,2361.msg27125.html#msg27125
	// sub_FFC45328
	return (void*)(*(int*)(0x50A4+0x4C));  //might wanna check this, found above ImagePlayer.c
}

// The JPEG encoder's working buffer, for the experimental engine's JPEG bus -
// see include/bendx.h and docs/EXPERIMENTAL_EFFECTS.md. Read only, and read
// inside the raw hook with the imaging pipeline stopped, so what it holds is
// the previous shot's compressed data.
//
// From the imaging buffer table recovered from the firmware, and worth stating
// why it is trusted rather than just found. Three things agree:
//
//   CRAW_BUFF from the same table is 0x10f06b20, which is the address
//     stubs_entry.S already records for this body's single RAW buffer
//   CRAW_BUFF_SIZE is 0x8d0a68 = 3152 x 2346 x 10bpp exactly, so the raw
//     buffer is unpadded and its end is 0x117d7588
//   this region starts 120 bytes above that end and finishes at 0x12000000
//     exactly, so it neither overlaps the raw buffer nor runs past the region
//
// Same shape of argument as the A480's, in platform/a480/sub/100b/lib.c.
char *hook_jpeg_buffer(unsigned *len)
{
    *len = 0x828a00;
    return (char*) 0x117d7600;
}

char *camera_jpeg_count_str()
{
    return (char*)0x49A80; // found above a9999
}
