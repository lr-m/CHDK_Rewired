#ifndef GBC_PORT_H
#define GBC_PORT_H

//-------------------------------------------------------------------
// The CHDK side of the koenk/gbc port.
//
// The upstream core is written against a hosted C library. CHDK is not that:
// there is no <stdint.h>, no <stdbool.h>, no stdio, and malloc is a very
// small arena. This header is included ahead of every upstream file (via
// -include on the module's compile line) and supplies exactly the pieces the
// core actually touches, so the vendored sources stay close to upstream and
// stay diffable against it.
//
// Everything here is deliberately thin. Where the core wanted an fprintf and
// an exit(), it gets a trap that unwinds back to the module instead - a game
// that hits an unimplemented opcode should drop the player back to the menu,
// not take the camera down with it.
//-------------------------------------------------------------------

#ifdef GBC_HOST_TEST

// Host build (tools/gbcam_selftest.c). The sensor pipeline is pure
// arithmetic, so it runs on the development machine against a real libc and
// the results can be looked at as an image.
#include <stdio.h>       // the core's debug chatter is real printf on the host
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <fcntl.h>      // the pager's open/read/lseek are POSIX on the host
#include <unistd.h>

#else

#include "stddef.h"      // size_t, NULL - CHDK's own, so nothing is redefined
#include "stdlib.h"
#include "string.h"

//-------------------------------------------------------------------
// Fixed-width types. CHDK has no <stdint.h>; these match the ARM ABI the
// cross-compiler is using and are checked at build time in gbcam_selftest.c.

typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned short      uint16_t;
typedef signed short        int16_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
typedef unsigned long long  uint64_t;
typedef signed long long    int64_t;

#ifndef __cplusplus
typedef int                 bool;
#define true  1
#define false 0
#endif


#endif /* GBC_HOST_TEST */

//-------------------------------------------------------------------
// Zeroing allocator. The core calls calloc() in two places and CHDK has no
// such thing.
//
// Note malloc, not umalloc, here and everywhere else in this port. umalloc is
// _AllocateUncacheableMemory - it exists for buffers shared with hardware DMA,
// and nothing the emulator owns is. Putting the emulated machine's RAM in
// uncacheable memory costs roughly 20-40x on every access, which measured as
// 400ms for a single Game Boy frame. CHDK's malloc chains aram_heap ->
// exmem_heap -> Canon's heap, and all three are cached.

static inline void *ucalloc(unsigned int n)
{
    void *p = malloc(n);
    if (p)
        memset(p, 0, n);
    return p;
}

// The core asserts on hardware invariants it cannot recover from. There is no
// stderr to print to, so an assertion becomes a panic unwind back to the
// module - see gbc_panic() below.
#ifndef assert
#define assert(c)   do { if (!(c)) gbc_panic("assertion: " #c); } while (0)
#endif

//-------------------------------------------------------------------
// Error handling.
//
// Upstream's cpu_error()/mmu_error() print and exit(1). Neither is available
// and neither is wanted: this runs inside the camera's GUI task.
//
// The obvious replacement is setjmp/longjmp, and that is what this did first.
// It does not work: setjmp is not in modules/exportlist.inc, so a module that
// calls it fails at elf2flt time. Hand-rolling one in ARM assembly to unwind
// out of the middle of the emulated CPU is not a trade worth making for an
// error path.
//
// So a fault is a sticky flag instead. gbc_panic() records the reason and
// sets it; the failing read returns 0xff and the instruction finishes on
// garbage, but gbc_step_frame() checks the flag after every instruction, so
// at most one bad instruction is retired before the module tears the emulator
// down. Nothing in the emulated machine can reach outside its own allocated
// buffers, so a junk instruction is junk on the emulated side only.

extern char gbc_panic_msg[64];
extern int  gbc_panicked;

// Records a short reason and raises the flag. Format arguments are discarded -
// there is no vsnprintf worth linking here, and the pc is recorded separately
// by dbg_run_debugger().
void gbc_panic(const char *what);

//-------------------------------------------------------------------
// Upstream debug chatter. The core is peppered with printf() under debug
// macros that are compiled out in a release build; these keep the source
// compiling unchanged if someone turns them back on.

#ifndef GBC_HOST_TEST
#define printf(...)             ((void)0)
#define fprintf(...)            ((void)0)
#define snprintf(b,n,...)       ((void)((b) && ((b)[0] = 0)), 0)
#endif

//-------------------------------------------------------------------
// Geometry, shared by the emulator, the blitter and the sensor.

#define GB_LCD_W        160
#define GB_LCD_H        144

// The sensor is 128x128 but the cartridge only digitises 128x112 of it; the
// extra lines are the dark reference rows at the top and bottom.
#define GBCAM_W                 128
#define GBCAM_H                 112
#define GBCAM_EXTRA_LINES       8
#define GBCAM_SENSOR_W          GBCAM_W
#define GBCAM_SENSOR_H          (GBCAM_H + GBCAM_EXTRA_LINES)

//-------------------------------------------------------------------
// Where things live on the card.

#define GBC_ROM_DIR     "A/CHDK/GBC"
#define GBC_SAVE_DIR    "A/CHDK/GBC/SAVE"
#define GBC_PHOTO_DIR   "A/CHDK/GBC/PHOTOS"

// And the copy that is never overwritten - see gbc_save_export_slot().
//
// The album holds thirty pictures and the ROM reuses the slots, so the export
// named after a slot is destroyed the moment that slot is shot over. This
// directory is numbered instead, sits beside the camera's own DCIM folders so
// it comes off the card with them, and is the reason the thirty is no longer a
// limit on how many photographs a session can keep.
#define GBC_DCIM_DIR    "A/DCIM/GBC"

#endif
