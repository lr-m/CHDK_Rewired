#ifndef HISTOGRAM_H
#define HISTOGRAM_H

// CHDK Histogram OSD interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//-------------------------------------------------------------------

// Values for conf.show_histo
#define SHOW_HISTO_ALWAYS   1
#define SHOW_HISTO_REC      2
#define SHOW_HISTO_HALF     3

//-------------------------------------------------------------------
#ifdef CAM_DRAW_YUV
    #define LARGE_HISTO 1
#endif

#ifndef LARGE_HISTO
#define HISTO_WIDTH                 128 // Note code is optimised for this value, it should not be changed!
#define HISTO_HEIGHT                50
#else
#define HISTO_WIDTH                 256 // Note code is optimised for this value, it should not be changed!
#define HISTO_HEIGHT                100 
#endif

#include "flt.h"

// Update version if changes are made to the module interface
#define HISTO_VERSION           {1,0}

typedef struct
{
    base_interface_t    base;

    void (*histogram_process)(void);
    void (*gui_osd_draw_histo)(int is_osd_edit);
} libhisto_sym;

extern libhisto_sym* libhisto;

#endif
