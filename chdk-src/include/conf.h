#ifndef CONF_H
#define CONF_H

// CHDK Config items interface

// Note: used in modules and platform independent code.
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

#include "stdlib.h"
#include "color.h"
#include "script.h"
#include "bend.h"
#include "bendx.h"

//==========================================================

#define CONF_STR_LEN        100

#define CONF_EMPTY          0
// regular char-int-short value
#define CONF_VALUE          1
#define CONF_DEF_VALUE      1
// pointer to function returning int
#define CONF_FUNC_PTR       2
// pointer to array of int
#define CONF_INT_PTR        3
// pointer to the string
#define CONF_CHAR_PTR       4
// OSD_pos
#define CONF_OSD_POS        5
// Pointer to struct containing item count, item size and pointer to items
#define CONF_STRUCT_PTR     6

typedef struct {
    unsigned short  x, y;
} OSD_pos;

typedef struct {
    unsigned short  x, y;
} OSD_scale;

typedef struct {
    int isNumb, isStr, isPInt, isPos;
    int numb;
    char* str;
    int* pInt;
    OSD_pos pos;
} tConfigVal;

// Struct to handle variable sized arrays of 'things' in config
typedef struct {
    int             num_items;
    int             item_size;
    int             (*saved_size)();
    char*           (*save)(char *dst);
    int             (*load)(char *src);
} tVarArrayConfig;

// User Menu config structures
typedef struct {
    int     var;            // index or hash of main menu item or script
    char    *script_file;   // name of script file
    char    *script_title;  // name of script (title from script)
} tUserMenuItem;

// Where a shot's bend recipe is written - conf.bend_shot_save.
#define BEND_SHOT_OFF       0
#define BEND_SHOT_IN_JPEG   1   // a comment segment in the picture itself
#define BEND_SHOT_SIDECAR   2   // IMG_nnnn.BND beside it
#define BEND_SHOT_WHERE_N   2

typedef struct {
    tVarArrayConfig cfg;
    tUserMenuItem   *items;
} tUserMenuConfig;

//==========================================================

// Changes to the 'conf' structure may require an update to the CONF_VERSION definition
// If add field to the end of structure minor api version should be increased.
// If any other change (remove something, change order, add not to the end, change meaning), major api version should be increased
// Don't make any of the entries conditionally compiled in - this will change the offsets between cameras causing problems with
// making the modules camera/platform independent

#define CONF_VERSION            {3,6}       // Version for Conf structure

