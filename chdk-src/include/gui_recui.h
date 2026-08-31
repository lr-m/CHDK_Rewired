#ifndef GUI_RECUI_H
#define GUI_RECUI_H

// The record-screen control selectors drawn by CHDK instead of by Canon.
// See core/gui_recui.c and docs/A480_UI_REVERSING.md.

#ifdef CAM_RECUI

// 1 while a selector readout is on screen and the arrows belong to it.
extern int  recui_active(void);

// Called from kbd_process(). Returns 1 when it has taken the keys, which is
// what stops Canon seeing the press and opening its own popup.
extern int  recui_kbd(void);

// Called from gui_redraw(). Draws or erases the readout.
extern void recui_draw(int force);

#else

#define recui_active()      0
#define recui_kbd()         0
#define recui_draw(force)   ((void)0)

#endif // CAM_RECUI

#endif
