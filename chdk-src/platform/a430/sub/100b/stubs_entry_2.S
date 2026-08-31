#include "stubs_asm.h"
#define NULL_SUB 0xFFC017E8

// Override stubs_entry.S
NHSTUB(EnterToCompensationEVF,      0xFFD28268)
NHSTUB(ExitFromCompensationEVF,     0xFFD282C4)
NHSTUB(MakeAFScan,                  0xFFD2AC58)
NHSTUB(WriteSDCard,                 0xFFC6C3DC)
NHSTUB(free,                        0xFFEB1EB0)
NHSTUB(kbd_pwr_off,                 NULL_SUB)
NHSTUB(kbd_pwr_on,                  NULL_SUB)

// These don't exist
NHSTUB(SetZoomActuatorSpeedPercent, NULL_SUB)

// Additional
NHSTUB(ExpCtrlTool_StartContiAE,    0xFFCDBAEC)
NHSTUB(ExpCtrlTool_StopContiAE,     0xFFCDBB1C)
NHSTUB(TurnOffMic,                  0xFFC1E40C)
NHSTUB(TurnOnMic,                   0xFFC1E3D0)
NHSTUB(GetVRAMHPixelsSize,0xFFE6DC44) // by find_eventproc name @ 0xFFECF74C
NHSTUB(GetVRAMVPixelsSize,0xFFE6DC50) // by find_eventproc name @ 0xFFECF754
