#ifndef MODELIST_H
#define MODELIST_H

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

/*
CHDK capture mode constants.
WARNING: This file is used with gen_modelist_lua.sed to generate modelist.lua
WARNING: These are used for platform independent script values.
Changing order or inserting new values will break script compatibility.
ADD NEW VALUES AT THE END!

0 is used as an invalid value
not every value is valid on every camera

Single line comments on the enum values will be carried over to lua

modemap notes:
- On cameras where the "manual" mode only allows +/- ev, rather than direct shutter
control, it should be mapped to P, not M
- Modes should be mapped to an enum value that matches their canon name. This is
displayed as a string when the mode is set, and can also be found in canon
manuals and spec lists. If in doubt refer to the canon manuals for description of
the modes function, and compare with existing cameras. Add a new enum value at the
end of the list if it doesn't closely match any existing function.
- As of CHDK 1.4, "scene" modes are not distinguished frome other modes. Previous
versions of CHDK used different enum values, e.g. MODE_PORTRAIT and MODE_SCN_PORTRAIT
- Some cameras have C, or C1 and C2 modes. These are not actual shooting modes, but
are used to load saved settings for other modes like M, P etc. These modes can be
set using _SetCurrrentCaptureMode, but are not currently supported by the modemap
system. If a C mode is set this way, the propcase used for PROPCASE_SHOOTING_MODE
on propset 2 cameras (49) reflects the actual mode. Propcase 50 appears to show the
set mode. Propset 1 behavior is unknown.
- Although canon mode values are similar between many cameras, they are not always the same!
- a list of valid canon modes can be found in the firmware, see existing cameras
for examples. This can be found several function calls after a reference to the
string AC:PTM_Init or similar
*/
enum {
    MODE_AUTO               =1,
    MODE_P                  , // Called "camera manual" on many cameras without a true manual mode, only allows +/- Ev adjustment
    MODE_TV                 ,
    MODE_AV                 ,
    MODE_M                  , // note, use only for true manual modes that allow direct control of Tv/Av

    MODE_AQUARIUM           ,
    MODE_BEACH              ,
    MODE_BEST_IMAGE         ,
    MODE_BLUR_REDUCTION     , // a800
    MODE_COLOR_ACCENT       ,
    MODE_COLOR_SWAP         ,
    MODE_CREATIVE_EFFECT    , // "creative light effect", only known on ixus950_sd850
    MODE_DIGITAL_IS         , // a810/a2300
    MODE_DIGITAL_MACRO      ,
    MODE_DISCREET           , // A3300is
    MODE_EASY               ,
    MODE_FACE_SELF_TIMER    , // sx30/g12 (Smart Shutter, Face Self Timer mode)
    MODE_FIREWORK           , // ixus1000 end
    MODE_FISHEYE            ,
    MODE_FOLIAGE            ,
    MODE_HDR                , // g12 (HDR scene mode)
    MODE_HIGHSPEED_BURST    ,
    MODE_INDOOR             ,
    MODE_ISO_3200           ,
    MODE_KIDS_PETS          ,
    MODE_LANDSCAPE          ,
    MODE_LIVE               , // A3300is
    MODE_LONG_SHUTTER       , // "long shutter" mode on cameras without true manual mode. Allows manual shutter >= 1 second, uses manual shutter value propcase. Usually found under func menu in "manual" mode.
    MODE_LOWLIGHT           , // g11
    MODE_MINIATURE          ,
    MODE_MONOCHROME         , // sx220
    MODE_MY_COLORS          ,
    MODE_NIGHT_SCENE        , // "night scene" mode. Note, this can be a dial position, or under the scene menu (SCN_NIGHT_SCENE).
    MODE_NIGHT_SNAPSHOT     ,
    MODE_NOSTALGIC          , // s90
    MODE_PORTRAIT           ,
    MODE_POSTER_EFFECT      ,
    MODE_QUICK              ,
    MODE_SMART_SHUTTER      , // ixus1000_sd4500 - the following are not under SCN
    MODE_SMOOTH_SKIN        , // sx260
    MODE_SNOW               ,
    MODE_SOFTFOCUS          , // sx260 asm1989
    MODE_SPORTS             ,
    MODE_STITCH             ,
    MODE_SUNSET             ,
    MODE_SUPER_MACRO        ,
    MODE_SUPER_VIVID        ,
    MODE_TOY_CAMERA         , // sx220
    MODE_UNDERWATER         ,
    MODE_UNDERWATER_MACRO   , // D20
    MODE_WINK_SELF_TIMER    , // sx30/g12 (Smart Shutter, Wink Self Timer mode)

    MODE_VIDEO_COLOR_ACCENT ,
    MODE_VIDEO_COLOR_SWAP   ,
    MODE_VIDEO_COMPACT      ,
    MODE_VIDEO_HIRES        ,
    MODE_VIDEO_IFRAME_MOVIE , // sx220
    MODE_VIDEO_MINIATURE    , // g12 (miniature effect video mode)
    MODE_VIDEO_MOVIE_DIGEST , // sx220 (the camera automatically record a short video clip (up to approximately 4 seconds) every time you shoot
    MODE_VIDEO_MY_COLORS    ,
    MODE_VIDEO_SPEED        ,
    MODE_VIDEO_STD          ,
    MODE_VIDEO_SUPER_SLOW   , // IXUS 310 HS Super Slow Motion Movie
    MODE_VIDEO_TIME_LAPSE   ,

    MODE_HYBRID_AUTO        , // sx280
    MODE_BACKGROUND_DEFOCUS , // g7x
    MODE_STAR_PORTRAIT      , // g7x
    MODE_STAR_NIGHTSCAPE    , // g7x
    MODE_STAR_TRAILS        , // g7x

    MODE_VIDEO_M            , // g7x manual video
    MODE_VIDEO_STAR_TIME_LAPSE, // g7x not clear if this should be VIDEO

    MODE_ART_BOLD           , // art bold, m10
    MODE_WATERPAINTING      , // water painting, m10
    MODE_CLOSEUP            , // close-up, m10
    MODE_FOOD               , // food, m10
    MODE_SELFIE             , // self portrait, m10

    MODE_VIDEO_SHORT_CLIP   , // sx710 (short clips with special effects like slomo, individual effects don't get their own mode)
    MODE_ZOOM_BLUR          , // ixus990_sd970 "The optical zoom operates automatically during shooting"

    MODE_PANNING            , // G7X2

};

#endif
