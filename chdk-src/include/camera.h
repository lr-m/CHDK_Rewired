// camera.h

#ifdef CHDK_MODULE_CODE
#error camera.h cannot be #included in module code (did you mean camera_info.h).
#endif

// This file contains the default values for various settings that may change across camera models.
// Setting values specific to each camera model can be found in the platform/XXX/platform_camera.h file for camera.

// If adding a new settings value put a suitable default value in here, along with documentation on
// what the setting does and how to determine the correct value.
// If the setting should not have a default value then add it here using the '#undef' directive
// along with appropriate documentation.

#ifndef CAMERA_H
#define CAMERA_H

//==========================================================
// Camera-dependent settings
//==========================================================

//----------------------------------------------------------
// Default values
//----------------------------------------------------------

    #undef  CAM_DRYOS                           // Camera is DryOS-based
    #undef  CAM_PROPSET                         // Camera's properties group (the generation)
    #undef  CAM_ILC                             // Camera uses interchangable lenses (EOS M series)
    #define CAM_FLASHPARAMS_VERSION         3   // flash parameters structure version (every camera from 2005 on is version 3)
    #undef  CAM_DRYOS_2_3_R39                   // Define for cameras with DryOS release R39 or greater
    #undef  CAM_DRYOS_2_3_R47                   // Define for cameras with DryOS release R47 or greater -> Cameras can boot from FAT32
    #undef  CAM_DRYOS_2_3_R59                   // Define for cameras with DryOS release R59 or greater -> some support for 64bit files

    #define CAM_CIRCLE_OF_CONFUSION         5   // CoC value for camera/sensor (see http://www.dofmaster.com/digital_coc.html)

    #undef  CAM_HAS_CMOS                        // Camera has CMOS sensor
    #undef  CAM_SWIVEL_SCREEN                   // Camera has rotated LCD screen
    #define CAM_USE_ZOOM_FOR_MF             1   // Zoom lever can be used for manual focus adjustments
    #define CAM_DEFAULT_ALT_BUTTON  KEY_PRINT   // alt button for cameras without adjustable alt
                                                // redefine if not print to avoid need for KEY_PRINT alias in keymap
    #undef  CAM_ADJUSTABLE_ALT_BUTTON           // ALT-button can be set from menu, must set next two values as well
    #undef  CAM_ALT_BUTTON_NAMES                // Define the list of names for the ALT button   - e.g. { "Print", "Display" }
    #undef  CAM_ALT_BUTTON_OPTIONS              // Define the list of options for the ALT button - e.g. { KEY_PRINT, KEY_DISPLAY }
    #define CAM_REMOTE                      1   // Camera supports a remote
    #undef  CAM_REMOTE_AtoD_CHANNEL             // Camera supports using 3rd battery terminal as well as USB for remote - value = A/D channel to poll (normally 5)
    #define CAM_REMOTE_AtoD_THRESHOLD       200 // 3rd battery terminal A/D reading threshold ( lower = 1, higher = 0 )
    #undef  CAM_REMOTE_HDMI_HPD                 // Camera supports using HDMI hotplug detect for remote signal
    #undef  CAM_REMOTE_HDMI_POWER_OVERRIDE      // Camera requires HDMI power to be forced on in rec mode
    #undef  CAM_REMOTE_ANALOG_AV                // Camera supports analog AV detect for remote signal
    #undef  CAM_REMOTE_USES_PRECISION_SYNC      // Disable experimental remote  precision sync patch
    #define CAM_ALLOWS_USB_PORT_FORCING     1   // USB remote state can be forced to be present (supported on all cameras that use common kbd code, others may undef)
    #define CAM_REMOTE_USB_HIGHSPEED        1   // Enable highspeed measurements of pulse width & counts on USB port 
    #define CAM_REMOTE_HIGHSPEED_LIMIT 1000     // Set lowest timer value settable by user
    #undef  GPIO_VSYNC_CURRENT                  // USB remote precision sync : might be 0xC0F070C8 or 0xC0F07008 (http://chdk.setepontos.com/index.php?topic=8312.msg104027#msg104027)
    #undef  CAM_MULTIPART                       // Camera supports SD-card multipartitioning
    #define CAM_HAS_ZOOM_LEVER              1   // Camera has dedicated zoom buttons
    #undef  CAM_DRAW_EXPOSITION                 // Output expo-pair on screen (for cameras which (sometimes) don't do that)
    #define CAM_HAS_ERASE_BUTTON            1   // Camera has dedicated erase button
    #define CAM_HAS_DISP_BUTTON             1   // Camera has dedicated DISP button
    #define CAM_HAS_IRIS_DIAPHRAGM          1   // Camera has real diaphragm mechanism (http://en.wikipedia.org/wiki/Diaphragm_%28optics%29)
    #undef  CAM_HAS_ND_FILTER                   // Camera has build-in ND filter
    #undef  CAM_HAS_NATIVE_ND_FILTER            // Camera has built-in ND filter with Canon menu support for enable/disable
    #undef  CAM_ND_SET_AV_VALUE                 // Camera needs AV propcase set to override ND. Only valid for cameras without iris.
                                                // Defaults on for cameras with ND only, define 0 to force off

    #define CAM_HAS_MANUAL_FOCUS            1   // Camera has native manual focus mode (disables MF shortcut feature)
    #undef  CAM_SD_OVER_IN_AF                   // Camera allows SD override if MF & AFL not set
    #undef  CAM_SD_OVER_IN_AFL                  // Camera allows SD override when AFL is set
    #undef  CAM_SD_OVER_IN_MF                   // Camera allows SD override when MF is set

    #define CAM_HAS_USER_TV_MODES           1   // Camera has tv-priority or manual modes with ability to set tv value
    #undef  CAM_SHOW_OSD_IN_SHOOT_MENU          // On some cameras Canon shoot menu has additional functionality and useful in this case to see CHDK OSD in this mode
    #define CAM_CAN_UNLOCK_OPTICAL_ZOOM_IN_VIDEO 1 // Camera can unlock optical zoom in video (if it is locked in Canon firmare, do not define for cameras that already allow)
    #undef  CAM_FEATURE_FEATHER                 // Cameras with "feather" or touch wheel.
    #define CAM_HAS_IS                      1   // Camera has image stabilizer
    #undef  CAM_HAS_JOGDIAL                     // Camera has a "jog dial"

    #undef  CAM_CONSOLE_LOG_ENABLED             // Development: internal camera stdout -> A/stdout.txt
    #define CAM_CHDK_HAS_EXT_VIDEO_MENU     1   // In CHDK for this camera realized adjustable video compression
    #undef  CAM_CAN_MUTE_MICROPHONE             // Camera has function to mute microphone

    // Camera port can replace Canon's four My Camera sound slots from
    // A/CHDK/SOUNDS/*.wav after the card filesystem is mounted.
    #undef  CAM_CUSTOM_SOUNDS
    #undef  CAM_SUPPRESS_LOW_BATTERY_ICON
    #undef  CAM_CUSTOM_BOOT_IMAGE

    // In bend mode, left/right load saved bends and the zoom lever selects the
    // pin being edited. Intended for bodies with a zoom lever.
    #undef  CAM_BEND_PRESET_ARROWS

    // The second bend engine - experimental profiles, the whole of bendx.c and
    // the EXPERIMENTAL window of the browser. See
    // docs/EXPERIMENTAL_EFFECTS.md.
    //
    // Separate from CAM_BEND_MODE because it is a much larger thing to carry
    // than the routing matrix: twenty-four effects, a chain in the config
    // block, its own window in the browser and its own submenu. A port that
    // wants the patchbay does not necessarily want all of that, and on a body
    // where every effect would have to be re-timed anyway it is better left
    // off than left on and slow.
    //
    // Requires CAM_BEND_MODE for the browser window to be reachable; the menu
    // half works without it.
    #undef  CAM_BEND_EXPERIMENTAL

    #define CAM_EMUL_KEYPRESS_DELAY         40  // Delay to interpret <alt>-button press as longpress
    #define CAM_EMUL_KEYPRESS_DURATION      5   // Length of keypress emulation

    #define CAM_MENU_BORDERWIDTH            10  // Defines the width of the border on each side of the CHDK menu. The CHDK menu will have this
                                                // many pixels left blank to the on each side. Should not be less than 10 to allow room for the
                                                // scroll bar on the right.

    #undef  CAM_TOUCHSCREEN_UI                  // Define to enable touch screen U/I (e.g. IXUS 310 HS)
    #define CAM_TS_BUTTON_BORDER            0   // Define this to leave a border on each side of the OSD display for touch screen buttons.
                                                // Used on the IXUS 310 to stop the OSD from overlapping the on screen buttons on each side
    #define CAM_TS_MENU_BORDER              0   // Define this to leave a border at top and bottom of menu to avoid areas where touch sensor does not work
                                                // Used on the IXUS 310 to allow touch selection of menu entries
    #define CAM_DISP_ALT_TEXT               1   // Display the '<ALT>' message at the bottom of the screen in ALT mode (IXUS 310 changes button color instead)

    #undef  CAM_AF_SCAN_DURING_VIDEO_RECORD     // CHDK can make single AF scan during video record
    #undef  CAM_RESET_AEL_AFTER_VIDEO_AF        // Cam needs AE Lock state reset after AF in video recording
    #undef  CAM_HAS_VIDEO_BUTTON                // Camera can take stills in video mode, and vice versa
    #define CAM_MASK_VID_REC_ACTIVE    0x0400   // mask so that Canon mode does not change when cam's with video button enter video recording mode
    #undef  CAM_EV_IN_VIDEO                     // CHDK can change exposure in video mode
    #define CAM_VIDEO_CONTROL               1   // pause / unpause video recordings
    #undef  CAM_VIDEO_QUALITY_ONLY              // Override Video Bitrate is not supported
    #undef  CAM_CHDK_HAS_EXT_VIDEO_TIME         // Camera can override time limit of video record -> sx220/230
    #undef  CAM_SIMPLE_MOVIE_STATUS             // Firmware movie_status nonzero when recording, zero when not - makes CAM_VIDEO_CONTROL ineffective
    #undef  CAM_MOVIEREC_NEWSTYLE               // Define for new style video recording code in fw (DIGIC 6)
    #undef  CAM_HAS_MOVIE_DIGEST_MODE           // The values in the 'movie_status' variable change if the camera has this mode (see is_video_recording())
    #undef  CAM_HAS_SPORTS_MODE                 // Define to enable the RAW exception override control for SPORTS mode (s3is, sx30, sx40, etc)

    #undef  CAM_REAR_CURTAIN                    // Camera do not have front/rear curtain flash sync in menu
    #undef  CAM_BRACKETING                      // Cameras that have bracketing (focus & ev) in original firmware already, most likely s- & g-series (propcase for digic III not found yet!)
    #define CAM_EXT_TV_RANGE                1   // CHDK can make exposure time longer than 64s
    #define CAM_EXT_AV_RANGE                6   // Number of 1/3 stop increments to extend the Av range beyond the Canon default smallest aperture
                                                //  override in platform_camera.h for cameras with different range (e.g. G1X can't go below F/16 so set this to 0)
    #define CAM_CHDK_PTP                    1   // include CHDK PTP support

    #undef  CAM_HAS_FILEWRITETASK_HOOK          // FileWriteTask hook is available (local file write can be prevented)
    #undef  CAM_FILEWRITETASK_SEEKS             // Camera's FileWriteTask can do Lseek() - DryOS r50 or higher, implicitly enables CAM_FILEWRITETASK_MULTIPASS when defined
    #undef  CAM_FILEWRITETASK_MULTIPASS         // Camera may use multiple FileWriteTask invocations to write larger JPEGs (approx. DryOS r39..49)

    #define CAM_UNCACHED_BIT                0x10000000 // bit indicating the uncached memory

    #define CAM_SENSOR_BITS_PER_PIXEL       10  // Bits per pixel. 10 is standard, 12 is supported except for curves
    #define CAM_WHITE_LEVEL                 ((1<<CAM_SENSOR_BITS_PER_PIXEL)-1)      // 10bpp = 1023 ((1<<10)-1), 12bpp = 4095 ((1<<12)-1)
    #define CAM_BLACK_LEVEL                 ((1<<(CAM_SENSOR_BITS_PER_PIXEL-5))-1)  // 10bpp = 31 ((1<<5)-1),    12bpp = 127 ((1<<7)-1)

    #define CAM_DEFAULT_MENU_CURSOR_BG      IDX_COLOR_BLUE      // Default menu cursor colors
    #define CAM_DEFAULT_MENU_CURSOR_FG      IDX_COLOR_YELLOW    // Default menu cursor colors

    // Older cameras had a screen/bitmap buffer that was 360 pixels wide (or 480 for wide screen models)
    // CHDK was built around this 360 pixel wide display model
    // Newer cameras have a 720 pixel wide bitmap (960 for wide screen cameras)
    // To accomadate this the CHDK co-ordinate system assumes a 360/480 wide buffer and the
    // pixel drawing routines draw every pixel twice to scale the image up to the actual buffer size
    // Define CAM_USES_ASPECT_CORRECTION with a value of 1 to enable this scaled display
    #define CAM_USES_ASPECT_CORRECTION      0
    #define CAM_SCREEN_WIDTH                360 // Width of bitmap screen in CHDK co-ordinates (360 or 480)
    #define CAM_SCREEN_HEIGHT               240 // Height of bitmap screen in CHDK co-ordinates (always 240 on all cameras so far)
    #define CAM_BITMAP_WIDTH                360 // Actual width of bitmap screen in bytes (may be larger than displayed area)
    #define CAM_BITMAP_HEIGHT               240 // Actual height of bitmap screen in rows (240 or 270)

    // How long the persistent overlay stays down once a shot starts. It has to
    // outlast Canon's post-shot review, because the erase that hides it has to
    // happen *before* the review does - see posd_hide_now() in core/gui.c.
    #define CAM_PERSISTENT_OSD_SHOT_HIDE_MS 2500

    // Keep calling gui_redraw() while Canon is still processing a shot. Canon's
    // visible review ends before its JPEG/card work does, and the spytask gate
    // in core/main.c otherwise skips every redraw until that work finishes -
    // stranding the persistent overlay for as long as the save takes. Define on
    // any body with CAM_PERSISTENT_OSD whose overlay comes back late after a
    // shot, in proportion to how long the picture took to write.
    // Diagnostic: draw the overlay/grid gate values from spytask. See
    // posd_gate_debug_draw() in core/gui.c.
    #undef  CAM_POSD_GATE_DEBUG

    #undef  CAM_PERSISTENT_OSD_REDRAW_WHILE_PROCESSING

    // Call vid_bitmap_refresh() ourselves while a shot is being processed, for a
    // body where Canon's repaint is what carries the bitmap to the panel and
    // CAM_HOLD_SCREEN_LOCK_IN_REC is holding that repaint off. Without it
    // everything CHDK draws during the save - overlay, grid, warnings - sits in
    // a buffer nothing scans out until Canon refreshes of its own accord.
    // Consumed in core/main.c.
    #undef  CAM_POSD_PUSH_SCREEN_WHILE_PROCESSING

    // Write the bend recipe into the JPEG as Canon writes it, from fwt_write(),
    // instead of rewriting the finished file to insert it at the front. Needs
    // BOTH: DryOS (fwt_write is inside #ifdef CAM_DRYOS in
    // platform/generic/filewrite.c) and boot.c actually installing CHDK's
    // replacement file-write task. Check both before defining it on a new port -
    // if fwt_write never runs, nothing is written and nothing says so.
    // See core/bend_tag.c.
    #undef  CAM_BEND_TAG_INJECT

    // Keep the persistent overlay down for the whole of the shot, not just for
    // CAM_PERSISTENT_OSD_SHOT_HIDE_MS. Consumed by posd_shot_hold().
    //
    // For a body that has no working "a review is on screen" flag. The plates
    // are erased before the shot and normally stay erased because nothing
    // repaints them - but raw_service_ui() in core/raw.c now calls gui_redraw()
    // from inside the bend loops every 100ms, so once the hide timer expires
    // they are painted back mid-capture and Canon then freezes the bitmap for
    // the review with them in it.
    //
    // On a body with a real review flag (CAM_REVIEW_ACTIVE_FLAG) this is not
    // needed and should not be set: there the hold tracks the review itself,
    // and holding through processing as well would put the overlay back on the
    // wrong side of the fix it took all of 2026-08-31 to find - the whole point
    // there is that the overlay stays live while the picture is being written.
    #undef  CAM_PERSISTENT_OSD_HOLD_THROUGH_PROCESSING

    // Let raw_service_ui() in core/raw.c repaint the screen from inside the
    // capture loops. OFF by default, and that default is the important part.
    //
    // It exists for one body: the A470, where the overlay was stranded for the
    // whole of a long bend because spytask is inside raw_process() and never
    // reaches its own gui_redraw(). It only works there because that port has a
    // real review flag, so the repaints paint nothing until Canon's review is
    // over.
    //
    // Anywhere else it is actively harmful: the plates are erased before the
    // shot and stay erased precisely because nothing repaints them, so turning
    // repaints on puts them back mid-capture and Canon then freezes the bitmap
    // for the review with the overlay on it. That is exactly what it did to the
    // A480, and the working build in CHDK_Rewired/ differs from this tree by
    // not having the function at all.
    #undef  CAM_POSD_SERVICE_UI_IN_CAPTURE

    // Bring the overlay back on the falling edge of Canon's review flag rather
    // than on the CAM_PERSISTENT_OSD_SHOT_HIDE_MS timer. Consumed by
    // posd_shot_hold() in core/gui.c.
    //
    // Neither of the two signals available is the one wanted on its own.
    // recreview_hold stays asserted into the JPEG/card tail, so gating on its
    // *level* holds the overlay down for the whole save - which is the delay
    // that grows with how heavy the bend was. The hide timeout does not know
    // how long the review is and so either cuts into it or overshoots it.
    // The *falling edge* of the flag is the event that actually matters: the
    // picture has left the screen. So the timeout owns the minimum hold and
    // the edge owns the release, and the two are used for what each is.
    //
    // Pairs with CAM_PERSISTENT_OSD_TIMEOUT_OWNS_REVIEW: with the edge tracked
    // here the plates are already held for the whole review, so keeping the
    // level test in posd_review_blocks() as well only adds the tail back.
    #undef  CAM_PERSISTENT_OSD_TRACK_REVIEW_EDGE

    // Backstop for the wait above, in ms from the shutter. Only reached if
    // recreview_hold never falls - a wrong stub address, or a review the user
    // is holding open. Releasing late is a cosmetic fault; never releasing is
    // an overlay that is gone until the next shot.
    #define CAM_PERSISTENT_OSD_REVIEW_MAX_MS 30000

    // Canon's review replaces the bitmap itself on this body, so CHDK must not
    // also erase before the review. Consumed in core/gui.c.
    #undef  CAM_PERSISTENT_OSD_CANON_REVIEW_OWNS_ERASE

    // How often spytask calls gui_redraw(), as a mask over its loop counter: the
    // loop ticks every 20ms, so 3 redraws every 4th pass (80ms), 1 every 2nd pass
    // (40ms), 0 every pass (20ms). Must be 2^n-1.
    // Canon repaints its OSD into the same bitmap buffer CHDK draws in, so outside
    // <ALT> mode (where vid_turn_off_updates holds Canon off) this interval is the
    // window in which the overlay is missing after a Canon repaint - i.e. how long
    // each flash lasts. Lowering it costs CPU in spytask; raise it back if a camera
    // starts dropping frames or responding sluggishly.
    #define CAM_OSD_REDRAW_MASK             3

    // EXPERIMENTAL. Redrawing faster only helps if CHDK can win the race against
    // Canon's repaint. When Canon repaints at a similar rate the two beat against
    // each other and the overlay is visible only in the shrinking gap between
    // them - no redraw interval fixes that, because it is a race that cannot be
    // won, only avoided. Define this to hold the screen refresh gate shut in
    // record mode as well as in <ALT> mode, so Canon does not repaint at all
    // while the CHDK OSD is the only thing on screen.
    // Cost: Canon's own OSD stops updating while the lock is held. The lock is
    // released on half-press, Canon menu, and leaving record mode, so anything
    // Canon needs to draw should still get through - but this is the part that
    // needs testing on real hardware.
    //
    // ONLY enable this on a port whose refresh gate is a boolean flag, because the
    // lock is re-asserted on every spytask pass (Canon clears it from its own live
    // view path). On a port where the gate is a lock counter - the a480 - that
    // would increment without bound. Check which kind you have before defining it.
    #undef  CAM_HOLD_SCREEN_LOCK_IN_REC

    // Draw the CHDK OSD in shooting mode unconditionally, the way <ALT> mode
    // already does, instead of gating it on canon_menu_active. Define this on
    // cameras where the OSD flickers or barely appears because that Canon RAM
    // variable is unreliable. Cost: the OSD is no longer hidden while a Canon
    // menu is open. See the comment at the use site in gui_osd.c.
    #undef  CAM_OSD_ALWAYS_DRAW

    // Draw the OSD directly from spytask instead of relying on gui_redraw()
    // reaching it through gui_mode->redraw. Use on cameras where the OSD is drawn
    // far less often than once per redraw pass. Also enables a small spinner in
    // the top right corner that shows the actual spytask redraw rate.
    #undef  CAM_OSD_FORCE_DRAW_IN_SPYTASK

    // Replace the stock multi-element OSD with a single compact box drawn the
    // same way (and from the same place) as the startup version splash. For
    // cameras where an OSD element function blocks the spytask loop badly enough
    // that the overlay cannot be repainted often enough to stay on screen.
    #undef  CAM_PERSISTENT_OSD

    // Draw the flash / focus / drive selectors in CHDK instead of letting Canon
    // draw them. Only meaningful with CAM_PERSISTENT_OSD, which is what takes
    // the bitmap away from Canon in the first place. See core/gui_recui.c.
    #undef  CAM_RECUI

    // The record UI's per-camera rows. Flash, focus, WB, MY COLOR and EV are
    // propset-wide and live in core/gui_recui.c; these three are not, because
    // their values differ body to body even inside one propset.
    //
    // DRIVE is required by CAM_RECUI. SIZE and QUALITY are optional - a camera
    // that omits them gets a menu with fewer rows, which is the right answer
    // when the values have not been reversed yet. CAM_RECUI_SIZE_DIMS is
    // optional on top of SIZE and only worth defining where a real <w,h> table
    // was found in that body's firmware; it feeds the detail column.
    #undef  CAM_RECUI_DRIVE_VALS                // e.g. { 0, 1, 2 }
    #undef  CAM_RECUI_DRIVE_NAMES               // e.g. { "SINGLE", "CONTINUOUS", "TIMER" }
    #undef  CAM_RECUI_SIZE_VALS
    #undef  CAM_RECUI_SIZE_NAMES
    #undef  CAM_RECUI_SIZE_DIMS                 // e.g. { {3648,2736}, ... }
    #undef  CAM_RECUI_QUALITY_VALS
    #undef  CAM_RECUI_QUALITY_NAMES

    // Address of Canon's "clock is valid" word, held at 1 by spytask so the
    // date/time screen does not appear on every boot with a dead backup battery.
    //
    // To find it: locate the clock init routine (it calls GetTimeFromRTC and
    // GetValidSystemCalender, then stores 1 or 0 to <struct>+0xc). The trivial
    // getter that returns that same word is what the UI gates the date screen on.
    // Resolve <struct> from the literal pool entry the routine loads.
    // Firmware version specific. Note this is a VALID flag - it is set, not cleared.
    #undef  CAM_CLOCK_VALID_FLAG

    // Optional companion: Canon's "already asked for the date" latch, the first
    // term of the same gate. Setting it suppresses the screen even if the clock
    // still reports invalid. Address is <uictrl struct>+0x34.
    #undef  CAM_DATE_PROMPT_LATCH

    // Call SetDate at startup so the clock is actually set, rather than trying to
    // suppress Canon's complaint about it. Needs a SetDate stub for the camera.
    #undef  CAM_SET_DATE_ON_BOOT

    #define EDGE_HMARGIN                    0   // define sup and inf screen margins on edge overlay without overlay.  Necessary to save memory buffer space. sx200is needs values other than 0

    #undef CAM_QUALITY_OVERRIDE                 // define this in platform_camera.h to enable 'Super Fine' JPEG compression mode
                                                // used to allow super fine JPEG option on cameras where this has been removed
                                                // from the Canon menu. Note: may not actually work on all cameras.

    #undef  CAM_ZEBRA_NOBUF                     // zebra draws directly on bitmap buffer.
    #undef  CAM_HAS_VARIABLE_ASPECT             // can switch between 16:9 and 4:3 (used by zebra code)
    
    #undef  CAM_DATE_FOLDER_NAMING              // set if camera uses date based folder naming (Option "Create Folder" in Canon Menu) and get_target_dir_name is implemented
    
    #define CAM_KEY_PRESS_DELAY             30  // delay after a press - TODO can we combine this with above ?
    #define CAM_KEY_RELEASE_DELAY           30  // delay after a release - TODO do we really need to wait after release ?
    
    // RAW & DNG related values
    #define DEFAULT_RAW_EXT                 1   // extension to use for raw (see raw_exts in conf.c)
    #define DNG_BADPIXEL_VALUE_LIMIT        0   // Max value of 'bad' pixel - this value or lower is considered a defective pixel on the sensor
    #undef  CAM_RAW_ROWPIX                      // Number of pixels in RAW row (physical size of the sensor Note : as of July 2011, this value can be found in stub_entry.S for dryos cameras)
    #undef  CAM_RAW_ROWS                        // Number of rows in RAW (physical size of the sensor       Note : as of July 2011, this value can be found in stub_entry.S for dryos cameras)
    #undef  CAM_JPEG_WIDTH                      // Default crop size (width) stored in DNG (to match camera JPEG size. From dimensions of the largest size jpeg your camera produces)
    #undef  CAM_JPEG_HEIGHT                     // Default crop size (height) stored in DNG (to match camera JPEG size. From dimensions of the largest size jpeg your camera produces)
    #undef  CAM_ACTIVE_AREA_X1                  // Define usable area of the sensor - needs to be divisible by 4 - calibrate using a CHDK RAW image converted with rawconvert.exe (eg :rawconvert -12to8 -pgm -w=4480 -h=3348 photo.crw photo.pgm)
    #undef  CAM_ACTIVE_AREA_Y1                  // Define usable area of the sensor - needs to be divisible by 2 - "
    #undef  CAM_ACTIVE_AREA_X2                  // Define usable area of the sensor - needs to be divisible by 4 - "
    #undef  CAM_ACTIVE_AREA_Y2                  // Define usable area of the sensor - needs to be divisible by 2 = "
    #undef  cam_CFAPattern                      // Camera Bayer sensor data layout (DNG colors are messed up if not correct)
                                                //   should be 0x01000201 = [Green Blue Red Green], 0x02010100 = [Red Green Green Blue] or 0x01020001 = [Green Red Blue Green]
    #undef  CAM_COLORMATRIX1                    // DNG color profile matrix
    #undef  cam_CalibrationIlluminant1          // DNG color profile illuminant - set it to 17 for standard light A
    #undef  CAM_COLORMATRIX2                    // DNG color profile matrix 2
    #undef  cam_CalibrationIlluminant2          // DNG color profile illuminant 2 - set it to 21 for D65
    #undef  CAM_CAMERACALIBRATION1              // DNG camera calibration matrix 1
    #undef  CAM_CAMERACALIBRATION2              // DNG camera calibration matrix 2
    #undef  CAM_FORWARDMATRIX1                  // DNG camera forward matrix 1
    #undef  CAM_FORWARDMATRIX2                  // DNG camera forward matrix 2
    #undef  CAM_DNG_EXPOSURE_BIAS               // Specify DNG exposure bias value (to override default of -0.5 in the dng.c code)
    #undef  CAM_CALC_BLACK_LEVEL                // Set this to enable dynamic Black Level calculation from RAW sensor data
                                                // E.G. G1X may use black = 2048 for long exposures at high ISO
                                                // TODO: Raw Merge code does not use this - merging files with different black points may give strange results
    #undef  DNG_EXT_FROM                        // Extension in the cameras known extensions to replace with .DNG to allow DNG
                                                // files to be transfered over standard PTP. Only applicable to older cameras

    #undef  CAM_DNG_LENS_INFO                   // Define this to include camera lens information in DNG files
                                                // Value should be an array of 4 DNG 'RATIONAL' values specifying
                                                //   - min focal length in mm
                                                //   - max focal length in mm
                                                //   - max aperture at min focal length
                                                //   - max aperture at max focal length
                                                // E.G - SX30 = { 43,10, 1505,10, 27,10, 58,10 }
                                                //            = 4.3 - 150.5mm, f/2.7 - f.5.8
                                                // Each pair of integers is one 'RATIONAL' value (numerator,denominator)

    #undef  PARAM_CAMERA_NAME                   // parameter number for GetParameterData to get camera name
    #undef  PARAM_OWNER_NAME                    // parameter number for GetParameterData to get owner name
    #undef  PARAM_ARTIST_NAME                   // parameter number for GetParameterData to get artist name
    #undef  PARAM_COPYRIGHT                     // parameter number for GetParameterData to get copyright
    #undef  PARAM_DISPLAY_MODE1                 // param number for LCD display mode when camera in playback
    #undef  PARAM_DISPLAY_MODE2                 // param number for LCD display mode when camera in record view hold mode
    #define PARAMETER_DATA_FLAG     0x4000      // Added to 'id' value in calls to _GetParameterData & _SetParameterData
                                                // Override if not needed or different value needed

    #undef  CAM_NO_MEMPARTINFO                  // VXWORKS camera does not have memPartInfoGet, fall back to memPartFindMax

    #undef  CAM_DRIVE_MODE_FROM_TIMER_MODE      // use PROPCASE_TIMER_MODE to check for multiple shot custom timer.
                                                // Used to enabled bracketing in custom timer, required on many recent cameras
                                                // see http://chdk.setepontos.com/index.php/topic,3994.405.html

    #undef  CAM_AV_OVERRIDE_IRIS_FIX            // for cameras that require _MoveIrisWithAv function to override Av (for bracketing).

    #undef  CAM_DISABLE_RAW_IN_LOW_LIGHT_MODE   // For cameras with 'low light' mode that does not work with raw define this
    #undef  CAM_ISO_LIMIT_IN_HQ_BURST           // Defines max 'market' ISO override value for HQ Burst mode (higher values crash camera)
    #undef  CAM_MAX_ISO_OVERRIDE                // Defines max 'market' (non-zero) ISO override value - higher value may crash
    #undef  CAM_MIN_ISO_OVERRIDE                // Defines min 'market' (non-zero) ISO override value - lower value may crash if flash used [0 = AUTO, so always allowed]
    
    #undef  CAM_HAS_GPS                         // for cameras with GPS reseiver: includes the GPS coordinates in in DNG file

    #undef  CHDK_COLOR_BASE                     // Start color index for CHDK colors loaded into camera palette.

    #undef  CAM_LOAD_CUSTOM_COLORS              // Define to enable loading CHDK custom colors into the camera color palette
                                                // requires load_chdk_palette() and vid_get_bitmap_active_palette() to be defined
                                                // correctly for the camera along with

    #undef  CAM_CLEAN_OVERLAY                   // define when port supports making Canon overlay invisible (DIGIC 6)

    #define CAM_USB_EVENTID         0x902       // event ID for USB control. Changed to 0x202 in DryOS R49 so needs to be overridable.
                                                // For DryOS only. These are "control events", and don't have the same IDs as "logical events"

    #undef CAM_USB_EVENTID_VXWORKS              // For vxworks cameras that need additional code to unlock mode switching
                                                // this is the logical event "ConnectUSBCable", usually 0x1085
                                                // different from CAM_USB_EVENTID since it should be undefined on most cameras

    #define CAM_PTP_SCREEN_UNLOCK_EVENT 1       // define to allow sending event in switch_mode_usb

    #undef  CAM_NEED_SET_ZOOM_DELAY             // Define to add a delay after setting the zoom position before resetting the focus position in shooting_set_zoom 
                                                // ignored if CAM_REFOCUS_AFTER_ZOOM is 0 (default when CAM_USE_ALT_SET_ZOOM_POINT set)

    #undef  CAM_USE_ALT_SET_ZOOM_POINT          // Define to use the alternate code in lens_set_zoom_point(). Defaults on for Digic 4 and above, may be used on earlier cams
    #undef  CAM_USE_OPTICAL_MAX_ZOOM_STATUS     // Use ZOOM_OPTICAL_MAX to reset zoom_status when switching from digital to optical zoom in gui_std_kbd_process(). Only meaningful with CAM_CAN_UNLOCK_OPTICAL_ZOOM_IN_VIDEO
    #undef  CAM_REFOCUS_AFTER_ZOOM              // save and restore focus distance after set_zoom, Defaults off (0) for CAM_USE_ALT_SET_ZOOM_POINT, on for others

    #undef  CAM_HAS_HI_ISO_AUTO_MODE            // Define if camera has 'HI ISO Auto' mode (as well as Auto ISO mode), needed for adjustment in user auto ISO menu 

    #define CAMERA_MIN_DIST         0           // Define min distance that can be set in _MoveFocusLensToDistance (allow override - e.g. G12 min dist = 1)
    #undef  CAMERA_MAX_DIST                     // Define max distance that can be set in _MoveFocusLensToDistance (allow override for superzooms - SX30/SX40, default defined below depending on OS)

    #undef  DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY   // Draw pixels on active bitmap buffer only.
                                                // Requires bitmap_buffer & active_bitmap_buffer location in stubs_min.S or stubs_entry.S.

    #undef  CAM_SUPPORT_BITMAP_RES_CHANGE       // port can adapt to resolution change of the bitmap buffer (when composite or hdmi output is used)

    #undef  CAM_ZOOM_ASSIST_BUTTON_CONTROL      // Activate menu option to enable/disable the zoom assist button on the SX30/SX40
                                                // For other cameras, requires additional support code in kbd.c (see the SX30 or SX40 version)

    #undef  CAM_MISSING_RAND                    // Define this if srand()/rand() functions not found in firmware (a810/a2300)
    #undef  MKDIR_RETURN_ONE_ON_SUCCESS         // Define this if mkdir() return 1 on success, 0 on fail (a810/a1300)

    #undef  CAM_OPTIONAL_EXTRA_BUTTON           // extra button (remapped, for use in ALT mode) can be set from menu, must set next two values as well
    #undef  CAM_EXTRA_BUTTON_NAMES              // Define the list of names for the extra button   - e.g. { "Off", "Display" }
    #undef  CAM_EXTRA_BUTTON_OPTIONS            // Define the list of button constants for the extra button, starting with 0 - e.g. { 0, KEY_DISPLAY }

    #undef  CAM_HOTSHOE_OVERRIDE                // the sense switch of the hot shoe can be overridden

    #define CAM_AF_LED                      -1  // ***** NOTE ***** Need to override this in platform_camera.h with correct value
                                                // Defines 'led' Index value for camera_set_led function to control the AutoFocus assist LED
                                                // Used for the Motion Detect & Live View buffer testing

    #undef CAM_FILE_COUNTER_IS_VAR              // file counter is variable file_counter_var in stubs, not a param
    
    #define CAM_GUI_FSELECT_SIZE  20, 6, 14     // filename, filesize, filedate camera file select window column widths

    #undef  CAM_IS_VID_REC_WORKS                // Define if the 'is_video_recording()' function works

    #undef  CAM_HAS_CANON_RAW                   // Define if the camera has native raw in the canon firmware

    // Keyboard repeat and initial delays (override in platform_camera.h if needed)
    #define KBD_REPEAT_DELAY                175
    #define KBD_INITIAL_DELAY               500
    #undef CAM_PTP_USE_NATIVE_BUFFER            // define to use native firmware buffers for PTP transfers
                                                // for cameras that have problems transfering to cached RAM
                                                // or specific alignment requirements

    #undef CAM_PTP_FILE_BUFFER_ID               // define to override default ID for file transfer buffer

    #undef CAM_UNLOCK_ANALOG_AV_IN_REC          // define to enable analog video out in rec mode for cameras without native support
                                                // ANALOG_AV_IDX and ANALOG_AV_FLAG must be identified. Some cameras crash if AF point zoom enabled

    #undef CAM_HAS_WIFI                         // Camera has wifi support. Note in some cases Canon uses the same firmware
                                                // for models with and without wireless hardware. Where possible, this define
                                                // should reflect the actual hardware

    #undef CAM_HAS_DISPLAY_REFRESH_FLAG         // 'display_needs_refresh' variable is available to trigger CHDK UI update.

    #undef  CAM_HAS_PLAYBACK_IMAGE_NO           // Can find image number for currently displayed image in playback mode

