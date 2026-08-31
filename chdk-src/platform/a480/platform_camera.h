// Camera - a480 - platform_camera.h

// This file contains the various settings values specific to the a480 camera.
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

    #define CAM_PROPSET                     2
    #define CAM_DRYOS                       1

    #define CAM_RAW_ROWPIX                  3720
    #define CAM_RAW_ROWS                    2772

    #undef  CAM_USE_ZOOM_FOR_MF
    #undef  CAM_HAS_ERASE_BUTTON                    // Camera does not have Erase button
    #undef  CAM_HAS_DISP_BUTTON                     // Camera does not have DISP button
    #undef  CAM_HAS_IRIS_DIAPHRAGM
    #define CAM_HAS_ND_FILTER               1
    #undef  CAM_HAS_MANUAL_FOCUS
    #undef  CAM_HAS_USER_TV_MODES
    #undef  CAM_HAS_IS
    #define CAM_MULTIPART                   1
    #undef  CAM_VIDEO_CONTROL
    #define CAM_REAR_CURTAIN                1
    #undef  DEFAULT_RAW_EXT
    #define DEFAULT_RAW_EXT                 2       // use .CR2
    #define CAM_AF_SCAN_DURING_VIDEO_RECORD 1
    #define CAM_CAN_MUTE_MICROPHONE         1
    #define CAM_CUSTOM_SOUNDS               1
    #define CAM_CUSTOM_BOOT_IMAGE           1
    #define CAM_EV_IN_VIDEO                 1

    #define CAM_DNG_LENS_INFO               { 66,10, 216,10, 30,10, 58,10 } // See comments in camera.h
    // pattern
    #define cam_CFAPattern                  0x02010100 // Red  Green  Green  Blue
    // color

    #define CAM_COLORMATRIX1                              \
     827547, 1000000, -290458, 1000000, -126086, 1000000, \
     -12829, 1000000,  530507, 1000000,   50537, 1000000, \
       5181, 1000000,   48183, 1000000,  245014, 1000000

    #define cam_CalibrationIlluminant1      1       // Daylight
    // cropping
    #define CAM_JPEG_WIDTH                  3648
    #define CAM_JPEG_HEIGHT                 2736
    #define CAM_ACTIVE_AREA_X1              6
    #define CAM_ACTIVE_AREA_Y1              12
    #define CAM_ACTIVE_AREA_X2              3690
    #define CAM_ACTIVE_AREA_Y2              2772

    // camera name
    #define PARAM_CAMERA_NAME               4       // parameter number for GetParameterData
    #undef  CAM_SENSOR_BITS_PER_PIXEL
    #define CAM_SENSOR_BITS_PER_PIXEL       12

    #define CAM_DRIVE_MODE_FROM_TIMER_MODE  1   // use PROPCASE_TIMER_MODE to check for multiple shot custom timer.
                                                // Used to enabled bracketing in custom timer, required on many recent cameras
                                                // see http://chdk.setepontos.com/index.php/topic,3994.405.html

    #define CAM_HAS_FILEWRITETASK_HOOK      1

    #define REMOTE_SYNC_STATUS_LED          0xC0220088  // specifies an LED that turns on while camera waits for USB remote to sync

    #define CAM_SD_OVER_IN_AF               1
    #define CAM_SD_OVER_IN_AFL              1
    #define CAM_SD_OVER_IN_MF               1

    #define CAM_IS_VID_REC_WORKS            1   // is_video_recording() function works

    #undef  CAM_OSD_REDRAW_MASK
    #define CAM_OSD_REDRAW_MASK             1   // 40ms instead of 80ms - halves the
                                                // visible gap after a Canon repaint

    // Splash-style overlay in place of the stock OSD - see camera.h and
    // gui_draw_persistent_osd() in core/gui.c. Enabled here for appearance
    // rather than necessity: the stock OSD works on this body, unlike on the
    // a470 where it was replaced because one of its element functions blocks
    // spytask for most of a second. But the two cameras are used side by side
    // and there is no reason for them to read differently, and the overlay is
    // laid out for what this project actually wants on screen - raw state, bend
    // state, frames left - which the stock elements do not show at all.
    //
    // Nothing about it depends on the a470's held refresh gate. It repaints on
    // every redraw pass and samples on its own timers, so Canon repainting over
    // it here costs the same as it costs the stock OSD, which is to say the
    // CAM_OSD_REDRAW_MASK gap above and nothing more.
    #define CAM_PERSISTENT_OSD              1

    // Readout colours come from the selected theme. The old per-boot shuffle
    // is deliberately not enabled: it made the same theme look different on
    // every start and bypassed theme_osd_color().
    // In the compact OSD build the grid colour selector is the authoritative
    // colour. Requiring the separate "override grid colours" switch made the
    // colour selector appear broken.
    #define CAM_GRID_ALWAYS_USER_COLOR      1

    // The record-screen control selectors, drawn by CHDK. This is what replaced
    // the handover in posd_screen_active() that used to give the bitmap back to
    // Canon for five seconds after every arrow press. See core/gui_recui.c and
    // docs/A480_UI_REVERSING.md.
    #define CAM_RECUI                       1

    // The three per-camera rows. See camera.h and core/gui_recui.c.
    #define CAM_RECUI_DRIVE_VALS    { 0, 1, 2 }
    #define CAM_RECUI_DRIVE_NAMES   { "SINGLE", "CONTINUOUS", "TIMER" }

    // Resolution 5 is *raw* on propset 2, not a size - see
    // shooting_get_canon_image_format() in core/shooting.c, which reads exactly
    // that. Offering it would arm Canon's own raw behind CHDK's back and fight
    // conf.save_raw, so the ladder stops at 4.
    #define CAM_RECUI_SIZE_VALS     { 0, 1, 2, 3, 4 }
    #define CAM_RECUI_SIZE_NAMES    { "L", "M1", "M2", "M3", "S" }

    // Read straight out of the firmware: a five-entry table of packed <w,h>
    // shorts at 0xffe6c058, one per resolution value 0-4. Entry 0 matches
    // CAM_JPEG_WIDTH/HEIGHT above, which is what ties it to still capture.
    // (A second, longer ladder at 0xffe6c06c is not the still set - do not use
    // it.) See docs/A480_UI_REVERSING.md.
    #define CAM_RECUI_SIZE_DIMS     { {3648,2736}, {2816,2112}, {2272,1704}, \
                                      {1600,1200}, { 640, 480} }

    #define CAM_RECUI_QUALITY_VALS  { 0, 1, 2 }
    #define CAM_RECUI_QUALITY_NAMES { "SUPERFINE", "FINE", "NORMAL" }

    // No CAM_RECUI_FUNC_MENU_FLAG, and none is needed. SET opens CHDK's own
    // FUNC menu now, so there is no Canon menu on the record screen to detect.
    // The attempt to find one, and why it failed, is written up at the foot of
    // core/gui_recui.c so it is not repeated.

    // NOT enabled here: CAM_FORWARD_MACRO_KEY_EVENT.
    //
    // It posted MacroController's logical event 0x852/0x890 on the down edge of
    // KEY_LEFT, because Canon's record UI was not acting on the physical
    // Macro/Infinity key while the custom GUI path was active.
    //
    // CAM_RECUI takes KEY_LEFT before Canon sees it and writes
    // REAL_FOCUS_MODE itself, so forwarding the event as well would mean two
    // things driving focus mode off one press - CHDK's write, and Canon's own
    // selector advancing from whatever it thought the value was. They would
    // disagree the moment a press was refused by the shoot-busy interlock.
    //
    // The forwarding code is still in platform/a480/kbd.c under the #ifdef.

    // Date/time prompt suppression, traced in the A480 firmware:
    //
    //   FUN_ffc2ad38 returns *(0x210c + 0x10), the RTC-valid flag.
    //   FUN_ffc184a0, FUN_ffc580ec and FUN_ffc5b588 all gate the
    //   DateTimeMenu call on *(0x1d10 + 0x38), then set that latch to 1.
    //
    // These addresses are specific to firmware 1.00B.
    #define CAM_CLOCK_VALID_FLAG            0x211c
    #define CAM_DATE_PROMPT_LATCH           0x1d48

    // Bend mode - the patchbay on the shooting screen. See BENDING_DESIGN.md
    // and core/gui_bend.c. Same key layout as the a470, so the same gesture:
    // hold the mode key (KEY_PRINT) for a second.
    //
    // The bend engine itself is core code and needs nothing per camera, but two
    // things about this body differ from the a470 and both are handled already:
    //
    //   12 bit sensor. raw.c has a 12bpp packed fast path as well as the 10bpp
    //   one - it matters more here, because 3720x2772 means it runs 10.3
    //   million times a shot. Both are checked against the real accessors in
    //   tools/bend_selftest.c.
    //
    //   CAM_ACTIVE_AREA_X1 is 6, not 12, so BUS_OB samples column 2 rather than
    //   column 8. Still inside the masked region, so the black level bus reads
    //   real dark current here too.
    //
    #define CAM_BEND_MODE                   1
    #define CAM_BEND_HOLD_MS                1000
    #define CAM_BEND_PRESET_ARROWS          1
    // Enter the patchbay with UP on the shooting screen instead of the second-
    // long hold on the ALT button. Only possible where CAM_RECUI already takes
    // the arrows before Canon sees them, and only worth doing because UP is the
    // one arrow that body binds to nothing - Canon's own selector for it opens
    // a box that never dismisses itself, so it was being blocked and thrown
    // away already (see core/gui_recui.c). The hold keeps the playback shot
    // prompt; MENU still leaves the mode.
    #define CAM_BEND_ENTER_UP               1
    // The second engine as well - see docs/EXPERIMENTAL_EFFECTS.md. This is
    // the body it was written against and the only one whose JPEG buffer has
    // been reversed, so it is the only one that gets the full effect list.
    #define CAM_BEND_EXPERIMENTAL           1
    #define CAM_SUPPRESS_LOW_BATTERY_ICON   1

    // NOT enabled here: CAM_HOLD_SCREEN_LOCK_IN_REC.
    //
    // It was tried, to fix overlay flashing on this body, and it did not fix it
    // - because Canon repainting over the overlay was not what the flashing
    // was. The overlay was tearing against its own repaint; see the note above
    // gui_draw_persistent_osd() in core/gui.c, which is where it was fixed.
    //
    // So this stays off, and the a480 keeps the stock screen refresh behaviour
    // it has always had, which was never observed to flash. The port-side half
    // of that experiment is still in lib.c: vid_turn_off_updates() there is now
    // idempotent against this body's lock *counter*, which is what made the
    // option unusable here before. If a future change does need the gate held
    // in record mode, this line is all that is missing.

//--------------------------------------------------

    #undef  CAM_DEFAULT_MENU_CURSOR_BG
    #undef  CAM_DEFAULT_MENU_CURSOR_FG
    #define CAM_DEFAULT_MENU_CURSOR_BG  IDX_COLOR_RED      // Override menu cursor colors
    #define CAM_DEFAULT_MENU_CURSOR_FG  IDX_COLOR_WHITE    // Override menu cursor colors

    #define CAM_QUALITY_OVERRIDE                1   // https://chdk.setepontos.com/index.php?topic=13342
