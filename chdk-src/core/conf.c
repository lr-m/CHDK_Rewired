#include "platform.h"
#include "conf.h"
#include "font.h"
#include "raw.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_osd.h"
#include "gui_lang.h"
#include "gui_menu.h"
#include "gui_user_menu.h"
#include "core.h"
#include "fileutil.h"
#include "lang.h"
#include "usb_remote.h"

#include "modules.h"
#include "module_def.h"

//-------------------------------------------------------------------
Conf conf;

// reyalp: putting these in conf, since the conf values are lookups for them
// prefixes and extentions available for raw images (index with conf.raw_prefix etc)
const char* img_folders[NUM_IMG_FOLDER_NAMES]={ "A/DCIM", "withJPG", "A/RAW" };
const char* img_prefixes[NUM_IMG_PREFIXES]={ "IMG_", "CRW_", "SND_" /*, "AUT_", "ETC_","SDR_", "MVI_", "MRK_"*/};
const char* img_exts[NUM_IMG_EXTS]={ ".JPG", ".CRW", ".CR2", ".THM", ".WAV"/*, ".AVI", ".MRK"*/};

//-------------------------------------------------------------------

void conf_change_dng(void)
{
    if (conf.save_raw && conf.dng_raw && conf.dng_version)
    {
        if (!libdng->badpixel_list_loaded_b()) libdng->load_bad_pixels_list_b("A/CHDK/badpixel.bin");
        if (!libdng->badpixel_list_loaded_b()) conf.dng_version=0;
    }
}

#if defined (DNG_EXT_FROM)
void cb_change_dng_usb_ext()
{
    extern void change_ext_to_dng(void);
    extern void change_ext_to_default(void);
    if (conf.dng_usb_ext)
        change_ext_to_dng();
    else
        change_ext_to_default();
}
#endif

/*
update the prevent display off/prevent shutdown based on current state
doesn't really belong in conf but not clear where else it should go
*/
void conf_update_prevent_shutdown(void) {
    if(conf.alt_prevent_shutdown == ALT_PREVENT_SHUTDOWN_ALWAYS 
        || (conf.alt_prevent_shutdown == ALT_PREVENT_SHUTDOWN_ALT && !camera_info.state.gui_mode_none)
        || (conf.alt_prevent_shutdown == ALT_PREVENT_SHUTDOWN_ALT_SCRIPT && camera_info.state.state_kbd_script_run)) {
        disable_shutdown();
    } else {
        enable_shutdown();
    }
}

//-------------------------------------------------------------------
void reset_alt_button()
{
#if CAM_ADJUSTABLE_ALT_BUTTON
    extern const char* gui_alt_mode_button_enum(int change, int arg);
    gui_alt_mode_button_enum(0,0); // will reset if not in list of valid alt buttons
#else
    conf.alt_mode_button = CAM_DEFAULT_ALT_BUTTON; // if not adjustable, force to default
#endif
}

void clear_values()
{
    reset_alt_button();

    if (conf.platformid != PLATFORMID) // the following config entries will be resetted if you switch the camera using the same cfg
    {
        conf.fast_ev = 0;
        conf.fast_movie_control = 0;
        conf.fast_movie_quality_control = 0;
        conf.platformid = PLATFORMID;
        conf.flash_video_override = 0;

        // remote input channel - all cams assumed to support USB
        // TODO could reset only if invalid on current cam
        conf.remote_input_channel = REMOTE_INPUT_USB;

        // video related entries are interpreted differently on D6 cameras
        conf.video_mode = 0;
        conf.video_quality = VIDEO_DEFAULT_QUALITY;
        conf.video_bitrate = VIDEO_DEFAULT_BITRATE;
        shooting_video_bitrate_change(conf.video_bitrate);
#ifdef CAM_MOVIEREC_NEWSTYLE
        shooting_video_minbitrate_change(conf.video_quality);
#endif
    }
    if (conf.osd_platformid != PLATFORMID) // the following config entries will be resetted if you switch the camera using the same cfg
    {
        conf.zoom_scale = 100;
        // battery voltages are camera dependent
        conf.batt_volts_max = get_vbatt_max();
        conf.batt_volts_min = get_vbatt_min();
        conf.osd_platformid = PLATFORMID;

        conf.mem_view_addr_init = 0x1000;

    }

    if (conf.clear_override)
    {
        conf.av_override_enabled=0;
        conf.tv_override_enabled=0;
        conf.subj_dist_override_koef=SD_OVERRIDE_OFF;
        conf.iso_override_koef=0;
        conf.nd_filter_state=0;
    }
    if (conf.clear_bracket)
    {
        conf.av_bracket_value=0;
        conf.tv_bracket_value=0;
        conf.iso_bracket_koef=0;
        conf.subj_dist_bracket_koef=0;
    }
    if (conf.clear_video)
    {
        conf.video_mode = 0;
#if CAM_CHDK_HAS_EXT_VIDEO_TIME
        conf.ext_video_time=0;
#endif
        conf.video_quality = VIDEO_DEFAULT_QUALITY;
        conf.video_bitrate = VIDEO_DEFAULT_BITRATE;
        shooting_video_bitrate_change(conf.video_bitrate);
#ifdef CAM_MOVIEREC_NEWSTYLE
        shooting_video_minbitrate_change(conf.video_quality);
#endif
    }
    //conf.edge_overlay_pano = 0; // reset it because otherwise this feature cant be used at startup (when buffer is empty) - needs workaround other than this!
}

//-------------------------------------------------------------------
static ConfInfo user_menu_conf_info[] = {
    CONF_INFO(  1, conf.user_menu_vars,         CONF_STRUCT_PTR,i:0),
    CONF_INFO(  2, conf.user_menu_as_root,      CONF_DEF_VALUE, i:0),
    CONF_INFO(  3, conf.user_menu_enable,       CONF_DEF_VALUE, i:0),

    {0}
};

void user_menu_conf_info_func(unsigned short id)
{
    switch (id)
    {
    case 1: 
        user_menu_restore(); 
        break;
    }
}

// Range of entry IDs for user adjustable colors.
// Used to reset colors. Adjust these if color entries changes
#define COLOR_FIRST_OVERRIDE    50
#define COLOR_LAST_OVERRIDE     63

