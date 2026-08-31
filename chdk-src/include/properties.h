#ifndef PROPERTIES_H
#define PROPERTIES_H

// CHDK Firmware params & props interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

// If building for CHDK core include appropriate propset file
// For modules & platform independent code trying to use any PROPCASE value will give an error
// In this case use the camera_info struct instead
#ifndef CHDK_MODULE_CODE

/* propcase ID constants. These are in their own header files for easier sed processing */
#if CAM_PROPSET == 13
    #include "propset13.h"  // G7X2
#elif CAM_PROPSET == 12
    #include "propset12.h"  // DryOS r59 (eos m100, sx730)
#elif CAM_PROPSET == 11
    #include "propset11.h"  // SX430 -
#elif CAM_PROPSET == 10
    #include "propset10.h"  // Some DryOS R57 (eos m10)
#elif CAM_PROPSET == 9
    #include "propset9.h"   // Some DryOS R55 (eos m3)
#elif CAM_PROPSET == 8
    #include "propset8.h"   // Some DryOS R55 (sx530, may be for earlier cameras than propset 7)
#elif CAM_PROPSET == 7
    #include "propset7.h"   // Some DryOS R55 (g7x)
#elif CAM_PROPSET == 6
    #include "propset6.h"   // DryOS R52
#elif CAM_PROPSET == 5
    #include "propset5.h"   // DryOS R50
#elif CAM_PROPSET == 4
    #include "propset4.h"
#elif CAM_PROPSET == 3
    #include "propset3.h"
#elif CAM_PROPSET == 2      // most digic3 cameras
    #include "propset2.h"
#elif CAM_PROPSET == 1      // most digic2 cameras
    #include "propset1.h"
#else
    #error unknown camera processor
#endif

#endif

extern long get_property_case(long id, void *buf, long bufsize);
extern long set_property_case(long id, void *buf, long bufsize);

extern void shooting_set_prop(int id, int v);
extern int shooting_get_prop(int id);

extern short get_parameter_size(long id);
extern long get_parameter_data(long id, void *buf, long bufsize);
extern long set_parameter_data(long id, void *buf, long bufsize);

extern int get_flash_params_count(void);

extern short get_uiprop_value(unsigned long id);
extern int uiprop_count;

#endif
