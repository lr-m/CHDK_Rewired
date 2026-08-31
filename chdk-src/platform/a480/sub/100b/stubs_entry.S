// !!! THIS FILE IS GENERATED. DO NOT EDIT. !!!
#include "stubs_asm.h"

// Camera info:
//   DRYOS R31 (DRYOS version 2.3, release #0031)
//   Firmware Ver GM1.00B   // Found @ 0xffc50a30, "GM1.00B" @ 0xffc50a3d
//   Firmware build timestamp: Dec  5 2008 17:19:05
//   Canon PowerShot A480

// Values for makefile.inc
//   DIGIC = 30# DIGIC III
//   PLATFORMOSVER = 31
//   PLATFORMID = 12735# (0x31bf) Found @ 0xfffe0130
//   MAXRAMADDR = 0x01ffffff
//   MEMISOSTART = 0x00102438
//   KEYSYS = d4# Found @ 0xffff1a44
//   NEED_ENCODED_DISKBOOT = 2# Found @ 0xffff1a54
//   ARAM_HEAP_START = 0x2ce000# Found @ 0xffcf0d10
//   ARAM_HEAP_SIZE  = 0x32000# Found @ 0xffcf0cd0

// Detected address ranges:
// ROM      0xffc00000 - 0xffffffff (4194304 bytes)
// RAM data 0x00001900 - 0x0000ba68 copied from 0xffe88e20 (  41320 bytes)

// Stubs below should be checked. Stub not matched 100%, or difference found to current 'stubs_entry_2.S'
//    Name                                     Address      Rule  %  Comp to stubs_entry_2.S
//NHSTUB(GetBatteryTemperature                ,0xffc2e964) //102        *** != 0xffc2eb38
//NHSTUB(GetCCDTemperature                    ,0xffc2e8f0) //102        *** != 0xffc2eb38
//NHSTUB(PutInNdFilter                        ,0xffdc22b8) //102        *** != 0xffe01e0c
//NHSTUB(PutOutNdFilter                       ,0xffdc22dc) //102        *** != 0xffe01e44
//NHSTUB(kbd_read_keys_r2                     ,0xffc2c384) //109        *** != 0xffc1138c
NHSTUB(kbd_pwr_off                            ,0xffc2c8c4) //  2 64%

// Check of modemap from 'platform/CAMERA/shooting.c':
// Firmware modemap table found @ffdcd6ec -> ffe71f6c
// No problems found with modemap table.

// Values below can be overridden in 'stubs_min.S':
DEF(physw_status                            ,0x0000cef8) // Found @0xffc11360
DEF(physw_run                               ,0x00001bec) // Found @0xffc10fd4
DEF(physw_sleep_delay                       ,0x00001bea) // Found @0xffc10fd4
DEF(levent_table                            ,0xffe57a18) // Found @0xffe57a18
DEF(FlashParamsTable                        ,0xffe72138) // Found @0xffe72138
DEF(movie_status                            ,0x00005700) // Found @0xffce5478
DEF(canon_menu_active                       ,0x00002a00) // Found @0xffc52450
//DEF(canon_shoot_menu_active                 ,0x00006861) // Found @0xffd16804, ** != ** stubs_min = 0xffc00414 (0xFFC00414)
DEF(playrec_mode                            ,0x00002798) // Found @0xffc46f04
DEF(zoom_status                             ,0x0000a2d4) // Found @0xffdcf93c
DEF(some_flag_for_af_scan                   ,0x00005bbc) // Found @0xffce7b58
// focus_len_table contains zoom focus lengths for use in 'get_focal_length' (main.c).
// each entry contains 3 int value(s), the first is the zoom focus length.
// there are 7 entries in the table - set NUM_FL to 7
DEF(focus_len_table                         ,0xfffe297c) // Found @0xfffe297c
DEF(zoom_busy                               ,0x00005698) // Found @0xffce3a34
DEF(focus_busy                              ,0x000055a0) // Found @0xffcdf584
//DEF(recreview_hold                          ,0x00002f60) // Found @0xffc5da24, ** != ** stubs_min = 0xffc00414 (0xFFC00414)
// Camera appears to have only 1 RAW buffer @ 0x10c5fa60 (Found @0xffe09b68)
DEF(fileio_semaphore                        ,0x00002520) // Found @0xffc39000
DEF(exmem_alloc_table                       ,0x00012e94) // Found @0xffc4bf8c
DEF(exmem_types_table                       ,0xffe89d70) // Found @0x00002850
DEF_CONST(exmem_type_count                  ,0x00000012)
DEF(imager_active                           ,0x00001dec) // Found @0xffc216ec (0x1de8 + 4)
DEF_CONST(uiprop_count                      ,0x0000006b) // Found @0xffd0832c
DEF(cameracon_state                         ,0x00002b78) // Found @0xffc566bc

