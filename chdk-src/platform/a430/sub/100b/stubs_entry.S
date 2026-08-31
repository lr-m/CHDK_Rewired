// !!! THIS FILE IS GENERATED. DO NOT EDIT. !!!
#include "stubs_asm.h"

// Camera info:
//   Firmware Ver GM1.00B   // Found @ 0xffc0a208, "GM1.00B" @ 0xffc0a215
//   Firmware build timestamp: Dec  6 2005 09:17:55
//   Canon PowerShot A430

// Values for makefile.inc
//   DIGIC = 20# DIGIC II
//   PLATFORMID = 12536# (0x30f8) Found @ 0xfffe0130
//   MAXRAMADDR = 0x00ffffff
//   MEMISOSTART = 0x0009f7d0

// Detected address ranges:
// ROM      0xffc00000 - 0xffffffff (4194304 bytes)
// RAM data 0x00001900 - 0x0000ebc0 copied from 0xffec54f0 (  53952 bytes)

// Stubs below should be checked. Stub not matched 100%, or difference found to current 'stubs_entry_2.S'
//    Name                                     Address      Rule  %  Comp to stubs_entry_2.S
// ERROR: EnterToCompensationEVF is not found.             //--- ---    *** != 0xffd28268
//NHSTUB(ExitFromCompensationEVF              ,0xffd2832c) //  1 76%    *** != 0xffd282c4
NHSTUB(GetFocusLensSubjectDistance            ,0xffe5bc90) //  1 96%
NHSTUB(GetFocusLensSubjectDistanceFromLens    ,0xffe5c230) //  3 52%
NHSTUB(GetParameterData                       ,0xffd40e2c) //  1 86%
NHSTUB(GetSystemTime                          ,0xffc04b2c) //  1 96%
NHSTUB(PutInNdFilter                          ,0xffe674bc) //  1 92%
// ALT: NHSTUB(PutInNdFilter, 0xffe6904c) // 1 25/2
// ALT: NHSTUB(PutInNdFilter, 0xffe69094) // 1 25/2
// ALT: NHSTUB(PutInNdFilter, 0xffe6904c) // 2 25/2
// ALT: NHSTUB(PutInNdFilter, 0xffe69094) // 2 25/2
NHSTUB(PutOutNdFilter                         ,0xffe67504) //  1 94%
NHSTUB(SetParameterData                       ,0xffd40d4c) //  1 86%
NHSTUB(UnsetZoomForMovie                      ,0xffd76190) //  1 89%
//NHSTUB(WriteSDCard                          ,0xffc6c234) //  3 64%    *** != 0xffc6c3dc
NHSTUB(add_ptp_handler                        ,0xffe1bc34) //  1 96%
// ERROR: free is not found.                               //--- ---    *** != 0xffeb1eb0
//NHSTUB(kbd_pwr_off                          ,0xffc18e98) //  1 83%    *** != 0x00000000
// ALT: NHSTUB(kbd_pwr_off, 0xffc18eb4) // 1 5/1
// ALT: NHSTUB(kbd_pwr_off, 0xffc18b40) // 3 5/1
// ALT: NHSTUB(kbd_pwr_off, 0xffc18b5c) // 3 5/1
// ALT: NHSTUB(kbd_pwr_off, 0xffc18c48) // 3 5/1
// ALT: NHSTUB(kbd_pwr_off, 0xffc18c64) // 3 5/1
// ALT: NHSTUB(kbd_pwr_off, 0xffc18d34) // 3 5/1
// ALT: NHSTUB(kbd_pwr_off, 0xffc18d50) // 3 5/1
// ERROR: kbd_pwr_on is not found.                         //--- ---    *** != 0x00000000
NHSTUB(malloc                                 ,0xffeb1ea4) //  1 53%
NHSTUB(taskIdListGet                          ,0xffebeacc) //  1 82%
NHSTUB(localtime                              ,0xffeaaae0) //  1 93%
NHSTUB(Mount_FileSystem                       ,0xffc5ca48) //  1 90%
// ERROR: MakeAFScan is not found.                         //--- ---    *** != 0xffd2ac58

// Check of modemap from 'platform/CAMERA/shooting.c':
// Firmware modemap table found @ffd3c47c -> ffd3c47c
// Mode  3614 in firmware but not in current modemap
// Mode  3615 in firmware but not in current modemap
// Mode  3616 in firmware but not in current modemap
// Mode  3618 in firmware but not in current modemap
// Current modemap entry not found in firmware - MODE_VIDEO_COLOR_SWAP     2592
// Current modemap entry not found in firmware - MODE_VIDEO_COLOR_ACCENT   2591
// Current modemap entry not found in firmware - MODE_VIDEO_COMPACT        2595
// Current modemap entry not found in firmware - MODE_VIDEO_SPEED          2594
// Current modemap entry not found in firmware - MODE_VIDEO_STD            3613

