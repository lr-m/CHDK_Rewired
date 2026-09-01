// Camera - a540 - platform_camera.h

// This file contains the various settings values specific to the a540 camera.
// This file is referenced via the 'include/camera.h' file and should not be loaded directly.

// If adding a new settings value put a suitable default in 'include/camera.h',
// along with documentation on what the setting does and how to determine the correct value.
// If the setting should not have a default value then add it in 'include/camera.h'
// using the '#undef' directive along with appropriate documentation.

// Override any default values with your camera specific values in this file. Try and avoid
// having override values that are the same as the default value.

// When overriding a setting value there are two cases:
// 1. If removing the value, because it does not apply to your camera, use the '#undef' directive.
// 2. If changing the value it is best to use an '#undef' directive to remove the default value
//    followed by a '#define' to set the new value.

// When porting CHDK to a new camera, check the documentation in 'include/camera.h'
// for information on each setting. If the default values are correct for your camera then
// don't override them again in here.

    #define CAM_PROPSET                     1

    #define CAM_RAW_ROWPIX                  2888   // for 6 MP
    #define CAM_RAW_ROWS                    2136   // for 6 MP

    #undef  CAM_USE_ZOOM_FOR_MF
    #define CAM_SHOW_OSD_IN_SHOOT_MENU      1
    #undef  CAM_HAS_IS
    #define CAM_CAN_MUTE_MICROPHONE         1
    #define CAM_ADJUSTABLE_ALT_BUTTON       1
    #define CAM_ALT_BUTTON_NAMES            { "Print", "Display" }
    #define CAM_ALT_BUTTON_OPTIONS          { KEY_PRINT, KEY_DISPLAY }
    #define CAM_AF_SCAN_DURING_VIDEO_RECORD 2
    #define CAM_EV_IN_VIDEO                 1

    #define CAM_DNG_LENS_INFO               { 58,10, 232,10, 26,10, 55,10 } // See comments in camera.h
    // pattern
    #define cam_CFAPattern                  0x02010100 // Red  Green  Green  Blue
    // color
    #define CAM_COLORMATRIX1                              \
     687147, 1000000, -201964, 1000000, -125024, 1000000, \
    -148403, 1000000,  566810, 1000000,   45401, 1000000, \
      -9472, 1000000,   63186, 1000000,  208602, 1000000

    #define cam_CalibrationIlluminant1      1       // Daylight
    // cropping
    #define CAM_JPEG_WIDTH                  2816
    #define CAM_JPEG_HEIGHT                 2112
    #define CAM_ACTIVE_AREA_X1              44
    #define CAM_ACTIVE_AREA_Y1              8
    #define CAM_ACTIVE_AREA_X2              2884
    #define CAM_ACTIVE_AREA_Y2              2136
    // camera name
    #define PARAM_CAMERA_NAME               3       // parameter number for GetParameterData
    #define DNG_EXT_FROM                    ".DPS"

//  #define REMOTE_SYNC_STATUS_LED  0xC0xxyyyy      // specifies an LED that turns on while camera waits for USB remote to sync

    #undef  CAM_AF_LED
    #define CAM_AF_LED                      9

    #define CAM_HAS_FILEWRITETASK_HOOK      1

    #define CAM_SD_OVER_IN_AF               1
    #define CAM_SD_OVER_IN_AFL              1
    #define CAM_SD_OVER_IN_MF               1

