/*
Common low level keyboard code. This file must not be included in module or platform independent code
This file is the interface between core/kbd_common.c and the platforms kbd.c file
Platform specific defines go in platform_kbd.h, see kbd_common.c for documentation
*/
#ifndef KBD_COMMON_H
#define KBD_COMMON_H

#include "platform_kbd.h"

typedef struct {
	short grp;
	short hackkey;
	long canonkey;
// TODO not clear these should be include in the main keymap
#ifdef CAM_TOUCHSCREEN_UI
    short   x1, y1, x2, y2;
#ifdef CAM_TOUCHSCREEN_SYMBOLS
    short   sc;
#endif
    short   redraw;
    char    *nm, *nm2;
    int     min_gui_mode, max_gui_mode, cam_mode_mask;
    int     *conf_val;
    const char* (*chg_val)(int,int);
    int     *conf_disable;
#endif
} KeyMap;

// key to canon bit mapping
extern KeyMap keymap[];

extern long kbd_new_state[];
extern long kbd_mod_state[];
extern long kbd_prev_state[];

long kbd_update_key_state(void);
void kbd_update_physw_bits(void);
void kbd_fetch_data(long *data);

// 0 send jogdial events to canon firmware, 1 block
void jogdial_control(int n);
extern int jogdial_stopped;

// sanity checks. These can't be in camera.h because they depend on platform_kbd.h
#if defined(CAM_UNLOCK_ANALOG_AV_IN_REC) && !defined(ANALOG_AV_FLAG)
    #error "CAM_UNLOCK_ANALOG_AV_IN_REC requires ANALOG_AV_FLAG"
#endif

#if defined(CAM_REMOTE_ANALOG_AV) && !defined(ANALOG_AV_FLAG)
    #error "CAM_REMOTE_ANALOG_AV requires ANALOG_AV_FLAG"
#endif

#if defined(CAM_REMOTE_HDMI_HPD) && !defined(HDMI_HPD_FLAG)
    #error "CAM_REMOTE_HDMI_HPD requires HDMI_HPD_FLAG"
#endif

#endif