// Base 'market' ISO value. Most (all?) DryOS R49 and later use 200, use tests/isobase.lua to check
#if defined(CAM_DRYOS_REL) && CAM_DRYOS_REL >= 49 // CAM_DRYOS_REL defined on command line, not from platform_camera.h
    #define CAM_MARKET_ISO_BASE             200
#else
    #define CAM_MARKET_ISO_BASE             100
#endif

    // Below limits are for the CHDK LFN support routines (DryOS only)
    // defaults can be overridden in camera_platform.h
    // file name limits used here based on what the OS functions can create/write without errors
    // in many cases, firmware can list, open and read longer paths
#if defined(CAM_DRYOS_REL) // CAM_DRYOS_REL defined on command line, not from platform_camera.h
    // #if CAM_DRYOS_REL == 20 // unknown
    #if CAM_DRYOS_REL == 23
        #define CAM_MAX_FNAME_LENGTH        18 
        #define CAM_MAX_PATH_LENGTH         255
    #elif CAM_DRYOS_REL == 31
        #define CAM_MAX_FNAME_LENGTH        18 
        #define CAM_MAX_PATH_LENGTH         59
    #elif CAM_DRYOS_REL == 39
        #define CAM_MAX_FNAME_LENGTH        30 
        #define CAM_MAX_PATH_LENGTH         32
    #elif CAM_DRYOS_REL == 43
        #define CAM_MAX_FNAME_LENGTH        30
        #define CAM_MAX_PATH_LENGTH         32
    #elif CAM_DRYOS_REL == 45
        #define CAM_MAX_FNAME_LENGTH        57 
        #define CAM_MAX_PATH_LENGTH         59
    #elif CAM_DRYOS_REL == 47
        #define CAM_MAX_FNAME_LENGTH        31 // note sx220hs seems to allow longer
        #define CAM_MAX_PATH_LENGTH         59
    #else // >= r49, and those unknown above
        #define CAM_MAX_FNAME_LENGTH        99 // camera can safely use filenames not longer than this (99 is a CHDK limit actually)
        #define CAM_MAX_PATH_LENGTH         255 // maximum path+filename length. Some actually allow much longer
    #endif 
