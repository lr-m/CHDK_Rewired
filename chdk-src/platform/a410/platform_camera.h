// Camera - A410 - platform_camera.h

// This file contains the various settings values specific to the A410 camera.
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

    #define CAM_RAW_ROWPIX                  2144  // for 3.34 MP 1/3.2" sensor size
    #define CAM_RAW_ROWS                    1560  // for 3.34 MP 1/3.2" sensor size

    #undef  CAM_USE_ZOOM_FOR_MF
    #undef  CAM_HAS_ZOOM_LEVER
    #define CAM_DRAW_EXPOSITION             1
    #undef  CAM_HAS_ERASE_BUTTON
    #undef  CAM_HAS_IRIS_DIAPHRAGM
    #define CAM_HAS_ND_FILTER               1
    #undef  CAM_HAS_MANUAL_FOCUS
    #define CAM_ADJUSTABLE_ALT_BUTTON       1
    #define CAM_ALT_BUTTON_NAMES            { "Print", "Display" }
    #define CAM_ALT_BUTTON_OPTIONS          { KEY_PRINT, KEY_DISPLAY }

    #undef  CAM_HAS_USER_TV_MODES
    #define CAM_SHOW_OSD_IN_SHOOT_MENU      1
    #undef  CAM_HAS_IS

    #define CAM_EV_IN_VIDEO                 1 //but not very reliable...

    #define CAM_DNG_LENS_INFO               { 54,10, 173,10, 28,10, 51,10 } // See comments in camera.h

    // pattern
    #define cam_CFAPattern                  0x02010100 // Red  Green  Green  Blue
    // color
/*  #define CAM_COLORMATRIX1                                \ //copy from A430
      479627,  1000000, -156240, 1000000,  -84926, 1000000, \
     -215238,  1000000,  534902, 1000000,   60219, 1000000, \
      -96906,  1000000,  148194, 1000000,  191583, 1000000
*/
    //well, first try, probably not accurate enough (used dng2ps2):
    #define CAM_COLORMATRIX1                                \
     270041, 1000000, -110546, 1000000,  -43914, 1000000,   \
    -121712, 1000000,  256251, 1000000,   12623, 1000000,   \
     -27956, 1000000,   36119, 1000000,  104654, 1000000

    #define cam_CalibrationIlluminant1      1 // Daylight
    // cropping
    #define CAM_JPEG_WIDTH                  2048
    #define CAM_JPEG_HEIGHT                 1536
    #define CAM_ACTIVE_AREA_X1              2
    #define CAM_ACTIVE_AREA_Y1              6
    #define CAM_ACTIVE_AREA_X2              2090
    #define CAM_ACTIVE_AREA_Y2              1558
    // camera name
    #define PARAM_CAMERA_NAME               3 // parameter number for GetParameterData

    #define CAM_HAS_FILEWRITETASK_HOOK      1

//  #define CAM_MULTIPART                   1

//    #define REMOTE_SYNC_STATUS_LED     0xC0xxyyyy                // specifies an LED that turns on while camera waits for USB remote to sync

    // "real" to "market" conversion definitions
    #define SV96_MARKET_OFFSET              -4          // market-real sv96 conversion value

    // Conversion values for pow(2,4/96) 'market' to 'real', and pow(2,-4/96) 'real' to 'market'
    // Uses integer arithmetic to avoid floating point calculations. Values choses to get as close
    // to the desired multiplication factor as possible within normal ISO range.
    #define ISO_MARKET_TO_REAL_MULT         8432
    #define ISO_MARKET_TO_REAL_SHIFT        14
    #define ISO_MARKET_TO_REAL_ROUND        8192
    #define ISO_REAL_TO_MARKET_MULT         3979
    #define ISO_REAL_TO_MARKET_SHIFT        12
    #define ISO_REAL_TO_MARKET_ROUND        2048

    #define CAM_SD_OVER_IN_AF               1
    #define CAM_SD_OVER_IN_AFL              1
    #define CAM_SD_OVER_IN_MF               1

    #define CAM_IS_VID_REC_WORKS            1   // is_video_recording() function works

