--[[
GENERATED PROPCASE TABLE
use propcase.lua instead
--]]
return {
  AE_LOCK=3,          -- 0 = AE not locked, 1 = AE locked
  AF_ASSIST_BEAM=5,          -- 0=disabled,  1=enabled
  REAL_FOCUS_MODE=6,          -- 0 = Normal AF, 1 = Macro, 4 = MF
  AF_FRAME=8,          -- 1 = one point af, 2 = Face AiAF / Tracking AF
  AF_LOCK=11,         -- 0 = AF not locked, 1 = AF locked (not verified, g7x AF lock just enables MF at current dist)
  CONTINUOUS_AF=12,         -- 0 = Continuous AF off, 1 = Continuous AF on (g7x)
  FOCUS_STATE=18,         -- 1 AF done
  AV2=22,         -- (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
  AV=23,         -- This values causes the actual aperture value to be overriden
  MIN_AV=28,         -- G7X2 = 28
  USER_AV=29,         -- updates instantly when setting aperture on cam
  BRACKET_MODE=33,         -- 0 = 0ff, 1 = exposure, 2 = focus (MF only) (g7x)
  BV=40,         -- guessed, but should be this one
  SHOOTING_MODE=55,         -- 56 would probably show C as distinct mode but no C mode on M10
  QUALITY=64,
  LANGUAGE=68,         -- Upper byte = language (see default.lua for known values) lowest bit: 0 = NTSC, 1 = PAL
  FLASH_SYNC_CURTAIN=71,         -- 0 first, 1 second
  SUBJECT_DIST2=72,
  DELTA_SV=86,
  DIGITAL_ZOOM_MODE=99,         -- Digital Zoom Mode/State 0 = off, 1=standard, 2 = 1.5x, 3 = 2.0x
  DIGITAL_ZOOM_POSITION=103,
  DRIVE_MODE=110,        -- 0 = single, 1 = cont, 4 = high-speed continuous (G7X2)
  OVEREXPOSURE=111,
  DISPLAY_MODE=113,
  EV_CORRECTION_1=115,
  FLASH_ADJUST_MODE=129,
  FLASH_FIRE=130,
  FLASH_EXP_COMP=135,        -- APEX96 units
  FOCUS_MODE=141,        -- 0 = auto / af+mf, 1 = MF
  FLASH_MANUAL_OUTPUT=149,        -- TODO guessed (ps6 had unsure comments too)
  FLASH_MODE=151,        -- 0 = Auto, 1 = ON, 2 = OFF
  IS_MODE=153,        -- 0 = Continuous, 2 = OFF
  ISO_MODE=157,
  METERING_MODE=165,        -- 0 = Evaluative, 1 = Spot, 2 = Center weighted avg 3 partial
  VIDEO_FRAMERATE=175,        -- 0=30, 7=60 (g7x) 1 25
  VIDEO_RESOLUTION=178,        -- 5=1920x1280, 4=1280x720 2=640x480 (g7x)
  ND_FILTER_STATE=203,        -- 0 = out, 1 = in
  OPTICAL_ZOOM_POSITION=206,
  EXPOSURE_LOCK=217,        -- Old PROPCASE_SHOOTING value - gets set when set_aelock called or AEL button pressed
  EV_CORRECTION_2=218,        -- g7x ok, ps6 +6
  IS_FLASH_READY=219,
  IMAGE_FORMAT=221,        -- 0 = RAW, 1 = JPEG, 2 = RAW+JPEG (g7x)
  RESOLUTION=229,        -- 0 = L, 2 = M1, 3 = M2, 5 = S
  ORIENTATION_SENSOR=230,
  TIMER_MODE=235,        -- 0 = OFF, 1 = 2 sec, 2 = 10 sec, 3 = Custom
  TIMER_DELAY=236,        -- timer delay in msec
  CUSTOM_SHARPNESS_STRENGTH=509,        -- Canon Menu slide bar values: 0 - 7
  CUSTOM_SHARPNESS_FINENESS=510,        -- Canon Menu slide bar values: 1 - 5
  CUSTOM_SHARPNESS_THRESHOLD=511,        -- Canon Menu slide bar values: 0 - 7
  CUSTOM_CONTRAST=512,        -- Canon Menu slide bar values: -4 - 4 (byte value: 252 - 4)
  CUSTOM_SATURATION=513,        -- Canon Menu slide bar values: -4 - 4 (byte value: 252 - 4)
  CUSTOM_COLORTONE=514,        -- Canon Menu slide bar values: -4 - 4 (byte value: 252 - 4)
  SUBJECT_DIST1=257,
  SV_MARKET=258,
  TV2=274,        -- (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
  TV=275,        -- Need to set this value for overrides to work correctly
  USER_TV=276,        --
  WB_MODE=282,        -- 0 = Auto, 1 = Daylight, 2 = Shade, 3 = Cloudy, 4 = Tungsten, 5 = Fluorescent, 7 = flash, 11 = under water, 6 = Fluorescent H, 9 = Custom 1, 10 = custom 2
  WB_ADJ=283,
  SERVO_AF=309,        -- 0 = Servo AF off, 1 = Servo AF on
  ASPECT_RATIO=310,        -- also 404, 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1, 4 = 4:5
  SV=357,        -- (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD
  REVIEW_TIME=374,        -- Review time 0 = off, -1 = hold, otherwise milliseconds
  TIMER_SHOTS=387,        -- Number of shots for TIMER_MODE=Custom
  SHOOTING_STATE=362,        -- Goes to 1 soon after half press, 2 around when override hook called, 3 after shot start, back to 2 when shoot_full released, back to 0 when half released
  SHOOTING=1001,        -- fake, emulated in platform/generic/wrappers.c using SHOOTING_STATE
}
