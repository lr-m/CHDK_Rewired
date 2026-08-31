#ifndef PROPSET13_H
#define PROPSET13_H

// based on propset 10

/*
constants for propset 13 - G7X2
WARNING:
The build uses tools/gen_propset_lua.sed to generate propset13.lua from this file
DO NOT USE MULTILINE COMMENTS AROUND DEFINES
*/

#define PROPCASE_AE_LOCK                         3          // 0 = AE not locked, 1 = AE locked
#define PROPCASE_AF_ASSIST_BEAM                  5          // 0=disabled,  1=enabled
#define PROPCASE_REAL_FOCUS_MODE                 6          // 0 = Normal AF, 1 = Macro, 4 = MF
#define PROPCASE_AF_FRAME                        8          // 1 = one point af, 2 = Face AiAF / Tracking AF
#define PROPCASE_AF_LOCK                         11         // 0 = AF not locked, 1 = AF locked (not verified, g7x AF lock just enables MF at current dist)
#define PROPCASE_CONTINUOUS_AF                   12         // 0 = Continuous AF off, 1 = Continuous AF on (g7x)
#define PROPCASE_FOCUS_STATE                     18         // 1 AF done

#define PROPCASE_AV2                             22         // (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
#define PROPCASE_AV                              23         // This values causes the actual aperture value to be overriden
#define PROPCASE_MIN_AV                          28         // G7X2 = 28
#define PROPCASE_USER_AV                         29         // updates instantly when setting aperture on cam

#define PROPCASE_BRACKET_MODE                    33         // 0 = 0ff, 1 = exposure, 2 = focus (MF only) (g7x)
#define PROPCASE_BV                              40         // guessed, but should be this one
#define PROPCASE_SHOOTING_MODE                   55         // 56 would probably show C as distinct mode but no C mode on M10
//#define PROPCASE_CUSTOM_SATURATION               62         // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_QUALITY                         64
//#define PROPCASE_CUSTOM_CONTRAST                 66         // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_LANGUAGE                        68         // Upper byte = language (see default.lua for known values) lowest bit: 0 = NTSC, 1 = PAL
#define PROPCASE_FLASH_SYNC_CURTAIN              71         // 0 first, 1 second
#define PROPCASE_SUBJECT_DIST2                   72

#define PROPCASE_DELTA_SV                        86

#define PROPCASE_DIGITAL_ZOOM_MODE               99         // Digital Zoom Mode/State 0 = off, 1=standard, 2 = 1.5x, 3 = 2.0x
#define PROPCASE_DIGITAL_ZOOM_POSITION           103
#define PROPCASE_DRIVE_MODE                      110        // 0 = single, 1 = cont, 4 = high-speed continuous (G7X2)
#define PROPCASE_OVEREXPOSURE                    111
#define PROPCASE_DISPLAY_MODE                    113
#define PROPCASE_EV_CORRECTION_1                 115
#define PROPCASE_FLASH_ADJUST_MODE               129
#define PROPCASE_FLASH_FIRE                      130
#define PROPCASE_FLASH_EXP_COMP                  135        // APEX96 units
#define PROPCASE_FOCUS_MODE                      141        // 0 = auto / af+mf, 1 = MF
#define PROPCASE_FLASH_MANUAL_OUTPUT             149        // TODO guessed (ps6 had unsure comments too)
#define PROPCASE_FLASH_MODE                      151        // 0 = Auto, 1 = ON, 2 = OFF

