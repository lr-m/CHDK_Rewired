#include "camera.h"
#include "conf.h"
#include "keyboard.h"
#include "action_stack.h"
#include "lang.h"
#include "gui.h"
#include "gui_lang.h"
#include "console.h"
#include "gui_recui.h"
#include "usb_remote.h"
#include "clock.h"
#include "debug_led.h"

//-------------------------------------------------------------------
// Keyboard auto repeat

static long last_kbd_key = 0;
static long last_kbd_time = 0;
static long press_count = 0;

long kbd_get_autoclicked_key()
{
	register long key, t;

	key=kbd_get_clicked_key();
	if (key && (key != last_kbd_key))
    {
		last_kbd_key = key;
		press_count = 0;
		last_kbd_time = get_tick_count();
		return key;
	}
    else
    {
		if (last_kbd_key && kbd_is_key_pressed(last_kbd_key))
        {
			t = get_tick_count();
			if (t-last_kbd_time > ((press_count) ? KBD_REPEAT_DELAY : KBD_INITIAL_DELAY))
            {
				++press_count;
				last_kbd_time = t;
				return last_kbd_key;
			}
            else
            {
				return 0;
			}
		}
        else
        {
			last_kbd_key = 0;
			return 0;
		}
	}
}

//-------------------------------------------------------------------

int kbd_blocked;

int kbd_is_blocked()
{
    return kbd_blocked;
}

void enter_alt(int script_mode)
{
    clear_usb_power();         // Prevent previous USB remote pulse from starting script.
    kbd_blocked = 1;
    gui_set_alt_mode_state(script_mode ? ALT_MODE_ENTER_SCRIPT : ALT_MODE_ENTER);
}

void exit_alt()
{
    kbd_blocked = 0;
    gui_set_alt_mode_state(ALT_MODE_LEAVE);
}

//-------------------------------------------------------------------
// Core keyboard handler

