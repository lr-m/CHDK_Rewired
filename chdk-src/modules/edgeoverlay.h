#ifndef EDGE_OVERLAY_H
#define EDGE_OVERLAY_H

#include "flt.h"

// if you change this, remember to change the mkdir in main too
#define EDGE_SAVE_DIR "A/CHDK/EDGE"

// Update version if changes are made to the module interface
#define EDGEOVERLAY_VERSION     {2,0}

typedef struct
{
    base_interface_t    base;

	void (*edge_overlay)();
    void (*load_edge_overlay)(const char* fn);
    void (*save_edge_overlay)(void);
} libedgeovr_sym;

extern libedgeovr_sym* libedgeovr;

#endif