typedef struct
{
    int save_raw;
    int save_raw_in_video;
    int save_raw_in_sports;
    int save_raw_in_burst;
    int save_raw_in_edgeoverlay;
    int save_raw_in_auto;
    int save_raw_in_ev_bracketing;
    int save_raw_in_timer;
    int raw_exceptions_warn;
    int raw_in_dir;
    int raw_prefix;
    int raw_ext;
    int raw_save_first_only; //for series shooting
    int raw_nr;
    int sub_batch_prefix; // output of batch subtracts
    int sub_batch_ext;
    int raw_cache;
    int dng_raw;
    int dng_version;    // User selectable DNG version (0=1.3, 1=1.1) - select 1.1 for CHDK to do bad pixel removal (requires creation of badpixel.bin)
    int raw_timer;
    int raw_dng_ext;
    int dng_usb_ext;
    int dng_crop_size;  // User selectable DNG crop size - 0 = JPEG size (as defined in platform_camera.h), 1 = Active Area, 2 = Full sensor

    int show_osd;
    int hide_osd;
    int show_osd_in_review;
    int rotate_osd;
    int script_shoot_delay;
    int show_histo;
    int script_allow_lua_native_calls;
    char script_file[CONF_STR_LEN];

    int show_dof;
    int batt_volts_max;
    int batt_volts_min;
    int batt_perc_show;
    int batt_volts_show;
    int batt_icon_show;
    int space_bar_show;
    int space_bar_size;
    int space_bar_width;
    int space_icon_show;
    int show_clock;
    int show_temp;
    int temperature_unit;
    int clock_format;
    int clock_indicator;
    int clock_halfpress;
    int space_perc_show;
    int space_mb_show;
    int show_partition_nr;
    int space_perc_warn;
    int space_mb_warn;
    int space_warn_type;
    int show_movie_time;
    int show_movie_refresh;

    int zoom_value;

    int show_state;
    int show_remaining_raw;
    int remaining_raw_treshold;
    int show_raw_state;
    int show_values;
    int show_values_in_video;
    int show_overexp;
    int show_hiddenfiles;

    int histo_mode;
    int histo_auto_ajust;
    int histo_ignore_boundary;
    int histo_layout;
    int histo_show_ev_grid;

    int zebra_draw;
    int zebra_mode;
    int zebra_restore_screen;
    int zebra_restore_osd;
    int zebra_over;
    int zebra_under;
    int zebra_draw_osd;
    int zebra_multichannel;

    OSD_pos histo_pos;
    OSD_pos dof_pos;
    OSD_pos batt_icon_pos;
    OSD_pos space_icon_pos;
    OSD_pos space_hor_pos;
    OSD_pos space_ver_pos;
    OSD_pos batt_txt_pos;
    OSD_pos space_txt_pos;
    OSD_pos mode_state_pos;
    OSD_pos mode_raw_pos;
    OSD_pos values_pos;
    OSD_pos clock_pos;
    OSD_pos mode_video_pos;
    OSD_pos mode_ev_pos;
    OSD_pos temp_pos;
    OSD_pos ev_video_pos;
    OSD_pos usb_info_pos;

    OSD_scale histo_scale;
    OSD_scale dof_scale;
    OSD_scale batt_icon_scale;
    OSD_scale space_icon_scale;
    OSD_scale space_hor_scale;
    OSD_scale space_ver_scale;
    OSD_scale batt_txt_scale;
    OSD_scale space_txt_scale;
    OSD_scale mode_state_scale;
    OSD_scale mode_raw_scale;
    OSD_scale values_scale;
    OSD_scale clock_scale;
    OSD_scale mode_video_scale;
    OSD_scale mode_ev_scale;
    OSD_scale temp_scale;
    OSD_scale ev_video_scale;
    OSD_scale usb_info_scale;

    // User adjustable colors
    confColor histo_color;
    confColor histo_color2;     // markers/border
    confColor osd_color;
    confColor osd_color_warn;
    confColor osd_color_override;
    confColor space_color;
    confColor menu_color;
    confColor menu_title_color;
    confColor menu_cursor_color;
    confColor menu_symbol_color;
    confColor reader_color;
    confColor zebra_color;      // under/over
    confColor grid_color;
    confColor edge_overlay_color;

    int menu_center;
    int menu_select_first_entry;
    int menu_symbol_enable;

    int font_cp;
    char menu_rbf_file[CONF_STR_LEN];
    char menu_symbol_rbf_file[CONF_STR_LEN];

    char lang_file[CONF_STR_LEN];

    char reader_file[CONF_STR_LEN];
    int  reader_pos;
    int  reader_autoscroll;
    int  reader_autoscroll_delay;
    char reader_rbf_file[100];
    int  reader_codepage;
    int  reader_wrap_by_words;

    int flashlight;
    int fast_ev;
    int fast_ev_step;
    int fast_image_quality;
    int fast_movie_control;
    int fast_movie_quality_control;
    int splash_show;
    int start_sound;
    int platformid;

    int use_zoom_mf;
    long alt_mode_button; //for S-series
    int alt_prevent_shutdown;

    int show_grid_lines;
    char grid_lines_file[CONF_STR_LEN];
    int grid_force_color;
    char grid_title[36];

    int video_mode;
    int video_quality;
    int video_bitrate;

    int tv_bracket_value;
    int av_bracket_value;
    int iso_bracket_value;
    int iso_bracket_koef;
    int subj_dist_bracket_value;
    int subj_dist_bracket_koef;
    int bracket_type;
    int bracketing_add_raw_suffix;
    int clear_bracket;
    int clear_video;
    int override_disable;       // Uses negative/obscure logic.
                                // 0 = CHDK Tv/Av/ISO/SD overrides are enabled
                                // 1 = Overrides are disabled
                                // 2 = Overrides are enabled and the shortcut button to enable/disable them is disabled
                                // Notes: Option 2 is depracated from version 1.2
                                //        Disabling these overrides does not disable Auto ISO and Bracketing overrides (see next item)
    int override_disable_all;   // 0 = Auto ISO and Bracketing enabled, even if other overrides disabled above
                                // 1 = Auto ISO and Bracketing enabled only when other overrides are enabled

    int tv_override_enabled;
    int tv_override_value;
    int tv_override_long_exp;
    int tv_override_short_exp;
    int tv_enum_type;           // 0 = Ev Step, 1 = Long Exposure (HH:MM:SS), 2 = Short Exposure (0.xxxxx)
    int av_override_enabled;
    int av_override_value;

    int nd_filter_state;

    int iso_override_value;
    int iso_override_koef;
    int subj_dist_override_value;
    int subj_dist_override_koef;
    int clear_override;

    int autoiso_enable;
    int autoiso_shutter_enum;
    int autoiso_user_factor;
    int autoiso_is_factor;
    int autoiso_max_iso_hi;
    int autoiso_max_iso_auto;
    int autoiso_min_iso;
    int autoiso2_shutter_enum;
    int autoiso2_max_iso_auto;
    //AutoISO2 precalced values
    float autoiso2_coef;        // C2=( iso2_max_auto_real - iso_max_auto_real) / ( tv_num[autoiso_shutter] - tv_numerator[autoiso2_shutter])
    int autoiso_max_iso_hi_real;    // converted from GUI (possible marketing) to real iso value
    int autoiso_max_iso_auto_real;
    int autoiso_min_iso_real;
    int autoiso2_max_iso_auto_real;
    int autoiso2_over;

    int overexp_threshold;
    int overexp_ev_enum;    // 0-off, 1=-1/3, ..

    int dof_subj_dist_as_near_limit;
    int dof_use_exif_subj_dist;
    int dof_subj_dist_in_misc;
    int dof_near_limit_in_misc;
    int dof_far_limit_in_misc;
    int dof_hyperfocal_in_misc;
    int dof_depth_in_misc;

    int values_show_in_review;
    int values_show_zoom;
    int values_show_real_aperture;
    int values_show_real_iso;
    int values_show_market_iso;
    int values_show_iso_only_in_autoiso_mode;
    int values_show_ev_seted;
    int values_show_ev_measured;
    int values_show_bv_measured;
    int values_show_bv_seted;
    int values_show_overexposure;
    int values_show_canon_overexposure;
    int values_show_luminance;

    int debug_shortcut_action;   // 0=none, 1=dump, 2=page display
    int debug_display;           // 0=none, 1=props, 2=flash param, 3=tasks
    int debug_propcase_page;     // save propcase page
    int debug_lua_restart_on_error; // 0=no, 1=yes
    int debug_misc_vals_show;    // 0=no, 1=yes

    int script_startup;          // remote autostart
    int remote_enable;           // remote enable
    int user_menu_enable;
    int user_menu_as_root;
    tUserMenuConfig user_menu_vars;
    int zoom_scale;
    int unlock_optical_zoom_for_video;
    int mute_on_zoom;
    int bad_pixel_removal;
    int video_af_key;

    // Bit bending - see include/bend.h and BENDING_DESIGN.md. Software
    // equivalent of rerouting the ADC data lines between the sensor front end
    // and DIGIC, plus the non-data signals you would clip onto in hardware.
    // The matrix and its knobs live in bitbend; the two ints beside it are
    // switches rather than part of the bend, so they are not saved in presets.
    // bitbend_simple holds the matrix to the sources that compile to a pure
    // lookup - see bend_simplify() for why that is the difference between a
    // shot that saves and a shot you wait out.
    int    bitbend_enable;
    int    bitbend_simple;
    bend_t bitbend;

    // Experimental profiles - see include/bendx.h and
    // docs/EXPERIMENTAL_EFFECTS.md. The second engine: not the bus between the
    // sensor and DIGIC but the frame buffer it writes into, failing one named
    // way at a time. Kept beside the matrix rather than inside it because the
    // two are orthogonal - either, neither or both - and because putting a
    // profile inside bend_t would invalidate every BND file on every card.
    // Profiles stack - see the chain layout note in bendx.h. The live one is
    // the slot the rocker turns; the locked ones are frozen and run first.
    //
    // NOT conditional on CAM_BEND_EXPERIMENTAL, deliberately, despite
    // everything that reads it being so. See the warning above this struct:
    // the modules are built once and share this layout by offset, so a field
    // that exists on some cameras and not others moves everything after it.
    // Fifty-six bytes of unread config is the cheaper mistake.
    int           bendx_enable;
    bendx_chain_t bendx;

    // Where what each bent frame was taken with gets written - one of the
    // BEND_SHOT_* values below. On by default: the whole value of it is that
    // it is already there when a frame turns out to be worth reproducing, and
    // a switch you have to remember to set beforehand does not have that
    // value.
    //
    // The default is inside the JPEG (include/bend_tag.h) rather than beside
    // it (include/bend_shot.h), because a sidecar is left behind the moment
    // the picture is copied anywhere. Old cards stored 1 for "on", which is
    // still the value that means the default - so a card that predates the
    // comment simply starts writing them.
    int    bend_shot_save;

    // Segments - see include/bend.h and include/bend_seg.h. The layout, the
    // region being edited, and the matrices for regions 1..3; region 0's
    // matrix is bitbend above, which is what keeps a preset file one bend and
    // keeps a camera with the layout off byte-identical to one without this.
    //
    // Unconditional for the same reason the chain above it is: the modules
    // share this layout by offset.
    bend_segs_t bend_segs;

    // Multiple exposure - see include/mexp.h. Plain ints: on, which of the
    // four blend modes, how many exposures make a picture, whether the ghost
    // is drawn, and whether bending happens to every exposure or once to the
    // completed composite. The
    // sequence's own state is deliberately not here - it is in mexp.c and it
    // does not survive a power cycle, because an accumulator that outlived the
    // battery would combine two exposures that have nothing to do with each
    // other and give no sign that it had.
    // Which colour scheme the fork's own UI draws in - see include/theme.h.
    // One int, because a theme is a choice from a fixed list rather than a set
    // of colours: the palette is Canon's and cannot be added to.
    // Draw the segment layout over the live view so the pattern can be seen
    // before it is shot - see the note on bm_mask_draw() in gui_bend.c. Its
    // own int rather than a field of bend_segs_t, because that struct is
    // stored as a blob and growing it shifts every matrix in it (see the
    // retired ID 294 in conf.c).
    // Game Boy screen tint and lens mode - see modules/gbc/gbc_emu.c. In the
    // config block rather than in the module because they are settings, and a
    // module's statics go away with the module.
    int    gbc_hue;

    int    bend_seg_mask;

    int    theme;

    int    mexp_enable;
    int    mexp_mode;
    int    mexp_frames;
    int    mexp_ghost;
    int    mexp_bend_each;

    char curve_file[CONF_STR_LEN];
    int curve_enable;

    int edge_overlay_enable;
    int edge_overlay_filter;
    int edge_overlay_thresh;
    int edge_overlay_zoom;              // shall zoom be set when *edg file is loaded?
    int edge_overlay_pano;              // whether a full press changes back to live mode
    int edge_overlay_pano_overlap;      // overlap in % in pano mode
    int edge_overlay_show;              // whether to show overlay even when no button is pressed
    int edge_overlay_play;              // whether edge overlay is switched on also for play mode

    int synch_enable;
    int ricoh_ca1_mode;
    int synch_delay_enable;
    int synch_delay_value;

    int script_param_set;
    int script_param_save;

    long mem_view_addr_init;
    int flash_sync_curtain;
    int flash_video_override;
    int flash_manual_override;
    int flash_video_override_power;
    int flash_enable_exp_comp;
    int flash_exp_comp;

    // Overrides to disable touchscreen U/I components (IXUS 310)
    int touchscreen_disable_video_controls;
    int touchscreen_disable_shortcut_controls;

    // Enable USB icon
    int usb_info_enable;

    // gen 2 USB remote
    int remote_switch_type;
    int remote_control_mode;

    int ext_video_time;

    int remote_enable_scripts;  // usb remote activates scripts in <ALT> mode

    int zoom_assist_button_disable;    // used to disable the zoom assist button on SX30 & SX40 for people who keep accidentaly pressing it

    int gps_record;
    int gps_navi_show;
    int gps_navi_hide;
    int gps_compass_show;
    int gps_compass_hide;
    int gps_coordinates_show;
    int gps_height_show;
    int gps_waypoint_save;
    int gps_track_time;
    int gps_wait_for_signal;
    int gps_compass_time;
    int gps_navi_time;
    int gps_wait_for_signal_time;
    int gps_compass_smooth;
    int gps_batt;
    int gps_countdown;
    int gps_2D_3D_fix;
    int gps_countdown_blink;
    int gps_rec_play_set;
    int gps_play_dark_set;
    int gps_rec_play_time;
    int gps_play_dark_time;
    int gps_rec_play_set_1;
    int gps_play_dark_set_1;
    int gps_rec_play_time_1;
    int gps_play_dark_time_1;
    int gps_show_symbol;

    int gps_batt_warn;
    int gps_track_symbol;
    int gps_test_timezone;
    int gps_beep_warn;
    int gps_on_off;

    int tbox_char_map;          // Text input box language/char map
    int show_alt_helper;        // Show <ALT> mode help screen
    int show_alt_helper_delay;  // Delay before showing help screen

    long extra_button; // extra button for use in ALT mode, 0 when disabled

    int module_logging;

    int enable_shortcuts;     // enable keyboard shortcuts

    int console_show;           // 0 = ALT mode only, 1 = always
    int console_timeout;        // Delay after last update to stop showing console (seconds)

    int memdmp_start;           // start address for memory dump
    int memdmp_size;            // amount of memory to dump in bytes (0 for all RAM)

    int remote_input_channel;   // remote input channel,  0=USB, 1=A/D

    int hotshoe_override;       // flash hot shoe override (0=no override, 1='not occupied', 2='occupied')

    int allow_unsafe_io;        // Allow open/close/write to continue if fileio_semaphore times out (instead of returning error)

    int disable_lfn_parser_ui;     // 0 = use built-in opendir functions for long filenames in filebrowser on dryos,
                                   // 1 = only use firmware opendir

    int enable_raw_shortcut;    // enable RAW state toggle keyboard shortcut, 0=no, 1=yes, 2=yes with RAW OSD enabled

    int osd_platformid; // platform ID in OSD cfg file

    int save_raw_in_canon_raw; // disable raw when Canon raw enabled

    int clean_overlay;  // invisible Canon overlay: 0 = never, 1 = rec mode, 2 = during movie record

    int unlock_av_out_in_rec;  // Force video out when in Rec if AV bit is set. Bool

    int check_firmware_crc;  // Check firmware CRC on startup. 0 = no. 1 = Check and set to 0. 2 = always

    // RAW EV Histogram
    int raw_ev_histo_enable;
    OSD_pos raw_ev_histo_pos;
    int raw_ev_histo_under_threshold;
    int raw_ev_histo_over_threshold;
    int raw_ev_histo_width;
    int raw_ev_histo_height;
    int raw_ev_histo_xstep;
    int raw_ev_histo_ystep;
    int raw_ev_histo_save_log;
    int raw_ev_histo_ettr_pct;
    int raw_ev_histo_save_for_image;

} Conf;

