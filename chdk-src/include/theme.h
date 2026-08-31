#ifndef THEME_H
#define THEME_H

//-------------------------------------------------------------------
// Themes - one named colour scheme across everything this fork draws.
//
// The bitmap plane on these bodies is 8-bit paletted and the palette is
// Canon's: no port here defines CAM_LOAD_CUSTOM_COLORS, so there is no way to
// invent a colour. A theme is therefore an *assignment*, not a palette - it
// says which of the entries the camera already has each part of the UI should
// use. That is a real constraint and it is the reason the themes are built out
// of the same twelve or so distinct record-mode entries listed in
// docs/OVERLAY_DESIGN.md rather than out of anything named after a decade.
//
// The slots below are semantic. Nothing asks a theme for "cyan"; it asks for
// the colour of a live value, or of a warning, or of the second engine, and
// the theme decides. That is what lets one enum cover the bend UI, the record
// UI's menu and the persistent overlay without any of them knowing what the
// others chose.
//
// Held as IDX_COLOR_* indices, not COLOR_*, because COLOR_* expands to a
// lookup into the runtime chdk_colors[] array and cannot initialise a static
// table - the same reason posd_pal_default[] in gui.c is written that way.
// theme_color() does the lookup, against whichever palette is live when it is
// called, so record and playback each get their own resolution for free.
//
//-------------------------------------------------------------------
// Colours CHDK never named, mixed rather than found.
//
// A palette byte is not one colour, it is *two*: each nibble names one of
// sixteen base entries and the palette holds their 50/50 blend. That is why
// CHDK's own table has "light red" at 0x21 - red on white - and "dark yellow"
// at 0x6f - yellow on black. Every byte whose appearance is known fits it and
// nothing contradicts it:
//
//   0x11 white      1+1               0x1f light grey  white + black
//   0x21 light red  red + white       0x25 "dark green" red + green (olive)
//   0x3f grey       grey + black      0x51 light green  green + white
//   0x66 yellow     6+6               0x6f dark yellow  yellow + black
//   0xdd blue       D+D               0xe1 light pink   orange + white
//   0xe2 red        orange + red      0xee orange       E+E
//
// which pins down the base entries: 1 white, 2 red, 3 grey, 4 dark grey,
// 5 green, 6 yellow, D blue, E orange, F black.
//
// So the colours the twenty names do not have are not missing from the
// hardware - they are mixtures, and they are made the same way "light red" is.
// The two ends of a mix are interchangeable: 0x2d and 0xd2 are one blend.
//
// This replaced a table read out of the ROM, which described a palette this
// camera does not load - four attempts at orange and purple came from it and
// all four were wrong on the body. Visual settings -> "Dump palette to card"
// writes the live palette to A/CHDK/LOGS/PALETTE.TXT and is how to check any
// of this rather than infer it again.
#define TRAW(b)         (0x100 | (b))           // a raw palette byte
#define TMIX(a,b)       TRAW(((a) << 4) | (b))  // two base entries, blended

#define PAL_WHITE       TRAW(0x11)
#define PAL_RED         TRAW(0x22)
#define PAL_GREEN       TRAW(0x55)
#define PAL_YELLOW      TRAW(0x66)
#define PAL_BLUE        TRAW(0xdd)
#define PAL_ORANGE      TRAW(0xee)      // the base entry itself
#define PAL_BLACK       TRAW(0xff)

#define PAL_PURPLE      TMIX(0x2,0xd)   // red + blue
#define PAL_TURQUOISE   TMIX(0xd,0x5)   // blue + green
#define PAL_PINK        TMIX(0x2,0x1)   // red + white
#define PAL_PINK_HOT    TRAW(0xe1)      // orange + white - seen on the body
#define PAL_AMBER       TMIX(0x6,0xe)   // yellow + orange
#define PAL_BLUE_LT     TMIX(0xd,0x1)   // blue + white
#define PAL_SILVER      TMIX(0x1,0x3)   // white + grey
#define PAL_NAVY        TMIX(0xd,0xf)   // blue + black
#define PAL_MOSS        TMIX(0x5,0xf)   // green + black
#define PAL_GREEN_LT    TMIX(0x5,0x1)   // green + white

// A blend has two ends and no more, so there is no light purple: purple is
// already red + blue and there is no third nibble to put white in. Anywhere a
// paler purple was wanted, pink - red + white - is the colour that exists.
//-------------------------------------------------------------------

#include "color.h"

#define THEME_APPLE     0       // the six-stripe logo, and the default
#define THEME_VAPORWAVE 1
#define THEME_AERO      2       // frutiger aero
#define THEME_TERMINAL  3
#define THEME_GUNMETAL  4
#define THEME_COUNT     5

// Semantic slots. Adding one means adding a column to every theme in theme.c,
// which is the point: a theme that has not decided what a new part of the UI
// looks like should not compile.
#define TC_ACCENT       0       // live, applied, in use
#define TC_ACCENT2      1       // committed, secondary
#define TC_TEXT         2       // ordinary text
#define TC_DIM          3       // annotation, disabled, off
#define TC_HILITE       4       // cursor background
#define TC_HILITE_FG    5       // text on the cursor
#define TC_EDIT         6       // the cell being edited, background
#define TC_WARN         7       // slow, costly, about to be surprising
#define TC_BAD          8       // destructive
#define TC_SPECIAL      9       // the second engine, and injected sources
// Bright by construction. For things that have to stay legible against a live
// view whatever the theme is doing - the histogram's bars above all, which are
// two pixels wide over an unknown picture and cannot afford a dark colour.
#define TC_LIGHT        10
#define TC_N            11

// Resolve a slot for the palette in use now. Cheap - one array index and one
// table lookup - so it is safe on a redraw path.
color theme_color(int slot);

// One of the persistent overlay's twelve readout fields, resolved. The overlay
// asks once, at posd_pal_init().
#define THEME_OSD_FIELDS 12
color theme_osd_color(int field);

// The histogram's gradient, bottom band to top. Three colours rather than a
// ramp because the palette is twenty fixed entries with no interpolation
// available: a gradient here is three bands of solid colour, and the bar's
// colour comes from where on the box it is rather than from which bar it is,
// so the whole histogram reads as one field of colour.
#define THEME_HISTO_BANDS 3
color theme_histo(int band);

extern const char * const theme_names[THEME_COUNT];

#endif
