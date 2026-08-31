#ifndef GUI_FSELECT_H
#define GUI_FSELECT_H

#include "flt.h"

// Update version if changes are made to the module interface
#define GUI_FSELECT_VERSION     {2,0}

typedef struct
{
    base_interface_t    base;

    void (*file_select)(int title, const char* prev_dir, const char* default_dir, void (*on_select)(const char *fn));
} libfselect_sym;

//-------------------------------------------------------------------
extern libfselect_sym* libfselect;

//-------------------------------------------------------------------
#endif
