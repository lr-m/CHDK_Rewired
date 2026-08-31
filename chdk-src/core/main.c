#include "platform.h"
#include "core.h"
#include "stdlib.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_osd.h"
#include "histogram.h"
#include "raw.h"
#include "console.h"
#include "shooting.h"
#include "lang.h"
#include "gui_lang.h"

#include "edgeoverlay.h"
#include "module_load.h"

#ifdef CAM_HAS_GPS
  #include "gps.h"
#endif

#include "kbd_common.h"

//==========================================================

extern char osd_buf[64];

// File scope so the persistent overlay can draw it with everything else instead
// of it being painted straight from spytask by the old route.
int no_modules_flag = 0;

#ifdef CAM_LOOP_PROFILE
// Time each section of the spytask loop and remember the worst one seen since
// the last report. spytask runs at 50Hz in the CHDK menu and 1Hz on the shooting
// screen, and the stock OSD elements are already skipped, so whatever blocks is
// one of the sections tagged below. File scope: gui.c reads these to display them.
int dbg_lp_worst = -1, dbg_lp_worst_ms = 0;
#define LP(idx, code)                                               \
    do {                                                            \
        int _t0 = get_tick_count();                                 \
        { code; }                                                   \
        { int _ms = get_tick_count() - _t0;                         \
          if (_ms > dbg_lp_worst_ms) {                              \
              dbg_lp_worst_ms = _ms; dbg_lp_worst = (idx); } }      \
    } while (0)
#else
#define LP(idx, code) do { code; } while (0)
#endif

//==========================================================

volatile int chdk_started_flag=0;

static volatile int spytask_can_start = 0;

static unsigned int memdmptick = 0;

void schedule_memdump(int memdmp_delay)
{
    memdmptick = get_tick_count() + memdmp_delay * 1000;
}

static void dump_memory()
{
    int fd;
    static int cnt=1;
    static char fn[32];

    // zero size means all RAM
    if (conf.memdmp_size == 0) conf.memdmp_size = (unsigned int)camera_info.maxramaddr + 1;
    // enforce RAM area
    if ((unsigned int)conf.memdmp_start > (unsigned int)camera_info.maxramaddr)
        conf.memdmp_start = 0;
    if ( (unsigned int)conf.memdmp_start + (unsigned int)conf.memdmp_size > ((unsigned int)camera_info.maxramaddr+1) )
        conf.memdmp_size = (unsigned int)camera_info.maxramaddr + 1 - (unsigned int)conf.memdmp_start;

    started();
    // try to avoid hanging the camera
    if ( !is_video_recording() ) {
        mkdir("A/DCIM");
        mkdir("A/DCIM/100CANON");
        fd = -1;
        do {
            sprintf(fn, "A/DCIM/100CANON/CRW_%04d.JPG", cnt++);
            if (stat(fn,0) != 0) {
                fd = open(fn, O_WRONLY|O_CREAT, 0777);
                break;
            }
        } while(cnt<9999);
        if (fd>=0) {
            if ( conf.memdmp_start == 0 ) {
                long val0 = *((long*)(0|CAM_UNCACHED_BIT));
                write(fd, &val0, 4);
                if  (conf.memdmp_size > 4) {
                    write(fd, (void*)4, conf.memdmp_size - 4);
                }
            }
            else {
                write(fd, (void*)conf.memdmp_start, conf.memdmp_size);
            }
            close(fd);
        }
        vid_bitmap_refresh();
    }
    finished();
}

int core_get_free_memory()
{
    cam_meminfo camera_meminfo;
    GetCombinedMemInfo(&camera_meminfo);
    return camera_meminfo.free_block_max_size;
}

static volatile long raw_data_available;

/* called from another process */
void core_rawdata_available()
{
    raw_data_available = 1;
}

void core_spytask_can_start() {
    spytask_can_start = 1;
}

// remote autostart
void script_autostart()
{
    // Tell keyboard task we are in <ALT> mode
    enter_alt(0);
    // We were called from the GUI task so switch to <ALT> mode before switching to Script mode
    gui_activate_alt_mode();
    // Switch to script mode and start the script running
    script_start_gui( 1 );
}