#endif

// default CAM_USE_ALT_SET_ZOOM_POINT for digic >= 4
#if CAM_DIGIC >= 40
    #define  CAM_USE_ALT_SET_ZOOM_POINT       1   // Define to use the alternate code in lens_set_zoom_point()
#endif

//----------------------------------------------------------
// Overridden values for each camera
//----------------------------------------------------------

// Include the settings file for the camera model currently being compiled.
#include "platform_camera.h"

// force off for ILC - CAM_ILC is a platform_camera define, not makefile, so has to be after
// but otherwise default on (CAM_USE_ALT_SET_ZOOM_POINT for digic >= 4
#ifdef CAM_ILC
#undef CAM_CAN_UNLOCK_OPTICAL_ZOOM_IN_VIDEO
#undef CAM_USE_ALT_SET_ZOOM_POINT
#endif

// default CAM_ND_SET_AV_VALUE on for ND only cams
#if !defined(CAM_ND_SET_AV_VALUE)
    #if defined(CAM_HAS_ND_FILTER) && !defined(CAM_HAS_IRIS_DIAPHRAGM)
        #define CAM_ND_SET_AV_VALUE 1
    #else
        #define CAM_ND_SET_AV_VALUE 0
    #endif
#endif

// DryOS r31 and later cameras use 32bit subject distance values
// set a default limit that's high enough
// earlier cameras need 65535 as default
#if defined(CAM_DRYOS) && CAM_DRYOS_REL >= 31
    #ifndef CAMERA_MAX_DIST
        #define CAMERA_MAX_DIST 2000000
    #endif
