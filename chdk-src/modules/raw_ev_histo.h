#ifndef RAW_EV_HISTO_H
#define RAW_EV_HISTO_H

// CHDK Calculate EV based histogram from RAW sensor interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

#include "flt.h"

// Update version if changes are made to the module interface
#define RAW_EV_HISTO_VERSION      {1,0}

// Shot Histogram module interface
typedef struct
{
    base_interface_t    base;

    void (*load)(int);
    void (*build)();
    void (*draw)(int force_redraw);
    void (*erase)();
} librawevhisto_sym;

extern librawevhisto_sym* librawevhisto;

#endif
