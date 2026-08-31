#ifndef GUI_READ_H
#define GUI_READ_H

#include "flt.h"

// Update version if changes are made to the module interface
#define GUI_READ_VERSION        {2,0}

typedef struct
{
    base_interface_t    base;

    int (*read_file)(const char *fn);
} libtxtread_sym;

//-------------------------------------------------------------------
extern libtxtread_sym* libtxtread;

//-------------------------------------------------------------------
#endif