// ---- Rewired Optics ------------------------------------------------------
// Derived from PRIMARY_a540_100b.BIN (GM1.00B), CRC32 0x0625df56 over
// 0xffc00000+0x2ff700 - matches firmware_crc_data.h, so the dump is genuine.

    #define CAM_BEND_MODE                   1
    #define CAM_BEND_HOLD_MS                1000

    // The second engine - docs/EXPERIMENTAL_EFFECTS.md. Nothing in it is
    // camera specific: raw.c hands it the buffers this port already supplies,
    // and the 10bpp packed fast path is the one this sensor uses.
    //
    // As on the A410 and A430, the JPEG bus is a dead entry: this is a VxWorks
    // build with no "JPEG BUFF %p" logging and so no literal table for
    // tools/newport.py to recover the encoder's buffer from (profile todo
    // JPEG_BUFF). There is no hook_jpeg_buffer() here, the generic weak one
    // returns null, and that one effect does nothing. Every other effect is
    // live.
    #define CAM_BEND_EXPERIMENTAL           1

    // This body has a real zoom rocker - kbd.c maps KEY_ZOOM_IN/KEY_ZOOM_OUT
    // at 0x40/0x80 - so the up and down arrows are not the zoom, unlike the
    // A410/A430/A470. That is what makes the two options below possible here
    // and impossible there.
    #define CAM_BEND_PRESET_ARROWS          1
    // Enter the patchbay with UP on the shooting screen as well as the hold.
    // CAM_RECUI below already blocks UP from Canon on the record screen - see
    // recui_kbd(), which swallows all four arrows on a body without
    // CAM_RECUI_UPDOWN_IS_ZOOM - so UP is dead whether or not this is set.
    // Binding it costs nothing and the hold still works.
    #define CAM_BEND_ENTER_UP               1

    // Splash-style overlay in place of the stock OSD, matching the other ports.
    #define CAM_PERSISTENT_OSD              1

    // Canon's My Camera sound slots, replaced from A/CHDK/SOUNDS/*.wav once
    // the card is up. STATUS.md used to record this body as having no My
    // Camera table; it has one, at 0x000737e0 - sub/100b/boot.c has had it
    // written down since the boot-screen work.
    //
    // The A480's technique does NOT transfer, and platform/a540/wrappers.c is
    // the whole argument: this ROM's getter rewrites the size beside the entry
    // and, on a theme change, memcpy's a flash-sized asset through the entry's
    // buffer pointer. So the pointer is left alone and the buffer contents are
    // overwritten in place instead - the same discipline boot.c uses for the
    // startup image here.
    //
    // UNVERIFIED ON HARDWARE. It writes into a Canon heap buffer, bounded by
    // the length Canon itself stored beside it, on a body nobody has booted
    // any of this on. If it misbehaves, comment out this one line.
    //
    // Requires a My Camera theme to be selected - with the theme off, the
    // allocator stores length zero for every slot and the getter reports zero
    // whatever the buffer holds.
    #define CAM_CUSTOM_SOUNDS               1

    // ...and keep CHDK's own start sound with it, as the A460 and A470 do.
    //
    // Not because this body is known to need it - it is not known either way.
    // CHDK's start sound plays on this camera today, and turning the slots on
    // without this would silently take it away (see gui_init()); entry 1, the
    // ROM's own startup sound slot, is left untouched by the code above, so
    // nothing else about startup changes. This is the option that changes the
    // least. If the body turns out to play its own and they double up, delete
    // this line.
    #define CAM_CHDK_START_SOUND            1

    //-- the record UI ------------------------------------------------------
    //
    // Propset 1, which core/gui_recui.c has had a value map for since the
    // A410. Two of that block's assumptions do NOT hold on this body and both
    // were checked rather than assumed:
    //
    //   Native manual focus. CAM_HAS_MANUAL_FOCUS is left defined here (the
    //   A410/A430/A460/A470/A480 all #undef it), so the FOCUS row's three
    //   values need to be the AF ladder and nothing more. They are: MF on this
    //   body is the separate PROPCASE_FOCUS_MODE flag, toggled 1/0 by its own
    //   controller at 0xffda3060 (which posts UI events 0x25 and 0x26), not a
    //   fourth REAL_FOCUS_MODE value. So the row offers NORMAL/MACRO/INFINITY
    //   and gui_recui.c's clear of PROPCASE_FOCUS_MODE on write is simply
    //   "picking an AF position leaves MF", which is what Canon's own selector
    //   does. MF itself is not offered - engaging it needs the focus-distance
    //   controller and the lens handshake as well, none of which is reversed.
    //
    //   Real iris, no ND filter (notes.txt). gui_recui.c does not reference
    //   either - grep CAM_ over that file - so nothing follows from it.
    //
    // EV writes EV_CORRECTION_1 in the propset 1 block, and this ROM agrees:
    // 0xffdd41a8 does GetPropertyCase(25) then SetPropertyCase(26) as the shot
    // starts, so _2 is a copy of _1 refreshed at capture. Propcase 25 has one
    // Set site in the whole ROM (0xffea729c, the generic default path) and 26
    // has seven - exactly the A410's signature.
    //
    // iso_table[] here has six entries; the ISO row reads that at runtime.
    #define CAM_RECUI                       1

    // As on the A410 and A430: the write is legal whichever way value 2 reads,
    // and only the label is at risk. This ROM does have a DriveModeSlct.c
    // (assert string 0xffdd1fd4) and its selector-apply at 0xffdd1de0 handles
    // three timer positions - 2s, 10s and custom, writing the delay to
    // propcase 14 as index*1000 ms - which is what confirms that the third
    // DRIVE_MODE position is the self timer and not continuous AF.
    #define CAM_RECUI_DRIVE_VALS    { 0, 1, 2 }
    #define CAM_RECUI_DRIVE_NAMES   { "SINGLE", "CONTINUOUS", "TIMER" }

    // SIZE and QUALITY, from the [resolution][quality] JPEG size estimator at
    // 0xffd157a8, recovered by tools/newport.py:
    //
    //   2785280 1658880 798720   <- 2816x2112, = CAM_JPEG_* above
    //   2050048 1142784 569344   <- 2272x1704
    //   1026048  571392 284672   <- 1600x1200
    //         0       0      0   <- index 3, empty
    //    254976  153600  86016   <- 640x480
    //
    // Index 3 is all zeros, exactly as on the A410, so it is skipped.
    #define CAM_RECUI_SIZE_VALS     { 0, 1, 2, 4 }
    #define CAM_RECUI_SIZE_NAMES    { "L", "M1", "M2", "S" }

    // Dimensions, which the A410 and A430 could not have. Every <w,h> ladder
    // in this ROM is a scaling set - the best of them, at 0xffc9346c, runs
    // 2816x2112 2048x1536 1600x1200 1280x960 1024x768 832x624 704x528, seven
    // entries against the estimator's four - so the still sizes are not
    // written down here as pixels.
    //
    // They are recoverable anyway, because the byte counts above are shared
    // across this whole generation. The A640's ROM has BOTH: an explicit
    // five-entry still ladder at 0xffcb50f8 AND an estimator at 0xffd224b4
    // whose rows are 4198400/2785280/2050048/1026048/254976. Four of those
    // rows are byte-identical to four of this body's, and the A640's ladder
    // names them 3648x2736 / 2816x2112 / 2272x1704 / 1600x1200 / 640x480.
    //
    // Row 0 is the anchor that ties the mapping to this camera rather than to
    // the A640: 2785280 is the A640's 2816x2112 row, and 2816x2112 is this
    // body's own CAM_JPEG_WIDTH/HEIGHT. The A430's estimator agrees from the
    // other end - its row 0 is 2050048 and its CAM_JPEG_* is 2272x1704.
    #define CAM_RECUI_SIZE_DIMS     { {2816,2112}, {2272,1704}, {1600,1200}, \
                                      { 640, 480} }

    #define CAM_RECUI_QUALITY_VALS  { 0, 1, 2 }
    #define CAM_RECUI_QUALITY_NAMES { "SUPERFINE", "FINE", "NORMAL" }

    // No CAM_RECUI_FUNC_MENU_FLAG, and none is needed - same as the A480. SET
    // opens CHDK's own FUNC menu, so there is no Canon menu on the record
    // screen to detect. (newport.py lists it as a todo because it looks for a
    // RecFuncContainer show/hide pair; this ROM has FuncContainer.c but the
    // pair is moot once CHDK owns the menu.)

    #undef  CAM_OSD_REDRAW_MASK
    #define CAM_OSD_REDRAW_MASK             1   // 40ms, matching the other ports

    // Date/time screen suppression.
    //
    //   clock_is_valid() getter at 0xffc1a76c returns *(0x00002024); the flag
    //   is written by the RTC module at 0xffc1a4f8. All three candidate prompt
    //   gates read this same word, which is what confirms it.
    //
    // Firmware version specific - re-derive for any other A540 build.
    #define CAM_CLOCK_VALID_FLAG            0x2024

    // The prompt gate, resolved in Ghidra rather than guessed:
    //
    //   FUN_ffd7f9c0:  if (clock_valid() == 0) { DateTimeMenu(); *0x6b28 = 1; }
    //
    // 0xffdf03c8 is the dialog - it asserts on "DateTimeMenu.c" at 0xffdefbe8,
    // which is what makes this conclusive. 0x6b28 is the word that path writes,
    // so it is the latch. The other two shortlisted candidates were false
    // positives: 0x6aa8 is written by an unrelated neighbour (0xffd7f9f4) and
    // 0x6f33c came from 0xffe7669c, which is a PropertyCase getter for property
    // 0xb5 and has nothing to do with the clock.
    #define CAM_DATE_PROMPT_LATCH           0x6b28

    // Boot screen. This body is the A430's case, not the A640's: 0xffebc588
    // hands the display path a RAM buffer that the allocator at 0xffebc68c
    // malloc'd and filled from flash, not a flash pointer - so there IS a
    // stable RAM copy to overwrite, and the A430's task-replacement technique
    // transfers directly.
    //
    //   MYCAM_TABLE 0x000737e0, entry 0 = item 0x2000, buffer malloc'd 0x5000
    //   stock JPEG  0xfff700ac, 18426 bytes, 320x240 - BYTE-IDENTICAL to the
    //               A430's, so boot_image.h transfers with no re-encode
    //
    // Matched on task entry address rather than name; see boot.c for the full
    // derivation. Untested on hardware - the probe in boot.c is there to
    // diagnose it if the splash comes up black or unchanged.
    // OFF for the build being sent out. The addresses and the mechanism are
    // derived and cross-checked, but replacing a task entry is the one change
    // here that could stop the camera booting, and nobody has run it on an
    // A540 yet. Everything needed is in boot.c behind this same #ifdef -
    // uncomment these two lines to build the boot-screen version.
    // #define CAM_STARTUP_IMAGE            1
    // #define CAM_STARTUP_IMAGE_TASK       0xffd7fd2c

    // NOT enabled: CAM_HOLD_SCREEN_LOCK_IN_REC. Same reasoning as the A640,
    // A480 and A460 - only the A470 needed it, and holding this camera's
    // refresh gate has never been tested on hardware.

//--------------------------------------------------

    #undef  CAM_DEFAULT_MENU_CURSOR_BG
    #undef  CAM_DEFAULT_MENU_CURSOR_FG
    #define CAM_DEFAULT_MENU_CURSOR_BG  IDX_COLOR_RED      // Override menu cursor colors
    #define CAM_DEFAULT_MENU_CURSOR_FG  IDX_COLOR_WHITE    // Override menu cursor colors

    #define CAM_IS_VID_REC_WORKS                1   // is_video_recording() function works
