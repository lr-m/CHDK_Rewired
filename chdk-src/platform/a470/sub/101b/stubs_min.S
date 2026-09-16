#include "stubs_asm.h"

DEF(led_table, 0x22AC) //Found using LEDDrv.c

// ---------------------------------------------------------------------------
// Ported from sub/102c by matching this ROM against that one - see
// docs/A470_101B_PORTING.md. Every address below was re-derived from
// cameras/a470/firmware/PRIMARY_a470_101b.BIN; none is copied across.
// ---------------------------------------------------------------------------

// Canon's startup-image task. Present in funcs_by_address.csv but not emitted
// into stubs_entry.S, so it lives here where the signature finder won't touch it.
// Entry logs f(0x20,"_StartupImage") then tail-branches to the worker at 0xffc1a6ac.
//
// The same address as 102c, and that is a result rather than an assumption: the
// two images are instruction-for-instruction identical here, down to the
// worker's branch target, because nothing below 0xffc1c000 moved between the
// revisions. The "_StartupImage" string is at 0xffc1a000 in both.
DEF(task_StartupImage, 0xffc19d68)

// Gate flag tested at the top of RefreshPhysicalScreen (0xffd66e5c on this
// firmware, per stubs_entry.S):
//     ldr r0, [r4, #0x30] / cmp r0, #1 / beq <skip repaint>
// r4 is the window-manager struct, *(0xffd669b4) = 0xa3a0, so the flag is 0xa3d0.
//
// NOTE THE POLARITY. As on 102c, 1 means "blocked" and 0 means "allowed" - the
// opposite of the a480's like-named counter. Canon's own setters confirm the
// sense on this image too: 0xffd66ecc stores 0, 0xffd66edc stores 1, and the
// predicate at 0xffd66eec returns (1 - flag). The function is identical to
// 102c's instruction for instruction; only the literal pool moved.
DEF(refresh_physical_screen_blocked, 0xa3a0 + 0x30)

// Address of the viewport buffer Canon is currently displaying. See the note in
// sub/102c/stubs_min.S for why the fixed address in vid_get_viewport_fb() is not
// enough on this body.
//
// Found the same way, in this image: the leaf at 0xffcc1fb4 is
// "return *(0x3e274)", and the producer loop at 0xffca054c calls it and walks a
// table comparing against the result - `bl 0xffcc1fb4 / ldr r1,[r9,r6,lsl#2] /
// cmp r0,r1`, which is the same three-instruction signature that identifies it
// on 102c.
DEF(current_viewport_buffer, 0x0003e274)
