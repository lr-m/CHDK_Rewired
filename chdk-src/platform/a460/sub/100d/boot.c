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
// Values traced in PRIMARY_a460_100d.BIN:
//
//   stock JPEG   0xffe61674, 18426 bytes, 320x240 - byte identical to the
//                a470's and a480's, so boot_image.h transfers unchanged
//   MYCAM_INIT   0xffe68b0c   populates the table and copies the ROM JPEG into
//                             the RAM buffer Canon allocated
//   guard        0x0000cf0c   init tests it at entry and sets it at exit, so
//                             calling it early is safe: Canon's own later call
//                             sees the flag and returns immediately
//   table        0x0006e088   entry 0 is the startup image, {buffer, size}, and
//                             the size written there is 0x47FA = 18426
//
// Overwrite the buffer, never repoint it. The init does memcpy(table[0].buffer,
// ROM_JPEG, size); if the table pointed at our array when that ran, Canon would
// copy its own logo straight over our image data.
//
// Unlike the a470 and a480 this body is VxWorks and has no startup image *task*
// to replace, so there is nothing to delegate to - the hook runs from the first
// task creation instead (platform/generic/main.c) and calls the init itself.
// That is the part of this that is unproven.

#define MYCAM_INIT   ((void (*)(void))0xffe68b0c)
#define MYCAM_TABLE  0x0006e088

#include "boot_image.h"

// The startup sound, embedded exactly as the A480 embeds its own. It cannot
// come off the card: Canon plays entry 1 long before the filesystem is up, so
// anything loaded by platform_load_custom_sounds() is necessarily late and the
// best it can do is play a second clip after the stock one has already been
// heard. That is the "stock, then ours" this replaces.
//
// Entry 1's buffer is 11111 bytes on this body - read out of the init at
// 0xffe68b0c, which writes the five stock sizes at a 16 byte stride:
// 18426 / 11111 / 3244 / 3300 / 16415, the same set the A540 carries by name.
// So the ceiling here is a whole second, and the clip is well inside it; the
// size check below is what enforces that rather than this comment.
#include "boot_sound.h"

// Read by platform/a460/wrappers.c, which plays the same clip through the
// shutter slot as a fallback. Set once this has actually written entry 1, so
// the fallback does not then play it a second time.
volatile int a460_boot_sound_patched;

// Readable after the fact if module loading is ever fixed enough for Lua peek():
//   [0] hook entry count   [1] buffer pointer   [2] first two buffer bytes
//   [3] non-zero once the image was actually written
volatile unsigned chdk_startup_probe[4];

// Called from the task creation hook, so it gets a go on every task Canon
// spawns during startup - dozens of sample points spread across boot, for the
// cost of a pointer dereference and two byte compares each time.
//
// It deliberately does NOT call MYCAM_INIT() any more. Doing that from the
// first task creation is what stopped the a460 booting: the init writes through
// pointers held in the table, and that early they are not yet valid, so it
// faults before Canon has even started. The idempotency guard makes the call
// *repeatable*, which is not the same as making it *safe to run early*, and I
// read more into it than it says.
//
// Waiting for Canon to run its own init instead means we only ever write into a
// buffer that already demonstrably holds a decoded-ready JPEG. The cost is that
// we might not get a turn between Canon populating the buffer and Canon drawing
// from it - in which case the logo simply does not change, which is the failure
// we can afford.
void patch_startup_image(void)
{
    static int done;
    unsigned char *buf;
    int i;

    if (done) return;

    chdk_startup_probe[0]++;

    buf = *(unsigned char **)(MYCAM_TABLE);
    chdk_startup_probe[1] = (unsigned)buf;
    if (!buf) return;

    chdk_startup_probe[2] = ((unsigned)buf[0] << 8) | buf[1];

    // Only write if it really is holding a JPEG. If the init has not run
    // properly this is garbage or an unallocated buffer, and writing 18KB into
    // it would be the difference between "the logo did not change" and "the
    // camera does not boot".
    if (buf[0] != 0xFF || buf[1] != 0xD8) return;

    // The image now comes from a marker-tagged slot rather than a bare array,
    // so that modules/boot_image.c can replace it inside DISKBOOT.BIN and
    // Settings -> Boot screen works on this body too. The compiled-in default
    // is the same picture this port has always drawn.
    if (!BOOT_IMAGE_SLOT_VALID(boot_image_slot, BOOT_TAG_A460)) return;

    // Copy the image, then pad the rest of Canon's buffer with 0xff exactly as
    // the generated header used to be padded, so the bytes landing in the
    // buffer are identical to what this port wrote before the slot existed.
    for (i = 0; i < (int)boot_image_slot.size; i++)
        buf[i] = boot_image_slot.jpeg[i];
    for (; i < (int)boot_image_slot.capacity; i++)
        buf[i] = 0xff;

    // Entry 1 is Canon's startup sound, and this is the only place on this body
    // early enough to replace it. Reaching here means the init has already run
    // and copied the ROM assets in - entry 0 is holding a JPEG, and entry 1 is
    // filled by the same straight-line function a few instructions later - so
    // there is no ordering hazard and nothing of Canon's left to overwrite ours.
    //
    // Overwrite the buffer, never repoint it. That is the discipline this file
    // already uses for the image above, and it is also what the A480's verified
    // path does for this exact entry: Canon's startup player ignores a
    // replacement table pointer.
    {
        unsigned char *sbuf = *(unsigned char **)(MYCAM_TABLE + 16);
        unsigned ssize = *(unsigned *)(MYCAM_TABLE + 20);

        if (sbuf && ssize >= sizeof(boot_sound_wav))
        {
            unsigned k;
            for (k = 0; k < sizeof(boot_sound_wav); k++)
                sbuf[k] = boot_sound_wav[k];
            // The 16 byte entries on this body are {buffer, size} and the
            // getter does not rewrite the size from flash - unlike the A430's
            // and A540's - so shortening the clip here is honoured.
            *(unsigned *)(MYCAM_TABLE + 20) = sizeof(boot_sound_wav);
            a460_boot_sound_patched = 1;
        }
    }

    done = 1;
    chdk_startup_probe[3] = 1;
}


