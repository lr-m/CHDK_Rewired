#ifndef PROPSET12_H
#define PROPSET12_H

// based on propset 10

/*
constants for propset 12
Checked entries are denoted by [m100: cam] or [m100: disasm]
WARNING:
The build uses tools/gen_propset_lua.sed to generate propset12.lua from this file
DO NOT USE MULTILINE COMMENTS AROUND DEFINES
*/

#define PROPCASE_AE_LOCK                      3 // [sx730: cam]
#define PROPCASE_AF_ASSIST_BEAM               5 // [sx730: cam]
#define PROPCASE_REAL_FOCUS_MODE              6 // sx730 0 = Normal, 1 = Macro, 4 = MF
#define PROPCASE_AF_FRAME                     8 // [sx730: cam] 1 = center, 2 = face or tracking
#define PROPCASE_AF_LOCK                     11 //
#define PROPCASE_CONTINUOUS_AF               12 // [sx730: cam]
#define PROPCASE_FOCUS_STATE                 18 // [sx730: cam]
// m10, sx730 both AV, not verified which does over and which does exif
#define PROPCASE_AV2                         22 //
#define PROPCASE_AV                          23 //
#define PROPCASE_MIN_AV                      28 // [sx730: cam]
#define PROPCASE_USER_AV                     29 // [sx730: cam]
// no bracketing on m10, sx730 propcase assumed to be unchanged
#define PROPCASE_BRACKET_MODE                33 //
#define PROPCASE_BV                          40 // [sx730: cam]
#define PROPCASE_SHOOTING_MODE               56 // [sx730: cam]
#define PROPCASE_CUSTOM_SATURATION           63 // [sx730: cam]
#define PROPCASE_QUALITY                     65 // [sx730: cam]
#define PROPCASE_CUSTOM_CONTRAST             67 // [sx730: cam]
#define PROPCASE_LANGUAGE                    69 // Upper byte = language (see default.lua for known values) lowest bit: 0 = NTSC, 1 = PAL
#define PROPCASE_FLASH_SYNC_CURTAIN          72 //
#define PROPCASE_SUBJECT_DIST2               73 // [sx730: cam]
// M10 has no date stamp option
#define PROPCASE_DATE_STAMP                  74 // [sx730: cam] 0 = off, 1 = date, 2 = date + time
#define PROPCASE_DELTA_SV                    87 // [sx730: cam]
// no digi zoom on m10
#define PROPCASE_DIGITAL_ZOOM_MODE          100 // [sx730: cam] 0 = off, 1 = standard, 2=1.6x, 3=2x
// #define PROPCASE_DIGITAL_ZOOM_STATE
#define PROPCASE_DIGITAL_ZOOM_POSITION      104 // [sx730: cam]
#define PROPCASE_DRIVE_MODE                 111 // [sx730: cam] 0 = single, 1 = cont, 2 = contaf
#define PROPCASE_OVEREXPOSURE               112 // [sx730: cam]
#define PROPCASE_DISPLAY_MODE               114 // [sx730: cam] 0 = normal, 1 = minimal OSD
#define PROPCASE_EV_CORRECTION_1            116 // [sx730: cam]
#define PROPCASE_FLASH_ADJUST_MODE          130 //
#define PROPCASE_FLASH_FIRE                 131 //
#define PROPCASE_FLASH_EXP_COMP             136 // [sx730: cam]
#define PROPCASE_FOCUS_MODE                 142 // [sx730: cam] 0 = AF, 1 = MF
#define PROPCASE_FLASH_MANUAL_OUTPUT        150 //
#define PROPCASE_FLASH_MODE                 152 // [m100: disasm, sx730 cam] 0 = auto, 1 = on, 2 = off
// TODO values changed?
#define PROPCASE_IS_MODE                    154 // [m100: disasm, sx730: cam] sx730 0 = cont, 1 = shoot, 2 = 0ff
#define PROPCASE_ISO_MODE                   158 // [m100: disasm, sx730 cam]
#define PROPCASE_METERING_MODE              167 // [m100: disasm]
#define PROPCASE_VIDEO_FRAMERATE            177 // [m100: cam] sx730: 60 fps = 8, 30 fps = 0
#define PROPCASE_VIDEO_RESOLUTION           180 // [m100: cam, sx730 cam] sx730 5 = 1080, 4 = 720, 2 = 640
#define PROPCASE_CUSTOM_BLUE                186 // [sx730: cam]
#define PROPCASE_CUSTOM_GREEN               187 // [sx730: cam]
#define PROPCASE_CUSTOM_RED                 188 // [sx730: cam]
#define PROPCASE_CUSTOM_SKIN_TONE           189 // [sx730: cam]
#define PROPCASE_MY_COLORS                  197 // [sx730: cam]
#define PROPCASE_ND_FILTER_STATE            205 //
#define PROPCASE_OPTICAL_ZOOM_POSITION      208 // [sx730: cam]
#define PROPCASE_EXPOSURE_LOCK              219 // [sx730: cam]
#define PROPCASE_EV_CORRECTION_2            220 // [sx730: cam]
#define PROPCASE_IS_FLASH_READY             221 // [sx730: cam]
#define PROPCASE_IMAGE_FORMAT               223 //
#define PROPCASE_RESOLUTION                 231 // [sx730: cam] sx730 0 = L, 2 = M1, 3 = M2, 5 = S
#define PROPCASE_ORIENTATION_SENSOR         232 // [m100: disasm, sx730: cam]
#define PROPCASE_TIMER_MODE                 237 // [m100: disasm, sx730: cam]
#define PROPCASE_TIMER_DELAY                238 // [m100: cam, sx730: cam]
#define PROPCASE_CUSTOM_SHARPNESS           239 // [sx730: cam]
// TODO guessed, no stitch on m10, sx730 (or any modern cam)
//#define PROPCASE_STITCH_DIRECTION         247 //
//#define PROPCASE_STITCH_SEQUENCE          252 //
#define PROPCASE_SUBJECT_DIST1              259 // [m100: disasm]
#define PROPCASE_SV_MARKET                  260 // [sx730: cam]
#define PROPCASE_TV2                        276 // [sx730: cam]
#define PROPCASE_TV                         277 // [sx730: cam]
#define PROPCASE_USER_TV                    279 // [sx730: cam]
#define PROPCASE_WB_MODE                    284 // [m100: cam]
#define PROPCASE_WB_ADJ                     285 //
#define PROPCASE_SERVO_AF                   311 // [m100: cam, sx730: cam]
#define PROPCASE_ASPECT_RATIO               312 // [m100: cam, sx730: cam] sx730 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1 (also in prop 406)
#define PROPCASE_SV                         359 // [m100: cam, sx730: cam]
// TODO no GPS on M10
// #define PROPCASE_GPS
#define PROPCASE_REVIEW_TIME                376 // [sx730: cam] Review time 0 = off, -1 = hold, 100 = quick, otherwise milliseconds
#define PROPCASE_TIMER_SHOTS                388 // [m100: cam, sx730: cam]

//
#define PROPCASE_SHOOTING_STATE             364 // [m100: disasm, sx730: cam]
#define PROPCASE_SHOOTING                  1001 //

#endif
