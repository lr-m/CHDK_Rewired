#include "platform.h"
#include "lolevel.h"

// enabled_refresh_physical_screen (0x73A0+0x30) is Canon's own lock counter,
// not a CHDK flag: _ScreenLock() increments it, _RefreshPhysicalScreen()
// decrements it, and the screen repaints only when it reaches zero.
//
// It therefore belongs to the firmware and has to be treated as a nesting
// count. CHDK takes exactly one lock, releases exactly one, and never writes
// the counter directly.
//
// It used to write it - vid_bitmap_refresh() forced it to 1 before releasing,
// so the release always landed on zero however far the count had run. That is
// what killed the display. Canon takes its own balanced locks for the flash
// and macro popups and for its menus; overwriting the count threw Canon's
// outstanding lock away, Canon's matching unlock then took the counter
// negative, and a counter that never returns to zero is a screen that never
// repaints again. Which is exactly how it failed: the popups worked once or
// twice, then nothing - not the popups, not the Canon menu - until a reboot.

// How many of the counter's outstanding locks are CHDK's own, so that
// vid_bitmap_refresh() can tell its own locks apart from Canon's without ever
// writing the counter. Taken and released only from spytask - the <ALT>
// enter/leave pair and posd_canon_osd_begin()/_end() in core/gui.c - so a plain
// int is enough.
static int chdk_screen_locks = 0;

// Repaint now, leaving the count as it was found.
//
// This has to stand CHDK's own outstanding locks down first. <ALT> mode takes
// one lock for as long as it is up (gui_activate_alt_mode()), so a plain
// lock-then-release inside <ALT> could never bring the count below one and
// never repainted - and the repaint is what erases the bitmap. That is what
// left the bottom of a long menu on screen underneath a shorter submenu:
// gui_menu_erase_and_redraw() asks for the erase via draw_restore(), the erase
// silently did nothing, and the submenu simply drew over the top of the rows it
// did not cover.
//
// Canon's own locks are still respected: releasing only as many as CHDK holds
// means a count Canon is holding up stays above zero and no repaint happens,
// which is what holding a lock is supposed to mean.
void vid_bitmap_refresh()
{
 int i, n = chdk_screen_locks;

 // Ours, released - the last of these lands on zero and repaints if nobody
 // else is holding the screen.
 for (i = 0; i < n; i++)
     _RefreshPhysicalScreen(1);

 // Nothing of ours to stand down: the balanced pair nets to zero and repaints
 // from a resting count, exactly as before.
 if (n == 0)
 {
     _ScreenLock();
     _RefreshPhysicalScreen(1);
 }

 // And taken back, so the caller's lock is still held on return.
 for (i = 0; i < n; i++)
     _ScreenLock();
}

// One lock. Paired with vid_turn_on_updates() by the caller - see
// posd_canon_osd_begin() and the <ALT> enter/leave pair in core/gui.c.
//
// No guard on the counter any more. The old one skipped the lock whenever the
// count was already non-zero, which meant CHDK silently took no lock at all
// whenever Canon happened to hold one, and then released a lock it never had.
void vid_turn_off_updates()
{
 _ScreenLock();
 chdk_screen_locks++;
}

// The matching release. Repaints as it lands on zero, so Canon's own OSD comes
// back immediately rather than at the next event that happens to refresh.
void vid_turn_on_updates()
{
 if (chdk_screen_locks > 0)
     chdk_screen_locks--;
 _RefreshPhysicalScreen(1);
}



void shutdown()
{
	volatile long *p = (void*)0xC022001C;    
	
	asm(
		"MRS     R1, CPSR\n"
		"AND     R0, R1, #0x80\n"
		"ORR     R1, R1, #0x80\n"
		"MSR     CPSR_cf, R1\n"
		:::"r1","r0");
	
	*p = 0x44;  // power off.
	
	while(1);
}

// only two LEDs in A480:

#define LED_PR 0xC0220088  // green LED
//#define LED_AF 0xC0220080  // orange AF LED

void debug_led(int state)
{
 *(int*)LED_PR=state ? 0x46 : 0x44;
}

void camera_set_led(int led, int state, __attribute__ ((unused))int bright) {
 static char led_table[]={7,9};
 _LEDDrive(led_table[led%sizeof(led_table)], state<=1 ? !state : state);
}

int get_flash_params_count(void){
 return 122; 
}