static ConfInfo osd_conf_info[] = {
/* !!! Do NOT change ID for items defined already! Append a new one at the end! !!! */

    CONF_INFO(  1, conf.show_osd,                               CONF_DEF_VALUE, i:1),
    CONF_INFO(  2, conf.show_osd_in_review,                     CONF_DEF_VALUE, i:0),
    CONF_INFO(  3, conf.override_disable,                       CONF_DEF_VALUE, i:0),
    CONF_INFO(  4, conf.override_disable_all,                   CONF_DEF_VALUE, i:1),
    CONF_INFO(  5, conf.hide_osd,                               CONF_DEF_VALUE, i:1),
    CONF_INFO(  6, conf.rotate_osd,                             CONF_DEF_VALUE, i:0),
    CONF_INFO(  7, conf.show_hiddenfiles,                       CONF_DEF_VALUE, i:0),

    // The default X & Y position values below are based on 360x240 size screen display.
    // The CONF_INFOP macro scales these, at build time, based on the actual camera screen size (CAM_SCREEN_WIDTH & CAM_SCREEN_HEIGHT).
#ifdef CAM_PERSISTENT_OSD
    // The stock default (45,150) puts a ~128x100 histogram straight on top of
    // the overlay's bottom-left plate, and partly off the bottom of the screen.
    // Sit it above them instead: y 40..140 clears both the plates (184..236) and
    // the bend mode pin strip (0..36). Movable as usual in the OSD layout editor.
    CONF_INFOP( 20, conf.histo_pos,                             CONF_OSD_POS,   10, 40),
#else
    CONF_INFOP( 20, conf.histo_pos,                             CONF_OSD_POS,   45,150),
#endif
    CONF_INFOP( 21, conf.dof_pos,                               CONF_OSD_POS,   90,45),
    CONF_INFOP( 22, conf.batt_icon_pos,                         CONF_OSD_POS,   178,0),
    CONF_INFOP( 23, conf.batt_txt_pos,                          CONF_OSD_POS,   178,16),
    CONF_INFOP( 24, conf.mode_state_pos,                        CONF_OSD_POS,   35,0),
    CONF_INFOP( 25, conf.values_pos,                            CONF_OSD_POS,   288,30),
    CONF_INFOP( 26, conf.clock_pos,                             CONF_OSD_POS,   294,0),
    CONF_INFOP( 27, conf.space_icon_pos,                        CONF_OSD_POS,   260,0),
    CONF_INFOP( 28, conf.space_txt_pos,                         CONF_OSD_POS,   128,0),
    CONF_INFOP( 29, conf.mode_raw_pos,                          CONF_OSD_POS,   302,214),
    CONF_INFOP( 30, conf.space_ver_pos,                         CONF_OSD_POS,   353,0),
    CONF_INFOP( 31, conf.space_hor_pos,                         CONF_OSD_POS,   0,233),
    CONF_INFOP( 32, conf.mode_video_pos,                        CONF_OSD_POS,   158,142),
    CONF_INFOP( 33, conf.mode_ev_pos,                           CONF_OSD_POS,   38,110),
    CONF_INFOP( 34, conf.temp_pos,                              CONF_OSD_POS,   286,16),
    CONF_INFOP( 35, conf.ev_video_pos,                          CONF_OSD_POS,   18,80),
    CONF_INFOP( 36, conf.usb_info_pos,                          CONF_OSD_POS,   95,0),
    CONF_INFOP( 37, conf.raw_ev_histo_pos,                      CONF_OSD_POS,   10,130),

    // Keep these together
    CONF_INFOC( 50, conf.histo_color,                           CONF_DEF_VALUE, IDX_COLOR_GREY_DK,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 51, conf.histo_color2,                          CONF_DEF_VALUE, IDX_COLOR_RED,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 52, conf.osd_color,                             CONF_DEF_VALUE, IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 53, conf.osd_color_warn,                        CONF_DEF_VALUE, IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_RED,1,1),
    CONF_INFOC( 54, conf.osd_color_override,                    CONF_DEF_VALUE, IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_RED,1,1),
    CONF_INFOC( 55, conf.menu_color,                            CONF_DEF_VALUE, IDX_COLOR_GREY_DK,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 56, conf.menu_title_color,                      CONF_DEF_VALUE, IDX_COLOR_WHITE,IDX_COLOR_BLACK,1,1),
    CONF_INFOC( 57, conf.menu_cursor_color,                     CONF_DEF_VALUE, CAM_DEFAULT_MENU_CURSOR_BG,CAM_DEFAULT_MENU_CURSOR_FG,1,1),
    CONF_INFOC( 58, conf.menu_symbol_color,                     CONF_DEF_VALUE, IDX_COLOR_GREY_DK,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 59, conf.reader_color,                          CONF_DEF_VALUE, IDX_COLOR_GREY,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 60, conf.grid_color,                            CONF_DEF_VALUE, IDX_COLOR_GREY_DK,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 61, conf.space_color,                           CONF_DEF_VALUE, IDX_COLOR_GREY_DK_TRANS,IDX_COLOR_WHITE,1,1),
    CONF_INFOC( 62, conf.zebra_color,                           CONF_DEF_VALUE, IDX_COLOR_RED,IDX_COLOR_RED,1,1),
    CONF_INFOC( 63, conf.edge_overlay_color,                    CONF_DEF_VALUE, 0,IDX_COLOR_RED,0,1),

    CONF_INFO( 80, conf.show_clock,                             CONF_DEF_VALUE, i:2),
    CONF_INFO( 81, conf.clock_format,                           CONF_DEF_VALUE, i:0),
    CONF_INFO( 82, conf.clock_indicator,                        CONF_DEF_VALUE, i:0),
    CONF_INFO( 83, conf.clock_halfpress,                        CONF_DEF_VALUE, i:1),

    CONF_INFO( 90, conf.show_state,                             CONF_DEF_VALUE, i:1),

    CONF_INFO(100, conf.batt_volts_max,                         CONF_FUNC_PTR,  func:get_vbatt_max),
    CONF_INFO(101, conf.batt_volts_min,                         CONF_FUNC_PTR,  func:get_vbatt_min),
    CONF_INFO(102, conf.batt_perc_show,                         CONF_DEF_VALUE, i:1),
    CONF_INFO(103, conf.batt_volts_show,                        CONF_DEF_VALUE, i:0),
    CONF_INFO(104, conf.batt_icon_show,                         CONF_DEF_VALUE, i:1),

    CONF_INFO(110, conf.reader_file,                            CONF_CHAR_PTR,  ptr:"A/CHDK/BOOKS/README.TXT"),
    CONF_INFO(111, conf.reader_pos,                             CONF_DEF_VALUE, i:0),
    CONF_INFO(112, conf.reader_autoscroll,                      CONF_DEF_VALUE, i:0),
    CONF_INFO(113, conf.reader_autoscroll_delay,                CONF_DEF_VALUE, i:5),
    CONF_INFO(114, conf.reader_rbf_file,                        CONF_CHAR_PTR,  ptr:""),
    CONF_INFO(115, conf.reader_codepage,                        CONF_DEF_VALUE, i:FONT_CP_WIN),
    CONF_INFO(116, conf.reader_wrap_by_words,                   CONF_DEF_VALUE, i:1),

    CONF_INFO(120, conf.splash_show,                            CONF_DEF_VALUE, i:1),
   	CONF_INFO(121, conf.start_sound,                            CONF_DEF_VALUE, i:0),

    CONF_INFO(130, conf.menu_symbol_enable,                     CONF_DEF_VALUE, i:1),
    CONF_INFO(131, conf.menu_rbf_file,                          CONF_CHAR_PTR,  ptr:""),
    CONF_INFO(132, conf.menu_symbol_rbf_file,                   CONF_CHAR_PTR,  ptr:DEFAULT_SYMBOL_FILE),
    CONF_INFO(133, conf.menu_select_first_entry,                CONF_DEF_VALUE, i:1),
    CONF_INFO(134, conf.menu_center,                            CONF_DEF_VALUE, i:1),
    CONF_INFO(135, conf.lang_file,                              CONF_CHAR_PTR,  ptr:""),
    CONF_INFO(136, conf.font_cp,                                CONF_DEF_VALUE, i:FONT_CP_WIN_1252),
    CONF_INFO(137, conf.tbox_char_map,                          CONF_DEF_VALUE, i:0),

    CONF_INFO(140, conf.show_grid_lines,                        CONF_DEF_VALUE, i:1),
    CONF_INFO(141, conf.grid_lines_file,                        CONF_CHAR_PTR,  ptr:"A/CHDK/GRIDS/rulecross.grd"),
    CONF_INFO(142, conf.grid_force_color,                       CONF_DEF_VALUE, i:0),

    CONF_INFO(150, conf.show_dof,                               CONF_DEF_VALUE, i:DOF_DONT_SHOW),
    CONF_INFO(151, conf.dof_subj_dist_as_near_limit,            CONF_DEF_VALUE, i:0),
    CONF_INFO(152, conf.dof_use_exif_subj_dist,                 CONF_DEF_VALUE, i:0),
    CONF_INFO(153, conf.dof_subj_dist_in_misc,                  CONF_DEF_VALUE, i:1),
    CONF_INFO(154, conf.dof_near_limit_in_misc,                 CONF_DEF_VALUE, i:1),
    CONF_INFO(155, conf.dof_far_limit_in_misc,                  CONF_DEF_VALUE, i:1),
    CONF_INFO(156, conf.dof_hyperfocal_in_misc,                 CONF_DEF_VALUE, i:1),
    CONF_INFO(157, conf.dof_depth_in_misc,                      CONF_DEF_VALUE, i:0),

    CONF_INFO(160, conf.show_values,                            CONF_DEF_VALUE, i:0),
    CONF_INFO(161, conf.values_show_in_review,                  CONF_DEF_VALUE, i:0),
    CONF_INFO(162, conf.values_show_zoom,                       CONF_DEF_VALUE, i:1),
    CONF_INFO(163, conf.values_show_real_aperture,              CONF_DEF_VALUE, i:0),
    CONF_INFO(164, conf.values_show_real_iso,                   CONF_DEF_VALUE, i:1),
    CONF_INFO(165, conf.values_show_market_iso,                 CONF_DEF_VALUE, i:0),
    CONF_INFO(166, conf.values_show_iso_only_in_autoiso_mode,   CONF_DEF_VALUE, i:0),
    CONF_INFO(167, conf.values_show_ev_seted,                   CONF_DEF_VALUE, i:0),
    CONF_INFO(168, conf.values_show_ev_measured,                CONF_DEF_VALUE, i:0),
    CONF_INFO(169, conf.values_show_bv_measured,                CONF_DEF_VALUE, i:0),
    CONF_INFO(170, conf.values_show_bv_seted,                   CONF_DEF_VALUE, i:0),
    CONF_INFO(171, conf.values_show_overexposure,               CONF_DEF_VALUE, i:0),
    CONF_INFO(172, conf.values_show_luminance,                  CONF_DEF_VALUE, i:0),
    CONF_INFO(173, conf.values_show_canon_overexposure,         CONF_DEF_VALUE, i:0),
    CONF_INFO(174, conf.show_values_in_video,                   CONF_DEF_VALUE, i:0),
    CONF_INFO(175, conf.zoom_value,                             CONF_DEF_VALUE, i:ZOOM_SHOW_X),
    CONF_INFO(176, conf.zoom_scale,                             CONF_DEF_VALUE, i:100), 
    
    CONF_INFO(180, conf.space_icon_show,                        CONF_DEF_VALUE, i:0),
    CONF_INFO(181, conf.space_perc_show,                        CONF_DEF_VALUE, i:1),
    CONF_INFO(182, conf.space_mb_show,                          CONF_DEF_VALUE, i:1),
    CONF_INFO(183, conf.space_bar_show,                         CONF_DEF_VALUE, i:1), 
    CONF_INFO(184, conf.space_bar_size,                         CONF_DEF_VALUE, i:1), 
    CONF_INFO(185, conf.space_bar_width,                        CONF_DEF_VALUE, i:2), 
    CONF_INFO(186, conf.space_perc_warn,                        CONF_DEF_VALUE, i:10),
    CONF_INFO(187, conf.space_mb_warn,                          CONF_DEF_VALUE, i:20),
    CONF_INFO(188, conf.space_warn_type,                        CONF_DEF_VALUE, i:0),
    CONF_INFO(189, conf.show_partition_nr,                      CONF_DEF_VALUE, i:0),

    CONF_INFO(200, conf.show_remaining_raw,                     CONF_DEF_VALUE, i:1),
    CONF_INFO(201, conf.show_raw_state,                         CONF_DEF_VALUE, i:1),
    CONF_INFO(202, conf.remaining_raw_treshold,                 CONF_DEF_VALUE, i:0),
    CONF_INFO(203, conf.raw_exceptions_warn,                    CONF_DEF_VALUE, i:1),

    CONF_INFO(210, conf.show_movie_time,                        CONF_DEF_VALUE, i:0),
    CONF_INFO(211, conf.show_movie_refresh,                     CONF_DEF_VALUE, i:1),

    CONF_INFO(220, conf.show_temp,                              CONF_DEF_VALUE, i:1),
    CONF_INFO(221, conf.temperature_unit,                       CONF_DEF_VALUE, i:0),

    CONF_INFO(230, conf.debug_shortcut_action,                  CONF_DEF_VALUE, i:0), // backwards compatible
   	CONF_INFO(231, conf.debug_display,                          CONF_DEF_VALUE, i:0),
    CONF_INFO(232, conf.debug_propcase_page,                    CONF_DEF_VALUE, i:0),
	CONF_INFO(233, conf.debug_misc_vals_show,                   CONF_DEF_VALUE, i:0),
    CONF_INFO(234, conf.mem_view_addr_init,                     CONF_DEF_VALUE, i:0x1000),

    // Touch screen U/I overrides
    CONF_INFO(240, conf.touchscreen_disable_video_controls,     CONF_DEF_VALUE, i:0),
    CONF_INFO(241, conf.touchscreen_disable_shortcut_controls,  CONF_DEF_VALUE, i:0),

    CONF_INFO(250, conf.usb_info_enable,                        CONF_DEF_VALUE, i:0),

    CONF_INFO(260, conf.show_alt_helper,                        CONF_DEF_VALUE, i:1),
    CONF_INFO(261, conf.show_alt_helper_delay,                  CONF_DEF_VALUE, i:3),
    CONF_INFO(262, conf.enable_shortcuts,                       CONF_DEF_VALUE, i:1),
    CONF_INFO(263, conf.extra_button,                           CONF_DEF_VALUE, i:0),

    // Console settings
    CONF_INFO(270, conf.console_show,                           CONF_DEF_VALUE, i:0),
    CONF_INFO(271, conf.console_timeout,                        CONF_DEF_VALUE, i:3),
 
    // OSD element scaling factor 
    CONF_INFO2( 280, conf.histo_scale,                             CONF_OSD_POS, 0,0  ),  // 0=no scale, 1=scale  (0 for future expansion)
    CONF_INFO2( 281, conf.dof_scale,                               CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 282, conf.batt_icon_scale,                         CONF_OSD_POS, 0,0  ),
    CONF_INFO2( 283, conf.batt_txt_scale,                          CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 284, conf.mode_state_scale,                        CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 285, conf.values_scale,                            CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 286, conf.clock_scale,                             CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 287, conf.space_icon_scale,                        CONF_OSD_POS, 0,0  ),
    CONF_INFO2( 288, conf.space_txt_scale,                         CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 289, conf.mode_raw_scale,                          CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 290, conf.space_ver_scale,                         CONF_OSD_POS, 0,0  ),
    CONF_INFO2( 291, conf.space_hor_scale,                         CONF_OSD_POS, 0,0  ),   
    CONF_INFO2( 292, conf.mode_video_scale,                        CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 293, conf.mode_ev_scale,                           CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 294, conf.temp_scale,                              CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 295, conf.ev_video_scale,                          CONF_OSD_POS, 1,1  ),
    CONF_INFO2( 296, conf.usb_info_scale,                          CONF_OSD_POS, 0,0  ),

    CONF_INFO( 297, conf.enable_raw_shortcut,                      CONF_DEF_VALUE, i:0),

    CONF_INFO( 298, conf.osd_platformid,                           CONF_DEF_VALUE, i:PLATFORMID),

    {0}
};

