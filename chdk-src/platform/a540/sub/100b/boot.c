#include "lolevel.h"
#include "platform.h"
#include "core.h"

const char * const new_sa = &_end;

/* Ours stuff */
extern void createHook (void *pNewTcb);
extern void deleteHook (void *pTcb);


void boot();

#ifdef CAM_STARTUP_IMAGE
// ---------------------------------------------------------------------------
// Replacement startup image (Rewired Optics boot screen)
//
// Everything below was derived from PRIMARY_a540_100b.BIN (GM1.00B) via the
// Ghidra export in cameras/a540/ghidra/. This body is structurally the A430's
// case, not the A640's - see BOOTSCREEN_PORTING.md.
//
// The chain, from the task down to flash:
//
//   CreateTask("StartupImage", prio 25, stack 0x800)  at 0xffd7fd58
//     -> task entry            0xffd7fd2c   logs "_StartupImage", tail-calls
//        -> worker             0xffd7fbdc   calls the allocator, then draws
//           -> MyCamFuncImage  0xffebc99c   the draw call (asserts
//                                           "MyCamFuncImage.c")
//              -> 0xffebc588   get {ptr,size} for item 0x2000 out of the table
//
// 0xffebc588 does NOT hand back a flash pointer. It returns table[0].buffer -
// a RAM buffer that the allocator at 0xffebc68c malloc'd and filled from flash
// at startup. That is the whole reason this camera is portable: there is a RAM
// copy with a stable address to overwrite, exactly as on the A430.
//
//   MYCAM_TABLE 0x000737e0   (literal at 0xffebc688 and 0xffebc7d0; RAM, bss)
//     entry n at +n*0x0c, five entries, ids from the table at 0xffebc44c:
//       0x2000 startup image, 0x2001..0x2004 the four sounds
//       +0x00  void *buffer   malloc'd at runtime
//       +0x04  u32   size
//       +0x08  u16   cached theme id
//
// Entry 0's buffer is malloc'd at a HARDCODED 0x5000 (20480) bytes - the
// allocator special-cases index 0 rather than sizing it from flash. Our image
// is 18426 bytes, so it fits with 2054 to spare. This is the check that makes
// the memcpy below safe rather than hopeful.
//
// Canon re-copies from flash only when the cached theme id at +0x08 no longer
// matches the selected theme (property 0x4019). Unlike the A430 we do not have
// to force that field: the allocator writes the correct id itself on the way
// out, so by the time we return it already matches and 0xffebc588 will leave
// our buffer alone. Nothing overwrites us unless the user switches theme in
// the My Camera menu.
//
// The flash sector at 0xfff70000 is deliberately NOT written. It is persistent,
// and everything else this project does is undone by pulling the card.

#define MYCAM_TABLE         0x000737e0      /* entry 0 = startup image */

// The StartupImage task entry, from _CreateTask at 0xffd7fd58.
#define TASK_STARTUP_IMAGE  ((void (*)(void))0xffd7fd2c)

// 0xffebc68c - allocates the five My Camera asset buffers and fills the table.
// Guarded on its own done-flag at 0x0000a9fc (set to 1 on the way out), so
// calling it early is safe and Canon's own later call falls straight through.
#define MYCAM_ALLOC         ((void (*)(void))0xffebc68c)

#include "boot_image.h"

// Same passive discipline as the A430 and A460: this must never call Canon's
// flash loader itself. Doing that early on the A460 stopped the camera booting,
// because the loader writes through pointers that are not valid yet. We only
// ever overwrite a buffer the allocator has already populated.
//
// UNVERIFIED ON HARDWARE. Every address here is derived and cross-checked, and
// the mechanism matches a port that works, but nobody has booted an A540 with
// it. The probe below is what turns a black screen into a diagnosis instead of
// a guess. Read it with CHDK/SCRIPTS/probe.lua; the address moves on every
// rebuild, so re-read it from nm.
//
//   [9]  buffer pointer as the replacement task saw it
//   [10] the task wrote our image
//   [11] 0xa5 - the replacement task ran at all (read this one first)
volatile unsigned chdk_startup_probe[12];

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
        // t[1] is reset from the flash size by 0xffebc588 on every call, so
        // setting it here is pointless - the decoder is always told the stock
        // 18426. That is exactly our length, and the image is padded to it.
        chdk_startup_probe[10] = 1;
    }

    chdk_startup_probe[11] = 0xa5;      /* our task ran at all */

    TASK_STARTUP_IMAGE();
}
#endif /* CAM_STARTUP_IMAGE */