/* "relocated" functions */
void __attribute__((naked,noinline)) h_usrInit();
void __attribute__((naked,noinline)) h_usrKernelInit();
void __attribute__((naked,noinline)) h_usrRoot();



void boot()
{
    long *canon_data_src = (void*)0xFFEEF370;
    long *canon_data_dst = (void*)0x1900;
    long canon_data_len = 0xE460 - 0x1900;
    long *canon_bss_start = (void*)0xE460; // just after data
    long canon_bss_len = 0x95730 - 0xE460;
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
	"BL      sub_FFCC62F8\n"     //unknown_libname_206 ; "Canon A-Series Firmware"
	"BL      sub_FFCBB3C4\n"     //excVecInit
	"BL      sub_FFC011C4\n"
	"BL      sub_FFC01728\n"
	"LDR     LR, [SP],#4\n"
  "B       h_usrKernelInit\n"
    );
}

void  h_usrKernelInit()
{
    asm volatile (
	"STMFD   SP!, {R4,LR}\n"
	"SUB     SP, SP, #8\n"
	"BL      sub_FFCC67F8\n"    //classLibInit
	"BL      sub_FFCD6924\n"    //taskLibInit
	"LDR     R3, =0x4F18\n"
	"LDR     R2, =0x92720\n"
	"LDR     R1, [R3]\n"
	"LDR     R0, =0x93270\n"
	"MOV     R3, #0x100\n"
	"BL      sub_FFCD2514\n"   //qInit
	"LDR     R3, =0x4ED8\n"
	"LDR     R0, =0x5278\n"
	"LDR     R1, [R3]\n"
	"BL      sub_FFCD2514\n"   //qInit
	"LDR     R3, =0x4F94\n"
	"LDR     R0, =0x93244\n"
	"LDR     R1, [R3]\n"
	"BL      sub_FFCD2514\n"   //qInit
	"BL      sub_FFCDACE0\n"   //workQInit
	"BL      sub_FFC012B0\n"
	"MOV     R4, #0\n"
	"MOV     R3, R0\n"
	"MOV     R12, #0x800\n"
	"LDR     R0, =h_usrRoot\n"
	"MOV     R1, #0x4000\n"
    );    
//	"LDR     R2, =0xC5730\n"	// 0x95730 + 0x30000
    asm volatile (
        "LDR     R2, =new_sa\n"
        "LDR     R2, [R2]\n"
    );
    asm volatile (
	"STR     R12, [SP]\n"
	"STR     R4, [SP,#4]\n"
	"BL      sub_FFCD3B64\n"  //kernelInit 
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
	"BL      sub_FFCCB2B0\n"     //memInit
	"MOV     R1, R4\n"
	"MOV     R0, R5\n"
	"BL      sub_FFCCBD28\n"     //memPartLibInit
	"BL      sub_FFC01704\n"
	"BL      sub_FFC01A0C\n"
	"BL      sub_FFC019F0\n"
	"BL      sub_FFC01A38\n"
	"BL      sub_FFC019C4\n"
    );

    _taskCreateHookAdd(createHook);
    _taskDeleteHookAdd(deleteHook);

    drv_self_hide();

    asm volatile (
	"LDMFD   SP!, {R4,R5,LR}\n"
	"B       sub_FFC0136C\n"   //IsEmptyWriteCache_2
    );
}





