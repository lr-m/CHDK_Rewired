#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include <inttypes.h>

#include <capstone.h>


#include "stubs_load.h"
#include "firmware_load_ng.h"
#include "ptp_op_names.h"

// arbitrary standardized constant for search "near" a string ref etc
// could base on ADR etc reach
#define SEARCH_NEAR_REF_RANGE 1024

#define SIG_NEAR_OFFSET_MASK    0x00FF
#define SIG_NEAR_COUNT_MASK     0xFF00
#define SIG_NEAR_COUNT_SHIFT    8
#define SIG_NEAR_REV            0x10000
#define SIG_NEAR_INDIRECT       0x20000
#define SIG_NEAR_JMP_SUB        0x40000
#define SIG_NEAR_AFTER(max_insns,n) (((max_insns)&SIG_NEAR_OFFSET_MASK) \
                                | (((n)<<SIG_NEAR_COUNT_SHIFT)&SIG_NEAR_COUNT_MASK))
#define SIG_NEAR_BEFORE(max_insns,n) (SIG_NEAR_AFTER(max_insns,n)|SIG_NEAR_REV)
#define SIG_NEAR_GET_OFFSET(param) ((param)&SIG_NEAR_OFFSET_MASK)
#define SIG_NEAR_GET_COUNT(param) (((param)&SIG_NEAR_COUNT_MASK)>>SIG_NEAR_COUNT_SHIFT)

// generic param bits for specifying which memory regions to search
// currently supported by
//  sig_match_near_str / users of find_call_near_str (but not indirect)
//  sig_match_str_arg_call / users of find_str_arg_call
//  sig_match_func_using_str
#define SIG_SEARCH_SHIFT 24
#define SIG_SEARCH_ROM (SEARCH_F_ROM_CODE << SIG_SEARCH_SHIFT) // search string in romcode (default if no others specified)
#define SIG_SEARCH_RAM (SEARCH_F_RAM_CODE << SIG_SEARCH_SHIFT) // search string in ramcode
#define SIG_SEARCH_TCM (SEARCH_F_TCM_CODE << SIG_SEARCH_SHIFT) // search string in ramcode
#define SIG_SEARCH_MASK (0x7 << SIG_SEARCH_SHIFT)
// convert SIG_SEARCH bits to SEARCH_F bits, default to main ROM if not set
#define SIG_SEARCH_GET_F(param) (((param) & SIG_SEARCH_MASK)?(((param) & SIG_SEARCH_MASK)>> SIG_SEARCH_SHIFT):SEARCH_F_ROM_CODE)

// param macros for sig_match_str_arg_call / users of find_str_arg_call
#define SIG_STRCALL_ARG_MASK    0x3
#define SIG_STRCALL_ARG(arg_num) (arg_num)
// TODO these should be a bitmask of call/jmp, imm/reg
#define SIG_STRCALL_TYPE_SHIFT  2
#define SIG_STRCALL_TYPE_MASK   (0x7 << SIG_STRCALL_TYPE_SHIFT)
#define SIG_STRCALL_CALL_IMM        (0 << SIG_STRCALL_TYPE_SHIFT)
#define SIG_STRCALL_JMP_IMM         (1 << SIG_STRCALL_TYPE_SHIFT)
#define SIG_STRCALL_CALL_JMP_IMM    (2 << SIG_STRCALL_TYPE_SHIFT)
#define SIG_STRCALL_JMP_REG         (4 << SIG_STRCALL_TYPE_SHIFT)
#define SIG_STRCALL_CALL_REG        (5 << SIG_STRCALL_TYPE_SHIFT)

#define SIG_STRCALL_GET_TYPE(param) ((param) & SIG_STRCALL_TYPE_MASK)
#define SIG_STRCALL_GET_REG(param) (ARM_REG_R0 + ((param) & SIG_STRCALL_ARG_MASK))

/* copied from finsig_dryos.c */
char    out_buf[32*1024] = "";
int     out_len = 0;
char    hdr_buf[32*1024] = "";
int     hdr_len = 0;
int     out_hdr = 1;

FILE *out_fp;

void bprintf(char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);

    if (out_hdr)
        hdr_len += vsprintf(hdr_buf+hdr_len,fmt,argp);
    else
        out_len += vsprintf(out_buf+out_len,fmt,argp);

    va_end(argp);
}

void add_blankline()
{
    if (strcmp(hdr_buf+hdr_len-2,"\n\n") != 0)
    {
        hdr_buf[hdr_len++] = '\n';
        hdr_buf[hdr_len] = 0;
    }
}

void write_output()
{
    add_blankline();
    if (out_fp)
    {
        fprintf(out_fp,"%s",hdr_buf);
        fprintf(out_fp,"%s",out_buf);
    }
}

// Master list of functions / addresses to find

#define DONT_EXPORT    0x01
#define OPTIONAL       0x02
#define UNUSED         0x04
#define BAD_MATCH      0x08
#define EV_MATCH       0x10
#define LIST_ALWAYS    0x20
// force an arm veneer (NHSTUB2)
#define ARM_STUB       0x80
#define DONT_EXPORT_ILC 0x100

typedef struct {
    char        *name;
    int         flags;
    uint32_t    val;
} sig_entry_t;

int next_sig_entry = 0;

#define MAX_SIG_ENTRY  5000

sig_entry_t  sig_names[MAX_SIG_ENTRY] =
{
    // Order here currently has no effect on search order, but mostly copied from finsig_dryos which did
    { "ExportToEventProcedure_FW", UNUSED|DONT_EXPORT },
    { "RegisterEventProcedure", UNUSED|DONT_EXPORT },
    { "RegisterEventProcedure_alt1", UNUSED|DONT_EXPORT },
    { "RegisterEventProcedure_alt2", UNUSED|DONT_EXPORT },
    { "RegisterEventProcTable", UNUSED|DONT_EXPORT },
    { "UnRegisterEventProcTable", UNUSED|DONT_EXPORT },
    { "UnRegisterEventProcedure", UNUSED|DONT_EXPORT },
    { "PrepareDirectory_1", UNUSED|DONT_EXPORT },
    { "PrepareDirectory_x", UNUSED|DONT_EXPORT },
    { "PrepareDirectory_0", UNUSED|DONT_EXPORT },
    { "CreateTaskStrictly", UNUSED|DONT_EXPORT },
    { "CreateTaskStrictly_alt", UNUSED|DONT_EXPORT },
    { "CreateTask_alt", UNUSED|DONT_EXPORT },
    { "CreateTask_low", UNUSED | OPTIONAL },
    { "CreateJumptable", UNUSED },
    { "_uartr_req", UNUSED },
    { "StartRecModeMenu", UNUSED },
    { "LogCameraEvent", UNUSED|DONT_EXPORT },
    { "getImageDirName", UNUSED|DONT_EXPORT },

    { "AllocateMemory", UNUSED|LIST_ALWAYS },
    { "AllocateUncacheableMemory" },
    { "Close" },
    { "CreateBinarySemaphore" },
    { "CreateCountingSemaphore", UNUSED|LIST_ALWAYS },
    { "CreateTask" },
    { "DebugAssert", OPTIONAL|LIST_ALWAYS },
    { "DeleteDirectory_Fut" },
    { "DeleteFile_Fut" },
    { "DeleteSemaphore", },
    { "DoAELock" },
    { "DoAFLock" },
    { "EnterToCompensationEVF" },
    { "ExecuteEventProcedure", ARM_STUB },
    { "ExitFromCompensationEVF" },
    { "ExitTask" },
    { "ExpCtrlTool_StartContiAE" },
    { "ExpCtrlTool_StopContiAE" },
    { "Fclose_Fut" },
    { "Feof_Fut" },
    { "Fflush_Fut" },
    { "Fgets_Fut" },
    { "Fopen_Fut" },
    { "Fread_Fut" },
    { "FreeMemory", UNUSED|LIST_ALWAYS },
    { "FreeUncacheableMemory" },
    { "Fseek_Fut" },
    { "Fwrite_Fut" },
    { "GetBatteryTemperature" },
    { "GetCCDTemperature" },

    { "GetCurrentAvValue" },
    { "GetCurrentShutterSpeed" },
    { "GetUsableMaxAv", OPTIONAL },
    { "GetUsableMinAv", OPTIONAL },
    { "GetUsableAvRange", UNUSED |OPTIONAL },
    { "get_nd_value", OPTIONAL },
    { "get_current_exp", UNUSED | OPTIONAL }, // helper, underlying function of ShowCurrentExp
    { "get_current_nd_value", OPTIONAL },
    { "get_current_deltasv", },
    { "GetCurrentDriveBaseSvValue", },
    { "GetDrive_ClusterSize" },
    { "GetDrive_FreeClusters", UNUSED }, // live_free_cluster_count variable is used instead
    { "GetDrive_TotalClusters" },
    { "GetFocusLensSubjectDistance" },
    { "GetFocusLensSubjectDistanceFromLens" },
    { "GetImageFolder", OPTIONAL },
    { "GetKbdState" },
    { "GetMemInfo" },
    { "GetOpticalTemperature" },
    { "GetParameterData" },
    { "GetPropertyCase" },
    { "GetSystemTime" },
    { "GetVRAMHPixelsSize" },
    { "GetVRAMVPixelsSize" },
    { "GetZoomLensCurrentPoint" },
    { "GetZoomLensCurrentPosition" },
    { "GiveSemaphore", OPTIONAL|LIST_ALWAYS },
    { "IsStrobeChargeCompleted" },
    { "LEDDrive", OPTIONAL },
    { "LocalTime" },
    { "LockMainPower" },
    { "Lseek", UNUSED|LIST_ALWAYS },
    { "MakeDirectory_Fut" },
    { "MakeSDCardBootable", OPTIONAL },
    { "MoveFocusLensToDistance" },
    { "MoveIrisWithAv", OPTIONAL },
    { "MoveZoomLensWithPoint", DONT_EXPORT_ILC},
    { "NewTaskShell", UNUSED },
    { "Open" },
    { "PB2Rec" },
    { "PT_MoveDigitalZoomToWide", OPTIONAL | DONT_EXPORT_ILC},
    { "PT_MoveOpticalZoomAt", OPTIONAL | DONT_EXPORT_ILC },
    { "MoveOpticalZoomAt", OPTIONAL | DONT_EXPORT_ILC },
    { "PT_PlaySound" },
    { "PostLogicalEventForNotPowerType" },
    { "PostLogicalEventToUI" },
    { "PutInNdFilter", OPTIONAL },
    { "PutOutNdFilter", OPTIONAL },
    { "Read" },
    { "ReadFastDir" },
    { "Rec2PB" },
    { "Remove", OPTIONAL|UNUSED },
    { "RenameFile_Fut" },
    { "Restart" },
    { "screenlock_helper", UNUSED|DONT_EXPORT },
    { "ScreenLock" },
    { "ScreenUnlock" },
    { "SetAE_ShutterSpeed" },
    { "SetAutoShutdownTime" },
    { "SetCurrentCaptureModeType" },
    { "SetDate" },
    { "SetFileAttributes" },
    { "SetFileTimeStamp" },
    { "SetLogicalEventActive" },
    { "SetParameterData" },
    { "SetPropertyCase" },
    { "SetScriptMode" },
    { "SleepTask" },
    { "TakeSemaphore" },
    { "TurnOffBackLight" },
    { "TurnOnBackLight" },
    { "TurnOnDisplay" },
    { "TurnOffDisplay" },
    { "UIFS_WriteFirmInfoToFile", OPTIONAL|UNUSED},
    { "UnlockAE" },
    { "UnlockAF" },
    { "UnlockMainPower" },
    { "UnsetZoomForMovie", OPTIONAL | DONT_EXPORT_ILC },
//    { "UpdateMBROnFlash" },
    { "VbattGet" },
    { "Write" },
    { "WriteSDCard" },
    { "ReadSDCard", OPTIONAL|UNUSED },
    { "InitSDCard", OPTIONAL|UNUSED },
    { "GetSDCardTotalSect", OPTIONAL|UNUSED },

    { "_log" },
    { "_log10" },
    { "_pow" },
    { "_sqrt" },
    { "add_ptp_handler" },
    { "apex2us" },
    { "close" },
    { "displaybusyonscreen", OPTIONAL },
    { "err_init_task", OPTIONAL },
    { "exmem_alloc", OPTIONAL },
    { "exmem_free", OPTIONAL|UNUSED },
    { "exmem_ualloc" },
    { "exmem_ufree" },
    { "free" },

    { "kbd_p1_f" },
    { "kbd_p1_f_cont" },
    { "kbd_p2_f" },
    { "kbd_read_keys" },
    { "kbd_read_keys_r2" },

    { "kbd_pwr_off", OPTIONAL },
    { "kbd_pwr_on", OPTIONAL },
    { "lseek" },
    { "malloc" },
    { "memcmp" },
    { "memcpy" },
    { "memset" },
// identical to MakeDirectory_Fut for recent cams
//    { "mkdir" },
    { "mktime_ext" },
    { "open" },
    { "OpenFastDir" },
    { "closedir" },
    { "get_fstype", OPTIONAL|LIST_ALWAYS },
    { "qsort" },
    { "rand" },
    { "read", UNUSED|OPTIONAL },
    { "realloc", OPTIONAL|LIST_ALWAYS },
    { "reboot_fw_update" },
    { "set_control_event" },
    { "srand" },
    { "stat" },
    { "strcat" },
    { "strchr" },
    { "strcmp" },
    { "strcpy" },
    { "strftime" },
    { "strlen" },
    { "strncmp" },
    { "strncpy" },
    { "strrchr" },
    { "strtol" },
    { "strtolx" },
    { "strstr", UNUSED|OPTIONAL},

    { "task_CaptSeq" },
    { "task_DvlpSeqTask", OPTIONAL },
    { "task_ExpDrv" },
    { "task_FileWrite", OPTIONAL },
    { "task_InitFileModules" },
    { "task_MovieRecord" },
    { "task_PhySw", OPTIONAL },
    { "task_RotaryEncoder", OPTIONAL },
    { "task_TouchPanel", OPTIONAL },
    { "task_TricInitTask", OPTIONAL },

    { "hook_CreateTask" },
    { "hook_CreateTask_low", UNUSED}, // unused changed at runtime if CreateTask in ROM

    { "time" },
    { "vsprintf" },
    { "write", UNUSED|OPTIONAL },
    { "undisplaybusyonscreen", OPTIONAL },

    { "EngDrvIn", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "EngDrvOut", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "EngDrvRead" },
    { "EngDrvBits", OPTIONAL|UNUSED|LIST_ALWAYS },

    { "PTM_GetCurrentItem" },
    { "PTM_SetCurrentItem", UNUSED|LIST_ALWAYS },
    { "PTM_NextItem", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "PTM_PrevItem", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "PTM_SetPropertyEnable", OPTIONAL|UNUSED|LIST_ALWAYS },

    { "DisableISDriveError", OPTIONAL },

    // OS functions, mostly to aid firmware analysis. Order is important!
    { "_GetSystemTime", OPTIONAL|UNUSED|LIST_ALWAYS }, // only for locating timer functions
    { "SetTimerAfter", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "SetTimerWhen", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CancelTimer", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CancelHPTimer" },
    { "SetHPTimerAfterTimeout", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "SetHPTimerAfterNow" },
    { "CreateTaskStrictly", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateMessageQueue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateRecursiveLock", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "GetSemaphoreValue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "TryTakeSemaphore", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateMessageQueueStrictly", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateEventFlagStrictly", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateBinarySemaphoreStrictly", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateCountingSemaphoreStrictly", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateRecursiveLockStrictly", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "TakeSemaphoreStrictly", OPTIONAL|UNUSED|LIST_ALWAYS }, // r23+
    { "ReceiveMessageQueueStrictly", OPTIONAL|UNUSED|LIST_ALWAYS }, // r23+
    { "PostMessageQueueStrictly", OPTIONAL|UNUSED|LIST_ALWAYS },    // r23+
    { "WaitForAnyEventFlagStrictly", OPTIONAL|UNUSED|LIST_ALWAYS }, // r23+
    { "WaitForAllEventFlagStrictly", OPTIONAL|UNUSED|LIST_ALWAYS }, // r23+
    { "AcquireRecursiveLockStrictly", OPTIONAL|UNUSED|LIST_ALWAYS }, // r23+
    { "DeleteMessageQueue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "PostMessageQueue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "ReceiveMessageQueue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "TryReceiveMessageQueue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "TryPostMessageQueue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "GetNumberOfPostedMessages", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "DeleteRecursiveLock", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "AcquireRecursiveLock", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "ReleaseRecursiveLock", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "WaitForAnyEventFlag", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "WaitForAllEventFlag", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "ClearEventFlag", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "SetEventFlag", OPTIONAL|LIST_ALWAYS },
    { "GetEventFlagValue", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CreateEventFlag", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "DeleteEventFlag", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CheckAnyEventFlag", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "CheckAllEventFlag", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "RegisterInterruptHandler", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "UnregisterInterruptHandler", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "GetSRAndDisableInterrupt", OPTIONAL|UNUSED|LIST_ALWAYS }, // disables IRQ, returns a value
    { "SetSR", OPTIONAL|UNUSED|LIST_ALWAYS }, // enables IRQ, puts back value returned by GetSR
    { "EnableInterrupt", OPTIONAL|UNUSED|LIST_ALWAYS }, // enables IRQ
    { "_divmod_signed_int", OPTIONAL|UNUSED|LIST_ALWAYS}, // division for signed integers, remainder is returned in r1
    { "_divmod_unsigned_int", OPTIONAL|UNUSED|LIST_ALWAYS}, // division for unsigned integers, remainder is returned in r1
    { "_dflt", OPTIONAL|UNUSED|LIST_ALWAYS}, // int -> double
    { "_dfltu", OPTIONAL|UNUSED|LIST_ALWAYS}, // uint -> double
    { "_dfix", OPTIONAL|UNUSED|LIST_ALWAYS}, // double -> int
    { "_dfixu", OPTIONAL|UNUSED|LIST_ALWAYS}, // double -> uint
    { "_dmul", OPTIONAL|UNUSED|LIST_ALWAYS}, // double precision float multiplication
    { "_ddiv", OPTIONAL|UNUSED|LIST_ALWAYS}, // double precision float division
    { "_dadd", OPTIONAL|UNUSED|LIST_ALWAYS}, // addition for doubles
    { "_dsub", OPTIONAL|UNUSED|LIST_ALWAYS}, // subtraction for doubles
    { "_drsb", OPTIONAL|UNUSED|LIST_ALWAYS}, // reverse subtraction for doubles (?)
    { "_dcmp", OPTIONAL|UNUSED|LIST_ALWAYS}, // comparison of 2 doubles, only updates condition flags
    { "_dcmp_reverse", OPTIONAL|UNUSED|LIST_ALWAYS}, // like _dcmp, but operands in reverse order, only updates condition flags
    { "_safe_sqrt", OPTIONAL|UNUSED|LIST_ALWAYS}, // only calls _sqrt for numbers >= 0
    { "_scalbn", OPTIONAL|UNUSED|LIST_ALWAYS}, // double scalbn (double x, long exp), returns x * FLT_RADIX ** exp
    { "_fflt", OPTIONAL|UNUSED|LIST_ALWAYS}, // int -> float
    { "_ffltu", OPTIONAL|UNUSED|LIST_ALWAYS}, // uint -> float
    { "_ffix", OPTIONAL|UNUSED|LIST_ALWAYS}, // float -> int
    { "_ffixu", OPTIONAL|UNUSED|LIST_ALWAYS}, // float -> uint
    { "_fmul", OPTIONAL|UNUSED|LIST_ALWAYS}, // single precision float multiplication
    { "_fdiv", OPTIONAL|UNUSED|LIST_ALWAYS}, // single precision float division
    { "_f2d", OPTIONAL|UNUSED|LIST_ALWAYS}, // float -> double
    { "DisplayBusyOnScreen", OPTIONAL|UNUSED|LIST_ALWAYS}, // displays full screen "busy" message
    { "UndisplayBusyOnScreen", OPTIONAL|UNUSED|LIST_ALWAYS},
    { "CreateDialogBox", OPTIONAL|UNUSED|LIST_ALWAYS},
    { "DisplayDialogBox", OPTIONAL|UNUSED|LIST_ALWAYS},
    { "add_ui_to_dialog", OPTIONAL|UNUSED|LIST_ALWAYS}, // name made up, assigns resources to a dialog
    { "get_string_by_id", OPTIONAL|UNUSED|LIST_ALWAYS}, // name made up, retrieves a localised or unlocalised string by its ID
    { "malloc_strictly", OPTIONAL|UNUSED|LIST_ALWAYS }, // name made up
    { "GetCurrentMachineTime", OPTIONAL|UNUSED|LIST_ALWAYS }, // reads usec counter, name from ixus30
    { "HwOcReadICAPCounter", OPTIONAL|UNUSED|LIST_ALWAYS }, // reads usec counter, name from ixus30
    { "transfer_src_overlay_helper",UNUSED}, // helper for other related functions
    { "transfer_src_overlay" },
    { "GraphicSystemCoreFinish_helper", OPTIONAL|UNUSED }, // function that calls GraphicSystemCoreFinish
    { "GraphicSystemCoreFinish", OPTIONAL|UNUSED }, // used to identify mzrm message functions
    { "mzrm_createmsg", OPTIONAL|UNUSED },
    { "mzrm_sendmsg", OPTIONAL|UNUSED },
    { "zicokick_start", OPTIONAL|UNUSED }, // used to identify Zico core Xtensa blobs
    { "zicokick_copy", OPTIONAL|UNUSED }, // used to identify Zico core Xtensa blobs
    { "init_ex_drivers", OPTIONAL|UNUSED }, // used to identify Omar core ARM blobs
    { "omar_init", OPTIONAL|UNUSED }, // used to identify Omar core ARM blobs
    { "init_error_handlers", OPTIONAL|UNUSED }, // used to identify assert, exception and panic handlers
    { "set_assert_handler", OPTIONAL|UNUSED },
    { "set_exception_handler", OPTIONAL|UNUSED },
    { "set_panic_handler", OPTIONAL|UNUSED },
    { "default_assert_handler", OPTIONAL|UNUSED },
    { "default_exception_handler", OPTIONAL|UNUSED },
    { "default_panic_handler", OPTIONAL|UNUSED },
    { "get_self_task_errno_pointer", OPTIONAL|UNUSED },
    { "get_self_task_id", OPTIONAL|UNUSED },
    { "get_task_properties", OPTIONAL|UNUSED },
    { "dry_error_printf", OPTIONAL|UNUSED },
    { "heap_alloc", OPTIONAL|UNUSED },
    { "heap_free", OPTIONAL|UNUSED },
    { "umalloc_strictly", OPTIONAL|UNUSED },
    { "GetRomID", OPTIONAL|UNUSED },
    { "init_task_error", OPTIONAL|UNUSED },
    { "dry_panic", OPTIONAL|UNUSED },
    { "dry_panic_low", OPTIONAL|UNUSED },
    { "dry_con_printf", OPTIONAL|UNUSED },
    { "cameracon_set_state", UNUSED },
    { "cameracon_get_state", UNUSED },

    { "createsemaphore_low", OPTIONAL|UNUSED },
//    { "deletesemaphore_low", UNUSED },
    { "givesemaphore_low", OPTIONAL|UNUSED}, // OPT_CONSOLE_REDIR_ENABLED
    { "takesemaphore_low", OPTIONAL|UNUSED },
    { "bzero" }, //
    { "memset32" }, // actually jump to 2nd instruction of bzero
    { "dry_memset", OPTIONAL|UNUSED },
    { "dry_memzero", OPTIONAL|UNUSED },
    { "dry_memcpy_bytes", OPTIONAL|UNUSED },
    { "dry_memmove_bytes", OPTIONAL|UNUSED },
    { "get_dial_hw_position", OPTIONAL },

//    { "icache_flush_and_enable", OPTIONAL|UNUSED },
//    { "icache_disable_and_flush", OPTIONAL|UNUSED },
    { "dcache_flush_and_enable", OPTIONAL|UNUSED },
    { "dcache_clean_flush_and_disable", OPTIONAL|UNUSED },
    { "dcache_flush_range", OPTIONAL|UNUSED },
    { "dcache_clean_range", OPTIONAL|UNUSED },
//    { "dcache_clean_flush_range", OPTIONAL|UNUSED },
    { "icache_flush_range", OPTIONAL|UNUSED },
    { "data_synchronization_barrier", OPTIONAL|UNUSED },

    // Other stuff needed for finding misc variables - don't export to stubs_entry.S
    { "GetSDProtect", UNUSED },
    { "DispCon_ShowBitmapColorBar", UNUSED },
    { "ResetZoomLens", OPTIONAL|UNUSED },
    { "ResetFocusLens", OPTIONAL|UNUSED },
    { "NR_GetDarkSubType", OPTIONAL|UNUSED },
    { "NR_SetDarkSubType", OPTIONAL|UNUSED },
    { "SavePaletteData", OPTIONAL|UNUSED },
    { "GUISrv_StartGUISystem", OPTIONAL|UNUSED|LIST_ALWAYS },
    { "get_resource_pointer", OPTIONAL|UNUSED|LIST_ALWAYS }, // name made up, gets a pointer to a certain resource (font, dialog, icon)
    { "CalcLog10", OPTIONAL|UNUSED|LIST_ALWAYS }, // helper
    { "CalcSqrt", OPTIONAL|UNUSED }, // helper
    { "dry_memcpy", OPTIONAL|UNUSED }, // helper, memcpy-like function in dryos kernel code
    { "get_playrec_mode", OPTIONAL|UNUSED }, // helper, made up name
    { "DebugAssert2", OPTIONAL|UNUSED }, // helper, made up name, two arg form of DebugAssert
    { "get_canon_mode_list", OPTIONAL|UNUSED }, // helper, made up name
    { "taskcreate_LowConsole", OPTIONAL|UNUSED }, // helper, made up name
    { "ImagerActivate", OPTIONAL|UNUSED }, // helper
    { "imager_active_callback", OPTIONAL|UNUSED }, // helper
    { "file_counter_var_init", OPTIONAL|UNUSED }, // helper
    { "get_displaytype", OPTIONAL|UNUSED }, // for camera bitmap res change
    // dry >= 58 wrap "Open" and "Close" in additional functions
    { "Close_low", OPTIONAL|UNUSED },
    { "Open_low", OPTIONAL|UNUSED },

    // underlying functions of F*_Fut functions, for analysis
    { "fclose_low", OPTIONAL|UNUSED },
//    { "feof_low", OPTIONAL|UNUSED },
//    { "fflush_low", OPTIONAL|UNUSED },
    { "fgets_low", OPTIONAL|UNUSED },
    { "fopen_low", OPTIONAL|UNUSED },
    { "fut_prepare", OPTIONAL|UNUSED },
    { "fut_finish", OPTIONAL|UNUSED },
    { "fut_flush", OPTIONAL|UNUSED },
    { "fread_low", OPTIONAL|UNUSED },
    { "fseek_low", OPTIONAL|UNUSED },
    { "fwrite_low", OPTIONAL|UNUSED },

    { "MFOn", OPTIONAL },
    { "MFOff", OPTIONAL },
    { "PT_MFOn", OPTIONAL },
    { "PT_MFOff", OPTIONAL },
    { "SS_MFOn", OPTIONAL },
    { "SS_MFOff", OPTIONAL },

    { "GetAdChValue", OPTIONAL },

    // for HDMI remote support - optional, probably only present on cameras with HDMI
    { "EnableHDMIPower", OPTIONAL },
    { "DisableHDMIPower", OPTIONAL },

    { "SetVideoOutType", OPTIONAL },
    { "GetVideoOutType", OPTIONAL },

    { "is_movie_recording", OPTIONAL|UNUSED },
    { "IsWirelessConnect", OPTIONAL },

    { "ui_malloc", OPTIONAL|UNUSED },
    { "ui_malloc_default", OPTIONAL|UNUSED },
    { "ui_free", OPTIONAL|UNUSED },
    { "ui_free_default", OPTIONAL|UNUSED },

    // made up names, see https://chdk.setepontos.com/index.php?topic=11246.0
    { "pvm_get_largest_free_block_size_ptr", OPTIONAL|UNUSED }, // wrapper for following that puts return ptr arg
    { "pvm_get_largest_free_block_size", OPTIONAL|UNUSED },
    { "pvm_malloc", OPTIONAL|UNUSED },
    { "pvm_free", OPTIONAL|UNUSED },
    { "pvm_init_pool", OPTIONAL|UNUSED },

    { "fw_yuv_layer_buf_helper", UNUSED},

    { "get_ef_lens_name", OPTIONAL|UNUSED },
    { "get_ef_lens_wide_fl", OPTIONAL|UNUSED },
    { "get_ef_lens_tele_fl", OPTIONAL|UNUSED },
    { "GetLensIDValue", OPTIONAL|UNUSED },
    { "GetLensExtenderTypeValue", OPTIONAL|UNUSED },
    { "GetLensTypeValue", OPTIONAL|UNUSED },
    { "GetFALensInfoData", OPTIONAL|UNUSED },
    { "GetFALensInfoData_vals", OPTIONAL|UNUSED },
    { "AllocateEFlensComBuffer", OPTIONAL|UNUSED },
    { "ReleaseEFlensComBuffer", OPTIONAL|UNUSED },
    { "EFLens_Send", OPTIONAL|UNUSED },
    { "EFLens_Send_low", OPTIONAL|UNUSED },
    { "EFLensCom_MoveFocus", OPTIONAL|UNUSED },
    { "EFLensCom_FocusSearchNear", OPTIONAL|UNUSED },
    { "EFLensCom_FocusSearchFar", OPTIONAL|UNUSED },
    { "GetEFLensFocusPositionWithLensCom", OPTIONAL|UNUSED },

    { "init_sd_io_funcs", OPTIONAL|UNUSED }, // helper for WriteSDCard
    { "SD_ChgClkSpd", OPTIONAL|UNUSED }, // SD clock control
    { "sddomChangeClockSpeed", OPTIONAL|UNUSED }, // lower level SD clock control
    { "SD_GetTtlSect", OPTIONAL|UNUSED }, // Helper, total sectors from SD card, probably
    { "SD_take_sem", OPTIONAL|UNUSED }, // function that takes sem associated with low level SD control
    { "SD_give_sem", OPTIONAL|UNUSED }, // function that gives sem associated with low level SD control
    { "SD_debug_log", OPTIONAL|UNUSED },// debug logging function often used by SD code
    { "SD_error_log", OPTIONAL|UNUSED },// debug logging function often used by SD code for error conditions
    { "SD_GetSpd", OPTIONAL|UNUSED },// gets SD speed id enum and another value
    { "SD_get_speed_id", OPTIONAL|UNUSED },// gets SD speed id
    { "SetSDClkFrequency", OPTIONAL|UNUSED },// sets MMIOs based on speed id
    { "SD_CMD55_SendAppCommand", OPTIONAL|UNUSED },// sends CMD55
    { "SD_cmd_setup", OPTIONAL|UNUSED },// set command and parameter MMIO
    { "SD_cmd_setup_resp48b", OPTIONAL|UNUSED },// set response size to 48 bits and some unknown flags 0x2701
    { "SD_cmd_send", OPTIONAL|UNUSED },// probably triggers actual sending of SD command
    { "sdconWaitInterrupt", OPTIONAL|UNUSED },

    {0,0,0},
};

typedef struct {
    char*   name;   // name
    int     id;     // propcase id, as found
    int     use;    // 0: informational only; 1: use for propset guess AND print as #define; 2: use for propset guess

    int     id_ps6; // id in propset 6
    int     id_ps7; // id in propset 7
    int     id_ps8; // id in propset 8
    int     id_ps9; // id in propset 9
    int     id_ps10;// id in propset 10
    int     id_ps11;// id in propset 11
    int     id_ps12;// id in propset 12
    int     id_ps13;// id in propset 13
} known_prop_t;

#define KNOWN_PROPSET_COUNT (13-5)

known_prop_t knownprops[] =
{   // name                          id  u ps6 ps7 ps8 ps9 ps10 ps11 ps12 ps13
    {"PROPCASE_AFSTEP"             , -1, 2, 13, 13, 13, 13,  13,  13,  13,  13},
    {"PROPCASE_FOCUS_STATE"        , -1, 1, 18, 18, 18, 18,  18,  18,  18,  18},
    {"PROPCASE_AV"                 , -1, 1, 23, 23, 23, 23,  23,  23,  23,  23},
    {"PROPCASE_BV"                 , -1, 1, 34, 38, 35, 38,  40,  40,  40,  40},
    {"PROPCASE_DELTA_DIGITALGAIN"  , -1, 2, 77, 82, 79, 82,  84,  85,  85,  84},
    {"PROPCASE_DELTA_SV"           , -1, 1, 79, 84, 81, 84,  86,  87,  87,  86},
    {"PROPCASE_DELTA_ND"           , -1, 2, 80, 85, 82, 85,  87,  88,  88,  87},
    {"PROPCASE_FELOCK"             , -1, 2,114,120,117,120, 122, 123, 123, 122},
    {"PROPCASE_FLASH_ADJUST_MODE"  , -1, 1,121,127,124,127, 129, 130, 130, 129},
    {"PROPCASE_FLASH_FIRE"         , -1, 1,122,128,125,128, 130, 131, 131, 130},
    {"PROPCASE_HSCAPTURE"          , -1, 2,138,144,141,144, 146, 147, 147, 146},
    {"PROPCASE_EV_CORRECTION_2"    , -1, 1,210,216,213,216, 218, 219, 220, 218},
    {"PROPCASE_ORIENTATION_SENSOR" , -1, 1,222,228,225,228, 230, 231, 232, 230},
    {"PROPCASE_SV_MARKET"          , -1, 1,249,255,252,255, 257, 259, 260, 258},
    {"PROPCASE_SVFIX"              , -1, 0,  0,  0,  0,  0,   0,   0,   0, 259},
    {"PROPCASE_TV"                 , -1, 1,265,272,269,272, 274, 276, 277, 275},
    {0,}
};

void add_prop_hit(char *name, int id)
{
    int n = 0;
    while (knownprops[n].name) {
        if (strcmp(knownprops[n].name,name) == 0) {
            knownprops[n].id = id;
            break;
        }
        n++;
    }
}

#define MISC_BLOB_XTENSA_MAX    5
#define MISC_BLOB_TYPE_NONE     0
#define MISC_BLOB_TYPE_XTENSA   1
#define MISC_BLOB_TYPE_OMAR     2
typedef struct {
    int         type;
    uint32_t    rom_adr; // location of data in ROM, if copied
    uint32_t    ram_adr; // location of data in RAM
    uint32_t    size;
} misc_blob_t;

// for values that don't get a DEF etc
#define MISC_VAL_NO_STUB    1
// DEF_CONST instead of DEF
#define MISC_VAL_DEF_CONST  2
#define MISC_VAL_OPTIONAL   4
// variables and constants
typedef struct {
    char        *name;
    int         flags;
    uint32_t    val;
    // informational values
    uint32_t    base; // if stub is found as ptr + offset, record
    uint32_t    offset;
    uint32_t    ref_adr; // code address near where value found (TODO may want list)
    misc_blob_t *blobs; // malloc'd array of blobs if this is a blob type value, terminated with flags = 0
} misc_val_t;

misc_val_t misc_vals[]={
    // stubs_min variables / constants
    { "ctypes",             },
    { "physw_run",          },
    { "physw_sleep_delay",  },
    { "physw_status",       },
    { "fileio_semaphore",   },
    { "levent_table",       },
    { "FlashParamsTable",   },
    { "playrec_mode",       },
    { "jpeg_count_str",     },
    { "zoom_busy",          },
    { "focus_busy",         },
    { "imager_active",      },
    { "canon_menu_active",  },
    { "file_counter_var",   },
    { "cameracon_state",    },
    { "_nrflag",            MISC_VAL_OPTIONAL},
    { "av_override_semaphore",MISC_VAL_OPTIONAL},
    { "active_bitmap_buffer",MISC_VAL_OPTIONAL},
    { "displaytype",        MISC_VAL_OPTIONAL},
    { "bitmap_buffer",      MISC_VAL_OPTIONAL},
    { "palette_control",    MISC_VAL_OPTIONAL},
    { "palette_buffer_ptr", MISC_VAL_OPTIONAL},
    { "active_palette_buffer",MISC_VAL_OPTIONAL},
    { "live_free_cluster_count",MISC_VAL_OPTIONAL},
    { "CAM_UNCACHED_BIT",   MISC_VAL_NO_STUB},
    { "physw_event_table",  MISC_VAL_NO_STUB},
    { "uiprop_count",       MISC_VAL_DEF_CONST},
    { "canon_mode_list",    MISC_VAL_NO_STUB},
    { "ARAM_HEAP_START",    MISC_VAL_NO_STUB},
    { "ARAM_HEAP_SIZE",     MISC_VAL_NO_STUB},
    { "zicokick_values",    MISC_VAL_NO_STUB}, // used to identify Zico core Xtensa blobs (dummy for now)
    { "omar_init_data",     MISC_VAL_NO_STUB}, // structure containing pointers to Omar blogs
    { "omar_init_values",   MISC_VAL_NO_STUB}, // Omar blobs
    { "CAM_HAS_ND_FILTER",  MISC_VAL_NO_STUB},
    { "CAM_IS_ILC",         MISC_VAL_NO_STUB}, // used for finsig code that wants to check for interchangeable lens
    { "CAM_HAS_IRIS_DIAPHRAGM",MISC_VAL_NO_STUB},
    { "CAM_HAS_WIFI",       MISC_VAL_NO_STUB},
    { "exmem_alloc_table",  },
    { "exmem_types_table",  },
    { "exmem_type_count",   MISC_VAL_DEF_CONST},
    { "ui_malloc_ptr",      MISC_VAL_NO_STUB}, // currently only used to find functions
    { "ui_free_ptr",        MISC_VAL_NO_STUB},
    { "debug_logging_ptr", }, // debug log function for mzrm, tric calls
    { "debug_logging_flag", },
    { "mzrm_sendmsg_ret_adr", }, // return address for hooking log call
    { "fw_yuv_layer_buf",   MISC_VAL_DEF_CONST},

    {0,0,0},
};

misc_val_t *get_misc_val(const char *name)
{
    misc_val_t *p=misc_vals;
    while(p->name) {
        if(strcmp(name,p->name) == 0) {
            return p;
        }
        p++;
    }
    return NULL;
}

// get value of misc val, if set. Name :<
uint32_t get_misc_val_value(const char *name)
{
    misc_val_t *p=get_misc_val(name);
    if(!p) {
        printf("get_misc_val_value: invalid name %s\n",name);
        return 0;
    }
    return p->val;
}
void save_misc_val(const char *name, uint32_t base, uint32_t offset, uint32_t ref_adr)
{
    misc_val_t *p=get_misc_val(name);
    if(!p) {
        printf("save_misc_val: invalid name %s\n",name);
        return;
    }
    p->val = base + offset;
    p->base = base;
    p->offset = offset;
    p->ref_adr = ref_adr;
    p->blobs = NULL;
}
void save_misc_val_blobs(const char *name, misc_blob_t *blobs, uint32_t ref_adr)
{
    misc_val_t *p=get_misc_val(name);
    if(!p) {
        printf("save_misc_val: invalid name %s\n",name);
        return;
    }
    p->val = p->base = p->offset = 0;
    p->ref_adr = ref_adr;
    p->blobs = blobs;
}

// Return the array index of a named function in the array above
#if 0
int find_saved_sig_index(const char *name)
{
    int i;
    for (i=0; sig_names[i].name != 0; i++)
    {
        if (strcmp(name,sig_names[i].name) == 0)
        {
            return i;
        }
    }
    return -1;
}
#endif

sig_entry_t * find_saved_sig(const char *name)
{
    int i;
    for (i=0; sig_names[i].name != 0; i++)
    {
        if (strcmp(name,sig_names[i].name) == 0)
        {
            return &sig_names[i];
        }
    }
    return NULL;
}

// return value of saved sig, or 0 if not found / doesn't exist
uint32_t get_saved_sig_val(const char *name)
{
    sig_entry_t *sig=find_saved_sig(name);
    if(!sig) {
        // printf("get_saved_sig_val: missing %s\n",name);
        return 0;
    }
    return sig->val;
}

// unused for now
// Return the array index of of function with given address
#if 0
int find_saved_sig_index_by_adr(uint32_t adr)
{
    if(!adr) {
        return  -1;
    }
    int i;
    for (i=0; sig_names[i].name != 0; i++)
    {
        if (sig_names[i].val == adr)
        {
            return i;
        }
    }
    return -1;
}
#endif
#if 0
sig_entry_t* find_saved_sig_by_val(uint32_t val)
{
    if(!val) {
        return NULL;
    }
    int i;
    for (i=0; sig_names[i].name != 0; i++)
    {
        if (sig_names[i].val == val)
        {
            return &sig_names[i];
        }
    }
    return NULL;
}
#endif

// Save the address value found for a function in the above array
void save_sig(firmware *fw, const char *name, uint32_t val)
{
    sig_entry_t *sig = find_saved_sig(name);
    if (!sig)
    {
        printf("save_sig: refusing to save unknown name %s\n",name);
        return;
    }
    // if we end up needed these, can add a flag
    if(!adr_is_main_fw_code(fw,val)) {
        printf("save_sig: refusing to save %s with out of range address 0x%08x\n",name,val);
        return;
    }
    if(sig->val && sig->val != val) {
        printf("save_sig: duplicate name %s existing 0x%08x != new 0x%08x\n",name,sig->val,val);
    }
    sig->val = val;
}

void add_func_name(firmware *fw, char *n, uint32_t eadr, char *suffix)
{
    int k;

    char *s = n;
    int mallocd = 0;
    if (suffix != 0)
    {
        s = malloc(strlen(n) + strlen(suffix) + 1);
        sprintf(s, "%s%s", n, suffix);
        mallocd = 1;
    }

    // if we end up needed these, can add a flag
    if(!adr_is_main_fw_code(fw,eadr)) {
        printf("save_sig: refusing to save %s with out of range address 0x%08x\n",s,eadr);
        if(mallocd) {
            free(s);
        }
        return;
    }

    for (k=0; sig_names[k].name != 0; k++)
    {
        if (strcmp(sig_names[k].name, s) == 0)
        {
            if (sig_names[k].val == 0)             // same name, no address
            {
                sig_names[k].val = eadr;
                sig_names[k].flags |= EV_MATCH;
                if (mallocd)
                    free(s);
                return;
            }
            else if (sig_names[k].val == eadr)     // same name, same address
            {
                if (mallocd)
                    free(s);
                return;
            }
            else // same name, different address
            {
                printf("add_func_name: duplicate name %s existing 0x%08x != new 0x%08x\n",s, sig_names[k].val, eadr);
            }
        }
    }

    sig_names[next_sig_entry].name = s;
    sig_names[next_sig_entry].flags = OPTIONAL|UNUSED;
    sig_names[next_sig_entry].val = eadr;
    next_sig_entry++;
    sig_names[next_sig_entry].name = 0;
}

// save up to 10 veneers starting at adr, return final non-veneer address, or 0 on failure
uint32_t save_sig_veneers(firmware *fw, const char *name, uint32_t adr)
{
    // attempt to disassemble target
    if(!fw_disasm_iter_single(fw,adr)) {
        printf("save_sig_veneers: %s disassembly failed at 0x%08x\n",name,adr);
        return 0;
    }
    // handle functions that immediately jump
    // doesn't check for conditionals, but first insn shouldn't be conditional
    uint32_t b_adr;
    int v_cnt;
    for(v_cnt = 0, b_adr = get_direct_jump_target(fw,fw->is);
            v_cnt < 10 && b_adr;
            v_cnt++,b_adr = get_direct_jump_target(fw,fw->is)) {
        char *buf=malloc(strlen(name)+7);
        if(v_cnt) {
            sprintf(buf,"j%d_%s",v_cnt,name);
        } else { // first level is just j_ for backward compatiblity
            // TODO could check for existing veeners, allowing for different paths
            sprintf(buf,"j_%s",name);
        }
        add_func_name(fw,buf,adr,NULL); // this is the orignal named address
        adr=b_adr; // thumb bit already handled by get_direct...
        if(!fw_disasm_iter_single(fw,adr)) {
            printf("save_sig_veneers: %s disassembly failed at 0x%08x\n",name,adr);
            return 0;
        }
    }
    return adr;
}

// save sig, with veneers added as j_... , j1_... etc
int save_sig_with_j(firmware *fw, char *name, uint32_t adr)
{
    if(!adr) {
        printf("save_sig_with_j: %s null adr\n",name);
        return 0;
    }
    adr = save_sig_veneers(fw, name, adr);
    if(adr) {
        save_sig(fw,name,adr);
        return 1;
    }
    return 0;
}

#define FIND_SIG_CALL_NO_UNK_VENEER 1
// find next call to func named "name" or j_name, up to max_offset form the current is address
// TODO max_offset is in bytes, unlike insn search functions that use insn counts
int find_next_sig_call_ex(firmware *fw, iter_state_t *is, uint32_t max_offset, const char *name, uint32_t flags)
{
    uint32_t adr=get_saved_sig_val(name);

    if(!adr) {
        printf("find_next_sig_call: missing %s\n",name);
        return 0;
    }

    search_calls_multi_data_t match_fns[12];

    match_fns[0].adr=adr;
    match_fns[0].fn=search_calls_multi_end;
    char veneer[128];
    int i;
    for(i = 1; i<11; i++) {
        // match convention in save_sig_veneers
        if(i>1) {
            sprintf(veneer,"j%d_%s",i-1,name);
        } else {
            sprintf(veneer,"j_%s",name);
        }
        adr=get_saved_sig_val(veneer);
        if(!adr) {
            break;
        } else {
            match_fns[i].adr=adr;
            match_fns[i].fn=search_calls_multi_end;
        }
    }
    search_insn_fn search_fn;
    if(flags & FIND_SIG_CALL_NO_UNK_VENEER) {
        search_fn = search_disasm_calls_multi;
    } else {
        search_fn = search_disasm_calls_veneer_multi;
    }
    match_fns[i].adr=0;
    return fw_search_insn(fw,is,search_fn,0,match_fns,is->adr + max_offset);
}
// as above, but default flags (resolving unknown veneers)
int find_next_sig_call(firmware *fw, iter_state_t *is, uint32_t max_offset, const char *name)
{
    return find_next_sig_call_ex(fw,is,max_offset,name,0);
}

// is the insn pointed to by is a call to "name" or one of it's veneers?
// note: inefficient, should not be used for large searches
int is_sig_call(firmware *fw, iter_state_t *is, const char *name)
{
    uint32_t adr=get_branch_call_insn_target(fw,is);
    // not a call at all
    // TODO could check if unknown veneer
    if(!adr) {
        return 0;
    }
    uint32_t sig_adr=get_saved_sig_val(name);
    osig* ostub2 = find_sig(fw->sv->stubs,name);
    if (ostub2 && ostub2->val)
        sig_adr = ostub2->val;
    if(!sig_adr) {
        printf("is_sig_call: missing %s\n",name);
        return 0;
    }
    if(adr == sig_adr) {
        return 1;
    }
    char veneer[128];
    sprintf(veneer,"j_%s",name);
    sig_adr=get_saved_sig_val(veneer);
    if(!sig_adr) {
        return 0;
    }
    return (adr == sig_adr);
}

// macros for dry_min, dry_max fields
#define SIG_DRY_MIN(min_rel) (min_rel)*FW_DRYOS_VER_MUL,0
#define SIG_DRY_MAX(max_rel) 0,(max_rel)*FW_DRYOS_VER_MUL+(FW_DRYOS_VER_MUL-1)
#define SIG_DRY_RANGE(min_rel,max_rel) (min_rel)*FW_DRYOS_VER_MUL,\
                                       (max_rel)*FW_DRYOS_VER_MUL+(FW_DRYOS_VER_MUL-1)
// patch level specifc
#define SIG_DRY_MINP(min_rel,min_patch) (min_rel)*FW_DRYOS_VER_MUL + (min_patch),0
#define SIG_DRY_MAXP(max_rel,max_patch) 0,(max_rel)*FW_DRYOS_VER_MUL + (max_patch)
#define SIG_DRY_RANGEP(min_rel,min_patch,max_rel,max_patch) \
                                        (min_rel)*FW_DRYOS_VER_MUL + (min_patch), \
                                        (max_rel)*FW_DRYOS_VER_MUL + (max_patch)

#define SIG_DRY_ANY 0,0

// defines for flags field
#define SIG_NO_D6 1
#define SIG_NO_D7 2
#define SIG_ILC_ONLY 4
#define SIG_NONILC_ONLY 8
#define SIG_OPTIONAL 16


typedef struct sig_rule_s sig_rule_t;
typedef int (*sig_match_fn)(firmware *fw, iter_state_t *is, sig_rule_t *rule);
// signature matching structure
struct sig_rule_s {
    sig_match_fn    match_fn;       // function to locate function
    char        *name;              // function name used in CHDK
    char        *ref_name;          // event / other name to match in the firmware
    int         param;              // function specific param/offset
    int         dryos_min;          // minimum fw->dryos_ver_full version (0 = any)
    int         dryos_max;          // max fw->dryos_ver_full version to apply this sig to (0 = any)
    unsigned    flags;              // non rule specific match flags
};

// initialize iter state using address from ref_name, print error and return 0 if not found
int init_disasm_sig_ref(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!rule->ref_name) {
        printf("init_disasm_sig_ref: %s missing ref_name\n",rule->name);
        return 0;
    }
    uint32_t adr=get_saved_sig_val(rule->ref_name);
    if(!adr) {
        printf("init_disasm_sig_ref: %s missing %s\n",rule->name,rule->ref_name);
        return 0;
    }
    if(!disasm_iter_init(fw,is,adr)) {
        printf("init_disasm_sig_ref: %s bad address 0x%08x for %s\n",rule->name,adr,rule->ref_name);
        return 0;
    }
    return 1;
}

int sig_match_near_str(firmware *fw, iter_state_t *is, sig_rule_t *rule);
uint32_t find_str_arg_call(firmware *fw, iter_state_t *is, sig_rule_t *rule);

// match
// r0=ref value
//...
// bl=<our func>
int sig_match_str_r0_call(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_str_r0_call: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return  0;
    }

//    printf("sig_match_str_r0_call: %s ref str %s 0x%08x\n",rule->name,rule->ref_name,str_adr);

    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(is->insn->detail->arm.operands[0].reg == ARM_REG_R0) {
            // printf("sig_match_str_r0_call: %s ref str %s ref 0x%"PRIx64"\n",rule->name,rule->ref_name,is->insn->address);
            // TODO should check if intervening insn nuke r0
            if(insn_match_find_next(fw,is,4,match_b_bl_blximm)) {
                uint32_t adr=get_branch_call_insn_target(fw,is);
                // printf("sig_match_str_r0_call: thumb %s call 0x%08x\n",rule->name,adr);
                return save_sig_with_j(fw,rule->name,adr);
            }
        }
    }
    return 0;
}

// find RegisterEventProcedure
int sig_match_reg_evp(firmware *fw, iter_state_t *is, __attribute__ ((unused))sig_rule_t *rule)
{
    const insn_match_t reg_evp_match[]={
        {MATCH_INS(MOV,   2),  {MATCH_OP_REG(R2),  MATCH_OP_REG(R1)}},
        {MATCH_INS(LDR,   2),  {MATCH_OP_REG(R1),  MATCH_OP_MEM_ANY}},
        {MATCH_INS(B,     MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };

    uint32_t e_to_evp=get_saved_sig_val("ExportToEventProcedure_FW");
    if(!e_to_evp) {
        printf("sig_match_reg_evp: failed to find ExportToEventProcedure, giving up\n");
        return 0;
    }

    //look for the underlying RegisterEventProcedure function (not currently used)
    uint32_t reg_evp=0;
    // start at identified Export..
    disasm_iter_init(fw,is,e_to_evp);
    if(insn_match_seq(fw,is,reg_evp_match)) {
        reg_evp=ADR_SET_THUMB(is->insn->detail->arm.operands[0].imm);
        //printf("RegisterEventProcedure found 0x%08x at %"PRIx64"\n",reg_evp,is->insn->address);
        save_sig(fw,"RegisterEventProcedure",reg_evp);
    }
    return (reg_evp != 0);
}

// find event proc table registration, and some other stuff
// TODO this should be broken up to some generic parts
int sig_match_reg_evp_table(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    // ref to find RegisterEventProcTable
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name); // note this string may appear more than once, assuming want first
    if(!str_adr) {
        printf("sig_match_reg_evp_table: failed to find %s\n",rule->ref_name);
        return 0;
    }
    //printf("sig_match_reg_evp_table: DispDev_EnableEventProc 0x%08x\n",str_adr);
    uint32_t reg_evp_alt1=0;
    uint32_t reg_evp_tbl=0;
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    uint32_t dd_enable_p=0;
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(is->insn->detail->arm.operands[0].reg != ARM_REG_R0) {
            continue;
        }
        if(!insn_match_find_next(fw,is,2,match_b_bl)) {
            continue;
        }
        reg_evp_alt1=ADR_SET_THUMB(is->insn->detail->arm.operands[0].imm);
        //printf("RegisterEventProcedure_alt1 found 0x%08x at %"PRIx64"\n",reg_evp_alt1,is->insn->address);
        save_sig(fw,"RegisterEventProcedure_alt1",reg_evp_alt1);

        uint32_t regs[4];

        // get r0 and r1, backtracking up to 4 instructions
        if((get_call_const_args(fw,is,4,regs)&3)==3) {
            // sanity check, arg0 was the original thing we were looking for
            if(regs[0]==str_adr) {
                dd_enable_p=regs[1]; // constant value should already have correct ARM/THUMB bit
                //printf("DispDev_EnableEventProc found 0x%08x at %"PRIx64"\n",dd_enable_p,is->insn->address);
                add_func_name(fw,"DispDev_EnableEventProc",dd_enable_p,NULL);
                break;
            }
        }
    }
    // found candidate function
    if(dd_enable_p) {
        disasm_iter_init(fw,is,dd_enable_p); // start at found func
        if(insn_match_find_next(fw,is,4,match_b_bl)) { // find the first bl
            // sanity check, make sure we get a const in r0
            uint32_t regs[4];
            if(get_call_const_args(fw,is,4,regs)&1) {
                reg_evp_tbl=ADR_SET_THUMB(is->insn->detail->arm.operands[0].imm);
                // printf("RegisterEventProcTable found 0x%08x at %"PRIx64"\n",reg_evp_tbl,is->insn->address);
                save_sig(fw,"RegisterEventProcTable",reg_evp_tbl);
            }
        }
    }
    return (reg_evp_tbl != 0);
}

// find an alternate eventproc registration call
int sig_match_reg_evp_alt2(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t reg_evp_alt2=0;
    // TODO could make this a param for different fw variants
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_reg_evp_alt2: failed to find %s\n",rule->ref_name);
        return 0;
    }
    //printf("sig_match_reg_evp_alt2: EngApp.Delete 0x%08x\n",str_adr);
    uint32_t reg_evp_alt1=get_saved_sig_val("RegisterEventProcedure_alt1");

    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(is->insn->detail->arm.operands[0].reg != ARM_REG_R0) {
            continue;
        }
        if(!insn_match_find_next(fw,is,3,match_b_bl)) {
            continue;
        }
        uint32_t regs[4];
        // sanity check, constants in r0, r1 and r0 was the original thing we were looking for
        if((get_call_const_args(fw,is,4,regs)&3)==3) {
            if(regs[0]==str_adr) {
                reg_evp_alt2=ADR_SET_THUMB(is->insn->detail->arm.operands[0].imm);
                // TODO could keep looking
                if(reg_evp_alt2 == reg_evp_alt1) {
                    printf("RegisterEventProcedure_alt2 == _alt1 at %"PRIx64"\n",is->insn->address);
                    reg_evp_alt2=0;
                } else {
                    save_sig(fw,"RegisterEventProcedure_alt2",reg_evp_alt2);
                   // printf("RegisterEventProcedure_alt2 found 0x%08x at %"PRIx64"\n",reg_evp_alt2,is->insn->address);
                    // TODO could follow alt2 and make sure it matches expected mov r2,0, bl register..
                }
                break;
            }
        }
    }
    return (reg_evp_alt2 != 0);
}

// find UnRegisterEventProc (made up name) for reference, finding tables missed by reg_event_proc_table
int sig_match_unreg_evp_table(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_unreg_evp_table: failed to find %s\n",rule->ref_name);
        return 0;
    }
    // for checks
    uint32_t reg_evp_alt1=get_saved_sig_val("RegisterEventProcedure_alt1");
    uint32_t reg_evp_alt2=get_saved_sig_val("RegisterEventProcedure_alt2");

    uint32_t mecha_unreg=0;
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        //printf("sig_match_unreg_evp_table: found ref 0x%"PRIx64"\n",is->insn->address);
        if(is->insn->detail->arm.operands[0].reg != ARM_REG_R0) {
            continue;
        }
        if(!insn_match_find_next(fw,is,3,match_b_bl)) {
            continue;
        }
        uint32_t reg_call=get_branch_call_insn_target(fw,is);
        // wasn't a registration call (could be unreg)
        // TODO could check veneers
        if(!reg_call || (reg_call != reg_evp_alt1 && reg_call != reg_evp_alt2)) {
            continue;
        }
        uint32_t regs[4];
        if((get_call_const_args(fw,is,4,regs)&3)==3) {
            // sanity check we got the right string
            if(regs[0]==str_adr) {
                mecha_unreg=ADR_SET_THUMB(regs[1]);
                break;
            }
        }
    }
    if(!mecha_unreg) {
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,mecha_unreg);
    // find first func call
    if(!insn_match_find_next(fw,is,7,match_b_bl)) {
        return 0;
    }
    // now find next ldr pc. Could follow above func, but this way picks up veneer on many fw
    const insn_match_t match_ldr_r0[]={
        {MATCH_INS(LDR, 2),   {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(PC)}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next(fw,is,18,match_ldr_r0)) {
        return 0;
    }
    uint32_t tbl=LDR_PC2val(fw,is->insn);
    if(!tbl) {
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_unreg_evp_table: disasm failed\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}
// find some veneers of RegisterEventProcTable/UnRegisterEventProcTable
// expects b <something else>, b <ref_name>
// TODO should be a generic find veneer near?
int sig_match_evp_table_veneer(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t ref_adr = get_saved_sig_val(rule->ref_name);
    int prevb = 0;
    uint32_t cadr;
    // following should probably be done using fw_search_insn
    // both veneers consist of a single b instruction, always preceded by another b instruction
    disasm_iter_init(fw,is,ref_adr); // start at our known function
    while (is->adr < (ref_adr+0x800)) {
        cadr = is->adr;
        if (!disasm_iter(fw,is)) {
            disasm_iter_set(fw,is,(is->adr+2) | fw->thumb_default);
        }
        else {
            if (is->insn->id == ARM_INS_B) {
                uint32_t b_adr = get_branch_call_insn_target(fw,is);
                if (prevb && (b_adr == ref_adr)) {
                    // this doesn't use _with_j since we want identify the veneer
                    add_func_name(fw,rule->name,cadr | is->thumb,NULL);
                    return 1;
                }
                prevb = 1;
            }
        }
    }
    return 0;
}

// like sig_match_str_r0_call, but check whether it's the same as existing func
int sig_match_createtaskstrictly_alt(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_createtaskstrictly_alt: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return  0;
    }

    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(is->insn->detail->arm.operands[0].reg == ARM_REG_R0) {
            // printf("sig_match_str_r0_call: %s ref str %s ref 0x%"PRIx64"\n",rule->name,rule->ref_name,is->insn->address);
            // TODO should check if intervening insn nuke r0
            if(insn_match_find_next(fw,is,4,match_b_bl_blximm)) {
                uint32_t adr=get_branch_call_insn_target(fw,is);
                uint32_t adr2 = get_saved_sig_val("CreateTaskStrictly");
                // only save IFF not identical to CreateTaskStrictly
                if(adr == adr2) {
                    // printf("sig_match_createtaskstrictly_alt: adr == CreateTaskStrictly\n");
                    return 0;
                }
                adr2 = get_saved_sig_val("j_CreateTaskStrictly");
                if(adr == adr2) {
                    // printf("sig_match_createtaskstrictly_alt: adr == j_CreateTaskStrictly\n");
                    return 0;
                }
                return save_sig_with_j(fw,rule->name,adr);
            }
        }
    }
    return 0;
}

// like named sub, but check if identical to existing CreateTask
int sig_match_createtask_alt(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    // if ref doesn't exist, quietly bail
    if(!get_saved_sig_val(rule->ref_name)) {
        return 0;
    }

    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
        printf("sig_match_createtask_alt: bl match failed\n");
        return 0;
    }
    uint32_t adr = get_branch_call_insn_target(fw,is);
    uint32_t adr2 = get_saved_sig_val("CreateTask");
    // only save IFF not identical to CreateTask
    if(adr == adr2) {
        // printf("sig_match_createtask_alt: adr == CreateTask\n");
        return 0;
    }
    adr2 = get_saved_sig_val("j_CreateTask");
    if(adr == adr2) {
        // printf("sig_match_createtask_alt: adr == j_CreateTask\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,adr);
}


int sig_match_get_nd_value(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    // this match is only valid for cameras with both ND and Iris
    if(!get_misc_val_value("CAM_HAS_ND_FILTER") || !get_misc_val_value("CAM_HAS_IRIS_DIAPHRAGM")) {
        return 0;
    }

    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,16,"ClearEventFlag")) {
        printf("sig_match_get_nd_value: no match ClearEventFlag\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        printf("sig_match_get_nd_value: bl match 1 failed\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    disasm_iter(fw,is);
    if (B_target(fw,is->insn))
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // first call should be either get_nd_value or GetUsableAvRange
    if(!insn_match_find_next(fw,is,5,match_bl_blximm)) {
        printf("sig_match_get_nd_value: bl match 2 failed\n");
        return 0;
    }
    uint32_t addr=get_branch_call_insn_target(fw,is);
    if(addr == get_saved_sig_val("GetUsableAvRange")) {
        printf("sig_match_get_nd_value: found GetUsableAvRange, iris or ND only?\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,addr);
}

int sig_match_get_current_exp(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,2,match_bl_blximm)) {
        printf("sig_match_get_current_exp: bl match 1 failed\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
        printf("sig_match_get_current_exp: bl match 2 failed\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
        printf("sig_match_get_current_exp: bl match 3 failed\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_get_current_nd_value(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    // this match is only valid for cameras with both ND and Iris
    if(!get_misc_val_value("CAM_HAS_ND_FILTER") || !get_misc_val_value("CAM_HAS_IRIS_DIAPHRAGM")) {
        return 0;
    }
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,36,"GetCurrentShutterSpeed_FW")) {
        printf("sig_match_get_current_nd_value: no match GetCurrentShutterSpeed_FW\n");
        return 0;
    }
    // bl <func we want>
    // strh r0, [rN,8]
    const insn_match_t match_bl_strh[]={
        {MATCH_INS(BL,MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS(STRH,2), {MATCH_OP_REG(R0),  MATCH_OP_MEM(INVALID,INVALID,0x8)}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,10,match_bl_strh)) {
        printf("sig_match_get_current_nd_value: match failed\n");
        return 0;
    }
    // rewind one for call
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,1));
    disasm_iter(fw,is);
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_get_current_deltasv(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,36,"GetCurrentShutterSpeed_FW")) {
        printf("sig_match_get_current_deltasv: no match GetCurrentShutterSpeed_FW\n");
        return 0;
    }
    // bl <func we want>
    // strh r0, [rN,4]
    const insn_match_t match_bl_strh[]={
        {MATCH_INS(BL,MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS(STRH,2), {MATCH_OP_REG(R0),  MATCH_OP_MEM(INVALID,INVALID,0x4)}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,8,match_bl_strh)) {
        printf("sig_match_get_current_deltasv: match failed\n");
        return 0;
    }
    // rewind one for call
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,1));
    disasm_iter(fw,is);
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}


int sig_match_imager_active_callback(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match_ldr_bl_mov_pop[]={
        {MATCH_INS(LDR, 2),   {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(BL, MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS(MOV,2),  {MATCH_OP_REG(R0),  MATCH_OP_IMM(0)}},
        {MATCH_INS(POP, MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };

    if(!insn_match_find_next_seq(fw,is,28,match_ldr_bl_mov_pop)) {
        printf("sig_match_imager_active_callback: match failed\n");
        return 0;
    }
    // rewind to LDR r0,...
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,3));
    // get LDR value
    disasm_iter(fw,is);
    uint32_t f1=LDR_PC2val(fw,is->insn);
//    printf("f1 0x%08x\n",f1);
    // thumb bit should be set correctly
    return save_sig_with_j(fw,rule->name,f1);
}
int sig_match_imager_active(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }

    const insn_match_t match_ldr_mov_str_pop[]={
        {MATCH_INS(LDR, 2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(MOV,2), {MATCH_OP_REG_ANY,  MATCH_OP_IMM(1)}},
        {MATCH_INS(STR, 2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM_ANY}},
        {MATCH_INS(POP, MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };

    int backtrack=3;
    if(!insn_match_find_next_seq(fw,is,10,match_ldr_mov_str_pop)) {
        // re-init and try reverse mov/ldr order
        init_disasm_sig_ref(fw,is,rule);
        const insn_match_t match_mov_ldr_str_pop[]={
            {MATCH_INS(MOV,2), {MATCH_OP_REG_ANY,  MATCH_OP_IMM(1)}},
            {MATCH_INS(LDR, 2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM_BASE(PC)}},
            {MATCH_INS(STR, 2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM_ANY}},
            {MATCH_INS(POP, MATCH_OPCOUNT_IGNORE)},
            {ARM_INS_ENDING}
        };
        if(!insn_match_find_next_seq(fw,is,10,match_mov_ldr_str_pop)) {
            printf("sig_match_imager_active: match failed\n");
            return 0;
        }
        backtrack=2;
    }
    // rewind to LDR
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,backtrack));
    disasm_iter(fw,is);
    uint32_t base=LDR_PC2val(fw,is->insn);
    uint32_t reg=is->insn->detail->arm.operands[0].reg;
//    printf("base 0x%08x @0x%08x\n",base,(uint32_t)is->insn->address);
    // skip mov if after LDR
    if(backtrack == 3) {
        disasm_iter(fw,is);
    }
    disasm_iter(fw,is);
    // sanity check base is the same as LDR'd to
    if(is->insn->detail->arm.operands[1].mem.base != reg) {
        printf("sig_match_imager_active: reg mismatch\n");
        return 0;
    }
    uint32_t off=is->insn->detail->arm.operands[1].mem.disp;
//    printf("off 0x%08x @0x%08x\n",off,(uint32_t)is->insn->address);
    save_misc_val("imager_active",base,off,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_screenlock_helper(firmware *fw, iter_state_t *is, sig_rule_t *rule) {
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t init_adr = (uint32_t)is->adr | is->thumb;
    // match specific instruction sequence instead of first call because g3x, g5x have different code
    // not by dryos rev, sx710 has same code as earlier cams
    const insn_match_t match_cmp_bne_bl[]={
        {MATCH_INS(CMP, 2), {MATCH_OP_REG(R0),  MATCH_OP_IMM(0)}},
        {MATCH_INS_CC(B,NE,MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS(BL,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    const insn_match_t match_ldrpc_mov_b[]={
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R1),  MATCH_OP_IMM(0)}},
        {MATCH_INS_CC(B,AL,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    // match first seq, this is the func we want
    if(insn_match_find_next_seq(fw,is,6,match_cmp_bne_bl)) {
        return save_sig_with_j(fw,rule->name,init_adr);

    }
    // try alternate match on newer cameras (around r57, but some variation)
    // this version puts a pointer to the function used in normal match in r0, 0 in R1, and jumps
    // go back to start
    disasm_iter_init(fw,is,init_adr);
    if(!insn_match_find_next_seq(fw,is,1,match_ldrpc_mov_b)) {
        printf("sig_match_screenlock_helper: match 2 failed 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    disasm_iter_init(fw,is,init_adr);
    disasm_iter(fw,is);
    uint32_t adr = LDR_PC2val(fw,is->insn);
    if(!adr) {
        printf("sig_match_screenlock_helper: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    disasm_iter_init(fw,is,adr);
    // retry original match on pointer
    if(!insn_match_find_next_seq(fw,is,6,match_cmp_bne_bl)) {
        printf("sig_match_screenlock_helper: match failed 0x%8x\n",init_adr);
        return 0;
    }
    return save_sig_with_j(fw,rule->name,adr);
}

int sig_match_fclose_low(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        // printf("sig_match_fclose_low: missing ref\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,24,"strlen")) {
        // printf("sig_match_fclose_low: no strlen\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,26,"malloc")) {
        // printf("sig_match_fclose_low: no malloc\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,14,"strcpy")) {
        // printf("sig_match_fclose_low: no strcpy\n");
        return 0;
    }

    if(!insn_match_find_nth(fw,is,12,3,match_bl_blximm)) {
        // printf("sig_match_fclose_low: no bl\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_screenunlock(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // look for ScreenLock call to verify right version of UIFS_DisplayFirmUpdateView
    if(!find_next_sig_call(fw,is,14,"ScreenLock")) {
        // printf("sig_match_screenunlock: no ScreenLock\n");
        return 0;
    }

    // expect tail call to ScreenUnlock
    const insn_match_t match_end[]={
        {MATCH_INS(POP, MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS_CC(B,AL,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,38,match_end)) {
        // printf("sig_match_screenunlock: match failed\n");
        return 0;
    }
    // TODO would be nice to have some validation
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// look for f(0x60,"_Simage") at start of task_StartupImage
int sig_match_log_camera_event(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
        // printf("sig_match_log_camera_event: bl match failed\n");
        return 0;
    }
    uint32_t regs[4];
    if((get_call_const_args(fw,is,4,regs)&3)!=3) {
        // printf("sig_match_log_camera_event: get args failed\n");
        return 0;
    }
    if(regs[0] != 0x60) {
        // printf("sig_match_log_camera_event: bad r0 0x%x\n",regs[0]);
        return 0;
    }
    const char *str=(char *)adr2ptr(fw,regs[1]);
    if(!str || strcmp(str,"_SImage") != 0) {
        // printf("sig_match_log_camera_event: bad r1 0x%x\n",regs[1]);
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// TODO this finds multiple values in PhySwTask main function
int sig_match_physw_misc(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        osig* ostub2 = find_sig(fw->sv->stubs,rule->ref_name);
        if (ostub2 && ostub2->val)
        {
            disasm_iter_init(fw,is,ostub2->val);
        }
        else
        {
            return 0;
        }
    }
    int i;
    uint32_t physw_run=0;
    for(i=0; i<3; i++) {
        if(!disasm_iter(fw,is)) {
            printf("sig_match_physw_misc: disasm failed\n");
            return 0;
        }
        physw_run=LDR_PC2val(fw,is->insn);
        if(physw_run) {
            if(adr_is_var(fw,physw_run)) {
                save_misc_val("physw_run",physw_run,0,(uint32_t)is->insn->address);
                break;
            } else {
                printf("sig_match_physw_misc: adr not data? 0x%08x\n",physw_run);
                return 0;
            }
        }
    }
    if(!physw_run) {
        return 0;
    }

    // look for physw_sleep_delay, offset from physw_run, loaded before SleepTask
    if(!insn_match_find_next(fw,is,7,match_bl_blximm)) {
        return 0;
    }
    uint32_t sleeptask=get_saved_sig_val("SleepTask");
    if(!sleeptask) {
        printf("sig_match_physw_misc: missing SleepTask\n");
        return 0;
    }
    uint32_t f=get_branch_call_insn_target(fw,is);

    // call wasn't direct, check for veneer
    if(f != sleeptask) {
        fw_disasm_iter_single(fw,f);
        uint32_t f2=get_direct_jump_target(fw,fw->is);
        if(f2 != sleeptask) {
            return 0;
        }
        // sleeptask veneer is useful for xref
        // re-adding existing won't hurt
        save_sig_with_j(fw,"SleepTask",f);
    }
    // rewind 1 for r0
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,1));
    if(!disasm_iter(fw,is)) {
        printf("sig_match_physw_misc: disasm failed\n");
        return 0;
    }
    // TODO could check base is same reg physw_run was loaded into
    if(is->insn->id != ARM_INS_LDR
        || is->insn->detail->arm.operands[0].reg != ARM_REG_R0) {
        return 0;
    }
    save_misc_val("physw_sleep_delay",physw_run,is->insn->detail->arm.operands[1].mem.disp,(uint32_t)is->insn->address);
    // skip over sleeptask to next insn
    if(!disasm_iter(fw,is)) {
        printf("sig_match_physw_misc: disasm failed\n");
        return 0;
    }

    // look for kbd_p1_f
    if(!insn_match_find_next(fw,is,2,match_bl_blximm)) {
        return 0;
    }
    save_sig(fw,"kbd_p1_f",get_branch_call_insn_target(fw,is));

    // look for kbd_p2_f
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        return 0;
    }
    save_sig(fw,"kbd_p2_f",get_branch_call_insn_target(fw,is));
    return 1;
}
// TODO also finds p1_f_cont, physw_status
int sig_match_kbd_read_keys(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // look for kbd_read_keys
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        return 0;
    }
    save_sig(fw,"kbd_read_keys",get_branch_call_insn_target(fw,is));
    if(!disasm_iter(fw,is)) {
        printf("sig_match_kbd_read_keys: disasm failed\n");
        return 0;
    }
    uint32_t physw_status=LDR_PC2val(fw,is->insn);
    if(physw_status) {
        save_misc_val("physw_status",physw_status,0,(uint32_t)is->insn->address);
        save_sig(fw,"kbd_p1_f_cont",(uint32_t)(is->insn->address) | is->thumb);
        return 1;
    }
    return 0;
}

// TODO also finds kbd_read_keys_r2
int sig_match_get_kbd_state(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // instructions that zero out physw_status
    insn_match_t match[]={
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };

    if(!insn_match_find_next_seq(fw,is,11,match)) {
        return 0;
    }
    save_sig_with_j(fw,"GetKbdState",get_branch_call_insn_target(fw,is));
    // look for kbd_read_keys_r2
    if(!insn_match_find_next(fw,is,5,match_b_bl_blximm)) {
        return 0;
    }
    save_sig_with_j(fw,"kbd_read_keys_r2",get_branch_call_insn_target(fw,is));
    return 1;
}

int sig_match_get_dial_hw_position(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t adr = find_last_call_from_func(fw,is,18,50);
    if(!adr) {
        // printf("sig_match_get_dial_hw_position: no match call 1 at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,adr);
    adr = find_last_call_from_func(fw,is,16,32);
    if(!adr) {
        // printf("sig_match_get_dial_hw_position: no match call 2 at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,adr);
    // find next function call
    if(!insn_match_find_next(fw,is,30,match_bl_blximm)) {
        // printf("sig_match_get_dial_hw_position: no match call 3 at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    uint32_t fadr = get_branch_call_insn_target(fw,is);
    // rewind and match instructions for sanity check
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,4));
    const insn_match_t match_hw_dial_call[]={
        {MATCH_INS(ADD,MATCH_OPCOUNT_ANY), {MATCH_OP_REG(R0)}},
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0),MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(BL,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next(fw,is,4,match_hw_dial_call)) {
        // printf("sig_match_get_dial_hw_position: no match seq 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    return save_sig_with_j(fw,rule->name,fadr);
}

int sig_match_create_jumptable(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // find second function call
    if(!insn_match_find_nth(fw,is,20,2,match_bl_blximm)) {
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_next(fw,is,15,match_bl_blximm)) {
        return 0;
    }
    // TODO could verify it looks right (version string)
    save_sig(fw,"CreateJumptable",get_branch_call_insn_target(fw,is));
    return 1;
}

// TODO this actually finds a bunch of different stuff
int sig_match_take_semaphore_strict(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // find first function call
    if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // find second call
    if(!insn_match_find_nth(fw,is,10,2,match_bl_blximm)) {
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // skip two calls, next should be DebugAssert
    if(!insn_match_find_nth(fw,is,20,3,match_bl_blximm)) {
        return 0;
    }
    save_sig_with_j(fw,"DebugAssert",get_branch_call_insn_target(fw,is));

    // next should be TakeSemaphoreStrictly
    if(!insn_match_find_next(fw,is,7,match_bl_blximm)) {
        return 0;
    }
    save_sig_with_j(fw,"TakeSemaphoreStrictly",get_branch_call_insn_target(fw,is));
    arm_reg ptr_reg = ARM_REG_INVALID;
    uint32_t sem_adr=0;
    int i;
    // iterate backwards looking for the value put in r0
    for(i=1; i<7; i++) {
        fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
        cs_insn *insn=fw->is->insn;
        if(insn->id != ARM_INS_LDR) {
            continue;
        }
        if(ptr_reg == ARM_REG_INVALID
            && insn->detail->arm.operands[0].reg == ARM_REG_R0
            && insn->detail->arm.operands[1].mem.base != ARM_REG_PC) {
            ptr_reg = insn->detail->arm.operands[1].mem.base;
            continue;
        }
        if(ptr_reg == ARM_REG_INVALID || !isLDR_PC(insn) || (arm_reg)insn->detail->arm.operands[0].reg != ptr_reg) {
            continue;
        }
        sem_adr=LDR_PC2val(fw,insn);
        if(sem_adr) {
            break;
        }
    }
    if(!sem_adr) {
        return 0;
    }
    save_misc_val("fileio_semaphore",sem_adr,0,(uint32_t)is->insn->address);
    // look for next call: GetDrive_FreeClusters
    if(!insn_match_find_next(fw,is,10,match_bl_blximm)) {
        return 0;
    }
    uint32_t adr=get_branch_call_insn_target(fw,is);
    int rv = save_sig_with_j(fw,"GetDrive_FreeClusters",adr);

    // GetDrive_TotalClusters is function immediately before GetDrive_FreeClusters
    adr -= 40;
    fw_disasm_iter_single(fw,adr);
    for (i=0; i<10; i+=1) {
        if (fw->is->insn->id == ARM_INS_PUSH && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_R4) {
            save_sig_with_j(fw,"GetDrive_TotalClusters",(fw->is->insn->address) | is->thumb);
            break;
        }
        fw_disasm_iter(fw);
    }

    return rv;
}

int sig_match_get_semaphore_value(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_get_semaphore_value: failed to find ref %s\n",rule->ref_name);
        return 0;
    }

    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    // assume first / only ref
    if(!fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // printf("sig_get_semaphore_value: failed to find code ref to %s\n",rule->ref_name);
        return 0;
    }
    // search backwards for func call
    uint32_t fadr=0;
    int i;
    for(i=1; i<=5; i++) {
        if(!fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i))) {
            // printf("sig_get_semaphore_value: disasm failed\n");
            return 0;
        }
        if(insn_match_any(fw->is->insn,match_bl_blximm)){
            fadr=get_branch_call_insn_target(fw,fw->is);
            break;
        }
    }
    if(!fadr) {
        // printf("sig_get_semaphore_value: failed to find bl 1\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,fadr);
    // look for first call
    if(!insn_match_find_next(fw,is,9,match_bl_blximm)) {
        // printf("sig_get_semaphore_value: failed to find bl 2\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}
// similar to sig_match_str_r0_call, but string also appears with Fopen_Fut
int sig_match_stat(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_stat: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return  0;
    }

    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(is->insn->detail->arm.operands[0].reg == ARM_REG_R0) {
            if(insn_match_find_next(fw,is,2,match_bl_blximm)) {
                uint32_t adr=get_branch_call_insn_target(fw,is);
                // same string ref'd by Fopen
                if(is_sig_call(fw,is,"Fopen_Fut_FW")) {
                    continue;
                }
                // TODO could check r1 not a const
                return save_sig_with_j(fw,rule->name,adr);
            }
        }
    }
    return 0;
}
static const insn_match_t match_open_mov_call[]={
    // 3 reg / reg movs, followed by a call
    {MATCH_INS(MOV, 2), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY}},
    {MATCH_INS(MOV, 2), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY}},
    {MATCH_INS(MOV, 2), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY}},
    {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
    {ARM_INS_ENDING}
};


// find low level open
int sig_match_open(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next_seq(fw,is,48,match_open_mov_call)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// AllocateUncacheableMemory
int sig_match_umalloc(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // looking for 3rd call
    if(!insn_match_find_nth(fw,is,15,3,match_bl_blximm)) {
        return 0;
    }
    //follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // looking for 3rd call again
    if(!insn_match_find_nth(fw,is,14,3,match_bl_blximm)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// FreeUncacheableMemory
int sig_match_ufree(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // find the first call to strcpy
    if(!find_next_sig_call(fw,is,60,"strcpy_FW")) {
        return 0;
    }
    // find 3rd call
    if(!insn_match_find_nth(fw,is,12,3,match_bl_blximm)) {
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // Look for Close call
    if(!find_next_sig_call(fw,is,40,"Close_FW")) {
        return 0;
    }
    // next call should be FreeUncacheableMemory
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_deletefile_fut(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_deletefile_fut: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // next call should be DeleteFile_Fut
        if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
            continue;
        }
        // follow
        uint32_t adr=get_branch_call_insn_target(fw,is);
        if(!fw_disasm_iter_single(fw,adr)) {
            printf("sig_match_deletefile_fut: disasm failed\n");
            return 0;
        }
        const insn_match_t match_mov_r1[]={
            {MATCH_INS(MOV,     2), {MATCH_OP_REG(R1),  MATCH_OP_IMM_ANY}},
#if CS_API_MAJOR < 4
            {MATCH_INS(MOVS,    2), {MATCH_OP_REG(R1),  MATCH_OP_IMM_ANY}},
#endif
            {ARM_INS_ENDING}
        };

        if(!insn_match_any(fw->is->insn,match_mov_r1)){
            continue;
        }
        return save_sig_with_j(fw,rule->name,adr);
    }
    return 0;
}

uint32_t find_call_near_str(firmware *fw, iter_state_t *is, sig_rule_t *rule);

int sig_match_closedir(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_closedir: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(!find_next_sig_call(fw,is,60,"sprintf_FW")) {
            continue;
        }
        if(insn_match_find_nth(fw,is,7,2,match_bl_blximm)) {
            return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
        }
    }

    uint32_t call_adr = find_call_near_str(fw,is,rule);
    if(call_adr) {
        disasm_iter_init(fw,is,call_adr); // reset to a bit before where the string was found
        const insn_match_t match_closedir[]={
            {MATCH_INS(BL,MATCH_OPCOUNT_IGNORE)},
            {MATCH_INS(MOV, 2), {MATCH_OP_REG(R0),  MATCH_OP_REG_ANY}},
            {MATCH_INS(BL,MATCH_OPCOUNT_IGNORE)},
            {ARM_INS_ENDING}
        };
        if(insn_match_seq(fw,is,match_closedir)){
            return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
        }
    }

    return 0;
}

// Save sig for function call (BL/BLX) at current instruction
int save_sig_match_call(firmware* fw, sig_rule_t *rule, uint32_t call_adr)
{
    disasm_iter_init(fw,fw->is,call_adr); // reset to a bit before where the string was found
    disasm_iter(fw,fw->is);
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,fw->is));
}

int sig_match_readfastdir(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr;
    str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_readfastdir: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return 0;
    }
    const insn_match_t match_cbnz_r0[]={
        {MATCH_INS(CBNZ, 2), {MATCH_OP_REG(R0), MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    const insn_match_t match_cbz_r0[]={
        {MATCH_INS(CBZ, 2), {MATCH_OP_REG(R0), MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    int max_insns = SIG_NEAR_GET_OFFSET(rule->param);
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        uint32_t ref_adr = iter_state_adr(is);
        // Check for bl followed by cbnz in previous 2 instructions
        fw_disasm_iter_single(fw,adr_hist_get(&is->ah,2));
        if(insn_match_any(fw->is->insn,match_bl_blximm)) {
            uint32_t call_adr = iter_state_adr(fw->is);
            fw_disasm_iter_single(fw,adr_hist_get(&is->ah,1));
            if(insn_match_any(fw->is->insn,match_cbnz_r0)) {
                return save_sig_match_call(fw, rule, call_adr);
            }
        }
        int i;
        for(i=3; i<=max_insns; i++) {
            // Check for bl followed by cbz branching to ref_adr
            fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
            if(insn_match_any(fw->is->insn,match_bl_blximm)) {
                uint32_t call_adr = iter_state_adr(fw->is);
                fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i-1));
                if(insn_match_any(fw->is->insn,match_cbz_r0)) {
                    uint32_t b_adr = get_branch_call_insn_target(fw,fw->is);
                    if (ref_adr == b_adr) {
                        return save_sig_match_call(fw, rule, call_adr);
                    }
                }
            }
        }
    }
    printf("sig_match_readfastdir: no match %s\n",rule->name);
    return 0;
}

int sig_match_strrchr(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t sig_adr=get_saved_sig_val(rule->name);
    if (sig_adr == 0)
    {
        uint32_t call_adr = find_call_near_str(fw,is,rule);
        if(call_adr) {
            disasm_iter_init(fw,is,call_adr-4); // reset to a bit before where the string was found
            const insn_match_t match_mov_r1_imm[]={
                {MATCH_INS(MOV, 2), {MATCH_OP_REG(R1),  MATCH_OP_IMM_ANY}},
                {ARM_INS_ENDING}
            };
            if(insn_match_find_next(fw,is,2,match_mov_r1_imm)){
                return save_sig_match_call(fw, rule, call_adr);
            }
        }
    }
    return 0;
}

int sig_match_time(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_time: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return  0;
    }
    uint32_t fadr=0;
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // find second func after str ref
        if(insn_match_find_nth(fw,is,6,2,match_bl_blximm)) {
            fadr=get_branch_call_insn_target(fw,is);
            break;
        }
    }
    if(!fadr) {
        return 0;
    }
    // follow found func
    disasm_iter_init(fw,is,fadr);
    // find second call
    if(insn_match_find_nth(fw,is,11,2,match_bl_blximm)) {
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}

int sig_match_strncpy(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,60,"strcpy_FW")) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_strncmp(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_strncmp: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
            continue;
        }
        uint32_t regs[4];
        if((get_call_const_args(fw,is,4,regs)&6)==6) {
            // sanity check we got the right string
            if(regs[1]==str_adr &&  regs[2] == strlen(rule->ref_name)) {
                return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
            }
        }
    }
    return 0;
}

int sig_match_strtolx(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,130,"strncpy")) {
        return 0;
    }
    // find first call after strncpy
    if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
        return 0;
    }
    uint32_t adr=get_branch_call_insn_target(fw,is);
    if(!adr) {
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,adr);
    if(!disasm_iter(fw,is)) {
        printf("sig_match_strtolx: disasm failed\n");
        return 0;
    }
    // expect
    // mov r3, #0
    // b ...
    const insn_match_t match_mov_r3_imm[]={
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R3),  MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    if(!insn_match(is->insn,match_mov_r3_imm)){
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_strtolx: disasm failed\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// find the version of ExecuteEventProcedure that doesn't assert evp isn't reg'd
int sig_match_exec_evp(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_exec_evp: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // search backwards for push {r0,...}
        int i;
        for(i=1; i<=18; i++) {
            if(!fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i))) {
                break;
            }
            if(fw->is->insn->id == ARM_INS_PUSH && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_R0) {
                // push should be start of func
                uint32_t adr=(uint32_t)(fw->is->insn->address) | is->thumb;
                // search forward in original iter_state for DebugAssert. If found, in the wrong ExecuteEventProcedure
                if(find_next_sig_call(fw,is,28,"DebugAssert")) {
                    break;
                }
                return save_sig_with_j(fw,rule->name,adr);
            }
        }
    }
    return 0;
}

int sig_match_fgets_fut(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,16,"Fopen_Fut_FW")) {
        return 0;
    }
    disasm_iter(fw,is);
    disasm_iter(fw,is);
    if (B_target(fw,is->insn) && (is->insn->detail->arm.cc == ARM_CC_NE)) {
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    } else {
        if (B_target(fw,is->insn) && (is->insn->detail->arm.cc == ARM_CC_NE)) {
            disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
        }
    }
    if(!insn_match_find_nth(fw,is,20,1,match_bl_blximm)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_log(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match_pop6[]={
        {MATCH_INS(POP, 6), {MATCH_OP_REST_ANY}},
        {ARM_INS_ENDING}
    };
    // skip forward through 3x pop     {r4, r5, r6, r7, r8, lr}
    if(!insn_match_find_nth(fw,is,38,3,match_pop6)) {
        return 0;
    }
    // third call
    if(!insn_match_find_nth(fw,is,24,3,match_bl_blximm)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// this only works on DryOS r52 cams
int sig_match_pow_dry_52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if (fw->dryos_ver != 52) {
        return 0;
    }
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match_ldrd_r0_r1[]={
        {MATCH_INS(LDRD,    3), {MATCH_OP_REG(R0), MATCH_OP_REG(R1),    MATCH_OP_ANY}},
        {ARM_INS_ENDING}
    };
    // skip forward to first ldrd    r0, r1, [r...]
    if(!insn_match_find_next(fw,is,50,match_ldrd_r0_r1)) {
        return 0;
    }
    // prevent false positive
    if(is->insn->detail->arm.operands[2].mem.base == ARM_REG_SP) {
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_pow: disasm failed\n");
        return 0;
    }
    uint32_t adr=get_branch_call_insn_target(fw,is);
    if(!adr) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,adr);
}

// match for dryos > r52 cams
int sig_match_pow_dry_gt_52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if (fw->dryos_ver <= 52) {
        return 0;
    }
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match1a[]={
        {MATCH_INS(LDRSH,   2), {MATCH_OP_REG(R0),  MATCH_OP_MEM(SP,INVALID,0x12)}},
        {MATCH_INS(LDRD,    3), {MATCH_OP_REG(R2),  MATCH_OP_REG(R3), MATCH_OP_MEM(SP,INVALID,0x20)}},
        {MATCH_INS(STR,     2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM(SP,INVALID,0)}},
        {MATCH_INS(BL,      MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    const insn_match_t match1b[]={
        {MATCH_INS(MOV,     2), {MATCH_OP_REG(R2),  MATCH_OP_REG(R7)}},
        {MATCH_INS(LDRSH,   2), {MATCH_OP_REG(R0),  MATCH_OP_MEM(SP,INVALID,0x12)}},
        {MATCH_INS(MOV,     2), {MATCH_OP_REG(R3),  MATCH_OP_REG(R6)}},
        {MATCH_INS(STR,     2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM(SP,INVALID,0)}},
        {MATCH_INS(BL,      MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    const insn_match_t* match1[]={ match1a, match1b };
    int idx;
    for (idx = 0; idx < 2; idx += 1)
    {
        // match above sequence
        if(insn_match_find_next_seq(fw,is,50,match1[idx]))
            break;
        init_disasm_sig_ref(fw,is,rule);
    }
    // check for match
    if (idx >= 2)
        return 0;
    // match above sequence
    uint32_t adr=get_branch_call_insn_target(fw,is);
    if(!adr) {
        return 0;
    }
    // follow bl
    disasm_iter_init(fw,is,adr);
    const insn_match_t match2a[]={
        {MATCH_INS(LDRD,3), {MATCH_OP_REG(R0),  MATCH_OP_REG(R1),   MATCH_OP_MEM_ANY}},
        {MATCH_INS(BLX, 1), {MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    const insn_match_t match2b[]={
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R2),  MATCH_OP_REG(R0)}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R3),  MATCH_OP_REG(R1)}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R4),  MATCH_OP_IMM(0x40000000)}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R0),  MATCH_OP_IMM(0)}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R1),  MATCH_OP_REG(R4)}},
        {MATCH_INS(BL,  1), {MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    const insn_match_t* match2[]={ match2a, match2b };
    // match above sequence
    if(!insn_match_find_next_seq(fw,is,15,match2[idx])) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_sqrt(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // third call
    if(!insn_match_find_nth(fw,is,12,3,match_bl_blximm)) {
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!disasm_iter(fw,is)) {
        printf("sig_match_sqrt: disasm failed\n");
        return 0;
    }
    uint32_t j_tgt=get_direct_jump_target(fw,is);
    // veneer?
    if(j_tgt) {
        // follow
        disasm_iter_init(fw,is,j_tgt);
        if(!disasm_iter(fw,is)) {
            printf("sig_match_sqrt: disasm failed\n");
            return 0;
        }
    }
    uint32_t adr = (uint32_t)is->adr|is->thumb;
    // second call/branch (seems to be bhs)
    if(!insn_match_find_nth(fw,is,12,2,match_b_bl_blximm)) {
        // not second call, reset and try first
        disasm_iter_init(fw,is,adr);
        if(!insn_match_find_nth(fw,is,8,1,match_bl_blximm)) {
            return 0;
        }
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}
int sig_match_get_drive_cluster_size(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // only handle first match, don't expect multiple refs to string
    if(fw_search_insn(fw,is,search_disasm_str_ref,0,"A/OpLogErr.txt",(uint32_t)is->adr+260)) {
        // find first call after string ref
        if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
            // printf("sig_match_get_drive_cluster_size: bl not found\n");
            return 0;
        }
        // follow
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
        // find second call
        if(!insn_match_find_nth(fw,is,13,2,match_bl_blximm)) {
            // printf("sig_match_get_drive_cluster_size: call 1 not found\n");
            return 0;
        }
        // follow
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
        disasm_iter(fw,is);
        if (B_target(fw, is->insn))
            disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
        // find next call
        if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
            // printf("sig_match_get_drive_cluster_size: call 2 not found\n");
            return 0;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}

int sig_match_mktime_ext(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_mktime_ext: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // expect sscanf after str
        if(!find_next_sig_call(fw,is,12,"sscanf_FW")) {
            // printf("sig_match_mktime_ext: no sscanf\n");
            return 0;
        }
        // find next call
        if(!insn_match_find_next(fw,is,22,match_bl_blximm)) {
            // printf("sig_match_mktime_ext: no call\n");
            return 0;
        }
        // follow
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
        if(!disasm_iter(fw,is)) {
            printf("sig_match_mktime_ext: disasm failed\n");
            return 0;
        }
        uint32_t j_tgt=get_direct_jump_target(fw,is);
        // veneer?
        if(j_tgt) {
            // follow
            disasm_iter_init(fw,is,j_tgt);
            if(!disasm_iter(fw,is)) {
                printf("sig_match_mktime_ext: disasm failed\n");
                return 0;
            }
        }
        const insn_match_t match_pop4[]={
            {MATCH_INS(POP, 4), {MATCH_OP_REST_ANY}},
            {MATCH_INS(POP, 6), {MATCH_OP_REST_ANY}},
            {ARM_INS_ENDING}
        };

        // find pop
        if(!insn_match_find_next(fw,is,54,match_pop4)) {
            // printf("sig_match_mktime_ext: no pop\n");
            return 0;
        }
        if(!insn_match_find_next(fw,is,1,match_b)) {
            // printf("sig_match_mktime_ext: no b\n");
            return 0;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}

// match call after ref to "_EnrySRec" because "AC:Rec2PB" ref before push in function, hard to be sure of start
int sig_match_rec2pb(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_mktime_ext: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        const insn_match_t match_ldr_cbnz_r0[]={
            {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0), MATCH_OP_MEM_ANY}},
            {MATCH_INS(CBNZ, 2), {MATCH_OP_REG(R0), MATCH_OP_IMM_ANY}},
            {ARM_INS_ENDING}
        };
        if(!insn_match_find_next_seq(fw,is,10,match_ldr_cbnz_r0)) {
            // printf("sig_match_rec2pb: no cbnz\n");
            continue;
        }
        // follow
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
        if(!insn_match_find_next(fw,is,3,match_b_bl_blximm)) {
            // printf("sig_match_rec2pb: no call\n");
            // followed branch, doesn't make sense to keep searching
            return 0;
        }
        uint32_t adr = iter_state_adr(is);
        // follow for sanity check
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
        if(!find_next_sig_call(fw,is,16,"LogCameraEvent")) {
            // printf("sig_match_rec2pb: no LogCameraEvent call\n");
            return 0;
        }
        uint32_t regs[4];
        if((get_call_const_args(fw,is,4,regs)&3)!=3) {
            // printf("sig_match_rec2pb: failed to get args\n");
            return 0;
        }
        // sanity check starts with LogCameraEvent with expected number and string
        if(regs[0]==0x60 && adr2ptr(fw,regs[1]) && (strcmp((const char *)adr2ptr(fw,regs[1]),"AC:Rec2PB")==0)) {
            return save_sig_with_j(fw,rule->name,adr);
        } else {
            // printf("sig_match_rec2pb: bad args\n");
            return 0;
        }
    }
    return 0;
}

// could just do sig_match_named, 3rd b, but want more validation
int sig_match_get_parameter_data(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match_cmp_bhs[]={
        {MATCH_INS(CMP, 2), {MATCH_OP_REG_ANY, MATCH_OP_IMM_ANY}},
        {MATCH_INS_CC(B,HS,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,4,match_cmp_bhs)) {
        // printf("sig_match_get_parameter_data: no match cmp, bhs\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_next(fw,is,1,match_b)) {
        // printf("sig_match_get_parameter_data: no match b\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// PrepareDirectory (via string ref) points at something like
// mov r1, 1
// b PrepareDirectory_x
int sig_match_prepdir_x(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match_mov_r1_1[]={
        {MATCH_INS(MOV,     2), {MATCH_OP_REG(R1),  MATCH_OP_IMM(1)}},
#if CS_API_MAJOR < 4
        {MATCH_INS(MOVS,    2), {MATCH_OP_REG(R1),  MATCH_OP_IMM(1)}},
#endif
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next(fw,is,1,match_mov_r1_1)) {
        //printf("sig_match_prepdir_x: no match mov\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,1,match_b)) {
        //printf("sig_match_prepdir_x: no match b\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

// PrepareDirectory (via string ref) points at something like
// mov r1, 1
// b PrepareDirectory_x
int sig_match_prepdir_1(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t call_adr = find_call_near_str(fw,is,rule);
    if(call_adr) {
        disasm_iter_init(fw,is,call_adr);
        disasm_iter(fw,is);
        disasm_iter(fw,is);
        if (!CBx_target(fw,is->insn))
        {
            rule->param = SIG_NEAR_BEFORE(20,5);
            call_adr = find_call_near_str(fw,is,rule);
            if(!call_adr) {
                return 0;
            }
            disasm_iter_init(fw,is,call_adr);
            disasm_iter(fw,is);
            return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
        }
    }

    rule->param = SIG_NEAR_BEFORE(7,2);
    call_adr = find_call_near_str(fw,is,rule);
    if(!call_adr) {
        return 0;
    }
    return save_sig_match_call(fw, rule, call_adr);
}
// assume this function is directly after the 2 instructions of ref
int sig_match_prepdir_0(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t ref_pdx=get_saved_sig_val("PrepareDirectory_x");
    if(!ref_pdx) {
        printf("sig_match_prepdir_0: missing PrepareDirectory_x\n");
        return 0;
    }
    // skip over, assume validated previously
    disasm_iter(fw,is);
    disasm_iter(fw,is);
    // this should be the start address of our function
    uint32_t adr=(uint32_t)is->adr|is->thumb;
    const insn_match_t match_mov_r1_1[]={
        {MATCH_INS(MOV,     2), {MATCH_OP_REG(R1),  MATCH_OP_IMM(0)}},
#if CS_API_MAJOR < 4
        {MATCH_INS(MOVS,    2), {MATCH_OP_REG(R1),  MATCH_OP_IMM(0)}},
#endif
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next(fw,is,1,match_mov_r1_1)) {
        //printf("sig_match_prepdir_0: no match mov\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,1,match_b)) {
        //printf("sig_match_prepdir_0: no match b\n");
        return 0;
    }
    uint32_t pdx=get_branch_call_insn_target(fw,is);
    if(pdx != ref_pdx) {
        //printf("sig_match_prepdir_0: target 0x%08x != 0x%08x\n",pdx,ref_pdx);
        return 0;
    }
    return save_sig_with_j(fw,rule->name,adr);
}
int sig_match_mkdir(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match[]={
        {MATCH_INS(STRB,2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM_ANY}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R0),  MATCH_OP_REG(SP)}},
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R1),  MATCH_OP_MEM_BASE(SP)}},
        {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(insn_match_find_next_seq(fw,is,148,match)) {
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }

    init_disasm_sig_ref(fw,is,rule);
    const insn_match_t match2[]={
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R1),  MATCH_OP_REG_ANY}},
        {MATCH_INS(STRB,2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM_ANY}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R0),  MATCH_OP_REG(SP)}},
        {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,148,match2)) {
        //printf("sig_match_mkdir: no match\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_add_ptp_handler(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_add_ptp_handler: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // expect CreateTaskStrictly
        if(!find_next_sig_call(fw,is,8,"CreateTaskStrictly")) {
            // printf("sig_match_add_ptp_handler: no CreateTaskStrictly\n");
            continue;
        }
        // expect add_ptp_handler is 3rd call after CreateTask
        if(!insn_match_find_nth(fw,is,13,3,match_bl_blximm)) {
            // printf("sig_match_add_ptp_handler: no match bl\n");
            return 0;
        }
        // sanity check, expect opcode, func, 0
        uint32_t regs[4];
        if((get_call_const_args(fw,is,5,regs)&7)!=7) {
            // printf("sig_match_add_ptp_handler: failed to get args\n");
            return 0;
        }
        if(regs[0] < 0x9000 || regs[0] > 0x10000 || !adr2ptr(fw,regs[1]) || regs[2] != 0) {
            // printf("sig_match_add_ptp_handler: bad args 0x%08x 0x%08x 0x%08x\n",regs[0],regs[1],regs[2]);
            return 0;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}
int sig_match_qsort(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,90,"DebugAssert")) {
        // printf("sig_match_qsort: no DebugAssert\n");
        return 0;
    }
    if(!insn_match_find_nth(fw,is,38,3,match_bl_blximm)) {
        // printf("sig_match_qsort: no match bl\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // if call in first 4 insn, follow again (newer cams have an extra sub)
    if(insn_match_find_next(fw,is,4,match_bl_blximm)) {
        disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    }
    if(!insn_match_find_next(fw,is,14,match_bl_blximm)) {
        // printf("sig_match_qsort: no match bl (qsort)\n");
        return 0;
    }
    // sanity check, expect r1-r3 to be const
    uint32_t regs[4];
    if((get_call_const_args(fw,is,5,regs)&0xe)!=0xe) {
        // printf("sig_match_qsort: failed to get args\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}
// looks for sequence of calls near ref string RedEyeController.c
// DeleteFile_Fut
// ...
// strcpy
// ...
// strrchr
// ...
// DeleteDirectory_Fut
int sig_match_deletedirectory_fut(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_deletedirectory_fut: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    // TODO using larger than default "near" range, needed for sx710
    // not looking for ref to string, just code near where the actual string is
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - 2048) | fw->thumb_default); // reset to a bit before where the string was found
    uint32_t end_adr = ADR_ALIGN4(str_adr) + 2048;
    while(find_next_sig_call(fw,is,end_adr - (uint32_t)is->adr,"DeleteFile_Fut")) {
        if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
            // printf("sig_match_deletedirectory_fut: no match bl strcpy\n");
            continue;
        }
        if(!is_sig_call(fw,is,"strcpy")) {
            // printf("sig_match_deletedirectory_fut: bl not strcpy at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
            // printf("sig_match_deletedirectory_fut: no match bl strrchr at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(!is_sig_call(fw,is,"strrchr")) {
            // printf("sig_match_deletedirectory_fut: bl not strrchr at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        // verify that arg1 to strrch is /
        uint32_t regs[4];
        if((get_call_const_args(fw,is,2,regs)&0x2)!=0x2) {
            // printf("sig_match_deletedirectory_fut: failed to get strrchr r1 at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(regs[1] != '/') {
            // printf("sig_match_deletedirectory_fut: strrchr r1 not '/' at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(!insn_match_find_next(fw,is,5,match_bl_blximm)) {
            // printf("sig_match_deletedirectory_fut: no match bl at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}

/*
match
   ref "LogicalEvent:0x%04x:adr:%p,Para:%ld" (or "LogicalEvent:0x%08x:adr:%p,Para:%ld")
   bl      LogCameraEvent
   mov     r0, rN
   bl      <some func>
   bl      set_control_event (or veneer)
same string is used elsewhere, so match specific sequence
*/
int sig_match_set_control_event(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        // not logged, two different ref strings so failing one is normal
        // printf("sig_match_set_control_event: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
            // printf("sig_match_set_control_event: no match bl at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(!is_sig_call(fw,is,"LogCameraEvent")) {
            // printf("sig_match_set_control_event: not LogCameraEvent at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        const insn_match_t match_seq[]={
            {MATCH_INS(MOV, 2), {MATCH_OP_REG(R0),  MATCH_OP_REG_ANY,}},
            {MATCH_INS(BL, MATCH_OPCOUNT_IGNORE)},
            {MATCH_INS(BL, MATCH_OPCOUNT_IGNORE)},
            {ARM_INS_ENDING}
        };
        if(!insn_match_find_next_seq(fw,is,1,match_seq)) {
            // printf("sig_match_set_control_event: no match seq\n");
            continue;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}
// find displaybusyonscreen for r52 cams, later uses different code
int sig_match_displaybusyonscreen_52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if (fw->dryos_ver != 52) {
        return 0;
    }
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_displaybusyonscreen: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
            // printf("sig_match_displaybusyonscreen: no match bl at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(!is_sig_call(fw,is,"LogCameraEvent")) {
            // printf("sig_match_displaybusyonscreen: not LogCameraEvent at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(!find_next_sig_call(fw,is,4,"GUISrv_StartGUISystem_FW")) {
            // printf("sig_match_displaybusyonscreen: no match GUISrv_StartGUISystem_FW\n");
            continue;
        }
        if(!insn_match_find_nth(fw,is,5,2,match_bl_blximm)) {
            // printf("sig_match_displaybusyonscreen: no match bl 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}
// find undisplaybusyonscreen for r52 cams, later uses different code
int sig_match_undisplaybusyonscreen_52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if (fw->dryos_ver != 52) {
        return 0;
    }
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_undisplaybusyonscreen: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // assume same function display* was found in, skip to that
        if(!find_next_sig_call(fw,is,24,"displaybusyonscreen")) {
            // printf("sig_match_undisplaybusyonscreen: no match displaybusyonscreen\n");
            continue;
        }
        if(!find_next_sig_call(fw,is,12,"GUISrv_StartGUISystem_FW")) {
            // printf("sig_match_undisplaybusyonscreen: no match GUISrv_StartGUISystem_FW\n");
            continue;
        }
        if(!insn_match_find_nth(fw,is,6,3,match_bl_blximm)) {
            // printf("sig_match_undisplaybusyonscreen: no match bl 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}

int sig_match_try_take_sem_dry_gt_57(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,24,"ReceiveMessageQueue")) {
        printf("sig_match_try_take_sem_dry_gt_57: failed to find ReceiveMessageQueue\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,60,"bzero")) {
        printf("sig_match_try_take_sem_dry_gt_57: failed to find bzero\n");
        return 0;
    }
    if(insn_match_find_next(fw,is,3,match_bl_blximm)) {
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    printf("sig_match_try_take_sem_dry_gt_57: no match\n");
    return 0;
}

int sig_match_wait_all_eventflag_strict(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t str_adr = find_str_bytes_main_fw(fw,"EFTool.c");
    if(!str_adr) {
        printf("sig_match_wait_all_eventflag_strict: failed to find ref EFTool.c\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,60,"SleepTask")) {
        printf("sig_match_wait_all_eventflag_strict: failed to find SleepTask\n");
        return 0;
    }

    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,is->adr + 60)) {
        if(!insn_match_find_next(fw,is,6,match_bl_blximm)) {
            printf("sig_match_wait_all_eventflag_strict: no match bl 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    return 0;
}

int sig_match_get_num_posted_messages(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,50,"TakeSemaphore")) {
        printf("sig_match_get_num_posted_messages: failed to find TakeSemaphore\n");
        return 0;
    }
    // find next call
    if(!insn_match_find_next(fw,is,5,match_bl_blximm)) {
        printf("sig_match_get_num_posted_messages:  no match bl 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_set_hp_timer_after_now(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if (!str_adr) {
        printf("sig_match_set_hp_timer_after_now: failed to find ref %s\n",rule->ref_name);
        return 0;
    }
    while (str_adr) {
        disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
        while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
            if(!find_next_sig_call(fw,is,20,"ClearEventFlag")) {
                // printf("sig_match_set_hp_timer_after_now: failed to find ClearEventFlag\n");
                continue;
            }
            // find 3rd call
            if(!insn_match_find_nth(fw,is,13,3,match_bl_blximm)) {
                // printf("sig_match_set_hp_timer_after_now: no match bl 0x%"PRIx64"\n",is->insn->address);
                continue;
            }
            // check call args, expect r0 = 70000
            uint32_t regs[4];
            uint32_t found_regs = get_call_const_args(fw,is,6,regs);
            if((found_regs&0x1)!=0x1) {
                // some cameras load r0 through a base reg, try alternate match
                // r3 == 3 and r2 or r1 found and in ROM
                if((found_regs & 0x8) && regs[3] == 4) {
                    if((found_regs & 0x2 && regs[1] > fw->rom_code_search_min_adr)
                        || (found_regs & 0x4 && regs[2] > fw->rom_code_search_min_adr)) {
                        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
                    }
                }
                // printf("sig_match_set_hp_timer_after_now: failed to match args 0x%"PRIx64"\n",is->insn->address);
                continue;
            }
            // r1, r2 should be func pointers but may involve reg-reg moves that get_call_const_args doesn't track
            if(regs[0] != 70000) {
                // printf("sig_match_set_hp_timer_after_now: args mismatch 0x%08x 0x%08x 0x%"PRIx64"\n",regs[0],regs[1],is->insn->address);
                continue;
            }
            return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
        }
        str_adr = find_next_str_bytes_main_fw(fw,rule->ref_name,str_adr+8);
    }
    return 0;
}
int sig_match_transfer_src_overlay(firmware *fw, iter_state_t *is, sig_rule_t *rule) {
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // skip to debugassert
    if(!find_next_sig_call(fw,is,32,"DebugAssert")) {
        printf("sig_match_transfer_src_overlay: no match DebugAssert\n");
        return 0;
    }
    var_ldr_desc_t desc;
    if(!find_and_get_var_ldr(fw, is, 20,4, ARM_REG_R0, &desc)) {
        printf("sig_match_transfer_src_overlay: no match ldr\n");
        return 0;
    }
    // following should be call
    if(!insn_match_find_next(fw,is,1,match_bl_blximm)) {
        printf("sig_match_transfer_src_overlay: no match bl 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // main sig value
    uint32_t fadr = get_branch_call_insn_target(fw,is);
    // adding active_bitmap_buffer here
    // note 4 bytes after value used on many ports, but the value normally sent to transfer_src_overlay
    save_misc_val("active_bitmap_buffer",desc.adr_adj,desc.off,(uint32_t)is->insn->address);
    // pick up bitmap_buffer
    // expect
    // ldr rx,[reg bitmap buffer]
    // add r0, <reg from bitmap buffer>, const
    const insn_match_t bm_buf_match[]={
        {MATCH_INS(LDR,   2),  {MATCH_OP_REG_ANY,  MATCH_OP_MEM_ANY}},
        {MATCH_INS(ADD,   3),  {MATCH_OP_REG(R0), MATCH_OP_REG_ANY, MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    if(insn_match_find_next_seq(fw,is,1,bm_buf_match)) {
        if((arm_reg)is->insn->detail->arm.operands[1].reg == desc.reg_base) {
            save_misc_val("bitmap_buffer",desc.adr_adj,is->insn->detail->arm.operands[2].imm,(uint32_t)is->insn->address);
        }
        /*
        else {
            printf("sig_match_transfer_src_overlay: no match bitmap_buffer add 0x%"PRIx64"\n",is->insn->address);
        }
        */
    }
    /*
    else {
        printf("sig_match_transfer_src_overlay: no match bitmap_buffer seq 0x%"PRIx64"\n",is->insn->address);
    }
    */
    return save_sig_with_j(fw,rule->name,fadr);
}

// find exmem related stuff
int sig_match_exmem_vars(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t adr[2], fnd[2];
    if(!init_disasm_sig_ref(fw,is,rule)) {
        printf("sig_match_exmem_vars: missing ref\n");
        return 0;
    }
    // expect first LDR pc
    if(!insn_match_find_next(fw,is,15,match_ldr_pc)) {
        printf("sig_match_exmem_vars: match LDR PC failed\n");
        return 0;
    }
    adr[0]=LDR_PC2val(fw,is->insn);
    fnd[0]=(uint32_t)is->insn->address;
    if(!insn_match_find_next(fw,is,5,match_ldr_pc)) {
        printf("sig_match_exmem_vars: 2nd match LDR PC failed\n");
        return 0;
    }
    adr[1]=LDR_PC2val(fw,is->insn);
    fnd[1]=(uint32_t)is->insn->address;
    //printf("sig_match_exmem_vars: %x, %x\n",adr[0], adr[1]);
    int n;
    for (n=0; n<2; n++) {
        if (adr[n] < fw->data_start+fw->data_len) {
            uint32_t ladr = adr[n]-fw->data_start+fw->data_init_start;
            save_misc_val("exmem_types_table",ladr,0,fnd[n]);
            int m;
            int exm_typ_cnt = 0;
            for (m=0; m<42; m++) {
                if ( (fw_u32(fw,ladr+m*4)!=0) && isASCIIstring(fw, fw_u32(fw,ladr+m*4)) )
                {
                    char *extyp = (char*)adr2ptr(fw, fw_u32(fw,ladr+m*4));
                    if ( strncmp(extyp,"EXMEM",5)==0 )
                    {
                        exm_typ_cnt++;
                    }
                }
                else
                {
                    break;
                }
            }
            save_misc_val("exmem_type_count",exm_typ_cnt,0,ladr);
        }
        else if (adr[n] < fw->memisostart) {
            save_misc_val("exmem_alloc_table",adr[n],0,fnd[n]);
        }
    }
    return 1;
}

// find function that copies Zico Xtensa blobs to their destination (dryos 52)
int sig_match_zicokick_52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_zicokick_52: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found

    // search for string ref
    if(!fw_search_insn(fw,is,search_disasm_str_ref,0,rule->ref_name,(uint32_t)is->adr+SEARCH_NEAR_REF_RANGE)) {
        printf("sig_match_zicokick_52: failed to find insn ref %s\n",rule->ref_name);
        return 0;
    }
    // check preceding instruction
    if(!fw_disasm_iter_single(fw,adr_hist_get(&is->ah,1))) {
        printf("sig_match_zicokick_52: disasm failed\n");
        return 0;
    }
    if (!(isLDR_PC(fw->is->insn) && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_R0)) {
        printf("sig_match_zicokick_52: match ldr r0 failed\n");
        return 0;
    }
    // save backtracked address
    uint32_t adr=(uint32_t)(fw->is->insn->address) | is->thumb;
    // step forward one from string ref
    if(!disasm_iter(fw,is)) {
        printf("sig_match_zicokick_52: disasm failed\n");
        return 0;
    }
    if (is->insn->id == ARM_INS_PUSH && is->insn->detail->arm.operands[0].reg == ARM_REG_R4) {
        return save_sig_with_j(fw,rule->name,adr);
    }
    return 0;
}
// find function that copies Zico Xtensa blobs to their destination (dryos >52)
int sig_match_zicokick_gt52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_zicokick_gt52: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found

    // search for string ref
    if(!fw_search_insn(fw,is,search_disasm_str_ref,0,rule->ref_name,(uint32_t)is->adr+SEARCH_NEAR_REF_RANGE)) {
        printf("sig_match_zicokick_gt52: failed to find insn ref %s\n",rule->ref_name);
        return 0;
    }
    int i;
    // search backward for
    // ldr r0,...
    // push r4,...
    for(i=1; i<=8; i++) {
        if (!fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i))) {
            printf("sig_match_zicokick_gt52: disasm failed\n");
            return 0;
        }
        if (fw->is->insn->id == ARM_INS_PUSH && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_R4) {
            if (!fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i+1))) {
                printf("sig_match_zicokick_gt52: disasm failed\n");
                return 0;
            }
            if (isLDR_PC(fw->is->insn) && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_R0) {
                return save_sig_with_j(fw,rule->name,(uint32_t)(fw->is->insn->address) | is->thumb);
            }
            return 0;
        }
    }
    return 0;
}
int sig_match_zicokick_copy(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // TODO could be less strict on regs, 5 LDRs in a row is rare
    const insn_match_t match_ldrs_bl[]={
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R1),  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R2),  MATCH_OP_MEM_BASE(R0)}},
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(R0)}},
        {MATCH_INS(BL,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,30,match_ldrs_bl)) {
        printf("sig_match_zicokick_copy no match ldr\n");
        return 0;
    }
    // TODO could sanity check bl target
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_zicokick_values(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
// get_call_const_args doesn't currently handle ldr sequence
#if 0
    // first call is further from function start
    if(!find_next_sig_call(fw,is,64,"zicokick_copy")) {
        printf("sig_match_zicokick_values: no zicokick_copy 1\n");
        return 0;
    }
    while(1) {
        uint32_t regs[4];
        if((get_call_const_args(fw,is,7,regs)&0x7)==0x7) {
            printf("xtensa blob @ 0x%08x, loads to 0x%08x, size 0x%08x\n",regs[1],regs[0],regs[2]);
        } else {
            printf("sig_match_zicokick_values: failed to get regs\n");
        }
        if(!find_next_sig_call(fw,is,8,"zicokick_copy")) {
            break;
        }
    }
    return 1;
#endif
    int i;
    uint32_t uv[3] = {0,0,0};
    int uvi = 0;
    misc_blob_t *blobs=malloc((MISC_BLOB_XTENSA_MAX + 1)*sizeof(misc_blob_t));
    int n_blobs = 0;

    for(i=1; i<=64; i++) {
        if (!disasm_iter(fw,is)) {
            free(blobs);
            return 0;
        }
        if (is->insn->id == ARM_INS_LDR && is->insn->detail->arm.operands[1].type == ARM_OP_MEM) {
            uint32_t u = LDR_PC2val(fw,is->insn);
            if ((u<fw->base+fw->size8) && (u>fw->rom_code_search_max_adr)) {
                // address outside the main fw
                if (uvi<3) {
                    uv[uvi] = u;
                    uvi++;
                }
            }
        }
        else if (is->insn->id == ARM_INS_BL) {
            if (uvi==3) {
                // func call, all 3 addresses are in collection
                uint32_t bsize, bloadedto, badr, u;
                int j;
                badr = MAX(MAX(uv[0],uv[1]),uv[2]);
                for (j=0; j<3; j++) {
                    if (uv[j]!=badr) {
                        u = fw_u32(fw, uv[j]);
                        if (u<1024*1024*2) {
                            bsize = u;
                        }
                        else {
                            bloadedto = u;
                        }
                    }
                }
                if (bsize) {
                    if(n_blobs == MISC_BLOB_XTENSA_MAX) {
                        printf("sig_match_zicokick_values: ignoring xtensa blobs > %d\n",MISC_BLOB_XTENSA_MAX);
                        blobs[n_blobs].type = MISC_BLOB_TYPE_NONE;
                        break;
                    }
                    // printf("xtensa blob @ 0x%08x, loads to 0x%08x, size 0x%08x\n",badr,bloadedto,bsize);
                    blobs[n_blobs].type = MISC_BLOB_TYPE_XTENSA;
                    blobs[n_blobs].rom_adr = badr;
                    blobs[n_blobs].ram_adr = bloadedto;
                    blobs[n_blobs].size = bsize;
                    n_blobs++;
                }
            }
            uvi = 0;
        }
        else if (is->insn->id == ARM_INS_POP) {
            break;
        }
    }
    if(n_blobs > 0) {
        blobs[n_blobs].type = MISC_BLOB_TYPE_NONE;
        save_misc_val_blobs("zicokick_values",blobs,0);
        return 1;
    } else {
        free(blobs);
        return 0;
    }
}

int sig_match_init_ex_drivers(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    int i;
    int b_count;
    // search forward 32 instructions or 14 calls
    for(i=0, b_count = 0; i < 32 && b_count < 14; i++) {
        if (!disasm_iter(fw,is)) {
            printf("sig_match_init_ex_drivers: disasm failed 1\n");
            return 0;
        }
        uint32_t b_tgt = get_branch_call_insn_target(fw,is);
        if(!b_tgt) {
            continue;
        }
        b_count++;
        uint64_t next_adr = is->adr | is->thumb;
        disasm_iter_init(fw,is,b_tgt);
        if (!disasm_iter(fw,is)) {
            printf("sig_match_init_ex_drivers: disasm failed 2\n");
            return 0;
        }
        // expect the function we're looking for to start with a push
        if(is->insn->id == ARM_INS_PUSH) {
            if(find_next_sig_call(fw,is,30,"DebugAssert")) {
                uint32_t regs[4];
                if((get_call_const_args(fw,is,5,regs)&0x2)==0x2) {
                    const char *str=(char *)adr2ptr(fw,regs[1]);
                    if(str && strcmp(str,"InitExDrivers.c") == 0) {
                        return save_sig_with_j(fw,rule->name,b_tgt);
                    }
                }
            }
        }
        disasm_iter_init(fw,is,next_adr);
    }
    return 0;
}

int sig_match_omar_init(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t fadr = find_last_call_from_func(fw,is,20,42);
    if(!fadr) {
        printf("sig_match_omar_init: no match call\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,fadr);
    if(!find_next_sig_call(fw,is,44,"dry_memcpy")) {
        printf("sig_match_omar_init: no match dry_memcpy\n");
        return 0;
    }
    uint32_t regs[4];
    // expect dry_memcpy(stack ptr,rom ptr, 0x18)
    if((get_call_const_args(fw,is,5,regs)&0x6)!=0x6) {
        printf("sig_match_omar_init: no match dry_memcpy args 1\n");
        return 0;
    }
    if(regs[2] != 0x18 || !adr2ptr(fw,regs[1])) {
        printf("sig_match_omar_init: no match dry_memcpy args 2\n");
        return 0;
    }
    uint32_t dadr = regs[1];
    save_misc_val("omar_init_data",dadr,0,(uint32_t)is->insn->address);
    misc_blob_t *blobs=malloc(3*sizeof(misc_blob_t));
    int i;
    for(i = 0; i<2; i++) {
        uint32_t dst = fw_u32(fw,dadr + i*12);
        uint32_t src = fw_u32(fw,dadr + i*12 + 4);
        uint32_t bsize = fw_u32(fw,dadr + i*12 + 8);
        if(src && dst && bsize) {
            blobs[i].type = MISC_BLOB_TYPE_OMAR;
            blobs[i].rom_adr = src;
            blobs[i].ram_adr = dst;
            blobs[i].size = bsize;
        } else {
            printf("sig_match_omar_init: invalid blobs\n");
            free(blobs);
            blobs = NULL;
            break;
        }
    }
    if(blobs) {
        blobs[2].type = MISC_BLOB_TYPE_NONE;
        save_misc_val_blobs("omar_init_values",blobs,0);
    }

    return save_sig_with_j(fw,rule->name,fadr);
}

int sig_match_init_error_handlers(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,64,"init_ex_drivers")) {
        printf("sig_match_init_error_handlers: no match init_ex_drivers\n");
        return 0;
    }
    if(!insn_match_find_nth(fw,is,4,2,match_bl_blximm)) {
        printf("sig_match_init_error_handlers: no match bl\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_default_assert_handler(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,14,"set_assert_handler")) {
        printf("sig_match_default_assert_handler: no match set_assert_handler\n");
        return 0;
    }
    // expect func r0
    uint32_t regs[4];
    if((get_call_const_args(fw,is,1,regs)&0x1)!=0x1) {
        printf("sig_match_default_assert_handler: no match arg\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,regs[0]);
}

int sig_match_default_exception_handler(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,20,"set_exception_handler")) {
        printf("sig_match_default_exception_handler: no match set_exception_handler\n");
        return 0;
    }
    // expect func in r0
    uint32_t regs[4];
    if((get_call_const_args(fw,is,1,regs)&0x1)!=0x1) {
        printf("sig_match_default_exception_handler: no match arg\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,regs[0]);
}

int sig_match_default_panic_handler(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,28,"set_panic_handler")) {
        printf("sig_match_default_panic_handler: no match set_panic_handler\n");
        return 0;
    }
    // expect func in r0
    uint32_t regs[4];
    if((get_call_const_args(fw,is,1,regs)&0x1)!=0x1) {
        printf("sig_match_default_panic_handler: no match arg\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,regs[0]);
}

int sig_match_get_task_properties(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(fw_search_insn(fw,is,search_disasm_str_ref,0,"Occured Time  %s\n",(uint32_t)is->adr+170)) {
        // expect printf function follow by call
        if(!find_next_sig_call(fw,is,16,"dry_error_printf")) {
            printf("get_task_properties: no match dry_error_printf 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
        if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
            printf("sig_match_get_task_properties: no match bl 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
        return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
    }
    printf("sig_match_get_task_properties: no match 'Occured Time' 0x%"PRIx64"\n",is->insn->address);
    return 0;
}

int sig_match_enable_hdmi_power(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,14,"CreateEventFlagStrictly")) {
        printf("sig_match_enable_hdmi_power: no match CreateEventFlagStrictly\n");
        return 0;
    }
    const insn_match_t match_seq[]={
        {MATCH_INS(BL,   MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS(CBNZ, MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,4,match_seq)) {
        printf("sig_match_enable_hdmi_power: no match bl seq cbnz 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // function should be next call
    if (!disasm_iter(fw,is)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_disable_hdmi_power(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,24,"EnableHDMIPower")) {
        printf("sig_match_disable_hdmi_power: no match EnableHDMIPower\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,22,"ClearEventFlag")) {
        printf("sig_match_disable_hdmi_power: no match ClearEventFlag\n");
        return 0;
    }
    const insn_match_t match_seq[]={
        {MATCH_INS(BL,   MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R0),  MATCH_OP_IMM(1)}},
        {MATCH_INS(POP, MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,12,match_seq)) {
        printf("sig_match_disable_hdmi_power: no match seq bl movs pop 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // bl matched above should be func
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,2));
    if (!disasm_iter(fw,is)) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_levent_table(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        // printf("sig_match_levent_table: no match bl 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));

    // find first call of next function
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        // printf("sig_match_levent_table: no match bl 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));

    // first instruction should load address
    disasm_iter(fw,is);
    uint32_t adr=LDR_PC2val(fw,is->insn);
    if(!adr) {
        // printf("sig_match_levent_table: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return  0;
    }
    uint32_t *p=(uint32_t *)adr2ptr(fw,adr);
    if(!p) {
        printf("sig_match_levent_table: 0x%08x not a ROM adr 0x%"PRIx64"\n",adr,is->insn->address);
        return  0;
    }
    if(*(p+1) != 0x800) {
        printf("sig_match_levent_table: expected 0x800 not 0x%x at 0x%08x ref 0x%"PRIx64"\n",*(p+1),adr,is->insn->address);
        return  0;
    }
    // TODO saving the function might be useful for analysis
    save_misc_val(rule->name,adr,0,(uint32_t)is->insn->address);
    return 1;
}
int sig_match_flash_param_table(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // expect 3 asserts
    if(!insn_match_find_next(fw,is,14,match_bl_blximm)) {
        // printf("sig_match_flash_param_table: no match bl 1\n");
        return 0;
    }
    if(!is_sig_call(fw,is,"DebugAssert")) {
        // printf("sig_match_flash_param_table: bl 1 not DebugAssert at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    if(!insn_match_find_next(fw,is,7,match_bl_blximm)) {
        // printf("sig_match_flash_param_table: no match bl 2\n");
        return 0;
    }
    if(!is_sig_call(fw,is,"DebugAssert")) {
        // printf("sig_match_flash_param_table: bl 2 not DebugAssert at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    if(!insn_match_find_next(fw,is,8,match_bl_blximm)) {
        // printf("sig_match_flash_param_table: no match bl 3\n");
        return 0;
    }
    if(!is_sig_call(fw,is,"DebugAssert")) {
        // printf("sig_match_flash_param_table: bl 3 not DebugAssert at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // expect AcquireRecursiveLockStrictly, func
    if(!insn_match_find_nth(fw,is,14,2,match_bl_blximm)) {
        // printf("sig_match_flash_param_table: no match sub 1\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));

    // first call
    if(!insn_match_find_next(fw,is,8,match_bl_blximm)) {
        // printf("sig_match_flash_param_table: no match sub 1 bl\n");
        return 0;
    }

    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // first instruction should load address
    disasm_iter(fw,is);
    uint32_t adr=LDR_PC2val(fw,is->insn);
    if(!adr) {
        // printf("sig_match_flash_param_table: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return  0;
    }
    save_misc_val(rule->name,adr,0,(uint32_t)is->insn->address);
    return 1;
}
int sig_match_jpeg_count_str(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_jpeg_count_str: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // printf("sig_match_jpeg_count_str: str match 0x%"PRIx64"\n",is->insn->address);
        if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
            // printf("sig_match_jpeg_count_str: no match bl\n");
            continue;
        }
        if(!is_sig_call(fw,is,"sprintf_FW")) {
            // printf("sig_match_jpeg_count_str: not sprintf_FW at 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        // expect ptr in r0, str in r1
        uint32_t regs[4];
        if((get_call_const_args(fw,is,5,regs)&0x3)!=0x3) {
            // printf("sig_match_jpeg_count_str: failed to get sprintf args 0x%"PRIx64"\n",is->insn->address);
            continue;
        }
        if(regs[1] != str_adr) {
            // printf("sig_match_jpeg_count_str: expected r1 == 0x%08x not 0x%08x at 0x%"PRIx64"\n",str_adr, regs[1],is->insn->address);
            return 0;
        }
        if(!adr_is_var(fw,regs[0])) {
            // printf("sig_match_jpeg_count_str: r0 == 0x%08x not var ptr at 0x%"PRIx64"\n",regs[0],is->insn->address);
            return 0;
        }
        save_misc_val(rule->name,regs[0],0,(uint32_t)is->insn->address);
        return 1;
    }
    return 0;
}

// set a boolean misc val if ref is present
int sig_match_misc_flag_named(__attribute__ ((unused))firmware *fw, __attribute__ ((unused))iter_state_t *is, sig_rule_t *rule)
{
    uint32_t ref=get_saved_sig_val(rule->ref_name);
    save_misc_val(rule->name,(ref)?1:0,0,ref);
    return 1;
}

int sig_match_dry_memset(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        printf("sig_match_dry_memset: no bl 1\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_nth(fw,is,12,3,match_bl_blximm)) {
        printf("sig_match_dry_memset: no match bl 2\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_dry_memzero(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
        printf("sig_match_dry_memset: no bl 1\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_next(fw,is,7,match_bl_blximm)) {
        printf("sig_match_dry_memset: no match bl 2\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

#if 0
// alt match from dry_memset, doesn't pick up veneer
int sig_match_dry_memzero(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match_start[]={
        {MATCH_INS(AND, 3), {MATCH_OP_REG(R3),  MATCH_OP_REG(R2),   MATCH_OP_IMM(0xff)}},
        {MATCH_INS(ORR, 3), {MATCH_OP_REG(R2),  MATCH_OP_REG(R3),   MATCH_OP_REG(R3)}},
        {MATCH_INS(ORR, 3), {MATCH_OP_REG(R2),  MATCH_OP_REG(R2),   MATCH_OP_REG(R2)}},
        {MATCH_INS(B, 1), {MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,1,match_start)) {
        printf("sig_match_dry_memzero: no match start\n");
        return 0;
    }
    // dry_memset jumps into this function after the mov r2,0 - assumed ARM
    uint32_t adr = get_branch_call_insn_target(fw,is) - 4;
    disasm_iter_init(fw,is,adr);
    const insn_match_t match_mov_r2_0[]={
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R2), MATCH_OP_IMM(0x0)}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next(fw,is,1,match_mov_r2_0)) {
        printf("sig_match_dry_memzero: no match mov\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,adr);
}
#endif

int sig_match_dry_memcpy_bytes(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!insn_match_find_next(fw,is,7,match_bl_blximm)) {
        printf("sig_match_dry_memcpy_bytes: no bl 1\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // expect tail call to dry_memcpy_bytes
    const insn_match_t match_end[]={
        {MATCH_INS(POP, MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS_CC(B,AL,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };

    if(!insn_match_find_next_seq(fw,is,20,match_end)) {
        printf("sig_match_dry_memcpy_bytes: no match end\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}


int sig_match_cam_has_iris_diaphragm(__attribute__ ((unused))firmware *fw, __attribute__ ((unused))iter_state_t *is, sig_rule_t *rule)
{
    uint32_t v;
    uint32_t ref=0;get_saved_sig_val(rule->ref_name);
    // ILC assumed to have iris
    if(get_misc_val_value("CAM_IS_ILC")) {
        v=1;
    } else {
        ref=get_saved_sig_val(rule->ref_name);
        v=(ref)?1:0;
    }
    save_misc_val(rule->name,v,0,ref);
    return 1;
}

int sig_match_cam_uncached_bit(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    const insn_match_t match_bic_r0[]={
        {MATCH_INS(BIC, 3), {MATCH_OP_REG(R0),  MATCH_OP_REG(R0),   MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    if(insn_match_find_next(fw,is,4,match_bic_r0)) {
        save_misc_val(rule->name,is->insn->detail->arm.operands[2].imm,0,(uint32_t)is->insn->address);
        return 1;
    }
    return 0;
}

int sig_match_umalloc_strictly(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_umalloc_strictly: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return  0;
    }

    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    if(!fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        printf("sig_match_umalloc_strictly: faild to find ref insn\n");
        return 0;
    }
    if(is->insn->detail->arm.operands[0].reg != ARM_REG_R0) {
        printf("sig_match_umalloc_strictly: reg mismatch\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,4,match_bl_blximm)) {
        printf("sig_match_umalloc_strictly: no bl 1\n");
        return 0;
    }
    if(!is_sig_call(fw,is,"CreateTaskStrictly")) {
        printf("sig_match_umalloc_strictly: no CreateTaskStrictly\n");
        return 0;
    }
    // b included because usually tail call
    if(!insn_match_find_next(fw,is,6,match_b_bl_blximm)) {
        printf("sig_match_umalloc_strictly: no bl 1\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_next(fw,is,10,match_bl_blximm)) {
        printf("sig_match_umalloc_strictly: no bl 2\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_dcache_clean_flush_and_disable(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,44,"GetSRAndDisableInterrupt")) {
        printf("sig_match_dcache_clean_flush_and_disable: no GetSRAndDisableInterrupt\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,2,match_bl_blximm)) {
        printf("sig_match_dcache_clean_flush_and_disable: no bl\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_get_rom_id(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // two variants, overlapping dryos versions
    if(!disasm_iter(fw,is)) {
        printf("sig_match_get_rom_id: disasm failed\n");
        return 0;
    }
    if(is->insn->id == ARM_INS_MOV) {
        if(!disasm_iter(fw,is)) {
            printf("sig_match_get_rom_id: disasm failed\n");
            return 0;
        }
        if(is->insn->id != ARM_INS_B) {
            printf("sig_match_get_rom_id: no b\n");
            return 0;
        }
    } else if(is->insn->id == ARM_INS_PUSH) {
        const insn_match_t match_seq[]={
            {MATCH_INS(MOV, 2), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY}},
            {MATCH_INS(BL, 1), {MATCH_OP_IMM_ANY}},
            {MATCH_INS(MOV, 2), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY}},
            {MATCH_INS(POP, MATCH_OPCOUNT_IGNORE)},
            {MATCH_INS(B, MATCH_OPCOUNT_IGNORE)},
            {ARM_INS_ENDING}
        };
        if(!insn_match_find_next_seq(fw,is,1,match_seq)) {
            printf("sig_match_get_rom_id: no seq\n");
            return 0;
        }
    } else {
        printf("sig_match_get_rom_id: no match first insn\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match_dcache_flush_and_enable(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,12,"GetSRAndDisableInterrupt")) {
        printf("sig_match_dcache_flush_and_enable: no GetSRAndDisableInterrupt\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,8,"dcache_clean_flush_and_disable")) {
        printf("sig_match_dcache_flush_and_enable: no dcache_clean_flush_and_disable\n");
        return 0;
    }
    // some variants have a call in between, others are inline, not dry version specific
    // expect SetSR just after sig_match_dcache_flush_and_enable
    if(!find_next_sig_call(fw,is,112,"SetSR")) {
        printf("sig_match_dcache_flush_and_enable: no SetSR\n");
        return 0;
    }
    // call should be 2 insns before, rewind
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,2));
    disasm_iter(fw,is);
    uint32_t adr = get_branch_call_insn_target(fw,is);
    if(!adr) {
        printf("sig_match_dcache_flush_and_enable: no match call\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,adr);
}


int sig_match_physw_event_table(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // expect first LDR pc
    if(!insn_match_find_next(fw,is,5,match_ldr_pc)) {
        printf("sig_match_physw_event_table: match LDR PC failed\n");
        return 0;
    }
    uint32_t adr=LDR_PC2val(fw,is->insn);
    if(!adr) {
        printf("sig_match_physw_event_table: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    if(!adr2ptr(fw,adr)) {
        printf("sig_match_physw_event_table: adr not ROM 0x%08x at 0x%"PRIx64"\n",adr,is->insn->address);
        return 0;
    }
    save_misc_val(rule->name,adr,0,(uint32_t)is->insn->address);
    return 1;
}
int sig_match_uiprop_count(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,38,"DebugAssert")) {
        // printf("sig_match_uiprop_count: no DebugAssert 1\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,14,"DebugAssert")) {
        // printf("sig_match_uiprop_count: no DebugAssert 2\n");
        return 0;
    }
    const insn_match_t match_bic_cmp[]={
        {MATCH_INS(BIC, 3), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY,   MATCH_OP_IMM(0x8000)}},
        {MATCH_INS(CMP, 2), {MATCH_OP_REG_ANY,  MATCH_OP_ANY}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,3,match_bic_cmp)) {
        // printf("sig_match_uiprop_count: no bic,cmp\n");
        return 0;
    }
    save_misc_val(rule->name,is->insn->detail->arm.operands[1].imm,0,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_get_canon_mode_list(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_str_bytes_main_fw(fw,rule->ref_name);
    if(!str_adr) {
        printf("sig_match_get_canon_mode_list: failed to find ref %s\n",rule->ref_name);
        return  0;
    }
    uint32_t adr=0;
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,(ADR_ALIGN4(str_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,str_adr+SEARCH_NEAR_REF_RANGE)) {
        // printf("sig_match_get_canon_mode_list: str match 0x%"PRIx64"\n",is->insn->address);
        if(!find_next_sig_call(fw,is,4,"LogCameraEvent")) {
            // printf("sig_match_get_canon_mode_list: no LogCameraEvent\n");
            continue;
        }
        // some cameras have a mov and an extra call
        if(!disasm_iter(fw,is)) {
            // printf("sig_match_get_canon_mode_list: disasm failed\n");
            return 0;
        }
        const insn_match_t match_mov_r0_1[]={
#if CS_API_MAJOR < 4
            {MATCH_INS(MOVS, 2), {MATCH_OP_REG(R0),  MATCH_OP_IMM(1)}},
#endif
            {MATCH_INS(MOV, 2), {MATCH_OP_REG(R0),  MATCH_OP_IMM(1)}},
            {ARM_INS_ENDING}
        };
        if(insn_match_any(is->insn,match_mov_r0_1)) {
            if(!insn_match_find_nth(fw,is,2,2,match_bl_blximm)) {
                // printf("sig_match_get_canon_mode_list: no match bl 1x\n");
                continue;
            }
        } else {
            if(!insn_match_any(is->insn,match_bl_blximm)) {
                // printf("sig_match_get_canon_mode_list: no match bl 1\n");
                continue;
            }
        }
        // found something to follow, break
        adr=get_branch_call_insn_target(fw,is);
        break;
    }
    if(!adr) {
        return 0;
    }
    // printf("sig_match_get_canon_mode_list: sub 1 0x%08x\n",adr);
    disasm_iter_init(fw,is,adr);
    if(!find_next_sig_call(fw,is,40,"TakeSemaphoreStrictly")) {
        // printf("sig_match_get_canon_mode_list: no TakeSemaphoreStrictly\n");
        return 0;
    }
    // match second call
    if(!insn_match_find_nth(fw,is,12,2,match_b_bl_blximm)) {
        // printf("sig_match_get_canon_mode_list: no match bl 2\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    const insn_match_t match_loop[]={
        {MATCH_INS(ADD, 3), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY,   MATCH_OP_IMM(1)}},
        {MATCH_INS(UXTH, 2), {MATCH_OP_REG_ANY,  MATCH_OP_REG_ANY}},
        {MATCH_INS(CMP, 2), {MATCH_OP_REG_ANY,  MATCH_OP_IMM_ANY}},
        {MATCH_INS_CC(B,LO,MATCH_OPCOUNT_IGNORE)},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,64,match_loop)) {
        // printf("sig_match_get_canon_mode_list: match 1 failed\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,2,match_bl_blximm)) {
        // printf("sig_match_get_canon_mode_list: no match bl 3\n");
        return 0;
    }
    // should be func
    adr=get_branch_call_insn_target(fw,is);
    // sanity check
    disasm_iter_init(fw,is,adr);
    const insn_match_t match_ldr_r0_ret[]={
        {MATCH_INS(LDR, 2),   {MATCH_OP_REG(R0),  MATCH_OP_MEM_BASE(PC)}},
        {MATCH_INS(BX, 1),   {MATCH_OP_REG(LR)}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,1,match_ldr_r0_ret)) {
        // printf("sig_match_get_canon_mode_list: match 2 failed\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,adr);
}

int sig_match_zoom_busy(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // first call
    if(!insn_match_find_next(fw,is,5,match_bl_blximm)) {
        // printf("sig_match_zoom_busy: no match bl\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // get base address from first LDR PC
    if(!insn_match_find_next(fw,is,5,match_ldr_pc)) {
        // printf("sig_match_zoom_busy: match LDR PC failed\n");
        return 0;
    }
    uint32_t base=LDR_PC2val(fw,is->insn);
    arm_reg rb=is->insn->detail->arm.operands[0].reg;

    // look for first TakeSemaphoreStrictly
    if(!find_next_sig_call(fw,is,40,"TakeSemaphoreStrictly")) {
        // printf("sig_match_zoom_busy: no match TakeSemaphoreStrictly\n");
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        // printf("sig_match_zoom_busy: disasm failed\n");
        return 0;
    }
    // assume next instruction is ldr
    if(is->insn->id != ARM_INS_LDR
        || is->insn->detail->arm.operands[0].reg != ARM_REG_R0
        || is->insn->detail->arm.operands[1].mem.base != rb) {
        // printf("sig_match_zoom_busy: no match LDR\n");
        return 0;
    }
    save_misc_val(rule->name,base,is->insn->detail->arm.operands[1].mem.disp,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_focus_busy(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // look for first TakeSemaphore
    if(!find_next_sig_call(fw,is,40,"TakeSemaphore")) {
        // printf("sig_match_focus_busy: no match TakeSemaphore\n");
        return 0;
    }
    // next call
    if(!insn_match_find_next(fw,is,5,match_bl_blximm)) {
        // printf("sig_match_focus_busy: no match bl\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    // get base address from first LDR PC
    if(!insn_match_find_next(fw,is,5,match_ldr_pc)) {
        // printf("sig_match_focus_busy: match LDR PC failed\n");
        return 0;
    }
    uint32_t base=LDR_PC2val(fw,is->insn);
    arm_reg rb=is->insn->detail->arm.operands[0].reg;

    // look for first TakeSemaphoreStrictly
    if(!find_next_sig_call(fw,is,50,"TakeSemaphoreStrictly")) {
        // printf("sig_match_focus_busy: no match TakeSemaphoreStrictly\n");
        return 0;
    }
    const insn_match_t match_ldr[]={
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0), MATCH_OP_MEM_ANY}},
        {MATCH_INS(CBZ, 2), {MATCH_OP_REG(R0), MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,10,match_ldr)) {
        // printf("sig_match_focus_busy: no match LDR\n");
        return 0;
    }
    // rewind to LDR
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,1));
    disasm_iter(fw,is);
    // check LDR
    if(is->insn->detail->arm.operands[1].mem.base != rb) {
        // printf("sig_match_focus_busy: no match LDR base\n");
        return 0;
    }
    save_misc_val(rule->name,base,is->insn->detail->arm.operands[1].mem.disp,(uint32_t)is->insn->address);
    return 1;
}
int sig_match_aram_size(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        printf("sig_match_aram_size: missing ref\n");
        return 0;
    }
    const insn_match_t match_ldr_r0_sp_cmp[]={
        {MATCH_INS(LDR, 2), {MATCH_OP_REG(R0),MATCH_OP_MEM(SP,INVALID,0xc)}},
        {MATCH_INS(CMP, 2), {MATCH_OP_REG(R0),MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,15,match_ldr_r0_sp_cmp)) {
        printf("sig_match_aram_size: no match LDR\n");
        return 0;
    }
    uint32_t val=is->insn->detail->arm.operands[1].imm;
    if(val != 0x22000 && val != 0x32000) {
        printf("sig_match_aram_size: unexpected ARAM size 0x%08x\n",val);
    }
    save_misc_val(rule->name,val,0,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_aram_size_gt58(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        printf("sig_match_aram_size: missing ref\n");
        return 0;
    }
    const insn_match_t match_ldrd_r0r1_mov[]={
        {MATCH_INS(LDRD, 3), {MATCH_OP_REG(R0),MATCH_OP_REG(R1),MATCH_OP_MEM(SP,INVALID,0x10)}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R2),MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    // d7? variant
    const insn_match_t match_ldrd_r2r1_mov[]={
        {MATCH_INS(LDRD, 3), {MATCH_OP_REG(R2),MATCH_OP_REG(R1),MATCH_OP_MEM(SP,INVALID,0x10)}},
        {MATCH_INS(MOV, 2), {MATCH_OP_REG(R3),MATCH_OP_IMM_ANY}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,15,match_ldrd_r0r1_mov)) {
        init_disasm_sig_ref(fw,is,rule); // reset to start
        if(!insn_match_find_next_seq(fw,is,15,match_ldrd_r2r1_mov)) {
            printf("sig_match_aram_size: no match LDR\n");
        }
        return 0;
    }
    uint32_t val=is->insn->detail->arm.operands[1].imm;
    if(val != 0x22000 && val != 0x32000) {
        printf("sig_match_aram_size: unexpected ARAM size 0x%08x\n",val);
    }
    save_misc_val(rule->name,val,0,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_aram_start(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        printf("sig_match_aram_start: missing ref\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,50,"DebugAssert")) {
        printf("sig_aram_start: no match DebugAssert\n");
        return 0;
    }
    uint32_t m_adr = is->insn->address | is->thumb;
    const insn_match_t match_cmp_bne_ldr[]={
        {MATCH_INS(CMP, 2), {MATCH_OP_REG(R1),MATCH_OP_IMM(0)}},
        {MATCH_INS_CC(B,NE,MATCH_OPCOUNT_IGNORE)},
        {MATCH_INS(LDR, 2), {MATCH_OP_REG_ANY,MATCH_OP_MEM_BASE(PC)}},
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,15,match_cmp_bne_ldr)) {
        // Try alternate instruction sequence
        disasm_iter_init(fw, is, m_adr);
        const insn_match_t match_cmp_bne_ldr2[]={
            {MATCH_INS(CMP, 2), {MATCH_OP_REG(R1),MATCH_OP_IMM(0)}},
            {MATCH_INS_CC(B,NE,MATCH_OPCOUNT_IGNORE)},
            {MATCH_INS(LDR, 2), {MATCH_OP_REG_ANY,MATCH_OP_MEM_BASE(SP)}},
            {MATCH_INS(LDR, 2), {MATCH_OP_REG_ANY,MATCH_OP_MEM_BASE(PC)}},
            {ARM_INS_ENDING}
        };
        if(!insn_match_find_next_seq(fw,is,15,match_cmp_bne_ldr2)) {
            printf("sig_match_aram_start: no match CMP\n");
            return 0;
        }
    }
    uint32_t adr=LDR_PC2val(fw,is->insn);
    if(!adr) {
        printf("sig_match_aram_start: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // could sanity check that it looks like a RAM address
    save_misc_val(rule->name,adr,0,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_icache_flush_range(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        printf("sig_match_icache_flush_range: missing ref\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,60,"DebugAssert")) {
        printf("sig_icache_flush_range: no match DebugAssert\n");
        return 0;
    }
    if(!find_next_sig_call(fw,is,44,"dcache_flush_range")) {
        printf("sig_icache_flush_range: no match DebugAssert\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,5,match_bl_blximm)) {
        printf("sig_icache_flush_range: bl match failed at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

int sig_match__nrflag(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t fadr=is->adr;
    // find range check on input arg
    const insn_match_t match_cmp_b[]={
        {MATCH_INS(CMP, 2), {MATCH_OP_REG(R0),MATCH_OP_IMM_ANY}},
        {MATCH_INS(B,MATCH_OPCOUNT_IGNORE)}, // blo or blt may be used, so don't include cond
        {ARM_INS_ENDING}
    };
    if(!insn_match_find_next_seq(fw,is,4,match_cmp_b) || is->insn->detail->arm.cc == ARM_CC_AL) {
        printf("sig_match__nrflag: no match CMP\n");
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!disasm_iter(fw,is)) {
        printf("sig_match__nrflag: disasm failed\n");
        return 0;
    }
    // assume next is base addres
    uint32_t adr=LDR_PC2val(fw,is->insn);
    if(!adr) {
        printf("sig_match__nrflag: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    arm_reg reg_base = is->insn->detail->arm.operands[0].reg; // reg value was loaded into
    if(!disasm_iter(fw,is)) {
        printf("sig_match__nrflag: disasm failed\n");
        return 0;
    }
    // firmware may use add/sub to get actual firmware base address
    if(isADDx_imm(is->insn) || isSUBx_imm(is->insn)) {
        if((arm_reg)is->insn->detail->arm.operands[0].reg != reg_base) {
            printf("sig_match__nrflag: no match ADD/SUB\n");
            return 0;
        }
        if(isADDx_imm(is->insn)) {
            adr+=is->insn->detail->arm.operands[1].imm;
        } else {
            adr-=is->insn->detail->arm.operands[1].imm;
        }
        if(!disasm_iter(fw,is)) {
            printf("sig_match__nrflag: disasm failed\n");
            return 0;
        }
    }
    if(is->insn->id != ARM_INS_STR || (arm_reg)is->insn->detail->arm.operands[1].reg != reg_base) {
        printf("sig_match__nrflag: no match STR\n");
        return 0;
    }
    uint32_t disp = is->insn->detail->arm.operands[1].mem.disp;
    save_misc_val(rule->name,adr,disp,fadr);
    return 1;
}
// get the address used by a function that does something like
// ldr rx =base
// ldr r0 [rx, offset]
// bx lr
int sig_match_var_struct_get(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t fadr=is->adr;
    var_ldr_desc_t desc;
    if(!find_and_get_var_ldr(fw, is, 1, 4, ARM_REG_R0, &desc)) {
        printf("sig_match_var_struct_get: no match ldr\n");
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_var_struct_get: disasm failed\n");
        return 0;
    }
    // TODO could check for other RET type instructions
    if(!insn_match(is->insn,match_bxlr)) {
        printf("sig_match_var_struct_get: no match BX LR\n");
        return 0;
    }
    save_misc_val(rule->name,desc.adr_adj,desc.off,fadr);
    return 1;
}

// get the address used by a function that does
// ldr r1 =base
// ldr r1 [rx, offset]
// bx r1
int sig_match_ui_mem_func_ptr(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t fadr=is->adr;
    var_ldr_desc_t desc;
    if(!find_and_get_var_ldr(fw, is, 1, 4, ARM_REG_R1, &desc)) {
        printf("sig_match_var_struct_get: no match ldr\n");
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_var_struct_get: disasm failed\n");
        return 0;
    }
    const insn_match_t match_bx_r1[]={
        {MATCH_INS(BX,   1),  {MATCH_OP_REG(R1),}},
        {ARM_INS_ENDING}
    };

    // TODO could check for other RET type instructions
    if(!insn_match(is->insn,match_bx_r1)) {
        printf("sig_match_var_struct_get: no match BX R1\n");
        return 0;
    }
    save_misc_val(rule->name,desc.adr_adj,desc.off,fadr);
    return 1;
}

// get the sig from the memory addressed by specified stubs_misc
int sig_match_func_ptr_val(firmware *fw, __attribute__ ((unused))iter_state_t *is, sig_rule_t *rule)
{
    uint32_t adr = get_misc_val_value(rule->ref_name);
    if(!adr) {
        return 0;
    }
    uint32_t *vp = (uint32_t *)adr2ptr_with_data(fw,adr);
    if(!vp) {
        return 0;
    }
    return save_sig_with_j(fw,rule->name,*vp);
}


int sig_match_av_over_sem(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    // don't bother on ND-only cams
    if(!get_misc_val_value("CAM_HAS_IRIS_DIAPHRAGM")) {
        return 0;
    }

    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,30,"TakeSemaphore")) {
        printf("sig_match_av_over_sem: no match TakeSemaphore at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    // rewind 5 ins
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,5));
    var_ldr_desc_t desc;
    if(!find_and_get_var_ldr(fw, is, 3, 4, ARM_REG_R0, &desc)) {
        printf("sig_match_av_over_sem: no match ldr at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    save_misc_val(rule->name,desc.adr_adj,desc.off,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_canon_menu_active(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    var_ldr_desc_t desc;
    if(!find_and_get_var_ldr(fw, is, 2, 4, ARM_REG_R0, &desc)) {
        printf("sig_match_canon_menu_active: no match ldr at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_canon_menu_active: disasm failed\n");
        return 0;
    }
    if(is->insn->id != ARM_INS_CMP) {
        printf("sig_match_canon_menu_active: no match cmp at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    save_misc_val(rule->name,desc.adr_adj,desc.off,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_file_counter_init(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // find first call
    if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
        // printf("sig_match_file_counter_init: bl match 1 failed at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // some cameras (dry 58+?) have a nullsub before the function of interest
    if(check_simple_func(fw,get_branch_call_insn_target(fw,is),MATCH_SIMPLE_FUNC_NULLSUB,NULL)) {
        if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
            // printf("sig_match_file_counter_init: bl match 1a failed at 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_next(fw,is,3,match_bl_blximm)) {
        // printf("sig_match_file_counter_init: bl match 2 failed at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    uint32_t fadr = get_branch_call_insn_target(fw,is);
    // follow
    disasm_iter_init(fw,is,fadr);
    if(!disasm_iter(fw,is)) {
        // printf("sig_match_file_counter_init: disasm failed\n");
        return 0;
    }
    // sanity check
    if(!isLDR_PC(is->insn)) {
        // printf("sig_match_file_counter_init: no match LDR PC at 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // function we're looking for
    return save_sig_with_j(fw,rule->name,fadr);
}
int sig_match_file_counter_var(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t adr=LDR_PC2val(fw,is->insn);
    if(!adr) {
        // printf("sig_match_file_counter_var: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    if(is->insn->detail->arm.operands[0].reg != ARM_REG_R0) {
        // printf("sig_match_file_counter_var: not R0 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    if(!adr_is_var(fw,adr)) {
        // printf("sig_match_file_counter_var: not a data address 0x%08x at 0x%"PRIx64"\n",adr,is->insn->address);
        return 0;
    }
    save_misc_val(rule->name,adr,0,(uint32_t)is->insn->address);
    return 1;
}

int sig_match_palette_vars(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,70,"transfer_src_overlay")) {
        printf("sig_match_palette_vars: no match transfer_src_overlay\n");
        return 0;
    }
    uint32_t fadr=0;
    int i;
    // search backwards for call before transfer_src_overlay
    for(i=1; i<=6; i++) {
        if(!fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i))) {
            printf("sig_match_palette_vars: disasm failed\n");
            return 0;
        }
        fadr=get_branch_call_insn_target(fw,fw->is);
        if(fadr) {
            break;
        }
   }
   if(!fadr) {
        printf("sig_match_palette_vars: no match bl 1 0x%"PRIx64"\n",fw->is->insn->address);
        return 0;
   }
    // follow
    disasm_iter_init(fw,is,fadr);
    // find first func call
    if(!insn_match_find_next(fw,is,3,match_bl)) {
        printf("sig_match_palette_vars: no match bl 2 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));

    if(!insn_match_find_next(fw,is,3,match_ldr_pc)) {
        printf("sig_match_palette_vars: no match ldr pc 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    uint32_t pal_base=LDR_PC2val(fw,is->insn);
    if(!pal_base || !adr_is_var(fw,pal_base)) {
        printf("sig_match_palette_vars: bad LDR PC 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // palette_control is at the start of struct, save register
    arm_reg ptr_reg = is->insn->detail->arm.operands[0].reg;

    save_misc_val(rule->name,pal_base,0,(uint32_t)is->insn->address);

    int found=0;
    // find LDR Rn  [ptr_reg +x]
    for(i=0; i<3; i++) {
        if(!disasm_iter(fw,is)) {
            printf("sig_match_palette_vars: disasm failed\n");
            return 0;
        }
        if (is->insn->id == ARM_INS_LDR && is->insn->detail->arm.operands[1].mem.base == ptr_reg) {
            save_misc_val("active_palette_buffer",
                        pal_base,
                        is->insn->detail->arm.operands[1].mem.disp,
                        (uint32_t)is->insn->address);
            found=1;
            break;
        }
    }
    if(!found) {
        printf("sig_match_palette_vars: no match active_palette_buffer 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    if(!find_next_sig_call(fw,is,20,"PTM_RestoreUIProperty_FW")) {
        printf("sig_match_palette_vars: no match PTM_RestoreUIProperty_FW\n");
        return 0;
    }
    // find LDR Rn  [ptr_reg +x]
    for(i=0; i<6; i++) {
        if(!disasm_iter(fw,is)) {
            printf("sig_match_palette_vars: disasm failed\n");
            return 0;
        }
        if (is->insn->id == ARM_INS_LDR && is->insn->detail->arm.operands[1].mem.base == ptr_reg) {
            save_misc_val("palette_buffer_ptr",
                        pal_base,
                        is->insn->detail->arm.operands[1].mem.disp,
                        (uint32_t)is->insn->address);
            return 1;
        }
    }
    printf("sig_match_palette_vars: no match palette_buffer_ptr 0x%"PRIx64"\n",is->insn->address);
    return 0;
}

int sig_match_live_free_cluster_count(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }

    // find third function call
    if(!insn_match_find_nth(fw,is,22,3,match_bl_blximm)) {
        printf("sig_match_live_free_cluster_count: no match bl1 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));

    if(!find_next_sig_call(fw,is,20,"get_fstype")) {
        printf("sig_match_live_free_cluster_count: no get_fstype 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    // find second function call
    if(!insn_match_find_nth(fw,is,12,2,match_bl_blximm)) {
        printf("sig_match_live_free_cluster_count: no match bl2 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));

    // find second LDR [pc ..]
    if(!insn_match_find_next(fw,is,3,match_ldr_pc)) {
        printf("sig_match_live_free_cluster_count: no match ldr1 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }

    if(!insn_match_find_next(fw,is,3,match_ldr_pc)) {
        printf("sig_match_live_free_cluster_count: no match ldr2 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    uint32_t base = LDR_PC2val(fw,is->insn);

    if(!find_next_sig_call(fw,is,16,"takesemaphore_low")) {
        printf("sig_match_live_free_cluster_count: no takesemaphore_low 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    const insn_match_t match_ldr_ldrd[]={
        {MATCH_INS(LDR,   2),  {MATCH_OP_REG_ANY, MATCH_OP_ANY}},
        {MATCH_INS(LDRD,   3),  {MATCH_OP_REG_ANY, MATCH_OP_REG_ANY, MATCH_OP_ANY}},
        {ARM_INS_ENDING}
    };

    if(!insn_match_find_next_seq(fw,is,50,match_ldr_ldrd)) {
        printf("sig_match_live_free_cluster_count: no match ldrd 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    // +4 because var is 2nd word of ldrd load
    save_misc_val(rule->name,base,is->insn->detail->arm.operands[2].mem.disp + 4,(uint32_t)is->insn->address);
    return 1;

}

int sig_match_init_sd_io_funcs(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t call_adr = find_call_near_str(fw, is, rule);

    if (call_adr == 0) {
        printf("sig_match_init_sd_io_funcs: no match call 1\n");
        return 0;
    }

    // initialize to call address
    disasm_iter_init(fw,is,call_adr);
    disasm_iter(fw,is);
    // follow
    disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
    if(!insn_match_find_nth(fw,is,14,2,match_bl_blximm)) {
        printf("sig_match_init_sd_io_funcs: no match call 2\n");
        return 0;
    }
    return save_sig_with_j(fw,rule->name,get_branch_call_insn_target(fw,is));
}

#define SIG_SD_IO_FUNC_OFF_MASK 0xff
#define SIG_SD_IO_FUNC_OFF(param) ((param)&SIG_SD_IO_FUNC_OFF_MASK)
#define SIG_SD_IO_FUNC_STR   0x10000
// strd, first reg
#define SIG_SD_IO_FUNC_STRD1 0x20000


int sig_match_sd_io_func(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t offset = SIG_SD_IO_FUNC_OFF(rule->param);
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t sadr = is->adr;
    const insn_match_t write_fn_str_match[]={
        {MATCH_INS(STR,     2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM(INVALID,INVALID,offset)}},
        {ARM_INS_ENDING}
    };
    const insn_match_t write_fn_strd_match[]={
        {MATCH_INS(STRD,     3), {MATCH_OP_REG_ANY, MATCH_OP_REG_ANY, MATCH_OP_MEM(INVALID,INVALID,offset)}},
        {ARM_INS_ENDING}
    };
    arm_reg fn_reg = ARM_REG_INVALID;
    // search for store to function offset
    if(rule->param & SIG_SD_IO_FUNC_STR) {
        if(insn_match_find_next(fw,is,46,write_fn_str_match)) {
            fn_reg = (arm_reg)is->insn->detail->arm.operands[0].reg;
        }
    }
    if(fn_reg == ARM_REG_INVALID && rule->param & SIG_SD_IO_FUNC_STRD1) {
        disasm_iter_init(fw,is,sadr | fw->thumb_default);
        if(insn_match_find_next(fw,is,46,write_fn_strd_match)) {
            fn_reg = (arm_reg)is->insn->detail->arm.operands[0].reg;
        }
    }
    if(fn_reg == ARM_REG_INVALID) {
        printf("sig_match_sd_io_func: %s no match STR(D) 0x%x\n",rule->name,sadr);
        return 0;
    }
    // backtrack until we find ldr into reg
    int i;
    for(i=1; i<5; i++) {
        fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
        cs_insn *insn=fw->is->insn;
        // currently assumed to be first reg, strd could be second but isn't in known firmware
        if(insn->id != ARM_INS_LDR || (arm_reg)insn->detail->arm.operands[0].reg != fn_reg) {
            continue;
        }
        if(insn->detail->arm.operands[1].mem.base != ARM_REG_PC) {
            printf("sig_match_sd_io_func: %s expected LDR PC 0x%"PRIx64"\n",rule->name,is->insn->address);
            return 0;
        }
        // printf("WriteSDCard %x 0x%"PRIx64"\n",LDR_PC2val(fw,insn),insn->address);
        return save_sig_with_j(fw,rule->name,LDR_PC2val(fw,insn));
    }
    printf("sig_match_sd_io_func: no match LDR\n");
    return 0;
}

int sig_match_debug_logging_ptr(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t call_adr = find_str_arg_call(fw,is,rule);
    if(!call_adr) {
        printf("sig_match_debug_logging_ptr: no match call\n");
        return 0;
    }
    // is should be pointing at bx instruction, get the register being called
    arm_reg call_reg = is->insn->detail->arm.operands[0].reg;

    // backtrack until we find ldr into reg
    int i;
    for(i=1; i<10; i++) {
        fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
        cs_insn *insn=fw->is->insn;
        if((arm_reg)insn->detail->arm.operands[0].reg != call_reg || insn->id == ARM_INS_CMP ) {
            continue;
        }
        // LDR into target reg
        if(insn->id == ARM_INS_LDR && insn->detail->arm.operands[1].type == ARM_OP_MEM) {
            arm_reg base_reg = (arm_reg)insn->detail->arm.operands[1].reg;
            int disp = insn->detail->arm.operands[1].mem.disp;
            i++;
            // backtrack one more (assume no gap)
            fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
            uint32_t adr = LDR_PC2val(fw,fw->is->insn);
            if(!adr || (arm_reg)fw->is->insn->detail->arm.operands[0].reg != base_reg) {
                printf("sig_match_debug_logging_ptr: no match ldr2 0x%x 0x%"PRIx64"\n",adr,fw->is->insn->address);
                return 0;
            }
            save_misc_val(rule->name,adr + disp,disp,(uint32_t)fw->is->insn->address);
            return 1;
        }
        printf("sig_match_debug_logging_ptr: reg clobbered 0x%"PRIx64"\n",fw->is->insn->address);
        return 0;
    }
    printf("sig_match_debug_logging_ptr: no match ldr 0x%"PRIx64"\n",fw->is->insn->address);
    return 0;
}

int sig_match_debug_logging_flag(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!find_str_arg_call(fw,is,rule)) {
        printf("sig_match_debug_logging_flag: no match call\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,8,match_ldr_pc)) {
        printf("sig_match_debug_logging_flag: no match ldr pc 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    uint32_t adr = LDR_PC2val(fw,is->insn);
    if(!disasm_iter(fw,is)) {
        printf("sig_match_debug_logging_flag: disasm failed\n");
        return 0;
    }
    arm_reg base_reg = (arm_reg)is->insn->detail->arm.operands[1].reg;
    uint32_t ref_adr = (uint32_t)is->insn->address;
    if (fw->arch_flags & FW_ARCH_FL_VMSA) {
        if(is->insn->id != ARM_INS_LDRB) {
            printf("sig_match_debug_logging_flag: no match ldrb 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
    } else {
        if(is->insn->id != ARM_INS_LDR) {
            printf("sig_match_debug_logging_flag: no match ldr 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
    }
    if((arm_reg)is->insn->detail->arm.operands[1].reg != base_reg) {
        printf("sig_match_debug_logging_flag: no match reg 0x%"PRIx64"\n",is->insn->address);
        return 0;
    }
    int disp = (arm_reg)is->insn->detail->arm.operands[1].mem.disp;
    if(!disasm_iter(fw,is)) {
        printf("sig_match_debug_logging_flag: disasm failed\n");
        return 0;
    }
    if(is->insn->id != ARM_INS_LSL) {
        printf("sig_match_debug_logging_flag: no match lsl\n");
        return 0;
    }
    save_misc_val(rule->name,adr + disp,disp,ref_adr);
    return 1;
}
int sig_match_mzrm_sendmsg_ret_adr(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!find_str_arg_call(fw,is,rule)) {
        printf("sig_match_mzrm_sendmsg_ret_adr: no match call\n");
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_mzrm_sendmsg_ret_adr: disasm failed\n");
        return 0;
    }
    // address after blx, thumb bit set for current state
    save_misc_val(rule->name,(uint32_t)is->insn->address | is->thumb,0,0);
    return 1;
}

int sig_match_fw_yuv_layer_buf_52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    // code may have an unconditional branch which find_next_sig_call thinks is a veneer
    if(!find_next_sig_call_ex(fw,is,54,"get_displaytype",FIND_SIG_CALL_NO_UNK_VENEER)) {
        printf("sig_match_fw_yuv_layer_buf_52: no match get_displaytype\n");
        return 0;
    }
//     printf("match get_displaytype 0x%"PRIx64"\n",is->insn->address);
    if(!insn_match_find_nth(fw,is,14,2,match_bl_blximm)) {
        printf("sig_match_fw_yuv_layer_buf_52: no match call\n");
        return 0;
    }
//     printf("match 0x%"PRIx64"\n",is->insn->address);
    uint32_t regs[4];
    // get r1, backtracking up to 8 instructions
    if ((get_call_const_args(fw,is,8,regs)&2)!=2) {
        printf("sig_match_fw_yuv_layer_buf_52: no match const arg\n");
        return 0;
    }
    save_misc_val(rule->name,regs[1],0,(uint32_t)fw->is->insn->address); // fw is has backtracked address
    return 0;
}

int sig_match_fw_yuv_layer_buf_gt52(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    if(!find_next_sig_call(fw,is,170,"DebugAssert")) {
        printf("sig_match_fw_yuv_layer_buf: no match DebugAssert\n");
        return 0;
    }
    if(!insn_match_find_next(fw,is,12,match_bl_blximm)) {
        printf("sig_match_fw_yuv_layer_buf: no match call\n");
        return 0;
    }
    uint32_t regs[4];
    // get r1, backtracking up to 8 instructions
    if ((get_call_const_args(fw,is,8,regs)&2)!=2) {
        printf("sig_match_fw_yuv_layer_buf: no match const arg\n");
        return 0;
    }
    save_misc_val(rule->name,regs[1],0,(uint32_t)fw->is->insn->address); // fw is has backtracked address
    return 0;
}

int sig_match_rom_ptr_get(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if(!init_disasm_sig_ref(fw,is,rule)) {
        return 0;
    }
    uint32_t fadr=is->adr;
    if(!disasm_iter(fw,is)) {
        printf("sig_match_rom_ptr_get: disasm failed\n");
        return 0;
    }
    uint32_t adr=LDR_PC2val(fw,is->insn);
    if(!adr) {
        printf("sig_match_rom_ptr_get: no match LDR PC 0x%"PRIx64"\n",is->insn->address);
        return  0;
    }
    if(is->insn->detail->arm.operands[0].reg != ARM_REG_R0) {
        printf("sig_match_rom_ptr_get: not R0\n");
        return 0;
    }
    if(!disasm_iter(fw,is)) {
        printf("sig_match_rom_ptr_get: disasm failed\n");
        return 0;
    }
    // TODO could check for other RET type instructions
    if(!insn_match(is->insn,match_bxlr)) {
        printf("sig_match_rom_ptr_get: no match BX LR\n");
        return 0;
    }
    save_misc_val(rule->name,adr,0,fadr);
    return 1;
}

// find Nth function call within max_insns ins of string ref,
// returns address w/thumb bit set according to current state of call instruction
// modifies is and potentially fw->is
uint32_t find_call_near_str(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t search_flags = SIG_SEARCH_GET_F(rule->param);

    uint32_t str_adr;
    if(rule->param & SIG_NEAR_INDIRECT) {
        str_adr = find_str_bytes(fw,rule->ref_name); // indirect string could be in data area
    } else {
        str_adr = find_next_str_bytes_code(fw, rule->ref_name, search_flags, 0); // direct string must be near actual code
    }
    if(!str_adr) {
        printf("find_call_near_str: %s failed to find ref %s\n",rule->name,rule->ref_name);
        return 0;
    }
    uint32_t search_adr = str_adr;
    // looking for ref to ptr to string, not ref to string
    // TODO only looks for first ptr
    if(rule->param & SIG_NEAR_INDIRECT) {
        // printf("find_call_near_str: %s str 0x%08x\n",rule->name,str_adr);
        // TODO should honor search ranges
        search_adr=find_u32_adr_range(fw,str_adr,fw->rom_code_search_min_adr,fw->rom_code_search_max_adr);
        if(!search_adr) {
            printf("find_call_near_str: %s failed to find indirect ref %s\n",rule->name,rule->ref_name);
            return 0;
        }
        // printf("find_call_near_str: %s indirect 0x%08x\n",rule->name,search_adr);
    }
    const insn_match_t *insn_match;
    if(rule->param & SIG_NEAR_JMP_SUB) {
        insn_match = match_b_bl_blximm;
    } else {
        insn_match = match_bl_blximm;
    }

    int max_insns = SIG_NEAR_GET_OFFSET(rule->param);
    int n = SIG_NEAR_GET_COUNT(rule->param);
    // adjust limits to avoid going out of adr range
    adr_range_t *rng = adr_get_range(fw,search_adr);
    if(!rng) {
        printf("find_call_near_str: %s invalid search_adr 0x%08x\n",rule->name,search_adr);
        return 0;
    }
    uint32_t search_start = adr_range_clamp_adr_align4(rng, search_adr - SEARCH_NEAR_REF_RANGE);
    uint32_t search_end = adr_range_clamp_adr_align4(rng, search_adr + SEARCH_NEAR_REF_RANGE);

    // printf("find_call_near_str: %s @ 0x%08x max_insns %d n %d %s\n",rule->name,search_adr,max_insns,n,(rule->param & SIG_NEAR_REV)?"rev":"fwd");
    // TODO should handle multiple instances of string
    disasm_iter_init(fw,is,search_start | fw->thumb_default); // reset to a bit before where the string was found
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,search_end)) {
        uint32_t ref_adr = is->insn->address | is->thumb;
        // bactrack looking for preceding call
        if(rule->param & SIG_NEAR_REV) {
            int i;
            int n_calls=0;
            for(i=1; i<=max_insns; i++) {
                fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
                if(insn_match_any(fw->is->insn,insn_match)) {
                    n_calls++;
                }
                if(n_calls == n) {
                    return iter_state_adr(fw->is);
                }
            }
        } else {
            if(insn_match_find_nth(fw,is,max_insns,n,insn_match)) {
                return iter_state_adr(is);
            }
        }
        // Not found - look for previous branch to ref_adr
        disasm_iter_init(fw, is, (ADR_ALIGN4(ref_adr) - SEARCH_NEAR_REF_RANGE) | fw->thumb_default);
        int i;
        for (i=0; i<50; i+=1) {
            if (insn_match_find_next(fw,is,1,match_b)) {
                uint32_t b_adr=get_branch_call_insn_target(fw,is);
                if (b_adr == ref_adr) {
                    // bactrack looking for preceding call
                    if(rule->param & SIG_NEAR_REV) {
                        int i;
                        int n_calls=0;
                        for(i=1; i<=max_insns; i++) {
                            fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
                            if(insn_match_any(fw->is->insn,insn_match)) {
                                n_calls++;
                            }
                            if(n_calls == n) {
                                return iter_state_adr(fw->is);
                            }
                        }
                    } else {
                        if(insn_match_find_nth(fw,is,max_insns,n,insn_match)) {
                            return iter_state_adr(is);
                        }
                    }
                }
            }
        }
        // Reset - second check not matched
        disasm_iter_init(fw, is, ref_adr);
        disasm_iter(fw, is);
    }
    printf("find_call_near_str: no match %s\n",rule->name);
    return 0;
}

// find Nth function call within max_insns ins of string ref
int sig_match_near_str(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    if (!get_saved_sig_val(rule->name))
    {
        uint32_t call_adr = find_call_near_str(fw,is,rule);
        if(call_adr) {
            return save_sig_match_call(fw, rule, call_adr);
        }
    }
    return 0;
}

// find call that recieves string sig->ref_name in reg
// returns address w/thumb bit set according to current state of call instruction
// modifies is and potentially fw->is
// arg register and call type set by SIG_STRCALL_ macros
// NOTE types are currently exclusive, not a bitmask
// SIG_SEARCH_* can be used to search specific code regions
// does not currently handle indirect refs
// handles multiple instances of string
uint32_t find_str_arg_call(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    arm_reg reg = SIG_STRCALL_GET_REG(rule->param);
    // TODO this should be a bitmask that builds match list
    int match_type = SIG_STRCALL_GET_TYPE(rule->param);
    const insn_match_t *match;
    if(match_type == SIG_STRCALL_CALL_IMM) {
        match = match_bl_blximm;
    } else if(match_type == SIG_STRCALL_JMP_IMM) {
        match = match_b;
    } else if(match_type == SIG_STRCALL_CALL_JMP_IMM) {
        match = match_b_bl_blximm;
    } else if(match_type == SIG_STRCALL_JMP_REG) {
        match = match_bxreg;
    } else if(match_type == SIG_STRCALL_CALL_REG) {
        match = match_blxreg;
    } else {
        printf("find_str_arg_call: %s invalid match type %d\n",rule->name,match_type);
        return 0;
    }

    uint32_t search_flags = SIG_SEARCH_GET_F(rule->param);
    uint32_t str_adr = find_next_str_bytes_code(fw, rule->ref_name, search_flags, 0);
    if(!str_adr) {
        if(!(rule->flags & SIG_OPTIONAL)) {
            printf("find_str_arg_call: %s failed to find ref %s\n",rule->name,rule->ref_name);
        }
        return  0;
    }

    do {
        // adjust limits to avoid going out of adr range
        adr_range_t *rng = adr_get_range(fw,str_adr);
        if(!rng) {
            printf("find_str_arg_call: %s invalid str_adr 0x%08x\n",rule->name,str_adr);
            return 0;
        }
        uint32_t search_start = adr_range_clamp_adr_align4(rng, str_adr - SEARCH_NEAR_REF_RANGE);
        uint32_t search_end = adr_range_clamp_adr_align4(rng, str_adr + SEARCH_NEAR_REF_RANGE);

        disasm_iter_init(fw,is,search_start | fw->thumb_default); // reset to a bit before where the string was found
        uint32_t call_adr = find_const_ref_match(fw, is, search_end - search_start, 8, reg, str_adr, match, FIND_CONST_REF_MATCH_ANY);
        if(call_adr) {
            return call_adr;
        }
        str_adr = find_next_str_bytes_code(fw, rule->ref_name, search_flags, str_adr + strlen(rule->ref_name));
    } while (str_adr);
    printf("find_str_arg_call: no match %s r%d\n",rule->name,reg-ARM_REG_R0);
    return 0;
}

int sig_match_str_arg_call(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t call_adr = find_str_arg_call(fw,is,rule);
    if(call_adr) {
        return save_sig_match_call(fw, rule, call_adr);
    }
    return 0;
}

int sig_match_prop_string(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t call_adr = find_call_near_str(fw, is, rule);

    if (call_adr == 0)
        return 0;

    // initialize to found address
    disasm_iter_init(fw,is,call_adr);
    disasm_iter(fw,is);

    uint32_t myreg;

    if (is_sig_call(fw,is,"GetPropertyCase")) {
        // looking for r0
        myreg = 0;
    }
    else {
        // semaphore version of GetPropertyCase, looking for r1
        myreg = 1;
    }

    // re-init 'is' to current address minus at least 8 insts
    const int hl = 8;
    disasm_iter_init(fw,is,call_adr - hl*4);
    // history needs to be made
    while (is->adr < call_adr) {
        if (!disasm_iter(fw,is))
            disasm_iter_init(fw,is,(is->adr | is->thumb)+2);
    }
    uint32_t regs[4];
    // get r0 or r1, backtracking up to 8 instructions
    if ((get_call_const_args(fw,is,hl,regs)&(1<<myreg))==(1<<myreg)) {
        add_prop_hit(rule->name,(int)regs[myreg]);
        return 1;
    }
    return 0;
}

// check if func is a nullsub or mov r0, x ; ret
// to prevent sig_named* matches from going off the end of dummy funcs
int is_immediate_ret_sub(firmware *fw,iter_state_t *is_init)
{
    fw_disasm_iter_single(fw,is_init->adr | is_init->thumb);
    const insn_match_t match_mov_r0_imm[]={
        {MATCH_INS(MOV,   2),  {MATCH_OP_REG(R0),  MATCH_OP_IMM_ANY}},
#if CS_API_MAJOR < 4
        {MATCH_INS(MOVS,  2),  {MATCH_OP_REG(R0),  MATCH_OP_IMM_ANY}},
#endif
        {ARM_INS_ENDING}
    };
    // if it's a MOV, check if next is ret
    if(insn_match_any(fw->is->insn,match_mov_r0_imm)) {
        fw_disasm_iter(fw);
    }
    if(isRETx(fw->is->insn)) {
        return 1;
    }
    return 0;
}

// match last function called by already matched sig,
// either the last bl/blximmg before pop {... pc}
// or b after pop {... lr}
// param defines min and max number of insns
// doesn't work on functions that don't push/pop since can't tell if unconditional branch is last
#define SIG_NAMED_LAST_MAX_MASK     0x00000FFF
#define SIG_NAMED_LAST_MIN_MASK     0x00FFF000
#define SIG_NAMED_LAST_MIN_SHIFT    12
#define SIG_NAMED_LAST_RANGE(min,max)   ((SIG_NAMED_LAST_MIN_MASK&((min)<<SIG_NAMED_LAST_MIN_SHIFT)) \
                                         | (SIG_NAMED_LAST_MAX_MASK&(max)))

int sig_match_named_last(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t ref_adr = get_saved_sig_val(rule->ref_name);
    int min = (rule->param&SIG_NAMED_LAST_MIN_MASK)>>SIG_NAMED_LAST_MIN_SHIFT;
    int max = (rule->param&SIG_NAMED_LAST_MAX_MASK);
    if(!ref_adr) {
        printf("sig_match_named_last: %s missing %s\n",rule->name,rule->ref_name);
        return 0;
    }
    disasm_iter_init(fw,is,ref_adr);
    if(is_immediate_ret_sub(fw,is)) {
        printf("sig_match_named_last: immediate return %s\n",rule->name);
        return 0;
    }
    uint32_t fadr = find_last_call_from_func(fw,is,min,max);
    if(fadr) {
        return save_sig_with_j(fw,rule->name,fadr);
    }
    return 0;
}

// default - use the named firmware function
#define SIG_NAMED_ASIS          0x00000000
// use the target of the first B, BX, BL, BLX etc
#define SIG_NAMED_JMP_SUB       0x00000001
// use the target of the first BL, BLX
#define SIG_NAMED_SUB           0x00000002
// match address of Nth instruction in named sub
#define SIG_NAMED_INSN          0x00000003
#define SIG_NAMED_TYPE_MASK     0x0000000F

#define SIG_NAMED_CLEARTHUMB    0x00000010
#define SIG_NAMED_FLAG_MASK     0x000000F0

#define SIG_NAMED_NTH_MASK      0x00000F00
#define SIG_NAMED_NTH_SHIFT     8

// number of instructions to search for each Nth
// default 5
#define SIG_NAMED_NTH_RANGE_MASK  0x0003F000
#define SIG_NAMED_NTH_RANGE_SHIFT 12

//#define SIG_NAMED_NTH(n,type)   ((SIG_NAMED_NTH_MASK&((n)<<SIG_NAMED_NTH_SHIFT)) | ((SIG_NAMED_##type)&SIG_NAME_TYPE_MASK))
#define SIG_NAMED_NTH(n,type)   ((SIG_NAMED_NTH_MASK&((n)<<SIG_NAMED_NTH_SHIFT)) | (SIG_NAMED_##type))

#define SIG_NAMED_NTH_RANGE(n)   ((SIG_NAMED_NTH_RANGE_MASK&((n)<<SIG_NAMED_NTH_RANGE_SHIFT)))

int sig_match_named_save_sig(firmware *fw,const char *name, uint32_t adr, uint32_t flags)
{
    adr = save_sig_veneers(fw, name, adr);
    if(adr) {
        if(flags & SIG_NAMED_CLEARTHUMB) {
            adr = ADR_CLEAR_THUMB(adr);
        }
        save_sig(fw,name,adr);
        return 1;
    }
    return 0;
}

// match already identified function found by name
// if offset is 1, match the first called function with 20 insn instead (e.g. to avoid eventproc arg handling)
uint32_t sig_match_named_find(firmware *fw, iter_state_t *is, sig_rule_t *rule, uint32_t ref_adr)
{
    if(!ref_adr) {
        if (!(rule->flags & SIG_OPTIONAL))
            printf("sig_match_named_find: missing %s\n",rule->ref_name);
        return 0;
    }
    uint32_t sig_type = rule->param & SIG_NAMED_TYPE_MASK;
    uint32_t sig_nth = (rule->param & SIG_NAMED_NTH_MASK)>>SIG_NAMED_NTH_SHIFT;
    uint32_t sig_nth_range = (rule->param & SIG_NAMED_NTH_RANGE_MASK)>>SIG_NAMED_NTH_RANGE_SHIFT;
    if(!sig_nth) {
        sig_nth=1;
    }
    if(!sig_nth_range) {
        sig_nth_range=5;
    }
    // no offset, just save match as is
    // TODO might want to validate anyway
    if(sig_type == SIG_NAMED_ASIS) {
        return ref_adr;
    }
    const insn_match_t *insn_match;
    if(sig_type == SIG_NAMED_JMP_SUB) {
        insn_match = match_b_bl_blximm;
    } else if(sig_type == SIG_NAMED_SUB) {
        insn_match = match_bl_blximm;
    } else if(sig_type == SIG_NAMED_INSN) {
        insn_match = NULL;
    } else {
        printf("sig_match_named: %s invalid type %d\n",rule->ref_name,sig_type);
        return 0;
    }

    disasm_iter_init(fw,is,ref_adr);
    // TODO for eventprocs, may just want to use the original
    if(is_immediate_ret_sub(fw,is)) {
        printf("sig_match_named: immediate return %s\n",rule->name);
        return 0;
    }
    if(sig_type == SIG_NAMED_INSN) {
        uint32_t i;
        // iter starts on the address given to init
        for(i=0;i<=sig_nth;i++) {
            if(!disasm_iter(fw,is)) {
                printf("sig_match_named: disasm failed %s 0x%08x\n",rule->name,(uint32_t)is->insn->address);
                return 0;
            }
        }
        return iter_state_adr(is);
    }

    // initial 15 is hard coded
    if(insn_match_find_nth(fw,is,15 + sig_nth_range*sig_nth,sig_nth,insn_match)) {
        uint32_t adr = B_BL_BLXimm_target(fw,is->insn);
        if(adr) {
            // BLX, set thumb bit
            if(is->insn->id == ARM_INS_BLX) {
                // curently not thumb, set in target
                if(!is->thumb) {
                    adr=ADR_SET_THUMB(adr);
                }
            } else {
                // preserve current state
                adr |= is->thumb;
            }
            return adr;
        } else {
            printf("sig_match_named: %s invalid branch target 0x%08x\n",rule->ref_name,adr);
        }
    } else if (!(rule->flags & SIG_OPTIONAL)) {
        printf("sig_match_named: %s branch not found 0x%08x\n",rule->ref_name,ref_adr);
    }
    return 0;
}

// match already identified function found by name
// if offset is 1, match the first called function with 20 insn instead (e.g. to avoid eventproc arg handling)
// veneers are added
int sig_match_named(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t ref_adr = get_saved_sig_val(rule->ref_name);
    uint32_t adr = sig_match_named_find(fw, is, rule, ref_adr);
    if (adr) {
        return sig_match_named_save_sig(fw,rule->name,adr,rule->param & SIG_NAMED_FLAG_MASK);
    }
    return 0;
}

int sig_seq[][2] = {
    { SIG_NAMED_SUB, SIG_NAMED_NTH(3,JMP_SUB) },        //MakeSDCardBootable
    { SIG_NAMED_SUB, SIG_NAMED_SUB },                   //GetFocusLensSubjectDistanceFromLens
    { SIG_NAMED_JMP_SUB, SIG_NAMED_NTH(3,JMP_SUB) },    //apex2us
};

// Follow sig_seq entries starting from saved sig val. Rule param is index into sig_seq array
int sig_match_named_seq(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t ref_adr = get_saved_sig_val(rule->ref_name);
    sig_rule_t trule;
    memcpy(&trule, rule, sizeof(sig_rule_t));
    trule.param = sig_seq[rule->param][0];
    // Follow first sig_seq entry
    uint32_t adr = sig_match_named_find(fw, is, &trule, ref_adr);
    if (adr) {
        // Check in case first link is to a veneer direct jump
        fw_disasm_iter_single(fw, adr);
        uint32_t b_adr = get_direct_jump_target(fw, fw->is);
        if (b_adr) adr = b_adr;
        trule.param = sig_seq[rule->param][1];
        // If 1st is valid, follow second entry
        adr = sig_match_named_find(fw, is, &trule, adr);
        if (adr) {
            return sig_match_named_save_sig(fw,rule->name,adr,rule->param & SIG_NAMED_FLAG_MASK);
        }
    }
    return 0;
}

// match already identified function found by name, select next func as result
int sig_match_named_next_func(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t adr = get_saved_sig_val(rule->ref_name);
    if(!adr) {
        printf("sig_match_named_next_func: missing %s\n",rule->ref_name);
        return 0;
    }
    adr += 32;
    int i;
    fw_disasm_iter_single(fw,adr);
    for (i=0; i<20; i+=1) {
        if (fw->is->insn->id == ARM_INS_PUSH && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_R4) {
            return save_sig_with_j(fw,rule->name,(fw->is->insn->address) | is->thumb);
        }
        fw_disasm_iter(fw);
    }

    return 0;
}

// macros for sig_match_func_using_str
// backtrack instruction count - matches ADR_HIST_SIZE 64
#define SIG_USESTR_BACK_BITS 6
#define SIG_USESTR_BACK_MASK ((1 << SIG_USESTR_BACK_BITS) - 1)
#define SIG_USESTR_FWD_SHIFT 6
// more than 7 instructions before push should be very rare
#define SIG_USESTR_FWD_BITS 3
#define SIG_USESTR_FWD_MASK (((1 << SIG_USESTR_FWD_BITS) - 1) << SIG_USESTR_FWD_SHIFT)

// macros for rule definitions
#define SIG_USESTR_BACK(x) ((x) & SIG_USESTR_BACK_MASK)
#define SIG_USESTR_FWD(x) (((x) << SIG_USESTR_FWD_SHIFT) & SIG_USESTR_FWD_MASK)
#define SIG_USESTR(back_insns,fwd_insns) (SIG_USESTR_BACK(back_insns)|SIG_USESTR_FWD(fwd_insns))
// bits starting at SIG_SEARCH_SHIFT used for memory range selection with SIG_SEARCH_RAM etc

// Match function using string, use SIG_USESTR* macros to define forward/backward limits
// and SIG_SEARCH_* to specify memory ranges
// backtracking assumes function begins with push starting with R4
// forward assumes string ref is first instruction of function
// Either may not be true!
int sig_match_func_using_str(firmware *fw, iter_state_t *is, sig_rule_t *rule)
{
    uint32_t str_adr = find_next_str_bytes_code(fw, rule->ref_name, SIG_SEARCH_GET_F(rule->param), 0);
    if(!str_adr) {
        if(!(rule->flags & SIG_OPTIONAL)) {
            printf("sig_match_func_using_str: %s failed to find ref %s\n",rule->name,rule->ref_name);
        }
        return  0;
    }

    // TODO may want flags to allow others
    const insn_match_t match_push_r4[]={
        {MATCH_INS(PUSH,   MATCH_OPCOUNT_ANY),  {MATCH_OP_REG(R4)}},
        {ARM_INS_ENDING}
    };
    uint32_t back_count = rule->param & SIG_USESTR_BACK_MASK;
    uint32_t fwd_count = (rule->param & SIG_USESTR_FWD_MASK) >> SIG_USESTR_FWD_SHIFT;

    // adjust limits to avoid going out of adr range
    adr_range_t *rng = adr_get_range(fw,str_adr);
    if(!rng) {
        printf("sig_match_func_using_str: %s invalid str_adr 0x%08x\n",rule->name,str_adr);
        return 0;
    }
    uint32_t search_start = adr_range_clamp_adr_align4(rng, str_adr - SEARCH_NEAR_REF_RANGE);
    uint32_t search_end = adr_range_clamp_adr_align4(rng, str_adr + SEARCH_NEAR_REF_RANGE);

    disasm_iter_init(fw,is,search_start | fw->thumb_default); // reset to a bit before where the string was found
    // Find references to string
    while(fw_search_insn(fw,is,search_disasm_const_ref,str_adr,NULL,search_end)) {
        // string reference address
        uint32_t ref_adr = is->insn->address | is->thumb;

        // search forward, for patterns like ldr rn,#str ... push ...
        uint32_t i;
        if(fwd_count) {
            fw_disasm_iter_single(fw,ref_adr);
            for(i=0; i<fwd_count; i++) {
                fw_disasm_iter(fw);
                // printf("%"PRIx64" sig_match_func_using_str: %s fwd %d %s %s\n",fw->is->insn->address, rule->name,i,fw->is->insn->mnemonic,fw->is->insn->op_str);

                // TODO should have match / break on instruction unlikely here
                if(insn_match(fw->is->insn,match_push_r4)) {
                    // printf("sig_match_func_using_str: forward match %s\n",rule->name);
                    // assume function starts with string ref
                    // NOTE might not be true, could load other consts first!
                    return save_sig_with_j(fw,rule->name, ref_adr);
                }
            }
        }
        if(back_count) {
            // printf("%"PRIx64" sig_match_func_using_str: %s back %d\n",is->insn->address,rule->name,back_count);
            // TODO functions may start with ldr, mov etc before push (e.g. ExpCtrlTool_StopContiAE on some d7)
            if(insn_match_find_hist_nth(fw,is,back_count,1,match_push_r4)) {
                // printf("sig_match_func_using_str: back match %s\n",rule->name);
                return save_sig_with_j(fw,rule->name,(fw->is->insn->address) | fw->is->thumb);
            }
        }
    }
    return 0;
}

// bootstrap sigs:
// Used to find the minimum needed to for find_generic_funcs to get generic task and eventproc matches
// order is important
sig_rule_t sig_rules_initial[]={
// function         CHDK name                   ref name/string         func param          dry rel             match flags
// NOTE _FW is in the CHDK column, because that's how it is in sig_names
{sig_match_str_r0_call, "ExportToEventProcedure_FW","ExportToEventProcedure"},
{sig_match_reg_evp,     "RegisterEventProcedure",},
{sig_match_reg_evp_table, "RegisterEventProcTable","DispDev_EnableEventProc"},
{sig_match_reg_evp_alt2, "RegisterEventProcedure_alt2","EngApp.Delete"},
{sig_match_unreg_evp_table,"UnRegisterEventProcTable","MechaUnRegisterEventProcedure"},
{sig_match_evp_table_veneer,"RegisterEventProcTable_alt","RegisterEventProcTable"},
{sig_match_evp_table_veneer,"UnRegisterEventProcTable_alt","UnRegisterEventProcTable"},
{sig_match_str_r0_call,"CreateTaskStrictly",    "LowConsole",},
{sig_match_str_r0_call,"CreateTask",            "EvShel",},
{sig_match_named,   "CreateTask_low",           "CreateTask",           (SIG_NAMED_NTH(2,SUB)|SIG_NAMED_NTH_RANGE(10)), SIG_DRY_MAX(52)},
{sig_match_named,   "CreateTask_low",           "CreateTask",           (SIG_NAMED_NTH(3,SUB)|SIG_NAMED_NTH_RANGE(10)), SIG_DRY_MIN(54)},
{sig_match_createtaskstrictly_alt,"CreateTaskStrictly_alt","HdmiCecTask",0,                 SIG_DRY_MIN(58)},
{sig_match_createtask_alt,"CreateTask_alt",     "CreateTaskStrictly_alt",0,                 SIG_DRY_MIN(58)},
{sig_match_near_str,   "dry_memcpy",            "EP Slot%d",            SIG_NEAR_BEFORE(4,1)},
{sig_match_add_ptp_handler,"add_ptp_handler",   "PTPtoFAPI_EventProcTask_Try",},
{NULL},
};

// main sigs:
// Run after find_generic_funcs. Order is important, matches must come after any matches they depend on.
// Matches that depend only on bootstrap sigs should be first
sig_rule_t sig_rules_main[]={
// function         CHDK name                   ref name/string         func param          dry rel             match flags
// these only depend on tasks, put first to allow checks in later sigs
{sig_match_misc_flag_named,"CAM_IS_ILC",        "task_EFLensComTask",},
{sig_match_misc_flag_named,"CAM_HAS_ND_FILTER", "task_Nd",},
{sig_match_misc_flag_named,"CAM_HAS_WIFI",      "task_ComWireless",},
{sig_match_named,   "SetParameterData",         "PTM_BackupUIProperty_FW", 0,               SIG_DRY_MIN(58)},
{sig_match_named,   "ExitTask",                 "ExitTask_FW",},
{sig_match_named,   "EngDrvRead",               "EngDrvRead_FW",        SIG_NAMED_JMP_SUB, 0, 0, SIG_OPTIONAL },
{sig_match_named,   "CalcLog10",                "CalcLog10_FW",         SIG_NAMED_JMP_SUB},
{sig_match_named,   "CalcSqrt",                 "CalcSqrt_FW",          SIG_NAMED_JMP_SUB},
{sig_match_named,   "Close",                    "Close_FW",},
{sig_match_named,   "Close_low",                "Close",                SIG_NAMED_NTH(3,SUB),SIG_DRY_MIN(58)},
{sig_match_named,   "close",                    "Close",                SIG_NAMED_SUB,      SIG_DRY_MAX(57)},
{sig_match_named,   "close",                    "Close_low",            SIG_NAMED_SUB,      SIG_DRY_MIN(58)},
{sig_match_named,   "DoAELock",                 "SS.DoAELock_FW",       SIG_NAMED_JMP_SUB},
{sig_match_named,   "DoAFLock",                 "SS.DoAFLock_FW",       SIG_NAMED_JMP_SUB},
{sig_match_named,   "Fclose_Fut",               "Fclose_Fut_FW",},
{sig_match_named,   "Fopen_Fut",                "Fopen_Fut_FW",},
{sig_match_named,   "Fread_Fut",                "Fread_Fut_FW",},
{sig_match_named,   "Fseek_Fut",                "Fseek_Fut_FW",},
{sig_match_named,   "Fwrite_Fut",               "Fwrite_Fut_FW",},
{sig_match_named,   "fopen_low",                "Fopen_Fut",            SIG_NAMED_NTH(3,SUB)},
{sig_match_named,   "fut_prepare",              "Fopen_Fut",            SIG_NAMED_NTH(1,SUB)},
{sig_match_named,   "fut_finish",               "Fopen_Fut",            SIG_NAMED_NTH(4,SUB)},
// high level functions not yet matched
//{sig_match_named,   "feof_low",                 "Feof_Fut",             SIG_NAMED_NTH(2,SUB)},
//{sig_match_named,   "fflush_low",               "Fflush_Fut",           SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "fread_low",                "Fread_Fut",            SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "fseek_low",                "Fseek_Fut",            SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "fwrite_low",               "Fwrite_Fut",           SIG_NAMED_NTH(2,SUB)},

{sig_match_named,   "GetAdChValue",             "GetAdChValue_FW",},
{sig_match_named,   "GetCurrentAvValue",        "GetCurrentAvValue_FW",},
{sig_match_named,   "GetCurrentShutterSpeed",   "GetCurrentShutterSpeed_FW",},
{sig_match_named,   "GetBatteryTemperature",    "GetBatteryTemperature_FW",},
{sig_match_named,   "GetCCDTemperature",        "GetCCDTemperature_FW",},
{sig_match_named,   "GetFocusLensSubjectDistance","GetFocusLensSubjectDistance_FW",SIG_NAMED_JMP_SUB},
{sig_match_named,   "EFLensCom_FocusSearchNear","EFLensCom.FocusSearchNear_FW",SIG_NAMED_ASIS,SIG_DRY_ANY,SIG_ILC_ONLY},
{sig_match_named,   "EFLensCom_FocusSearchFar", "EFLensCom.FocusSearchFar_FW",SIG_NAMED_ASIS,SIG_DRY_ANY,SIG_ILC_ONLY},
{sig_match_named,   "GetEFLensFocusPositionWithLensCom","GetEFLensFocusPositionWithLensCom_FW",SIG_NAMED_ASIS,SIG_DRY_ANY,SIG_ILC_ONLY},
{sig_match_named,   "GetLensExtenderTypeValue", "GetLensExtenderTypeValue_FW",SIG_NAMED_SUB,SIG_DRY_ANY,    SIG_ILC_ONLY},
{sig_match_named,   "GetLensTypeValue",         "GetLensTypeValue_FW",  0,                  SIG_DRY_ANY,    SIG_ILC_ONLY},
{sig_match_named,   "GetFALensInfoData",        "GetFALensInfoData_FW", SIG_NAMED_SUB ,     SIG_DRY_ANY,    SIG_ILC_ONLY},
{sig_match_named,   "GetFALensInfoData_vals",   "GetFALensInfoData",    SIG_NAMED_SUB ,     SIG_DRY_ANY,    SIG_ILC_ONLY},
{sig_match_named,   "AllocateEFlensComBuffer",  "EFLensCom.FocusSearchFar_FW",SIG_NAMED_SUB ,SIG_DRY_ANY,   SIG_ILC_ONLY},
{sig_match_named,   "EFLens_Send",              "EFLensCom.FocusSearchFar_FW",SIG_NAMED_NTH(2,SUB),SIG_DRY_ANY,SIG_ILC_ONLY},
{sig_match_named,   "EFLens_Send_low",          "EFLens_Send",(SIG_NAMED_NTH(3,SUB)|SIG_NAMED_NTH_RANGE(10)),SIG_DRY_MAX(55),SIG_ILC_ONLY},
{sig_match_named,   "EFLens_Send_low",          "EFLens_Send",(SIG_NAMED_NTH(4,SUB)|SIG_NAMED_NTH_RANGE(10)),SIG_DRY_RANGE(57,57),SIG_ILC_ONLY},
{sig_match_named,   "EFLens_Send_low",          "EFLens_Send",(SIG_NAMED_NTH(2,SUB)),       SIG_DRY_RANGE(59,59),SIG_ILC_ONLY},
{sig_match_named,   "ReleaseEFlensComBuffer",   "EFLensCom.FocusSearchFar_FW",SIG_NAMED_NTH(3,SUB),SIG_DRY_ANY,SIG_ILC_ONLY},
{sig_match_named_last,"EFLensCom_MoveFocus",    "EFLensCom.MoveFocus_FW",SIG_NAMED_LAST_RANGE(10,18),SIG_DRY_ANY,SIG_ILC_ONLY},

{sig_match_named,   "GetOpticalTemperature",    "GetOpticalTemperature_FW",},
{sig_match_named,   "GetPropertyCase",          "GetPropertyCase_FW",   SIG_NAMED_SUB},
{sig_match_named,   "GetSystemTime",            "GetSystemTime_FW",},
{sig_match_named,   "_GetSystemTime",           "GetSystemTime",        SIG_NAMED_SUB},
// d7 cams have an extra call in _GetSystemTime
{sig_match_named,   "GetSRAndDisableInterrupt", "_GetSystemTime",       SIG_NAMED_SUB,      SIG_DRY_ANY,    SIG_NO_D7},
{sig_match_named,   "GetSRAndDisableInterrupt", "_GetSystemTime",       SIG_NAMED_NTH(2,SUB),SIG_DRY_ANY,   SIG_NO_D6},
{sig_match_named,   "SetSR",                    "_GetSystemTime",       SIG_NAMED_NTH(2,SUB),SIG_DRY_ANY,   SIG_NO_D7},
{sig_match_named,   "SetSR",                    "_GetSystemTime",       SIG_NAMED_NTH(3,SUB),SIG_DRY_ANY,   SIG_NO_D6},
{sig_match_named,   "GetUsableMaxAv",           "GetUsableMaxAv_FW",},
{sig_match_named,   "GetUsableMinAv",           "GetUsableMinAv_FW",},
// a different match would be needed for older, ND only cams maybe based on "AE Result Tv Setting "
{sig_match_named,   "GetUsableAvRange",         "GetUsableMinAv",       SIG_NAMED_SUB},
{sig_match_named,   "GetVRAMHPixelsSize",       "GetVRAMHPixelsSize_FW",},
{sig_match_named,   "GetVRAMVPixelsSize",       "GetVRAMVPixelsSize_FW",},
{sig_match_named,   "GetZoomLensCurrentPoint",  "GetZoomLensCurrentPoint_FW",},
{sig_match_named,   "GetZoomLensCurrentPosition","GetZoomLensCurrentPosition_FW",},
{sig_match_named,   "GiveSemaphore",            "GiveSemaphore_FW",},
{sig_match_named,   "IsStrobeChargeCompleted",  "EF.IsChargeFull_FW",},
{sig_match_named,   "Read",                     "Read_FW",},
{sig_match_named,   "LEDDrive",                 "LEDDrive_FW",},
{sig_match_named,   "LockMainPower",            "LockMainPower_FW",},
{sig_match_named,   "MoveFocusLensToDistance",  "MoveFocusLensToDistance_FW",0,         SIG_DRY_ANY, SIG_NONILC_ONLY},
{sig_match_named,   "MoveIrisWithAv",           "MoveIrisWithAv_FW",},
{sig_match_named,   "MoveZoomLensWithPoint",    "MoveZoomLensWithPoint_FW",},
{sig_match_named,   "Open",                     "Open_FW",},
{sig_match_named,   "Open_low",                 "Open",                SIG_NAMED_NTH(3,SUB),SIG_DRY_MIN(58)},
{sig_match_named,   "PostLogicalEventForNotPowerType",  "PostLogicalEventForNotPowerType_FW",},
{sig_match_named,   "PostLogicalEventToUI",     "PostLogicalEventToUI_FW",},
{sig_match_named,   "PT_MFOn",                  "SS.MFOn_FW",           SIG_NAMED_JMP_SUB},
{sig_match_named,   "PT_MFOff",                 "SS.MFOff_FW",          SIG_NAMED_JMP_SUB},
{sig_match_named,   "PT_MoveDigitalZoomToWide", "SS.MoveDigitalZoomToWide_FW", SIG_NAMED_JMP_SUB},
{sig_match_named,   "PT_MoveOpticalZoomAt",     "SS.MoveOpticalZoomAt_FW",},
{sig_match_named,   "PutInNdFilter",            "PutInNdFilter_FW",     0,              SIG_DRY_ANY, SIG_NONILC_ONLY},
{sig_match_named,   "PutOutNdFilter",           "PutOutNdFilter_FW",    0,              SIG_DRY_ANY, SIG_NONILC_ONLY},
{sig_match_named,   "SetAE_ShutterSpeed",       "SetAE_ShutterSpeed_FW",},
{sig_match_named,   "SetAutoShutdownTime",      "SetAutoShutdownTime_FW",},
{sig_match_named,   "SetCurrentCaptureModeType","SetCurrentCaptureModeType_FW",},
{sig_match_named,   "SetDate",                  "SetDate_FW",},
{sig_match_named,   "SetLogicalEventActive",    "UiEvnt_SetLogicalEventActive_FW",},
{sig_match_named,   "SetScriptMode",            "SetScriptMode_FW",},
{sig_match_named,   "SleepTask",                "SleepTask_FW",},
{sig_match_named,   "SetPropertyCase",          "SetPropertyCase_FW",   SIG_NAMED_SUB},
{sig_match_named,   "TakeSemaphore",            "TakeSemaphore_FW",},
{sig_match_named,   "TurnOnDisplay",            "DispCon_TurnOnDisplay_FW",SIG_NAMED_SUB},
{sig_match_named,   "TurnOffDisplay",           "DispCon_TurnOffDisplay_FW",SIG_NAMED_SUB},
{sig_match_named,   "TurnOnBackLight",          "DispCon_TurnOnBackLight_FW",SIG_NAMED_SUB, SIG_DRY_MAX(57)},
{sig_match_named,   "TurnOffBackLight",         "DispCon_TurnOffBackLight_FW",SIG_NAMED_SUB,SIG_DRY_MAX(57)},
{sig_match_named,   "UIFS_WriteFirmInfoToFile", "UIFS_WriteFirmInfoToFile_FW",},
{sig_match_named,   "UnlockAE",                 "SS.UnlockAE_FW",       SIG_NAMED_JMP_SUB},
{sig_match_named,   "UnlockAF",                 "SS.UnlockAF_FW",       SIG_NAMED_JMP_SUB},
{sig_match_named,   "UnlockMainPower",          "UnlockMainPower_FW",},
{sig_match_named,   "UnRegisterEventProcedure", "UnRegisterEventProcTable", SIG_NAMED_SUB},
//{sig_match_named,   "UnsetZoomForMovie",        "UnsetZoomForMovie_FW",},
{sig_match_named,   "VbattGet",                 "VbattGet_FW",},
{sig_match_named,   "Write",                    "Write_FW",},
{sig_match_named,   "bzero",                    "exec_FW",              SIG_NAMED_SUB},
{sig_match_named,   "exmem_free",               "ExMem.FreeCacheable_FW",SIG_NAMED_JMP_SUB, SIG_DRY_MAX(58)},
{sig_match_named,   "exmem_alloc",              "ExMem.AllocCacheable_FW",SIG_NAMED_JMP_SUB,SIG_DRY_MAX(58)},
{sig_match_named,   "exmem_ufree",              "ExMem.FreeUncacheable_FW",SIG_NAMED_JMP_SUB, SIG_DRY_MAX(58)},
{sig_match_named,   "exmem_ualloc",             "ExMem.AllocUncacheable_FW",SIG_NAMED_JMP_SUB,SIG_DRY_MAX(58)},
{sig_match_named,   "free",                     "FreeMemory_FW",        SIG_NAMED_JMP_SUB},
{sig_match_named,   "heap_free",                "free",                 SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "lseek",                    "Lseek_FW",},
{sig_match_named,   "_log10",                   "CalcLog10",            SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "malloc",                   "AllocateMemory_FW",    SIG_NAMED_JMP_SUB},
{sig_match_named,   "heap_alloc",               "malloc",               SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "memcmp",                   "memcmp_FW",},
{sig_match_named,   "memcpy",                   "memcpy_FW",},
{sig_match_named,   "memset",                   "memset_FW",},
{sig_match_named,   "strcmp",                   "strcmp_FW",},
{sig_match_named,   "strcpy",                   "strcpy_FW",},
{sig_match_named,   "strlen",                   "strlen_FW",},
{sig_match_named,   "task_CaptSeq",             "task_CaptSeqTask",},
{sig_match_named,   "task_ExpDrv",              "task_ExpDrvTask",},
{sig_match_named,   "task_FileWrite",           "task_FileWriteTask", 0, 0, 0, SIG_OPTIONAL },
//{sig_match_named,   "task_MovieRecord",         "task_MovieRecord",},
//{sig_match_named,   "task_PhySw",               "task_PhySw",},
{sig_match_named,   "vsprintf",                 "sprintf_FW",           SIG_NAMED_SUB},
{sig_match_named,   "PTM_GetCurrentItem",       "PTM_GetCurrentItem_FW",},
{sig_match_named,   "DisableISDriveError",      "DisableISDriveError_FW",},
{sig_match_named,   "hook_CreateTask",          "CreateTask",           SIG_NAMED_CLEARTHUMB},
// alternate if CreateTask is in ROM
{sig_match_named,   "hook_CreateTask_low",      "CreateTask_low",       SIG_NAMED_CLEARTHUMB},
{sig_match_named,   "malloc_strictly",          "task_EvShel",          SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "DebugAssert2",             "malloc_strictly",      SIG_NAMED_NTH(3,SUB)},
// fails on d7 ILCs
//{sig_match_named,   "AcquireRecursiveLockStrictly","StartWDT_FW",       SIG_NAMED_NTH(1,SUB)},
{sig_match_named,   "AcquireRecursiveLockStrictly","PTM_AllReset_FW",   SIG_NAMED_SUB},
{sig_match_named,   "CheckAllEventFlag",        "ChargeStrobeForFA_FW", SIG_NAMED_SUB},
{sig_match_named,   "ClearEventFlag",           "GetAEIntegralValueWithFix_FW",SIG_NAMED_SUB},
{sig_match_named,   "CheckAnyEventFlag",        "task_SynchTask",       SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "taskcreate_LowConsole",    "task_EvShel",          SIG_NAMED_SUB},
{sig_match_named,   "CreateMessageQueueStrictly","taskcreate_LowConsole",SIG_NAMED_SUB},
{sig_match_named,   "CreateBinarySemaphoreStrictly","taskcreate_LowConsole",SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "PostMessageQueue",         "GetCh_FW",             SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "CreateEventFlagStrictly",  "InitializeDigicon_FW", SIG_NAMED_SUB},
{sig_match_named,   "WaitForAnyEventFlag",      "task_DPOFTask",        SIG_NAMED_SUB},
{sig_match_named,   "GetEventFlagValue",        "task_DPOFTask",        SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "CreateBinarySemaphore",    "task_UartLog",         SIG_NAMED_SUB},
{sig_match_named,   "PostMessageQueueStrictly", "EF.IsChargeFull_FW",   SIG_NAMED_SUB},
{sig_match_named,   "SetEventFlag",             "StopStrobeChargeForFA_FW",SIG_NAMED_SUB},
{sig_match_named,   "TryReceiveMessageQueue",   "task_DvlpSeqTask",     SIG_NAMED_NTH(3,SUB)},
// Semaphore funcs found by eventproc match, but want veneers. Will warn if mismatched
{sig_match_named,   "TakeSemaphore",            "task_Bye",             SIG_NAMED_SUB},
{sig_match_named_last,"GiveSemaphore",          "TurnOnVideoOutMode_FW",SIG_NAMED_LAST_RANGE(10,24)},
// TODO finding through veneers would be better for disassembly
{sig_match_named,   "givesemaphore_low",        "GiveSemaphore",        SIG_NAMED_SUB,      SIG_DRY_MAX(52)}, // first call on dry <=52
{sig_match_named,   "givesemaphore_low",        "GiveSemaphore",        SIG_NAMED_NTH(2,SUB),SIG_DRY_MIN(53)}, // 2nd call on dry >52

// can't use last because func has early return POP
{sig_match_named,   "ReleaseRecursiveLock",     "StartWDT_FW",          SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "MoveOpticalZoomAt",        "SS.MoveOpticalZoomAt_FW",SIG_NAMED_SUB},
{sig_match_named,   "SetVideoOutType",          "SetVideoOutType_FW",   SIG_NAMED_SUB},
{sig_match_named,   "GetVideoOutType",          "GetVideoOutType_FW"},
{sig_match_named,   "is_movie_recording",       "UIFS_StopMovieRecord_FW",SIG_NAMED_SUB},
{sig_match_named,   "dry_con_printf",           "ShowCameraLogInfo_FW", SIG_NAMED_SUB},
{sig_match_named,   "ui_malloc",                "CreateController_FW",  SIG_NAMED_SUB},
{sig_match_ui_mem_func_ptr,"ui_malloc_ptr",     "ui_malloc", },
{sig_match_func_ptr_val, "ui_malloc_default",   "ui_malloc_ptr", },
{sig_match_named,   "pvm_malloc",               "ui_malloc_default",    SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "pvm_get_largest_free_block_size_ptr","ui_malloc_default",SIG_NAMED_NTH(3,SUB)},
{sig_match_named,   "pvm_get_largest_free_block_size","pvm_get_largest_free_block_size_ptr",SIG_NAMED_SUB},
{sig_match_named,   "ui_free",                  "CreateController_FW",  SIG_NAMED_NTH(3,SUB)},
{sig_match_ui_mem_func_ptr,"ui_free_ptr",       "ui_free",},
{sig_match_func_ptr_val, "ui_free_default",     "ui_free_ptr", },
{sig_match_named_last,"pvm_free",               "ui_free_default",      SIG_NAMED_LAST_RANGE(11,16)},
{sig_match_near_str,"pvm_init_pool",            "\n%ld-byte from heap\n",SIG_NEAR_BEFORE(14,3)},

{sig_match_near_str,"cameracon_set_state",      "AC:ChkCom2PB",         SIG_NEAR_BEFORE(4,1),},
{sig_match_near_str,"cameracon_get_state",      "DlvrUSBCnct",          SIG_NEAR_AFTER(5,2)},
{sig_match_near_str,"IsWirelessConnect",        "WiFiDisconnect",       SIG_NEAR_BEFORE(6,1),SIG_DRY_MAX(52)},
{sig_match_near_str,"IsWirelessConnect",        "USBDisconnect",        SIG_NEAR_BEFORE(6,1),SIG_DRY_MIN(54)},
{sig_match_var_struct_get,"cameracon_state",    "cameracon_get_state",},
{sig_match_str_arg_call,"strstr",               "AUTPLAY",              SIG_STRCALL_ARG(1)},
{sig_match_named_last,"strchr",                 "strstr",               SIG_NAMED_LAST_RANGE(14,22)},
{sig_match_near_str,"init_task_error",          "USER_MEM size checking",SIG_NEAR_AFTER(3,1)},
{sig_match_named_last,"dry_panic",              "init_task_error",      SIG_NAMED_LAST_RANGE(4,12)},
{sig_match_named,   "dry_panic_low",            "dry_panic",            SIG_NAMED_NTH(3,SUB),SIG_DRY_ANY,   SIG_NO_D6},
// not present in d7, inlined in d6 dry 58p9+
{sig_match_near_str,"data_synchronization_barrier","ER DlphCntInv",     SIG_NEAR_AFTER(3,2),SIG_DRY_MAXP(58,8),    SIG_NO_D7},
// alternate match because "exec" lands near a literal pool on some cams
{sig_match_near_str,"bzero",                    "Canon Degital Camera"/*sic*/,SIG_NEAR_AFTER(8,2)|SIG_NEAR_INDIRECT},
//{sig_match_near_str,"bzero",                    "FromDate",             SIG_NEAR_BEFORE(2,1)},
// eventproc on most cams, but not EOS Ms
{sig_match_named,   "GetCurrentDriveBaseSvValue","GetCurrentDriveBaseSvValue_FW",SIG_NAMED_NTH(1,SUB)},
{sig_match_near_str,"GetCurrentDriveBaseSvValue","IsExecutePreConti",   SIG_NEAR_AFTER(7,2)},
{sig_match_named,   "memset32",                 "bzero",                SIG_NAMED_NTH(1,INSN)},
{sig_match_dry_memset,"dry_memset",             "ClearDefectTurnTable_FW",0,            SIG_DRY_MIN(53)},
{sig_match_near_str,"dry_memset",               "[xWiSE] generateRandom Err.\n",SIG_NEAR_BEFORE(8,2),SIG_DRY_MAX(52)},
{sig_match_dry_memzero,"dry_memzero",           "SetDefaultRecParameter_FW"},
{sig_match_dry_memcpy_bytes,"dry_memcpy_bytes", "SaveDefectAdjTable_FW",},
{sig_match_near_str,"dry_memmove_bytes",        "NoOperation BulkOut!Remain.Length = %lu",SIG_NEAR_AFTER(18,3)},
{sig_match_cam_has_iris_diaphragm,"CAM_HAS_IRIS_DIAPHRAGM","task_IrisEvent",},
{sig_match_near_str,"ImagerActivate",           "Fail ImagerActivate(ErrorCode:%x)\r",SIG_NEAR_BEFORE(6,1)},
{sig_match_screenlock_helper,"screenlock_helper","UIFS_DisplayFirmUpdateView_FW"},
{sig_match_named,   "ScreenLock",               "screenlock_helper",SIG_NAMED_SUB},
{sig_match_fclose_low,"fclose_low",             "Fclose_Fut"},
{sig_match_named,   "fut_flush",                "fclose_low",          SIG_NAMED_NTH(1,SUB)},
//{sig_match_screenlock,"ScreenLock",             "UIFS_DisplayFirmUpdateView_FW"},
{sig_match_screenunlock,"ScreenUnlock",         "screenlock_helper",    0,SIG_DRY_MAX(55)},
{sig_match_near_str,"ScreenUnlock",             "PB._ErrorRef",         SIG_NEAR_AFTER(8,2)|SIG_NEAR_JMP_SUB,SIG_DRY_RANGE(57,58)},
{sig_match_near_str,"ScreenUnlock",             "PB._ErrorRef:SI",      SIG_NEAR_AFTER(8,2)|SIG_NEAR_JMP_SUB,SIG_DRY_MIN(59)},
{sig_match_log_camera_event,"LogCameraEvent",   "task_StartupImage",},
{sig_match_physw_misc, "physw_misc",            "task_PhySw"},
{sig_match_kbd_read_keys, "kbd_read_keys",      "kbd_p1_f"},
{sig_match_get_kbd_state, "GetKbdState",        "kbd_read_keys"},
{sig_match_create_jumptable, "CreateJumptable", "InitializeAdjustmentSystem_FW"},
// also finds DebugAssert
{sig_match_take_semaphore_strict, "TakeSemaphoreStrictly","Fopen_Fut"},
{sig_match_near_str,"dry_error_printf",         "\nSystem Panic: Module = %d, Panic = %d\n",SIG_NEAR_AFTER(2,1)},
// string switched between Dry58p3 and Dry58p9
{sig_match_get_semaphore_value,"GetSemaphoreValue","\tRaw[%i]",         0,              SIG_DRY_MAXP(58,8)},
{sig_match_get_semaphore_value,"GetSemaphoreValue","BlankRaw(%d)",      0,              SIG_DRY_MINP(58,9)},
{sig_match_stat,    "stat",                     "A/uartr.req"},
{sig_match_open,    "open",                     "Open_FW",              0,              SIG_DRY_MAX(57)},
{sig_match_open,    "open",                     "Open_low",             0,              SIG_DRY_MIN(58)},
{sig_match_named,   "get_self_task_errno_pointer","open",               SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "get_self_task_id",         "get_self_task_errno_pointer",SIG_NAMED_SUB},
{sig_match_umalloc, "AllocateUncacheableMemory","Fopen_Fut_FW"},
{sig_match_named,   "dcache_flush_range",       "AllocateUncacheableMemory",SIG_NAMED_NTH(2,SUB)},
{sig_match_ufree,   "FreeUncacheableMemory",    "Fclose_Fut_FW"},
{sig_match_cam_uncached_bit,"CAM_UNCACHED_BIT", "FreeUncacheableMemory"},
{sig_match_umalloc_strictly,"umalloc_strictly", "DPOFTask"},
{sig_match_dcache_clean_flush_and_disable,"dcache_clean_flush_and_disable", "MemoryChecker_FW"},
{sig_match_get_rom_id,"GetRomID",               "GetRomID_FW",},
{sig_match_dcache_flush_and_enable,"dcache_flush_and_enable", "GetRomID",0,             SIG_DRY_ANY,            SIG_NO_D7}, // d7 code is different
{sig_match_deletefile_fut,"DeleteFile_Fut",     "Get Err TempPath"},
{sig_match_near_str,"createsemaphore_low",      "termLock",             SIG_NEAR_AFTER(3,1)},
// old match, malloc gets more cams and veneers
//{sig_match_near_str,"takesemaphore_low",        "sem_test_callback",    SIG_NEAR_AFTER(12,2)},
{sig_match_named,"takesemaphore_low",        "malloc",                  SIG_NAMED_SUB},
// not using Strictly, to pick up veneers
{sig_match_near_str,"AcquireRecursiveLock",     "not executed\n",SIG_NEAR_BEFORE(20,3),SIG_DRY_ANY,             SIG_NO_D7},
{sig_match_near_str,"AcquireRecursiveLock",     "COCOA: ERR: QIF AcquireRecursiveLock QifPushmqif failure!!",SIG_NEAR_BEFORE(10,1),SIG_DRY_ANY,             SIG_NO_D6},
{sig_match_near_str,"CreateCountingSemaphoreStrictly","DvlpSeqTask",    SIG_NEAR_BEFORE(18,3)},
{sig_match_near_str,"CreateMessageQueue",       "CreateMessageQueue:%ld",SIG_NEAR_BEFORE(7,1)},
{sig_match_near_str,"CreateEventFlag",          "CreateEventFlag:%ld",  SIG_NEAR_BEFORE(7,1),SIG_DRY_MAX(57)},
{sig_match_near_str,"CreateEventFlag",          "CreateEventFlag:%ld",  SIG_NEAR_BEFORE(9,1),SIG_DRY_MIN(58)},
{sig_match_near_str,"CreateRecursiveLock",      "WdtInt",               SIG_NEAR_BEFORE(9,1)},
{sig_match_near_str,"CreateRecursiveLockStrictly","LoadedScript",       SIG_NEAR_AFTER(6,2)},
{sig_match_near_str,"DeleteMessageQueue",       "DeleteMessageQueue(%d) is FAILURE",SIG_NEAR_BEFORE(10,1)},
{sig_match_near_str,"DeleteEventFlag",          "DeleteEventFlag(%d) is FAILURE",SIG_NEAR_BEFORE(10,1)},
{sig_match_near_str,"ReceiveMessageQueue",      "ReceiveMessageQue:%d", SIG_NEAR_BEFORE(9,1)},
{sig_match_near_str,"RegisterInterruptHandler", "WdtInt",               SIG_NEAR_AFTER(3,1)},
{sig_match_near_str,"TryPostMessageQueue",      "TryPostMessageQueue(%d)\n",SIG_NEAR_BEFORE(9,1),SIG_DRY_MAX(52)},
// different string on cams newer than sx280
{sig_match_near_str,"TryPostMessageQueue",      "[CWS]TryPostMessageQueue(%d) Failed\n",SIG_NEAR_BEFORE(9,1)},
{sig_match_near_str,"TryTakeSemaphore",         "FileScheduleTask",     SIG_NEAR_AFTER(10,2),SIG_DRY_MAX(57)},
{sig_match_try_take_sem_dry_gt_57,"TryTakeSemaphore","task_ImageStoreTask",0,SIG_DRY_MIN(58)},
// pick up takesemaphore_low from TryTakeSemaphore in case not matched by earlier rule
{sig_match_named,   "takesemaphore_low",        "TryTakeSemaphore",     SIG_NAMED_SUB},
{sig_match_near_str,"WaitForAllEventFlag",      "Error WaitEvent PREPARE_TESTREC_EXECUTED.", SIG_NEAR_BEFORE(5,1)},
{sig_match_near_str,"WaitForAnyEventFlagStrictly","_imageSensorTask",   SIG_NEAR_AFTER(10,2)},
{sig_match_wait_all_eventflag_strict,"WaitForAllEventFlagStrictly","EF.StartInternalMainFlash_FW"},
{sig_match_near_str,"DeleteSemaphore",          "DeleteSemaphore passed",SIG_NEAR_BEFORE(3,1)},
{sig_match_get_num_posted_messages,"GetNumberOfPostedMessages","task_CtgTotalTask"},
{sig_match_near_str,"LocalTime",                "%Y-%m-%dT%H:%M:%S",    SIG_NEAR_BEFORE(5,1),SIG_DRY_MAX(57)},
{sig_match_near_str,"LocalTime",                "%Y.%m.%d %H:%M:%S",    SIG_NEAR_BEFORE(5,1)},
{sig_match_near_str,"strftime",                 "%Y/%m/%d %H:%M:%S",    SIG_NEAR_AFTER(3,1)},
{sig_match_near_str,"OpenFastDir",              "OpenFastDir_ERROR\n",  SIG_NEAR_BEFORE(5,1)},
{sig_match_readfastdir,"ReadFastDir",           "ReadFast_ERROR\n",     SIG_NEAR_BEFORE(24,1)},
{sig_match_near_str,"PT_PlaySound",             "BufAccBeep",           SIG_NEAR_AFTER(7,2)|SIG_NEAR_JMP_SUB,SIG_DRY_MAX(58)},
// for dry 59+ the above match finds function that takes different params on some cameras (d7?)
{sig_match_near_str,"PT_PlaySound",             "PB._PMenuCBR",         SIG_NEAR_BEFORE(7,3),SIG_DRY_MIN(59)},
{sig_match_closedir,"closedir",                 "ReadFast_ERROR\n",     SIG_NEAR_AFTER(1,1)},
{sig_match_strrchr,"strrchr",                   "ReadFast_ERROR\n",     SIG_NEAR_AFTER(9,2)},
{sig_match_strrchr,"strrchr",                   "ReadFast_ERROR\n",     SIG_NEAR_BEFORE(18,4)},
{sig_match_time,    "time",                     "<UseAreaSize> DataWidth : %d , DataHeight : %d\r\n",},
{sig_match_near_str,"strcat",                   "String can't be displayed; no more space in buffer",SIG_NEAR_AFTER(5,2),SIG_DRY_MAX(52)},
{sig_match_near_str,"strcat",                   "/api/getimagelist",    SIG_NEAR_AFTER(4,1),SIG_DRY_MIN(53)},
//{sig_match_near_str,"strchr",                   "-._~",                 SIG_NEAR_AFTER(4,1)}, // ref not in d7 main fw
{sig_match_strncpy, "strncpy",                  "UnRegisterEventProcedure",},
{sig_match_strncmp, "strncmp",                  "EXFAT   ",},
{sig_match_strtolx, "strtolx",                  "CheckSumAll_FW",},
{sig_match_near_str,"strtol",                   "prio <task ID> <priority>\n",SIG_NEAR_AFTER(7,1)},
{sig_match_exec_evp,"ExecuteEventProcedure",    "Can not Execute "},
{sig_match_fgets_fut,"Fgets_Fut",               "CheckSumAll_FW",},
{sig_match_named,   "fgets_low",                "Fgets_Fut",            SIG_NAMED_NTH(2,SUB)},
{sig_match_log,     "_log",                     "_log10",},
{sig_match_pow_dry_52,"_pow",                   "GetDefectTvAdj_FW",    0,                  SIG_DRY_MAX(52)},
{sig_match_pow_dry_gt_52,"_pow",                "GetDefectTvAdj_FW",    0,                  SIG_DRY_RANGEP(53,0,59,3)},
{sig_match_sqrt,    "_sqrt",                    "CalcSqrt",},
{sig_match_named,   "get_fstype",               "OpenFastDir",          SIG_NAMED_NTH(2,SUB)},
{sig_match_near_str,"GetMemInfo",               " -- refusing to print malloc information.\n",SIG_NEAR_AFTER(7,2)},
{sig_match_get_drive_cluster_size,"GetDrive_ClusterSize","OpLog.WriteToSD_FW",},
{sig_match_mktime_ext,"mktime_ext",             "%04d%02d%02dT%02d%02d%02d.%01d",},
{sig_match_near_str,"PB2Rec",                   "AC:ActionP2R Fail",    SIG_NEAR_BEFORE(6,1)},
{sig_match_rec2pb,  "Rec2PB",                   "_EnrySRec",},
//{sig_match_named,   "GetParameterData",         "PTM_RestoreUIProperty_FW",          SIG_NAMED_NTH(3,JMP_SUB)},
{sig_match_get_parameter_data,"GetParameterData","PTM_RestoreUIProperty_FW",},
{sig_match_prepdir_1,"PrepareDirectory_1",      "<OpenFileWithDir> PrepareDirectory NG\r\n",SIG_NEAR_BEFORE(7,1)},
{sig_match_prepdir_x,"PrepareDirectory_x",      "PrepareDirectory_1",},
{sig_match_prepdir_0,"PrepareDirectory_0",      "PrepareDirectory_1",},
{sig_match_mkdir,   "MakeDirectory_Fut",        "PrepareDirectory_x",},
// moved to sig_rules_initial to allow auto-detecting handlers
//{sig_match_add_ptp_handler,"add_ptp_handler",   "PTPtoFAPI_EventProcTask_Try",},
{sig_match_qsort,   "qsort",                    "task_MetaCtg",},
{sig_match_deletedirectory_fut,"DeleteDirectory_Fut","RedEyeController.c",},
{sig_match_set_control_event,"set_control_event","LogicalEvent:0x%04x:adr:%p,Para:%ld",},
// newer cams use %08x
{sig_match_set_control_event,"set_control_event","LogicalEvent:0x%08x:adr:%p,Para:%ld",},
{sig_match_displaybusyonscreen_52,"displaybusyonscreen","_PBBusyScrn",  0,                  SIG_DRY_MAX(52)},
{sig_match_undisplaybusyonscreen_52,"undisplaybusyonscreen","_PBBusyScrn",0,                SIG_DRY_MAX(52)},
{sig_match_near_str,"srand",                    "Canon Degital Camera"/*sic*/,SIG_NEAR_AFTER(14,4)|SIG_NEAR_INDIRECT},
{sig_match_near_str,"rand",                     "Canon Degital Camera"/*sic*/,SIG_NEAR_AFTER(15,5)|SIG_NEAR_INDIRECT},
{sig_match_set_hp_timer_after_now,"SetHPTimerAfterNow","MechaNC.c",},
{sig_match_levent_table,"levent_table",         "ShowLogicalEventName_FW",},
{sig_match_flash_param_table,"FlashParamsTable","GetParameterData",},
{sig_match_named,   "get_playrec_mode",         "task_SsStartupTask",   SIG_NAMED_SUB},
{sig_match_var_struct_get,"playrec_mode",       "get_playrec_mode",},
{sig_match_jpeg_count_str,"jpeg_count_str",     "9999",},
{sig_match_physw_event_table,"physw_event_table","kbd_read_keys_r2",},
{sig_match_uiprop_count,"uiprop_count",         "PTM_SetCurrentItem_FW",},
{sig_match_get_canon_mode_list,"get_canon_mode_list","AC:PTM_Init",},
{sig_match_rom_ptr_get,"canon_mode_list",       "get_canon_mode_list",},
{sig_match_zoom_busy,"zoom_busy",               "ResetZoomLens_FW",},
{sig_match_focus_busy,"focus_busy",             "MoveFocusLensToTerminate_FW",},
{sig_match_aram_size,"ARAM_HEAP_SIZE",          "AdditionAgentRAM_FW",  0,                  SIG_DRY_MAX(58)},
{sig_match_aram_size_gt58,"ARAM_HEAP_SIZE",     "AdditionAgentRAM_FW",  0,                  SIG_DRY_MIN(59)},
{sig_match_aram_start,"ARAM_HEAP_START",        "AdditionAgentRAM_FW",},
{sig_match_icache_flush_range,"icache_flush_range","AdditionAgentRAM_FW",},
{sig_match__nrflag,"_nrflag",                   "NRTBL.SetDarkSubType_FW",},
{sig_match_near_str,"transfer_src_overlay_helper","Window_EmergencyRefreshPhysicalScreen",SIG_NEAR_BEFORE(6,1)},
{sig_match_transfer_src_overlay,"transfer_src_overlay","transfer_src_overlay_helper",},
{sig_match_named,"GraphicSystemCoreFinish_helper","transfer_src_overlay",SIG_NAMED_NTH(3,SUB),SIG_DRY_MAX(52)},
{sig_match_named,"GraphicSystemCoreFinish_helper","transfer_src_overlay",SIG_NAMED_NTH(4,SUB),SIG_DRY_RANGE(53,57)},// VTMReduuce fails on M10
{sig_match_named,"GraphicSystemCoreFinish_helper","transfer_src_overlay",SIG_NAMED_NTH(5,SUB),SIG_DRY_MIN(58)},// additional call
//{sig_match_near_str,"GraphicSystemCoreFinish_helper","VTMReduuce"/*sic*/,SIG_NEAR_BEFORE(6,1),SIG_DRY_RANGE(58,58)},
//{sig_match_near_str,"GraphicSystemCoreFinish_helper","VTMReduce",SIG_NEAR_BEFORE(6,1),SIG_DRY_MIN(59)},  // canon fixed the typo, works on sx730 but others have diff code
{sig_match_named_last,"fw_yuv_layer_buf_helper","transfer_src_overlay",SIG_NAMED_LAST_RANGE(60,85)},
{sig_match_named,"GraphicSystemCoreFinish","GraphicSystemCoreFinish_helper",SIG_NAMED_SUB},
{sig_match_named,"mzrm_createmsg","GraphicSystemCoreFinish",SIG_NAMED_SUB},
{sig_match_named_last,"mzrm_sendmsg","GraphicSystemCoreFinish",SIG_NAMED_LAST_RANGE(10,16)},
{sig_match_zicokick_52,"zicokick_start",        "ZicoKick Start\n",0,SIG_DRY_MAX(52)},
{sig_match_zicokick_gt52,"zicokick_start",      "ZicoKick Start\n",0,SIG_DRY_MIN(53)},
{sig_match_zicokick_copy,"zicokick_copy",       "zicokick_start"},
{sig_match_zicokick_values,"zicokick_values",   "zicokick_start"},
{sig_match_named_last,"dcache_clean_range",     "zicokick_start",               SIG_NAMED_LAST_RANGE(40,58)},
{sig_match_init_ex_drivers,"init_ex_drivers",   "task_Startup"},
{sig_match_omar_init,"omar_init",               "init_ex_drivers",              0,          SIG_DRY_ANY,        SIG_NO_D7},
{sig_match_init_error_handlers,"init_error_handlers","task_Startup"},
{sig_match_named,   "set_assert_handler",       "init_error_handlers",          SIG_NAMED_NTH(2,SUB)},
{sig_match_named,   "set_exception_handler",    "init_error_handlers",          SIG_NAMED_NTH(3,SUB)},
{sig_match_named,   "set_panic_handler",        "init_error_handlers",          SIG_NAMED_NTH(4,JMP_SUB)},
{sig_match_default_assert_handler,"default_assert_handler","init_error_handlers"},
{sig_match_default_exception_handler,"default_exception_handler","init_error_handlers"},
{sig_match_default_panic_handler,"default_panic_handler","init_error_handlers"},
{sig_match_get_task_properties,"get_task_properties","default_assert_handler"},
{sig_match_enable_hdmi_power,"EnableHDMIPower", "HecHdmiCecPhysicalCheckForScript_FW"},
{sig_match_disable_hdmi_power,"DisableHDMIPower","HecHdmiCecPhysicalCheckForScript_FW"},
{sig_match_get_nd_value,"get_nd_value",         "PutInNdFilter",},
{sig_match_get_current_exp,"get_current_exp","ShowCurrentExp_FW",},
{sig_match_get_current_nd_value,"get_current_nd_value","get_current_exp",},
{sig_match_get_current_deltasv,"get_current_deltasv","get_current_exp",},
{sig_match_imager_active_callback,"imager_active_callback","ImagerActivate",},
{sig_match_imager_active,"imager_active","imager_active_callback",},
{sig_match_get_dial_hw_position,"get_dial_hw_position","kbd_p1_f",},
{sig_match_near_str,"get_displaytype","DisplayType : %d\r\n",SIG_NEAR_BEFORE(5,1)},
{sig_match_near_str,"get_ef_lens_name","LENS NAME: %s (Size: %ld Byte)\n",      SIG_NEAR_BEFORE(3,1),SIG_DRY_ANY,   SIG_ILC_ONLY},
{sig_match_near_str,"get_ef_lens_wide_fl","Wide FocalLength = %ld mm\n",        SIG_NEAR_BEFORE(3,1),SIG_DRY_ANY,   SIG_ILC_ONLY},
{sig_match_near_str,"get_ef_lens_tele_fl","Tele FocalLength = %ld mm\n",        SIG_NEAR_BEFORE(3,1),SIG_DRY_ANY,   SIG_ILC_ONLY},
{sig_match_named,"GetLensIDValue","GetLensIDValue_FW",                          0,                   SIG_DRY_ANY,   SIG_ILC_ONLY},
{sig_match_prop_string,"PROPCASE_AFSTEP", "\n\rError : GetAFStepResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_FOCUS_STATE", "\n\rError : GetAFResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_AV", "\n\rError : GetAvResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_BV", "\n\rError : GetBvResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_DELTA_DIGITALGAIN", "\n\rError : GetDeltaDigitalResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_DELTA_SV", "\n\rError : GetDeltaGainResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_DELTA_ND", "\n\rError : GetDeltaNdResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_EV_CORRECTION_2", "\n\rError : GetRealExposureCompensationResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_ORIENTATION_SENSOR", "\n\rError : GetRotationAngleResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_SV_MARKET", "\n\rError : GetSvResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_SVFIX", "\n\rError : GetSvFixResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_TV", "\n\rError : GetTvResult",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_HSCAPTURE", "GetPropertyFromCase Error [HSCapture]",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_FLASH_FIRE", "FlashDecision",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_FELOCK", "GetPropertyFromCurrentCase Error [FELock]",SIG_NEAR_BEFORE(7,1)},
{sig_match_prop_string,"PROPCASE_FLASH_ADJUST_MODE", "GetPropertyFromCurrentCase Error [FlashAdjust]",SIG_NEAR_BEFORE(7,1)},
{sig_match_exmem_vars,"exmem_types_table", "ExMem.View_FW"},
{sig_match_av_over_sem,"av_override_semaphore", "MoveIrisWithAv_FW"},
{sig_match_canon_menu_active,"canon_menu_active", "StartRecModeMenu_FW"},
{sig_match_file_counter_init,"file_counter_var_init","task_InitFileModules",},
{sig_match_file_counter_var,"file_counter_var","file_counter_var_init",},
{sig_match_var_struct_get,"displaytype",       "get_displaytype",},
{sig_match_palette_vars,"palette_control",     "transfer_src_overlay_helper",0, SIG_DRY_MAX(58)},// Dry59 code is different
{sig_match_live_free_cluster_count,"live_free_cluster_count","Close",0,         SIG_DRY_MAX(57)},
{sig_match_live_free_cluster_count,"live_free_cluster_count","Close_low",0,     SIG_DRY_MIN(58)},
{sig_match_debug_logging_ptr,"debug_logging_ptr","[GRYP]T: Terminate(Pri): Completed.\n",SIG_STRCALL_ARG(0)|SIG_STRCALL_JMP_REG},
{sig_match_debug_logging_flag,"debug_logging_flag","[GRYP]E: Terminate(Pri): Event flag delete error.[0x%08x]\n",SIG_STRCALL_ARG(0)|SIG_STRCALL_CALL_REG},
{sig_match_mzrm_sendmsg_ret_adr,"mzrm_sendmsg_ret_adr","SendMsg   : %d\n",      SIG_STRCALL_ARG(0)|SIG_STRCALL_CALL_REG},
{sig_match_fw_yuv_layer_buf_52,"fw_yuv_layer_buf","fw_yuv_layer_buf_helper",0,SIG_DRY_MAX(52)}, // dry52 has different code
{sig_match_fw_yuv_layer_buf_gt52,"fw_yuv_layer_buf","fw_yuv_layer_buf_helper",0,SIG_DRY_MIN(54)}, // dry52 has different code

{sig_match_named_seq,   "MakeSDCardBootable",                   "MakeBootDisk_FW",              0 },    // param = index into sig_seq array
{sig_match_named_seq,   "GetFocusLensSubjectDistanceFromLens",  "SetISFocusLensDistance_FW",    1 },    // param = index into sig_seq array
{sig_match_named_seq,   "apex2us",                              "ConvertTvToExposureTime_FW",   2 },    // param = index into sig_seq array

{sig_match_named,   "CancelHPTimer",    "task_TouchPanel",      SIG_NAMED_NTH(8,SUB), 0, 0, SIG_OPTIONAL },

{sig_match_named_next_func, "Feof_Fut", "Fseek_Fut" },
{sig_match_named_next_func, "Fflush_Fut", "Feof_Fut" },

{sig_match_named,   "err_init_task",    "init_task_error" },

// either push, ldr or ldr, ldr, push)
{sig_match_func_using_str,  "EnterToCompensationEVF",   "ExpOn",        SIG_USESTR(1,2) },
{sig_match_func_using_str,  "ExitFromCompensationEVF",  "ExpOff",       SIG_USESTR(1,2) },
// search back for push
{sig_match_func_using_str,  "ExpCtrlTool_StartContiAE", "StartContiAE", SIG_USESTR_BACK(13) },
// TODO some d7 cams have ldr before push, but fail to match for other reasons
{sig_match_func_using_str,  "ExpCtrlTool_StopContiAE",  "StopContiAE",  SIG_USESTR_BACK(13) },
{sig_match_init_sd_io_funcs,"init_sd_io_funcs",         "\nStartDiskboot\n",   SIG_NEAR_BEFORE(7,2) },
{sig_match_sd_io_func,   "WriteSDCard",         "init_sd_io_funcs",     0x54 | SIG_SD_IO_FUNC_STR }, // WriteSDCard is at 0x54 for all known d6/d7 firmware
{sig_match_sd_io_func,   "ReadSDCard",          "init_sd_io_funcs",     0x50 | SIG_SD_IO_FUNC_STR },
{sig_match_sd_io_func,   "InitSDCard",          "init_sd_io_funcs",     0x58 | SIG_SD_IO_FUNC_STRD1, SIG_DRY_MAX(58) },
{sig_match_sd_io_func,   "InitSDCard",          "init_sd_io_funcs",     0x58 | SIG_SD_IO_FUNC_STR | SIG_SD_IO_FUNC_STRD1, SIG_DRY_MIN(59) },
// note this doesn't match on M6, which has two sets of possible IO funcs
{sig_match_sd_io_func,   "GetSDCardTotalSect",  "init_sd_io_funcs",     0x64 | SIG_SD_IO_FUNC_STR },
// sx280 is different from other dry52, no trailing "s" on the logs
{sig_match_func_using_str,  "SD_ChgClkSpd",     "ChgClkSpd(%d,%d)",     SIG_USESTR_BACK(11)|SIG_SEARCH_ROM,   SIG_DRY_MAX(52), SIG_OPTIONAL }, // sx280
{sig_match_func_using_str,  "SD_ChgClkSpd",     "ChgClkSpd(%d,%d)s",    SIG_USESTR_BACK(11)|SIG_SEARCH_ROM,   SIG_DRY_MAX(52), SIG_OPTIONAL }, // other r52
{sig_match_func_using_str,  "SD_ChgClkSpd",     "ChgClkSpd(%d,%d)s",    SIG_USESTR_BACK(11)|SIG_SEARCH_RAM|SIG_SEARCH_ROM, SIG_DRY_MIN(53) }, // all others, not optional
{sig_match_func_using_str,  "SD_GetTtlSect",    "GetTtlSect(%d)",       SIG_USESTR_BACK(7) |SIG_SEARCH_ROM,   SIG_DRY_MAX(52), SIG_OPTIONAL }, // sx280
{sig_match_func_using_str,  "SD_GetTtlSect",    "GetTtlSect(%d)s",      SIG_USESTR_BACK(7) |SIG_SEARCH_ROM,   SIG_DRY_MAX(52), SIG_OPTIONAL }, // other r52
{sig_match_func_using_str,  "SD_GetTtlSect",    "GetTtlSect(%d)s",      SIG_USESTR_BACK(10)|SIG_SEARCH_RAM|SIG_SEARCH_ROM, SIG_DRY_MIN(53) }, // all others, not optional
{sig_match_named,           "SD_take_sem",      "SD_ChgClkSpd",         SIG_NAMED_NTH(1,SUB), SIG_DRY_MAXP(59,3) },
{sig_match_named,           "SD_give_sem",      "SD_GetTtlSect",        SIG_NAMED_NTH(3,SUB), SIG_DRY_MAXP(59,3) },
{sig_match_named,           "SD_debug_log",     "SD_GetTtlSect",        SIG_NAMED_NTH(2,SUB), SIG_DRY_MAXP(59,3) },
{sig_match_named,           "sddomChangeClockSpeed","SD_ChgClkSpd",     SIG_NAMED_NTH(3,SUB), SIG_DRY_MAXP(59,3) },
// m100 (59p4, d7) adds an extra call to the start of a bunch of SD functions, sx730 (59p4, d6) does not
// could use string_match_near but sx280 strings differ
{sig_match_named,           "SD_take_sem",      "SD_ChgClkSpd",         SIG_NAMED_NTH(1,SUB), SIG_DRY_MINP(59,4), SIG_NO_D7 },
{sig_match_named,           "SD_give_sem",      "SD_GetTtlSect",        SIG_NAMED_NTH(3,SUB), SIG_DRY_MINP(59,4), SIG_NO_D7 },
{sig_match_named,           "SD_debug_log",     "SD_GetTtlSect",        SIG_NAMED_NTH(2,SUB), SIG_DRY_MINP(59,4), SIG_NO_D7 },
{sig_match_named,           "sddomChangeClockSpeed","SD_ChgClkSpd",     SIG_NAMED_NTH(3,SUB), SIG_DRY_MINP(59,4), SIG_NO_D7 },
// +1 to Nths for M100
{sig_match_named,           "SD_take_sem",      "SD_ChgClkSpd",         SIG_NAMED_NTH(2,SUB), SIG_DRY_MINP(59,4), SIG_NO_D6 },
{sig_match_named,           "SD_give_sem",      "SD_GetTtlSect",        SIG_NAMED_NTH(4,SUB), SIG_DRY_MINP(59,4), SIG_NO_D6 },
{sig_match_named,           "SD_debug_log",     "SD_GetTtlSect",        SIG_NAMED_NTH(3,SUB), SIG_DRY_MINP(59,4), SIG_NO_D6 },
{sig_match_named,           "sddomChangeClockSpeed","SD_ChgClkSpd",     SIG_NAMED_NTH(4,SUB), SIG_DRY_MINP(59,4), SIG_NO_D6 },
// error messages changed
{sig_match_str_arg_call,"SD_error_log", "%s(%d) MicroSeconds = 0 NG!\n",SIG_STRCALL_ARG(1)|SIG_STRCALL_JMP_IMM|SIG_SEARCH_ROM|SIG_SEARCH_RAM,SIG_DRY_MAX(55)},
{sig_match_str_arg_call,"SD_error_log", "%s(%d) MicroSeconds = 0 ERR!\n",SIG_STRCALL_ARG(1)|SIG_STRCALL_JMP_IMM|SIG_SEARCH_ROM|SIG_SEARCH_RAM,SIG_DRY_MIN(56)},
{sig_match_func_using_str,  "SD_GetSpd",     "GetSpd(%d)",     SIG_USESTR_BACK(11)|SIG_SEARCH_ROM,   SIG_DRY_MAX(52), SIG_OPTIONAL }, // sx280
{sig_match_func_using_str,  "SD_GetSpd",     "GetSpd(%d)s",    SIG_USESTR_BACK(11)|SIG_SEARCH_ROM,   SIG_DRY_MAX(52), SIG_OPTIONAL }, // other r52
{sig_match_func_using_str,  "SD_GetSpd",     "GetSpd(%d)s",    SIG_USESTR_BACK(11)|SIG_SEARCH_RAM|SIG_SEARCH_ROM, SIG_DRY_MIN(53) }, // all others, not optional
{sig_match_named,           "SD_get_speed_id","SD_GetSpd",     SIG_NAMED_NTH(3,SUB), SIG_DRY_MAXP(59,3) },
{sig_match_named,           "SD_get_speed_id","SD_GetSpd",     SIG_NAMED_NTH(3,SUB), SIG_DRY_MINP(59,4), SIG_NO_D7 },
{sig_match_named,           "SD_get_speed_id","SD_GetSpd",     SIG_NAMED_NTH(4,SUB), SIG_DRY_MINP(59,4), SIG_NO_D6 },
{sig_match_near_str, "SetSDClkFrequency","%s(%d) SetSDClkFrequency() NG!\n",SIG_NEAR_BEFORE(4,1)|SIG_SEARCH_RAM|SIG_SEARCH_ROM, SIG_DRY_MAX(55) },
{sig_match_near_str, "SetSDClkFrequency","%s(%d) SetSDClkFrequency() ERR!\n",SIG_NEAR_BEFORE(6,1)|SIG_SEARCH_RAM|SIG_SEARCH_ROM, SIG_DRY_MIN(56) },
{sig_match_near_str, "SD_CMD55_SendAppCommand","%s(%d) CMD41_GetCardPowerUpStatu() NG!\n",SIG_NEAR_AFTER(10,3)|SIG_SEARCH_RAM|SIG_SEARCH_ROM, SIG_DRY_MAX(55) },
{sig_match_near_str, "SD_CMD55_SendAppCommand","%s(%d) CMD41_GetCardPowerUpStatu() ERR!\n",SIG_NEAR_AFTER(10,3)|SIG_SEARCH_RAM|SIG_SEARCH_ROM, SIG_DRY_MIN(56) },
{sig_match_named,    "SD_cmd_setup",        "SD_CMD55_SendAppCommand",  SIG_NAMED_NTH(1,SUB) },
{sig_match_named,    "SD_cmd_setup_resp48b","SD_CMD55_SendAppCommand",  SIG_NAMED_NTH(2,SUB) },
{sig_match_named,    "SD_cmd_send",         "SD_CMD55_SendAppCommand",  SIG_NAMED_NTH(3,SUB) },
{sig_match_named,    "sdconWaitInterrupt",  "SD_CMD55_SendAppCommand",  SIG_NAMED_NTH(4,SUB) },

{NULL},
};

int sig_rule_applies(firmware *fw, sig_rule_t *rule)
{
    // dryos version
    if((rule->dryos_min && fw->dryos_ver_full < rule->dryos_min) || (rule->dryos_max && fw->dryos_ver_full > rule->dryos_max)) {
        return 0;
    }
    // empty flags == all
    if(!rule->flags) {
        return 1;
    }
    // digic 7 excluded, VMSA
    if((rule->flags & SIG_NO_D7) && (fw->arch_flags & FW_ARCH_FL_VMSA)) {
        return 0;
    }
    // digic 6 excluded, not VMSA
    if((rule->flags & SIG_NO_D6) && !(fw->arch_flags & FW_ARCH_FL_VMSA)) {
        return 0;
    }
    // only valid after ILC detect
    if((rule->flags & SIG_ILC_ONLY) && !get_misc_val_value("CAM_IS_ILC")) {
        return 0;
    }
    if((rule->flags & SIG_NONILC_ONLY) && get_misc_val_value("CAM_IS_ILC")) {
        return 0;
    }
    return 1;
}

void run_sig_rules(firmware *fw, sig_rule_t *sig_rules)
{
    sig_rule_t *rule=sig_rules;
    // for convenience, pass an iter_state to match fns so they don't have to manage
    iter_state_t *is=disasm_iter_new(fw,0);
    while(rule->match_fn) {
        if(!sig_rule_applies(fw,rule)) {
            rule++;
            continue;
        }
//        printf("rule: %s ",rule->name);
        //int r=rule->match_fn(fw,is,rule);
        rule->match_fn(fw,is,rule);
//        printf("%d\n",r);
        rule++;
    }
    disasm_iter_free(is);
}

void add_event_proc(firmware *fw, char *name, uint32_t adr)
{
    // TODO - no ARM eventprocs seen so far, warn
    if(!ADR_IS_THUMB(adr)) {
        printf("add_event_proc: %s is ARM 0x%08x\n",name,adr);
    }
    // attempt to disassemble target
    if(!fw_disasm_iter_single(fw,adr)) {
        printf("add_event_proc: %s disassembly failed at 0x%08x\n",name,adr);
        return;
    }
    // handle functions that immediately jump
    // only one level of jump for now, doesn't check for conditionals, but first insn shouldn't be conditional
    //uint32_t b_adr=B_target(fw,fw->is->insn);
    uint32_t b_adr=get_direct_jump_target(fw,fw->is);
    if(b_adr) {
        char *buf=malloc(strlen(name)+6);
        sprintf(buf,"j_%s_FW",name);
        add_func_name(fw,buf,adr,NULL); // this is the orignal named address
//        adr=b_adr | fw->is->thumb; // thumb bit from iter state
        adr=b_adr; // thumb bit already handled by get_direct...
    }
    add_func_name(fw,name,adr,"_FW");
}

uint32_t process_reg_eventproc_call_loop_check(firmware *fw, iter_state_t *is, uint64_t adr) {
    // check for special case: one of the 2 arg eventprocs is used in loop to register a table

    // using the existing 'is' iterator
    uint32_t tbla = 0;
    int ar = -1;
    // go back at most 10 instructions
    int backtrack = 10;
    if (backtrack > is->ah.count) backtrack = is->ah.count-1;
    disasm_iter_init(fw,is,adr_hist_get(&is->ah,backtrack));
    // search for ldr reg, =address where address is higher in ROM (supposed to be the eventproc table)
    while(1) {
        if (!disasm_iter(fw,is)) break;
        if (is->insn->address >= adr) break;
        if (is->insn->id == ARM_INS_LDR && is->insn->detail->arm.operands[1].type == ARM_OP_MEM) {
            uint32_t u = LDR_PC2val(fw,is->insn);
            if ((u<fw->base+fw->size8) && (u>adr) && (!isASCIIstring(fw,u))) {
                ar = is->insn->detail->arm.operands[0].reg;
                tbla = u;
                break;
            }
        }
    }
    // search for found register appearing later in an add instruction
    while (ar >= 0) {
        if (!disasm_iter(fw,is)) break;
        if (is->insn->address >= adr) break;
        if (is->insn->id == ARM_INS_ADD && is->insn->detail->arm.operands[1].reg == ar) {
            return tbla;
            //printf("found loop eventproc table at 0x%"PRIx64"\n",is->insn->address);
            break;
        }
        if (is->insn->id == ARM_INS_B) {
            // if jumping over data, follow branches
            disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
            disasm_iter(fw,is);
            disasm_iter(fw,is);
            disasm_iter(fw,is);
            if (is->insn->id == ARM_INS_B) {
                disasm_iter_init(fw,is,get_branch_call_insn_target(fw,is));
            }
        }
    }

    return 0;
}

// process a call to an 2 arg event proc registration function
int process_reg_eventproc_call(firmware *fw, iter_state_t *is, __attribute__ ((unused))uint32_t unused) {
    static uint32_t last_adr = 0;

    uint32_t fadr = get_saved_sig_val("RegisterEventProcTable");
    if (fadr) {
        // Ignore call inside 'RegisterEventProcTable'
        if ((is->insn->address - fadr) < 10)
            return 0;
    }

    uint32_t regs[4];
    // get r0, r1, backtracking up to 4 instructions
    if((get_call_const_args(fw,is,4,regs)&3)==3) {
        // TODO follow ptr to verify code, pick up underlying functions
        if(isASCIIstring(fw,regs[0])) {
            last_adr = is->insn->address | is->thumb;
            char *nm=(char *)adr2ptr(fw,regs[0]);
            add_event_proc(fw,nm,regs[1]);
            //add_func_name(fw,nm,regs[1],NULL);
            //printf("eventproc found %s 0x%08x at 0x%"PRIx64"\n",nm,regs[1],is->insn->address);
        } else {
            printf("eventproc name not string at 0x%"PRIx64"\n",is->insn->address);
        }
    } else {
        // Save for later
        uint32_t adr_thumb = is->thumb;
        uint64_t adr = is->insn->address;

        // check for special case: one of the 2 arg eventprocs is used in loop to register a table
        uint32_t tbla = process_reg_eventproc_call_loop_check(fw, is, adr);

        if (!tbla) {
            // Not found - back up further and look for branch instruction that jumps over data
            disasm_iter_init(fw,is,(adr - 220) | adr_thumb);
            disasm_iter(fw,is);
            int i;
            for (i = 0; i < 20; i += 1) {
                if (insn_match_find_next(fw,is,1,match_b)) {
                    uint32_t badr = get_branch_call_insn_target(fw,is);
                    if ((badr - adr) < 8) {
                        tbla = process_reg_eventproc_call_loop_check(fw, is, adr);
                        break;
                    }
                }
            }
        }

        if (tbla) {
            // following is taken from process_eventproc_table_call
            uint32_t *p=(uint32_t*)adr2ptr_with_data(fw,tbla);
            if(p) {
                while(*p) {
                    uint32_t nm_adr=*p;
                    // NULL name = end of table
                    if (!nm_adr) break;
                    if(!isASCIIstring(fw,nm_adr)) {
                        printf("eventproc name not string tbl2 0x%08x 0x%08x\n",tbla,nm_adr);
                        break;
                    }
                    char *nm=(char *)adr2ptr(fw,nm_adr);
                    p++;
                    uint32_t fn=*p;
                    p++;
                    add_event_proc(fw,nm,fn);
                }
            } else {
                printf("eventproc tbl2 not table 0x%08x\n",tbla);
            }
        }
        else {
            // Reset to previous eventproc call and look for branch in the sequence
            //      bl eventproc_call   // previous
            //      ldr/adr r1, func_address
            //      b  xxx
            //          ...
            //   xxx:
            //      adr r0, func_name
            //      bl eventproc_call   // current
            // or
            //      bl eventproc_call   // previous
            //      ldr/adr r1, func_address
            //      adr r0, func_name
            //      b  xxx
            //          ...
            //   xxx:
            //      bl eventproc_call   // current
            disasm_iter_init(fw,is,last_adr);
            disasm_iter(fw,is);
            disasm_iter(fw,is);
            int found = 0;
            if (is->insn->detail->arm.operands[0].reg == ARM_REG_R1) {
                uint8_t* r0_adr = 0;    // ptr to func name
                uint32_t r1_adr = 0;    // func adress
                if (isLDR_PC(is->insn)) {
                    r1_adr = LDR_PC2val(fw,is->insn);
                } else if (isADRx(is->insn)) {
                    r1_adr = ADRx2adr(fw,is->insn);
                }
                if (r1_adr) {
                    disasm_iter(fw, is);
                    if (isADRx(is->insn) && (is->insn->detail->arm.operands[0].reg == ARM_REG_R0)) {
                        r0_adr = adr2ptr(fw,ADR2adr(fw,is->insn));
                        if (insn_match_find_next(fw,is,1,match_b)) {
                            uint32_t badr=get_branch_call_insn_target(fw,is);
                            disasm_iter_init(fw, is, badr);
                            disasm_iter(fw, is);
                        }
                    } else {
                        disasm_iter_init(fw, is, adr_hist_get(&is->ah,1));
                        disasm_iter(fw, is);
                        if (insn_match_find_next(fw,is,1,match_b)) {
                            uint32_t badr=get_branch_call_insn_target(fw,is);
                            disasm_iter_init(fw, is, badr);
                            disasm_iter(fw, is);
                            if (isADRx(is->insn) && (is->insn->detail->arm.operands[0].reg == ARM_REG_R0)) {
                                r0_adr = adr2ptr(fw,ADR2adr(fw,is->insn));
                                disasm_iter(fw, is);
                            }
                        }
                    }
                    if (r0_adr && (is->insn->address == adr)) {
                        add_event_proc(fw,(char*)r0_adr,r1_adr);
                        found = 1;
                    }
                }
            }
            if (!found) {
                printf("failed to get export/register eventproc args at 0x%"PRIx64"\n",adr);
            }
        }
        // restore address in 'is' to avoid infinite loop
        disasm_iter_init(fw,is,adr | adr_thumb);
        disasm_iter(fw,is);
    }
    return 0; // always keep looking
}

// process a call to event proc table registration
int process_eventproc_table_call(firmware *fw, iter_state_t *is, __attribute__ ((unused))uint32_t unused) {
    uint32_t regs[4];
    int foundr0 = 0;
    // get r0, backtracking up to 4 instructions
    foundr0 = get_call_const_args(fw,is,4,regs) & 1;
    if (!foundr0) {
        // case 1: table memcpy'd onto stack
        uint32_t ca = iter_state_adr(is);
        uint32_t sa = adr_hist_get(&is->ah,2);
        uint32_t ta = adr_hist_get(&is->ah,8);
        disasm_iter_set(fw,is,ta);
        int n = 0;
        while(++n<=(8-2))
        {
            disasm_iter(fw,is);
        }
        fw_disasm_iter_single(fw,sa);
        uint32_t adr1 = get_saved_sig_val("j_dry_memcpy");
        uint32_t adr2 = get_branch_call_insn_target(fw,fw->is);
        if (fw->is->insn->id == ARM_INS_BLX && adr1 == adr2) {
            foundr0 = get_call_const_args(fw,is,8-2,regs) & 2;
            if (foundr0) {
                regs[0] = regs[1];
                // printf("eventproc table case1 0x%x found table 0x%x\n",ca,regs[1]);
            }
        }
        // restore iter address
        disasm_iter_init(fw,is,ca);
        disasm_iter(fw,is);
    }
    if(foundr0 && regs[0]) {
        // include tables in RAM data
        uint32_t *p=(uint32_t*)adr2ptr_with_data(fw,regs[0]);
        //printf("found eventproc table 0x%08x\n",regs[0]);
        // if it was a valid address
        if(p) {
            while(*p) {
                uint32_t nm_adr=*p;
                if(!isASCIIstring(fw,nm_adr)) {
                    printf("eventproc name not string tbl 0x%08x 0x%08x\n",regs[0],nm_adr);
                    break;
                }
                char *nm=(char *)adr2ptr(fw,nm_adr);
                p++;
                uint32_t fn=*p;
                p++;
                //printf("found %s 0x%08x\n",nm,fn);
                add_event_proc(fw,nm,fn);
                //add_func_name(fw,nm,fn,NULL);
            }
        } else {
            printf("failed to get *EventProcTable arg 0x%08x at 0x%"PRIx64"\n",regs[0],is->insn->address);
        }
    } else {
        printf("failed to get *EventProcTable r0 at 0x%"PRIx64"\n",is->insn->address);
    }
    return 0;
}

int process_createtask_call(firmware *fw, iter_state_t *is, __attribute__ ((unused))uint32_t unused) {

    uint32_t fadr = get_saved_sig_val("CreateTaskStrictly");
    if (fadr) {
        // Ignore call inside 'CreateTaskStrictly'
        if ((is->insn->address - fadr) < 16)
            return 0;
    }

    fadr = get_saved_sig_val("AdditionAgentRAM_FW");
    if (fadr) {
        // Ignore call inside 'AdditionAgentRAM_FW'
        if ((is->insn->address - fadr) < 150)
            return 0;
    }

    //printf("CreateTask call at %"PRIx64"\n",is->insn->address);
    uint32_t regs[4];
    // get r0 (name) and r3 (entry), backtracking up to 10 instructions
    if((get_call_const_args(fw,is,20,regs)&9)==9) {
        if(isASCIIstring(fw,regs[0])) {
            // TODO
            char *buf=malloc(64);
            char *nm=(char *)adr2ptr(fw,regs[0]);
            sprintf(buf,"task_%s",nm);
            //printf("found %s 0x%08x at 0x%"PRIx64"\n",buf,regs[3],is->insn->address);
            add_func_name(fw,buf,regs[3],NULL);
        } else {
            printf("task name name not string at 0x%"PRIx64"\n",is->insn->address);
        }
    } else {
        printf("failed to get CreateTask args at 0x%"PRIx64"\n",is->insn->address);
    }
    return 0;
}

int save_ptp_handler_func(firmware *fw,uint32_t op,uint32_t handler) {
    if((op >= 0x9000 && op < 0x10000) || (op >= 0x1000 && op < 0x2000)) {
        char *buf=malloc(64);
        const char *nm=get_ptp_op_name(op);
        if(nm) {
            sprintf(buf,"handle_%s",nm);
        } else {
            sprintf(buf,"handle_PTP_OC_0x%04x",op);
        }
        // TODO Canon sometimes uses the same handler for multiple opcodes
        add_func_name(fw,buf,handler,NULL);
    } else {
        return 0;
    }
    return 1;
}
int process_add_ptp_handler_call(firmware *fw, iter_state_t *is, __attribute__ ((unused))uint32_t unused) {
    uint32_t regs[4];
    // get r0 (opcode) and r1 (handler), backtracking up to 8 instructions
    if((get_call_const_args(fw,is,8,regs)&3)==3) {
        //uint32_t op=regs[0];
        if(!save_ptp_handler_func(fw,regs[0],regs[1])) {
            printf("add_ptp_handler op 0x%08x out of range 0x%"PRIx64"\n",regs[0],is->insn->address);
        }
        return 0;
    } else {
        // if above failed, check for opcode table
        arm_reg ptr_reg = ARM_REG_INVALID;
        int i;
        // backtrack until we get to ldrh r0, ...
        for(i=1; i<6; i++) {
            fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
            cs_insn *insn=fw->is->insn;
            if(insn->id != ARM_INS_LDRH) {
                continue;
            }
            if(insn->detail->arm.operands[0].reg != ARM_REG_R0
                || insn->detail->arm.operands[1].mem.base == ARM_REG_PC
                // shift isn't set correctly under capstone 3, not required for current cams
                /*|| insn->detail->arm.operands[1].shift.value != 3*/) {
                continue;
            }
            ptr_reg = insn->detail->arm.operands[1].mem.base;
            //printf("add_ptp_handler ptr_reg %d at 0x%"PRIx64"\n",ptr_reg,insn->address);
            break;
        }
        // didn't find args or anything that looks like table load
        if(ptr_reg == ARM_REG_INVALID) {
            printf("failed to get add_ptp_handler args at 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
        uint32_t op_table=0;
        // backtrack looking for LDR into ptr_reg
        // starting from previous i
        for(; i<20; i++) {
            fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i));
            cs_insn *insn=fw->is->insn;
            if(!isLDR_PC(insn)) {
                continue;
            }
            if((arm_reg)insn->detail->arm.operands[0].reg != ptr_reg) {
                continue;
            }
            // printf("add_ptp_handler LDR PC 0x%08x at 0x%"PRIx64"\n",LDR_PC2val(fw,insn),insn->address);
            uint32_t adr=LDR_PC2val(fw,insn);
            // check loaded address points to expected value (OC GetStorageIDs)
            if(fw_u32(fw,adr) == 0x1004) {
                op_table=adr;
            }
            break;
        }
        if(!op_table) {
            printf("failed to get ptp handler table adr at 0x%"PRIx64"\n",is->insn->address);
            return 0;
        }
        // TODO canon firmware has count in loop that calls add_ptp_handler,
        // but for simplicity just checking for valid opcode with hardcoded max
        for(i=0; i<64; i++) {
            uint32_t op=fw_u32(fw,op_table+i*8);
            uint32_t handler=fw_u32(fw,op_table+i*8+4);
            // fails on op out of range
            if(!save_ptp_handler_func(fw,op,handler)) {
                break;
            }
        }
        return 0;
    }
}

int add_generic_func_match(search_calls_multi_data_t *match_fns,
                            int *match_fn_count,
                            int max_funcs,
                            search_calls_multi_fn fn,
                            uint32_t adr)
{
    if(*match_fn_count >= max_funcs-1) {
        printf("add_generic_func_match: ERROR max_funcs %d reached\n",max_funcs);
        match_fns[max_funcs-1].adr=0;
        match_fns[max_funcs-1].fn=NULL;
        return 0;
    }
    match_fns[*match_fn_count].adr=adr;
    match_fns[*match_fn_count].fn=fn;
    (*match_fn_count)++;
    match_fns[*match_fn_count].adr=0;
    match_fns[*match_fn_count].fn=NULL;
    return 1;
}
#define MAX_GENERIC_FUNCS 16
void add_generic_sig_match(search_calls_multi_data_t *match_fns,
                                int *match_fn_count,
                                search_calls_multi_fn fn,
                                const char *name)
{
    uint32_t adr=get_saved_sig_val(name);
    if(!adr) {
        printf("add_generic_sig_match: missing %s\n",name);
        return;
    }
    add_generic_func_match(match_fns,match_fn_count,MAX_GENERIC_FUNCS,fn,adr);
    char veneer[128];
    sprintf(veneer,"j_%s",name);
    adr=get_saved_sig_val(veneer);
    if(adr) {
        add_generic_func_match(match_fns,match_fn_count,MAX_GENERIC_FUNCS,fn,adr);
    }
}

void find_exception_handlers(firmware *fw, iter_state_t *is)
{
    uint32_t ex_vec = 0;
    // somewhat duplicated from to firmware_load_ng, but it's simple
    if (fw->arch_flags & FW_ARCH_FL_VMSA) {
        const insn_match_t match_mcr_vbar[]={
            // Vector Base Address Register MCR p15, 0, <Rt>, c12, c0, 0 - not present on PMSA
            {MATCH_INS(MCR, 6), {MATCH_OP_PIMM(15),MATCH_OP_IMM(0),MATCH_OP_REG_ANY,MATCH_OP_CIMM(12),MATCH_OP_CIMM(0),MATCH_OP_IMM(0)}},
            {ARM_INS_ENDING}
        };
        // reset to main fw start
        disasm_iter_init(fw, is, fw->base + fw->main_offs + 12 + fw->thumb_default);
        if(!insn_match_find_next(fw,is,4,match_mcr_vbar)) {
            return;
        }
        // back up one
        disasm_iter_init(fw, is, adr_hist_get(&is->ah,1));
        disasm_iter(fw, is);
        // expect ldr to be exception vector address
        ex_vec  = LDR_PC2val(fw,is->insn);
        if(!ex_vec || adr_get_range_type(fw,ex_vec) != ADR_RANGE_ROM) {
            return;
        }
    }
    // both d6 and d7 appear to have an ARM instruction in reset, and thumb in the remaining
    // which appears contrary to arm documentation (ARM DDI 0406C.c (ID051414)
    // On digic 6, Reset appears to be an infinte loop, so must not be expected in any case
    disasm_iter_init(fw, is, ex_vec);
    disasm_iter(fw, is);

    char *names[]={
        "exception_handler_Reset",
        "exception_handler_UndefinedInstruction",
        "exception_handler_SupervisorCall",
        "exception_handler_PrefetchAbort",
        "exception_handler_DataAbort",
        "exception_handler_NotUsed",
        "exception_handler_IRQ",
        "exception_handler_FIQ",
    };

    uint32_t addr=LDR_PC2val(fw,is->insn);
    if(!addr && is->insn->id == ARM_INS_B) {
        addr=get_branch_call_insn_target(fw,is);
    }
    // addr may be 0 (= inf loop at reset vector) but stubs system won't recognize it anyway
    if(addr) {
        add_func_name(fw,names[0],addr,NULL);
    }
    disasm_iter_init(fw, is, ADR_SET_THUMB(ex_vec + 4));
    int i;
    for(i=1; i<8; i++) {
        disasm_iter(fw, is);
        // all seen so far use a thumb ldr.w pc,...
        // "NotUsed" is typically a NOP, which won't get picked up here, but would fall through to IRQ
        addr=LDR_PC2val(fw,is->insn);
        if(addr) {
            add_func_name(fw,names[i],addr,NULL);
        }
    }
}

/*
collect as many calls as possible of functions identified by name, whether or not listed in funcs to find
this does a full disassembly of the entire firmware, which is slow
*/
void find_generic_funcs(firmware *fw) {
    search_calls_multi_data_t match_fns[MAX_GENERIC_FUNCS];

    int match_fn_count=0;

    add_generic_sig_match(match_fns,&match_fn_count,process_reg_eventproc_call,"ExportToEventProcedure_FW");
    add_generic_sig_match(match_fns,&match_fn_count,process_reg_eventproc_call,"RegisterEventProcedure_alt1");
    add_generic_sig_match(match_fns,&match_fn_count,process_reg_eventproc_call,"RegisterEventProcedure_alt2");
    add_generic_sig_match(match_fns,&match_fn_count,process_eventproc_table_call,"RegisterEventProcTable");
    add_generic_sig_match(match_fns,&match_fn_count,process_eventproc_table_call,"UnRegisterEventProcTable");
    add_generic_sig_match(match_fns,&match_fn_count,process_createtask_call,"CreateTaskStrictly");
    if(get_saved_sig_val("CreateTaskStrictly_alt")) {
        add_generic_sig_match(match_fns,&match_fn_count,process_createtask_call,"CreateTaskStrictly_alt");
    }
    add_generic_sig_match(match_fns,&match_fn_count,process_createtask_call,"CreateTask");
    if(get_saved_sig_val("CreateTask_alt")) {
        add_generic_sig_match(match_fns,&match_fn_count,process_createtask_call,"CreateTask_alt");
    }
    add_generic_sig_match(match_fns,&match_fn_count,process_eventproc_table_call,"RegisterEventProcTable_alt");
    add_generic_sig_match(match_fns,&match_fn_count,process_eventproc_table_call,"UnRegisterEventProcTable_alt");
    add_generic_sig_match(match_fns,&match_fn_count,process_add_ptp_handler_call,"add_ptp_handler");

    iter_state_t *is=disasm_iter_new(fw,0);
    disasm_iter_init(fw,is,fw->rom_code_search_min_adr | fw->thumb_default); // reset to start of fw
    fw_search_insn(fw,is,search_disasm_calls_multi,0,match_fns,0);

    int i;
    for(i=0;i<fw->adr_range_count;i++) {
        if(fw->adr_ranges[i].type != ADR_RANGE_RAM_CODE) {
            continue;
        }
        disasm_iter_init(fw,is,fw->adr_ranges[i].start | fw->thumb_default); // reset to start of range
        // check additional veneers, since regions are small and often need to jump back to ROM
        fw_search_insn(fw,is,search_disasm_calls_veneer_multi,0,match_fns,0);
    }

    find_exception_handlers(fw,is);

    disasm_iter_free(is);
}

void find_ctypes(firmware *fw)
{
    static unsigned char ctypes[] =
    {
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x60, 0x60, 0x60, 0x60, 0x60, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x48, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0x10, 0x10, 0x10, 0x10, 0x20
    };

    uint32_t ctypes_matches[10];
    int match_count = find_bytes_all(fw,ctypes,sizeof(ctypes),fw->base,ctypes_matches,10);
    if(!match_count) {
        return;
    }
    if(match_count == 10) {
        fprintf(stderr,"WARNING found 10+ ctypes!\n");
    }
    int i;
    int match_i;
    uint32_t min_adr = 0xFFFFFFFF;
    for(i = 0; i< match_count; i++) {
        // ref should easily be in the first 1M
        uint32_t maxadr = (fw->rom_code_search_max_adr > fw->base + 0x400000)?fw->base + 0x100000:fw->rom_code_search_max_adr;
        uint32_t adr = find_u32_adr_range(fw,ctypes_matches[i],fw->rom_code_search_min_adr,maxadr);
        if(adr && adr < min_adr) {
            min_adr = adr;
            match_i = i;
        }
    }
    if(min_adr == 0xFFFFFFFF) {
        fprintf(stderr,"WARNING cytpes pointer not found, defaulting to first\n");
        match_i = 0;
    }
    save_misc_val("ctypes",ctypes_matches[match_i],0,min_adr);
}

void print_misc_val_makefile(const char *name)
{
    misc_val_t *mv=get_misc_val(name);
    if(!mv) {
        return;
    }
    // TODO legitimate 0 values might be possible, if so can add found bit
    if(!mv->val) {
        bprintf("// %s not found\n",name);
        return;
    }
    bprintf("//   %s = 0x%08x# ",name,mv->val);
    if(mv->offset) {
        bprintf(" (0x%x+0x%x)",mv->base,mv->offset);
    }
    if(mv->ref_adr) {
        bprintf(" Found @0x%08x",mv->ref_adr);
    }
    bprintf("\n");
}


void output_firmware_vals(firmware *fw)
{
    bprintf("// Camera info:\n");
    bprintf("//   Main firmware start: 0x%08x\n",fw->base+fw->main_offs);
    if (fw->dryos_ver == 0)
    {
        bprintf("//   Can't find DRYOS version !!!\n\n");
    } else {
        bprintf("//   DRYOS R%d (%s) @ 0x%08x ref @ 0x%08x\n",
                    fw->dryos_ver,
                    fw->dryos_ver_str,
                    fw->dryos_ver_adr,
                    fw->dryos_ver_ref_adr);
    }
    if (fw->firmware_ver_str == 0)
    {
        bprintf("//   Can't find firmware version !!!\n\n");
    }
    else
    {
        char *c = strrchr(fw->firmware_ver_str,' ') + 1; // points after the last space char
        uint32_t j = ptr2adr(fw,(uint8_t *)fw->firmware_ver_str);
        uint32_t k = j + c - fw->firmware_ver_str;
        if ( (k>=j) && (k<j+32) )
        {
            bprintf("//   %s   // Found @ 0x%08x, \"%s\" @ 0x%08x\n",fw->firmware_ver_str,j,c,k);
        }
        else
        {
            // no space found in string (shouldn't happen)
            bprintf("//   %s   // Found @ 0x%08x, \"%s\" @ 0x%08x\n",fw->firmware_ver_str,j,fw->firmware_ver_str,j);
        }
    }
    if (fw->arch_flags & FW_ARCH_FL_VMSA) {
        bprintf("//   VMSA detected, probably digic >= 7\n");
    }

    bprintf("\n// Values for makefile.inc\n");
    bprintf("//   PLATFORMOSVER = %d\n",fw->dryos_ver);
    if (fw->arch_flags & FW_ARCH_FL_VMSA) {
        bprintf("//   DIGIC = 70\n");
    } else {
        // 6+ exists, but not for known powershot firmware cams
        bprintf("//   DIGIC = 60\n");
    }

    if (fw->memisostart) {
        bprintf("//   MEMISOSTART = 0x%x\n",fw->memisostart);
    } else {
        bprintf("//   MEMISOSTART not found !!!\n");
    }
    if (fw->data_init_start)
    {
        bprintf("//   MEMBASEADDR = 0x%x\n",fw->data_start);
    }
    print_misc_val_makefile("ARAM_HEAP_START");
    print_misc_val_makefile("ARAM_HEAP_SIZE");

    bprintf("\n// Detected address ranges:\n");
    int i;
    for(i=0; i<fw->adr_range_count; i++) {
        if(fw->adr_ranges[i].type == ADR_RANGE_ROM) {
            bprintf("// %-8s 0x%08x - 0x%08x (%7d bytes)\n",
                    adr_range_desc_str(&fw->adr_ranges[i]),
                    fw->adr_ranges[i].start,
                    fw->adr_ranges[i].start+fw->adr_ranges[i].bytes,
                    fw->adr_ranges[i].bytes);
        } else {
            bprintf("// %-8s 0x%08x - 0x%08x copied from 0x%08x (%7d bytes)\n",
                    adr_range_desc_str(&fw->adr_ranges[i]),
                    fw->adr_ranges[i].start,
                    fw->adr_ranges[i].start+fw->adr_ranges[i].bytes,
                    fw->adr_ranges[i].src_start,
                    fw->adr_ranges[i].bytes);
        }
    }
    misc_val_t *mv=get_misc_val("zicokick_values");
    if(mv->blobs) {
        bprintf("\n// Zico Xtensa blobs:\n");
        for(i=0;mv->blobs[i].type != MISC_BLOB_TYPE_NONE;i++) {
            bprintf("// zico_%d 0x%08x - 0x%08x copied from 0x%08x (%7d bytes)\n",
                    i,
                    mv->blobs[i].ram_adr,
                    mv->blobs[i].ram_adr+mv->blobs[i].size,
                    mv->blobs[i].rom_adr,
                    mv->blobs[i].size);
        }

    }
    mv=get_misc_val("omar_init_values");
    if(mv->blobs) {
        bprintf("\n// Omar ARM blobs:\n");
        for(i=0;mv->blobs[i].type != MISC_BLOB_TYPE_NONE;i++) {
            bprintf("// omar_%d 0x%08x - 0x%08x copied from 0x%08x (%7d bytes)\n",
                    i,
                    mv->blobs[i].ram_adr,
                    mv->blobs[i].ram_adr+mv->blobs[i].size,
                    mv->blobs[i].rom_adr,
                    mv->blobs[i].size);
        }
    }
    if(fw->dryos_ver_count) {
        bprintf("\n// Found DryOS versions:\n");
        for(i=0;i<(int)fw->dryos_ver_count;i++) {
            bprintf("// 0x%08x %s \"%s\"\n",
                fw->dryos_ver_list[i], (fw->dryos_ver_list[i] == fw->dryos_ver_adr) ? "main ":"other",
                (char *)adr2ptr(fw,fw->dryos_ver_list[i]));
        }
        add_blankline();
    }
    add_blankline();

    // check if CreateTask is in ROM, offer CreateTask_low if it's in RAM
    sig_entry_t * ct = find_saved_sig("hook_CreateTask");
    if(ct && adr_get_range_type(fw,ct->val) != ADR_RANGE_RAM_CODE) {
        bprintf("// CreateTask is not in RAM code\n");
        ct->flags |= UNUSED;
        sig_entry_t * ctl = find_saved_sig("CreateTask_low");
        if(ctl && adr_get_range_type(fw,ctl->val) == ADR_RANGE_RAM_CODE) {
            bprintf("// use hook_CreateTask_low instead\n");
            ctl->flags &= ~UNUSED;
            sig_entry_t * hctl = find_saved_sig("hook_CreateTask_low");
            hctl->flags &= ~UNUSED;
        }
        add_blankline();
    }
}

// print platform.h define, if not default value
void print_platform_misc_val_undef(const char *name, uint32_t def)
{
    misc_val_t *mv=get_misc_val(name);
    if(mv && mv->val && mv->val != def) {
        bprintf("//#undef  %s\n",name);
        bprintf("//#define %s  0x%08x // Found @0x%08x\n",name,mv->val,mv->ref_adr);
    }
}

void output_platform_vals(firmware *fw) {
    bprintf("// Values below go in 'platform_camera.h':\n");
    bprintf("//#define CAM_DRYOS         1\n");
    if (fw->dryos_ver >= 39)
        bprintf("//#define CAM_DRYOS_2_3_R39 1 // Defined for cameras with DryOS version R39 or higher\n");
    if (fw->dryos_ver >= 47)
        bprintf("//#define CAM_DRYOS_2_3_R47 1 // Defined for cameras with DryOS version R47 or higher\n");
    if (fw->dryos_ver >= 59)
        bprintf("//#define CAM_DRYOS_2_3_R59 1 // Defined for cameras with DryOS version R59 or higher\n");

    if(get_misc_val_value("CAM_IS_ILC")) {
        bprintf("//#define CAM_ILC 1 // Camera is interchangeable lens\n");
    }

    if(get_misc_val_value("CAM_HAS_WIFI")) {
        bprintf("//#define CAM_HAS_WIFI 1 // Firmware has wifi support (only define if camera has hardware)\n");
    }


    print_platform_misc_val_undef("CAM_UNCACHED_BIT",0x10000000);

    if(get_misc_val_value("CAM_HAS_ND_FILTER")) {
        bprintf("//#define CAM_HAS_ND_FILTER 1 // Camera has ND filter\n");
    } else {
        bprintf("//#undef CAM_HAS_ND_FILTER // Camera does not have an ND filter\n");
    }
    if(get_misc_val_value("CAM_HAS_IRIS_DIAPHRAGM")) {
        bprintf("// Camera has an iris (CAM_HAS_IRIS_DIAPHRAGM default)\n");
    } else {
        bprintf("//#undef CAM_HAS_IRIS_DIAPHRAGM // Camera does not have an iris\n");
    }

    add_blankline();
}

void output_propcases(firmware *fw) {

    uint32_t used=0;
    uint32_t hits[KNOWN_PROPSET_COUNT];
    const uint32_t ps_offset = 6;

    memset(hits, 0, KNOWN_PROPSET_COUNT*sizeof(uint32_t));

    bprintf("// Known propcases\n");

    int n = 0;
    while (knownprops[n].name) {
        used += knownprops[n].use>0?1:0;
        if (knownprops[n].id >= 0)
        {
            if (knownprops[n].use)
            {
                if (knownprops[n].id == knownprops[n].id_ps6) hits[6-ps_offset] += 1;
                if (knownprops[n].id == knownprops[n].id_ps7) hits[7-ps_offset] += 1;
                if (knownprops[n].id == knownprops[n].id_ps8) hits[8-ps_offset] += 1;
                if (knownprops[n].id == knownprops[n].id_ps9) hits[9-ps_offset] += 1;
                if (knownprops[n].id == knownprops[n].id_ps10) hits[10-ps_offset] += 1;
                if (knownprops[n].id == knownprops[n].id_ps11) hits[11-ps_offset] += 1;
                if (knownprops[n].id == knownprops[n].id_ps12) hits[12-ps_offset] += 1;
                if (knownprops[n].id == knownprops[n].id_ps13) hits[13-ps_offset] += 1;
            }
            if (knownprops[n].use == 1)
            {
                bprintf("// #define %s %i\n", knownprops[n].name, knownprops[n].id);
            }
            else
            {
                // propcases not used by CHDK, name may be made up
                bprintf("// //      %s %i\n", knownprops[n].name, knownprops[n].id);
            }
        }
        else
        {
            bprintf("//         %s not found\n", knownprops[n].name);
        }
        n++;
    }

    bprintf("// Guessed propset: ");
    int m = 0;
    uint32_t fmax = 0;
    int okay = 0;
    for (n=0; n<KNOWN_PROPSET_COUNT; n++)
    {
        if (hits[n] == used)
        {
            if (m) bprintf(", ");
            bprintf("%i", n+ps_offset);
            if (fw->sv->propset == n+ps_offset) okay = 1; // if the propset equals to (one of) the complete propset matches
            m += 1;
        }
        if (hits[n] > fmax) fmax = hits[n];
    }
    if (m == 0)
    {
        bprintf("uncertain (%i of %u match), closest to ",fmax,used);
        for (n=0; n<KNOWN_PROPSET_COUNT; n++)
        {
            if (hits[n] == fmax)
            {
                if (m) bprintf(", ");
                bprintf("%i", n+ps_offset);
                if (fw->sv->propset == n+ps_offset) okay = 1; // if the propset equals to (one of) the most complete propset matches
                m += 1;
            }
        }
    }
    bprintf("\n");
    if (!okay && fw->sv->propset>0)
    {
        // only shown when there's a clear mismatch
        bprintf("// Port's propset (%i) may be set incorrectly\n", fw->sv->propset);
    }

    add_blankline();
}

void output_exmem_types(firmware *fw)
{

    misc_val_t *ett=get_misc_val("exmem_types_table");
    misc_val_t *etc=get_misc_val("exmem_type_count");
    if (ett->val == 0 || etc->val == 0) {
        return;
    }
    bprintf("// EXMEM types:\n");
    uint32_t n;
    for (n=0; n<etc->val; n++) {
        char *extyp = (char*)adr2ptr(fw, fw_u32(fw,ett->val+n*4));
        bprintf("// %s %i\n", extyp, n);
    }
    add_blankline();
}

void print_misc_val_comment(const char *name)
{
    misc_val_t *mv=get_misc_val(name);
    if(!mv) {
        return;
    }
    // TODO legitimate 0 values might be possible, if so can add found bit
    if(!mv->val) {
        bprintf("// %s not found\n",name);
        return;
    }
    bprintf("// %s 0x%08x",name,mv->val);
    if(mv->offset) {
        bprintf(" (0x%x+0x%x)",mv->base,mv->offset);
    }
    if(mv->ref_adr) {
        bprintf(" Found @0x%08x",mv->ref_adr);
    }
    bprintf("\n");
}

typedef struct {
    int reg;
    uint32_t bit;
    uint32_t ev;
    uint32_t raw_info;
    int no_invert;
} physw_table_entry_t;

void get_physw_table_entry(firmware *fw, uint32_t adr, physw_table_entry_t *vals)
{
    uint32_t info=fw_u32(fw,adr);
    vals->raw_info=info;
    vals->ev=fw_u32(fw,adr+4);
    // taken from finsig_dryos print_physw_raw_vals
    vals->reg=(info >>5) & 7;
    vals->bit=(1 << (info & 0x1f));
    // vals->no_invert=(info >> 16) & 1;
    vals->no_invert=((info&0xff0000)==0x10000)?1:0;
}
uint32_t find_physw_table_entry(firmware *fw, uint32_t tadr, int tcount, uint32_t ev)
{
    int i;
    for(i=0; i<tcount; i++,tadr += 8) {
        if(fw_u32(fw,tadr+4) == ev) {
            return tadr;
        }
    }
    return 0;
}
// look for the first invalid looking entry
uint32_t find_physw_table_max(firmware *fw, uint32_t tadr, int max_count)
{
    int i;
    for(i=0; i<max_count; i++,tadr += 8) {
        physw_table_entry_t v;
        get_physw_table_entry(fw,tadr,&v);
        if(v.raw_info == 0 || v.raw_info == 0xFFFFFFFF || v.reg > 2) {
            return i;
        }
        // TODO could check that no event numbers (except -1) are repeated
    }
    return max_count;
}
void write_physw_event_table_dump(firmware *fw, uint32_t tadr, int tcount)
{
    FILE *f=fopen("physw_bits.txt","w");
    if(!f) {
        return;
    }
    fprintf(f,"physw_event_table dump (%d entries printed, may not all be valid)\n",tcount);
    fprintf(f,"address    info       event      index bit        non-inverted\n");
    int i;
    physw_table_entry_t v;

    for(i=0; i<tcount; i++,tadr += 8) {
        get_physw_table_entry(fw,tadr,&v);
        fprintf(f,"0x%08x 0x%08x 0x%08x %-5d 0x%08x %d\n",tadr,v.raw_info,v.ev,v.reg,v.bit,v.no_invert);
    }
    fclose(f);
}
void print_kval(firmware *fw, uint32_t tadr, int tcount, uint32_t ev, const char *name, const char *sfx)
{
    uint32_t adr=find_physw_table_entry(fw,tadr,tcount,ev);
    if(!adr) {
        return;
    }
    physw_table_entry_t v;
    get_physw_table_entry(fw,adr,&v);

    char fn[100], rn[100];
    strcpy(fn,name); strcat(fn,sfx);
    strcpy(rn,name); strcat(rn,"_IDX");

    bprintf("//#define %-20s0x%08x // Found @0x%08x, levent 0x%x%s\n",fn,v.bit,adr,v.ev,v.no_invert?" (non-inverted logic)":"");
    bprintf("//#define %-20s%d\n",rn,v.reg);

}

// key stuff copied from finsig_dryos.c
typedef struct {
    int         reg;
    uint32_t    bits;
    char        nm[32];
    uint32_t    fadr;
    uint32_t    ev;
    int         inv;
} kinfo;

int     kmask[3];
kinfo   key_info[100];
int     kcount = 0;
uint32_t kshutter_min_bits = 0xFFFFFFFF;

void add_kinfo(int r, uint32_t b, const char *nm, uint32_t adr, uint32_t ev, int inv)
{
    key_info[kcount].reg = r;
    key_info[kcount].bits = b;
    strcpy(key_info[kcount].nm, nm);
    key_info[kcount].fadr = adr;
    key_info[kcount].ev = ev;
    key_info[kcount].inv = inv;
    kcount++;
    kmask[r] |= b;
    if ((ev <= 1) && (b < kshutter_min_bits)) kshutter_min_bits = b;
}

uint32_t add_kmval(firmware *fw, uint32_t tadr, __attribute__ ((unused))int tsiz, int tlen, uint32_t ev, const char *name, uint32_t xtra)
{
    uint32_t adr=find_physw_table_entry(fw,tadr,tlen,ev);
    if(!adr) {
        return 0;
    }
    physw_table_entry_t v;
    get_physw_table_entry(fw,adr,&v);

    add_kinfo(v.reg,v.bit|xtra,name,adr,v.ev,(v.no_invert)?0:1);
    return v.bit;
}

int kinfo_compare(const kinfo *p1, const kinfo *p2)
{
    if (p1->reg > p2->reg)
    {
        return 1;
    }
    else if (p1->reg < p2->reg)
    {
        return -1;
    }
    if ((p1->ev <= 1) && (p2->ev <= 1))    // output shutter entries in reverse order
    {
        if (p1->bits > p2->bits)
        {
            return -1;
        }
        else if (p1->bits < p2->bits)
        {
            return 1;
        }
    }
    // if one entry is shutter then compare to min shutter bits
    if (p1->ev <= 1)
    {
        if (kshutter_min_bits > p2->bits)
        {
            return 1;
        }
        else if (kshutter_min_bits < p2->bits)
        {
            return -1;
        }
    }
    if (p2->ev <= 1)
    {
        if (p1->bits > kshutter_min_bits)
        {
            return 1;
        }
        else if (p1->bits < kshutter_min_bits)
        {
            return -1;
        }
    }
    if (p1->bits > p2->bits)
    {
        return 1;
    }
    else if (p1->bits < p2->bits)
    {
        return -1;
    }

    return 0;
}

void print_kmvals()
{
    qsort(key_info, kcount, sizeof(kinfo), (void*)kinfo_compare);

    bprintf("//KeyMap keymap[] = {\n");

    int k;
    for (k=0; k<kcount; k++)
    {
        bprintf("//    { %d, %-20s,0x%08x }, // Found @0x%08x, levent 0x%02x%s\n",key_info[k].reg,key_info[k].nm,key_info[k].bits,key_info[k].fadr,key_info[k].ev,(key_info[k].inv==0)?"":" (uses inverted logic in physw_status)");
    }

    bprintf("//    { 0, 0, 0 }\n//};\n");
}

void do_km_vals(firmware *fw, uint32_t tadr,int tsiz,int tlen)
{
    uint32_t key_half = add_kmval(fw,tadr,tsiz,tlen,0,"KEY_SHOOT_HALF",0);
    add_kmval(fw,tadr,tsiz,tlen,1,"KEY_SHOOT_FULL",key_half);
    add_kmval(fw,tadr,tsiz,tlen,1,"KEY_SHOOT_FULL_ONLY",0);

    add_kmval(fw,tadr,tsiz,tlen,0x101,"KEY_PLAYBACK",0);
    add_kmval(fw,tadr,tsiz,tlen,0x100,"KEY_POWER",0);

    // mostly copied from finsig_dryos, with < r52 stuff removed
    if (fw->dryos_ver == 52)  // unclear if this applies any other ver
    {
        add_kmval(fw,tadr,tsiz,tlen,3,"KEY_ZOOM_IN",0);
        add_kmval(fw,tadr,tsiz,tlen,4,"KEY_ZOOM_OUT",0);
        add_kmval(fw,tadr,tsiz,tlen,6,"KEY_UP",0);
        add_kmval(fw,tadr,tsiz,tlen,7,"KEY_DOWN",0);
        add_kmval(fw,tadr,tsiz,tlen,8,"KEY_LEFT",0);
        add_kmval(fw,tadr,tsiz,tlen,9,"KEY_RIGHT",0);
        add_kmval(fw,tadr,tsiz,tlen,0xA,"KEY_SET",0);
        add_kmval(fw,tadr,tsiz,tlen,0xB,"KEY_MENU",0);
        add_kmval(fw,tadr,tsiz,tlen,0xC,"KEY_DISPLAY",0);
        add_kmval(fw,tadr,tsiz,tlen,0x12,"KEY_HELP",0);
        add_kmval(fw,tadr,tsiz,tlen,0x19,"KEY_ERASE",0);
        add_kmval(fw,tadr,tsiz,tlen,2,"KEY_VIDEO",0);
//        add_kmval(fw,tadr,tsiz,tlen,18,"KEY_SHORTCUT",0);
    }
    else if (fw->dryos_ver < 54)
    {
        add_kmval(fw,tadr,tsiz,tlen,2,"KEY_ZOOM_IN",0);
        add_kmval(fw,tadr,tsiz,tlen,3,"KEY_ZOOM_OUT",0);
        add_kmval(fw,tadr,tsiz,tlen,4,"KEY_UP",0);
        add_kmval(fw,tadr,tsiz,tlen,5,"KEY_DOWN",0);
        add_kmval(fw,tadr,tsiz,tlen,6,"KEY_LEFT",0);
        add_kmval(fw,tadr,tsiz,tlen,7,"KEY_RIGHT",0);
        add_kmval(fw,tadr,tsiz,tlen,8,"KEY_SET",0);
        add_kmval(fw,tadr,tsiz,tlen,9,"KEY_MENU",0);
        add_kmval(fw,tadr,tsiz,tlen,0xA,"KEY_DISPLAY",0);
    }
    else if (fw->dryos_ver < 55)
    {
        add_kmval(fw,tadr,tsiz,tlen,3,"KEY_ZOOM_IN",0);
        add_kmval(fw,tadr,tsiz,tlen,4,"KEY_ZOOM_OUT",0);
        add_kmval(fw,tadr,tsiz,tlen,6,"KEY_UP",0);
        add_kmval(fw,tadr,tsiz,tlen,7,"KEY_DOWN",0);
        add_kmval(fw,tadr,tsiz,tlen,8,"KEY_LEFT",0);
        add_kmval(fw,tadr,tsiz,tlen,9,"KEY_RIGHT",0);
        add_kmval(fw,tadr,tsiz,tlen,0xA,"KEY_SET",0);
        add_kmval(fw,tadr,tsiz,tlen,0xE,"KEY_MENU",0);
        add_kmval(fw,tadr,tsiz,tlen,2,"KEY_VIDEO",0);
        add_kmval(fw,tadr,tsiz,tlen,0xD,"KEY_DISPLAY",0);
        add_kmval(fw,tadr,tsiz,tlen,0x103,"KEY_WIFI",0);
// framing assist / shortuct
//        add_kmval(fw,tadr,tsiz,tlen,0xF,"KEY_",0);
    }
    else if (fw->dryos_ver < 59)
    {
        add_kmval(fw,tadr,tsiz,tlen,3,"KEY_ZOOM_IN",0);
        add_kmval(fw,tadr,tsiz,tlen,4,"KEY_ZOOM_OUT",0);
        add_kmval(fw,tadr,tsiz,tlen,6,"KEY_UP",0);
        add_kmval(fw,tadr,tsiz,tlen,7,"KEY_DOWN",0);
        add_kmval(fw,tadr,tsiz,tlen,8,"KEY_LEFT",0);
        add_kmval(fw,tadr,tsiz,tlen,9,"KEY_RIGHT",0);
        add_kmval(fw,tadr,tsiz,tlen,0xA,"KEY_SET",0);
        add_kmval(fw,tadr,tsiz,tlen,0x14,"KEY_MENU",0);
        add_kmval(fw,tadr,tsiz,tlen,2,"KEY_VIDEO",0);
        add_kmval(fw,tadr,tsiz,tlen,0xD,"KEY_DISPLAY",0);
        add_kmval(fw,tadr,tsiz,tlen,0x103,"KEY_WIFI",0);
// framing assist, shortcut
//        add_kmval(fw,tadr,tsiz,tlen,0xF,"KEY_",0);
    }
    else
    {
        add_kmval(fw,tadr,tsiz,tlen,3,"KEY_ZOOM_IN",0);
        add_kmval(fw,tadr,tsiz,tlen,4,"KEY_ZOOM_OUT",0);
        add_kmval(fw,tadr,tsiz,tlen,6,"KEY_UP",0);
        add_kmval(fw,tadr,tsiz,tlen,7,"KEY_DOWN",0);
        add_kmval(fw,tadr,tsiz,tlen,8,"KEY_LEFT",0);
        add_kmval(fw,tadr,tsiz,tlen,9,"KEY_RIGHT",0);
        add_kmval(fw,tadr,tsiz,tlen,0xA,"KEY_SET",0);
        add_kmval(fw,tadr,tsiz,tlen,0x15,"KEY_MENU",0);
        add_kmval(fw,tadr,tsiz,tlen,2,"KEY_VIDEO",0);
        add_kmval(fw,tadr,tsiz,tlen,0xB,"KEY_ERASE",0); // also framing assist etc
        add_kmval(fw,tadr,tsiz,tlen,0x103,"KEY_WIFI",0);
    }

    bprintf("\n// Keymap values for kbd.c. Additional keys may be present, only common values included here.\n");
    bprintf("// WARNING: Key name / function may vary! Values for unknown DryOS versions should not be trusted!\n");
    print_kmvals();
}
void output_physw_vals(firmware *fw) {
    print_misc_val_comment("physw_event_table");
    uint32_t physw_tbl=get_misc_val_value("physw_event_table");
    if(!physw_tbl) {
        return;
    }
    int physw_tbl_len=find_physw_table_max(fw,physw_tbl,50);
    write_physw_event_table_dump(fw,physw_tbl,physw_tbl_len);

    bprintf("// Values below go in 'platform_kbd.h':\n");
    if (fw->dryos_ver >= 58)
    {
        // Event ID's have changed in DryOS 58 **********
        print_kval(fw,physw_tbl,physw_tbl_len,0x30A,"SD_READONLY","_FLAG");
        print_kval(fw,physw_tbl,physw_tbl_len,0x302,"USB","_MASK");
        print_kval(fw,physw_tbl,physw_tbl_len,0x305,"BATTCOVER","_FLAG");
        print_kval(fw,physw_tbl,physw_tbl_len,0x304,"HOTSHOE","_FLAG");
        print_kval(fw,physw_tbl,physw_tbl_len,0x300,"ANALOG_AV","_FLAG");
    }
    else
    {
        print_kval(fw,physw_tbl,physw_tbl_len,0x20A,"SD_READONLY","_FLAG");
        print_kval(fw,physw_tbl,physw_tbl_len,0x202,"USB","_MASK");
        print_kval(fw,physw_tbl,physw_tbl_len,0x205,"BATTCOVER","_FLAG");
        print_kval(fw,physw_tbl,physw_tbl_len,0x204,"HOTSHOE","_FLAG");
        print_kval(fw,physw_tbl,physw_tbl_len,0x200,"ANALOG_AV","_FLAG");
    }
    do_km_vals(fw,physw_tbl,2,physw_tbl_len);

    add_blankline();
}

/*
typedef struct
{
    uint16_t mode;
    char *nm;
} ModeMapName;


// TODO numbers changed after g1xmk2
// M=32770
ModeMapName mmnames[] = {
    { 32768,"MODE_AUTO" },
    { 32769,"MODE_M" },
    { 32770,"MODE_AV" },
    { 32771,"MODE_TV" },
    { 32772,"MODE_P" },

    { 65535,"" }
};

char* mode_name(uint16_t v)
{
    int i;
    for (i=0; mmnames[i].mode != 65535; i++)
    {
        if (mmnames[i].mode == v)
            return mmnames[i].nm;
    }

    return "";
}
*/


void output_modemap(firmware *fw) {
    print_misc_val_comment("canon_mode_list");
    bprintf("// Check modemap values from 'platform/CAMERA/shooting.c':\n");
    misc_val_t *mv=get_misc_val("canon_mode_list");
    if(!mv) {
        add_blankline();
        return;
    }
    int i;
    uint32_t adr=mv->val;
    int bad=0;
    for(i=0; i<50; i++,adr+=2) {
        uint16_t *pv=(uint16_t*)adr2ptr(fw,adr);
        if(!pv) {
            break;
        }
        if(*pv==0xFFFF) {
            break;
        }
        osig *m = find_sig_val(fw->sv->modemap, *pv);
        if (!m) {
            bprintf("// %5hu  0x%04hx In firmware but not in current modemap",*pv,*pv);
            /*
            char *s = mode_name(*pv);
            if (strcmp(s,"") != 0) {
                bprintf(" (%s)",s);
            }
            */
            bprintf("\n");
            bad++;
        } else {
//            bprintf("// %5hu  0x%04hx %s\n", *pv,*pv,m->nm);
            m->pct = 100;
        }
    }
    osig *m = fw->sv->modemap;
    while (m)
    {
        if (m->pct != 100)    // not matched above?
        {
            bprintf("// Current modemap entry not found in firmware - %-24s %5d\n",m->nm,m->val);
            bad++;
        }
        m = m->nxt;
    }
    if (bad == 0)
    {
        bprintf("// No problems found with modemap table.\n");
    }

    add_blankline();
}

// copied from finsig_dryos
int compare_sig_names(const sig_entry_t **p1, const sig_entry_t **p2)
{
    int rv = strcasecmp((*p1)->name, (*p2)->name);     // Case insensitive
    if (rv != 0)
        return rv;
    rv = strcmp((*p1)->name, (*p2)->name);          // Case sensitive (if equal with insensitive test)
    if (rv != 0)
        return rv;
    if ((*p1)->val < (*p2)->val)
        return -1;
    else if ((*p1)->val > (*p2)->val)
        return 1;
    return 0;
}

int compare_func_addresses(const sig_entry_t **p1, const sig_entry_t **p2)
{
    if ((*p1)->val < (*p2)->val)
        return -1;
    else if ((*p1)->val > (*p2)->val)
        return 1;
    return compare_sig_names(p1,p2);
}

void write_funcs(firmware *fw, char *filename, sig_entry_t *fns[], int (*compare)(const sig_entry_t **p1, const sig_entry_t **p2))
{
    int k;

    qsort(fns, next_sig_entry, sizeof(sig_entry_t*), (void*)compare);

    FILE *out_fp = fopen(filename, "w");
    for (k=0; k<next_sig_entry; k++)
    {
        if (strncmp(fns[k]->name,"hook_",5) == 0) {
            continue;
        }
        if (fns[k]->val != 0)
        {
            if (fns[k]->flags & (BAD_MATCH))
            {
                osig* ostub2 = find_sig(fw->sv->stubs,fns[k]->name);
                if (ostub2 && ostub2->val)
                    fprintf(out_fp, "0x%08x,%s,(stubs_entry_2.s)\n", ostub2->val, fns[k]->name);
            }
            else
                fprintf(out_fp, "0x%08x,%s\n", fns[k]->val, fns[k]->name);
        }
#ifdef LIST_IMPORTANT_FUNCTIONS
        else if (fns[k]->flags & LIST_ALWAYS)
        {
            // helps development by listing important functions even when not found
            fprintf(out_fp, "0,%s,(NOT FOUND)\n", fns[k]->name);
        }
#endif
    }
    fclose(out_fp);
}
// end copy finsig_dryos
void write_func_lists(firmware *fw) {
    sig_entry_t *fns[MAX_SIG_ENTRY];
    int k;
    for (k=0; k<next_sig_entry; k++)
        fns[k] = &sig_names[k];

    write_funcs(fw, "funcs_by_name.csv", fns, compare_sig_names);
    write_funcs(fw, "funcs_by_address.csv", fns, compare_func_addresses);
}

void print_other_stubs_min(firmware *fw, const char *name, uint32_t fadr, uint32_t atadr)
{
    osig *o = find_sig(fw->sv->stubs_min,name);
    if (o)
    {
        bprintf("//DEF(%-40s,0x%08x) // Found @0x%08x",name,fadr,atadr);
        if (fadr != o->val)
        {
            bprintf(", ** != ** stubs_min = 0x%08x (%s)",o->val,o->sval);
        }
        else
        {
            bprintf(",          stubs_min = 0x%08x (%s)",o->val,o->sval);
        }
    }
    else
    {
        bprintf("DEF(%-40s,0x%08x) // Found @0x%08x",name,fadr,atadr);
    }
    bprintf("\n");
}

void print_stubs_min_def(firmware *fw, misc_val_t *sig)
{
    if(sig->flags & MISC_VAL_NO_STUB) {
        return;
    }
    // find best match and report results
    osig* ostub2=find_sig(fw->sv->stubs_min,sig->name);
    const char *ostub_src = "stubs_min";

    if(!ostub2) {
        ostub2=find_sig(fw->sv->stubs,sig->name);
        ostub_src="stubs_entry_2";
    }

    const char *macro = "DEF";
    if(sig->flags & MISC_VAL_DEF_CONST) {
        macro="DEF_CONST";
    }

    if (ostub2)
    {
        bprintf("//%s(%-34s,0x%08x)",macro,sig->name,sig->val);
        if (sig->val != ostub2->val)
        {
            bprintf(", ** != ** %s = 0x%08x (%s)",ostub_src,ostub2->val,ostub2->sval);
        }
        else
        {
            bprintf(",          %s = 0x%08x (%s)",ostub_src,ostub2->val,ostub2->sval);
        }
    }
    else if(sig->base || sig->offset)
    {
        bprintf("%s(%-34s,0x%08x)",macro,sig->name,sig->val);
        if(sig->offset || sig->ref_adr) {
            bprintf(" //");
            if(sig->offset) {
                bprintf(" (0x%x+0x%x)",sig->base,sig->offset);
            }
            if(sig->ref_adr) {
                bprintf(" Found @0x%08x",sig->ref_adr);
            }
        }
    }
    else
    {
        if (sig->flags & MISC_VAL_OPTIONAL) return;
        bprintf("// %s not found",sig->name);
    }
    bprintf("\n");
}

// Search for other things that go in 'stubs_min.S'
void find_other_stubs_min(firmware *fw)
{
    int k,k1;

    out_hdr = 1;

    // focus_len_table
    if (fw->sv->min_focus_len != 0)
    {
        int found = 0, pos = 0, len = 0, size = 0;
        for (k=0; k<fw->size32; k++)
        {
            if (fw->buf32[k] == fw->sv->min_focus_len)
            {
                int mul = 1;
                if ((fw->buf32[k+1] == 100) && (fw->buf32[k+2] == 0)) mul = 3;
                if ((fw->buf32[k+1] == 100) && (fw->buf32[k+2] != 0)) mul = 2;
                if ((fw->buf32[k+1] ==   0) && (fw->buf32[k+2] != 0)) mul = 2;
                for (k1 = k + mul; (k1 < fw->size32) && (fw->buf32[k1] > fw->buf32[k1-mul]) && (fw->buf32[k1] > fw->sv->min_focus_len) && (fw->buf32[k1] < fw->sv->max_focus_len); k1 += mul) ;
                if (fw->buf32[k1] == fw->sv->max_focus_len)
                {
                    if ((found == 0) || ((size < mul) && (len < ((k1 - k) / mul) + 1)))
                    {
                        found = 1;
                        pos = k;
                        len = ((k1 - k) / mul) + 1;
                        size = mul;
                    }
                }
            }
        }
        if (found == 1)
        {
            uint32_t adr = fw->base + (pos << 2);
            bprintf("// focus_len_table contains zoom focus lengths for use in 'get_focal_length' (main.c).\n");
            if (size == 1)
                bprintf("// each entry contains 1 int value, which is the the zoom focus length.\n",size);
            else
                bprintf("// each entry contains %d int value(s), the first is the zoom focus length.\n",size);
            bprintf("// there are %d entries in the table - set NUM_FL to %d\n",len,len);
            print_other_stubs_min(fw,"focus_len_table",adr,adr);
        }
    }
}

// Output match results for function
// matches stuff butchered out for now, just using value in sig_names table
void print_results(firmware *fw, sig_entry_t *sig)
{
    int i;
    int err = 0;
    char line[500] = "";

    if (sig->flags & DONT_EXPORT) {
        return;
    }

    if ((sig->flags & DONT_EXPORT_ILC) && get_misc_val_value("CAM_IS_ILC")) {
        return;
    }

    // find best match and report results
    osig* ostub2 = find_sig(fw->sv->stubs,sig->name);

    if (ostub2 && (sig->val != ostub2->val))
    {
        if (ostub2->type != TYPE_IGNORE)
        {
            err = 1;
            sig->flags |= BAD_MATCH;
        }
    }
    else
    {
        if (sig->flags & UNUSED) return;
    }

    // write to header (if error) or body buffer (no error)
    out_hdr = err;

    char *macro = "NHSTUB";
    if (sig->flags & ARM_STUB) {
        macro = "NHSTUB2";
    }
    if (strncmp(sig->name,"task_",5) == 0 ||
        strncmp(sig->name,"hook_",5) == 0) macro = "   DEF";

    if (!sig->val && !ostub2)
    {
        if (sig->flags & OPTIONAL) return;
        char fmt[51] = "";
        sprintf(fmt, "// ERROR: %%s is not found. %%%ds//--- --- ", (int)(34-strlen(sig->name)));
        sprintf(line+strlen(line), fmt, sig->name, "");
    }
    else
    {
        if (ostub2 || (sig->flags & UNUSED))
            sprintf(line+strlen(line),"//%s(%-37s,0x%08x) //%3d ", macro, sig->name, sig->val, 0);
        else
            sprintf(line+strlen(line),"%s(%-39s,0x%08x) //%3d ", macro, sig->name, sig->val, 0);

        /*
        if (matches->fail > 0)
            sprintf(line+strlen(line),"%2d%% ", matches->success*100/(matches->success+matches->fail));
        else
            */
            sprintf(line+strlen(line),"    ");
    }

    if (ostub2)
    {
        if (ostub2->type == TYPE_IGNORE)
            sprintf(line+strlen(line),"       Overridden");
        else if (sig->val == ostub2->val)
            sprintf(line+strlen(line),"       == 0x%08x    ",ostub2->val);
        else {
            // if both have same value check if differs only by veneer
            if(sig->val && ostub2->val) {
                fw_disasm_iter_single(fw,ostub2->val);
                if(get_direct_jump_target(fw,fw->is) == sig->val) {
                    sprintf(line+strlen(line)," <-veneer 0x%08x    ",ostub2->val);
                } else {
                    fw_disasm_iter_single(fw,sig->val);
                    if(get_direct_jump_target(fw,fw->is) == ostub2->val) {
                        sprintf(line+strlen(line)," veneer-> 0x%08x    ",ostub2->val);
                    } else {
                        sprintf(line+strlen(line),"   *** != 0x%08x    ",ostub2->val);
                    }
                }
            } else {
                sprintf(line+strlen(line),"   *** != 0x%08x    ",ostub2->val);
            }
        }
    }
    else
        sprintf(line+strlen(line),"                        ");

    for (i=strlen(line)-1; i>=0 && line[i]==' '; i--) line[i] = 0;
    bprintf("%s\n",line);

    /*
    for (i=1;i<count && matches[i].fail==matches[0].fail;i++)
    {
        if (matches[i].ptr != matches->ptr)
        {
            bprintf("// ALT: %s(%s, 0x%x) // %d %d/%d\n", macro, curr_name, matches[i].ptr, matches[i].sig, matches[i].success, matches[i].fail);
        }
    }
    */
}

void write_stubs(firmware *fw,int max_find_func) {
    int k;
    bprintf("// Values below can be overridden in 'stubs_min.S':\n");
    misc_val_t *stub_min=misc_vals;
    while(stub_min->name) {
        print_stubs_min_def(fw,stub_min);
        stub_min++;
    }

    find_other_stubs_min(fw);

    add_blankline();

    for (k = 0; k < max_find_func; k++)
    {
        print_results(fw,&sig_names[k]);
    }
}

int main(int argc, char **argv)
{
    clock_t t1 = clock();

    firmware fw;
    memset(&fw,0,sizeof(firmware));
    if (argc < 4) {
        fprintf(stderr,"finsig_thumb2 <primary> <base> <outputfilename>\n");
        exit(1);
    }

    for (next_sig_entry = 0; sig_names[next_sig_entry].name != 0; next_sig_entry++);
    int max_find_sig = next_sig_entry;

    fw.sv = new_stub_values();
    load_stubs(fw.sv, "stubs_entry_2.S", 1);
    load_stubs_min(fw.sv);
    load_modemap(fw.sv);
    load_platform(fw.sv);

    bprintf("// !!! THIS FILE IS GENERATED. DO NOT EDIT. !!!\n");
    bprintf("#include \"stubs_asm.h\"\n\n");

    firmware_load(&fw,argv[1],strtoul(argv[2], NULL, 0),FW_ARCH_ARMv7);
    if(!firmware_init_capstone(&fw)) {
        exit(1);
    }
    firmware_init_data_ranges(&fw);

    out_fp = fopen(argv[3],"w");
    if (out_fp == NULL) {
        fprintf(stderr,"Error opening output file %s\n",argv[3]);
        exit(1);
    }


    // find ctypes - previously separate from regular sig matches to set code search limit
    find_ctypes(&fw);

    run_sig_rules(&fw,sig_rules_initial);
    find_generic_funcs(&fw);
    run_sig_rules(&fw,sig_rules_main);

    output_firmware_vals(&fw);

    output_platform_vals(&fw);
    output_physw_vals(&fw);
    output_modemap(&fw);

    output_propcases(&fw);
    output_exmem_types(&fw);

    write_stubs(&fw,max_find_sig);

    write_output();
    fclose(out_fp);

    write_func_lists(&fw);

    firmware_unload(&fw);

    clock_t t2 = clock();

    printf("Time to generate stubs %.2f seconds\n",(double)(t2-t1)/(double)CLOCKS_PER_SEC);

    return 0;
}
