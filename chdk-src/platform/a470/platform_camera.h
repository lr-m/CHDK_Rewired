// Camera - a470 - platform_camera.h

// This file contains the various settings values specific to the a470 camera.
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

    #define CAM_RAW_ROWPIX                  3152
    #define CAM_RAW_ROWS                    2346 // correct firmware value matches raw size, was previously 2340

    #undef  CAM_USE_ZOOM_FOR_MF
    #undef  CAM_HAS_ZOOM_LEVER
    #undef  CAM_HAS_ERASE_BUTTON
    #undef  CAM_HAS_DISP_BUTTON                     // Camera does not have DISP button
    #undef  CAM_HAS_IRIS_DIAPHRAGM
    #define CAM_HAS_ND_FILTER               1
    #undef  CAM_HAS_MANUAL_FOCUS
    #undef  CAM_HAS_USER_TV_MODES
    #define CAM_HAS_HI_ISO_AUTO_MODE        1
    #define CAM_SHOW_OSD_IN_SHOOT_MENU      1
    #undef  CAM_HAS_IS
    #define CAM_CAN_MUTE_MICROPHONE         1
    #define CAM_AF_SCAN_DURING_VIDEO_RECORD 1
    #define CAM_EV_IN_VIDEO                 1
    #define CAM_MULTIPART                   1

    #define CAM_DNG_LENS_INFO               { 63,10, 216,10, 30,10, 58,10 } // See comments in camera.h
    // pattern
    #define cam_CFAPattern                  0x02010100 // Red  Green  Green  Blue
    // color
    #define CAM_COLORMATRIX1                              \
     673251, 1000000, -200684, 1000000,  -98680, 1000000, \
    -163638, 1000000,  651247, 1000000,   74004, 1000000, \
      14221, 1000000,   52979, 1000000,  265291, 1000000
    #define cam_CalibrationIlluminant1      1       // Daylight
    // cropping
    #define CAM_JPEG_WIDTH                  3096
    #define CAM_JPEG_HEIGHT                 2324
    #define CAM_ACTIVE_AREA_X1              12
    #define CAM_ACTIVE_AREA_Y1              8
    #define CAM_ACTIVE_AREA_X2              3108
    #define CAM_ACTIVE_AREA_Y2              2332
    // camera name
    #define PARAM_CAMERA_NAME               4       // parameter number for GetParameterData

    #define CAM_HAS_FILEWRITETASK_HOOK      1

    //#define DNG_EXT_FROM                  ".DPS"

    #define REMOTE_SYNC_STATUS_LED          0xc0220084  // specifies an LED that turns on while camera waits for USB remote to sync

    // Make the ALT button selectable from the menu. It is hard-wired to KEY_PRINT
    // by default, and a phantom press of that button opens ALT/the CHDK menu on
    // its own. If Print is electrically noisy on this 20 year old body, being able
    // to move ALT onto another key is both the test and the fix.
    #define CAM_ADJUSTABLE_ALT_BUTTON       1
    #define CAM_ALT_BUTTON_NAMES            { "Print", "Menu" }
    #define CAM_ALT_BUTTON_OPTIONS          { KEY_PRINT, KEY_MENU }

    #define CAM_OPTIONAL_EXTRA_BUTTON       1       // allow the Power button to be remapped in ALT mode
    #define CAM_EXTRA_BUTTON_NAMES          { "OFF", "Display" }
    #define CAM_EXTRA_BUTTON_OPTIONS        { 0, KEY_DISPLAY }

    #define CAM_SD_OVER_IN_AF               1
    #define CAM_SD_OVER_IN_AFL              1
    #define CAM_SD_OVER_IN_MF               1

    #define CAM_IS_VID_REC_WORKS            1   // is_video_recording() function works

    #undef  CAM_OSD_REDRAW_MASK
    #define CAM_OSD_REDRAW_MASK             1   // 40ms instead of 80ms - halves the
                                                // visible gap after a Canon repaint
    // This body installs CHDK's replacement file-write task (see boot.c) and is
    // DryOS, so fwt_write() sees Canon's writes and the bend recipe goes into
    // the picture as it is written rather than by rewriting it afterwards.
    // Keep the overlay serviced from inside the capture loops. Only safe here
    // because this port has a working review flag, so the repaints paint
    // nothing until Canon's review has finished. See camera.h.
    #define CAM_POSD_SERVICE_UI_IN_CAPTURE  1

    #define CAM_BEND_TAG_INJECT             1

    #define CAM_PERSISTENT_OSD              1   // splash-style overlay - see camera.h
    // OFF - calling SetDate this early in spytask crashes the camera right after
    // the boot screen. The signature is right (verified against its own range
    // checks) so the problem is when it is called, not how: it runs before
    // conf_restore/gui_init and quite possibly before Canon's clock subsystem is
    // up. If retried, move the call much later in startup and test it alone.
    //#define CAM_SET_DATE_ON_BOOT          1

    // Date/time screen suppression, traced in the 102c decompilation.
    //   clock struct   = *(0xffc2f5c0) = 0x2284, valid flag at +0xc  -> 0x2290
    //   uictrl struct  = *(0xffc19fc8) = 0x1d18, asked latch at +0x34 -> 0x1d4c
    // Gate is FUN_ffc19cd0, getter FUN_ffc2f70c, screen FUN_ffc588a0.
    // Both are firmware version specific - re-derive for any other build.
    // spytask writes to these addresses directly, so they must stay gated to the
    // firmware they were traced on. 102c is the only a470 revision with a dump;
    // on 100e/101a/101b the date screen is simply not suppressed.
    #ifdef CAMERA_a470_102c
    #define CAM_CLOCK_VALID_FLAG            0x2290
    #define CAM_DATE_PROMPT_LATCH           0x1d4c

    // Canon's My Camera sound slots, repointed at A/CHDK/SOUNDS/*.wav after
    // the card is up. Gated to 102c for the same reason as the two addresses
    // above: platform/a470/wrappers.c writes to a RAM table and calls a ROM
    // function, both traced in this firmware's decompilation and neither
    // re-derived for the revisions without a dump. See the long note there
    // for the three checks the addresses passed, and for the one thing about
    // this that a body still has to confirm - which slot is which.
    #define CAM_CUSTOM_SOUNDS               1

    // The boot screen is now a marker-tagged slot in the core image rather than
    // a fixed array (sub/102c/boot_image.h, tools/mkbootslot.py), so the
    // on-camera importer can rewrite it inside DISKBOOT.BIN. This is what puts
    // "Boot screen -> Import JPG/PNG" in CHDK Settings, as on the A480.
    //
    // Inside the 102c gate because only that revision has the dump, and so only
    // that revision has the startup-image hook the slot feeds.
    #define CAM_CUSTOM_BOOT_IMAGE           1

    // Preserve the ownership model of the archived, hardware-tested A470 OSD:
    // Canon's review replaces the bitmap itself. The pre-shot wipe, review flag
    // gate and per-overlay screen lock were later A480 fixes and cause the A470
    // to flash before review, then wait through JPEG/card processing.
    #define CAM_PERSISTENT_OSD_CANON_REVIEW_OWNS_ERASE 1
    #define CAM_PERSISTENT_OSD_TIMEOUT_OWNS_REVIEW 1

    // "A review is on screen", reversed out of this ROM because CHDK's
    // recreview_hold is not that on this firmware. 0x5b64 - the address finsig
    // labels recreview_hold - is written twice in the whole image:
    // ShtCon_StartReview clears it to 0, and ShootCon_NotifyStartReviewHold
    // (FUN_ffc6432c, the function finsig found it in) sets it to 1. It is the
    // review *hold* flag, 0 for an ordinary review and 1 only while the shutter
    // holds one up, and not cleared again until NotifyCompleteReviewHold at the
    // end of the sequence.
    //
    // 0x5b4c is the state flag: ShootCon struct (base 0x5adc, from the literals
    // at 0xffc62640 / 0xffc638a4 / 0xffc648f0) + 0x70. Set to 1 in
    // ShtCon_StartReview beside the _EntryActionReview log, cleared to 0 in
    // _ExitActionReview (FUN_ffc62828), and read as a state guard by five other
    // functions.
    //
    // Firmware-specific. This number is for 102c and nothing else.
    #define CAM_REVIEW_ACTIVE_FLAG          0x5b4c

    // ...and with the level test gone, the *edge* is what holds the plates off
    // the review. TIMEOUT_OWNS_REVIEW on its own left nothing but the 2500ms
    // hide timer between the shutter and the plates, which is why the overlay
    // started being drawn on top of the reviewed photograph. The two belong
    // together: the timeout is the floor, the edge is the release.
    #define CAM_PERSISTENT_OSD_TRACK_REVIEW_EDGE 1

    // The visible review ends before JPEG/card processing does (the orange LED
    // continues flashing). Normally spytask stops calling gui_redraw() during
    // that processing tail, which strands the cleared overlay until it ends.
    // Keep servicing the persistent UI; its own review/menu/shutter gates still
    // decide whether anything is allowed to be painted.
    #define CAM_PERSISTENT_OSD_REDRAW_WHILE_PROCESSING 1

    // NOT enabled: CAM_POSD_PUSH_SCREEN_WHILE_PROCESSING. It was added for a
    // theory that turned out to be wrong - that the overlay was being drawn and
    // not reaching the panel - and the real cause was bend_tag_service()
    // blocking spytask so that nothing was drawn at all. Left out rather than
    // left on: it calls RefreshPhysicalScreen at 5Hz through every save to fix
    // a fault that does not exist. The define and its implementation in
    // core/main.c remain, for a body that genuinely needs the push.

    // The on-screen gate readout, off now that the post-shot stall is understood
    // (bend_tag_service() blocking spytask inside Canon's JPEG write - see
    // STATUS.md). The code stays in core/gui.c and core/raw.c: its L and Z
    // columns are what finally distinguished "spytask is not painting" from
    // "spytask is not running", and the a430 and a410 faults are still open.
    // Define it again on whichever body is being chased.
    // #define CAM_POSD_GATE_DEBUG          1

    // Startup is replaced in the early StartupImage task, before Canon plays
    // entry 1. Do not enable the later generic CHDK beep as well.