extern Conf conf;

//==========================================================

// Some macros to simplify the code

// True if TV/AV/ISO/SD overrides are enabled, false if disabled (excludes Auto ISO and bracketing overrides which can be still be enabled)
#define overrides_are_enabled   ( conf.override_disable != 1 )
// True if Auto ISO and bracketing overrides are enabled, false if disabled
#define autoiso_and_bracketing_overrides_are_enabled    ( !(conf.override_disable == 1 && conf.override_disable_all) )

// True if a TV override value is set, false otherwise
#define TV_OVERRIDE_EV_STEP     0
#define TV_OVERRIDE_SHORT_EXP   1
#define TV_OVERRIDE_LONG_EXP    2
#define is_tv_override_enabled  (  conf.tv_override_enabled && overrides_are_enabled && \
                                    ((conf.tv_enum_type == TV_OVERRIDE_EV_STEP) || \
                                     (conf.tv_override_short_exp && (conf.tv_enum_type == TV_OVERRIDE_SHORT_EXP)) ||  \
                                     (conf.tv_override_long_exp && (conf.tv_enum_type == TV_OVERRIDE_LONG_EXP)) \
                                     ) \
                                )
// True if a AV override value is set, false otherwise
#define is_av_override_enabled  ( conf.av_override_enabled && overrides_are_enabled )
// True if a ISO override value is set, false otherwise
#define is_iso_override_enabled ( conf.iso_override_value && conf.iso_override_koef && overrides_are_enabled )
// True if a SD (subject distance) override value is set, false otherwise
#define SD_OVERRIDE_OFF         0
#define SD_OVERRIDE_ON          1
#define SD_OVERRIDE_INFINITY    2
#define is_sd_override_enabled  ( conf.subj_dist_override_value && conf.subj_dist_override_koef && overrides_are_enabled )

