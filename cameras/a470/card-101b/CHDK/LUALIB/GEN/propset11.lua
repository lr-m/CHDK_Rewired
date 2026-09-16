--[[
GENERATED PROPCASE TABLE
use propcase.lua instead
--]]
return {
  AE_LOCK=3,          --? 0 = AE not locked, 1 = AE locked
  AF_ASSIST_BEAM=5,          --Y 0=disabled,  1=enabled
  REAL_FOCUS_MODE=6,          -- [ixus185] AF=0, macro=1, infinity=3
  AF_FRAME=8,          --Y 1 = one point af, 2 = Face AiAF / Tracking AF
  AF_LOCK=11,         --? [m10:untested] 0 = AF not locked, 1 = AF locked (not verified, g7x AF lock just enables MF at current dist)
  CONTINUOUS_AF=12,         --Y 0 = Continuous AF off, 1 = Continuous AF on (g7x)
  FOCUS_STATE=18,         --Y sx430 1 AF done
  AV2=22,         --Y (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
  AV=23,         --Y sx430 This values causes the actual aperture value to be overriden
  MIN_AV=28,         --?seems right M10: 24, 25, 27, 28 (set only after halfshoot) elph180: (27 varies with ND, 24-26=62664)
  USER_AV=29,         --?seems right updates instantly when setting aperture on cam with kit lens
  BRACKET_MODE=33,         --? [m10:untested] 0 = 0ff, 1 = exposure, 2 = focus (MF only) (g7x)
  BV=40,         --? sx430 guessed, but should be this one
  SHOOTING_MODE=56,         --Y-even 57 56 would probably show C as distinct mode but no C mode on M10
  CUSTOM_SATURATION=63,         --? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
  QUALITY=65,         --Y [m10:untested]
  CUSTOM_CONTRAST=67,         --? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
  LANGUAGE=69,         -- ixus190 (note, values were reported to have high bit of language set)
  FLASH_SYNC_CURTAIN=72,         --? 0 first, 1 second
  SUBJECT_DIST2=73,         --Y [m10:zero]
  DELTA_SV=87,         --? sx430
  DIGITAL_ZOOM_MODE=100,         --YC [m10:untested] Digital Zoom Mode/State 0 = off, 1=standard, 2 = 1.5x, 3 = 2.0x
  DIGITAL_ZOOM_STATE=104,		--???? required in Core/shooting.c was not
  DIGITAL_ZOOM_POSITION=104,        --Y [m10:untested] also 269?
  DRIVE_MODE=111,        --Y 0 = single, 1 = cont
  OVEREXPOSURE=112,        --? [m10:untested]
  DISPLAY_MODE=114,			--Y
  EV_CORRECTION_1=116,		--Y
  FLASH_ADJUST_MODE=130,        --? seems ok because of 131 [m10:untested]
  FLASH_FIRE=131,        --Y [m10:untested]
  FLASH_EXP_COMP=136,        --? APEX96 units
  FOCUS_MODE=142,        --? 0 = auto / af+mf, 1 = MF
  FLASH_MANUAL_OUTPUT=150,        --? TODO guessed (ps6 had unsure comments too)
  FLASH_MODE=152,        --Y 0 = Auto, 1 = ON, 2 = OFF
  IS_MODE=154,        --Y 0 = Continuous, 2 = OFF
  ISO_MODE=158,			--Y
  METERING_MODE=166,        --Y 0 = Evaluative, 1 = Spot, 2 = Center weighted avg 3 partial
  VIDEO_FRAMERATE=175,        --?Y 0=30, 7=60 (g7x) 1 25
  VIDEO_RESOLUTION=179,        --YC 5=1920x1280, 4=1280x720 2=640x480 (g7x, ixus190)
  CUSTOM_BLUE=185,        --? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
  CUSTOM_GREEN=186,        --? [m10:untested] ??Canon Menu slide bar values: 255, 254, 0, 1, 2
  CUSTOM_RED=187,        --? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
  CUSTOM_SKIN_TONE=188,        --? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
  MY_COLORS=196,        --Y [m10:untested] 0 = Off, 1 = Vivid, 2 = Neutral, 3 = B/W, 4 = Sepia, 5 = Positive Film, 6 = Lighter Skin Tone, 7 = Darker Skin Tone, 8 = Vivid Red, 9 = Vivid Green, 10 = Vivid Blue, 11 = Custom Color
  ND_FILTER_STATE=204,        --? [m10:untested] 0 = out, 1 = in
  OPTICAL_ZOOM_POSITION=207,        --Y [m10:zero]
  EXPOSURE_LOCK=218,        --? Old PROPCASE_SHOOTING value - gets set when set_aelock called or AEL button pressed
  EV_CORRECTION_2=219,        -- sx430
  IS_FLASH_READY=220,       --Y [m10:untested] not certain
  IMAGE_FORMAT=222,        --? 0 = RAW, 1 = JPEG, 2 = RAW+JPEG (g7x)
  RESOLUTION=230,        --Y 0 = L, 2 = M1, 3 = M2, 5 = S
  ORIENTATION_SENSOR=231,        --Y sx430
  TIMER_MODE=236,        --Y 0 = OFF, 1 = 2 sec, 2 = 10 sec, 3 = Custom
  TIMER_DELAY=237,        --Y timer delay in msec
  CUSTOM_SHARPNESS=238,        --? [m10:untested] Canon Menu slide bar values: 255, 254, 0, 1, 2
  SUBJECT_DIST1=258,        --Y [m10:zero]
  SV_MARKET=259,        --? sx430
  TV2=275,        --Y (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
  TV=276,        --Y sx430 Need to set this value for overrides to work correctly
  USER_TV=278,        --?
  WB_MODE=283,        --Y 0 = Auto, 1 = Daylight, 2 = Shade, 3 = Cloudy, 4 = Tungsten, 5 = Fluorescent, 7 = flash, 11 = under water, 6 = Fluorescent H, 9 = Custom 1, 10 = custom 2
  WB_ADJ=284,        --Y [m10:untested]
  SERVO_AF=310,        --Y 0 = Servo AF off, 1 = Servo AF on
  ASPECT_RATIO=311,        --Y also 404, 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1, 4 = 4:5
  SV=358,        --? (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
  TIMER_SHOTS=388,        --?Y seems right Number of shots for TIMER_MODE=Custom
  SHOOTING_STATE=363,        --Y Goes to 1 soon after half press, 2 around when override hook called, 3 after shot start, back to 2 when shoot_full released, back to 0 when half released
  SHOOTING=1001,        --?? fake, emulated in platform/generic/wrappers.c using SHOOTING_STATE
}
