#include "lolevel.h"
#include "platform.h"
#include "core.h"

const char * const new_sa = &_end;

/* Ours stuff */
extern long wrs_kernel_bss_start;
extern long wrs_kernel_bss_end;
extern void createHook (void *pNewTcb);
extern void deleteHook (void *pTcb);


void boot();

//-------------------------------------------------------------------
// Custom startup image - see BOOTSCREEN_PORTING.md.
//
// This camera does not work the way the a460/a470/a480 do, and the difference
// is why the first attempt at it concluded the port was impossible.
//
// There is no single startup JPEG in ROM reached by a pointer literal. Instead
// there are four My Camera theme containers - 0xfff30000, 0xfff40000,
// 0xfff50000, 0xfff70000 - and 0xfff70000 is a *user-writable flash slot*
// (magic 0xa5a5 at 0xfff7fffe, length at 0xfff7fff4) which is where the My
// Camera transfer feature lands. Nothing in the ROM points at any of them,
// which is why every literal scan came up empty.
//
// The display path instead goes:
//
//   FUN_ffe8bfbc(kind, &ptr, &size)     the equivalent of MYCAM_INIT
//     -> FUN_ffe8d074 / FUN_ffe8d0d8    size / memcpy the asset out
//        -> FUN_ffe8d008               walk the 26-byte directory entries
//           -> FUN_ffe8d55c            hand out the slot base 0xfff70000
//
// and caches the result in a table, which is what we patch. Entry 0 is the
// startup image: FUN_ffe8be98(0x2000,1) returns asset index 0 and
// FUN_ffe8be98(0x2000,2) returns cache index 0.
//
//   MYCAM_TABLE 0x000812d8    (literal at 0xffe8c0bc; RAM, inside bss)
//     entry n at +n*0x0c
//       +0x00  void *buffer   allocated at runtime
//       +0x04  u32   size
//       +0x08  u16   cached slot id
//
// The 12-byte stride and the extra slot field are the only structural
// differences from the other three cameras' 16-byte {buffer, size} entries.
//
// Canon re-copies only when the cached slot id no longer matches the selected
// theme, so once the buffer holds our image nothing overwrites it unless the
// user switches theme in the My Camera menu. That is the same accident of
// timing the idempotency guard gives us on the other bodies.
//
// The flash slot is deliberately NOT written. It is persistent, and everything
// else this project does is recoverable by pulling the card.

#define MYCAM_TABLE  0x000812d8     /* entry 0 = startup image */

// The StartupImage task, from _CreateTask("StartupImage", 25, 0x800, 0xffd61e4c, 0)
// at 0xffd61e78. The entry logs "_StartupImage" then tail-calls the worker at
// 0xffd61cfc, which allocates the asset buffers and draws the splash.
#define TASK_STARTUP_IMAGE  ((void (*)(void))0xffd61e4c)

// FUN_ffe8c0c0 - allocates the six My Camera asset buffers and fills in the
// table. Guarded on its own done-flag, so calling it early is safe and Canon's
// own later call falls straight through.
#define MYCAM_ALLOC         ((void (*)(void))0xffe8c0c0)

#include "boot_image.h"

// Same passive discipline as the a460, and for the same hard-won reason: this
// must never call Canon's loader itself. Calling the equivalent early on the
// a460 stopped that camera booting, because the loader writes through pointers
// that are not valid yet. We only ever overwrite a buffer that already visibly
// holds a decoded-ready JPEG.
//
// Our image is exactly 18426 bytes, which is exactly what this camera's theme
// slot declares, so the size field at +0x04 needs no adjustment.
//
// DIAGNOSTIC BUILD. The first attempt latched itself off after one successful
// write and the logo did not change, which leaves several explanations that a
// probe can separate: the hook never sees a populated buffer, or it sees one too
// late, or Canon re-copies over us, or entry 0 is not the entry being drawn.
// See BOOTSCREEN_PORTING.md "The diagnostic loop".
//
// Probe kept in the shipped build - 48 bytes of BSS, and the next VxWorks port
// will want it. Read with CHDK/SCRIPTS/probe.lua; the address moves on every
// rebuild, so re-read it from nm.
//
//   [9]  buffer pointer as the replacement task saw it
//   [10] the task wrote our image
//   [11] 0xa5 - the replacement task ran at all (read this one first)
volatile unsigned chdk_startup_probe[12];