#else
    #ifndef CAMERA_MAX_DIST
        #define CAMERA_MAX_DIST 65535
    #endif
#endif

#if !defined(CAM_IS_VID_REC_WORKS)
#if !defined(CAM_FILEIO_SEM_TIMEOUT)
    #define CAM_FILEIO_SEM_TIMEOUT          3000    // TakeSemaphore timeout
#endif
#else
#if !defined(CAM_FILEIO_SEM_TIMEOUT)
    #define CAM_FILEIO_SEM_TIMEOUT          0       // TakeSemaphore timeout - is_video_recording() == false
#endif
#if !defined(CAM_FILEIO_SEM_TIMEOUT_VID)
    #define CAM_FILEIO_SEM_TIMEOUT_VID      200     // TakeSemaphore timeout - is_video_recording() == true
#endif
#endif

#if defined(CAM_VIDEO_CONTROL) && defined(CAM_SIMPLE_MOVIE_STATUS)
    #error "CAM_VIDEO_CONTROL and CAM_SIMPLE_MOVIE_STATUS are mutually exclusive"
#endif

// video quality/bitrate defaults, moved here from conf.h
#ifdef CAM_MOVIEREC_NEWSTYLE
    // video defaults.for D6
    #define VIDEO_DEFAULT_QUALITY   9   // VBR relative min bitrate, 9 = CBR
    #define VIDEO_MAX_QUALITY       9   // 0..9
    #define VIDEO_DEFAULT_BITRATE   3   // 3 = fw default

    #undef  CAM_VIDEO_QUALITY_ONLY