long kbd_process()
{
    static int key_pressed;
#ifdef CAM_BEND_MODE
    static int bend_hold_start;     // tick the ALT button went down, 0 = up
    static int bend_hold_done;      // the hold has already fired this press
#endif

    if( usb_HPtimer_handle==0) usb_remote_key();

    if (camera_info.perf.md_af_tuning)
    {
        switch (camera_info.perf.md_af_on_flag)
        {
        case 1:
            if (get_tick_count() >= (int)(camera_info.perf.md_detect_tick + camera_info.perf.md_af_on_delay))
            {
                camera_info.perf.md_af_on_flag = 2;
                camera_set_led(camera_info.cam_af_led,1,200);
            }
            break;
        case 2:
            if (get_tick_count() >= (int)(camera_info.perf.md_detect_tick + camera_info.perf.md_af_on_delay + camera_info.perf.md_af_on_time))
            {
                camera_info.perf.md_af_on_flag = 0;
                camera_set_led(camera_info.cam_af_led,0,0);
            }
            break;
        }
    }

    // check for & process non-keyboard script terminate
    script_check_terminate();

    // Reset keyboard auto repeat if no buttons pressed
    if (kbd_get_pressed_key() == 0)
        last_kbd_key = 0;

    // Set clicked key for scripts.
    if (kbd_get_clicked_key())
    {
        camera_info.state.kbd_last_clicked = kbd_get_clicked_key();
        camera_info.state.kbd_last_clicked_time = get_tick_count();
    }

#ifdef CAM_HAS_JOGDIAL
    // dial movement in script if script is running and set to handle clicks
    if(camera_info.state.state_kbd_script_run && camera_info.state.script_dial_control == DIAL_SCRIPT_KEYCLICK)
    {
        long jog_direction = get_jogdial_direction();
        if (jog_direction)
        {
            camera_info.state.kbd_last_clicked      = jog_direction;
            camera_info.state.kbd_last_clicked_time = get_tick_count();
        }
    }
#endif

    // Set Shutter Half Press state for GUI task.
    camera_info.state.is_shutter_half_press = kbd_is_key_pressed(KEY_SHOOT_HALF);

    // update any state that needs to be updated regularly in shooting
    extern void shooting_update_state(void);
    shooting_update_state();

#ifdef CAM_RECUI
    // MENU on Canon's plain record screen belongs wholly to Canon. Returning
    // before the record UI, ALT emulation and generic CHDK GUI processing
    // guarantees that no stale swallow state or shortcut can mask its physw
    // bit. Canon menus are still detected by the draw-side ownership gates;
    // this is the matching keyboard-side handoff.
    if (kbd_is_key_pressed(KEY_MENU) &&
        camera_info.state.mode_rec && !camera_info.state.mode_play &&
        camera_info.state.gui_mode_none && !camera_info.state.gui_mode_alt &&
        !gui_bend_active())
        return 0;
#endif

#ifdef CAM_RECUI
    // The flash / focus / drive keys on the plain record screen, before Canon
    // gets a look at them. Returning non-zero from kbd_process() is what makes
    // kbd_update_key_state() overwrite physw_status from kbd_mod_state, so the
    // press never reaches the firmware and Canon's own selector never opens.
    //
    // Ahead of the <ALT> block because these are the Canon-mode arrows, and
    // behind recui_kbd()'s own screen test because <ALT>, bend mode and the
    // menus all want the same keys for their own purposes.
    if (recui_kbd())
    {
        kbd_key_release_all();
        return 1;
    }
#endif

	// Alternative keyboard mode stated/exited by pressing print key.
	// While running Alt. mode shoot key will start a script execution.

	// alt-mode switch and delay emulation
 
	if ( key_pressed  && !usb_remote_active )
	{
        if (kbd_is_key_pressed(conf.alt_mode_button)
                || ((key_pressed >= CAM_EMUL_KEYPRESS_DELAY)
                && (key_pressed < CAM_EMUL_KEYPRESS_DELAY+CAM_EMUL_KEYPRESS_DURATION)))
        {
            if (key_pressed <= CAM_EMUL_KEYPRESS_DELAY+CAM_EMUL_KEYPRESS_DURATION)
                key_pressed++;
#ifdef CAM_BEND_MODE
            // Hold the ALT button to open or close bend mode.
            //
            // Timed in milliseconds rather than in kbd_process ticks. The tick
            // rate is a property of the platform's physw task, so a hold that
            // feels right counted in ticks on this body would be wrong on the
            // next one; get_tick_count() is milliseconds everywhere.
            //
            // This also replaces the keypress emulation below, which passed a
            // held ALT button through to Canon's direct print function. Nothing
            // visible happens on this camera when it fires, and letting it fire
            // at 400ms on the way to a 1000ms hold would mean two things
            // happening for one gesture.
            // Not while <ALT> or a script is up: the button means "leave" there,
            // and the arrows bend mode wants are already spoken for.
            if (kbd_is_key_pressed(conf.alt_mode_button) && !kbd_blocked)
            {
                if (!bend_hold_start)
                    bend_hold_start = get_tick_count();
                else if (!bend_hold_done &&
                         (int)(get_tick_count() - bend_hold_start) >= CAM_BEND_HOLD_MS)
                {
                    // Only swallow the release if the request was accepted. If
                    // bend mode refused - playback, say - the press has to keep
                    // meaning what it always meant.
                    bend_hold_done = gui_bend_toggle();
                }
            }
#else
            if (key_pressed == CAM_EMUL_KEYPRESS_DELAY)
                kbd_key_press(conf.alt_mode_button);
            else if (key_pressed == CAM_EMUL_KEYPRESS_DELAY+CAM_EMUL_KEYPRESS_DURATION)
                kbd_key_release(conf.alt_mode_button);
#endif
            return 1;
        }
        else if (kbd_get_pressed_key() == 0)
        {
#ifdef CAM_BEND_MODE
            if (bend_hold_done)
            {
                // The hold already did the work. Releasing must not also be
                // read as the short press that toggles <ALT>, or every entry
                // into bend mode would drop <ALT> on top of it.
            }
            else if (gui_bend_active())
            {
                // Short press inside bend mode rerolls the patch. <ALT> is not
                // reachable from here, which is deliberate - the way out is the
                // same hold that got you in, or the menu button.
                if (key_pressed < CAM_EMUL_KEYPRESS_DELAY)
                    gui_bend_randomise();
            }
            else
#endif
            if (key_pressed < CAM_EMUL_KEYPRESS_DELAY)
            {
                if (!kbd_blocked)
                {
                    // if start script on alt set, flag to run it
                    if(conf.script_startup==SCRIPT_AUTOSTART_ALT) script_run_on_alt_flag = 1;
                    enter_alt(camera_info.state.state_kbd_script_run);
                }
                else
                    exit_alt();
            }
#ifdef CAM_BEND_MODE
            bend_hold_start = 0;
            bend_hold_done  = 0;
#endif
            key_pressed = 0;
            return 1;
        }
        return 1;
    }
       
    // auto iso shift
    if (camera_info.state.is_shutter_half_press && kbd_is_key_pressed(conf.alt_mode_button)) 
        return 0;

    if (kbd_is_key_pressed(conf.alt_mode_button)) 
	{
        key_pressed = 1;
        kbd_key_release_all();          
        return 1;
    }

#ifdef CAM_TOUCHSCREEN_UI
    extern int ts_process_touch();
    if (ts_process_touch())
    {
        gui_set_need_restore();
    }
#endif

    // deals with the rest

    if ( !kbd_blocked || usb_remote_active ) 
	{
		kbd_blocked = handle_usb_remote();
	}

    if (gui_kbd_process())
        return 1;

    action_stack_process_all();

    // Check if a PTP script needs to be started
    //  do this after action_stack_process_all so new script is not run until next timeslice
    extern void start_ptp_script();
    start_ptp_script();

    return kbd_blocked;
}
