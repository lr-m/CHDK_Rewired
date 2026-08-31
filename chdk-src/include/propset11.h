#ifndef PROPSET11_H
#define PROPSET11_H

// based on propset 10

/*
constants for propset 11
Guessed, untested entries are denoted by [m10:untested], they may be completely unusable
WARNING:
The build uses tools/gen_propset_lua.sed to generate propset11.lua from this file
DO NOT USE MULTILINE COMMENTS AROUND DEFINES
*/

#define PROPCASE_AE_LOCK                         3          //? 0 = AE not locked, 1 = AE locked
#define PROPCASE_AF_ASSIST_BEAM                  5          //Y 0=disabled,  1=enabled
#define PROPCASE_REAL_FOCUS_MODE                 6          // [ixus185] AF=0, macro=1, infinity=3
#define PROPCASE_AF_FRAME                        8          //Y 1 = one point af, 2 = Face AiAF / Tracking AF
#define PROPCASE_AF_LOCK                         11         //? [m10:untested] 0 = AF not locked, 1 = AF locked (not verified, g7x AF lock just enables MF at current dist)
#define PROPCASE_CONTINUOUS_AF                   12         //Y 0 = Continuous AF off, 1 = Continuous AF on (g7x)
#define PROPCASE_FOCUS_STATE                     18         //Y sx430 1 AF done
// m10 both AV, not verified which does over and which does exif
#define PROPCASE_AV2                             22         //Y (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
#define PROPCASE_AV                              23         //Y sx430 This values causes the actual aperture value to be overriden
#define PROPCASE_MIN_AV                          28         //?seems right M10: 24, 25, 27, 28 (set only after halfshoot) elph180: (27 varies with ND, 24-26=62664)
#define PROPCASE_USER_AV                         29         //?seems right updates instantly when setting aperture on cam with kit lens
// no bracketing on m10, propcase assumed to be unchanged
#define PROPCASE_BRACKET_MODE                    33         //? [m10:untested] 0 = 0ff, 1 = exposure, 2 = focus (MF only) (g7x)
#define PROPCASE_BV                              40         //? sx430 guessed, but should be this one
#define PROPCASE_SHOOTING_MODE                   56         //Y-even 57 56 would probably show C as distinct mode but no C mode on M10
#define PROPCASE_CUSTOM_SATURATION               63         //? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_QUALITY                         65         //Y [m10:untested]
#define PROPCASE_CUSTOM_CONTRAST                 67         //? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_LANGUAGE                        69         // ixus190 (note, values were reported to have high bit of language set)
#define PROPCASE_FLASH_SYNC_CURTAIN              72         //? 0 first, 1 second
#define PROPCASE_SUBJECT_DIST2                   73         //Y [m10:zero]
// TODO M10 has no date stamp option
//#define PROPCASE_DATE_STAMP
#define PROPCASE_DELTA_SV                        87         //? sx430
// no digi zoom on m10
#define PROPCASE_DIGITAL_ZOOM_MODE               100         //YC [m10:untested] Digital Zoom Mode/State 0 = off, 1=standard, 2 = 1.5x, 3 = 2.0x
#define PROPCASE_DIGITAL_ZOOM_STATE					104		//???? required in Core/shooting.c was not
#define PROPCASE_DIGITAL_ZOOM_POSITION           104        //Y [m10:untested] also 269?
#define PROPCASE_DRIVE_MODE                      111        //Y 0 = single, 1 = cont
#define PROPCASE_OVEREXPOSURE                    112        //? [m10:untested]
#define PROPCASE_DISPLAY_MODE                    114			//Y
#define PROPCASE_EV_CORRECTION_1                 116		//Y
#define PROPCASE_FLASH_ADJUST_MODE               130        //? seems ok because of 131 [m10:untested]
#define PROPCASE_FLASH_FIRE                      131        //Y [m10:untested]
#define PROPCASE_FLASH_EXP_COMP                  136        //? APEX96 units
#define PROPCASE_FOCUS_MODE                      142        //? 0 = auto / af+mf, 1 = MF
#define PROPCASE_FLASH_MANUAL_OUTPUT             150        //? TODO guessed (ps6 had unsure comments too)
#define PROPCASE_FLASH_MODE                      152        //Y 0 = Auto, 1 = ON, 2 = OFF
// TODO values changed?
#define PROPCASE_IS_MODE                         154        //Y 0 = Continuous, 2 = OFF
#define PROPCASE_ISO_MODE                        158			//Y
#define PROPCASE_METERING_MODE                   166        //Y 0 = Evaluative, 1 = Spot, 2 = Center weighted avg 3 partial
#define PROPCASE_VIDEO_FRAMERATE                 175        //?Y 0=30, 7=60 (g7x) 1 25
#define PROPCASE_VIDEO_RESOLUTION                179        //YC 5=1920x1280, 4=1280x720 2=640x480 (g7x, ixus190)
#define PROPCASE_CUSTOM_BLUE                     185        //? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_CUSTOM_GREEN                    186        //? [m10:untested] ??Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_CUSTOM_RED                      187        //? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_CUSTOM_SKIN_TONE                188        //? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
#define PROPCASE_MY_COLORS                       196        //Y [m10:untested] 0 = Off, 1 = Vivid, 2 = Neutral, 3 = B/W, 4 = Sepia, 5 = Positive Film, 6 = Lighter Skin Tone, 7 = Darker Skin Tone, 8 = Vivid Red, 9 = Vivid Green, 10 = Vivid Blue, 11 = Custom Color
#define PROPCASE_ND_FILTER_STATE                 204        //? [m10:untested] 0 = out, 1 = in
#define PROPCASE_OPTICAL_ZOOM_POSITION           207        //Y [m10:zero]
#define PROPCASE_EXPOSURE_LOCK                   218        //? Old PROPCASE_SHOOTING value - gets set when set_aelock called or AEL button pressed
#define PROPCASE_EV_CORRECTION_2                 219        // sx430
#define PROPCASE_IS_FLASH_READY                  220       //Y [m10:untested] not certain
#define PROPCASE_IMAGE_FORMAT                    222        //? 0 = RAW, 1 = JPEG, 2 = RAW+JPEG (g7x)
#define PROPCASE_RESOLUTION                      230        //Y 0 = L, 2 = M1, 3 = M2, 5 = S
#define PROPCASE_ORIENTATION_SENSOR              231        //Y sx430
#define PROPCASE_TIMER_MODE                      236        //Y 0 = OFF, 1 = 2 sec, 2 = 10 sec, 3 = Custom
#define PROPCASE_TIMER_DELAY                     237        //Y timer delay in msec
#define PROPCASE_CUSTOM_SHARPNESS                238        //? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
// TODO guessed, no stitch on m10
//#define PROPCASE_STITCH_DIRECTION                244        // [m10:untested] 0=left>right, 1=right>left. Some cams have more
//#define PROPCASE_STITCH_SEQUENCE                 249        // [m10:untested] counts shots in stitch sequence, positive=left>right, negative=right>left
#define PROPCASE_SUBJECT_DIST1                   258        //Y [m10:zero]
#define PROPCASE_SV_MARKET                       259        //? sx430
#define PROPCASE_TV2                             275        //Y (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
#define PROPCASE_TV                              276        //Y sx430 Need to set this value for overrides to work correctly
#define PROPCASE_USER_TV                         278        //?
#define PROPCASE_WB_MODE                         283        //Y 0 = Auto, 1 = Daylight, 2 = Shade, 3 = Cloudy, 4 = Tungsten, 5 = Fluorescent, 7 = flash, 11 = under water, 6 = Fluorescent H, 9 = Custom 1, 10 = custom 2
#define PROPCASE_WB_ADJ                          284        //Y [m10:untested]
#define PROPCASE_SERVO_AF                        310        //Y 0 = Servo AF off, 1 = Servo AF on
#define PROPCASE_ASPECT_RATIO                    311        //Y also 404, 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1, 4 = 4:5
#define PROPCASE_SV                              358        //? (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
// TODO no GPS on M10
// #define PROPCASE_GPS
#define PROPCASE_TIMER_SHOTS                     388        //?Y seems right Number of shots for TIMER_MODE=Custom

//
#define PROPCASE_SHOOTING_STATE                  363        //Y Goes to 1 soon after half press, 2 around when override hook called, 3 after shot start, back to 2 when shoot_full released, back to 0 when half released
#define PROPCASE_SHOOTING                       1001        //?? fake, emulated in platform/generic/wrappers.c using SHOOTING_STATE

#endif