#else
    // video quality defaults. Ideally, these should match the camera default settings
    #define VIDEO_DEFAULT_QUALITY   84  // ? where does 84 come from
    #define VIDEO_MAX_QUALITY       99
    #define VIDEO_DEFAULT_BITRATE   3   // should be 1 for all cams
#endif

// sanity check platform_camera.h defined values for some common errors
#if (CAM_ACTIVE_AREA_X1&1) || (CAM_ACTIVE_AREA_Y1&1) || (CAM_ACTIVE_AREA_X2&1) || (CAM_ACTIVE_AREA_Y2&1)
    #error "CAM_ACTIVE_AREA values must be even"
#endif

#if (CAM_ACTIVE_AREA_X2 - CAM_ACTIVE_AREA_X1) < CAM_JPEG_WIDTH
    #error "CAM_JPEG_WIDTH larger than active area"
#endif

#if (CAM_ACTIVE_AREA_Y2 - CAM_ACTIVE_AREA_Y1) < CAM_JPEG_HEIGHT
    #error "CAM_JPEG_HEIGHT larger than active area"
#endif

// default PTP buffer id
#ifndef CAM_PTP_FILE_BUFFER_ID
#if defined(CAM_DRYOS) && CAM_DRYOS_REL >= 43 && CAM_DRYOS_REL <= 52
    #define CAM_PTP_FILE_BUFFER_ID 5
