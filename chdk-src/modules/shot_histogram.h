#ifndef SHOT_HISTOGRAM_H
#define SHOT_HISTOGRAM_H

// CHDK Calculate histogram from RAW sensor interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

#include "flt.h"

// Update version if changes are made to the module interface
#define SHOT_HISTO_VERSION      {1,0}

// Shot Histogram module interface
typedef struct
{
    base_interface_t    base;

    int (*shot_histogram_set)(int);
    int (*shot_histogram_get_range)(int, int);
    void (*build_shot_histogram)();
    void (*write_to_file)();
} libshothisto_sym;

extern libshothisto_sym* libshothisto;

#endif
