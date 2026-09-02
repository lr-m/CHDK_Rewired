// Camera - a460 - platform_camera.h

// This file contains the various settings values specific to the a460 camera.
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

    #define CAM_RAW_ROWPIX                  2664    // for 5 MP 1/3" sensor size
    #define CAM_RAW_ROWS                    1968    // for 5 MP 1/3" sensor size

    #undef  CAM_CIRCLE_OF_CONFUSION
    #define CAM_CIRCLE_OF_CONFUSION         4   // CoC value for camera/sensor (see http://www.dofmaster.com/digital_coc.html)

    #undef  CAM_USE_ZOOM_FOR_MF
    #undef  CAM_HAS_ZOOM_LEVER
    #define CAM_DRAW_EXPOSITION             1
    #undef  CAM_HAS_ERASE_BUTTON
    #undef  CAM_HAS_IRIS_DIAPHRAGM
    #define CAM_HAS_ND_FILTER               1
    #undef  CAM_HAS_MANUAL_FOCUS
    #undef  CAM_HAS_USER_TV_MODES
    #define CAM_SHOW_OSD_IN_SHOOT_MENU      1
    #undef  CAM_HAS_IS
    #define CAM_CAN_MUTE_MICROPHONE         1
    #define CAM_AF_SCAN_DURING_VIDEO_RECORD 1
    #define CAM_EV_IN_VIDEO                 1

    #define CAM_DNG_LENS_INFO               { 54,10, 216,10, 28,10, 58,10 } // See comments in camera.h
    // pattern
    #define cam_CFAPattern                  0x02010100 // Red  Green  Green  Blue
    // color
    #define CAM_COLORMATRIX1                               \
     649324,  1000000, -233893, 1000000,  -88521, 1000000, \
    -158955,  1000000,  593407, 1000000,   69775, 1000000, \
     -44551,  1000000,  136891, 1000000,  254362, 1000000

    #define cam_CalibrationIlluminant1      1       // Daylight
    // cropping
    #define CAM_JPEG_WIDTH                  2592
    #define CAM_JPEG_HEIGHT                 1944
    #define CAM_ACTIVE_AREA_X1              6
    #define CAM_ACTIVE_AREA_Y1              6
    #define CAM_ACTIVE_AREA_X2              2618
    #define CAM_ACTIVE_AREA_Y2              1962
    // camera name
    #define PARAM_CAMERA_NAME               4       // parameter number for GetParameterData
    #define DNG_EXT_FROM                    ".DPS"

    #define CAM_HAS_FILEWRITETASK_HOOK      1

    #define REMOTE_SYNC_STATUS_LED          0xc0220084  // specifies an LED that turns on while camera waits for USB remote to sync

    #define CAM_SD_OVER_IN_AF               1
    #define CAM_SD_OVER_IN_AFL              1
    #define CAM_SD_OVER_IN_MF               1

    #define CAM_IS_VID_REC_WORKS            1   // is_video_recording() function works

