#ifndef KEYBOARD_H
#define KEYBOARD_H

// CHDK Keyboard / Button interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

#define KEY_UP              1
#define KEY_DOWN            2
#define KEY_LEFT            3
#define KEY_RIGHT           4
#define KEY_SET             5
#define KEY_SHOOT_HALF      6
#define KEY_SHOOT_FULL      7   // includes KEY_SHOOT_HALF press/release. See also KEY_SHOOT_FULL_ONLY.
#define KEY_ZOOM_IN         8
#define KEY_ZOOM_OUT        9
#define KEY_MENU            10
#define KEY_DISPLAY         11
#define KEY_PRINT           12  //S-series: Shortcut
#define KEY_ERASE           13  //S-series: Func
#define KEY_ISO             14  //S-series
#define KEY_FLASH           15  //S-series
#define KEY_MF              16  //S-series
#define KEY_MACRO           17  //S-series
#define KEY_VIDEO           18  //S-series
#define KEY_TIMER           19  //S-series
#define KEY_EXPO_CORR       20  //G-series
#define KEY_MICROPHONE      21
#define KEY_FACE            22  //SX100IS
#define KEY_DUMMY           23  //dummy key for pressing to disable energy saving in alt mode

// SX30
#define KEY_ZOOM_IN1	    24	// SX30 has three zoom speeds each way
#define KEY_ZOOM_IN2	    25
#define KEY_ZOOM_IN3	    26
#define KEY_ZOOM_OUT1	    27
#define KEY_ZOOM_OUT2	    28
#define KEY_ZOOM_OUT3	    29
#define	KEY_ZOOM_ASSIST	    30	// SX30 new button

// G12
#define	KEY_AE_LOCK		    31	// G12 AE/FE Lock button
#define	KEY_METERING	    32	// G12 metering mode button

//SX220 
#define KEY_PLAYBACK        33 
#define KEY_LEFT_SOFT       34  // jogdial keys have two steps 
#define KEY_RIGHT_SOFT      35 
#define KEY_UP_SOFT         36 
#define KEY_DOWN_SOFT       37  

#define KEY_SHOOT_FULL_ONLY 38  // As KEY_SHOOT_FULL but press/release KEY_SHOOT_HALF is not included

#define KEY_POWER           39

//A810/A1300/A2300/A2400/A4000
#define KEY_HELP            40

//D10/A490/A495
#define KEY_MODE            41

//S90/S95/S100/S110
#define KEY_RING_FUNC       42

// dedicated wifi connect button (G7x)
#define KEY_WIFI            43

//SX530 - Framing Assist Lock feature to help reduce camera shake on the telephoto end.
#define KEY_FRAMING_ASSIST  44

//SX520 - drive mode button
#define KEY_DRIVE_MODE      45

//SX400/SX410 - Switching from the AUTO mode to another mode and vice versa.
#define KEY_AUTO            46

#define JOGDIAL_LEFT        100
#define JOGDIAL_RIGHT       101
#define FRONTDIAL_LEFT      102
#define FRONTDIAL_RIGHT     103

// constants for camera_info.state.script_dial_control
#define DIAL_SCRIPT_KEYCLICK 1 // report dial movement as key clicks, block from Canon FW in script

/******************************************************************/

extern void kbd_key_press(long key);
extern void kbd_key_release(long key);
extern void kbd_key_release_all();
extern long kbd_is_key_pressed(long key);
extern long kbd_is_key_clicked(long key);
extern long kbd_get_pressed_key();
extern long kbd_get_clicked_key();
extern long kbd_get_autoclicked_key();
extern long kbd_use_zoom_as_mf();
extern void kbd_set_extra_button(short);
extern long get_jogdial_direction(void);
extern int kbd_force_analog_av(int);

extern void JogDial_CW(void);
extern void JogDial_CCW(void);

extern void enter_alt(int script_mode);
extern void exit_alt();

// get HDMI hotplug status from as seen by canon firmware (possibly modified by CHDK)
int get_hdmi_hpd_physw_mod(void);
// get analog video status from as seen by canon firmware (possibly modified by CHDK)
int get_analog_av_physw_mod(void);
// get USB bit from as seen by canon firmware (possibly modified by CHDK)
int get_usb_bit_physw_mod(void);
/******************************************************************/

#endif