#endif

    // Bend mode - the patchbay on the shooting screen. See BENDING_DESIGN.md
    // section 4 and core/gui_bend.c. Entered by holding the ALT button (Print)
    // for CAM_BEND_HOLD_MS; the same hold leaves it again.
    //
    // Turning this on replaces the long-press keypress emulation in
    // kbd_process(), which passed a held ALT button through to Canon's direct
    // print function. On this body that produces nothing visible - there is no
    // printer - so the trade is a button that does nothing for one that opens
    // the patchbay. Short presses still enter <ALT> exactly as before.
    #define CAM_BEND_MODE                   1
    #define CAM_BEND_HOLD_MS                1000

    // The second engine - docs/EXPERIMENTAL_EFFECTS.md. Nothing in it is
    // camera specific: raw.c hands it the buffers this port already supplies,
    // and the 10bpp packed fast path is the one this sensor uses.
    //
    // Unlike the A410, the JPEG bus is live here too - hook_jpeg_buffer() is
    // in sub/102c/lib.c with the argument for why its address is trusted. On
    // the three revisions without a dump that hook does not exist, the generic
    // weak one returns null, and the JPEG bus is a dead entry in the list
    // rather than an address nobody checked. Every other effect is live on all
    // four.
    #define CAM_BEND_EXPERIMENTAL           1

    // No CAM_BEND_ENTER_UP. Like the A410, this body has no separate zoom
    // control: UP and DOWN are the zoom on the shooting screen, so UP is
    // spoken for by the lens and the patchbay is entered by holding the ALT
    // button. Bend mode itself is unaffected - it is a GUI mode that owns the
    // keyboard, and inside it the arrows are the arrows.
    //
    // This keymap therefore has no KEY_ZOOM_IN / KEY_ZOOM_OUT at all - ten
    // keys, and the zoom is not among them (platform/a470/kbd.c). Two things
    // follow.
    //
    // No CAM_BEND_PRESET_ARROWS: the rocker cannot walk saved bends because
    // there is no rocker.
    //
    // And the browser's tuning knob - the segment size, the experimental
    // amount - has nowhere to live, so it moves onto LEFT/RIGHT for the one
    // row that owns a number. See CAM_BEND_NO_ROCKER in core/gui_bend.c.
    #define CAM_BEND_NO_ROCKER              1

    //-- the record UI ------------------------------------------------------
    //
    // Propset 2, which is what gui_recui.c's value maps are written against,
    // and this body matches the A480 on every assumption those maps make:
    // CAM_HAS_MANUAL_FOCUS undefined, ND filter, no iris diaphragm. Its kbd.c
    // has no macro-key event forwarding either, so nothing else drives focus
    // mode off the KEY_LEFT this takes.
    #define CAM_RECUI                       1

    // UP and DOWN are the zoom on this body, exactly as on the A410, so the
    // record UI takes neither: DOWN does not get the DRIVE selector it has on
    // the A480, and UP is not blocked the way the note at the foot of
    // recui_kbd() blocks it there. Blocking them would take the zoom off the
    // camera on the one screen it is for.
    //
    // Nothing is lost - DRIVE is on the FUNC menu, which carries every control
    // whether or not it also has an arrow. FLASH and FOCUS keep their arrows,
    // since LEFT and RIGHT are free here.
    #define CAM_RECUI_UPDOWN_IS_ZOOM        1

    // Inherited from the A480 rather than derived from this ROM. It is the
    // propset 2 drive enumeration and it is what Canon's own selector cycles
    // on a body with single, continuous and self-timer - but say plainly that
    // it was not read out of this firmware, because everything else in this
    // file was.
    #define CAM_RECUI_DRIVE_VALS    { 0, 1, 2 }
    #define CAM_RECUI_DRIVE_NAMES   { "SINGLE", "CONTINUOUS", "TIMER" }

    // SIZE and QUALITY, from two tables in the ROM image rather than from the
    // A480 or from a specification.
    //
    // This firmware has no SizeSlct.c or CompressionSlct.c in its assert
    // strings - the route the A410's came by - so both were found in the data
    // instead:
    //
    //   0xffe88d10  five <w,h> pairs, u16:
    //                 3072x2304  2592x1944  2048x1536  1600x1200  640x480
    //               A five entry still ladder, L down to S. The seven entry
    //               scaling set that sits nearby at 0xffe5e79c ends 768x576
    //               and is not it - that is the same decoy the A410 has.
    //
    //   0xffe5e6f8  a [5][3] grid of JPEG byte estimates, one row per
    //               resolution and three qualities falling left to right:
    //                 3118080 1942528  923648    <- 3072x2304
    //                 2563072 1428480  711680    <- 2592x1944
    //                 1640448  914432  455680    <- 2048x1536
    //                 1026048  571392  284672    <- 1600x1200
    //                  254976  153600   86016    <- 640x480
    //
    // The grid is what settles the values. Five contiguous rows, so the
    // resolution indices are 0..4 with no gap - unlike the A410, whose grid
    // has a zeroed row at 3 and therefore skips that value. Three columns
    // falling left to right orders quality as SUPERFINE/FINE/NORMAL.
    //
    // And the two bodies agree where they should: three of these rows are
    // byte-identical to the A410's, at exactly the pixel dimensions the two
    // cameras have in common (2048x1536, 1600x1200, 640x480). The same
    // estimator over the same image size gives the same numbers, which is
    // about as good a confirmation of the row order as this can have without
    // the function that reads it.
    #define CAM_RECUI_SIZE_VALS     { 0, 1, 2, 3, 4 }
    #define CAM_RECUI_SIZE_NAMES    { "L", "M1", "M2", "M3", "S" }

    // Straight off the <w,h> table above, which the A410 had no equivalent of
    // - so unlike that body this one can print real dimensions rather than
    // leaving the detail column empty.
    #define CAM_RECUI_SIZE_DIMS     { {3072,2304}, {2592,1944}, {2048,1536}, \
                                      {1600,1200}, {640,480} }

    #define CAM_RECUI_QUALITY_VALS  { 0, 1, 2 }
    #define CAM_RECUI_QUALITY_NAMES { "SUPERFINE", "FINE", "NORMAL" }

    // The live view display and DMA registers reversed during the live-bend
    // attempt are recorded in LIVEVIEW_NOTES.md rather than here - none of them
    // turned out to be usable, and leaving them as defines invited retrying them.

    #define CAM_OSD_ALWAYS_DRAW             1   // drop the canon_menu_active gate
    //#define CAM_OSD_FORCE_DRAW_IN_SPYTASK 1   // off: no longer needed, and it
                                                // carries the L/R/W/M counters

    // Hold Canon's screen refresh gate shut in record mode, not just in <ALT>.
    // <ALT> mode calls vid_turn_off_updates() on entry and is rock solid; plain
    // shooting mode does not and flickers. That is the only difference between
    // the two, which makes the lock the fix rather than anything about drawing.
    //
    // This was disabled earlier for the wrong reason: at the time the condition
    // still included gui_mode_none, which measures 0 on the shooting screen, so
    // the lock never once engaged and "it did not help" meant "it never ran".
    // Measured: gui_redraw() takes ~1s in shooting mode and ~10ms in <ALT>. The
    // cost is draw_restore() -> vid_bitmap_refresh() -> _RefreshPhysicalScreen(1),
    // which blocks for most of a second on this camera. <ALT> is fast only because
    // it holds the refresh gate, and RefreshPhysicalScreen (0xffd6789c) tests that
    // gate in its first few instructions and returns immediately when it is set.
    //
    // So this is not about stopping Canon painting over us - it stops CHDK making
    // a blocking firmware call every time it wants a restore. Canon's own OSD does
    // not need repainting anyway now that CAM_PERSISTENT_OSD replaces it.
    // The gate is released on half press and when a Canon menu opens, so the
    // Canon menus still repaint normally.
    #define CAM_HOLD_SCREEN_LOCK_IN_REC     1
    //#define CAM_SCREEN_LOCK_DEBUG         1   // on-screen counters

//--------------------------------------------------

    #undef  CAM_DEFAULT_MENU_CURSOR_BG
    #undef  CAM_DEFAULT_MENU_CURSOR_FG
    #define CAM_DEFAULT_MENU_CURSOR_BG  IDX_COLOR_RED      // Override menu cursor colors
    #define CAM_DEFAULT_MENU_CURSOR_FG  IDX_COLOR_WHITE    // Override menu cursor colors