void osd_conf_info_func(unsigned short id)
{
    switch (id)
    {
    case 6:
        update_draw_proc();
        break;
    case 131: 
        rbf_load_from_file(conf.menu_rbf_file, FONT_CP_WIN);
        break;
    case 132: 
        if (!rbf_load_symbol(conf.menu_symbol_rbf_file))
            conf.menu_symbol_enable=0;
        break;
    case 135: 
        gui_lang_init(); 
        break;
    case 136:
        font_set(conf.font_cp);
        break;
#if CAM_OPTIONAL_EXTRA_BUTTON
    case 263: 
        kbd_set_extra_button((short)conf.extra_button);
        break;
#endif
    }
}

static ConfInfo conf_info[] = {
/* !!! Do NOT change ID for items defined already! Append a new one at the end! !!! */

    CONF_INFO(  1, conf.platformid,                             CONF_DEF_VALUE, i:PLATFORMID),

    CONF_INFO(  2, conf.fast_image_quality,                     CONF_DEF_VALUE, i:3),

    CONF_INFO(  3, conf.nd_filter_state,                        CONF_DEF_VALUE, i:0),

    CONF_INFO( 20, conf.save_raw,                               CONF_DEF_VALUE, i:0),
    CONF_INFO( 21, conf.raw_in_dir,                             CONF_DEF_VALUE, i:1),
    CONF_INFO( 22, conf.raw_prefix,                             CONF_DEF_VALUE, i:RAW_PREFIX_CRW),
    CONF_INFO( 23, conf.raw_ext,                                CONF_DEF_VALUE, i:DEFAULT_RAW_EXT),
    CONF_INFO( 24, conf.raw_save_first_only,                    CONF_DEF_VALUE, i:0),
    CONF_INFO( 25, conf.save_raw_in_video,                      CONF_DEF_VALUE, i:1),
    CONF_INFO( 26, conf.save_raw_in_sports,                     CONF_DEF_VALUE, i:0),
    CONF_INFO( 27, conf.save_raw_in_burst,                      CONF_DEF_VALUE, i:0),
    CONF_INFO( 28, conf.save_raw_in_ev_bracketing,              CONF_DEF_VALUE, i:0),
    CONF_INFO( 29, conf.save_raw_in_timer,                      CONF_DEF_VALUE, i:0),
    CONF_INFO( 30, conf.raw_cache,                              CONF_DEF_VALUE, i:1),
    CONF_INFO( 31, conf.save_raw_in_edgeoverlay,                CONF_DEF_VALUE, i:0),
    CONF_INFO( 32, conf.save_raw_in_auto,                       CONF_DEF_VALUE, i:0),
    CONF_INFO( 33, conf.bad_pixel_removal,                      CONF_DEF_VALUE, i:0),
    CONF_INFO( 34, conf.raw_timer,                              CONF_DEF_VALUE, i:0),
    CONF_INFO( 35, conf.raw_dng_ext,                            CONF_DEF_VALUE, i:1),
    CONF_INFO( 36, conf.dng_raw,                                CONF_DEF_VALUE, i:1),
    CONF_INFO( 37, conf.dng_version,                            CONF_DEF_VALUE, i:1),
    CONF_INFO( 38, conf.dng_usb_ext,                            CONF_DEF_VALUE, i:0),
    CONF_INFO( 39, conf.raw_nr,                                 CONF_DEF_VALUE, i:NOISE_REDUCTION_AUTO_CANON),
    CONF_INFO( 40, conf.bracketing_add_raw_suffix,              CONF_DEF_VALUE, i:0),			
    CONF_INFO( 41, conf.sub_batch_prefix,                       CONF_DEF_VALUE, i:RAW_PREFIX_SND), // SND_
    CONF_INFO( 42, conf.sub_batch_ext,                          CONF_DEF_VALUE, i:DEFAULT_RAW_EXT), // .CRW
    CONF_INFO( 43, conf.dng_crop_size,                          CONF_DEF_VALUE, i:0),

    CONF_INFO( 50, conf.script_shoot_delay,                     CONF_DEF_VALUE, i:0),
    CONF_INFO( 52, conf.script_param_set,                       CONF_DEF_VALUE, i:0),
    CONF_INFO( 53, conf.script_startup,                         CONF_DEF_VALUE, i:SCRIPT_AUTOSTART_NONE),
    CONF_INFO( 54, conf.script_file,                            CONF_CHAR_PTR,  ptr:""),
    CONF_INFO( 55, conf.script_param_save,                      CONF_DEF_VALUE, i:1),

#ifdef CAM_PERSISTENT_OSD
    // On by default: the overlay draws its own histogram at the top left, and
    // the point of putting it there was to have it visible without hunting
    // through a menu for it.
    CONF_INFO( 60, conf.show_histo,                             CONF_DEF_VALUE, i:1),
#else
    CONF_INFO( 60, conf.show_histo,                             CONF_DEF_VALUE, i:0),
#endif
    CONF_INFO( 61, conf.show_overexp,                           CONF_DEF_VALUE, i:1),
    CONF_INFO( 62, conf.histo_mode,                             CONF_DEF_VALUE, i:0),
    CONF_INFO( 63, conf.histo_auto_ajust,                       CONF_DEF_VALUE, i:1),
    CONF_INFO( 64, conf.histo_ignore_boundary,                  CONF_DEF_VALUE, i:4),
    CONF_INFO( 65, conf.histo_layout,                           CONF_DEF_VALUE, i:0),
    CONF_INFO( 66, conf.histo_show_ev_grid,                     CONF_DEF_VALUE, i:0),

    CONF_INFO( 70, conf.zebra_draw,                             CONF_DEF_VALUE, i:1),
    CONF_INFO( 71, conf.zebra_mode,                             CONF_DEF_VALUE, i:ZEBRA_MODE_BLINKED_2),
    CONF_INFO( 72, conf.zebra_restore_screen,                   CONF_DEF_VALUE, i:1),
    CONF_INFO( 73, conf.zebra_restore_osd,                      CONF_DEF_VALUE, i:1),
    CONF_INFO( 74, conf.zebra_over,                             CONF_DEF_VALUE, i:1),
    CONF_INFO( 75, conf.zebra_under,                            CONF_DEF_VALUE, i:0),
    CONF_INFO( 76, conf.zebra_draw_osd,                         CONF_DEF_VALUE, i:ZEBRA_DRAW_HISTO),
    CONF_INFO( 77, conf.zebra_multichannel,                     CONF_DEF_VALUE, i:0),

    CONF_INFO( 80, conf.video_mode,                             CONF_DEF_VALUE, i:0),
    CONF_INFO( 81, conf.video_quality,                          CONF_DEF_VALUE, i:VIDEO_DEFAULT_QUALITY),
    CONF_INFO( 82, conf.video_bitrate,                          CONF_DEF_VALUE, i:VIDEO_DEFAULT_BITRATE),
    CONF_INFO( 83, conf.clear_video,                            CONF_DEF_VALUE, i:0),
    CONF_INFO( 84, conf.mute_on_zoom,                           CONF_DEF_VALUE, i:0), 
    CONF_INFO( 85, conf.video_af_key,                           CONF_DEF_VALUE, i:0),
    CONF_INFO( 86, conf.unlock_optical_zoom_for_video,          CONF_DEF_VALUE, i:1), 
    CONF_INFO( 87, conf.fast_ev,                                CONF_DEF_VALUE, i:0),
    CONF_INFO( 88, conf.fast_ev_step,                           CONF_DEF_VALUE, i:1),
    CONF_INFO( 89, conf.fast_movie_control,                     CONF_DEF_VALUE, i:0),
    CONF_INFO( 90, conf.fast_movie_quality_control,             CONF_DEF_VALUE, i:0),
    CONF_INFO( 91, conf.ext_video_time,                         CONF_DEF_VALUE, i:0),
    CONF_INFO( 92, conf.clean_overlay,                          CONF_DEF_VALUE, i:0),
    CONF_INFO( 93, conf.unlock_av_out_in_rec,                   CONF_DEF_VALUE, i:0),

    CONF_INFO(100, conf.clear_override,                         CONF_DEF_VALUE, i:1),
    
    CONF_INFO(110, conf.tv_override_enabled,                    CONF_DEF_VALUE, i:0),
    CONF_INFO(111, conf.tv_enum_type,                           CONF_DEF_VALUE, i:TV_OVERRIDE_EV_STEP),
    CONF_INFO(112, conf.tv_override_value,                      CONF_DEF_VALUE, i:0),
    CONF_INFO(113, conf.tv_override_long_exp,                   CONF_DEF_VALUE, i:0),
    CONF_INFO(114, conf.tv_override_short_exp,                  CONF_DEF_VALUE, i:0),

    CONF_INFO(120, conf.av_override_enabled,                    CONF_DEF_VALUE, i:0),
    CONF_INFO(121, conf.av_override_value,                      CONF_DEF_VALUE, i:0),

    CONF_INFO(130, conf.iso_override_value,                     CONF_DEF_VALUE, i:0),
    CONF_INFO(131, conf.iso_override_koef,                      CONF_DEF_VALUE, i:0),
    
    CONF_INFO(140, conf.subj_dist_override_value,               CONF_DEF_VALUE, i:0),
    CONF_INFO(141, conf.subj_dist_override_koef,                CONF_DEF_VALUE, i:SD_OVERRIDE_OFF),
    
    CONF_INFO(150, conf.tv_bracket_value,                       CONF_DEF_VALUE, i:0),
    CONF_INFO(151, conf.av_bracket_value,                       CONF_DEF_VALUE, i:0),
    CONF_INFO(152, conf.iso_bracket_value,                      CONF_DEF_VALUE, i:0),
    CONF_INFO(153, conf.iso_bracket_koef,                       CONF_DEF_VALUE, i:0),
    CONF_INFO(154, conf.subj_dist_bracket_value,                CONF_DEF_VALUE, i:0),
    CONF_INFO(155, conf.subj_dist_bracket_koef,                 CONF_DEF_VALUE, i:0),
    CONF_INFO(156, conf.bracket_type,                           CONF_DEF_VALUE, i:0),
    CONF_INFO(157, conf.clear_bracket,                          CONF_DEF_VALUE, i:1),
    
    CONF_INFO(160, conf.autoiso_enable,                         CONF_DEF_VALUE, i:0),
    CONF_INFO(161, conf.autoiso_shutter_enum,                   CONF_DEF_VALUE, i:5), // 5='1/125'
    CONF_INFO(162, conf.autoiso_user_factor,                    CONF_DEF_VALUE, i:5),
    CONF_INFO(163, conf.autoiso_is_factor,                      CONF_DEF_VALUE, i:2),
    CONF_INFO(164, conf.autoiso_max_iso_hi,                     CONF_DEF_VALUE, i:550),
    CONF_INFO(165, conf.autoiso_max_iso_auto,                   CONF_DEF_VALUE, i:320),
    CONF_INFO(166, conf.autoiso_min_iso,                        CONF_DEF_VALUE, i:50),

    // AutoISO2
    CONF_INFO(170, conf.autoiso2_shutter_enum,                  CONF_DEF_VALUE, i:0), // 0="off" 6='1/20'
    CONF_INFO(171, conf.autoiso2_max_iso_auto,                  CONF_DEF_VALUE, i:600),
    CONF_INFO(172, conf.autoiso2_over,                          CONF_DEF_VALUE, i:1),
    CONF_INFO(173, conf.overexp_threshold,                      CONF_DEF_VALUE, i:5),
    CONF_INFO(174, conf.overexp_ev_enum,                        CONF_DEF_VALUE, i:0), // 0="off"

    CONF_INFO(180, conf.curve_file,                             CONF_CHAR_PTR,  ptr:""),
    CONF_INFO(181, conf.curve_enable,                           CONF_DEF_VALUE, i:0),

    CONF_INFO(190, conf.edge_overlay_enable,                    CONF_DEF_VALUE, i:0),
    CONF_INFO(191, conf.edge_overlay_thresh,                    CONF_DEF_VALUE, i:60),
    CONF_INFO(192, conf.edge_overlay_play,                      CONF_DEF_VALUE, i:0),
    CONF_INFO(193, conf.edge_overlay_pano,                      CONF_DEF_VALUE, i:0),
    CONF_INFO(194, conf.edge_overlay_zoom,                      CONF_DEF_VALUE, i:1),
	CONF_INFO(195, conf.edge_overlay_filter,                    CONF_DEF_VALUE, i:0),
	CONF_INFO(196, conf.edge_overlay_show,                      CONF_DEF_VALUE, i:0),
    CONF_INFO(197, conf.edge_overlay_pano_overlap,              CONF_DEF_VALUE, i:30),
    
    CONF_INFO(200, conf.remote_enable,                          CONF_DEF_VALUE, i:0),
    CONF_INFO(201, conf.synch_enable,                           CONF_DEF_VALUE, i:0),
    CONF_INFO(202, conf.synch_delay_enable,                     CONF_DEF_VALUE, i:0),
    CONF_INFO(203, conf.synch_delay_value,                      CONF_DEF_VALUE, i:100),
    CONF_INFO(204, conf.remote_switch_type,                     CONF_DEF_VALUE, i:0),
    CONF_INFO(205, conf.remote_control_mode,                    CONF_DEF_VALUE, i:0),
    CONF_INFO(206, conf.remote_enable_scripts,                  CONF_DEF_VALUE, i:0),
    CONF_INFO(207, conf.ricoh_ca1_mode,                         CONF_DEF_VALUE, i:0),
    CONF_INFO(208, conf.remote_input_channel,                   CONF_DEF_VALUE, i:0),

    CONF_INFO(210, conf.flash_sync_curtain,                     CONF_DEF_VALUE, i:0),
    CONF_INFO(211, conf.flash_video_override,                   CONF_DEF_VALUE, i:0),
    CONF_INFO(212, conf.flash_video_override_power,             CONF_DEF_VALUE, i:0),
    CONF_INFO(213, conf.flash_manual_override,                  CONF_DEF_VALUE, i:0),
    CONF_INFO(214, conf.flash_exp_comp,                         CONF_DEF_VALUE, i:9),
    CONF_INFO(215, conf.flash_enable_exp_comp,                  CONF_DEF_VALUE, i:0),
    CONF_INFO(216, conf.hotshoe_override,                       CONF_DEF_VALUE, i:0),

    CONF_INFO(220, conf.alt_mode_button,                        CONF_DEF_VALUE, i:KEY_PRINT),
    CONF_INFO(221, conf.alt_prevent_shutdown,                   CONF_DEF_VALUE, i:ALT_PREVENT_SHUTDOWN_ALT),
    CONF_INFO(222, conf.use_zoom_mf,                            CONF_DEF_VALUE, i:0),
    CONF_INFO(223, conf.zoom_assist_button_disable,             CONF_DEF_VALUE, i:0),
    CONF_INFO(224, conf.debug_lua_restart_on_error,             CONF_DEF_VALUE, i:0),
    CONF_INFO(225, conf.module_logging,                         CONF_DEF_VALUE, i:0),
    CONF_INFO(226, conf.flashlight,                             CONF_DEF_VALUE, i:0),

    CONF_INFO(230, conf.memdmp_start,                           CONF_DEF_VALUE, i:0),
    CONF_INFO(231, conf.memdmp_size,                            CONF_DEF_VALUE, i:0),

    CONF_INFO(240, conf.allow_unsafe_io,                        CONF_DEF_VALUE, i:0),

    CONF_INFO(250, conf.disable_lfn_parser_ui,                  CONF_DEF_VALUE, i:0),

    CONF_INFO(251, conf.save_raw_in_canon_raw,                  CONF_DEF_VALUE, i:1),

    CONF_INFO(252, conf.check_firmware_crc,                     CONF_DEF_VALUE, i:1),

    CONF_INFO(260, conf.raw_ev_histo_enable,                    CONF_DEF_VALUE, i:0),
    CONF_INFO(261, conf.raw_ev_histo_under_threshold,           CONF_DEF_VALUE, i:1),
    CONF_INFO(262, conf.raw_ev_histo_over_threshold,            CONF_DEF_VALUE, i:1),
    CONF_INFO(263, conf.raw_ev_histo_width,                     CONF_DEF_VALUE, i:80),
    CONF_INFO(264, conf.raw_ev_histo_height,                    CONF_DEF_VALUE, i:80),
    CONF_INFO(265, conf.raw_ev_histo_xstep,                     CONF_DEF_VALUE, i:16),
    CONF_INFO(266, conf.raw_ev_histo_ystep,                     CONF_DEF_VALUE, i:16),
    CONF_INFO(267, conf.raw_ev_histo_save_log,                  CONF_DEF_VALUE, i:0),
    CONF_INFO(268, conf.raw_ev_histo_ettr_pct,                  CONF_DEF_VALUE, i:1),  // .1% (Decimal * 10)
    CONF_INFO(269, conf.raw_ev_histo_save_for_image,            CONF_DEF_VALUE, i:0),

    // Bit bending (raw.c). IDs from 280 to leave room above the upstream range.
    CONF_INFO(280, conf.bitbend_enable,                         CONF_DEF_VALUE, i:0),
    // 287 was bitbend_live. The live preview is gone - see raw.c - and the ID
    // is left retired rather than reused, so a config written by a build that
    // still had it loads without the old value landing in a new field.
    // Defaults on. The non-data buses are the interesting half of the model,
    // but they cost a per-pixel loop over the whole sensor, and a camera that
    // takes the better part of a minute per frame out of the box reads as
    // broken rather than as slow.
    CONF_INFO(289, conf.bitbend_simple,                         CONF_DEF_VALUE, i:1),
    // The matrix is stored as a blob. Its default from config_load_defaults is
    // all zeroes, which is not the identity bend - bend_sanitize() catches that
    // and resets it, which is the same path that protects against a preset file
    // written by a camera with a different sensor depth.
    CONF_INFO(288, conf.bitbend,                                CONF_INT_PTR,   i:0),

    // Experimental profiles (raw.c, bendx.c). Same shape as the matrix above
    // and for the same reasons: a switch as a plain int, the profile as a
    // blob whose all-zero default is BX_OFF and therefore already correct.
    // 291 was a single bendx_t, before profiles could stack. The blob at that
    // ID is twelve bytes and this one is fifty-two, and CONF_INT_PTR restores
    // by size - so the ID is retired rather than reused, the same way 287 was.
    // A card carrying the older config loads with an empty chain, which is the
    // right answer: the one profile it held was not applied until it was
    // picked either.
    // Registered unconditionally, matching the struct - see the note there.
    CONF_INFO(290, conf.bendx_enable,                           CONF_DEF_VALUE, i:0),
    CONF_INFO(293, conf.bend_shot_save,                         CONF_DEF_VALUE, i:1),

    // Segments (raw.c, bend.c). A blob again, and its all-zero default is the
    // layout switched off - which is the one default that cannot be wrong,
    // because it is the camera that was there before segments existed. The
    // zero radius that comes with it is not usable and bend_seg_sanitize()
    // turns it into the real default; see the note there for why that is not
    // done with a CONF_DEF_VALUE.
    //
    // 294 was this without the per-region preset numbers, four bytes shorter.
    // CONF_INT_PTR copies min(stored, current) bytes into the front of the
    // struct, so a card holding the older blob does not fail to load - it
    // loads shifted, and every region matrix lands four bytes out. The
    // symptom is regions that will not hold a bend, which looks like a bug in
    // the segments rather than in the config, so the ID is retired the same
    // way 287 and 291 were rather than reused.
    CONF_INFO(295, conf.bend_segs,                              CONF_INT_PTR,   i:0),

    // Multiple exposure (raw.c, mexp.c). Plain ints, all five, and the
    // defaults are the double exposure the feature was asked for: two frames,
    // averaged, ghost on. Off until switched on, like both bend engines -
    // every exposure of a sequence costs a raw-sized card read, and a camera
    // that does that out of the box reads as broken rather than as busy.
    CONF_INFO(300, conf.mexp_enable,                            CONF_DEF_VALUE, i:0),
    CONF_INFO(301, conf.mexp_mode,                              CONF_DEF_VALUE, i:3),  // MEXP_AVERAGE
    CONF_INFO(302, conf.mexp_frames,                            CONF_DEF_VALUE, i:2),
    CONF_INFO(303, conf.mexp_ghost,                             CONF_DEF_VALUE, i:1),

    // Game Boy screen tint (modules/gbc). 0 is the DMG green it always had.
    CONF_INFO(306, conf.gbc_hue,                                CONF_DEF_VALUE, i:0),

    // The segment mask overlay (gui_bend.c). Off by default: it is a thing you
    // switch on to see where the regions are and switch off to frame with.
    CONF_INFO(305, conf.bend_seg_mask,                          CONF_DEF_VALUE, i:0),

    // Theme (theme.c). Defaults to Apple, which is the assignment the fork
    // already drew, so an existing card looks unchanged until this is touched.
    CONF_INFO(304, conf.theme,                                  CONF_DEF_VALUE, i:0),

    // Kept last because gen_conf_lua uses the final registered ID as its
    // range limit, while the older fork settings above are grouped by feature
    // rather than ID.
    CONF_INFO(307, conf.mexp_bend_each,                         CONF_DEF_VALUE, i:1),

    // 292 held effect numbers from the larger experimental list. Five entries
    // were removed and the survivors compacted, so loading that blob would
    // silently turn saved effects into different ones. Retire it and start the
    // new list empty. Kept last because gen_conf_lua uses the final ID.
    CONF_INFO(308, conf.bendx,                                  CONF_INT_PTR,   i:0),

    CONF_INFO(999, conf.script_allow_lua_native_calls,          CONF_DEF_VALUE, i:0),

    {0}
};

