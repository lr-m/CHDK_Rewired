#include "stubs_asm.h"
#define NULL_SUB 0xFFEBDC9C

// Override stubs_entry.S
NHSTUB(ExitFromCompensationEVF,     0xffd55be0)
NHSTUB(free,                        0xFFEFD1AC)

// These don't exist
NHSTUB(SetZoomActuatorSpeedPercent, NULL_SUB)

// Additional
NHSTUB(ExpCtrlTool_StartContiAE,    0xFFCF89C8)
NHSTUB(ExpCtrlTool_StopContiAE,     0xFFCF89F8)
NHSTUB(TurnOffMic,                  0xffc1f588)
NHSTUB(TurnOnMic,                   0xffc1f54c)
NHSTUB(GetVRAMHPixelsSize,0xFFEB704C) // by find_eventproc name @ 0xFFF18ECC
NHSTUB(GetVRAMVPixelsSize,0xFFEB7058) // by find_eventproc name @ 0xFFF18ED4