//--------------------------------------------------
// Project additions - see BENDING_DESIGN.md and OVERLAY_DESIGN.md.
//
// This body is VxWorks / DIGIC II where the a470 and a480 are DryOS / DIGIC III
// (r23 and r31). Everything below is OS-independent core code, which is why it
// ports as configuration rather than as work. What did NOT come across is
// anything hand-derived from those two ROMs - the screen refresh gate, the boot
// screen task hook, the a470's date prompt suppression. Those are DryOS
// addresses and structures and would have to be walked again from
// PRIMARY_a460_100d.BIN.

    // Make the ALT button selectable. On this body the Print button is awkward
    // to reach, and Display is right next to the thumb. Same mechanism the a470
    // uses; the a460 keymap has KEY_DISPLAY at physw bit 0x200 and it is inside
    // KEYS_MASK2, so it is already being read.
    #define CAM_ADJUSTABLE_ALT_BUTTON       1
    #define CAM_ALT_BUTTON_NAMES            { "Print", "Display" }
    #define CAM_ALT_BUTTON_OPTIONS          { KEY_PRINT, KEY_DISPLAY }

    // Bend mode - the patchbay on the shooting screen. Entered by holding the
    // ALT button, whichever one is selected above.
    //
    // The engine needs nothing per camera here. The sensor is 10 bit (the
    // CAM_SENSOR_BITS_PER_PIXEL default, same as the a470) so the existing
    // 10bpp packed fast path in raw.c applies unchanged, and CAM_ACTIVE_AREA_X1
    // is 6, so BUS_OB samples column 2 - inside the masked region, so the black
    // level bus reads real dark current here too, as on the a480.
    #define CAM_BEND_MODE                   1
    #define CAM_BEND_HOLD_MS                1000

    // The second engine - docs/EXPERIMENTAL_EFFECTS.md. Nothing in it is
    // camera specific: raw.c hands it the buffers this port already supplies,
    // and the 10bpp packed fast path is the one this sensor uses.
    //
    // The JPEG bus is a dead entry, as on the A410 and A430 and unlike the
    // A470 and A480: this is a VxWorks build whose imaging-buffer logging
    // reads the addresses out of globals rather than a literal pool, so there
    // is nothing for tools/newport.py to recover and no hook_jpeg_buffer()
    // here. The generic weak one returns null and that one effect does
    // nothing. Every other effect is live.
    #define CAM_BEND_EXPERIMENTAL           1

    // No separate zoom control - the up and down arrows are the zoom, as on
    // the A410, A430 and A470. So no CAM_BEND_ENTER_UP (UP belongs to the
    // lens) and no CAM_BEND_PRESET_ARROWS (there is no rocker). The keymap has
    // no KEY_ZOOM_IN / KEY_ZOOM_OUT at all, which is the signature all three
    // of those bodies share.
    #define CAM_BEND_NO_ROCKER              1

    //-- the record UI ------------------------------------------------------
    //
    // Propset 2 on a VxWorks body, which is the interesting part: propset is
    // about propcase numbering, not about the OS, so gui_recui.c's propset 2
    // block applies here exactly as it does on the A470 and A480. This camera
    // matches every assumption that block makes - no manual focus, ND filter,
    // no iris, no IS - and its iso_table[] has five entries, which the ISO row
    // reads at runtime rather than assuming.
    #define CAM_RECUI                       1
    #define CAM_RECUI_UPDOWN_IS_ZOOM        1

    // SIZE and QUALITY, from the JPEG size estimator at 0xffd32420 recovered
    // by tools/newport.py:
    //
    //   2563072 1428480 711680    <- 2592x1944, = CAM_JPEG_* above
    //   1640448  914432 455680    <- 2048x1536
    //   1026048  571392 284672    <- 1600x1200
    //         0       0      0    <- index 3, reserved
    //    254976  153600  86016    <- 640x480
    //
    // The zeroed row is an index Canon reserves and does not use, exactly as
    // on the A410 - which is why the values skip 3 rather than running 0..3.
    // Four of these rows are byte-identical to rows in the A410, A430, A470
    // and A480 grids at the pixel sizes those bodies share, which is what ties
    // the row order to the dimensions.
    #define CAM_RECUI_SIZE_VALS     { 0, 1, 2, 4 }
    #define CAM_RECUI_SIZE_NAMES    { "L", "M1", "M2", "S" }
    #define CAM_RECUI_SIZE_DIMS     { {2592,1944}, {2048,1536}, {1600,1200}, \
                                      { 640, 480} }
    #define CAM_RECUI_QUALITY_VALS  { 0, 1, 2 }
    #define CAM_RECUI_QUALITY_NAMES { "SUPERFINE", "FINE", "NORMAL" }

    // Propset 2's drive enumeration, as on the A470 and A480. Not read out of
    // this firmware - no detector finds it - but the write is legal whichever
    // way value 2 reads and only the label is at risk.
    #define CAM_RECUI_DRIVE_VALS    { 0, 1, 2 }
    #define CAM_RECUI_DRIVE_NAMES   { "SINGLE", "CONTINUOUS", "TIMER" }

    // Splash-style overlay in place of the stock OSD. Enabled for consistency
    // with the other two bodies rather than out of necessity.
    //
    // The hide logic should actually behave better here: canon_menu_active and
    // canon_shoot_menu_active are both real RAM addresses on this port (0x2EE4
    // and 0xD504), where the a480 has the latter pinned to a ROM zero.
    #define CAM_PERSISTENT_OSD              1

    // Canon's visible review ends well before its JPEG/card processing does on
    // this body, and the spytask gate in core/main.c skips every gui_redraw()
    // until that processing finishes. The overlay therefore stayed off screen
    // for the whole save after each shot - seconds, and longer the heavier the
    // bend, because the wait was the encode rather than a timer. Keep the UI
    // serviced through the tail; the overlay's own ownership gates still decide
    // what may be painted. Same fix, same reason, as the A470.
    #define CAM_PERSISTENT_OSD_REDRAW_WHILE_PROCESSING 1

    // Keeping the redraw call alive was only half of it. posd_screen_active()
    // still gated the plates on recreview_hold's *level*, and that flag stays
    // asserted through exactly the JPEG/card tail the redraw was restored for -
    // so the overlay came back at the end of the save either way and the delay
    // still grew with the bend. Hand the review to the shot-hold instead: the
    // hide timer is the floor and the falling edge of the flag is the release,
    // which is the moment the picture leaves the screen rather than the moment
    // the card work behind it finishes.
    #define CAM_PERSISTENT_OSD_TIMEOUT_OWNS_REVIEW 1
    #define CAM_PERSISTENT_OSD_TRACK_REVIEW_EDGE   1

    // "A review is on screen", reversed out of this ROM for the same reason as
    // the A470's: recreview_hold is not that flag on this firmware either, so
    // without this the only thing between the shutter and the plates is the
    // hide timer - and the Bend UI, which asks posd_review_active() directly
    // (core/gui_bend.c), gets an answer that has nothing to do with the review.
    // The overlay was drawn over the reviewed photograph.
    //
    // 0x2444 is the state field. Written in exactly two places in the whole
    // image: set to 1 beside the _EntryActionReview log (0xffc18390, which
    // returns early if it is already 1) and cleared to 0 in _ExitActionReview
    // (0xffc183ec, which returns early if it is already 0). The only other
    // reference, at 0xffc1adec, reads it as a state guard. Same shape as the
    // A470's 0x5b4c, found the same way.
    //
    // Firmware-specific. This number is for 100d and nothing else.
    #define CAM_REVIEW_ACTIVE_FLAG          0x2444

    // The on-screen gate readout, off now that the post-shot stall is understood
    // (bend_tag_service() blocking spytask inside Canon's JPEG write - see
    // STATUS.md). The code stays in core/gui.c and core/raw.c: its L and Z
    // columns are what finally distinguished "spytask is not painting" from
    // "spytask is not running", and the a430 and a410 faults are still open.
    // Define it again on whichever body is being chased.
    // #define CAM_POSD_GATE_DEBUG          1
    #undef  CAM_OSD_REDRAW_MASK
    #define CAM_OSD_REDRAW_MASK             1   // 40ms, matching the other two

    // Date/time screen suppression, traced in PRIMARY_a460_100d.BIN. Same
    // mechanism as the a470, different addresses; the suppression itself is
    // generic code in core/main.c and needs only these two words.
    //
    // Gate is FUN_ffc0f8d4, structurally identical to the a470's FUN_ffc19cd0:
    //
    //     ldr  r5, =0x00002010     ; the "already asked" latch
    //     ldr  r3, [r5]
    //     cmp  r3, #0
    //     popne {r4,r5,pc}         ; latch set -> never ask again
    //     bl   0xffdc005c          ; clock_is_valid()
    //     subs r4, r0, #0
    //     bne  0xffc0f90c          ; clock good -> skip
    //     bl   0xffc682e8          ; show the date/time screen
    //     mov  r3, #1
    //     str  r3, [r5]            ; latch it
    //
    // clock_is_valid() at 0xffdc005c is three instructions and just returns
    // *(0x00008f34), so that word is the valid flag. Note the polarity - it is
    // a VALID flag, set to 1, not an "invalid" flag to clear.
    //
    // The screen itself is the DateTimeMenu module: 0xffc682e8 creates it and
    // 0xffc67aec is its worker, both identified from the "DateTimeMenu.c"
    // assert string at 0xffc67adc.
    //
    // Firmware version specific - re-derive for any other A460 build.
    #define CAM_CLOCK_VALID_FLAG            0x8f34
    #define CAM_DATE_PROMPT_LATCH           0x2010

    // Custom startup image. Implementation and the traced addresses are in
    // sub/100d/boot.c; the hook fires from task_start_hook in
    // platform/generic/main.c, which is the earliest point CHDK runs on a
    // VxWorks body.
    // Canon's My Camera sound slots, repointed at A/CHDK/SOUNDS/*.wav once the
    // card is up. The table is the same one the boot screen below already
    // uses, and that boot screen works on the body - so unlike the A470's,
    // this address is confirmed by hardware rather than by argument. See
    // platform/a460/wrappers.c for the disassembly of the init and for the
    // one thing still unconfirmed: which sound each slot ID plays.
    #define CAM_CUSTOM_SOUNDS               1

    // ...and keep CHDK's own start sound with it. Turning the slots on used to
    // compile that out - see gui_init() - on the assumption that a body with
    // usable My Camera slots plays its own startup sound from them. This one
    // does not, and enabling the slots took its boot sound away.
    // NOT enabled any more: CAM_CHDK_START_SOUND.
    //
    // It gave this body CHDK's own beep, because its ROM does not play a
    // startup sound from the My Camera slots. platform/a460/wrappers.c now
    // plays A/CHDK/SOUNDS/startup.wav through the shutter slot at gui_init,
    // the same route the A470 uses, so the beep would double up with it.
    // #define CAM_CHDK_START_SOUND         1

    #define CAM_STARTUP_IMAGE               1

    // The boot screen is now a marker-tagged slot in the core image rather than
    // a fixed array (sub/*/boot_image.h, tools/mkbootslot.py), so the on-camera
    // importer can rewrite it inside DISKBOOT.BIN. This is what puts
    // "Boot screen -> Import JPG/PNG" in CHDK Settings, as on the A480.
    #define CAM_CUSTOM_BOOT_IMAGE           1

    // NOT enabled: CAM_HOLD_SCREEN_LOCK_IN_REC. Same reasoning as the a480 -
    // the overlay repaints only on change now, so Canon's repaint rate is not
    // what drives flashing, and holding this camera's gate has never been
    // tested. RefreshPhysicalScreen is stubbed at 0xffcb9180 if it is ever
    // wanted, but ScreenLock has not been located on this firmware.

//--------------------------------------------------

    #undef  CAM_DEFAULT_MENU_CURSOR_BG
    #undef  CAM_DEFAULT_MENU_CURSOR_FG
    #define CAM_DEFAULT_MENU_CURSOR_BG  IDX_COLOR_RED      // Override menu cursor colors
    #define CAM_DEFAULT_MENU_CURSOR_FG  IDX_COLOR_WHITE    // Override menu cursor colors