// Values below go in 'lib.c':
//void *vid_get_bitmap_fb()        { return (void*)0x10361000; }             // Found @0xffc2845c
//void *vid_get_viewport_fb()      { return (void*)0x10659ec0; }             // Found @0xffe07650
DEF(viewport_fb_d                           ,0x000025a8) // Found 0x2554 (@0xffc3c058) + 0x54 (@0xffc3c098)
//void *vid_get_viewport_fb_d()    { return (void*)(*(int*)(0x2554+0x54)); } // Found @0xffc3c058 & 0xffc3c098
DEF(jpeg_count_str                          ,0x0002cff8) // Found @0xffd41b24
//char *camera_jpeg_count_str()    { return (char*)0x0002cff8; }             // Found @0xffd41b24
//int get_flash_params_count(void) { return 0x7a; }                          // Found @0xffd08d70

// Bitmap masks and physw_status index values for SD_READONLY and USB power flags (for kbd.c).
//#define SD_READONLY_FLAG    0x00040000 // Found @0xffe34fec, levent 0x90a
//#define SD_READONLY_IDX     2
//#define USB_MASK            0x00080000 // Found @0xffe34ff8, levent 0x902
//#define USB_IDX             2
//#define BATTCOVER_FLAG      0x00010000 // Found @0xffe34fd4, levent 0x905
//#define BATTCOVER_IDX       2
//#define ANALOG_AV_FLAG      0x00100000 // Found @0xffe34f38, levent 0x900 (non-inverted logic)
//#define ANALOG_AV_IDX       0

// Keymap values for kbd.c. Additional keys may be present, only common values included here.
//static KeyMap keymap[] = {
//    { 0, KEY_PLAYBACK        ,0x00020000 }, // Found @0xffe34f2c, levent 0x601
//    { 0, KEY_POWER           ,0x00800000 }, // Found @0xffe34f44, levent 0x600
//    { 2, KEY_UP              ,0x00000001 }, // Found @0xffe34f50, levent 0x04
//    { 2, KEY_DOWN            ,0x00000002 }, // Found @0xffe34f5c, levent 0x05
//    { 2, KEY_RIGHT           ,0x00000004 }, // Found @0xffe34f68, levent 0x07
//    { 2, KEY_LEFT            ,0x00000008 }, // Found @0xffe34f74, levent 0x06
//    { 2, KEY_SHOOT_FULL      ,0x00000030 }, // Found @0xffe34f8c, levent 0x01
//    { 2, KEY_SHOOT_FULL_ONLY ,0x00000020 }, // Found @0xffe34f8c, levent 0x01
//    { 2, KEY_SHOOT_HALF      ,0x00000010 }, // Found @0xffe34f80, levent 0x00
//    { 2, KEY_ZOOM_OUT        ,0x00000040 }, // Found @0xffe34f98, levent 0x03
//    { 2, KEY_ZOOM_IN         ,0x00000080 }, // Found @0xffe34fa4, levent 0x02
//    { 2, KEY_SET             ,0x00000100 }, // Found @0xffe34fb0, levent 0x08
//    { 2, KEY_MENU            ,0x00000400 }, // Found @0xffe34fbc, levent 0x09
//    { 0, 0, 0 }
//};

// Values below go in 'platform_camera.h':
//#define CAM_DRYOS         1
//#define CAM_RAW_ROWPIX    3720 // Found @0xffcb8f20
//#define CAM_RAW_ROWS      2772 // Found @0xffcb8f28
//#undef  CAM_UNCACHED_BIT
//#define CAM_UNCACHED_BIT  0x10000000 // Found @0xffc16738
//#define PARAM_CAMERA_NAME 4 // Found @0xffe73ec4

// Below goes in 'filewrite.c' or 'platform_camera.h':
//#define MAX_CHUNKS_FOR_FWT 3 // Found @0xffdabbe8

// Known propcases
// //      PROPCASE_AFSTEP 13
// #define PROPCASE_FOCUS_STATE 18
// #define PROPCASE_AV 23
// #define PROPCASE_BV 34
// //      PROPCASE_DELTA_DIGITALGAIN 77
// #define PROPCASE_DELTA_SV 79
// //      PROPCASE_DELTA_ND 80
// #define PROPCASE_EV_CORRECTION_2 207
// #define PROPCASE_ORIENTATION_SENSOR 219
// #define PROPCASE_SV_MARKET 246
// //      PROPCASE_SVFIX 247
// #define PROPCASE_TV 262
// Guessed propset: 2

