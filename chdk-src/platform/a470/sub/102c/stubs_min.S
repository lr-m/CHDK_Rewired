#include "stubs_asm.h"

DEF(led_table, 0x22AC) //Found using LEDDrv.c

// Canon's startup-image task. Present in funcs_by_address.csv but not emitted
// into stubs_entry.S, so it lives here where the signature finder won't touch it.
// Entry logs f(0x20,"_StartupImage") then tail-branches to the worker at 0xffc1a6ac.
DEF(task_StartupImage, 0xffc19d68)

// Gate flag tested at the top of RefreshPhysicalScreen (0xffd6789c):
//     ldr r0, [r4, #0x30] / cmp r0, #1 / beq <skip repaint>
// r4 is the window-manager struct, *(0xffd673f4) = 0xa3f0, so the flag is 0xa420.
//
// NOTE THE POLARITY. On this DryOS R23 firmware 1 means "blocked" and 0 means
// "allowed" - the opposite of the a480's like-named counter, which ScreenLock
// increments and RefreshPhysicalScreen decrements to zero. Hence the different
// name here: assigning a480 semantics to this variable would turn the display
// off rather than on. Canon's own setters confirm the sense - 0xffd6790c stores
// 0, 0xffd6791c stores 1, and the predicate at 0xffd6792c returns (1 - flag).
DEF(refresh_physical_screen_blocked, 0xa3f0 + 0x30)

// Address of the viewport buffer Canon is currently displaying.
//
// This camera cycles the live view through several frame buffers, so the fixed
// address in vid_get_viewport_fb() is only the one that happens to be live
// every few frames - writing to it gives a picture that flickers rather than
// holds.
//
// Found in ImgDDev.c: the accessor at 0xffcc24d0 is just "return *(0x3e2b4)",
// and the firmware itself treats that value as the current buffer - the loop at
// 0xffca0a68 calls it and walks a table comparing against the result to recover
// the buffer index. So this word is the authoritative answer to "which one is
// on screen right now".
DEF(current_viewport_buffer, 0x0003e2b4)