/* "relocated" functions */
void __attribute__((naked,noinline)) h_usrInit();
void __attribute__((naked,noinline)) h_usrKernelInit();
void __attribute__((naked,noinline)) h_usrRoot();

#if 0
#define LED_PR 0xc0220084

static void blink(int cnt)
{
	volatile long *p=(void*)LED_PR;
	int i;

	for(;cnt>0;cnt--){
		p[0]=0x46;

		for(i=0;i<0x200000;i++){
			asm ("nop\n");
			asm ("nop\n");
		}
		p[0]=0x44;
		for(i=0;i<0x200000;i++){
			asm ("nop\n");
			asm ("nop\n");
		}
	}
}

static void __attribute__((noreturn)) panic(int cnt)
{
        blink(cnt);
	shutdown();
}
#endif

void boot()
{

  asm volatile (
//        "B       0xFFC00000\n"
        "MOV     R0, #2\n"
        "TEQ     R0, #2\n"
        "LDR     SP, =0x1900\n"
        "MOV     R11, #0\n"
        
        "MOV     R0, #0x3D\n"
        "MCR     p15, 0, R0,c6,c0\n"
        "MOV     R0, #0xC000002F\n"
        "MCR     p15, 0, R0,c6,c1\n"
        "MOV     R0, #0x31\n"
        "MCR     p15, 0, R0,c6,c2\n"
        "LDR     R0, =0x10000031\n"
        "MCR     p15, 0, R0,c6,c3\n"
        "MOV     R0, #0x40000017\n"
        "MCR     p15, 0, R0,c6,c4\n"
        "LDR     R0, =0xFF80002D\n"
        "MCR     p15, 0, R0,c6,c5\n"
        "MOV     R0, #0x34\n"
        "MCR     p15, 0, R0,c2,c0\n"
        "MOV     R0, #0x34\n"
        "MCR     p15, 0, R0,c2,c0, 1\n"
        "MOV     R0, #0x34\n"
        "MCR     p15, 0, R0,c3,c0\n"
        "LDR     R0, =0x3333330\n"
        "MCR     p15, 0, R0,c5,c0, 2\n"
        "LDR     R0, =0x3333330\n"
        "MCR     p15, 0, R0,c5,c0, 3\n"
        "MRC     p15, 0, R0,c1,c0\n"
        "ORR     R0, R0, #0x1000\n"
        "ORR     R0, R0, #1\n"
        "MCR     p15, 0, R0,c1,c0\n"
        
        "STR     LR, [SP,#-4]!\n"
        "MRC     p15, 0, R0,c1,c0\n"
        "ORR     R0, R0, #0x1000\n"
        "ORR     R0, R0, #4\n"
        "ORR     R0, R0, #1\n"
        "MCR     p15, 0, R0,c1,c0\n"
        "LDR     R3, =0xB910\n"
        "MOV     R12, #0\n"
        "CMP     R12, R3\n"
        "LDR     R2, =0xFFEF3DF0\n"
        "MOV     R1, #0x1900\n"
        "BCS     loc_FFC00130\n"
        "MOV     LR, R3\n"
  "loc_FFC00114:\n"
        "LDR     R3, [R2]\n"
        "ADD     R12, R12, #4\n"
        "CMP     R12, LR\n"
        "STR     R3, [R1]\n"
        "ADD     R2, R2, #4\n"
        "ADD     R1, R1, #4\n"
        "BCC     loc_FFC00114\n"
  "loc_FFC00130:\n"
        "LDR     R1, =0xD210\n"
        "LDR     R3, =0x922D0\n"
        "MOV     R12, #0\n"
        "RSB     R3, R1, R3\n"
        "CMP     R12, R3\n"
        "BCS     loc_FFC0015C\n"
        "MOV     R2, R12\n"
  "loc_FFC0014C:\n"
        "ADD     R2, R2, #4\n"
        "CMP     R2, R3\n"
        "STR     R12, [R1],#4\n"
        "BCC     loc_FFC0014C\n"
  "loc_FFC0015C:\n"
        "MRC     p15, 0, R0,c1,c0\n"
        "ORR     R0, R0, #0x1000\n"
        "BIC     R0, R0, #4\n"
        "ORR     R0, R0, #1\n"
        "MCR     p15, 0, R0,c1,c0\n"
        "LDR     LR, [SP],#4\n"
//        "B       sub_FFC0198C\n"
        "B       h_usrInit\n"
  );
//  blink(2);

}