void core_spytask()
{
    int cnt = 1;
    int i=0;
#ifdef CAM_HAS_GPS
    int gps_delay_timer = 200 ;
    int gps_state = -1 ;
#endif
#if (OPT_DISABLE_CAM_ERROR)
    extern void DisableCamError();
    int dce_cnt=0;
    int dce_prevmode=0;
    int dce_nowmode;
#endif
    
    parse_version(&chdk_version, BUILD_NUMBER, BUILD_SVNREV);

    // Init camera_info bits that can't be done statically
    camera_info_init();

#if !defined(CAM_DRYOS)
// create semaphore to protect Canon memory malloc/free/memPartInfo
// on VxWorks, spytask should start before any other CHDK tasks
    extern void canon_malloc_init(void);
    canon_malloc_init();
#endif

    extern void aram_malloc_init(void);
    aram_malloc_init();

    extern void exmem_malloc_init(void);
    exmem_malloc_init();

#ifdef CAM_CHDK_PTP
    extern void init_chdk_ptp_task();
    init_chdk_ptp_task();
#endif

    while((i++<1000) && !spytask_can_start) msleep(10);

    started();
    msleep(50);
    finished();

#ifdef CAM_SET_DATE_ON_BOOT
    // The clock backup battery is dead in these bodies, so Canon finds the clock
    // unset on every power on and puts up its date/time screen. Set it instead of
    // trying to suppress the complaint - done as early as possible, before Canon
    // gets as far as deciding to show that screen.
    //
    // The date is arbitrary; there is no RTC to recover the real one from, and it
    // resets again at every power off. Anything valid stops the prompt.
    {
        extern void _SetDate(void *d);
        int d[6];
        d[0] = 2026;    // year  (1970..2069)
        d[1] = 1;       // month (1..12)
        d[2] = 1;       // day   (1..31)
        d[3] = 12;      // hour
        d[4] = 0;       // minute
        d[5] = 0;       // second
        _SetDate(d);
    }
#endif

#if !CAM_DRYOS
    drv_self_unhide();
#endif

    conf_restore();

    extern void gui_init();
    gui_init();

#if CAM_CONSOLE_LOG_ENABLED
    extern void cam_console_init();
    cam_console_init();
#endif

    static char *chdk_dirs[] =
    {
        "A/CHDK",
        "A/CHDK/FONTS",
        "A/CHDK/SYMBOLS",
        "A/CHDK/SCRIPTS",
        "A/CHDK/LANG",
        "A/CHDK/BOOKS",
        "A/CHDK/MODULES",
        "A/CHDK/MODULES/CFG",
        "A/CHDK/GRIDS",
        "A/CHDK/CURVES",
        "A/CHDK/DATA",
        "A/CHDK/LOGS",
        "A/CHDK/EDGE",
        "A/CHDK/BENDS",
        "A/CHDK/BOOT",
        "A/DCIM",
    };
    for (i = 0; i < (int)(sizeof(chdk_dirs) / sizeof(char*)); i++)
        mkdir_if_not_exist(chdk_dirs[i]);

    // "Are modules installed at all" - one probe file standing in for the set.
    //
    // Upstream only asks for the uppercase name, but every actual module load
    // goes through a lowercase one ("fselect.flt", "lua.flt", "edgeovr.flt" and
    // so on), and that is the case the build writes to the card. If the path
    // lookup here is case sensitive, the uppercase probe misses a card whose
    // modules are present and loading perfectly well, and the warning sits on
    // screen permanently. Ask for both spellings; only complain if neither is
    // there. Costs one extra stat at startup.
    //
    // Probed with open() rather than stat(), so the probe asks exactly the
    // question that matters: can the module loader open a module? stat() is a
    // "// 1" confidence stub on all three of these ports where open() is 109,
    // and more to the point a probe that succeeds where the real load would
    // fail is worse than useless - it hides the fault.
    //
    // (This was originally changed while chasing a module loading failure that
    // turned out to be the card's volume label, not stat. Kept because it is
    // the better probe on its own merits, not because it fixed anything.)
    {
        extern int open(const char *name, int flags, int mode);
        extern int close(int fd);
        int fd = open("A/CHDK/MODULES/fselect.flt", O_RDONLY, 0777);
        if (fd < 0)
            fd = open("A/CHDK/MODULES/FSELECT.FLT", O_RDONLY, 0777);
        no_modules_flag = (fd < 0) ? 1 : 0;
        if (fd >= 0)
            close(fd);
    }

    // Calculate the value of get_tick_count() when the clock ticks over to the next second
    // Used to calculate the SubSecondTime value when saving DNG files.
    long t1, t2;
    t2 = time(0);
    do
    {
        t1 = t2;
        camera_info.tick_count_offset = get_tick_count();
        t2 = time(0);
        msleep(10);
    } while (t1 != t2);
    camera_info.tick_count_offset = camera_info.tick_count_offset % 1000;

    // remote autostart
    if (conf.script_startup==SCRIPT_AUTOSTART_ALWAYS)
    {
        script_autostart();
    }
    else if (conf.script_startup==SCRIPT_AUTOSTART_ONCE)
    {
        conf.script_startup=SCRIPT_AUTOSTART_NONE;
        conf_save();
        script_autostart();
    }

    shooting_init();

    // if starting  with HDMI connected, suspend drawing for 3s to avoid crashes
#ifdef HDMI_HPD_FLAG
    if(get_hdmi_hpd_physw_mod()) {
        draw_suspend(3000);
    }
#endif

    if(conf.check_firmware_crc) {
        module_run("fwcrc.flt");
    }

    while (1)
    {
#ifdef CAM_OSD_FORCE_DRAW_IN_SPYTASK
        // Raw loop iterations, counted before ANY gate. Compared against R this
        // separates "the loop is slow" from "the loop is fine but the redraw is
        // being gated out". Expect ~50 (msleep(20) per pass).
        {
            extern int dbg_loops;
            dbg_loops++;
        }
#endif
#ifdef CAM_CLOCK_VALID_FLAG
        // Stop Canon's date/time screen coming up on every power on now that the
        // clock backup batteries in these bodies are dead.
        //
        // Traced in the a470 102c decompilation. The gate is FUN_ffc19cd0:
        //
        //     if (*(uictrl + 0x34) == 0 && clock_is_valid() == 0) {
        //         show_date_screen(0);          // FUN_ffc588a0
        //         *(uictrl + 0x34) = 1;         // "already asked" latch
        //     }
        //
        // clock_is_valid() is FUN_ffc2f70c, which just returns the word this
        // define points at. It is set by the clock init at FUN_ffc2f7d8, which
        // compares the RTC against a built-in calendar and stores 1 for good /
        // 0 for bad - with a flat battery it always stores 0.
        //
        // Note the polarity: this is a VALID flag, so it is set to 1, not
        // cleared. An earlier attempt cleared a word at 0x55f0 believing it was
        // an "invalid" flag off IsInvalidTime(); that turned out to be a system
        // timer helper next to PauseTimeOfSystem/ResumeTimeOfSystem and had
        // nothing to do with the calendar, which is why it never had any effect.
        //
        // Written every pass rather than once. It is two stores and it removes
        // any dependence on whether spytask beats Canon's startup to it - the
        // getter is called live from ~25 sites, not cached at boot.
        *(volatile int*)CAM_CLOCK_VALID_FLAG = 1;
#ifdef CAM_DATE_PROMPT_LATCH
        // Belt and braces: set the "already asked" latch too, so the gate above
        // fails on its first term regardless of what the clock reports.
        *(volatile int*)CAM_DATE_PROMPT_LATCH = 1;
#endif
#endif

        // Set up camera mode & state variables
        LP(1, mode_get());

#ifdef CAM_CLEAN_OVERLAY
        extern void handle_clean_overlay();
        handle_clean_overlay();
#endif

#ifdef  CAM_UNLOCK_ANALOG_AV_IN_REC
        extern void handle_analog_av_in_rec();
        handle_analog_av_in_rec();
#endif
        // update HDMI power override based on mode and remote settings
#ifdef CAM_REMOTE_HDMI_POWER_OVERRIDE
        extern void update_hdmi_power_override(void);
        update_hdmi_power_override();
#endif

        extern void set_palette();
        set_palette();

#if (OPT_DISABLE_CAM_ERROR)
        dce_nowmode = camera_info.state.mode_play;
        if (dce_prevmode==dce_nowmode)
        {                       //no mode change
            dce_cnt++;          // overflow is not a concern here
        }
        else
        {                       //mode has changed
            dce_cnt=0;
        }
        if (dce_cnt==100)
        {                       // 1..2s past play <-> rec mode change
            DisableCamError();
        }
        dce_prevmode=dce_nowmode;
#endif

        if ( memdmptick && ((unsigned)get_tick_count() >= memdmptick) )
        {
            memdmptick = 0;
            dump_memory();
        }

#ifdef CAM_HAS_GPS
        if ( --gps_delay_timer == 0 )
        {
            gps_delay_timer = 50 ;
            if ( gps_state != (int)conf.gps_on_off )
            {
                gps_state = (int)conf.gps_on_off ;
                init_gps_startup(!gps_state) ; 
            }
        }
#endif        
        
        // Change ALT mode if the KBD task has flagged a state change
        LP(2, gui_activate_alt_mode());

#ifdef  CAM_LOAD_CUSTOM_COLORS
        // Color palette function
        extern void load_chdk_palette();
        load_chdk_palette();
#endif

        // A bent shot may be waiting to have its recipe written into it - see
        // include/bend_tag.h. Costs one comparison per pass when nothing is
        // queued, which is every pass but a handful.
        {
            extern void bend_tag_service(void);
            bend_tag_service();
        }

        if (raw_data_available)
        {
            raw_process();
            extern void hook_raw_save_complete();
            hook_raw_save_complete();
            raw_data_available = 0;
#ifdef CAM_HAS_GPS
            if (((int)conf.gps_on_off == 1) && ((int)conf.gps_waypoint_save == 1)) gps_waypoint();
#endif
#if defined(CAM_CALC_BLACK_LEVEL)
            // Reset to default in case used by non-RAW process code (e.g. raw merge)
            camera_sensor.black_level = CAM_BLACK_LEVEL;
#endif
            continue;
        }

        if ((camera_info.state.state_shooting_progress != SHOOTING_PROGRESS_PROCESSING) || recreview_hold)
        {
#ifdef CAM_HOLD_SCREEN_LOCK_IN_REC
            // See CAM_HOLD_SCREEN_LOCK_IN_REC in camera.h.
            //
            // The lock has to be re-asserted on EVERY pass, not just when entering
            // record mode. Canon clears the gate itself from its live view path,
            // and CHDK's own draw_restore() -> vid_bitmap_refresh() clears it too,
            // so a lock taken once on entry silently stops holding within a frame
            // or two. In <ALT> mode nothing clears it, which is why locking once is
            // enough there and was enough to make the menus stable.
            //
            // This is why the option requires a flag-style gate that is idempotent
            // to write. Do not enable it on a port whose gate is a lock *counter*
            // (the a480's is) - re-asserting would run the count away.
            {
                extern int canon_menu_active;
                static int lock_held = 0;
                // Deliberately NOT gated on gui_mode_none. Measured on the a470 in
                // the shooting screen it reads 0, so including it meant want_lock
                // was never once true and the lock never engaged on any build.
                // It was the wrong test anyway: what matters is that Canon has
                // nothing of its own it must draw, not which CHDK GUI mode is up.
                int want_lock = camera_info.state.mode_rec
                             && !camera_info.state.is_shutter_half_press
                             && (canon_menu_active == (int)&canon_menu_active-4);
                if (want_lock)
                {
                    vid_turn_off_updates();
                }
                else if (lock_held)
                {
                    vid_turn_on_updates();
                }
                lock_held = want_lock;
            }
#endif
            if (((cnt++) & CAM_OSD_REDRAW_MASK) == 0) {
                LP(3, gui_redraw());
#ifndef CAM_PERSISTENT_OSD
                if ( no_modules_flag == 1 ) {
                    // visible warning if modules missing - always drawn on top
                    draw_string(FONT_WIDTH, FONT_HEIGHT, lang_str(LANG_ERROR_MISSING_MODULES), user_color(conf.osd_color_warn));
                }
#endif
#ifdef CAM_OSD_FORCE_DRAW_IN_SPYTASK
                // Draw the OSD the same way the warning above is drawn: straight
                // from spytask, after gui_redraw(), depending on nothing except
                // being in record mode.
                //
                // gui_redraw() reaches the OSD only via gui_mode->redraw, i.e. only
                // if defaultGuiHandler happens to be the installed handler and only
                // if gui_default_draw() gets past its early exits. Measured on the
                // a470, gui_draw_osd() ran once a second instead of ~25 times even
                // after the canon_menu_active gate was removed, so something further
                // up that chain is dropping the call. This does not care.
                //
                // Drawing twice in <ALT> mode (gui_chdk_draw already calls it) is
                // harmless - the same pixels get written with the same values.
                // Only when no CHDK GUI is up - otherwise this draws the OSD over
                // the menu, and the menu handler and this fight over the screen.
                if (camera_info.state.mode_rec && conf.show_osd &&
                    camera_info.state.gui_mode_none)
                {
                    gui_draw_osd();
                }
                // Redraw-rate tell-tale, as a number rather than a spinner so it can
                // just be read off. R is how many times a second we get here, i.e.
                // how often the OSD is actually being drawn. Expect ~25.
                // If R is ~25 and the overlay still vanishes, something is erasing
                // it. If R is ~1, spytask itself is stalling and no drawing change
                // will help. G counts gui_mode changes per second - if that is not
                // 0, CHDK's GUI mode is being switched underneath us, which is what
                // the menu opening on its own suggests.
                {
                    extern int dbg_mode_changes, dbg_loops;
                    static int passes = 0, passes_sec = 0, g_sec = 0, l_sec = 0, last = 0;
                    int tk;
                    passes++;
                    tk = get_tick_count();
                    if ((tk - last) >= 1000)
                    {
                        passes_sec = passes; passes = 0;
                        g_sec = dbg_mode_changes; dbg_mode_changes = 0;
                        l_sec = dbg_loops;        dbg_loops        = 0;
                        last = tk;
                    }
                    // L = raw loop rate (expect ~50). R = redraws (expect ~25).
                    // S = state_shooting_progress, which gates the redraw block:
                    //     if S is stuck at SHOOTING_PROGRESS_PROCESSING then
                    //     gui_redraw() is skipped and that alone is the bug.
                    // W = index of the slowest OSD element seen, M = its ms.
                    // 0 histo  1 grid  2 dof  3 values  4 state  5 rawinfo
                    // 6 batt   7 space 8 temp 9 clock  10 usb   11 ev
                    // 12 movietime  13 debugvals
                    {
                        extern int dbg_osd_worst, dbg_osd_worst_ms;
                        sprintf(osd_buf, "L%d R%d W%d M%d",
                                l_sec, passes_sec, dbg_osd_worst, dbg_osd_worst_ms);
                        dbg_osd_worst = -1; dbg_osd_worst_ms = 0;
                    }
                    draw_string(camera_screen.width - FONT_WIDTH*16, 0, osd_buf,
                                user_color(conf.osd_color_warn));
                }
#endif
#ifdef CAM_DRAW_RGBA
                extern int display_needs_canon_refresh;
                if (!draw_is_suspended()) {
                    if (display_needs_canon_refresh) {
                        display_needs_canon_refresh = 0;
                        vid_bitmap_refresh();
                    }
                }
#endif
            }

#ifdef CAM_SCREEN_LOCK_DEBUG
            // TEMPORARY DIAGNOSTIC - remove once the flicker is understood.
            // Answers three questions that guessing could not:
            //   rec/non/hp/cm - which term of the lock condition is false, if any
            //   blk           - is the gate actually still set, or is something
            //                   clearing it faster than we re-assert it
            //   wipe          - how many times a second the bitmap buffer is being
            //                   clobbered (guard pixel destroyed), which tells us
            //                   whether anything is writing it while the gate holds
            {
                extern int refresh_physical_screen_blocked;
                extern int dbg_osd_draws, dbg_restores;
                extern int dbg_dd_enter, dbg_dd_hdp, dbg_dd_gate, dbg_dd_noosd, dbg_cm_ok;
                #define CANARY_VAL 0xA5
                static int c0 = 0, c1 = 0, last_tick = 0;
                static int c_sec = 0, dr_sec = 0, rs_sec = 0;
                static int en_sec = 0, hp_sec = 0, gt_sec = 0, no_sec = 0, cm_sec = 0;
                int t;
                // Sample a pixel in the middle of the screen rather than the guard
                // pixel at (0,0). The guard never tripped, but that only shows the
                // top-left corner survives - Canon may repaint a sub-region that
                // excludes it. Mid-screen is where the overlay actually lives.
                // Checked in each buffer separately, because "buffer 0 is clean but
                // buffer 1 is clobbered" and "neither is touched" mean very
                // different things: the first is an overwrite, the second means the
                // display is showing a buffer CHDK does not write at all.
                unsigned char *fb0 = (unsigned char*)vid_get_bitmap_fb();
                unsigned char *fb1 = fb0 + camera_screen.buffer_size;
                unsigned int off = (camera_screen.height/2) * camera_screen.buffer_width
                                 + (camera_screen.width/2);

                if (fb0[off] != CANARY_VAL) c0++;
                if (fb1[off] != CANARY_VAL) c1++;
                fb0[off] = fb1[off] = CANARY_VAL;

                // Latch all the per-second counters together, in one place.
                //   dr = how many times a second gui_draw_osd() actually runs.
                //        ~25 means the overlay is drawn constantly and something
                //        removes it; ~1 means the draw path itself is being
                //        skipped, and no amount of screen locking will help.
                //   rs = draw_restore() calls - CHDK asking Canon to repaint,
                //        which wipes our overlay as a side effect.
                //   c  = canary trips, i.e. the buffers being written by anyone
                //        other than us.
                t = get_tick_count();
                if ((t - last_tick) >= 1000)
                {
                    c_sec  = c0 + c1;   c0 = c1 = 0;
                    dr_sec = dbg_osd_draws; dbg_osd_draws = 0;
                    rs_sec = dbg_restores;  dbg_restores  = 0;
                    en_sec = dbg_dd_enter;  dbg_dd_enter  = 0;
                    hp_sec = dbg_dd_hdp;    dbg_dd_hdp    = 0;
                    gt_sec = dbg_dd_gate;   dbg_dd_gate   = 0;
                    no_sec = dbg_dd_noosd;  dbg_dd_noosd  = 0;
                    cm_sec = dbg_cm_ok;     dbg_cm_ok     = 0;
                    last_tick = t;
                }
                // Two lines: the second breaks down where the passes are going.
                sprintf(osd_buf, "gm%d blk%d dr%d rs%d c%d",
                        camera_info.state.gui_mode,
                        refresh_physical_screen_blocked,
                        dr_sec, rs_sec, c_sec);
                draw_string(0, FONT_HEIGHT*3, osd_buf, user_color(conf.osd_color_warn));
                sprintf(osd_buf, "en%d hp%d gt%d no%d cm%d",
                        en_sec, hp_sec, gt_sec, no_sec, cm_sec);
                draw_string(0, FONT_HEIGHT*4, osd_buf, user_color(conf.osd_color_warn));
            }
#endif
        }

        if (camera_info.state.state_shooting_progress != SHOOTING_PROGRESS_PROCESSING)
        {
#ifndef CAM_PERSISTENT_OSD
            // Skipped entirely under CAM_PERSISTENT_OSD: the overlay computes
            // its own luma histogram in gui.c, on its own timer, and does not
            // use libhisto at all. Leaving this in would walk the viewport ~50
            // times a second to build a histogram nothing then reads - and on a
            // DIGIC II body that is waste you can feel in the UI.
            if (conf.show_histo)
                LP(4, libhisto->histogram_process());
#endif

            if ((camera_info.state.gui_mode_none || camera_info.state.gui_mode_alt) && conf.edge_overlay_thresh && conf.edge_overlay_enable)
            {
                // We need to skip first tick because stability
                if (chdk_started_flag)
                {
                    LP(5, libedgeovr->edge_overlay());
                }
            }
        }

        if ((camera_info.state.state_shooting_progress == SHOOTING_PROGRESS_PROCESSING) && (!shooting_in_progress()))
        {
            camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_DONE;
        }

        i = 0;

#ifdef DEBUG_PRINT_TO_LCD
        sprintf(osd_buf, "%d", cnt );   // modify cnt to what you want to display
        draw_txt_string(1, i++, osd_buf, user_color(conf.osd_color));
#endif
#if defined(OPT_FILEIO_STATS)
        sprintf(osd_buf, "%3d %3d %3d %3d %3d %3d %3d %4d",
                camera_info.fileio_stats.fileio_semaphore_errors, camera_info.fileio_stats.close_badfile_count,
                camera_info.fileio_stats.write_badfile_count, camera_info.fileio_stats.open_count,
                camera_info.fileio_stats.close_count, camera_info.fileio_stats.open_fail_count,
                camera_info.fileio_stats.close_fail_count, camera_info.fileio_stats.max_semaphore_timeout);
        draw_txt_string(1, i++, osd_buf,user_color( conf.osd_color));
#endif

        if (camera_info.perf.md_af_tuning)
        {
            sprintf(osd_buf, "MD last %-4d min %-4d max %-4d avg %-4d", 
                camera_info.perf.af_led.last, camera_info.perf.af_led.min, camera_info.perf.af_led.max, 
                (camera_info.perf.af_led.count>0)?camera_info.perf.af_led.sum/camera_info.perf.af_led.count:0);
            draw_txt_string(1, i++, osd_buf, user_color(conf.osd_color));
        }

        // Process async module unload requests
        LP(6, module_tick_unloader());

        msleep(20);
        chdk_started_flag=1;
    }
}