void conf_info_func(unsigned short id)
{
    switch (id)
    {
    case  20:
    case  36: 
    case  37: 
        conf_change_dng(); 
        break;
#if defined (DNG_EXT_FROM)
    case  38: 
        {
        extern void save_ext_for_dng(void);
        save_ext_for_dng();
        cb_change_dng_usb_ext();
        }
        break;
#endif 
    case  54:
        script_load(conf.script_file);
        break;
#ifdef CAM_MOVIEREC_NEWSTYLE
    case  81:
        shooting_video_minbitrate_change(conf.video_quality);
        break;
#endif
    case  82: 
        shooting_video_bitrate_change(conf.video_bitrate);
        break;
    case 200:
    case 204:
    case 205:   // USB Remote
    case 208:
        set_usb_remote_state();
        break;
    case 220:
        reset_alt_button();
        break;
    case 221: 
        conf_update_prevent_shutdown(); 
        break;
    case 260:
        librawevhisto->load(conf.raw_ev_histo_enable);
        break;
    }
}

#ifdef CAM_HAS_GPS
static ConfInfo gps_conf_info[] = {
    // GPS
    CONF_INFO(  1, conf.gps_record,                 CONF_DEF_VALUE,     i:0),
    CONF_INFO(  2, conf.gps_navi_show,              CONF_DEF_VALUE,     i:0),
    CONF_INFO(  3, conf.gps_compass_show,           CONF_DEF_VALUE,     i:0),
    CONF_INFO(  4, conf.gps_coordinates_show,       CONF_DEF_VALUE,     i:0),
    CONF_INFO(  5, conf.gps_height_show,            CONF_DEF_VALUE,     i:0),
    CONF_INFO(  6, conf.gps_waypoint_save,          CONF_DEF_VALUE,     i:0),
    CONF_INFO(  7, conf.gps_track_time,             CONF_DEF_VALUE,     i:1),
    CONF_INFO(  8, conf.gps_compass_hide,           CONF_DEF_VALUE,     i:0),

    CONF_INFO(  9, conf.gps_wait_for_signal,        CONF_DEF_VALUE,     i:300),
    CONF_INFO( 10, conf.gps_compass_time,           CONF_DEF_VALUE,     i:1),
    CONF_INFO( 11, conf.gps_navi_time,              CONF_DEF_VALUE,     i:1),
    CONF_INFO( 12, conf.gps_wait_for_signal_time,   CONF_DEF_VALUE,     i:5),
    CONF_INFO( 13, conf.gps_compass_smooth,         CONF_DEF_VALUE,     i:7),
    CONF_INFO( 14, conf.gps_batt,                   CONF_DEF_VALUE,     i:25),
    CONF_INFO( 15, conf.gps_countdown,              CONF_DEF_VALUE,     i:0),
    CONF_INFO( 16, conf.gps_2D_3D_fix,              CONF_DEF_VALUE,     i:2),
    CONF_INFO( 17, conf.gps_countdown_blink,        CONF_DEF_VALUE,     i:1),
    
    CONF_INFO( 18, conf.gps_rec_play_set,           CONF_DEF_VALUE,     i:0),
    CONF_INFO( 19, conf.gps_play_dark_set,          CONF_DEF_VALUE,     i:0),
    CONF_INFO( 20, conf.gps_rec_play_time,          CONF_DEF_VALUE,     i:30),
    CONF_INFO( 21, conf.gps_play_dark_time,         CONF_DEF_VALUE,     i:45),

    CONF_INFO( 22, conf.gps_rec_play_set_1,         CONF_DEF_VALUE,     i:0),
    CONF_INFO( 23, conf.gps_play_dark_set_1,        CONF_DEF_VALUE,     i:0),
    CONF_INFO( 24, conf.gps_rec_play_time_1,        CONF_DEF_VALUE,     i:10),
    CONF_INFO( 25, conf.gps_play_dark_time_1,       CONF_DEF_VALUE,     i:15),
    CONF_INFO( 26, conf.gps_show_symbol,            CONF_DEF_VALUE,     i:0),
    CONF_INFO( 27, conf.gps_batt_warn,              CONF_DEF_VALUE,     i:0),
    CONF_INFO( 28, conf.gps_track_symbol,           CONF_DEF_VALUE,     i:0),
    CONF_INFO( 29, conf.gps_test_timezone,          CONF_DEF_VALUE,     i:0),
    CONF_INFO( 30, conf.gps_beep_warn,              CONF_DEF_VALUE,     i:0),
    CONF_INFO( 31, conf.gps_on_off,                 CONF_DEF_VALUE,     i:0),

    {0}
};
#endif

