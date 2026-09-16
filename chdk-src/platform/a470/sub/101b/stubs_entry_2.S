#include "stubs_asm.h"
#define NULL_SUB 0xffc00948

// Override stubs_entry.S
NHSTUB(PutInNdFilter,               0xFFE267B0)
NHSTUB(PutOutNdFilter,              0xFFE267E8)
NHSTUB(kbd_pwr_off,                 NULL_SUB)
NHSTUB(kbd_pwr_on,                  NULL_SUB)

NHSTUB(GetZoomLensCurrentPositionMy,NULL_SUB)
NHSTUB(SetZoomActuatorSpeedPercent, NULL_SUB)

// Additional
// SetDate(struct { int year, month, day, hour, min, sec; } *)
// Same function 102c carries at 0xFFE2CF74, re-derived here rather than copied:
// its head is the identical range check - year-1970 <= 99 (sub r0,r4,#0x700 /
// sub r0,r0,#0xb2 / cmp r0,#0x63), month-1 <= 11, day-1 <= 30. Used to set the
// clock at boot so Canon stops showing its date/time screen when the backup
// battery is dead. Not called unless CAM_SET_DATE_ON_BOOT is defined, and it is
// not - see platform_camera.h.
NHSTUB(SetDate,                     0xFFE2C534)

// Corrected while porting the 102c work back to this firmware. The old value
// here, 0xFFD0B374, is a different function altogether: it pushes {r3-r9,lr} and
// dispatches on a struct at 0xffd0b604, where MakeAFScan pushes {r3-fp,lr} and
// loads its scan state from 0xffdd7770. 0xffdd7538 is the instruction-for-
// instruction match of 102c's 0xFFDD7F78, which is the address that body's port
// was corrected to.
NHSTUB(MakeAFScan,                  0xFFDD7538)
NHSTUB(RefreshUSBMode,              0xFFC27730)

// Also corrected. TurnOnMic/TurnOffMic are an adjacent pair that differ in one
// instruction - `orr r0,r0,#0x20` sets the mute bit, `bic r0,r0,#0x20` clears it
// - and the old TurnOffMic address, 0xFFC311C4, pointed past the end of the bic
// half at the function after it. 0xFFC3119C is the bic one, and it matches
// 102c's 0xFFC31660 instruction for instruction.
NHSTUB(TurnOffMic,                  0xFFC3119C)
NHSTUB(TurnOnMic,                   0xFFC31170)
