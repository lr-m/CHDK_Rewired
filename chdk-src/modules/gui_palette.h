#ifndef GUI_PALETTE_H
#define GUI_PALETTE_H

#include "flt.h"

//-------------------------------------------------------------------

#define PALETTE_MODE_DEFAULT    0
#define PALETTE_MODE_SELECT     1
#define PALETTE_MODE_TEST       2

//-------------------------------------------------------------------

// Update version if changes are made to the module interface
#define GUI_PALETTE_VERSION     {2,2}

typedef struct
{
    base_interface_t    base;

    void (*show_palette)(int mode, chdkColor st_color, void (*on_select)(chdkColor clr));
} libpalette_sym;

//-------------------------------------------------------------------
extern libpalette_sym* libpalette;

//-------------------------------------------------------------------
#endif
