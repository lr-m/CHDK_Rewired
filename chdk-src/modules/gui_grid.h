#ifndef GUI_GRID_H
#define GUI_GRID_H

#include "flt.h"

// Update version if changes are made to the module interface
#define GUI_GRID_VERSION        {2,0}

typedef struct
{
    base_interface_t    base;

    void (*gui_grid_draw_osd)(int force);
    void (*grid_lines_load)(const char *fn);
} libgrids_sym;

//-------------------------------------------------------------------
extern libgrids_sym* libgrids;

//-------------------------------------------------------------------
#endif
