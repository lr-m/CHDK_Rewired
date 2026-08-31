#ifndef HEXBOX_H
#define HEXBOX_H

#include "flt.h"

// enforce word alignment
#define HEXBOX_FLAG_WALIGN 0x10

//#define HEXBOX_FLAG_DECIMAL 0x20

// Update version if changes are made to the module interface
#define GUI_HEXBOX_VERSION      {1,0}

typedef struct
{
    base_interface_t    base;

    int (*hexbox_init)(int *num, char *title, int flags);
} libhexbox_sym;

extern libhexbox_sym* libhexbox;

#endif
