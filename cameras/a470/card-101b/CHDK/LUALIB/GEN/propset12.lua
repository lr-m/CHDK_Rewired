--[[
GENERATED PROPCASE TABLE
use propcase.lua instead
--]]
return {
  AE_LOCK=3, -- [sx730: cam]
  AF_ASSIST_BEAM=5, -- [sx730: cam]
  REAL_FOCUS_MODE=6, -- sx730 0 = Normal, 1 = Macro, 4 = MF
  AF_FRAME=8, -- [sx730: cam] 1 = center, 2 = face or tracking
  AF_LOCK=11, --
  CONTINUOUS_AF=12, -- [sx730: cam]
  FOCUS_STATE=18, -- [sx730: cam]
  AV2=22, --
  AV=23, --
  MIN_AV=28, -- [sx730: cam]
  USER_AV=29, -- [sx730: cam]
  BRACKET_MODE=33, --
  BV=40, -- [sx730: cam]
  SHOOTING_MODE=56, -- [sx730: cam]
  CUSTOM_SATURATION=63, -- [sx730: cam]
  QUALITY=65, -- [sx730: cam]
  CUSTOM_CONTRAST=67, -- [sx730: cam]
  LANGUAGE=69, -- Upper byte = language (see default.lua for known values) lowest bit: 0 = NTSC, 1 = PAL
  FLASH_SYNC_CURTAIN=72, --
  SUBJECT_DIST2=73, -- [sx730: cam]
  DATE_STAMP=74, -- [sx730: cam] 0 = off, 1 = date, 2 = date + time
  DELTA_SV=87, -- [sx730: cam]
  DIGITAL_ZOOM_MODE=100, -- [sx730: cam] 0 = off, 1 = standard, 2=1.6x, 3=2x
  DIGITAL_ZOOM_POSITION=104, -- [sx730: cam]
  DRIVE_MODE=111, -- [sx730: cam] 0 = single, 1 = cont, 2 = contaf
  OVEREXPOSURE=112, -- [sx730: cam]
  DISPLAY_MODE=114, -- [sx730: cam] 0 = normal, 1 = minimal OSD
  EV_CORRECTION_1=116, -- [sx730: cam]
  FLASH_ADJUST_MODE=130, --
  FLASH_FIRE=131, --
  FLASH_EXP_COMP=136, -- [sx730: cam]
  FOCUS_MODE=142, -- [sx730: cam] 0 = AF, 1 = MF
  FLASH_MANUAL_OUTPUT=150, --
  FLASH_MODE=152, -- [m100: disasm, sx730 cam] 0 = auto, 1 = on, 2 = off
  IS_MODE=154, -- [m100: disasm, sx730: cam] sx730 0 = cont, 1 = shoot, 2 = 0ff
  ISO_MODE=158, -- [m100: disasm, sx730 cam]
  METERING_MODE=167, -- [m100: disasm]
  VIDEO_FRAMERATE=177, -- [m100: cam] sx730: 60 fps = 8, 30 fps = 0
  VIDEO_RESOLUTION=180, -- [m100: cam, sx730 cam] sx730 5 = 1080, 4 = 720, 2 = 640
  CUSTOM_BLUE=186, -- [sx730: cam]
  CUSTOM_GREEN=187, -- [sx730: cam]
  CUSTOM_RED=188, -- [sx730: cam]
  CUSTOM_SKIN_TONE=189, -- [sx730: cam]
  MY_COLORS=197, -- [sx730: cam]
  ND_FILTER_STATE=205, --
  OPTICAL_ZOOM_POSITION=208, -- [sx730: cam]
  EXPOSURE_LOCK=219, -- [sx730: cam]
  EV_CORRECTION_2=220, -- [sx730: cam]
  IS_FLASH_READY=221, -- [sx730: cam]
  IMAGE_FORMAT=223, --
  RESOLUTION=231, -- [sx730: cam] sx730 0 = L, 2 = M1, 3 = M2, 5 = S
  ORIENTATION_SENSOR=232, -- [m100: disasm, sx730: cam]
  TIMER_MODE=237, -- [m100: disasm, sx730: cam]
  TIMER_DELAY=238, -- [m100: cam, sx730: cam]
  CUSTOM_SHARPNESS=239, -- [sx730: cam]
  SUBJECT_DIST1=259, -- [m100: disasm]
  SV_MARKET=260, -- [sx730: cam]
  TV2=276, -- [sx730: cam]
  TV=277, -- [sx730: cam]
  USER_TV=279, -- [sx730: cam]
  WB_MODE=284, -- [m100: cam]
  WB_ADJ=285, --
  SERVO_AF=311, -- [m100: cam, sx730: cam]
  ASPECT_RATIO=312, -- [m100: cam, sx730: cam] sx730 0 = 4:3, 1 = 16:9, 2 = 3:2, 3 = 1:1 (also in prop 406)
  SV=359, -- [m100: cam, sx730: cam]
  REVIEW_TIME=376, -- [sx730: cam] Review time 0 = off, -1 = hold, 100 = quick, otherwise milliseconds
  TIMER_SHOTS=388, -- [m100: cam, sx730: cam]
  SHOOTING_STATE=364, -- [m100: disasm, sx730: cam]
  SHOOTING=1001, --
}
