// Camera - a640 - platform_camera.h

// This file contains the various settings values specific to the a640 camera.
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

    #define CAM_RAW_ROWPIX                  3736    // for 10 MP
    #define CAM_RAW_ROWS                    2772    // for 10 MP

    #undef  CAM_CIRCLE_OF_CONFUSION
    #define CAM_CIRCLE_OF_CONFUSION         6   // CoC value for camera/sensor (see http://www.dofmaster.com/digital_coc.html)

    #define CAM_SWIVEL_SCREEN               1
    #define CAM_MULTIPART                   1
    #undef  CAM_HAS_IS
    #define CAM_HAS_HI_ISO_AUTO_MODE        1
    #define CAM_CAN_MUTE_MICROPHONE         1
    #define CAM_AF_SCAN_DURING_VIDEO_RECORD 1
    #define CAM_EV_IN_VIDEO                 1
    #define CAM_ADJUSTABLE_ALT_BUTTON       1
    #define CAM_ALT_BUTTON_NAMES            { "Print", "Display" }
    #define CAM_ALT_BUTTON_OPTIONS          { KEY_PRINT, KEY_DISPLAY }

    #define CAM_DNG_LENS_INFO               { 73,10, 292,10, 28,10, 41,10 } // See comments in camera.h
    // pattern
    #define cam_CFAPattern                  0x02010100 // Red  Green  Green  Blue
    // color
    #define CAM_COLORMATRIX1                         \
      13124, 10000,   -5329, 10000,   -1390, 10000,  \
      -3602, 10000,   11658, 10000,    1944, 10000,  \
      -1612, 10000,    2863, 10000,    4885, 10000

    #define cam_CalibrationIlluminant1      17      // Standard light A
    // cropping
    #define CAM_JPEG_WIDTH                  3648
    #define CAM_JPEG_HEIGHT                 2736
    #define CAM_ACTIVE_AREA_X1              14
    #define CAM_ACTIVE_AREA_Y1              8
    #define CAM_ACTIVE_AREA_X2              3682
    #define CAM_ACTIVE_AREA_Y2              2764
    // camera name
    #define PARAM_CAMERA_NAME               4       // parameter number for GetParameterData
    #define DNG_EXT_FROM                    ".DPS"

    #define CAM_HAS_FILEWRITETASK_HOOK      1

//  #define REMOTE_SYNC_STATUS_LED  0xC0xxyyyy      // specifies an LED that turns on while camera waits for USB remote to sync

    #define CAM_SD_OVER_IN_AFL              1
    #define CAM_SD_OVER_IN_MF               1

    #define CAM_IS_VID_REC_WORKS            1   // is_video_recording() function works