// True if a TV bracketing value is set, false otherwise
#define is_tv_bracketing_enabled  ( conf.tv_bracket_value && autoiso_and_bracketing_overrides_are_enabled )
// True if a AV bracketing value is set, false otherwise
#define is_av_bracketing_enabled  ( conf.av_bracket_value && autoiso_and_bracketing_overrides_are_enabled )
// True if a ISO bracketing value is set, false otherwise
#define is_iso_bracketing_enabled ( conf.iso_bracket_value && conf.iso_bracket_koef && autoiso_and_bracketing_overrides_are_enabled )
// True if a SD (subject distance) bracketing value is set, false otherwise
#define is_sd_bracketing_enabled  ( conf.subj_dist_bracket_value && conf.subj_dist_bracket_koef && autoiso_and_bracketing_overrides_are_enabled && shooting_can_focus() )

#define ALT_PREVENT_SHUTDOWN_NO         0
#define ALT_PREVENT_SHUTDOWN_ALT        1
#define ALT_PREVENT_SHUTDOWN_ALT_SCRIPT 2
#define ALT_PREVENT_SHUTDOWN_ALWAYS     3

#define SHOOTING_PROGRESS_NONE          0
#define SHOOTING_PROGRESS_STARTED       1
#define SHOOTING_PROGRESS_PROCESSING    2
#define SHOOTING_PROGRESS_DONE          3