// Values below can be overridden in 'stubs_min.S':
DEF(levent_table                            ,0xffd3547c) // Found @0xffd3547c
DEF(FlashParamsTable                        ,0xffd3e68c) // Found @0xffd3e68c
DEF(fileio_semaphore                        ,0x000027b4) // Found @0xffc5ae6c
DEF(exmem_alloc_table                       ,0x00001bc8) // Found @0xffc01f18
DEF(exmem_types_table                       ,0xffc01d30) // Found @0xffc01d30
DEF_CONST(exmem_type_count                  ,0x0000000d)
DEF(imager_active                           ,0x00005428) // Found @0xffcb11c8 (0x5428 + 0)
DEF_CONST(uiprop_count                      ,0x00000057) // Found @0xffd400a0
DEF(cameracon_state                         ,0x00006ca8) // Found @0xffd62c1c

// Values below go in 'lib.c':
//int get_flash_params_count(void) { return 0x6c; }                          // Found @0xffd40e98

// Values below go in 'platform_camera.h':
//#define PARAM_CAMERA_NAME 3 // Found @0xffd3de48

// Misc stuff

// EXMEM types:
// EXMEM_RAMDISK   0
// EXMEM_COM       1
// EXMEM_COMTMP    2
// EXMEM_FORMATTER 3
// EXMEM_SHADING   4
// EXMEM_FAMODE    5
// EXMEM_FIRMUP    6
// EXMEM_FILESTORE 7
// EXMEM_MOVIE_REC 8
// EXMEM_MOVIE_PLY 9
// EXMEM_WIDE      10
// EXMEM_NARROW    11
// EXMEM_PCEVF     12

// LED table init @ 0xffc1d614
// LED #1: 0xc022008c, offset 0x4
// LED #2: 0xc0220088, offset 0x44
// LED #3: 0xc0220090, offset 0x84
// LED #4: 0xc0220084, offset 0x204
// LED #5: 0xc0220080, offset 0x244


// For capt_seq.c
DEF(_nrflag,0x60e8) // Found @ ffd2c978 (0x60e8)
//static long *nrflag = (long*)(0x60e8);       // Found @ ffd2c978