//-------------------------------------------------------------------
// ID offsets for ConfInfo mapping from Lua libraries
#define CONF_CORE   1000
#define CONF_OSD    2000
#define CONF_USER   3000
#define CONF_GPS    4000

#define CONF_FILE           "A/CHDK/CCHDK4.CFG"
#define OSD_CONF_FILE       "A/CHDK/OSD__4.CFG"
#define USER_MENU_CONF_FILE "A/CHDK/UMENU4.CFG"
#define GPS_CONF_FILE       "A/CHDK/GPS__4.CFG"

static const confinfo_handler confinfo_handlers[] = {
    { CONF_CORE, CONF_CORE+999, conf_info,           CONF_FILE,           conf_info_func },
    { CONF_OSD,  CONF_OSD +999, osd_conf_info,       OSD_CONF_FILE,       osd_conf_info_func },
    { CONF_USER, CONF_USER+999, user_menu_conf_info, USER_MENU_CONF_FILE, user_menu_conf_info_func },
#ifdef CAM_HAS_GPS
    { CONF_GPS,  CONF_GPS +999, gps_conf_info,       GPS_CONF_FILE,       0 },
#endif
    { 0,0,0,0,0 }
};

//-------------------------------------------------------------------
// make hash of memory block
unsigned int memhash31(unsigned char *mem, int len)
{
	unsigned hash=0;
	for (; len > 0; mem++, len-- )
		hash = *mem ^ (hash<<6) ^ (hash>>25);
	return hash;
}