// script conf.script_startup values
#define SCRIPT_AUTOSTART_NONE             0
#define SCRIPT_AUTOSTART_ALWAYS           1
#define SCRIPT_AUTOSTART_ONCE             2
#define SCRIPT_AUTOSTART_ALT              3

extern void conf_save();
extern void conf_restore();
extern void conf_load_defaults();
extern void conf_change_dng(void);
extern void conf_update_prevent_shutdown(void);
extern void cb_autoiso_menu_change(unsigned int item);  // gui.c
extern int conf_getValue(unsigned short id, tConfigVal* configVal);
extern int conf_setValue(unsigned short id, tConfigVal configVal);
extern void conf_setAutosave(int n);

// reyalp: putting these in conf, since the conf values are lookups for them
// prefixes and extentions available for raw images (index with conf.raw_prefix etc)
#define NUM_IMG_FOLDER_NAMES 3  //
#define NUM_IMG_PREFIXES 3  // IMG_ CRW_ SND_ (could add ETC_ SDR_ AUT_ MVI_ MRK_)
#define NUM_IMG_EXTS 5      // .JPG .CRW .CR2 .THM .WAV (could add .AVI .MRK)
extern const char* img_folders[NUM_IMG_FOLDER_NAMES];
extern const char* img_prefixes[NUM_IMG_PREFIXES];
extern const char* img_exts[NUM_IMG_EXTS];