//--------------------------------------------------
// Circuit Bending - a640 100b
//
// Derived by tools/newport.py from PRIMARY_a640_100b.BIN (2026-08-19). That
// dump is verified genuine: "GM1.00B" at 0xffc0a3a5 and a CRC32 over the first
// 0x31bde0 bytes matching the 0xebd4230b in sub/100b/firmware_crc_data.h. The
// detectors were re-validated against the a460/a470/a480 hand ports first, all
// twelve reference values reproduced.
//
// CAM_ADJUSTABLE_ALT_BUTTON and the button option/name lists are already set
// upstream further up this file, so they are not repeated here.

    // First DIGIC II body with the full bend set. Propset 1, 10bpp, and the
    // A640 has a real iris and no ND filter (see notes.txt) - the reverse of
    // the a530 - so iris-driven overrides behave as on the a460 rather than
    // being no-ops.
    #define CAM_BEND_MODE                   1
    #define CAM_BEND_HOLD_MS                1000

    // The second engine - docs/EXPERIMENTAL_EFFECTS.md. Nothing in it is
    // camera specific: raw.c hands it the buffers this port already supplies,
    // and the 10bpp packed fast path is the one this sensor uses. 3736x2772
    // is the largest buffer any body here runs it over.
    //
    // As on the A410, A430 and A540, the JPEG bus is a dead entry: this is a
    // VxWorks build with no "JPEG BUFF %p" logging and so no literal table for
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

    //-- the record UI ------------------------------------------------------
    //
    // Propset 1, which core/gui_recui.c has had a value map for since the
    // A410. Two of that block's assumptions do NOT hold on this body and both
    // were checked rather than assumed:
    //
    //   Native manual focus. CAM_HAS_MANUAL_FOCUS is left defined here (the
    //   A410/A430/A460/A470/A480 all #undef it), so the FOCUS row's three
    //   values need to be the AF ladder and nothing more. They are: the macro
    //   controller at 0xffdabab8 flips REAL_FOCUS_MODE between 0 and 1 only,
    //   and MF is the separate PROPCASE_FOCUS_MODE flag written 1/0 at
    //   0xffdb684c - a different control, not a fourth REAL_FOCUS_MODE value.
    //   So the row offers NORMAL/MACRO/INFINITY and gui_recui.c's clear of
    //   PROPCASE_FOCUS_MODE on write is simply "picking an AF position leaves
    //   MF", which is what Canon's own selector does. MF itself is not offered
    //   - engaging it needs the focus-distance controller and the lens
    //   handshake as well, none of which is reversed.
    //
    //   Real iris, no ND filter (notes.txt). gui_recui.c does not reference
    //   either - grep CAM_ over that file - so nothing follows from it.
    //
    // EV writes EV_CORRECTION_1 in the propset 1 block, and this ROM agrees:
    // 0xffdb52c4 does GetPropertyCase(25) then SetPropertyCase(26) as the shot
    // starts, so _2 is a copy of _1 refreshed at capture. Propcase 25 has one
    // Set site in the whole ROM (0xffec2a5c, the generic default path) and 26
    // has six - exactly the A410's and A540's signature.
    //
    // iso_table[] here has seven entries including HI; the ISO row reads that
    // at runtime rather than assuming a count.
    #define CAM_RECUI                       1

    // As on the A410, A430 and A540: the write is legal whichever way value 2
    // reads, and only the label is at risk. This ROM has a DriveModeSlct.c
    // (assert string 0xffdeb4fc) whose selector carries the self-timer
    // positions, which is what puts the timer at the third DRIVE_MODE value
    // rather than continuous AF.
    #define CAM_RECUI_DRIVE_VALS    { 0, 1, 2 }
    #define CAM_RECUI_DRIVE_NAMES   { "SINGLE", "CONTINUOUS", "TIMER" }

    // SIZE and QUALITY, from the [resolution][quality] JPEG size estimator at
    // 0xffd224b4, recovered by tools/newport.py:
    //
    //   4198400 2519040 1198080  <- 3648x2736, = CAM_JPEG_* above
    //   2785280 1658880  798720  <- 2816x2112
    //   2050048 1142784  569344  <- 2272x1704
    //   1026048  571392  284672  <- 1600x1200
    //    254976  153600   86016  <- 640x480
    //
    // Five real rows, no reserved index - unlike the A410/A430/A540, whose
    // index 3 is empty or a duplicate. The same five values and the same five
    // names as the A480, which is the other 10 MP body here.
    #define CAM_RECUI_SIZE_VALS     { 0, 1, 2, 3, 4 }
    #define CAM_RECUI_SIZE_NAMES    { "L", "M1", "M2", "M3", "S" }

    // Read straight out of the firmware, which is what makes this the ROM the
    // A540's dimensions were cross-derived from: a five-entry table of packed
    // <w,h> shorts at 0xffcb50f8, one per resolution value 0-4, agreeing row
    // for row with the estimator above. Entry 0 matches CAM_JPEG_WIDTH/HEIGHT.
    // (A sixth short pair follows it repeating 3648x2736 - it is not a sixth
    // size. The longer 3648/2592/2048/1600/1280/1024 ladders elsewhere in this
    // ROM are scaling sets; do not use them.)
    #define CAM_RECUI_SIZE_DIMS     { {3648,2736}, {2816,2112}, {2272,1704}, \
                                      {1600,1200}, { 640, 480} }

    #define CAM_RECUI_QUALITY_VALS  { 0, 1, 2 }
    #define CAM_RECUI_QUALITY_NAMES { "SUPERFINE", "FINE", "NORMAL" }

    // No CAM_RECUI_FUNC_MENU_FLAG, and none is needed - same as the A480. SET
    // opens CHDK's own FUNC menu, so there is no Canon menu on the record
    // screen to detect. (newport.py lists it as a todo because it looks for a
    // RecFuncContainer show/hide pair; this ROM has FuncContainer.c but the
    // pair is moot once CHDK owns the menu.)

    #undef  CAM_OSD_REDRAW_MASK
    #define CAM_OSD_REDRAW_MASK             1   // 40ms, matching the other three

    // Date/time screen suppression. Same mechanism as the a460/a470, addresses
    // traced in PRIMARY_a640_100b.BIN:
    //
    //   clock_is_valid() getter at 0xffc1b6dc returns *(0x00002010); the flag
    //   is written by the RTC module at 0xffc1b454. Polarity is VALID=1.
    //   The prompt gate at 0xffd8fe14 pre-tests the latch at 0x00006d44.
    //
    // Firmware version specific - re-derive for any other A640 build.
    #define CAM_CLOCK_VALID_FLAG            0x2010
    #define CAM_DATE_PROMPT_LATCH           0x6d44

    // NOT enabled: CAM_STARTUP_IMAGE. The A640 does not have the a460/a470/a480
    // arrangement of a ROM-resident default JPEG copied into a RAM table by an
    // idempotent MYCAM_INIT, so that technique has nothing to hook here.
    //
    // What it has instead: the My Camera theme lives in a WRITABLE FLASH sector
    // at 0xfff80000 - magic 0xa5a5 at 0xfff8fffe, payload size 0xe3e9 at
    // 0xfff8fff4, container magic 0x0111 with 5 entries (startup image + 4
    // sounds). Entry 0 is the 320x240 18426-byte JPEG at 0xfff800ac, confirmed
    // by SOI/EOI and SOF0. Access goes through a flash storage layer:
    //
    //   0xffed95fc  get pointer+size   -> wrapper 0xffed8f78 / 0xffed8e64
    //   0xffed951c  copy out           -> wrapper 0xffed8eb4
    //   0xffed938c  erase+write        -> wrapper 0xffed8eec
    //
    // The task side is located and matches the a480 shape:
    //   task_StartupImage        0xffd901d8   ("_StartupImage" @ 0xffd901c8)
    //   CreateTask_StartupImage  0xffd90204   ("StartupImage"  @ 0xffd901f4)
    //   worker                   0xffd9004c   (dispatches event 0x8042)
    //
    // Left undefined deliberately rather than guessed: a wrong address here is
    // a camera that does not boot, and the failure would land on a remote
    // tester. See BOOTSCREEN_PORTING.md for what is still needed.

    // NOT enabled: CAM_HOLD_SCREEN_LOCK_IN_REC. Same reasoning as the a480 and
    // a460 - only the a470 needed it, and holding this camera's refresh gate
    // has never been tested on hardware.

//--------------------------------------------------

    #undef  CAM_DEFAULT_MENU_CURSOR_BG
    #undef  CAM_DEFAULT_MENU_CURSOR_FG
    #define CAM_DEFAULT_MENU_CURSOR_BG  IDX_COLOR_RED      // Override menu cursor colors
    #define CAM_DEFAULT_MENU_CURSOR_FG  IDX_COLOR_WHITE    // Override menu cursor colors