// The selected My Camera theme. 1 is the single flash slot FUN_ffe8d55c serves,
// confirmed by probe on this body.
#define MYCAM_SLOT   1

// ---------------------------------------------------------------------------
// The actual fix: replace the StartupImage task rather than race it.
//
// Patching the buffer from the task-creation hook cannot work on this camera and
// the probe proved why. FUN_ffe8c3d0 calls FUN_ffe8bfbc to get the image pointer
// - which is what *fills* the buffer - and draws from it on the next line. Fill
// and draw are back to back inside one function, so there is no window: every
// write from outside lands after the splash is already on screen. The evidence
// was our image sitting in the buffer afterwards, and the My Camera menu
// previewing our image, while the boot logo stayed Canon's.
//
// Replacing the task entry puts us *before* both. Then, in order:
//
//   1. call the allocator ourselves, so table[0].buffer exists
//   2. write our image into it and mark the entry already-loaded
//   3. hand control to Canon's original task entry
//
// Step 2 is what makes step 3 harmless: FUN_ffe8bfbc only re-copies from flash
// when the cached slot id does not match the selected theme, so an entry that
// claims to be loaded is drawn as-is.
//
// Same shape as the a470's my_startup_image_task, which calls MYCAM_INIT before
// overwriting for exactly this reason.
void my_startup_image_task(void)
{
    volatile unsigned *t = (volatile unsigned *)MYCAM_TABLE;
    unsigned char *buf;
    int i;

    MYCAM_ALLOC();

    buf = (unsigned char *)t[0];
    chdk_startup_probe[9] = (unsigned)buf;

    if (buf)
    {
        for (i = 0; i < (int)sizeof(boot_image_jpeg); i++)
            buf[i] = boot_image_jpeg[i];
        t[1] = sizeof(boot_image_jpeg);
        t[2] = (t[2] & 0xffff0000) | MYCAM_SLOT;
        chdk_startup_probe[10] = 1;
    }

    chdk_startup_probe[11] = 0xa5;      /* our task ran at all */

    TASK_STARTUP_IMAGE();
}

/* "relocated" functions */
void __attribute__((naked,noinline)) h_usrInit();
void __attribute__((naked,noinline)) h_usrKernelInit();
void __attribute__((naked,noinline)) h_usrRoot();


#if 0
void blink_blue(int duration)
{
  int i;

  *((volatile long *) 0xc0220084) = 0x46; // Turn on LED   
  for (i=0; i<duration; i++) // Wait a while   
  {   
    asm volatile ( "nop\n" );   
  }   
   
  *((volatile long *) 0xc0220084) = 0x44; // Turn off LED   
  for (i=0; i<duration; i++) // Wait a while   
  {   
    asm volatile ( "nop\n" );   
  }
}

void blink_orange(int duration)
{
  int i;

  *((volatile long *) 0xc0220080) = 0x46; // Turn on LED   
  for (i=0; i<duration; i++) // Wait a while   
  {   
    asm volatile ( "nop\n" );   
  }   
   
  *((volatile long *) 0xc0220080) = 0x44; // Turn off LED   
  for (i=0; i<duration; i++) // Wait a while   
  {   
    asm volatile ( "nop\n" );   
  }
}

void blink()
{
    blink_blue(10000000);
    blink_orange(10000000);
    blink_blue(10000000);
}
#endif