// Misc stuff
DEF(ctypes, 0xffe34a9e)

// EXMEM types:
// EXMEM_RAMDISK        0
// EXMEM_COM            1
// EXMEM_FORMATTER      2
// EXMEM_SHADING        3
// EXMEM_FAMODE         4
// EXMEM_FIRMUP         5
// EXMEM_FIRMUP_VRAM    6
// EXMEM_FIRMUP_INFLATE 7
// EXMEM_DVLPWORK       8
// EXMEM_EVF            9
// EXMEM_SCRIPT         10
// EXMEM_FILESTORE      11
// EXMEM_MOVIE_REC      12
// EXMEM_MOVIE_PLY      13
// EXMEM_WIDE           14
// EXMEM_NARROW         15
// EXMEM_CRAW_REC       16
// EXMEM_PCEVF          17

// ISR flag: 0x1960, pointer to current task's control block: 0x195c
// pointer to TCB area: 0x19f4, found @ 0xffc05e84

// LED table init @ 0xffca5b98
// LED #1: 0xc0220080, offset 0x244
// LED #2: 0xc0220088, offset 0x1c4

// For capt_seq.c
DEF(_nrflag,0x5ba0) // Found @ ffce987c (0x5ba8) & ffce989c (-0x08)
//static long *nrflag = (long*)(0x5ba8-0x08);  // Found @ ffce987c & ffce9898