// Stubs below matched 100%.
//    Name                                     Address                Comp to stubs_entry_2.S
NHSTUB(AllocateUncacheableMemory              ,0xffc048f4) //  1
NHSTUB(Close                                  ,0xffc5d7a8) //120
NHSTUB(CreateBinarySemaphore                  ,0xffc108b0) //115
NHSTUB(CreateTask                             ,0xffc11260) //120
NHSTUB(DebugAssert                            ,0xffc03aec) //1100
NHSTUB(DeleteFile_Fut                         ,0xffc56944) //  1
NHSTUB(DeleteSemaphore                        ,0xffc10aa4) //120
NHSTUB(DoAELock                               ,0xffd265cc) //120
NHSTUB(DoAFLock                               ,0xffd2647c) //120
NHSTUB(ExecuteEventProcedure                  ,0xffc0680c) //  1
NHSTUB(ExitTask                               ,0xffc11620) //120
NHSTUB(Fclose_Fut                             ,0xffc56a84) //120
NHSTUB(Feof_Fut                               ,0xffc56c98) //  1
NHSTUB(Fflush_Fut                             ,0xffc56cd0) //  1
NHSTUB(Fgets_Fut                              ,0xffc56c08) //  1
NHSTUB(Fopen_Fut                              ,0xffc56a48) //120
NHSTUB(Fread_Fut                              ,0xffc56b68) //120
NHSTUB(FreeUncacheableMemory                  ,0xffc04928) //  1
NHSTUB(Fseek_Fut                              ,0xffc56c50) //120
NHSTUB(Fwrite_Fut                             ,0xffc56bb8) //120
NHSTUB(GetBatteryTemperature                  ,0xffc1ccd0) //102
NHSTUB(GetCCDTemperature                      ,0xffc1ccbc) //102
NHSTUB(GetCurrentAvValue                      ,0xffe67338) //120
NHSTUB(GetCurrentShutterSpeed                 ,0xffe66fe8) //120
NHSTUB(GetDrive_ClusterSize                   ,0xffc5cf80) //119
NHSTUB(GetDrive_FreeClusters                  ,0xffc5cff8) //1103
NHSTUB(GetDrive_TotalClusters                 ,0xffc5cfbc) //119
NHSTUB(GetOpticalTemperature                  ,0xffc1cca8) //120
NHSTUB(GetPropertyCase                        ,0xffc14518) //  1
NHSTUB(GetZoomLensCurrentPoint                ,0xffe4c738) //120
NHSTUB(GetZoomLensCurrentPosition             ,0xffe4c74c) //102
NHSTUB(GiveSemaphore                          ,0xffc10e58) //120
NHSTUB(IsStrobeChargeCompleted                ,0xffce7fac) //  1
NHSTUB(LEDDrive                               ,0xffc1d228) //120
NHSTUB(LocalTime                              ,0xffc041bc) //115
NHSTUB(LockMainPower                          ,0xffd3c1f4) //120
NHSTUB(MakeDirectory_Fut                      ,0xffc569d0) //  1
NHSTUB(MoveFocusLensToDistance                ,0xffe696e8) //102
NHSTUB(MoveZoomLensWithPoint                  ,0xffe4c618) //120
NHSTUB(Open                                   ,0xffc5d780) //120
NHSTUB(PT_PlaySound                           ,0xffd33988) //  1
NHSTUB(PostLogicalEventForNotPowerType        ,0xffd382ec) //120
NHSTUB(PostLogicalEventToUI                   ,0xffd38368) //120
NHSTUB(Read                                   ,0xffc5d83c) //120
NHSTUB(RefreshPhysicalScreen                  ,0xffd52390) //120
NHSTUB(RenameFile_Fut                         ,0xffc568c4) //  1
NHSTUB(Restart                                ,0xffc1277c) //  1
NHSTUB(SetAE_ShutterSpeed                     ,0xffe6cb9c) //102
NHSTUB(SetAutoShutdownTime                    ,0xffd3c0ec) //120
NHSTUB(SetCurrentCaptureModeType              ,0xffd6b100) //120
NHSTUB(SetDate                                ,0xffe6ebdc) //120
NHSTUB(SetFileAttributes                      ,0xffc5d960) //  1
NHSTUB(SetLogicalEventActive                  ,0xffd3a0c4) //1104
NHSTUB(SetPropertyCase                        ,0xffc143ec) //  1
NHSTUB(SetScriptMode                          ,0xffd3a12c) //120
NHSTUB(SleepTask                              ,0xffc11174) //120
NHSTUB(TakeSemaphore                          ,0xffc10c70) //1101
NHSTUB(TurnOffBackLight                       ,0xffd81e3c) //  2
NHSTUB(TurnOnBackLight                        ,0xffd81e0c) //  2
NHSTUB(TurnOnDisplay                          ,0xffd0b068) //120
NHSTUB(TurnOffDisplay                         ,0xffd0b078) //120
NHSTUB(UnlockAE                               ,0xffd265ec) //120
NHSTUB(UnlockAF                               ,0xffd264c0) //120
NHSTUB(UnlockMainPower                        ,0xffd3c2a8) //120
NHSTUB(UpdateMBROnFlash                       ,0xffc5ccb4) //  1
NHSTUB(VbattGet                               ,0xffc1bca0) //120
NHSTUB(Write                                  ,0xffc5d848) //120
NHSTUB(_log                                   ,0xffea2c50) //121
NHSTUB(_log10                                 ,0xffe9f128) //121
NHSTUB(_pow                                   ,0xffe9f2b0) //121
NHSTUB(_sqrt                                  ,0xffea104c) //  1
NHSTUB(apex2us                                ,0xffc96f4c) //  1
NHSTUB(close                                  ,0xffeaee64) //109
NHSTUB(exmem_alloc                            ,0xffc02968) //122
NHSTUB(exmem_ualloc                           ,0xffc0281c) //122
NHSTUB(exmem_ufree                            ,0xffc028ec) //122
NHSTUB(get_nd_value                           ,0xffe5508c) //115
NHSTUB(get_current_nd_value                   ,0xffe54fe8) //122
NHSTUB(get_current_deltasv                    ,0xffc97f08) //122
NHSTUB(GetCurrentDriveBaseSvValue             ,0xffca094c) //107
NHSTUB(kbd_p1_f                               ,0xffc16914) //  1
NHSTUB(kbd_p1_f_cont                          ,0xffc16920) //109
NHSTUB(kbd_p2_f                               ,0xffc16cf0) //  1
NHSTUB(kbd_read_keys_r2                       ,0xffc17678) //  1
NHSTUB(lseek                                  ,0xffeaefa8) //  1
NHSTUB(memcmp                                 ,0xffea9ec0) //120
NHSTUB(memcpy                                 ,0xffea9efc) //120
NHSTUB(memset                                 ,0xffea9f74) //120
NHSTUB(mkdir                                  ,0xffc5d990) //  1
NHSTUB(open                                   ,0xffeaf4a8) //109
NHSTUB(closedir                               ,0xffeac2ec) //  1
NHSTUB(qsort                                  ,0xffea9720) //  1
NHSTUB(rand                                   ,0xffea9744) //  1
NHSTUB(realloc                                ,0xffe999f4) //115
NHSTUB(srand                                  ,0xffea9768) //  1
NHSTUB(stat                                   ,0xffeac3b4) //  1
NHSTUB(strcat                                 ,0xffea9f94) //  1
NHSTUB(strchr                                 ,0xffea9fc0) //  1
NHSTUB(strcmp                                 ,0xffea9fe4) //120
NHSTUB(strcpy                                 ,0xffeaa178) //120
NHSTUB(strftime                               ,0xffeab800) //  1
NHSTUB(strlen                                 ,0xffeaa2dc) //120
NHSTUB(strncmp                                ,0xffeaa340) //  1
NHSTUB(strncpy                                ,0xffeaa384) //  1
NHSTUB(strrchr                                ,0xffeaa408) //  1
NHSTUB(strtol                                 ,0xffea9b5c) //  1
NHSTUB(isdigit                                ,0xffea63c0) //  1
NHSTUB(isspace                                ,0xffea6438) //  1
NHSTUB(isalpha                                ,0xffea6390) //  1
NHSTUB(isupper                                ,0xffea6450) //  1
NHSTUB(islower                                ,0xffea63f0) //  1
NHSTUB(ispunct                                ,0xffea6420) //  1
NHSTUB(isxdigit                               ,0xffea6468) //  1
NHSTUB(toupper                                ,0xffea6490) //  1
NHSTUB(tolower                                ,0xffea6480) //  1
NHSTUB(taskLock                               ,0xffebf188) //  1
NHSTUB(taskUnlock                             ,0xffebf230) //  1
NHSTUB(taskName                               ,0xffebe9c4) //  1
NHSTUB(errnoOfTaskGet                         ,0xffeacab8) //  1
NHSTUB(readdir                                ,0xffeac2b8) //  1
NHSTUB(strtoul                                ,0xffea9d04) //  1
NHSTUB(strpbrk                                ,0xffeaa3cc) //  1
NHSTUB(utime                                  ,0xffeac448) //  1
NHSTUB(mktime                                 ,0xffeaaca8) //  1
NHSTUB(opendir                                ,0xffeac320) //  1
NHSTUB(memchr                                 ,0xffea9e8c) //  1
NHSTUB(memPartInfoGet                         ,0xffeb22c4) //  1
NHSTUB(iosDrvInstall                          ,0xffeb0348) //  1
NHSTUB(iosDevAdd                              ,0xffeb0084) //  1
NHSTUB(iosDevDelete                           ,0xffeb005c) //  1
NHSTUB(GetCurrentTargetDistance               ,0xffe69448) //  1
NHSTUB(taskCreateHookAdd                      ,0xffeb4b04) //  1
NHSTUB(taskDeleteHookAdd                      ,0xffeb4a40) //  1
NHSTUB(Unmount_FileSystem                     ,0xffc5cb60) //  1
NHSTUB(taskSuspend                            ,0xffebebd4) //  1
NHSTUB(taskResume                             ,0xffebeda8) //  1
NHSTUB(time                                   ,0xffeab82c) //  1
NHSTUB(vsprintf                               ,0xffeadc0c) //115
NHSTUB(EngDrvRead                             ,0xffc6e774) //102
NHSTUB(PTM_GetCurrentItem                     ,0xffd3edc8) //102
NHSTUB(CancelHPTimer                          ,0xffc059f0) //119
NHSTUB(SetHPTimerAfterNow                     ,0xffc0557c) //119
NHSTUB(MFOn                                   ,0xffd26f44) //120
NHSTUB(MFOff                                  ,0xffd26f74) //120
NHSTUB(GetAdChValue                           ,0xffc195f4) //120
NHSTUB(get_ptp_buf_size                       ,0xffe10c74) //122
NHSTUB(get_ptp_file_buf                       ,0xffe2bfac) //121
