#ifndef PROPSET9_H
#define PROPSET9_H

/*
constants for propset 9
WARNING:
The build uses tools/gen_propset_lua.sed to generate propset9.lua from this file
DO NOT USE MULTILINE COMMENTS AROUND DEFINES
*/

#define PROPCASE_AE_LOCK                         3          // 0 = AE not locked, 1 = AE locked
#define PROPCASE_AF_ASSIST_BEAM                  5          // 0=disabled,  1=enabled
#define PROPCASE_REAL_FOCUS_MODE                 6          // 0 = AF, 1 = Macro, 3 = INF, 4 = MF. Always 0 on M cameras
#define PROPCASE_AF_FRAME                        8          // 1 = FlexiZone, 2 = Face AiAF / Tracking AF
#define PROPCASE_AF_LOCK                            11         // 0 = AF not locked, 1 = AF locked (not verified, g7x AF lock just enables MF at current dist)
#define PROPCASE_CONTINUOUS_AF                   12         // 0 = Continuous AF off, 1 = Continuous AF on (g7x)
#define PROPCASE_FOCUS_STATE                     18         // 1 AF done
// G7x both AV, not verified which does over and which does exif
#define PROPCASE_AV2                             22         // (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
#define PROPCASE_AV                              23         // This values causes the actual aperture value to be overriden
// mismatch from propset 6 starts here, +3
#define PROPCASE_MIN_AV                          28         //
#define PROPCASE_USER_AV                         29         //
#define PROPCASE_BRACKET_MODE                    33         // 0 = 0ff, 1 = exposure, 2 = focus (MF only) (g7x)
#define PROPCASE_BV                              38			// or 40? value
#define PROPCASE_SHOOTING_MODE                   53         // 54 shows C as distinct mode
// ps6 +5
#define PROPCASE_CUSTOM_SATURATION               503        // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_QUALITY                         62
#define PROPCASE_LANGUAGE                        66         // Upper byte = language (see default.lua for known values) lowest bit: 0 = NTSC, 1 = PAL
#define PROPCASE_CUSTOM_CONTRAST                 502        // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_FLASH_SYNC_CURTAIN              69         // 0 first, 1 second
#define PROPCASE_SUBJECT_DIST2                   254			//??? 70 for g7x
// TODO Guessed as ps6 +5, g7x has no date stamp option
//#define PROPCASE_DATE_STAMP                      71         // 0 = Off, 1 = Date, 2 = Date & Time
#define PROPCASE_DELTA_SV                        84         // TODO not certain
// ps6 + 6
// TODO maybe different from older cams (off / standard are different)
#define PROPCASE_DIGITAL_ZOOM_MODE               	97         // Digital Zoom Mode/State 0 = off, 1=standard, 2 = 1.5x, 3 = 2.0x
// TODO does not seem to exist in ps7, combined with _MODE
// #define PROPCASE_DIGITAL_ZOOM_STATE
#define PROPCASE_DIGITAL_ZOOM_POSITION           101        // also 269?
#define PROPCASE_DRIVE_MODE                      108        // 0 = single, 1 = cont, 2 = cont AF
#define PROPCASE_OVEREXPOSURE                    109        // TODO guessed
#define PROPCASE_DISPLAY_MODE                    111		//???
#define PROPCASE_EV_CORRECTION_1                 113
#define PROPCASE_FLASH_ADJUST_MODE               127    // 0 = E-TTL, 1 = manual
#define PROPCASE_FLASH_FIRE                      128    // TODO guessed ps6 +6
#define PROPCASE_FLASH_EXP_COMP                  133    // APEX96 units
#define PROPCASE_FOCUS_MODE                      139    // 0 = auto, 1 = MF
#define PROPCASE_FLASH_MANUAL_OUTPUT             147        // 0 = min, 1 = med, 2 = max
#define PROPCASE_FLASH_MODE                      149        // 0 = Auto, 1 = ON, 2 = OFF
// TODO values changed?
#define PROPCASE_IS_MODE                         151        // 0 = Continuous, 1 = only Shoot, 2 = OFF (399 "dynamic IS" setting?)
#define PROPCASE_ISO_MODE                        155        // 156-MAX  AUTO ISO
#define PROPCASE_METERING_MODE                   163        // 0 = Evaluative, 1 = Spot, 2 = Center weighted avg
#define PROPCASE_VIDEO_FRAMERATE                 173        // 0=30, 7=60 (g7x)
#define PROPCASE_VIDEO_RESOLUTION                176        // 5=1920x1280, 4=1280x720 2=640x480 (g7x)
#define PROPCASE_CUSTOM_BLUE                     	182        // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_CUSTOM_GREEN                    	182        // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_CUSTOM_RED                      	184        // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_CUSTOM_SKIN_TONE                	185        // Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_MY_COLORS                       	193        // 0 = Off, 1 = Vivid, 2 = Neutral, 3 = B/W, 4 = Sepia, 5 = Positive Film, 6 = Lighter Skin Tone, 7 = Darker Skin Tone, 8 = Vivid Red, 9 = Vivid Green, 10 = Vivid Blue, 11 = Custom Color
#define PROPCASE_ND_FILTER_STATE                 	201        // 0 = out, 1 = in
#define PROPCASE_OPTICAL_ZOOM_POSITION           	204
#define PROPCASE_EXPOSURE_LOCK                   215        // and 213 Old PROPCASE_SHOOTING value - gets set when set_aelock called or AEL button pressed
#define PROPCASE_EV_CORRECTION_2                 216        // g7x ok, ps6 +6
#define PROPCASE_IS_FLASH_READY                  217        // not certain
#define PROPCASE_IMAGE_FORMAT                    219        // 0 = RAW, 1 = JPEG, 2 = RAW+JPEG (g7x)
#define PROPCASE_RESOLUTION                      227        // 0 = L, 2 = M1, 3 = M2, 5 = S
#define PROPCASE_ORIENTATION_SENSOR              228
#define PROPCASE_TIMER_MODE                      232        // 0 = OFF, 1 = 2 sec, 2 = 10 sec, 3 = Custom
#define PROPCASE_TIMER_DELAY                     233        // timer delay in msec
#define PROPCASE_CUSTOM_SHARPNESS                501        // Canon Menu slide bar values: 255, 254, 0, 1, 2
// TODO guessed propset 6 +6, stitch not present on g7x
//#define PROPCASE_STITCH_DIRECTION                242        // 0=left>right, 1=right>left. Some cams have more
//#define PROPCASE_STITCH_SEQUENCE                 247        // counts shots in stitch sequence, positive=left>right, negative=right>left
// g7x OK propset 6 +6
#define PROPCASE_SUBJECT_DIST1                   254        // 262 TargetDistanceResult MF value?
#define PROPCASE_SV_MARKET                       255		//or 256?
// ps6 + 7, TV vs TV2 not verified
#define PROPCASE_TV2                             271        // (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
#define PROPCASE_TV                              272        // Need to set this value for overrides to work correctly
#define PROPCASE_USER_TV                         274
// ps6 + 8, TODO note values changed from ps6
#define PROPCASE_WB_MODE                         279        // 0 = Auto, 1 = Daylight, 2 = Shade, 3 = Cloudy, 4 = Tungsten, 5 = Fluorescent, 7 = flash, 11 = under water, 6 = Fluorescent H, 9 = Custom 1, 10 = custom 2
#define PROPCASE_WB_ADJ                          280
#define PROPCASE_SERVO_AF                        306        // 0 = Servo AF off, 1 = Servo AF on
#define PROPCASE_ASPECT_RATIO                    307        // and 402 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1, 4 = 4:5
#define PROPCASE_SV                              354        // used for exif / UI. 256 appears similar but does not update exif
// TODO GPS guessed, ps6 + 8
// #define PROPCASE_GPS                             365        // (CHDKLover, August 2011) - contains a 272 bytes long structure
#define PROPCASE_REVIEW_TIME                     371        // Review time 0 = off, -1 = hold, 100= quick, otherwise milliseconds
#define PROPCASE_TIMER_SHOTS                     	384        // Number of shots for TIMER_MODE=Custom

//
#define PROPCASE_SHOOTING_STATE                  359        // Goes to 1 soon after half press, 2 around when override hook called, 3 after shot start, back to 2 when shoot_full released, back to 0 when half released
#define PROPCASE_SHOOTING                       1001        // fake, emulated by wrapper using SHOOTING_STATE

#endif
