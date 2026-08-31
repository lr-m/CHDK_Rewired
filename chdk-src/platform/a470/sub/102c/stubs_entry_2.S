#include "stubs_asm.h"
#define NULL_SUB 0xffc00948

// Override stubs_entry.S
NHSTUB(PutInNdFilter,               0xFFE271F0)
NHSTUB(PutOutNdFilter,              0xFFE27228)
NHSTUB(kbd_pwr_off,                 NULL_SUB)
NHSTUB(kbd_pwr_on,                  NULL_SUB)

// These don't exist
NHSTUB(SetZoomActuatorSpeedPercent, NULL_SUB)

// Additional
// SetDate(struct { int year, month, day, hour, min, sec; } *)
// Signature read off the range checks at its head: year-1970 <= 99,
// month-1 <= 11, day-1 <= 30. Used to set the clock at boot so Canon stops
// showing its date/time screen when the backup battery is dead.
NHSTUB(SetDate,                     0xFFE2CF74)
NHSTUB(MakeAFScan,                  0xFFDD7F78)
NHSTUB(RefreshUSBMode,              0xFFC27730)
NHSTUB(TurnOffMic,                  0xFFC31660)
NHSTUB(TurnOnMic,                   0xFFC31634)

//NHSTUB(LogCameraEvent,                   0xFFC547F4)