//--------------------------------------------------

    #undef  CAM_DEFAULT_MENU_CURSOR_BG
    #undef  CAM_DEFAULT_MENU_CURSOR_FG
    #define CAM_DEFAULT_MENU_CURSOR_BG  IDX_COLOR_RED      // Override menu cursor colors
    #define CAM_DEFAULT_MENU_CURSOR_FG  IDX_COLOR_WHITE    // Override menu cursor colors

//--------------------------------------------------
// Circuit Bending - see BENDING_DESIGN.md
//
// Derived from the A410 100e firmware. The A410 is the closest sibling of the A460 the
// project has: VxWorks, DIGIC II, a 10 bit 2144x1560 sensor, and a keymap
// identical to it down to the bit masks. Everything below is the A460's
// configuration, and it is that way because the two cameras genuinely match,
// not because it was copied and hoped for.

    // Same two spare buttons as the A460. The A470 has neither Display nor
    // zoom keys in its keymap, which is why bend mode never bound anything to
    // them; here Display is free and worth offering.
    #define CAM_ADJUSTABLE_ALT_BUTTON       1
    #define CAM_ALT_BUTTON_NAMES            { "Print", "Display" }
    #define CAM_ALT_BUTTON_OPTIONS          { KEY_PRINT, KEY_DISPLAY }

    #define CAM_BEND_MODE                   1
    #define CAM_BEND_HOLD_MS                1000

    // The second engine - docs/EXPERIMENTAL_EFFECTS.md. Enabled here on the
    // strength of the second-wave detectors, which found this
    // camera has everything the engine needs from a port except one thing.
    //
    // The engine itself carries no address; raw.c hands it the buffers the
    // port already supplies, and the 10 bit packed fast path is the one this
    // sensor uses. What is missing is the JPEG encoder's working buffer: the
    // A470 and A480 both log their imaging buffers from a literal table ahead
    // of a "JPEG BUFF %p" format string, and this ROM has no such string at
    // all - the VxWorks build does not log them that way. So there is no
    // hook_jpeg_buffer() here, the generic weak one returns null, and the
    // JPEG bus is a dead entry in the EXPERIMENTAL list. The other effects do
    // not need another camera-specific address.
    //
    // Not tested on hardware yet.
    #define CAM_BEND_EXPERIMENTAL           1

    // The record-screen selectors, drawn by CHDK. Propset 1 maps derived from
    // the A410 firmware - see the CAM_PROPSET == 1 block in core/gui_recui.c
    // for what each value rests on.
    //
    // With this on, the #ifndef CAM_RECUI handover in core/gui.c compiles out -
    // CHDK takes the arrow keys before the firmware sees them, so Canon's
    // selectors never open and there is nothing to hand the bitmap back for.
    #define CAM_RECUI                       1

    // This body has no separate zoom control: the up and down arrows are the
    // zoom rocker on the shooting screen, and there is nowhere else to put it.
    // So the record UI takes neither of them - DOWN loses the DRIVE selector
    // it has on the A480, and UP is not blocked the way the note at the foot
    // of recui_kbd() blocks it there. DRIVE is still on the FUNC menu, which
    // is where every control is regardless.
    //
    // This is also why bend mode is entered by holding the ALT button here and
    // not by CAM_BEND_ENTER_UP: UP is spoken for by the lens.
    #define CAM_RECUI_UPDOWN_IS_ZOOM        1

    // The three per-camera rows, reversed out of the A410 firmware.
    //
    // SIZE and QUALITY both come from one function: FUN_ffcfb6cc(resolution,
    // quality), the JPEG size estimator that CompressionSlct.c and SizeSlct.c
    // both call. It is unusually explicit about its own domain -
    //
    //   cmp r0,#6 / ldrls pc,[pc,r0,lsl #2]     resolution 0..6, else assert
    //   r3 = quality % 3                        quality is taken mod 3
    //   r4 = tbl_0xffcfb678[3*resolution + quality]
    //
    // - so the table at 0xffcfb678 is [7][3] and reading it settles both rows:
    //
    //         qual0      qual1     qual2
    //   res0  1640448    914432    455680     <- 2048x1536, = CAM_JPEG_* above
    //   res1  1026048    571392    284672
    //   res2   583680    327680    174080
    //   res3        0         0         0     <- unused
    //   res4   254976    153600     86016
    //
    // Sizes fall monotonically left to right on every valid row, which orders
    // quality 0/1/2 as SUPERFINE/FINE/NORMAL. Rows 5 and 6 hold string data,
    // not sizes - res 5 takes a fixed constant in code and res 6 indexes the
    // table by something else - so neither is a still size. Row 3 is all zeros.
    //
    // That leaves 0/1/2/4, which is *exactly* the "0 = L, 1 = M1, 2 = M2,
    // 4 = S" pattern several propsetN.h headers document, and exactly the four
    // still sizes this camera is specified to have.
    #define CAM_RECUI_SIZE_VALS     { 0, 1, 2, 4 }
    #define CAM_RECUI_SIZE_NAMES    { "L", "M1", "M2", "S" }
    // The ROM has no separate four-entry <w,h> table, but these are the
    // documented still sizes in the L/M1/M2/S order proven above.
    #define CAM_RECUI_SIZE_DIMS     { {2048,1536}, {1600,1200}, {1024,768}, \
                                      { 640, 480} }
    #define CAM_RECUI_QUALITY_VALS  { 0, 1, 2 }
    #define CAM_RECUI_QUALITY_NAMES { "SUPERFINE", "FINE", "NORMAL" }

    // 0 = single and 1 = continuous are agreed by every propset header. The
    // third is the one label in this port still unconfirmed: the newer propsets
    // call 2 "continuous AF" while the A480 calls it TIMER, and nothing in this
    // ROM settles which this body means - propset1.h has no timer propcase at
    // all, which is a point in TIMER's favour but not proof.
    //
    // It is included because the *write* is legal either way and both readings
    // are harmless and reversible by cycling the key; only the label is at
    // risk, which is the standard this project already uses for the A480's
    // uncertain rows. If it turns out to be continuous AF, it is one word here.
    #define CAM_RECUI_DRIVE_VALS    { 0, 1, 2 }
    #define CAM_RECUI_DRIVE_NAMES   { "SINGLE", "CONTINUOUS", "TIMER" }

    // 40ms instead of 80ms, matching the other three - halves the latency of
    // the persistent overlay without measurably costing anything.
    #undef  CAM_OSD_REDRAW_MASK
    #define CAM_OSD_REDRAW_MASK             1
    #define CAM_PERSISTENT_OSD              1

    // NOT ported, and both blocked on further tracing rather than on effort:
    //
    //   CAM_STARTUP_IMAGE     the My Camera machinery is present in this ROM
    //                         (the "StartupImage" and "MyCameraIn" strings are
    //                         there) but there is no 320x240 JPEG anywhere in
    //                         it - the A460 has one at 0xffe61674, this camera
    //                         has nothing above 2380 bytes. So the default
    //                         splash is not stored the way the other three
    //                         store it, and patching a buffer we have not
    //                         found is not something to guess at.
    //
    // Date/time prompt suppression, found by anchoring on
    // the RTC module's own "RTC.c" assert string rather than on the test-and-set
    // idiom - that idiom alone matches 73 sites in this ROM.
    //
    //   0x200c  clock_is_valid() at 0xffc1a748 returns this word; the RTC
    //           module sets it to 1 at 0xffc1a410. It is the only global in
    //           the ROM that is both.
    //   0x70e0  the "already asked" latch, tested and set at 0xffd5c6c8.
    //
    // Unlike the A460, this camera splits the gate across two adjacent tiny
    // functions - 0xffd5c6c8 owns the latch and tail-calls the prompt, while
    // 0xffd5c6f4 calls the clock getter and tail-calls onward - rather than
    // combining both tests in one. Same semantics, so writing 1 to both words
    // suppresses the screen the same way it does on the other two.
    #define CAM_CLOCK_VALID_FLAG            0x200c
    #define CAM_DATE_PROMPT_LATCH           0x70e0