static void config_update_last_saved(ConfInfo *ci)
{
    int i;
    for (i=0; ci[i].id > 0; ++i)
    {
        switch (ci[i].type)
        {
            case CONF_OSD_POS:
            case CONF_DEF_VALUE:
            case CONF_FUNC_PTR:
                switch (ci[i].size)
                {
                    case sizeof(int):
                        ci[i].last_saved = (unsigned int)*((int*)ci[i].var);
                        break;
                    case sizeof(short):
                        ci[i].last_saved = (unsigned int)*((short*)ci[i].var);
                        break;
                    case sizeof(char):
                        ci[i].last_saved = (unsigned int)*((char*)ci[i].var);
                        break;
                }
                break;
            case CONF_INT_PTR:
                ci[i].last_saved = memhash31((unsigned char*)ci[i].var, ci[i].size);
                break;
            case CONF_CHAR_PTR:
                ci[i].last_saved = lang_strhash31((int)ci[i].var);
                break;
        }
    }
}

static int config_has_changed(ConfInfo *ci)
{
    int i;
    for (i=0; ci[i].id > 0; ++i)
    {
        switch (ci[i].type)
        {
            case CONF_OSD_POS:
            case CONF_DEF_VALUE:
            case CONF_FUNC_PTR:
                switch (ci[i].size)
                {
                    case sizeof(int):
                        if (ci[i].last_saved != (unsigned int)*((int*)ci[i].var))
                            return 1;
                        break;
                    case sizeof(short):
                        if (ci[i].last_saved != (unsigned int)*((short*)ci[i].var))
                            return 1;
                        break;
                    case sizeof(char):
                        if (ci[i].last_saved != (unsigned int)*((char*)ci[i].var))
                            return 1;
                        break;
                }
                break;
            case CONF_INT_PTR:
                if (ci[i].last_saved != memhash31((unsigned char*)ci[i].var, ci[i].size))
                    return 1;
                break;
            case CONF_CHAR_PTR:
                if (ci[i].last_saved != lang_strhash31((int)ci[i].var))
                    return 1;
                break;
        }
    }

    return 0;
}

