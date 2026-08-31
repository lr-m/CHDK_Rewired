#include "lolevel.h"
#include "platform.h"
#include "core.h"
#include "keyboard.h"
#include "conf.h"
#include "kbd_common.h"

KeyMap keymap[] = {
    /* tiny bug: key order matters. see kbd_get_pressed_key()
     * for example
     */
//  { 1, KEY_POWER      ,     0x04000000 }, // levent 0x600 (uses inverted logic in physw_status)
    { 2, KEY_UP         ,     0x00000001 },
    { 2, KEY_DOWN       ,     0x00000002 },
    { 2, KEY_LEFT       ,     0x00000008 },
    { 2, KEY_RIGHT      ,     0x00000004 },
    { 2, KEY_SET        ,     0x00000100 },
    { 2, KEY_SHOOT_FULL ,     0x00000030 },
    { 2, KEY_SHOOT_FULL_ONLY, 0x00000020 },
    { 2, KEY_SHOOT_HALF ,     0x00000010 },
    { 2, KEY_MENU       ,     0x00000400 },
    { 2, KEY_PRINT      ,     0x00001000 },
    { 0, 0, 0 }, // placeholder for fake key
    { 0, 0, 0 }
};

long kbd_new_state[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
long kbd_prev_state[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
long kbd_mod_state[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

int keys_mask1 = 0x00000000;
int keys_inv1 = 0x00000000;
static int set_fake_key=0;

int get_usb_bit() 
{
	long usb_physw[3];
	usb_physw[USB_IDX] = 0;
	_kbd_read_keys_r2(usb_physw);
	return(( usb_physw[USB_IDX] & USB_MASK)==USB_MASK) ; 
}

void kbd_fetch_data(long*);


long __attribute__((naked)) wrap_kbd_p1_f() ;


// task created directly, stack manipulation is not needed
void __attribute__((naked,noinline))
mykbd_task(long ua, long ub, long uc, long ud, long ue, long uf)
{
    (void)ua; (void)ub; (void)uc; (void)ud; (void)ue; (void)uf;

    kbd_new_state[0] = physw_status[0];
    kbd_new_state[1] = physw_status[1] ^ keys_inv1;
    kbd_new_state[2] = physw_status[2];

    while (physw_run){
        _SleepTask(10);

        if (wrap_kbd_p1_f() == 1){ // autorepeat ?
            _kbd_p2_f();
        }
    }

    _ExitTask();
}


long __attribute__((naked,noinline)) wrap_kbd_p1_f()
{

    asm volatile(
                "STMFD   SP!, {R1-R5,LR}\n"
                "MOV     R4, #0\n"
                "BL      my_kbd_read_keys\n"
               	"B	 _kbd_p1_f_cont\n"
    );
    return 0; // shut up the compiler
}

void enable_extra_button(short);

void my_kbd_read_keys()
{
    kbd_prev_state[0] = kbd_new_state[0];
    kbd_prev_state[1] = kbd_new_state[1];
    kbd_prev_state[2] = kbd_new_state[2];

    kbd_fetch_data(kbd_new_state);

    kbd_new_state[1] = kbd_new_state[1] ^ keys_inv1;
    
    if (kbd_process() == 0){
	// leave it alone...
	physw_status[0] = kbd_new_state[0];
	physw_status[1] = kbd_new_state[1] ^ keys_inv1;
	physw_status[2] = kbd_new_state[2];
    } else {
	// override keys
	physw_status[0] = (kbd_new_state[0] & (~KEYS_MASK0)) |
			  (kbd_mod_state[0] & KEYS_MASK0);

	physw_status[1] = ((kbd_new_state[1] & (~KEYS_MASK1)) |
			  (kbd_mod_state[1] & KEYS_MASK1)) ^ keys_inv1;

	physw_status[2] = (kbd_new_state[2] & (~KEYS_MASK2)) |
			  (kbd_mod_state[2] & KEYS_MASK2);
    }

    _kbd_read_keys_r2(physw_status);

    // usb and SD read-only are standard
    kbd_update_physw_bits();

    if (set_fake_key & 0x10000000) {
        enable_extra_button((short)(set_fake_key & 0xffff));
        set_fake_key=0;
    }
}


void enable_extra_button(short key) {
/*
 * enable or disable the additional "fake" button
 * in this case, the power button will be re-mapped in ALT mode
 * called from the kbd task
 * beware: the "placeholder" in keymap[] is directly addressed here
 * the placeholder has to be the last entry before the terminating { 0, 0, 0 }, if it is ever set to { 0, 0, 0 }
 */
    if (key) {
        keys_mask1=0x04000000;
        keys_inv1= 0x04000000;
        keymap[10].grp=1;
        keymap[10].canonkey=0x04000000;
        keymap[10].hackkey=key;
    }
    else {
        keys_mask1=0x00000000;
        keys_inv1= 0x00000000;
        keymap[10].grp=0;
        keymap[10].canonkey=0;
        keymap[10].hackkey=0;
    }
}

void kbd_set_extra_button(short key) { // called by core
    set_fake_key=key|0x10000000;    
}

void kbd_fetch_data(long *dst)
{
    volatile long *mmio0 = (void*)0xc0220200;
    volatile long *mmio1 = (void*)0xc0220204;
    volatile long *mmio2 = (void*)0xc0220208;

    dst[0] = *mmio0;
    dst[1] = *mmio1;
    dst[2] = *mmio2 & 0xffff;
}