#else
    #define CAM_PTP_FILE_BUFFER_ID 4
#endif
#endif // ifndef CAM_PTP_FILE_BUFFER_ID

// set default refocus behavior if not set by port
#ifndef CAM_REFOCUS_AFTER_ZOOM
#ifdef CAM_USE_ALT_SET_ZOOM_POINT
    #define CAM_REFOCUS_AFTER_ZOOM 0
#else
    #define CAM_REFOCUS_AFTER_ZOOM 1
#endif
#endif // CAM_REFOCUS_AFTER_ZOOM

#if defined(CAM_USE_OPTICAL_MAX_ZOOM_STATUS) && !defined(CAM_CAN_UNLOCK_OPTICAL_ZOOM_IN_VIDEO)
    #error "CAM_USE_OPTICAL_MAX_ZOOM_STATUS requires CAM_CAN_UNLOCK_OPTICAL_ZOOM_IN_VIDEO"
#endif


#ifdef CAM_PTP_SCREEN_UNLOCK_EVENT
// set default values based on OS. Could add the ability to override by
// setting ID in CAM_PTP_SCREEN_UNLOCK_EVENT, but not currently needed
#ifdef CAM_DRYOS
#define CAM_PTP_SCREEN_UNLOCK_EVENTID 4482
#else
#define CAM_PTP_SCREEN_UNLOCK_EVENTID 4418
#endif // CAM_DRYOS
#endif // CAM_PTP_SCREEN_UNLOCK_EVENT