//-------------------------------------------------------------------
static void config_load_defaults(ConfInfo *ci, void (*info_func)(unsigned short id))
{
    register int i;

    for (i=0; ci[i].id > 0; ++i) {
        switch (ci[i].type) {
            case CONF_OSD_POS:
            case CONF_DEF_VALUE:
                memcpy(ci[i].var, &(ci[i].i), ci[i].size);
                break;
            case CONF_INT_PTR:
                memset(ci[i].var, 0, ci[i].size);
                break;
            case CONF_FUNC_PTR:
                *((int*)ci[i].var) = ci[i].func();
                break;
            case CONF_CHAR_PTR:
                memcpy(ci[i].var, ci[i].ptr, ci[i].size);
                break;
        }
        if (info_func) info_func(ci[i].id);
    }
}

void conf_load_defaults()
{
    int i;
    for (i=0; confinfo_handlers[i].ci != 0; i++)
        config_load_defaults(confinfo_handlers[i].ci, confinfo_handlers[i].info_func);
}

//-------------------------------------------------------------------

// Structure for saved conf info (allows for changes to ConfInfo without affecting saved configurations
typedef struct
{
    unsigned short id;
    unsigned short size;
} ConfInfoSave;

void config_save(ConfInfo *ci, const char *filename, int config_base)
{
// if Lua native calls are forced on, don't save state to config file since user did not select it
#if defined(OPT_FORCE_LUA_CALL_NATIVE)
    conf.script_allow_lua_native_calls = 0;
#endif

    register int i;
    
    for (i=0; ci[i].id > 0; i++);   // Find # of items

    int size = 2 * sizeof(int) + i * sizeof(ConfInfoSave);

    for (i=0; ci[i].id > 0; i++)
    {
        if (ci[i].type == CONF_STRUCT_PTR)
        {
            tVarArrayConfig *cfg = (tVarArrayConfig*)(ci[i].var);
            size += cfg->saved_size();
        }
        else
        {
            size += ci[i].size;
        }
    }

    char *buf = umalloc(size);
    char *p = buf;

    int fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0777); 
    if (fd >= 0)
    {
        int t = CONF_MAGICK_VALUE;
        memcpy(p, &t, sizeof(t));
        p += sizeof(t);

        t = config_base;
        memcpy(p, &t, sizeof(t));
        p += sizeof(t);

        for (i=0; ci[i].id > 0; ++i)
        {
            ((ConfInfoSave*)p)->id   = ci[i].id;
            ((ConfInfoSave*)p)->size = ci[i].size;
            p += sizeof(ConfInfoSave);
            if (ci[i].type == CONF_STRUCT_PTR)
            {
                tVarArrayConfig *cfg = (tVarArrayConfig*)(ci[i].var);
                p = cfg->save(p);
            }
            else
            {
                memcpy(p, ci[i].var, ci[i].size);
                // Clear out unused space after string config item value
                if ((ci[i].size == CONF_STR_LEN) && (strlen(ci[i].var) < CONF_STR_LEN))
                    memset(p+strlen(ci[i].var), 0, CONF_STR_LEN-strlen(ci[i].var));
                p += ci[i].size;
            }
        }

        write(fd, buf, p-buf);
        close(fd);
    }
    ufree(buf);

// if Lua native calls are forced on, re-enable native calls
#if defined(OPT_FORCE_LUA_CALL_NATIVE)
    conf.script_allow_lua_native_calls = 1;
#endif
}

void conf_save()
{
    int i;
    for (i=0; confinfo_handlers[i].ci != 0; i++)
    {
        if (config_has_changed(confinfo_handlers[i].ci) || ((confinfo_handlers[i].start_id == CONF_USER) && camera_info.state.user_menu_has_changed))
        {
            if (confinfo_handlers[i].start_id == CONF_USER)
        		user_menu_save();

            config_save(confinfo_handlers[i].ci, confinfo_handlers[i].filename, confinfo_handlers[i].start_id);
            config_update_last_saved(confinfo_handlers[i].ci);
        }
    }
    // Save current script parameters if they have changed.
    save_params_values(0);
}

int save_config_file(int config_base, const char *filename)
{
    int i;
    for (i=0; confinfo_handlers[i].ci != 0; i++)
    {
        if ((config_base >= confinfo_handlers[i].start_id) && (config_base <= confinfo_handlers[i].end_id))
        {
            config_save(confinfo_handlers[i].ci, (filename != 0) ? filename : confinfo_handlers[i].filename, confinfo_handlers[i].start_id);
            if (filename == 0)  // Reset 'last_saved' values after saving default file
                config_update_last_saved(confinfo_handlers[i].ci);
            return 1;
        }
    }
    return 0;
}

//-------------------------------------------------------------------
void config_restore(ConfInfo *ci, const char *filename, void (*info_func)(unsigned short id))
{
    int rcnt, i, offs;
    unsigned short id, size;

    char* buf = load_file(filename, &rcnt, 0);

    if (buf)
    {
        if (*(int*)buf == CONF_MAGICK_VALUE)
        {
            offs = 2 * sizeof(int);
            while (1)
            {
                if (offs + (int)sizeof(short) > rcnt)
                    break;
                id = *((short*)(buf + offs));
                offs += sizeof(short);

                if (offs + (int)sizeof(short) > rcnt)
                    break;
                size = *((short*)(buf + offs));
                offs += sizeof(short);

                for (i=0; ci[i].id > 0; ++i)
                {
                    if (ci[i].id==id && ci[i].size==size)
                    {
                        if (offs + size <= rcnt)
                        {
                            if (ci[i].type == CONF_STRUCT_PTR)
                            {
                                tVarArrayConfig *cfg = (tVarArrayConfig*)(ci[i].var);
                                size = cfg->load(buf+offs);
                            }
                            else if (ci[i].size == size)    // only restore if size matches
                            {
                                memcpy(ci[i].var, buf+offs, size);
                            }
                            if (info_func) info_func(ci[i].id);
                        }
                        break;
                    }
                }

                offs += size;
            }
        }
        free(buf);
    }
}

void conf_restore()
{
    int i;
    int old_config_loaded = 0;

    // init some defaults values
    init_user_menu(USER_MENU_ITEMS);

    // Load all defaults first, in case we end up loading old file
    conf_load_defaults();

    // Check if CORE config file exists, if not try loading old (1.2) file
    if (stat(confinfo_handlers[0].filename,0) != 0)
        old_config_loaded = libconfmigrate->load_old_config(confinfo_handlers);

    // Load all new files (that exist)
    for (i=0; confinfo_handlers[i].ci != 0; i++)
    {
        config_restore(confinfo_handlers[i].ci, confinfo_handlers[i].filename, confinfo_handlers[i].info_func);
        if (old_config_loaded == 0)
            config_update_last_saved(confinfo_handlers[i].ci);
    }

    // clear any "clear on restart" values
    clear_values();

// Enable Lua native calls if builder wants them forced on
#if defined(OPT_FORCE_LUA_CALL_NATIVE)
    conf.script_allow_lua_native_calls = 1;
#endif

// If curves not compiled in force option off
#if !defined(OPT_CURVES)
    conf.curve_enable = 0;
#endif

    // Per-shot .BND sidecars were retired in favour of the recipe embedded in
    // the JPEG. Cards which stored the old enum value should keep recording,
    // not index past the shorter Off/In JPEG menu or silently stop tagging.
    if (conf.bend_shot_save == BEND_SHOT_SIDECAR)
        conf.bend_shot_save = BEND_SHOT_IN_JPEG;
}

int load_config_file(int config_base, const char *filename)
{
    int i;
    for (i=0; confinfo_handlers[i].ci != 0; i++)
    {
        if ((config_base >= confinfo_handlers[i].start_id) && (config_base <= confinfo_handlers[i].end_id))
        {
            config_restore(confinfo_handlers[i].ci, (filename != 0) ? filename : confinfo_handlers[i].filename, confinfo_handlers[i].info_func);
            if (filename == 0)  // Reset 'last_saved' values if loading default file
                config_update_last_saved(confinfo_handlers[i].ci);
            return 1;
        }
    }
    return 0;
}

