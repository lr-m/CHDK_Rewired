#include "stubs_asm.h"

DEF(canon_shoot_menu_active,            0xFFC00414)     //it works, but in AFL mode OSD disappers
DEF(recreview_hold,                     0xFFC00414)     // not found
DEF(enabled_refresh_physical_screen,    0x73A0 + 0x30)  //RefreshPhysicalScreen


// Canon's startup-image task. Entry logs f(0x60,"_SImage") then tail-calls
// the worker at 0xffc18f44. Task name string "StartupImage" @0xffc187e0.
// Not found by the signature finder; taken from funcs_by_address.csv.
DEF(task_StartupImage,                  0xffc1856c)