#define PROPCASE_IS_MODE                         153        // 0 = Continuous, 2 = OFF
#define PROPCASE_ISO_MODE                        157
#define PROPCASE_METERING_MODE                   165        // 0 = Evaluative, 1 = Spot, 2 = Center weighted avg 3 partial
#define PROPCASE_VIDEO_FRAMERATE                 175        // 0=30, 7=60 (g7x) 1 25
#define PROPCASE_VIDEO_RESOLUTION                178        // 5=1920x1280, 4=1280x720 2=640x480 (g7x)
//#define PROPCASE_CUSTOM_BLUE                     184        // Canon Menu slide bar values: 255, 254, 0, 1, 2
//#define PROPCASE_CUSTOM_GREEN                    185        // Canon Menu slide bar values: 255, 254, 0, 1, 2
//#define PROPCASE_CUSTOM_RED                      186        // Canon Menu slide bar values: 255, 254, 0, 1, 2
//#define PROPCASE_CUSTOM_SKIN_TONE                187        // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_ND_FILTER_STATE                 203        // 0 = out, 1 = in
#define PROPCASE_OPTICAL_ZOOM_POSITION           206
#define PROPCASE_EXPOSURE_LOCK                   217        // Old PROPCASE_SHOOTING value - gets set when set_aelock called or AEL button pressed
#define PROPCASE_EV_CORRECTION_2                 218        // g7x ok, ps6 +6
#define PROPCASE_IS_FLASH_READY                  219
#define PROPCASE_IMAGE_FORMAT                    221        // 0 = RAW, 1 = JPEG, 2 = RAW+JPEG (g7x)
#define PROPCASE_RESOLUTION                      229        // 0 = L, 2 = M1, 3 = M2, 5 = S
#define PROPCASE_ORIENTATION_SENSOR              230
#define PROPCASE_TIMER_MODE                      235        // 0 = OFF, 1 = 2 sec, 2 = 10 sec, 3 = Custom
#define PROPCASE_TIMER_DELAY                     236        // timer delay in msec
#define PROPCASE_CUSTOM_SHARPNESS_STRENGTH       509        // Canon Menu slide bar values: 0 - 7
#define PROPCASE_CUSTOM_SHARPNESS_FINENESS       510        // Canon Menu slide bar values: 1 - 5
#define PROPCASE_CUSTOM_SHARPNESS_THRESHOLD      511        // Canon Menu slide bar values: 0 - 7
#define PROPCASE_CUSTOM_CONTRAST                 512        // Canon Menu slide bar values: -4 - 4 (byte value: 252 - 4)
#define PROPCASE_CUSTOM_SATURATION               513        // Canon Menu slide bar values: -4 - 4 (byte value: 252 - 4)
#define PROPCASE_CUSTOM_COLORTONE                514        // Canon Menu slide bar values: -4 - 4 (byte value: 252 - 4)

#define PROPCASE_SUBJECT_DIST1                   257
#define PROPCASE_SV_MARKET                       258
#define PROPCASE_TV2                             274        // (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
#define PROPCASE_TV                              275        // Need to set this value for overrides to work correctly
#define PROPCASE_USER_TV                         276        //
#define PROPCASE_WB_MODE                         282        // 0 = Auto, 1 = Daylight, 2 = Shade, 3 = Cloudy, 4 = Tungsten, 5 = Fluorescent, 7 = flash, 11 = under water, 6 = Fluorescent H, 9 = Custom 1, 10 = custom 2
#define PROPCASE_WB_ADJ                          283
#define PROPCASE_SERVO_AF                        309        // 0 = Servo AF off, 1 = Servo AF on
#define PROPCASE_ASPECT_RATIO                    310        // also 404, 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1, 4 = 4:5
#define PROPCASE_SV                              357        // (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD

#define PROPCASE_REVIEW_TIME                     374        // Review time 0 = off, -1 = hold, otherwise milliseconds
#define PROPCASE_TIMER_SHOTS                     387        // Number of shots for TIMER_MODE=Custom

#define PROPCASE_SHOOTING_STATE                  362        // Goes to 1 soon after half press, 2 around when override hook called, 3 after shot start, back to 2 when shoot_full released, back to 0 when half released
#define PROPCASE_SHOOTING                       1001        // fake, emulated in platform/generic/wrappers.c using SHOOTING_STATE

#endif
