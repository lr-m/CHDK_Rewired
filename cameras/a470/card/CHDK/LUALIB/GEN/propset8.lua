--[[
GENERATED PROPCASE TABLE
use propcase.lua instead
--]]
return {
  AE_LOCK=3,          -- 0 = AE not locked, 1 = AE locked-----blackhole
  AF_ASSIST_BEAM=5,          -- 0=disabled,  1=enabled-----blackhole
  REAL_FOCUS_MODE=6,          -- Propcase focus_mode 0-Normal 1-Macro 4-Manual -----blackhole
  AF_FRAME=8,          -- 1 = Center, 2 = Face AiAF / Tracking AF----blackhole
  AF_LOCK=11,         -- 0 = AF not locked, 1 = AF locked
  CONTINUOUS_AF=12,         -- 0 = Continuous AF off, 1 = Continuous AF on-----blackhole
  FOCUS_STATE=18,         -- AF done-----blackhole
  AV2=22,         -- (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD---blackhole
  AV=23,         -- This values causes the actual aperture value to be overriden----blackhole
  MIN_AV=25,         --blackhole
  USER_AV=26,         --blackhole
  BRACKET_MODE=29,
  BV=35,         --blackhole-->35 & 37 ??????The same value at the same time
  SHOOTING_MODE=50,         -- jeronymo 
  CUSTOM_SATURATION=57,         -- Canon Menu slide bar values: 254, 255, 0, 1, 2-----blackhole
  QUALITY=59,         --blackhole
  CUSTOM_CONTRAST=61,         -- Canon Menu slide bar values: 254, 255, 0, 1, 2-----blackhole
  LANGUAGE=63,         -- Upper byte = language (see default.lua for known values) lowest bit: 0 = NTSC, 1 = PAL
  FLASH_SYNC_CURTAIN=66,         -- TODO guess ps6+2
  SUBJECT_DIST2=67,         -------blackhole        
  DATE_STAMP=68,         -- 0 = Off, 1 = Date, 2 = Date & Time----blackhole
  DELTA_SV=81,         --??? 88 or 89 ???-----blackhole
  DIGITAL_ZOOM_MODE=94,         -- Digital Zoom Mode/State 0 = off/standard, 2 = 1.5x, 3 = 2.0x----blackhole
  DIGITAL_ZOOM_STATE=97,         -- TODO, guessed ps6+3 required for compile Digital Zoom Mode/State 0 = Digital Zoom off, 1 = Digital Zoom on
  DIGITAL_ZOOM_POSITION=98,         --0-6-----blackhole
  DRIVE_MODE=105,        --blackhole
  OVEREXPOSURE=106,        -- TODO guessed ps6+3
  DISPLAY_MODE=108,        --blackhole
  EV_CORRECTION_1=110,        --blackhole
  FLASH_ADJUST_MODE=122,        -- TODO guessed ps6+3
  FLASH_FIRE=125,        --blackhole
  FLASH_EXP_COMP=130,        --1=96 APEX96 units-----blackhole
  FOCUS_MODE=136,        --0=AF 1=MF-----blackhole
  FLASH_MANUAL_OUTPUT=144,        -- TODO guessed ps6+3
  FLASH_MODE=146,        -- 0 = Auto, 1 = ON, 2 = OFF-----blackhole
  IS_MODE=148,        -- 0 = Continuous, 1 = only Shoot, 2 = OFF-----blackhole
  ISO_MODE=152,        -- jeronymo
  METERING_MODE=160,        -- 0 = Evaluative, 1 = Spot, 2 = Center weighted avg----blackhole
  VIDEO_FRAMERATE=170,        -- 7 = 60 FPS, 5 = 120 FPS, 4 = 240 FPS 0 = 30 FPS (sx60hs)
  VIDEO_RESOLUTION=173,        -- 5 = 1920x1080, 4 = 1280x720, 2 = 640x480, 1 = 320x240 (sx60hs)
  CUSTOM_BLUE=179,        -- Canon Menu slide bar values: 254, 255, 0, 1, 2----blackhole
  CUSTOM_GREEN=180,        -- Canon Menu slide bar values: 254, 255, 0, 1, 2----blackhole
  CUSTOM_RED=181,        -- Canon Menu slide bar values: 254, 255, 0, 1, 2----blackhole
  CUSTOM_SKIN_TONE=182,        -- Canon Menu slide bar values: 254, 255, 0, 1, 2----blackhole
  MY_COLORS=190,        -- 0 = Off, 1 = Vivid, 2 = Neutral, 3 = B/W, 4 = Sepia, 5 = Positive Film, 6 = Lighter Skin Tone, 7 = Darker Skin Tone, 8 = Vivid Red, 9 = Vivid Green, 10 = Vivid Blue, 11 = Custom Color----blackhole
  OPTICAL_ZOOM_POSITION=201,        --blackhole
  EXPOSURE_LOCK=212,     -- Old PROPCASE_SHOOTING value - gets set when set_aelock called or AEL button pressed---blackhole
  SHOOTING=307,     -- This value appears to work better - gets set to 1 when camera has focused and set exposure, returns to 0 after shot----blackhole
  EV_CORRECTION_2=213,        -- TODO guessed ps6+3
  IS_FLASH_READY=214,        --blackhole
  IMAGE_FORMAT=216,        -- 0 = RAW, 1 = JPEG, 2 = RAW+JPEG (sx60)
  RESOLUTION=224,        -- 0 = L, 2 = M1, 3 = M2, 5 = S (sx60)
  ORIENTATION_SENSOR=225,        --blackhole
  TIMER_MODE=229,        -- 0 = OFF, 1 = 2 sec, 2 = 10 sec, 3 = Custom-----blackhole
  TIMER_DELAY=230,        -- timer delay in msec----blackhole
  CUSTOM_SHARPNESS=231,        -- Canon Menu slide bar values: 254, 255, 0, 1, 2----blackhole
  STITCH_DIRECTION=239,        -- TODO guessed ps6+3 0=left>right, 1=right>left. Some cams have more
  STITCH_SEQUENCE=244,        -- TODO guessed ps6+3 counts shots in stitch sequence, positive=left>right, negative=right>left
  SUBJECT_DIST1=251,        --same as 67 & 259-----blackhole
  SV_MARKET=252,        --blackhole 253 is same value as PROPCASE_SV 252:253=672:603 ISO400 
  TV2=268,        -- (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD---blackhole
  TV=269,        -- Need to set this value for overrides to work correctly----blackhole
  USER_TV=271,        ------blackhole
  WB_MODE=276,        -- 0 = Auto, 1 = Daylight, 3 = Cloudy, 4 = Tungsten, 5 = Fluorescent, 6 = Fluorescent H, 8 = Custom-----blackhole
  WB_ADJ=277,        --(philmoz, Nov 2017) - fixed White Balance for DNG files.
  SERVO_AF=303,        -- 0 = Servo AF off, 1 = Servo AF on-----blackhole
  ASPECT_RATIO=304,        -- 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1 (sx530hs) 4 = 4:5 (sx60hs)
  SV=351,       -- (philmoz, May 2011) - this value causes overrides to be saved in JPEG and shown on Canon OSD---blackhole???
  TIMER_SHOTS=381,        -- Number of shots for TIMER_MODE=Custom----blackhole
}
