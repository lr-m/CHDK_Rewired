#ifndef CHDK_COLOR_H
#define CHDK_COLOR_H

// CHDK color definitions

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

// Structures to store user configurable color entry
#pragma pack(1)     // make sure they are packed as tightly as possible

typedef struct {
    unsigned char   col;        // color value
    unsigned char   type;       // 0 = Canon palette index, 1 = CHDK color index (chdk_colors)
} chdkColor;

typedef struct {
    chdkColor       fg;         // foreground
    chdkColor       bg;         // background
} confColor;

#pragma pack()

typedef int             coord;
typedef unsigned char   color;
typedef unsigned short  twoColors;

#define MAKE_COLOR(bg, fg)  ((twoColors)(((color)(bg) << 8) | (color)(fg)))
#define FG_COLOR(c)         ((color)(c     ))
#define BG_COLOR(c)         ((color)(c >> 8))

//==========================================================

#endif
