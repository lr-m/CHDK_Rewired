#include "platform.h"

char *hook_raw_image_addr()
{
	return (char*) 0x10C5FA60; // "CRAW BUFF"
}

// JPEG encoder working buffer - see raw_buffer.h and
// docs/EXPERIMENTAL_EFFECTS.md.
//
// Both numbers come from the same place the raw buffer address above does: the
// startup routine at 0xffe09f?? that logs "JPEG BUFF %p (%lx)" alongside
// "CRAW BUFF %p" and "CRAW BUFF SIZE %p". Its literal pool holds
//
//   JPEG_BUFF       0x11b2e800   size 0x4d1800
//   IMG_VRAM_BUFF   0x106a8b00
//   THUM_VRAM_BUFF  0x10489740
//   CRAW_BUFF       0x10c5fa60   size 0x00ec04f0
//
// and the CRAW pair checks out exactly against 5580 x 2772, which is what
// makes the rest of the table worth trusting. The region sits above the raw
// buffer's end at 0x11b1ff50, so the two do not overlap.
//
// Read only, and read inside the raw hook with the pipeline stopped, so what
// it holds is the previous shot's compressed data - entropy coded, which is
// why it looks nothing like the ROM on the data lanes.
char *hook_jpeg_buffer(unsigned *len)
{
	*len = 0x4d1800;
	return (char*) 0x11b2e800;
}

void *vid_get_viewport_live_fb()
{
    void **fb=(void **)0x3E80;
    unsigned char buff = *((unsigned char*)0x3CF0); // sub_FFC87F0C
    if (buff == 0) buff = 2;  else buff--;    
    return fb[buff];
}

void *vid_get_bitmap_fb()
{
	return (void*)0x10361000; // "BmpDDev.c"
}

void *vid_get_viewport_fb()
{
	return (void*)0x10659EC0;  // "VRAM Address"
}

void *vid_get_viewport_fb_d()
{
    // TODO need to check if this is valid if camera started with video selected in PB mode
	return (void*)(*(int*)(0x2554+0x54)); // sub_FFC3C050
}

long vid_get_viewport_height()
{
	return 240;
}

char *camera_jpeg_count_str()
{
	return (char*)0x2CFF8;  // "9999"
}

// PTP display stuff, untested, adapted from ewavr chdkcam patch
// reyalp - type probably wrong, chdkcam patch suggests opposite order from a540 (e.g. vuya)
int vid_get_palette_type() { return 1; }
int vid_get_palette_size() { return 16*4; }

void *vid_get_bitmap_active_palette() {
    return (void *)(0x4C0C+0x20); // sub_FFCA5000
}
void *vid_get_bitmap_active_buffer()
{
    return (void*)(*(int*)(0x4C0C+0x0C)); //SaveBmpVRAMData()->sub_FFCA50C4
}

// values from chdkcam patch
// commented for now, protocol changes needed to handle correctly
// note, play mode may be 704, needs to be tested
#if 0
int vid_get_viewport_width_proper() { 
    return ((mode_get()&MODE_MASK) == MODE_PLAY)?720:*(int*)0x3E50; // VRAM DataSiz
}
int vid_get_viewport_height_proper() {
    return ((mode_get()&MODE_MASK) == MODE_PLAY)?240:*(int*)(0x32C68+4);
}
#endif