// Stubs below matched 100%.
//    Name                                     Address                Comp to stubs_entry_2.S
NHSTUB(AllocateUncacheableMemory              ,0xffc166f4) //112
NHSTUB(Close                                  ,0xffc1322c) //120
NHSTUB(CreateBinarySemaphore                  ,0xffc0b6e4) //115
NHSTUB(CreateTask                             ,0xffc0b9c4) //120
NHSTUB(DebugAssert                            ,0xffc0be9c) //107
NHSTUB(DeleteDirectory_Fut                    ,0xffc12bbc) //116
NHSTUB(DeleteFile_Fut                         ,0xffc12ad8) //112
NHSTUB(DeleteSemaphore                        ,0xffc0b7a4) //120
NHSTUB(DoAELock                               ,0xffc14978) //120
NHSTUB(DoAFLock                               ,0xffc14830) //120
NHSTUB(EnterToCompensationEVF                 ,0xffc148e0) //104
NHSTUB(ExecuteEventProcedure                  ,0xffc4dbd8) //104
NHSTUB(ExitFromCompensationEVF                ,0xffc14924) //104
NHSTUB(ExitTask                               ,0xffc0bc54) //120
NHSTUB(ExpCtrlTool_StartContiAE               ,0xffc1eee4) //104
NHSTUB(ExpCtrlTool_StopContiAE                ,0xffc1f028) //104
NHSTUB(Fclose_Fut                             ,0xffc12c54) //120
NHSTUB(Feof_Fut                               ,0xffc12e40) //  1
NHSTUB(Fflush_Fut                             ,0xffc12e7c) //  1
NHSTUB(Fgets_Fut                              ,0xffc12da8) //  1
NHSTUB(Fopen_Fut                              ,0xffc12c14) //120
NHSTUB(Fread_Fut                              ,0xffc12d00) //120
NHSTUB(FreeUncacheableMemory                  ,0xffc16734) //112
NHSTUB(Fseek_Fut                              ,0xffc12df4) //120
NHSTUB(Fwrite_Fut                             ,0xffc12d54) //120
NHSTUB(GetCurrentAvValue                      ,0xffce0fe8) //120
NHSTUB(GetCurrentShutterSpeed                 ,0xffc64a90) //120
NHSTUB(GetDrive_ClusterSize                   ,0xffc3a924) //  5
NHSTUB(GetDrive_FreeClusters                  ,0xffc3a97c) //  5
NHSTUB(GetDrive_TotalClusters                 ,0xffc3a950) //  5
NHSTUB(GetFocusLensSubjectDistance            ,0xffce00c8) //102
NHSTUB(GetFocusLensSubjectDistanceFromLens    ,0xffce03d4) //  3
NHSTUB(GetKbdState                            ,0xffc2c8ac) //109
NHSTUB(GetMemInfo                             ,0xffc03be8) //115
NHSTUB(GetOpticalTemperature                  ,0xffc2e9dc) //120
NHSTUB(GetParameterData                       ,0xffd08d30) //101
NHSTUB(GetPropertyCase                        ,0xffc51944) //102
NHSTUB(GetSystemTime                          ,0xffdc4608) //120
NHSTUB(GetVRAMHPixelsSize                     ,0xffc8aa34) //120
NHSTUB(GetVRAMVPixelsSize                     ,0xffc8aa40) //120
NHSTUB(GetZoomLensCurrentPoint                ,0xffce3dac) //120
NHSTUB(GetZoomLensCurrentPosition             ,0xffdbcdf4) //102
NHSTUB(GiveSemaphore                          ,0xffc0b920) //120
NHSTUB(IsStrobeChargeCompleted                ,0xffc7ccc4) //102
NHSTUB(LEDDrive                               ,0xffc2c020) //120
NHSTUB(LocalTime                              ,0xffc4b954) //115
NHSTUB(LockMainPower                          ,0xffc55210) //120
NHSTUB(MakeDirectory_Fut                      ,0xffc12b4c) //116
NHSTUB(MoveFocusLensToDistance                ,0xffe04080) //102
NHSTUB(MoveIrisWithAv                         ,0xffdc1ec8) //120
NHSTUB(MoveZoomLensWithPoint                  ,0xffdc2454) //120
NHSTUB(MoveOpticalZoomAt                      ,0xffc14424) //109
NHSTUB(Open                                   ,0xffc13204) //120
NHSTUB(PB2Rec                                 ,0xffc57ebc) //104
NHSTUB(PT_MoveDigitalZoomToWide               ,0xffc40e3c) //120
NHSTUB(PT_MoveOpticalZoomAt                   ,0xffc40e14) //120
NHSTUB(PT_PlaySound                           ,0xffc40a50) //102
NHSTUB(PostLogicalEventForNotPowerType        ,0xffc54838) //120
NHSTUB(PostLogicalEventToUI                   ,0xffc54884) //120
NHSTUB(Read                                   ,0xffc0a21c) //120
NHSTUB(ReadFastDir                            ,0xffdb0044) //  1
NHSTUB(Rec2PB                                 ,0xffc57efc) //104
NHSTUB(RefreshPhysicalScreen                  ,0xffd4be50) //104
NHSTUB(RenameFile_Fut                         ,0xffc12a84) //  1
NHSTUB(Restart                                ,0xffc17e44) //122
NHSTUB(SetAE_ShutterSpeed                     ,0xffe06860) //102
NHSTUB(SetAutoShutdownTime                    ,0xffc55284) //120
NHSTUB(SetCurrentCaptureModeType              ,0xffc5b514) //120
NHSTUB(SetDate                                ,0xffe0829c) //120
NHSTUB(SetFileAttributes                      ,0xffc13558) //  1
NHSTUB(SetFileTimeStamp                       ,0xffc13508) //  1
NHSTUB(SetLogicalEventActive                  ,0xffc54f8c) //120
NHSTUB(SetParameterData                       ,0xffd08ca0) //101
NHSTUB(SetPropertyCase                        ,0xffc5183c) //102
NHSTUB(SetScriptMode                          ,0xffc54fdc) //120
NHSTUB(SleepTask                              ,0xffc16578) //120
NHSTUB(TakeSemaphore                          ,0xffc0b850) //120
NHSTUB(TurnOffBackLight                       ,0xffc5fcec) //104
NHSTUB(TurnOnBackLight                        ,0xffc5fcd4) //104
NHSTUB(TurnOnDisplay                          ,0xffc28524) //120
NHSTUB(TurnOffDisplay                         ,0xffc28534) //120
NHSTUB(UIFS_WriteFirmInfoToFile               ,0xffcfa8ec) //120
NHSTUB(UnlockAE                               ,0xffc1499c) //120
NHSTUB(UnlockAF                               ,0xffc14868) //120
NHSTUB(UnlockMainPower                        ,0xffc55164) //120
NHSTUB(UnsetZoomForMovie                      ,0xffd0e960) //104
NHSTUB(UpdateMBROnFlash                       ,0xffc3a77c) //105
NHSTUB(VbattGet                               ,0xffc10294) //120
NHSTUB(Write                                  ,0xffc132d8) //120
NHSTUB(WriteSDCard                            ,0xffcd598c) //108
NHSTUB(_log                                   ,0xffe328a4) //111
NHSTUB(_log10                                 ,0xffe317e8) //112
NHSTUB(_pow                                   ,0xffe31958) //121
NHSTUB(_sqrt                                  ,0xffe33c0c) //  1
NHSTUB(add_ptp_handler                        ,0xffd8a108) //121
NHSTUB(apex2us                                ,0xffd4cf08) //121
NHSTUB(close                                  ,0xffc0a034) //109
NHSTUB(err_init_task                          ,0xffc04e30) //111
NHSTUB(exmem_alloc                            ,0xffc4c588) //103
NHSTUB(exmem_ualloc                           ,0xffc4c3dc) //103
NHSTUB(exmem_ufree                            ,0xffc4c528) //103
NHSTUB(free                                   ,0xffc03a40) //112
NHSTUB(get_nd_value                           ,0xffdba7c0) //123
NHSTUB(get_current_nd_value                   ,0xffdba77c) //122
NHSTUB(get_current_deltasv                    ,0xffc65620) //122
NHSTUB(GetCurrentDriveBaseSvValue             ,0xffc6a554) //123
NHSTUB(kbd_p1_f                               ,0xffc113dc) //109
NHSTUB(kbd_p1_f_cont                          ,0xffc113e8) //109
NHSTUB(kbd_p2_f                               ,0xffc10ca0) //109
NHSTUB(kbd_read_keys                          ,0xffc11360) //109
NHSTUB(lseek                                  ,0xffc1336c) //120
NHSTUB(malloc                                 ,0xffc0396c) //112
NHSTUB(memcmp                                 ,0xffc0d9b4) //120
NHSTUB(memcpy                                 ,0xffc63638) //120
NHSTUB(memset                                 ,0xffd4c1f0) //120
NHSTUB(mkdir                                  ,0xffc135d0) //109
NHSTUB(mktime_ext                             ,0xffc63ea4) //  1
NHSTUB(open                                   ,0xffc09f84) //109
NHSTUB(OpenFastDir                            ,0xffdaffb0) //  1
NHSTUB(closedir                               ,0xffdb0114) //  1
NHSTUB(qsort                                  ,0xffdf2a18) //  1
NHSTUB(rand                                   ,0xffc0dabc) //121
NHSTUB(realloc                                ,0xffc03b20) //115
NHSTUB(reboot_fw_update                       ,0xffdb3b28) //106
NHSTUB(set_control_event                      ,0xffc54cc0) //111
NHSTUB(srand                                  ,0xffc0dab0) //122
NHSTUB(stat                                   ,0xffc13438) //  1
NHSTUB(strcat                                 ,0xffc6344c) //  1
NHSTUB(strchr                                 ,0xffc634d4) //  1
NHSTUB(strcmp                                 ,0xffc0d94c) //120
NHSTUB(strcpy                                 ,0xffc0d934) //120
NHSTUB(strftime                               ,0xffc638ac) //  1
NHSTUB(strlen                                 ,0xffc0d990) //120
NHSTUB(strncmp                                ,0xffc63488) //  1
NHSTUB(strncpy                                ,0xffc1bf08) //  1
NHSTUB(strrchr                                ,0xffd4c1c8) //  1
NHSTUB(strtol                                 ,0xffc1c140) //101
NHSTUB(strtolx                                ,0xffc1bf44) //109
   DEF(task_CaptSeq                           ,0xffc42f34) //120
   DEF(task_DvlpSeqTask                       ,0xffcea518) //120
   DEF(task_ExpDrv                            ,0xffc74cb0) //120
   DEF(task_FileWrite                         ,0xffdabb84) //120
   DEF(task_InitFileModules                   ,0xffc589cc) //120
   DEF(task_MovieRecord                       ,0xffce56dc) //120
   DEF(task_PhySw                             ,0xffc10fd0) //120
NHSTUB(time                                   ,0xffc4b808) //109
NHSTUB(vsprintf                               ,0xffc0d8f8) //115
NHSTUB(EngDrvRead                             ,0xffc12218) //102
NHSTUB(PTM_GetCurrentItem                     ,0xffc55c0c) //105
NHSTUB(CancelHPTimer                          ,0xffc4d2c0) //119
NHSTUB(SetHPTimerAfterNow                     ,0xffc4d1b8) //119
NHSTUB(PT_MFOn                                ,0xffc40eb4) //120
NHSTUB(PT_MFOff                               ,0xffc40ec4) //120
NHSTUB(GetAdChValue                           ,0xffc29f00) //120
NHSTUB(get_ptp_buf_size                       ,0xffc8d44c) //122
NHSTUB(get_ptp_file_buf                       ,0xffd8bc84) //121
NHSTUB(SetVideoOutType                        ,0xffe0704c) //109
NHSTUB(GetVideoOutType                        ,0xffe06fb0) //120
