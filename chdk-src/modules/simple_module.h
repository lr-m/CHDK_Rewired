#ifndef SIMPLE_MODULE_H
#define SIMPLE_MODULE_H

#include "flt.h"

// Update version if changes are made to the module interface
#define SIMPLE_MODULE_VERSION   {1,0}       // Version for simple modules & games

// Simple modules (e.g. games, calendar)
typedef struct
{
    base_interface_t    base;
} libsimple_sym;

//-------------------------------------------------------------------
#endif
