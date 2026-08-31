#ifndef CURVES_H
#define CURVES_H

#include "flt.h"

#define CURVE_DIR "A/CHDK/CURVES"

// Update version if changes are made to the module interface
#define CURVES_VERSION          {2,0}

typedef struct 
{
    base_interface_t    base;

	void (*curve_init_mode)();
	void (*curve_apply)();
    void (*curve_set_mode)();
    void (*curve_set_file)();
} libcurves_sym;

extern libcurves_sym* libcurves;

//-------------------------------------------------------------------

#endif