//==========================================================
// END of Camera-dependent settings
//==========================================================

// For newer cameras where the screen bitmap is double the width we need to scale
// the CHDK horizontal (X) co-ordinates
#if CAM_USES_ASPECT_CORRECTION
    #define ASPECT_XCORRECTION(x)   ((x)<<1)    // See comments for CAM_USES_ASPECT_CORRECTION above
#else
    #define ASPECT_XCORRECTION(x)   (x)         // See comments for CAM_USES_ASPECT_CORRECTION above
#endif

// curves only work in 10bpp for now
#if CAM_SENSOR_BITS_PER_PIXEL != 10
    #undef OPT_CURVES
#endif

#ifndef OPT_PTP
    #undef CAM_CHDK_PTP
#endif

#ifdef CAM_FILEWRITETASK_SEEKS
    #define CAM_FILEWRITETASK_MULTIPASS 1
#endif

// Define default video AF scan buttons if not already defined in platform_camera.h
#if CAM_AF_SCAN_DURING_VIDEO_RECORD
    #ifndef CAM_VIDEO_AF_BUTTON_NAMES
        #define CAM_VIDEO_AF_BUTTON_NAMES   { "", "Shutter", "Set" }
        #define CAM_VIDEO_AF_BUTTON_OPTIONS { 0, KEY_SHOOT_HALF, KEY_SET }
    #endif