//-------------------------------------------------------------------
// Find ConfInfo entry index for given ID
static int findConfInfo(ConfInfo *ci, unsigned short id)
{
    int i;
    for( i=0; ci[i].id > 0; ++i )
        if( ci[i].id==id )
            return i;
    return -1;
}

// Reset all user adjustable color settings to default values
void resetColors()
{
    int i, n;

    // Iterate over color override ID's
    for (n=COLOR_FIRST_OVERRIDE; n<=COLOR_LAST_OVERRIDE; n++)
    {
        i = findConfInfo(osd_conf_info, n);
        if (i != -1)
        {
            *((confColor*)osd_conf_info[i].var) = osd_conf_info[i].cl;
        }
    }
}

//-------------------------------------------------------------------
static int getValue(ConfInfo *ci, unsigned short id, tConfigVal* configVal)
{
    int i;
    int ret = CONF_EMPTY;
    OSD_pos* pos;
    
    if (id == 0)
    {
        configVal->numb = 0;
        for ( i=0; ci[i].id > 0; ++i )
            if ( configVal->numb<ci[i].id )
                configVal->numb = ci[i].id;
        ret = CONF_VALUE;
    }
    else
    {
        i = findConfInfo(ci, id);
        if (i != -1)
        {
            switch( ci[i].type )
            {
                case CONF_VALUE:
                case CONF_FUNC_PTR:
                    switch( ci[i].size )
                    {
                        case sizeof(int):
                            configVal->numb = *(int*)ci[i].var;
                            ret = CONF_VALUE;
                            break;
                        case sizeof(short):
                            configVal->numb = *(short*)ci[i].var;
                            ret = CONF_VALUE;
                            break;
                        case sizeof(char):
                            configVal->numb = *(char*)ci[i].var;
                            ret = CONF_VALUE;
                            break;
                    }
                    configVal->pInt = (int*)ci[i].var;
                    break;
                case CONF_INT_PTR:
                    configVal->numb = ci[i].size/sizeof(int);
                    configVal->pInt = (int*)ci[i].var;
                    ret = CONF_INT_PTR;
                    break;
                case CONF_CHAR_PTR:
                    configVal->str = ci[i].var;
                    ret = CONF_CHAR_PTR;
                    break;
                case CONF_OSD_POS:
                    pos = (OSD_pos*)ci[i].var;
                    configVal->pos.x = pos->x;
                    configVal->pos.y = pos->y;
                    ret = CONF_OSD_POS;
                    configVal->pInt = (int*)ci[i].var;
                    break;
            }
        }
    }
    return ret;
}

int conf_getValue(unsigned short id, tConfigVal* configVal)
{
    int i;

    if (id < CONF_CORE)
        id = libconfmigrate->map_old_id(id);  // Convert old ID to new ID

    for (i=0; confinfo_handlers[i].ci != 0; i++)
        if ((id >= confinfo_handlers[i].start_id) && (id <= confinfo_handlers[i].end_id))
            return getValue(confinfo_handlers[i].ci, id - confinfo_handlers[i].start_id, configVal);

    return CONF_EMPTY;
}

//-------------------------------------------------------------------
static int config_autosave = 1;

static int setValue(ConfInfo *ci, unsigned short id, tConfigVal configVal, void (*info_func)(unsigned short id))
{
    int i;
    int ret = CONF_EMPTY, len, len2;
    OSD_pos* pos;
    
    // Don't allow scripts to enable Lua native calls.
    if (id == 999) return ret;

    i = findConfInfo(ci, id);
    if (i != -1)
    {
        switch( ci[i].type )
        {
            case CONF_VALUE:
            case CONF_FUNC_PTR:
                if( configVal.isNumb )
                {
                    switch( ci[i].size )
                    {
                        case sizeof(int):
                            *(int*)ci[i].var = (int)configVal.numb;
                            ret = CONF_VALUE;
                            break;
                        case sizeof(short):
                            *(short*)ci[i].var = (short)configVal.numb;
                            ret = CONF_VALUE;
                            break;
                        case sizeof(char):
                            *(char*)ci[i].var = (char)configVal.numb;
                            ret = CONF_VALUE;
                            break;
                    }
                }
                break;
            case CONF_INT_PTR:
                if( configVal.isPInt )
                {
                    len = ci[i].size;
                    len2 = configVal.numb*sizeof(int);
                    if( len2<len ) len = len2;
                    memcpy(ci[i].var, configVal.pInt, len);
                    ret = CONF_INT_PTR;
                }
                break;
            case CONF_CHAR_PTR:
                if( configVal.isStr )
                {
                    len = strlen(configVal.str);
                    if (len<CONF_STR_LEN)
                    {
                        strncpy(ci[i].var, configVal.str ,len+1);
                    }
                    ret = CONF_CHAR_PTR;
                }
                break;
            case CONF_OSD_POS:
                if( configVal.isPos )
                {
                    pos = (OSD_pos*)ci[i].var;
                    pos->x = configVal.pos.x;
                    pos->y = configVal.pos.y;
                    ret = CONF_OSD_POS;
                }
                break;
        }
    }

    if (ret != CONF_EMPTY)
    {
        // Perform an updates required on change of value
        if (info_func)
            info_func(id);
        // Save config file is autosave enabled
        if (config_autosave)
            conf_save();
    }

    return ret;
}

int conf_setValue(unsigned short id, tConfigVal configVal)
{
    int i;

    if (id < CONF_CORE)
        id = libconfmigrate->map_old_id(id);  // Convert old ID to new ID

    for (i=0; confinfo_handlers[i].ci != 0; i++)
        if ((id >= confinfo_handlers[i].start_id) && (id <= confinfo_handlers[i].end_id))
            return setValue(confinfo_handlers[i].ci, id - confinfo_handlers[i].start_id, configVal, confinfo_handlers[i].info_func);

    return CONF_EMPTY;
}

void conf_setAutosave(int n)
{
    config_autosave = (n == 0) ? 0 : 1;
}

//-------------------------------------------------------------------
// Common code extracted from raw.c (raw_savefile) and gui_osd.c (gui_osd_draw_raw_info)
// returns 0 if RAW save is disabled due to mode settings, etc, return 1 if RAW save OK
/*
is raw disabled by exceptions
*/
int is_raw_exception() {
    int m = camera_info.state.mode_shooting;
    // NOTE: the conf.save_raw_in variables are negative logic
    //       1 = disable saving raw in this mode, 0 = allow saving raw
    //       variables should be named conf.disable_save_raw_in_XXX
    return (
        (is_video_recording() && conf.save_raw_in_video) ||                                 // True is movie mode and save_raw_in_video is disabled
#ifdef CAM_HAS_CANON_RAW
        ((shooting_get_canon_image_format() & SHOOTING_CANON_FMT_RAW) && conf.save_raw_in_canon_raw) ||                                                   // True if Canon RAW enabled, for cams that treat it as a resolution setting (g9, g10, s90, sx1? not g12, g1x)
#endif
#ifdef CAM_HAS_SPORTS_MODE
        ((m==MODE_SPORTS) && conf.save_raw_in_sports) ||                                    // True if sports mode and save_raw_in_sports is disabled
#endif
        ((m==MODE_AUTO) && conf.save_raw_in_auto) ||                                        // True if auto mode and save_raw_in_auto is disabled
        (conf.edge_overlay_enable && conf.save_raw_in_edgeoverlay) ||                       // True if edge overlay on and save_raw_in_edgeoverlay is disabled
        ((shooting_get_drive_mode()==1) && conf.save_raw_in_burst && !(m==MODE_SPORTS)) ||  // True if drive mode is continuous and save_raw_in_burst is disabled and not sports mode
        ((shooting_get_drive_mode()>=2) && conf.save_raw_in_timer) ||                       // True if drive mode is timer and save_raw_in_timer is disabled
        ((shooting_get_prop(PROPCASE_BRACKET_MODE)==1) && conf.save_raw_in_ev_bracketing)   // True if bracketing enabled and save_raw_in_ev_bracketing is disabled
    );
}

/*
is raw possible (i.e. valid raw buffer exists in current mode)
TODO this might be better as a platform lib.c function rather than a bunch of camera.h ifdefs
*/
int is_raw_possible() {
    return !(                                           // Return false if any of these tests are true
       (camera_info.state.mode & MODE_DISABLE_RAW)      // Disable if bit set in modemap
#ifdef CAM_DISABLE_RAW_IN_LOW_LIGHT_MODE
       || (shooting_get_resolution()==7)                // True if shooting resolution is 'low light'
#endif
    );
}

int is_raw_enabled()
{
    return is_raw_possible() && !is_raw_exception();
}

//-------------------------------------------------------------------
