#include "lolevel.h"
#include "platform.h"
#include "keyboard.h"
#include "kbd_common.h"
#include "camera_info.h"
#include "gui.h"
#include "levent.h"

extern int kbd_is_blocked(void);

long kbd_new_state[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
long kbd_prev_state[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
long kbd_mod_state[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

extern void _platformsub_kbd_fetch_data(long*);

int get_usb_bit() 
{
	long usb_physw[3];
	usb_physw[USB_IDX] = 0;
	_kbd_read_keys_r2(usb_physw);
	return(( usb_physw[USB_IDX] & USB_MASK)==USB_MASK) ; 
}

#define NEW_SS (0x2000)

static char kbd_stack[NEW_SS];

KeyMap keymap[] = {
	/* tiny bug: key order matters. see kbd_get_pressed_key()
	 * for example
	 */

    { 0, KEY_DISPLAY	, 0x00020000 },   // playback key

    { 2, KEY_SHOOT_FULL	, 0x00000030 },
    { 2, KEY_SHOOT_FULL_ONLY, 0x00000020 },
    { 2, KEY_SHOOT_HALF	, 0x00000010 },
    { 2, KEY_UP		, 0x00000001 },
    { 2, KEY_DOWN		, 0x00000002 },
    { 2, KEY_LEFT		, 0x00000008 },
    { 2, KEY_RIGHT		, 0x00000004 },
    { 2, KEY_SET		, 0x00000100 },
    { 2, KEY_ZOOM_IN	, 0x00000080 },
    { 2, KEY_ZOOM_OUT	, 0x00000040 },
    { 2, KEY_MENU		, 0x00000400 },
    { 2, KEY_PRINT		, 0x00000800 },   // mode key
    { 0, 0, 0 }
};


long __attribute__((naked)) wrap_kbd_p1_f() ;


static void __attribute__((noinline)) mykbd_task_proceed()
{
	while (physw_run){
		_SleepTask(10);
		
		if (wrap_kbd_p1_f() == 1){ // autorepeat ?
			_kbd_p2_f();
		}
	}
}

void __attribute__((naked,noinline)) mykbd_task()
{
    /* WARNING
     * Stack pointer manipulation performed here!
     * This means (but not limited to):
     *	function arguments destroyed;
     *	function CAN NOT return properly;
     *	MUST NOT call or use stack variables before stack
     *	is setup properly;
     *
     */

	register int i;
	register long *newstack;

	newstack = (void*)kbd_stack;

	for (i=0;i<NEW_SS/4;i++)
		newstack[i]=0xdededede;

	asm volatile (
		"MOV	SP, %0"
		:: "r"(((char*)newstack)+NEW_SS)
		: "memory"
	);

	mykbd_task_proceed();

	/* function can be modified to restore SP here...
	 */

	_ExitTask();
}


long __attribute__((naked,noinline)) wrap_kbd_p1_f()
{

	asm volatile(
		"STMFD   SP!, {R1-R5,LR}\n"
		"MOV     R4, #0\n"
		"BL      my_kbd_read_keys\n"
		"B       _kbd_p1_f_cont\n"
	);
	return 0; // shut up the compiler
}


void my_kbd_read_keys()
{
#ifdef CAM_FORWARD_MACRO_KEY_EVENT
    static int macro_was_down;
#endif
    kbd_update_key_state();

    _kbd_read_keys_r2(physw_status);

    kbd_update_physw_bits();

#ifdef CAM_FORWARD_MACRO_KEY_EVENT
    {
        int macro_down = kbd_is_key_pressed(KEY_LEFT);
        if (macro_down != macro_was_down && !kbd_is_blocked() &&
            camera_info.state.mode_rec && camera_info.state.gui_mode_none &&
            !gui_bend_active())
        {
            // MacroController's event handler at 0xffd11008 accepts 0x852
            // and 0x890. The key-table value 0x06 is only the physical event
            // and posting it to the UI does not reach this controller.
            PostLogicalEventToUI(macro_down ? 0x852 : 0x890, 0);
        }
        macro_was_down = macro_down;
    }
#endif
}

void kbd_fetch_data(long *dst)
{
    _platformsub_kbd_fetch_data(dst);
}