#endif

// Define default value for DISP button name in shortcut text unless aready set in platform_camera.h
// e.g. G1X uses Meter button as DISP in CHDK
#ifndef CAM_DISP_BUTTON_NAME
    #define CAM_DISP_BUTTON_NAME            "DISP"
#endif

// used for conditional LANG strings and convenience - defined if any non-USB remote input supported
#if (defined(CAM_REMOTE_AtoD_CHANNEL) || defined(CAM_REMOTE_HDMI_HPD) || defined(CAM_REMOTE_ANALOG_AV))
#define CAM_REMOTE_MULTICHANNEL 1
#endif
//------------------------------------------------------------------- 
// Keyboard / Button shortcuts - define in platform_camera.h
// if the default values are not suitable
// Default values are set below if not overridden
//------------------------------------------------------------------

// For models without ZOOM_LEVER  (#if !CAM_HAS_ZOOM_LEVER)
// SHORTCUT_SET_INFINITY is not used
// KEY_DISPLAY is used for gui_subj_dist_override_koef_enum;
// KEY_LEFT/KEY_RIGHT is used for gui_subj_dist_override_value_enum (because of no separate ZOOM_IN/OUT)

// Define keyboard / button shortcut values not already set above

//Alt mode
#if !defined(SHORTCUT_TOGGLE_RAW)
    #if CAM_HAS_ERASE_BUTTON
        #define SHORTCUT_TOGGLE_RAW         KEY_ERASE
    #else
        #define SHORTCUT_TOGGLE_RAW         KEY_DISPLAY
    #endif
#endif
#if !defined(CAM_HAS_MANUAL_FOCUS) && !defined(SHORTCUT_MF_TOGGLE)
    #if CAM_HAS_ERASE_BUTTON
        #define SHORTCUT_MF_TOGGLE          KEY_DISPLAY
    #else
        #define SHORTCUT_MF_TOGGLE          KEY_UP
    #endif
#endif

//Half press shoot button    
#if !defined(SHORTCUT_TOGGLE_HISTO)
    #if CAM_HAS_ERASE_BUTTON
        #define SHORTCUT_TOGGLE_HISTO       KEY_UP
    #else
        #define SHORTCUT_TOGGLE_HISTO       KEY_MENU
    #endif
#endif
#if !defined(SHORTCUT_TOGGLE_ZEBRA)
    #define SHORTCUT_TOGGLE_ZEBRA       KEY_LEFT
#endif
#if !defined(SHORTCUT_TOGGLE_OSD)
    #define SHORTCUT_TOGGLE_OSD         KEY_RIGHT
#endif
#if !defined(SHORTCUT_DISABLE_OVERRIDES)
    #define SHORTCUT_DISABLE_OVERRIDES  KEY_DOWN
#endif

//Alt mode & Manual mode  
#if !defined(SHORTCUT_SET_INFINITY)
    #if CAM_HAS_ERASE_BUTTON
        #define SHORTCUT_SET_INFINITY       KEY_UP
    #else
        #define SHORTCUT_SET_INFINITY       KEY_DISPLAY
    #endif
#endif
#if !defined(SHORTCUT_SET_HYPERFOCAL)
    #define SHORTCUT_SET_HYPERFOCAL     KEY_DOWN
#endif

#if CAM_HAS_ZOOM_LEVER
    #define SHORTCUT_SD_SUB KEY_ZOOM_OUT
    #define SHORTCUT_SD_ADD KEY_ZOOM_IN
#endif

//==========================================================

// DebugAssert
#ifndef CAM_3ARG_DebugAssert
    #ifdef THUMB_FW
        #define CAM_3ARG_DebugAssert        1   // Digic6 - default is 3 arg version of DebugAssert
    #else
        #define CAM_3ARG_DebugAssert        0   // All others default is 2 arg version
    #endif
#endif

//==========================================================
#include "camera_info.h"
//==========================================================

#endif /* CAMERA_H */
