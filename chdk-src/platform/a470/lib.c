#include "platform.h"
#include "lolevel.h"

// 102c only. refresh_physical_screen_blocked is a raw RAM address derived from
// the 102c dump (the only a470 firmware we have one for), so it is DEF'd in
// sub/102c/stubs_min.S and nowhere else. On 100e/101a/101b these three
// functions are left undefined and the weak defaults in
// platform/generic/wrappers.c apply instead: vid_bitmap_refresh() calls
// _RefreshPhysicalScreen(1) and the two update hooks are no-ops. That is the
// pre-overlay-fix behaviour - those subs flicker, but they build and run.
// To enable the fix on another revision, re-derive the flag address from a dump
// of that firmware; see docs/OVERLAY_FLICKER.md.
#ifdef CAMERA_a470_102c

// Canon repaints its own OSD straight into the bitmap buffer CHDK draws in, so
// every repaint wipes whatever CHDK had put there until the next spytask redraw
// pass picks it up again - which is what shows up as the overlay flashing.
// RefreshPhysicalScreen is the entry point for that repaint, and it can be gated
// with the flag defined in stubs_min.S. 1 blocks it, 0 allows it (see the note
// there - this firmware's polarity is inverted relative to the a480's).
extern int refresh_physical_screen_blocked;

void vid_bitmap_refresh()
{
    refresh_physical_screen_blocked = 0;
    _RefreshPhysicalScreen(1);
}

// Called when CHDK takes over the screen (<ALT> mode, menus, script console).
// Canon has nothing it needs to draw while we own the display, so hold it off
// rather than racing it.
void vid_turn_off_updates()
{
    refresh_physical_screen_blocked = 1;
}

// Called when CHDK hands the screen back. Re-enable Canon's repaint and force
// one immediately, so its OSD comes back instead of waiting for the next event
// that happens to trigger a refresh.
void vid_turn_on_updates()
{
    vid_bitmap_refresh();
}

#endif // CAMERA_a470_102c

char *hook_raw_image_addr()
{
    return (char*)0x10F06B20; //found at 0xFFD82C10 (100e)
}

void *vid_get_viewport_fb()
{
	 return (void*)0x10659D50; // found at 0xFFE2B904 (100e)
}

long vid_get_viewport_height()
{
    return 240;
}

void *vid_get_bitmap_fb()  //OSD buffer     
{
    return (void*)0x10361000; //found at 0xFFCC2F24 (100e)
}

void shutdown()
{
    volatile long *p = (void*)0xc02200a0;
        
    asm(
         "MRS     R1, CPSR\n"
         "AND     R0, R1, #0x80\n"
         "ORR     R1, R1, #0x80\n"
         "MSR     CPSR_cf, R1\n"
         :::"r1","r0");
        
    *p = 0x44;

    while(1);
}

// print led (blue)
#define LED_PR 0xc0220084

void debug_led(int state)
{
    volatile long *p=(void*)LED_PR;
    if (state)
	p[0]=0x46;
    else
	p[0]=0x44;
}

#define LED_AF 0xc0220080

int get_flash_params_count(void){
 return 115;
}