// is raw data available in the current mode and not blocked by current "raw exceptions" ?
extern int is_raw_enabled();
// is raw data valid raw data available in the current mode
extern int is_raw_possible();

//-------------------------------------------------------------------

typedef struct {
    unsigned short      id;
    unsigned char       size;
    char                type;
    void                *var;
    union {
        void            *ptr;
        int             i;
        confColor       cl;
        OSD_pos         pos;
        OSD_scale       scale;
        long            (*func)(void);
    };
    unsigned int        last_saved;     // Record last value saved to file to determine if file needs updating
} ConfInfo;

// if new CONF_INFO macros are added, update tools/gen_conf_lua.c
#define CONF_INFO(id, param, type, def)             { id, sizeof( param ), type, &param, {def}, 0 }
#define CONF_INFO2(id, param, type, px, py)         { id, sizeof( param ), type, &param, {pos:{px,py}}, 0 }
#define CONF_INFOP(id, param, type, px, py)         { id, sizeof( param ), type, &param, {pos:{(px)*CAM_SCREEN_WIDTH/360,(py)*CAM_SCREEN_HEIGHT/240}}, 0 }
#define CONF_INFOC(id, param, type, bc, fc, bt, ft) { id, sizeof( param ), type, &param, {cl:{fg:{fc,ft},bg:{bc,bt}}}, 0 }

extern void config_save(ConfInfo *conf_info, const char *filename, int config_base);
extern void config_restore(ConfInfo *confinfo, const char *filename, void (*info_func)(unsigned short id));

extern int save_config_file(int config_base, const char *filename);
extern int load_config_file(int config_base, const char *filename);

extern void resetColors();

//==========================================================

#define CONF_MAGICK_VALUE   (0x33204741)

typedef struct
{
    int         start_id, end_id;
    ConfInfo    *ci;
    char        *filename;
    void        (*info_func)(unsigned short id);
} confinfo_handler;

//-------------------------------------------------------------------

#endif