void h_usrInit()
{
  asm volatile (
        "STR     LR, [SP,#-4]!\n"
        "BL      sub_FFC01968\n"
        "MOV     R0, #2\n"
        "MOV     R1, R0\n"
        "BL      sub_FFEDA92C\n"
        "BL      sub_FFECD5A8\n"
        "BL      sub_FFC011C4\n"
        "BL      sub_FFC01728\n"
        "LDR     LR, [SP],#4\n"
//        "B       sub_FFC01744\n"
        "B       h_usrKernelInit\n"
  );
}

void  h_usrKernelInit()
{

  asm volatile (
        "STMFD   SP!, {R4,LR}\n"
        "SUB     SP, SP, #8\n"
        "BL      sub_FFEDAE2C\n"
        "BL      sub_FFEEDC14\n"
        "LDR     R3, =0xC230\n"
        "LDR     R2, =0x8E900\n"
        "LDR     R1, [R3]\n"
        "LDR     R0, =0x91C90\n"
        "MOV     R3, #0x100\n"
        "BL      sub_FFEE6D24\n"
        "LDR     R3, =0xC1F0\n"
        "LDR     R0, =0xCA38\n"
        "LDR     R1, [R3]\n"
        "BL      sub_FFEE6D24\n"
        "LDR     R3, =0xC2AC\n"
        "LDR     R0, =0x91C64\n"
        "LDR     R1, [R3]\n"
        "BL      sub_FFEE6D24\n"
        "BL      sub_FFEF1FD0\n"
        "BL      sub_FFC012B0\n"
        "MOV     R4, #0\n"
        "MOV     R3, R0\n"
        "MOV     R12, #0x800\n"
        
//        "LDR     R0, =0xFFC01A60\n"
//        "MOV     R1, #0x4000\n"
//        "LDR     R2, =0x922D0\n"
        
        "LDR     R0, =h_usrRoot\n"
        "MOV     R1, #0x4000\n"
        );    
//        "LDR     R2, =0xD22D0\n" // 0x922D0 + MEMISOSIZE(0x40000)
        asm volatile (
            "LDR     R2, =new_sa\n"
            "LDR     R2, [R2]\n"
        );
        asm volatile (

        "STR     R12, [SP]\n"
        "STR     R4, [SP,#4]\n"
        "BL      sub_FFEEAE54\n"
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
        "BL      sub_FFEDFAC0\n"
        "MOV     R1, R4\n"
        "MOV     R0, R5\n"
        "BL      sub_FFEE0538\n"
        "BL      sub_FFC017E8\n"
        "BL      sub_FFC01704\n"
        "BL      sub_FFC01A0C\n"
        "BL      sub_FFC019F0\n"
        "BL      sub_FFC01A38\n"
        "BL      sub_FFC019C4\n"
    );

// patch begin
    _taskCreateHookAdd(createHook);
    _taskDeleteHookAdd(deleteHook);

   drv_self_hide();

// patch end

    asm volatile (
        "LDMFD   SP!, {R4,R5,LR}\n"
        "B       sub_FFC0136C\n"
    );
}