void boot()
{
    long *canon_data_src = (void*)0xFFEC54F0;
    long *canon_data_dst = (void*)0x1900;
    long canon_data_len = 0xEBC0 - 0x1900;
    long *canon_bss_start = (void*)0xEBC0; // just after data
    long canon_bss_len = 0x9F7D0 - 0xEBC0;
    long i;

    asm volatile (
	"MRC     p15, 0, R0,c1,c0\n"
	"ORR     R0, R0, #0x1000\n"
	"ORR     R0, R0, #4\n"
	"ORR     R0, R0, #1\n"
	"MCR     p15, 0, R0,c1,c0\n"
    :::"r0");

    for(i=0;i<canon_data_len/4;i++)
	canon_data_dst[i]=canon_data_src[i];

    for(i=0;i<canon_bss_len/4;i++)
	canon_bss_start[i]=0;

    asm volatile (
	"MRC     p15, 0, R0,c1,c0\n"
	"ORR     R0, R0, #0x1000\n"
	"BIC     R0, R0, #4\n"
	"ORR     R0, R0, #1\n"
	"MCR     p15, 0, R0,c1,c0\n"
    :::"r0");

    h_usrInit();
}


void h_usrInit()
{
    asm volatile (
	"STR     LR, [SP,#-4]!\n"
	"BL      sub_FFC01968\n"
	"MOV     R0, #2\n"
	"MOV     R1, R0\n"
	"BL      sub_FFEAB86C\n"     //unknown_libname_773 ; "Canon A-Series Firmware"
	"BL      sub_FFE9CEDC\n"     //excVecInit
	"BL      sub_FFC011C4\n"
	"BL      sub_FFC01728\n"
	"LDR     LR, [SP],#4\n"
	"B       h_usrKernelInit\n"
    );
}

void  h_usrKernelInit()
{
//    blink();
    asm volatile (
	"STMFD   SP!, {R4,LR}\n"
	"SUB     SP, SP, #8\n"
	"BL      sub_FFEABD6C\n"    //classLibInit
	"BL      sub_FFEBEB54\n"    //taskLibInit
	"LDR     R3, =0xDBD8\n"
	"LDR     R2, =0x9C460\n"
	"LDR     R1, [R3]\n"
	"LDR     R0, =0x9F190\n"
	"MOV     R3, #0x100\n"
	"BL      sub_FFEB7C64\n"   //qInit
	"LDR     R3, =0xDB98\n"
	"LDR     R0, =0xE3E0\n"
	"LDR     R1, [R3]\n"
	"BL      sub_FFEB7C64\n"   //qInit
	"LDR     R3, =0xDC54\n"
	"LDR     R0, =0x9F164\n"
	"LDR     R1, [R3]\n"
	"BL      sub_FFEB7C64\n"   //qInit
	"BL      sub_FFEC36D4\n"   //workQInit
	"BL      sub_FFC012B0\n"
	"MOV     R4, #0\n"
	"MOV     R3, R0\n"
	"MOV     R12, #0x800\n"
	"LDR     R0, =h_usrRoot\n"
	"MOV     R1, #0x4000\n"
    );
    asm volatile (
        "LDR     R2, =new_sa\n"
        "LDR     R2, [R2]\n"
    );
    asm volatile (
	"STR     R12, [SP]\n"
	"STR     R4, [SP,#4]\n"
	"BL      sub_FFEBBD94\n"  //kernelInit
	"ADD     SP, SP, #8\n"
	"LDMFD   SP!, {R4,PC}\n"
    );
}


void  h_usrRoot()
{
    asm volatile (
	"STMFD   SP!, {R4,R5,LR}\n"
	"MOV     R5, R0\n"
	"MOV     R4, R1\n"
	"BL      sub_FFC019D0\n"
	"MOV     R1, R4\n"
	"MOV     R0, R5\n"
	"BL      sub_FFEB0A00\n"     //memInit
	"MOV     R1, R4\n"
	"MOV     R0, R5\n"
	"BL      sub_FFEB1478\n"     //memPartLibInit
	"BL      sub_FFC017E8\n"     //SetZoomActuatorSpeedPercent
	"BL      sub_FFC01704\n"
	"BL      sub_FFC01A10\n"
	"BL      sub_FFC019F4\n"
	"BL      sub_FFC01A3C\n"
	"BL      sub_FFC019C4\n"
    );

    _taskCreateHookAdd(createHook);
    _taskDeleteHookAdd(deleteHook);

    drv_self_hide();
//    blink(); //    "BL      blink\n"

    asm volatile (
	"LDMFD   SP!, {R4,R5,LR}\n"
	"B       sub_FFC0136C\n"
    );
}
