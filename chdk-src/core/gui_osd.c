#include "platform.h"
#include "keyboard.h"
#include "lang.h"
#include "conf.h"
#include "console.h"
#include "gui.h"
#include "gui_menu.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "gui_osd.h"
#include "gui_batt.h"
#include "gui_usb.h"
#include "gui_space.h"
#include "histogram.h"
#include "usb_remote.h"
#include "modules.h"
#include "raw_ev_histo.h"

//-------------------------------------------------------------------

extern const char* gui_video_bitrate_enum(int change, int arg);
extern const char* gui_video_min_bitrate_enum(int change, int arg);

//-------------------------------------------------------------------
char osd_buf[64];

// Set by the fast movie quality/bitrate keys in
// kbd_use_up_down_left_right_as_fast_switch(), consumed by
// gui_osd_draw_movie_time_left() to force its remaining-time estimate to
// recalculate after the bitrate changes under it.
//
// Declared out here because the setter survives CAM_PERSISTENT_OSD and the
// consumer does not. Nothing is lost when it goes inert: it only ever reset a
// display that is no longer drawn, never the recording itself.
static unsigned int movie_reset;

// Everything from here to the matching #endif is the stock OSD: the element
// functions gui_draw_osd() used to call, and the helpers only they use.
//
// CAM_PERSISTENT_OSD makes gui_draw_osd() return before reaching any of it, so
// on those cameras this is unreachable code that the linker still keeps - the
// element functions are global, and two of them are in the module export list.
// On the a480 that was 7912 bytes of text in a 204800-byte budget with 15328
// free, which is most of the room a replacement UI needs.
//
// Compiling it out rather than deleting it: the a410/a430/a460/a540/a640 ports
// do not set CAM_PERSISTENT_OSD and still draw the stock OSD from these very
// functions. See docs/A480_UI_REVERSING.md for the size accounting.
#ifndef CAM_PERSISTENT_OSD

typedef struct {
    short av96;
    short tv96;
    short sv96;
    short iso;
    //short sv96_market;
    short iso_market;
    short bv96_measured;
    short bv96_seted;       //Ev96_internal-Sv96
    short ev96_seted;       //Tv96+Av96
    short ev96_measured;    //Bv96+Sv96
    short dev96;            // Ev96_external-Ev96_internal
    short dev96_canon;      // Canon OverExposure
    int b;                  //average scene luminance 
} EXPO_TYPE;

static EXPO_TYPE expo;

//-------------------------------------------------------------------
// Append scaled value display of 'dist' to 'osd_buf'
static void sprintf_dist(char *buf, int dist) {
// length of printed string is always 4
    if (dist<=0) {
        sprintf(buf, " inf");
    } else {
        int i = dist / 1000;
        int f = dist % 1000;
        if (i == 0)
            sprintf(buf, ".%03d", f);
        else if (i < 10)
            sprintf(buf, "%d.%02d", i, (f+5)/10);
        else if (i < 100)
            sprintf(buf, "%02d.%d", i, (f+50)/100);
        else
            sprintf(buf, "%4d", i);
    }
}

static void sprintf_canon_values(char *buf, short dist) 
{
    short v=((dist<0)?-dist:dist);
    sprintf(buf, "%s%d.%02d", ((dist<0)?"-":""), v/96, v%96);
}


//-------------------------------------------------------------------
static void gui_osd_calc_expo_param()
{
    expo.av96=shooting_get_av96();
    expo.tv96=shooting_get_tv96();
    expo.sv96=shooting_get_sv96_real();
    expo.iso=shooting_get_iso_real();
    //expo.sv96_market=shooting_get_sv96_market();
    expo.iso_market=shooting_get_iso_market();
    expo.bv96_measured=shooting_get_bv96();
    expo.ev96_seted=expo.tv96+expo.av96; //Tv96+Av96
    expo.ev96_measured=expo.bv96_measured+expo.sv96;//Bv96+Sv96
    expo.dev96=expo.ev96_measured-expo.ev96_seted;// Ev96_external-Ev96_internal
    expo.bv96_seted=expo.ev96_seted-expo.sv96;
    expo.dev96_canon=shooting_get_canon_overexposure_value();
    expo.b=shooting_get_luminance();
}

void gui_osd_draw_dof(int is_osd_edit) 
{
    if (conf.show_dof != DOF_DONT_SHOW)
        shooting_update_dof_values();

    if (is_osd_edit ||
        (camera_info.state.mode_rec_or_review && ((conf.show_dof == DOF_SHOW_IN_DOF) || (conf.show_dof == DOF_SHOW_IN_DOF_EX)) &&
         (((camera_info.state.is_shutter_half_press || camera_info.state.state_kbd_script_run || shooting_get_common_focus_mode()) &&
           (camera_info.state.mode_photo || camera_info.state.mode_shooting==MODE_STITCH)) ||
          ((camera_info.state.mode_video || is_video_recording()) && conf.show_values_in_video))))
    {
        twoColors col = user_color(conf.osd_color);
        twoColors valid_col = MAKE_COLOR(BG_COLOR(col), COLOR_GREEN);
        int i = 8, j;
        short f_ex = (conf.show_dof==DOF_SHOW_IN_DOF_EX);
        draw_osd_string(conf.dof_pos, 0, 0, "S/NL/FL:", col, conf.dof_scale);
        sprintf_dist(osd_buf, camera_info.dof_values.subject_distance);
        j = strlen(osd_buf);
        draw_osd_string(conf.dof_pos, i*FONT_WIDTH, 0, osd_buf, (f_ex && (camera_info.dof_values.distance_valid || shooting_get_focus_mode()))?valid_col:col, conf.dof_scale);
        i = i+j;
        draw_osd_string(conf.dof_pos, i*FONT_WIDTH, 0, "/", col, conf.dof_scale);
        sprintf_dist(osd_buf, camera_info.dof_values.near_limit);
        j = strlen(osd_buf);
        draw_osd_string(conf.dof_pos, (++i)*FONT_WIDTH, 0, osd_buf, (f_ex && camera_info.dof_values.distance_valid)?valid_col:col, conf.dof_scale);
        i = i+j;
        draw_osd_string(conf.dof_pos, i*FONT_WIDTH, 0, "/", col, conf.dof_scale);
        sprintf_dist(osd_buf, camera_info.dof_values.far_limit);
        draw_osd_string(conf.dof_pos, (++i)*FONT_WIDTH, 0, osd_buf, (f_ex && camera_info.dof_values.distance_valid)?valid_col:col, conf.dof_scale);
        i = 8;
        draw_osd_string(conf.dof_pos, 0, FONT_HEIGHT, "DOF/HYP:", col, conf.dof_scale);
        sprintf_dist(osd_buf, camera_info.dof_values.depth_of_field);
        j = strlen(osd_buf);
        draw_osd_string(conf.dof_pos, i*FONT_WIDTH, FONT_HEIGHT, osd_buf, (f_ex && camera_info.dof_values.distance_valid)?valid_col:col, conf.dof_scale);
        i = i+j;
        draw_osd_string(conf.dof_pos, i*FONT_WIDTH, FONT_HEIGHT, "/", col, conf.dof_scale);
        sprintf_dist(osd_buf, camera_info.dof_values.hyperfocal_distance);
        draw_osd_string(conf.dof_pos, (++i)*FONT_WIDTH, FONT_HEIGHT, osd_buf, (f_ex && camera_info.dof_values.hyperfocal_valid)?valid_col:col, conf.dof_scale);
    }
}

//-------------------------------------------------------------------
static short state_y, misc_y; //string row position

static void gui_print_osd_state_string()
{
    sprintf(osd_buf+strlen(osd_buf), "%12s", "");
    osd_buf[12]=0;  // limit length to 12 max
    draw_osd_string(conf.mode_state_pos, 0, state_y, osd_buf, user_color(conf.osd_color_override), conf.mode_state_scale);
    state_y += FONT_HEIGHT;
}

static void gui_print_osd_state_string_int(const char * title, int value)
{
    sprintf(osd_buf, "%s%d", title, value);
    gui_print_osd_state_string();
}

static void gui_print_osd_state_string_chr(const char *title, const char *value)
{
    sprintf(osd_buf, "%s%s", title, value);
    gui_print_osd_state_string();
}

static void gui_print_osd_state_string_float(const char * fmt, int divisor, int value)
{
    sprintf(osd_buf, fmt, (int)(value/divisor), (int)(value%divisor));
    gui_print_osd_state_string();
}

static void gui_print_osd_misc_string()
{
    sprintf(osd_buf+strlen(osd_buf), "%9s", "");
    osd_buf[9]=0;  // limit length to 9 max
    draw_osd_string(conf.values_pos, 0, misc_y, osd_buf, user_color(conf.osd_color), conf.values_scale);
    misc_y += FONT_HEIGHT;
}

static void gui_print_osd_misc_string_int(const char * title, int value)
{
    sprintf(osd_buf, "%s%d", title, value);
    gui_print_osd_misc_string();
}

static void gui_print_osd_misc_string_float(const char * fmt, int divisor, int value)
{
    sprintf(osd_buf, fmt, (int)(value/divisor), (int)(value%divisor));
    gui_print_osd_misc_string();
}

static void gui_print_osd_misc_string_canon_values(const char * title, short value)
{
    strcpy(osd_buf, title);
    sprintf_canon_values(osd_buf+strlen(osd_buf), value);
    gui_print_osd_misc_string();
}
    
static void gui_print_osd_dof_string_dist(const char * title, int value, short use_good_color) {
  strcpy(osd_buf, title);
  int i=strlen(osd_buf);
  twoColors col = user_color(conf.osd_color);
  twoColors valid_col = MAKE_COLOR(BG_COLOR(col), COLOR_GREEN);
  if (i<8) {
    draw_osd_string(conf.values_pos, 0, misc_y, osd_buf, col, conf.values_scale);
    sprintf_dist(osd_buf, value);
    sprintf(osd_buf+strlen(osd_buf), "%9s", "");
    osd_buf[9-i]=0;
    draw_osd_string(conf.values_pos, i*FONT_WIDTH, misc_y, osd_buf, use_good_color?valid_col:col, conf.values_scale);
  } else {
    osd_buf[9]=0;
    draw_osd_string(conf.values_pos, 0, misc_y, osd_buf, col,conf.values_scale);
  }
  misc_y += FONT_HEIGHT;
}

//-------------------------------------------------------------------
void gui_osd_draw_raw_info(int is_osd_edit) 
{
    if (is_osd_edit ||
        (conf.save_raw && conf.show_raw_state && 
         !camera_info.state.mode_video && !camera_info.state.is_shutter_half_press && 
         camera_info.state.mode_rec_or_review
        )
       )
    {
        static int b;   // controls blinking color for warning
        strcpy(osd_buf, gui_raw_type_string());
        if (is_raw_enabled() || is_osd_edit)
        { 
            int raw_count = GetRawCount();
            twoColors col = user_color(((raw_count > conf.remaining_raw_treshold) || (b <= 6)) ? conf.osd_color : conf.osd_color_warn);
            if (conf.show_remaining_raw || is_osd_edit) 
            {
                sprintf(osd_buf+strlen(osd_buf), ":%3d", raw_count);
            }
            draw_osd_string(conf.mode_raw_pos, 0, 0, osd_buf, col, conf.mode_raw_scale);
            if (++b > 12) b = 0;
        }
        else if (conf.raw_exceptions_warn)
        {
            gui_print_osd_state_string_chr(osd_buf," Disabled"); 
        }
    }
}

//-------------------------------------------------------------------

// From gui.c
extern const char* gui_bracket_values_modes[];
extern const char* gui_bracket_type_modes[];
extern const char* gui_fast_image_quality_modes[];

void gui_osd_draw_state(int is_osd_edit)
{
    state_y = 0;

    if ((conf.show_state && camera_info.state.mode_rec_or_review) || is_osd_edit)
    {
        long t; 

        ///////////////////////////
        //sprintf(osd_buf,"%s",get_debug());
        //draw_string(conf.mode_state_pos.x, conf.mode_state_pos.y+6*FONT_HEIGHT, osd_buf, user_color(conf.osd_color));
        ////////////////////////////  

        if (is_tv_override_enabled || is_osd_edit)
        {
            if(camera_info.state.is_shutter_half_press) 
            { 
                t=(int)(shooting_get_shutter_speed_from_tv96(shooting_get_tv96())*100000);
                gui_print_osd_state_string_float("TV:%d.%05d", 100000, t);
            }
            else 
            {
                if (conf.tv_enum_type==TV_OVERRIDE_EV_STEP)
                    gui_print_osd_state_string_chr("TV:",tv_override_value_string()); 
                else if (conf.tv_enum_type==TV_OVERRIDE_SHORT_EXP)
                    gui_print_osd_state_string_float("TV:%d.%05d", 100000, conf.tv_override_short_exp);
                else
                    gui_print_osd_state_string_chr("TV:",gui_hhmss_enum(0,(int)(&conf.tv_override_long_exp))); 
            }
        }
        if (is_av_override_enabled || is_osd_edit)  
            gui_print_osd_state_string_float("AV:%d.%02d", 100, shooting_get_aperture_from_av96(shooting_get_av96_override_value())/10);
        if (camera_info.cam_has_nd_filter)
            if ((conf.nd_filter_state && !(conf.override_disable==1))|| is_osd_edit) 
                gui_print_osd_state_string_chr("NDFILTER:", ((conf.nd_filter_state==1)?"IN":"OUT"));
        if ((conf.autoiso_enable && shooting_get_iso_mode()<=0 && !(camera_info.state.mode_shooting==MODE_M || camera_info.state.mode_shooting==MODE_TV) && shooting_get_flash_mode() && (autoiso_and_bracketing_overrides_are_enabled)) || is_osd_edit)  
            gui_print_osd_state_string_chr("AUTOISO:", ((conf.autoiso_enable==1)?"ON":"OFF"));
        if ((is_sd_override_enabled && shooting_can_focus()) || (camera_info.state.gui_mode_alt && shooting_get_common_focus_mode()) || is_osd_edit)
        {
            gui_print_osd_state_string_chr("SD:",gui_subj_dist_override_value_enum(0,0));
            if (camera_info.state.gui_mode_alt)
                gui_print_osd_state_string_chr("Adj:",(conf.subj_dist_override_koef == SD_OVERRIDE_ON) ? menu_increment_factor_string() : gui_subj_dist_override_koef_enum(0,0));
        }
        if (is_iso_override_enabled || is_osd_edit)
            gui_print_osd_state_string_int("ISO:", shooting_iso_real_to_market(shooting_get_iso_override_value())); // get_iso_override returns "real" units, clamped within camera limits
        if (is_osd_edit || (shooting_get_drive_mode() && camera_info.state.mode_shooting!=MODE_STITCH && camera_info.state.mode_shooting!=MODE_BEST_IMAGE))
        {
          if (is_tv_bracketing_enabled || is_av_bracketing_enabled || is_iso_bracketing_enabled || is_sd_bracketing_enabled)
            gui_print_osd_state_string_chr("BRACKET:", gui_bracket_type_modes[conf.bracket_type]);
          if (is_tv_bracketing_enabled)
            gui_print_osd_state_string_chr("TV:", gui_bracket_values_modes[conf.tv_bracket_value]);
          else if (is_av_bracketing_enabled)
            gui_print_osd_state_string_chr("AV:", gui_bracket_values_modes[conf.av_bracket_value]);
          else if (is_iso_bracketing_enabled)
            gui_print_osd_state_string_int("ISO:", conf.iso_bracket_value);
          else if (is_sd_bracketing_enabled)
            gui_print_osd_state_string_int("SD:", conf.subj_dist_bracket_value);
        }
#ifdef OPT_CURVES
        if (conf.curve_enable || is_osd_edit) {
            extern const char* gui_conf_curve_enum(int change, int arg);
            gui_print_osd_state_string_chr("CURVE:", gui_conf_curve_enum(0,0));
        }
#endif
        if (conf.override_disable == 1) gui_print_osd_state_string_chr("NO ", "OVERRIDES");
        if (conf.flash_manual_override) gui_print_osd_state_string_chr("Flash:M ", flash_power_mode_string());
        if (conf.flash_enable_exp_comp) gui_print_osd_state_string_chr("Flash:A ", flash_exp_comp_modes_string());
        // edgeoverlay state
        if (conf.edge_overlay_enable || is_osd_edit) {
            gui_print_osd_state_string_chr("EDGE:", (camera_info.edge.state_draw == 0) ? "LIVE" : ((conf.edge_overlay_pano == 0) ? "FROZEN" : "PANO"));
        }
#ifdef CAM_QUALITY_OVERRIDE
        // displaying the overriding picture quality if active
        if ((conf.fast_image_quality!=3) || is_osd_edit) {
            gui_print_osd_state_string_chr("QUALI:", gui_fast_image_quality_modes[conf.fast_image_quality]);
        }
#endif

/*
 draw_string(conf.mode_state_pos.x, conf.mode_state_pos.y+n, get_debug(), user_color(conf.osd_color));
        n+=FONT_HEIGHT;*/
    }
}

//-------------------------------------------------------------------
static void gui_osd_draw_values(int is_osd_edit, int is_zebra)
{
    if (is_osd_edit || (camera_info.state.mode_rec_or_review && conf.show_values))
    {
        // showtype
        //  1   - show all values
        //  2   - show DOF values only (for zebra & MF)
        int showtype = 0;
        if (!is_zebra &&
            ((conf.show_values==SHOW_MISC_ALWAYS && camera_info.state.mode_photo) ||
             ((camera_info.state.mode_video || is_video_recording()) && conf.show_values_in_video) ||
             ((camera_info.state.is_shutter_half_press || (recreview_hold==1)) && (conf.show_values==SHOW_MISC_SHOOT))))
            showtype = 1;
        else if (is_zebra || (shooting_get_common_focus_mode() && camera_info.state.mode_photo && conf.show_values && !((conf.show_dof==DOF_SHOW_IN_DOF) || (conf.show_dof==DOF_SHOW_IN_DOF_EX))))
            showtype = 2;

        if (conf.values_show_real_iso || conf.values_show_market_iso || conf.values_show_ev_seted || 
            conf.values_show_ev_measured || conf.values_show_bv_measured || conf.values_show_bv_seted || 
            conf.values_show_overexposure || conf.values_show_canon_overexposure || conf.values_show_luminance)
            gui_osd_calc_expo_param();

        misc_y = 0;

        short f_ex = (conf.show_dof==DOF_SHOW_IN_MISC_EX);

        if (((conf.show_dof==DOF_SHOW_IN_MISC) || f_ex) && showtype && !is_osd_edit)
        {
          if (conf.dof_subj_dist_in_misc)
              gui_print_osd_dof_string_dist("SD :", camera_info.dof_values.subject_distance, f_ex && (camera_info.dof_values.distance_valid || shooting_get_focus_mode()));
          if (conf.dof_near_limit_in_misc)
              gui_print_osd_dof_string_dist("NL :", camera_info.dof_values.near_limit, f_ex && camera_info.dof_values.distance_valid);
          if (conf.dof_far_limit_in_misc)
              gui_print_osd_dof_string_dist("FL :", camera_info.dof_values.far_limit, f_ex && camera_info.dof_values.distance_valid);
          if (conf.dof_depth_in_misc)
              gui_print_osd_dof_string_dist("DOF:", camera_info.dof_values.depth_of_field, f_ex && camera_info.dof_values.distance_valid);
          if (conf.dof_hyperfocal_in_misc)
              gui_print_osd_dof_string_dist("HYP:", camera_info.dof_values.hyperfocal_distance, f_ex && camera_info.dof_values.hyperfocal_valid);
        }

        if ((showtype == 1) || is_osd_edit)
        {
            if (conf.values_show_zoom || is_osd_edit)
            {
                int fl, zp=shooting_get_zoom(), fl1=get_focal_length(zp);     
                switch (conf.zoom_value)
                {
                 case ZOOM_SHOW_FL:
                     sprintf(osd_buf, "Z:%d.%dmm%8s", fl1/1000, fl1%1000/100, "");
                     break;
                 case ZOOM_SHOW_EFL:
                     fl=get_effective_focal_length(zp);
                     // scale by users adapter lens eg. Canon Wide .42 or Canon Tele 1.75
                     fl = fl * conf.zoom_scale / 100;
                     sprintf(osd_buf, "Z:%3dmm%8s", fl/1000, "");
                     break;
                 case ZOOM_SHOW_X:
                 default:
                     fl=get_zoom_x(zp);
                     sprintf(osd_buf, "Z:%d/%d.%dx%8s", zp, fl/10, fl%10, "");
                     break;
                }
                gui_print_osd_misc_string();
            }

            if (conf.values_show_real_aperture || is_osd_edit)
                 gui_print_osd_misc_string_float("Av :%d.%02d ", 100, shooting_get_real_aperture()/10);

            int iso_mode = shooting_get_iso_mode();

            if ((iso_mode <= 0) || !(conf.values_show_iso_only_in_autoiso_mode))
            {
                if (conf.values_show_real_iso)      gui_print_osd_misc_string_int("I-R:", expo.iso);
                if (conf.values_show_market_iso)    gui_print_osd_misc_string_int("I-M:", expo.iso_market);
            }
            if (conf.values_show_bv_measured)           gui_print_osd_misc_string_canon_values("Bvm:", expo.bv96_measured );
            if (conf.values_show_bv_seted)              gui_print_osd_misc_string_canon_values("Bvs:", expo.bv96_seted );
            if (conf.values_show_ev_measured)           gui_print_osd_misc_string_canon_values("Evm:", expo.ev96_measured);
            if (conf.values_show_ev_seted)              gui_print_osd_misc_string_canon_values("Evs:", expo.ev96_seted );
            if (conf.values_show_overexposure)          gui_print_osd_misc_string_canon_values("dE :", expo.dev96);
            if (conf.values_show_canon_overexposure)    gui_print_osd_misc_string_canon_values("dEc:", expo.dev96_canon);
            if (conf.values_show_luminance)             gui_print_osd_misc_string_float("B  :%d.%02d", 100, expo.b);
        }
    }
}

//-------------------------------------------------------------------
#define CLOCK_FORMAT_24     0
#define CLOCK_FORMAT_12     1
#define CLOCK_WITHOUT_SEC   1
#define CLOCK_WITH_SEC      2

void gui_osd_draw_clock(int x, int y, twoColors cl, int is_osd_edit)
{
    if (conf.show_clock || is_osd_edit)
    {
        static char *ampm[2][3] = { { " AM", "A", " "}, { " PM", "P", "." } };

        struct tm *ttm = get_localtime();

        int w = 0;          // Extra width from AM/PM indicator and seconds (if displayed)

        if ((FG_COLOR(cl) == 0) && (BG_COLOR(cl) == 0)) cl = user_color(conf.osd_color);

        if ((camera_info.state.is_shutter_half_press == 0) || (conf.clock_halfpress == 0) || is_osd_edit)
        {
            unsigned int hour = (ttm->tm_hour);
            char *ampm_ind = "";    // AM / PM indicator

            if (conf.clock_format == CLOCK_FORMAT_12)
            {
                ampm_ind = ampm[hour/12][conf.clock_indicator]; //(hour >= 12) ? pm : am; 
                w = strlen(ampm_ind);
                if (hour == 0)
                    hour = 12;
                else if (hour > 12)
                    hour = hour - 12;
            }

            if ((conf.show_clock != CLOCK_WITHOUT_SEC) || is_osd_edit)
            {
                sprintf(osd_buf, "%2u:%02u:%02u%s", hour, ttm->tm_min, ttm->tm_sec, ampm_ind);
                w += 3;
            }
            else
            {
                sprintf(osd_buf, "%2u:%02u%s", hour, ttm->tm_min, ampm_ind);
            }

            if (x)  // for gui_4wins.c
                draw_string(x, y, osd_buf, cl);
            else
                draw_osd_string(conf.clock_pos, 0, 0, osd_buf, cl, conf.clock_scale);
        }
        else if (camera_info.state.is_shutter_half_press && (conf.clock_halfpress == 1))
        {
            sprintf(osd_buf, "%02u", ttm->tm_sec);
            if (conf.clock_pos.x >= 4*FONT_WIDTH) w = 3;
            draw_osd_string(conf.clock_pos, w*FONT_WIDTH, 0, osd_buf, cl, conf.clock_scale);
        }
    }
}

static void gui_osd_draw_movie_time_left()
{
    static int card_used, init_space, elapsed, avg_use, time_left;
    static long init_time;
    static int record_running = 0;
    static int init = 0;
    static unsigned int skipcalls = 1;
    unsigned int hour=0, min=0, sec=0;

    twoColors col = user_color(conf.osd_color);

    if (is_video_recording())
        record_running = 1;
    else
    {
        record_running = 0;
        init = 0;
    }

    if ((conf.show_movie_time > 0) && (camera_info.state.mode_video || record_running) && !camera_info.state.mode_play)
    {
#if CAM_CHDK_HAS_EXT_VIDEO_MENU
        if (camera_info.state.mode_video || is_video_recording())
        {
            // if manual adjust, show the field item to be adjusted
            // if any value overriden, show the override value
#ifndef CAM_MOVIEREC_NEWSTYLE
#if !CAM_VIDEO_QUALITY_ONLY
            if ((conf.video_mode == 0 && conf.fast_movie_quality_control==1) || conf.video_bitrate != VIDEO_DEFAULT_BITRATE)
            {
                // gui_print_osd_state_string_chr("Bitrate: ",video_bitrate_strings[conf.video_bitrate]);
                sprintf(osd_buf, "Bit:%5s",gui_video_bitrate_enum(0,0));
                draw_osd_string(conf.mode_video_pos, 0, 2*FONT_HEIGHT, osd_buf, col, conf.mode_video_scale);
            }
#endif
            if ((conf.video_mode == 1 && conf.fast_movie_quality_control==1) || conf.video_quality != VIDEO_DEFAULT_QUALITY)
            {
                // gui_print_osd_state_string_int("Quality: ",conf.video_quality);
                sprintf(osd_buf, "Qual:%2i",conf.video_quality);
                draw_osd_string(conf.mode_video_pos, 0, 3*FONT_HEIGHT, osd_buf, col, conf.mode_video_scale);
            }
#else // CAM_MOVIEREC_NEWSTYLE
            if ((conf.video_bitrate != VIDEO_DEFAULT_BITRATE) || (conf.video_quality != VIDEO_DEFAULT_QUALITY)) {
                if (conf.video_mode == 1) // CBR, see gui.c
                {
                    sprintf(osd_buf, "CBR %5s",gui_video_bitrate_enum(0,0));
                    draw_osd_string(conf.mode_video_pos, 0, 2*FONT_HEIGHT, osd_buf, col, conf.mode_video_scale);
                }
                else if (conf.video_mode > 1) // VBR, see gui.c
                {
                    sprintf(osd_buf, "VBR %5s / %s",gui_video_bitrate_enum(0,0),gui_video_min_bitrate_enum(0,0));
                    draw_osd_string(conf.mode_video_pos, 0, 2*FONT_HEIGHT, osd_buf, col, conf.mode_video_scale);
                }
            }
#endif // CAM_MOVIEREC_NEWSTYLE
            // everything else is for when recording
            if (!record_running)
            {
                return;
            }
        }
#endif

        if (movie_reset == 1)
        {
            init = 0;
            movie_reset = 0;
        }

        if (record_running == 1 && init == 0)
        {
            init = 1;
            init_space = GetFreeCardSpaceKb();
            init_time  = get_tick_count();
        }

        if (init == 1)
        {
#ifndef CAM_MOVIEREC_NEWSTYLE
            card_used = init_space - GetFreeCardSpaceKb();
            elapsed = (int) ( get_tick_count() - init_time ) / 1000;
            avg_use = elapsed?(card_used / elapsed):1;  // running average Kb/sec (avoids division by zero)
            time_left = avg_use?(GetFreeCardSpaceKb() / avg_use):1; // (avoids division by zero)
#else
            // D6: filesystem related info is not updated during recording
            card_used = shooting_get_video_recorded_size_kb();
            elapsed = (int) ( get_tick_count() - init_time ) / 1000;
            avg_use = elapsed?(card_used / elapsed):1;  // running average Kb/sec (avoids division by zero)
            time_left = avg_use?((init_space - card_used) / avg_use):1; // (avoids division by zero)
#endif
            hour = time_left / 3600;
            min = (time_left % 3600) / 60;
            sec = (time_left % 3600) % 60;

            if (elapsed < 1)
            {
                sprintf(osd_buf, "Calc...");
                draw_osd_string(conf.mode_video_pos, 0, 0, osd_buf, col, conf.mode_video_scale);
            }

            if (--skipcalls ==0)
            {
                if (elapsed > 1)
                {
                    int time_yofst = 0;
                    if (conf.show_movie_time == 3)
                    {
                        // Both lines displayed so offset time value below bit rate
                        time_yofst = FONT_HEIGHT;
                    }
                    if (conf.show_movie_time & 2)
                    {
                        sprintf(osd_buf, "%04d KB/s", avg_use);
                        draw_osd_string(conf.mode_video_pos, 0, 0, osd_buf, col, conf.mode_video_scale);
                    }
                    if (conf.show_movie_time & 1)
                    {
                        sprintf(osd_buf, "-%02d:%02d:%02d", hour, min, sec);
                        draw_osd_string(conf.mode_video_pos, 0, time_yofst, osd_buf, col, conf.mode_video_scale);
                    }
#if CAM_CHDK_HAS_EXT_VIDEO_TIME
                    if( (int)conf.ext_video_time == 1 )
                    {
                        draw_txt_string(0, 13, lang_str(LANG_WARN_VIDEO_EXT_TIME), user_color(conf.osd_color_warn));
                    }
#endif
                }

                skipcalls = conf.show_movie_refresh*5;
            }
        }
    }
}

void gui_osd_draw_ev(int is_osd_edit)
{
    static char *s[6] = {"   ", "1/6", "1/3", "1/2", "2/3", "5/6"};

    if ((!camera_info.state.mode_video && camera_info.state.mode_rec && conf.fast_ev) || is_osd_edit)
    {
        short ev = shooting_get_ev_correction1();

        if (ev/96 || (ev==0))
            sprintf(osd_buf, "EV:  %d %s", abs(ev/96), s[abs(ev/16%6)]);
        else
            sprintf(osd_buf, "EV:  %s  ", s[abs(ev/16%6)]);

        if (ev>0)
            osd_buf[4]='+';
        else if (ev<0)
            osd_buf[4]='-';

        draw_osd_string(conf.mode_ev_pos, 0, 0, osd_buf, user_color(conf.osd_color), conf.mode_ev_scale);
    }
}

//-------------------------------------------------------------------
static void draw_temp(char *lbl, int val, int yofst)
{
    if (conf.temperature_unit != 0)
        val = (val*18+320)/10;
    sprintf(osd_buf,"%s: %i\xb0",lbl, val);
    draw_osd_string(conf.temp_pos, 0, yofst*FONT_HEIGHT, osd_buf, user_color(conf.osd_color), conf.temp_scale);
}

void gui_osd_draw_temp(int is_osd_edit)
{
    int yofst = 0;

    if ((conf.show_temp == 1) || (conf.show_temp == 4) || is_osd_edit)
        draw_temp("opt", get_optical_temp(), yofst++);

    if ((conf.show_temp == 2) || (conf.show_temp == 4) || is_osd_edit)
#ifdef CAM_HAS_CMOS
        draw_temp("CMOS", get_ccd_temp(), yofst++);
#else
        draw_temp("CCD", get_ccd_temp(), yofst++);
#endif

    if ((conf.show_temp == 3) || (conf.show_temp == 4) || is_osd_edit)
        draw_temp("bat", get_battery_temp(), yofst++);
}

//-------------------------------------------------------------------
#if CAM_EV_IN_VIDEO
void gui_osd_draw_ev_video(int is_osd_edit)
{
    static int is_drawn = 0;

    if (!is_video_recording() && !is_osd_edit) return;

    int visible = get_ev_video_avail() || is_osd_edit;

    int x0=conf.ev_video_pos.x, y0=conf.ev_video_pos.y;

    twoColors col = user_color(conf.osd_color);

    if (visible || is_drawn)
    {
        draw_rectangle(x0,y0,x0+70,y0+24, visible? MAKE_COLOR(BG_COLOR(col),BG_COLOR(col)): COLOR_TRANSPARENT, RECT_BORDER1|DRAW_FILLED);
        is_drawn = 0;   // prevent erasing once already erased
    }
    
    if (visible)
    {
        is_drawn = 1;

        int i;
        for (i = 0; i < 9; i += 1)
        {
            draw_vline(x0+2+i*8,   y0+12-(i&1 ? 5 : 10), i&1 ? 6 : 11, col);
            draw_vline(x0+2+i*8+1, y0+12-(i&1 ? 5 : 10), i&1 ? 6 : 11, col);
        }

        x0 += (8 * get_ev_video() + 32);

        static int llen[] = { 4, 5, 7, 7, 5, 4};
    
        for (i = 0; i < 6; i += 1)
        {
            draw_vline(x0+i,y0+23-llen[i],llen[i],col);
        }
    }
}
#endif

#endif  // !CAM_PERSISTENT_OSD - end of the stock OSD element functions

//-------------------------------------------------------------------
static void set_fast_ev(int direction)
{
    int ev = shooting_get_ev_correction1() + ((conf.fast_ev_step + 1) * 16 * direction);

    shooting_set_prop(PROPCASE_EV_CORRECTION_1, ev);
    shooting_set_prop(PROPCASE_EV_CORRECTION_2, ev);

    EnterToCompensationEVF();
}

// Process up/down/left/right/jogdial shortcuts when control options enabled
static int kbd_use_up_down_left_right_as_fast_switch()
{
    static long key_pressed = 0;

#if CAM_VIDEO_CONTROL
    int ev_video = 0;
#if CAM_EV_IN_VIDEO
    ev_video = get_ev_video_avail(); 
#endif
#endif

    // One of the control options must be enabled or don't do anything
    if (!conf.fast_ev && !conf.fast_movie_control && !conf.fast_movie_quality_control) return 0;

    // Clear state variable is neither UP or DOWN is pressed
    if (!kbd_is_key_pressed(KEY_UP) && !kbd_is_key_pressed(KEY_DOWN)) key_pressed = 0;

    // Must be in record mode and not have either Canon menu open
    if ( (canon_menu_active!=(int)&canon_menu_active-4) || canon_shoot_menu_active!=0 || !camera_info.state.mode_rec) return 0;

    // Adjust exposure if 'Enable Fast EV switch?' option is set
    if (conf.fast_ev && (key_pressed == 0) && (camera_info.state.mode_shooting != MODE_M))
    {
#if !CAM_HAS_JOGDIAL
        if (kbd_is_key_pressed(KEY_UP))
        {
            set_fast_ev(1);
            key_pressed = KEY_UP;
            return 1;
        } 

        if (kbd_is_key_pressed(KEY_DOWN))
        {
            set_fast_ev(-1);
            key_pressed = KEY_DOWN;
            return 1;
        }
#else
        int jogdial=get_jogdial_direction();

        if (camera_info.state.is_shutter_half_press && (jogdial==JOGDIAL_RIGHT))
        {
            set_fast_ev(1);
        }

        if (camera_info.state.is_shutter_half_press && (jogdial==JOGDIAL_LEFT))
        {
            set_fast_ev(-1);
        }
#endif
    }

    // Adjust video quality/bitrate if 'Video Quality Control?' option is set
#ifndef CAM_MOVIEREC_NEWSTYLE
    if (conf.fast_movie_quality_control && key_pressed == 0 && is_video_recording())
    {
        if (kbd_is_key_pressed(KEY_UP))
        {
            if (conf.video_mode==0)
            {
#if !CAM_VIDEO_QUALITY_ONLY
                gui_video_bitrate_enum(1,0);
                movie_reset = 1;
#endif
            }    
            else if (conf.video_mode==1)
            {
                conf.video_quality+=1;
                if (conf.video_quality>VIDEO_MAX_QUALITY)
                    conf.video_quality=VIDEO_MAX_QUALITY;
                movie_reset = 1;
            }              
            key_pressed = KEY_UP;
            return 1;
        }
    
        if (kbd_is_key_pressed(KEY_DOWN))
        {
            if (conf.video_mode==0)
            {
#if !CAM_VIDEO_QUALITY_ONLY
                conf.video_bitrate-=1;
                if (conf.video_bitrate<0)
                    conf.video_bitrate=0;

                shooting_video_bitrate_change(conf.video_bitrate);
                movie_reset = 1;
#endif
            }
            else if (conf.video_mode==1)
            {
                conf.video_quality-=1;
                if (conf.video_quality<1)
                    conf.video_quality=1;
                movie_reset = 1;
            }          
            key_pressed = KEY_DOWN;
            return 1;
        }
    }
#endif // !CAM_MOVIEREC_NEWSTYLE
    
#if CAM_VIDEO_CONTROL
    // Pause / unpause video if 'Fast Movie Control' option is set
    if (conf.fast_movie_control && key_pressed == 0 && !ev_video
#ifndef CAM_HAS_VIDEO_BUTTON 
        && (camera_info.state.mode_video || is_video_recording())
#endif
        )
    {
        if (kbd_is_key_pressed(KEY_LEFT) && is_video_recording())
        {
            set_movie_status(1); // set to stop
            key_pressed = KEY_LEFT;
            return 1;
        }

        // reyalp - HACK for cams that can do video in any mode
        // note that this means this will probably run whenever you press right
        // BUG this doesn't know whether recording was stopped or paused.
        if (kbd_is_key_pressed(KEY_RIGHT) && (get_movie_status() == VIDEO_RECORD_STOPPED))
        {
            set_movie_status(2); // resume
            movie_reset = 1;
            key_pressed = KEY_RIGHT;
            return 1;
        }
    } 
#endif

    return key_pressed;
}

//------------------------------------------------------------------- 
// Process Shutter Half Press + BUTTON shortcuts
static int half_disp_press = 0;

static void kbd_shortcut(int button, int *var, int max_value)
{
    if (kbd_is_key_clicked(button))
    {
        (*var)++;
        if (*var > max_value)
        {
            *var = 0;
            gui_set_need_restore();
        }
    }
}

void gui_kbd_shortcuts()
{
    static int half_disp_press_old=0;
    
    if (camera_info.state.is_shutter_half_press)
    {
        if (conf.enable_shortcuts == 1)
        {
            kbd_shortcut(SHORTCUT_TOGGLE_ZEBRA, &conf.zebra_draw, 1);
            kbd_shortcut(SHORTCUT_TOGGLE_HISTO, &conf.show_histo, SHOW_HISTO_HALF);
            kbd_shortcut(SHORTCUT_TOGGLE_OSD, &conf.show_osd, 1);
            kbd_shortcut(SHORTCUT_DISABLE_OVERRIDES, &conf.override_disable, 1);
        }
#if !CAM_HAS_MANUAL_FOCUS && CAM_HAS_ZOOM_LEVER
        // Todo, check for AF and if its running, don't override
        if (kbd_is_key_pressed(SHORTCUT_SD_SUB)) {
            if (!(conf.override_disable==1) && shooting_can_focus() && shooting_get_common_focus_mode()) {
                gui_subj_dist_override_value_enum(-1,0);
                shooting_set_focus(shooting_get_subject_distance_override_value(),SET_NOW);
            }
        } else if (kbd_is_key_pressed(SHORTCUT_SD_ADD)) {
            if (!(conf.override_disable==1) && shooting_can_focus() && shooting_get_common_focus_mode()) {
                gui_subj_dist_override_value_enum(1,0);
                shooting_set_focus(shooting_get_subject_distance_override_value(),SET_NOW);
            }
        }
#endif
    }

    half_disp_press = camera_info.state.mode_photo && camera_info.state.is_shutter_half_press && kbd_is_key_pressed(KEY_DISPLAY);
    if (half_disp_press && !half_disp_press_old)
        gui_set_need_restore();
#ifdef CAM_DISP_ALT_TEXT
    if (half_disp_press)
    {
        extern void gui_reset_alt_helper();
        gui_reset_alt_helper();
    }
#endif
    half_disp_press_old = half_disp_press;
}

//------------------------------------------------------------------- 
// Handler for button presses in normal camera shooting or playback modes
static int gui_std_kbd_process()
{
#ifdef CAM_USE_ZOOM_FOR_MF
    if (conf.use_zoom_mf && kbd_use_zoom_as_mf())
        return 1;
#endif 

    if (kbd_use_up_down_left_right_as_fast_switch())
        return 1;

    // Process Shutter Half Press + BUTTON shortcuts
    gui_kbd_shortcuts();

    // process other keys in not <alt> mode

#if CAM_AF_SCAN_DURING_VIDEO_RECORD 
    if (is_video_recording())
        if (kbd_is_key_clicked(conf.video_af_key)) MakeAFScan(); 
#endif 

#if CAM_CAN_UNLOCK_OPTICAL_ZOOM_IN_VIDEO
    // return from digital to optical zoom in video
#if CAM_HAS_ZOOM_LEVER
    if (conf.unlock_optical_zoom_for_video && is_video_recording() &&  kbd_is_key_clicked(KEY_ZOOM_OUT))
#else
    if (conf.unlock_optical_zoom_for_video && is_video_recording() &&  kbd_is_key_clicked(KEY_DOWN))
#endif
    {
        short x;
        // state = 1 => digital zoom standard
        if (shooting_get_digital_zoom_state())
        {
            get_property_case(PROPCASE_DIGITAL_ZOOM_POSITION, &x, sizeof(x));
#if defined(CAM_USE_OPTICAL_MAX_ZOOM_STATUS)
            if (x==0) zoom_status=ZOOM_OPTICAL_MAX; //ERR99: No zoom back from digital to optical zoom possible if set to medium
#else
            if (x==0) zoom_status=ZOOM_OPTICAL_MEDIUM;
#endif
        }
    }
#endif

#if CAM_EV_IN_VIDEO
    if (is_video_recording() && !camera_info.state.is_shutter_half_press)
    {
#if CAM_HAS_ERASE_BUTTON
        if (kbd_is_key_clicked(KEY_ERASE))
#elif CAM_HAS_DISP_BUTTON
        if (kbd_is_key_clicked(KEY_DISPLAY))
#else
        if (kbd_is_key_clicked(KEY_MENU))
#endif
        {
            set_ev_video_avail(!get_ev_video_avail());
#ifdef CAM_TOUCHSCREEN_UI
            redraw_buttons = 1;
#endif
        }
        if (get_ev_video_avail())
        {
            if (kbd_is_key_clicked(KEY_LEFT))
            {
                set_ev_video(get_ev_video()-1);
            }
            if (kbd_is_key_clicked(KEY_RIGHT))
            {
                set_ev_video(get_ev_video()+1);
            }
        }
    }
#endif

    return 0;
}

//-------------------------------------------------------------------
// int osd_visible()
//    conf.hide_osd =  0=Don't, 1=In Playback, 2=On Disp Press, 3=Both
//-------------------------------------------------------------------
int osd_visible()
{
    if ( conf.show_osd )
    {
        if ( conf.hide_osd == 0 ) return 1;

        if (!camera_info.state.is_shutter_half_press)
        {
            if (camera_info.state.mode_rec)
            {
                if ( conf.hide_osd < 2 ) return 1;

#if defined(PARAM_DISPLAY_MODE1) && defined(PARAM_DISPLAY_MODE2)
                short disp_key_mode ;
                if (recreview_hold == 0)
                {
                    if (shooting_get_display_mode() == 0) return 1;
                }
                else
                {
                    if (conf.show_osd_in_review)
                    {
                        get_parameter_data(PARAM_DISPLAY_MODE2, &disp_key_mode, 2);
                        if (disp_key_mode == 0) return 1;
                    }
                }
            }
            else
            {
                if (conf.hide_osd == 2 )
                {
                    short disp_key_mode ;
                    get_parameter_data(PARAM_DISPLAY_MODE1, &disp_key_mode, 2);
                    if (disp_key_mode == 1) return 1;
                }
            }
#else
                if (shooting_get_display_mode() == 0) return 1;
            }
#endif
        }
    }

    return 0;
}

//-------------------------------------------------------------------

#ifdef OPT_DEBUGGING

extern int debug_display_direction;

#ifndef CAM_DRYOS
extern int debug_tasklist_start;
#endif

#define TASKLIST_MAX_LINES 12 // probably as much as will fit on screen
#define TASKLIST_NUM_TASKS 64 // should be enough ?

#ifndef CAM_DRYOS
static void gui_debug_draw_tasklist(void)
{
    int tasklist[TASKLIST_NUM_TASKS]; // max number of tasks we will look at
    int n_tasks,n_show_tasks,show_start;
    const char *name;
    int i;
    n_tasks = task_id_list_get(tasklist,sizeof(tasklist)/sizeof(tasklist[0]));
    show_start = debug_tasklist_start;
    n_show_tasks = n_tasks - show_start;
    // auto adjust to show the last N tasks
    if(n_show_tasks < TASKLIST_MAX_LINES) {
        show_start = n_tasks - TASKLIST_MAX_LINES;
        if(show_start<0)
            show_start = 0;
        n_show_tasks = n_tasks - show_start;
    }
    else if( n_show_tasks > TASKLIST_MAX_LINES ) {
        n_show_tasks = TASKLIST_MAX_LINES;
    }
    sprintf(osd_buf,"%d-%d of %d tasks %c",show_start,show_start+n_show_tasks,n_tasks,debug_display_direction > 0?'+':'-');
    draw_string(64,0,osd_buf, user_color(conf.osd_color));
    for( i = 0;  i < n_show_tasks; i++ ) {
        // TODO get full task info
        name = task_name(tasklist[show_start+i]);
        if ( !name || !*name ) {
            name = "(unknown)";
        }
        sprintf(osd_buf,"%10s %8X",name,tasklist[show_start+i]);
        draw_string(64,FONT_HEIGHT+FONT_HEIGHT*i,osd_buf, user_color(conf.osd_color));
    }
}
#endif //CAM_DRYOS

#endif

//------------------------------------------------------------------- 
void gui_draw_debug_vals_osd()
{
#ifdef OPT_DEBUGGING

#if defined(OPT_EXMEM_TESTING)
    // Only do memory corruption testing if not recording video
    if (!camera_info.state.mode_video)
    {
        extern void exmem_testing();
        exmem_testing();
    }
#endif

#if defined(OPT_ARAM_TESTING)
    extern void aram_testing();
    aram_testing();
#endif

    twoColors col = user_color(conf.osd_color);

    int DBGMISCVALS_X = ((camera_screen.width/FONT_WIDTH)-17);
    int DBGMISCVALS_Y = ((camera_screen.height/FONT_HEIGHT)-6);

    // DEBUG: "Show misc. values"
    // change ROW to fit values on screen in draw_txt_string(COLUMN, ROW, ...)
    // uncomment call to gui_draw_debug_vals_osd() in gui_redraw() if you want debug values always on top
    if (conf.debug_misc_vals_show) {

#ifdef OPT_VIEWPORT_DEBUG
        {
            extern char avb_history[32];
            extern unsigned char avb_times[32];
            unsigned avbtsum = 0;
            int n;
            for (n=0; n<32; n++) {
                avbtsum += avb_times[n];
                osd_buf[n] = avb_history[n]+'0';
            }
            sprintf(osd_buf+24, " FPS x 10: %3u",avbtsum?320000/avbtsum:0);
            draw_txt_string(DBGMISCVALS_X-25,  DBGMISCVALS_Y-1, osd_buf, col);
        }
#endif

        // show value of Memory Address selected with Memory Browser
        sprintf(osd_buf, "MEM: %#8x", (void*) (*(int*)conf.mem_view_addr_init));    // show value in Hexadecimal integer
        //sprintf(osd_buf, "MEM: %8u", (void*) (*(int*)conf.mem_view_addr_init));    // show value in Decimal integer
        draw_txt_string(DBGMISCVALS_X,  DBGMISCVALS_Y, osd_buf, col);

        // show Autofocus status (if AF is working)
        extern volatile long focus_busy;
        sprintf(osd_buf, "FB:  %8u", focus_busy);
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+1, osd_buf, col);

        // show Zoom status (if Lens is moving)
        extern volatile long zoom_busy;
        sprintf(osd_buf, "ZB:  %8u", zoom_busy);
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+2, osd_buf, col);

        // show USB-Power status to debug remote / sync
        sprintf(osd_buf, "USB: %8u", get_usb_power(1));
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+3, osd_buf, col);

        /*
        // some cameras missing zoom_status
        sprintf(osd_buf, "ZS:  %#8x", zoom_status);
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+4, osd_buf, col);
        */

        /*
        sprintf(osd_buf, "VP:  %#8x", vid_get_viewport_active_buffer());
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+5, osd_buf, col);
        */

        /*
        // debug keymap, KEYS_MASKx, SD_READONLY_FLAG, USB_MASK
        extern long physw_status[3];
        sprintf(osd_buf, "PS1: %#8x", physw_status[0]);
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+1, osd_buf, col);

        sprintf(osd_buf, "PS2: %#8x", physw_status[1]);
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+2, osd_buf, col);

        sprintf(osd_buf, "PS3: %#8x", physw_status[2]);
        draw_txt_string(DBGMISCVALS_X, DBGMISCVALS_Y+3, osd_buf, col);
        */

        /*
        // user friendlier keymap display with separated bits, grouped by hex digits
        // comment out the above FB, ZB and USB related lines when using this
        extern long physw_status[3];
        #define BITDISP(str, val) { unsigned bdi; \
                            char dig[4] = {'1','2','4','8'}; \
                            for(bdi=0; bdi<32; bdi++) { \
                              str[31-bdi] = (val&(1<<bdi))?dig[bdi&3]:'0'; \
                            } \
                            str[32] = 0; \
                          }
        draw_txt_string(DBGMISCVALS_X-20, DBGMISCVALS_Y+1, "     [ 7][ 6][ 5][ 4][ 3][ 2][ 1][ 0]", col);
        strcpy(osd_buf, "PS1: ");
        BITDISP((osd_buf+5), physw_status[0])
        draw_txt_string(DBGMISCVALS_X-20, DBGMISCVALS_Y+2, osd_buf, col);

        strcpy(osd_buf, "PS2: ");
        BITDISP((osd_buf+5), physw_status[1])
        draw_txt_string(DBGMISCVALS_X-20, DBGMISCVALS_Y+3, osd_buf, col);

        strcpy(osd_buf, "PS3: ");
        BITDISP((osd_buf+5), physw_status[2])
        draw_txt_string(DBGMISCVALS_X-20, DBGMISCVALS_Y+4, osd_buf, col);
        */

        /*
        long v=get_file_counter();
        sprintf(osd_buf, "1:%03d-%04d", (v>>18)&0x3FF, (v>>4)&0x3FFF);
        sprintf(osd_buf, "1:%d, %08X", xxxx, eeee);
        */
    }
    {
        int r,i, p, len;
        if (conf.debug_display == DEBUG_DISPLAY_PROPS){

            for (i=0;i<10;i++){
                r = 0;
                p = conf.debug_propcase_page*10+i;
                get_property_case(p, &r, 4);
                sprintf(osd_buf, "%3d: %d              ", p, r);
                osd_buf[20]=0;
                draw_string(64,FONT_HEIGHT+FONT_HEIGHT*i,osd_buf, col);
            }
        }

        if (conf.debug_display == DEBUG_DISPLAY_PARAMS){
            char s[30];
            int count;

            for (i=0;i<10;i++){
                r = 0;
                p = conf.debug_propcase_page*10+i;
                if (p>=get_flash_params_count()) {
                    sprintf(osd_buf, "%3d: This parameter does not exists", p);
                } else  {
                    len = get_parameter_size(p);
                    if ((len==1)||(len==2)||(len==4)){
                        get_parameter_data(p, &r, len); 
                        sprintf(osd_buf, "%3d: %30d :%2d ", p, r,len);
                    }
                    else {
                        if (len>=(int)sizeof(s)) count=sizeof(s)-1; else count=len;
                        get_parameter_data(p, &s, count);
                        s[count]=0;
                        sprintf(osd_buf, "%3d: %30s :%2d ", p, s,len);
                    }
                }
                draw_string(16,FONT_HEIGHT+FONT_HEIGHT*i,osd_buf, col);
            }
        }
        if (conf.debug_display == DEBUG_DISPLAY_UIPROPS){

            for (i=0;i<10;i++){
                p = conf.debug_propcase_page*10+i;
                if (p>=uiprop_count) {
                    sprintf(osd_buf, "%3d: Does not exist ", p);
                } else  {
                    r = get_uiprop_value(p);
                    sprintf(osd_buf, "%3d: %hi               ", p, r);
    }
                osd_buf[20]=0;
                draw_string(64,FONT_HEIGHT+FONT_HEIGHT*i,osd_buf, col);
            }
        }
    }

#ifndef CAM_DRYOS
    if(conf.debug_display == DEBUG_DISPLAY_TASKS)
            gui_debug_draw_tasklist();
#endif
#endif
}

// Update displayed debug page for tasks/props/params
#ifdef OPT_DEBUGGING
void gui_update_debug_page()
{
#ifndef CAM_DRYOS
    if(conf.debug_display == DEBUG_DISPLAY_TASKS)
    {
        debug_tasklist_start += debug_display_direction*(TASKLIST_MAX_LINES-2); // a little intentional overlap
        if(debug_tasklist_start >= TASKLIST_NUM_TASKS || debug_tasklist_start < 0)
            debug_tasklist_start = 0;
    }
    else 
#endif
    if (conf.debug_display == DEBUG_DISPLAY_PROPS || conf.debug_display == DEBUG_DISPLAY_PARAMS
        || conf.debug_display == DEBUG_DISPLAY_UIPROPS)
    {
        conf.debug_propcase_page += debug_display_direction*1;
        if(conf.debug_propcase_page > 128 || conf.debug_propcase_page < 0) 
            conf.debug_propcase_page = 0;
    }
}
#endif

//-------------------------------------------------------------------
// void gui_draw_osd()
//      Common OSD display code
//-------------------------------------------------------------------
#ifdef CAM_OSD_FORCE_DRAW_IN_SPYTASK
// Per-element timings. spytask reports the slowest one, so a single test names
// whichever OSD element is blocking the loop for ~a second.
int dbg_osd_worst = -1, dbg_osd_worst_ms = 0;
#define OSD_TIME(idx, call)                                     \
    do {                                                        \
        int _t0 = get_tick_count();                             \
        call;                                                   \
        int _ms = get_tick_count() - _t0;                        \
        if (_ms > dbg_osd_worst_ms) {                           \
            dbg_osd_worst_ms = _ms; dbg_osd_worst = (idx);      \
        }                                                       \
    } while (0)
#else
#define OSD_TIME(idx, call) call
#endif

#ifndef CAM_PERSISTENT_OSD
void gui_draw_osd_elements(int is_osd_edit, int is_zebra)
{
    if (!is_osd_edit)
        OSD_TIME(1, libgrids->gui_grid_draw_osd(is_zebra));

    OSD_TIME(2, gui_osd_draw_dof(is_osd_edit));
    OSD_TIME(3, gui_osd_draw_values(is_osd_edit,is_zebra));

    if (is_osd_edit || is_zebra || osd_visible())
    {
        OSD_TIME(4, gui_osd_draw_state(is_osd_edit));
        OSD_TIME(5, gui_osd_draw_raw_info(is_osd_edit));
        OSD_TIME(6, gui_batt_draw_osd(is_osd_edit));
        OSD_TIME(7, gui_space_draw_osd(is_osd_edit));
        OSD_TIME(8, gui_osd_draw_temp(is_osd_edit));
        OSD_TIME(9, gui_osd_draw_clock(0,0,MAKE_COLOR(0,0),is_osd_edit));
        if (!is_zebra)
        {
            OSD_TIME(10, gui_usb_draw_osd(is_osd_edit));
            OSD_TIME(11, gui_osd_draw_ev(is_osd_edit));
        }
    }

#if CAM_EV_IN_VIDEO
    if (!is_zebra)
        gui_osd_draw_ev_video(is_osd_edit);
#endif
}
#else   // CAM_PERSISTENT_OSD

// The stock OSD is compiled out above, but these two are in
// modules/module_exportlist.c, which is a fixed ABI shared by every camera's
// modules - the hash table generated from it references them by name whether
// this build has them or not. So they stay, as stubs.
//
// The only caller is modules/gui_osd_edit.c, the OSD layout editor, and doing
// nothing is the honest answer there: with CAM_PERSISTENT_OSD there are no
// stock elements to lay out. The editor's own menu entry should go too.
void gui_draw_osd_elements(int is_osd_edit, int is_zebra)
{
    (void)is_osd_edit; (void)is_zebra;
}

void gui_osd_draw_clock(int x, int y, twoColors cl, int is_osd_edit)
{
    (void)x; (void)y; (void)cl; (void)is_osd_edit;
}

#endif  // !CAM_PERSISTENT_OSD

#ifdef CAM_SCREEN_LOCK_DEBUG
int dbg_osd_draws = 0;      // TEMPORARY - counted per second in spytask
int dbg_dd_enter  = 0;      // gui_default_draw() entries
int dbg_dd_hdp    = 0;      // ...exited on half_disp_press
int dbg_dd_gate   = 0;      // ...exited on the show_osd / canon_menu_active gate
int dbg_dd_noosd  = 0;      // ...of which were conf.show_osd being off
int dbg_cm_ok     = 0;      // canon_menu_active held the "no menu" sentinel
#endif

void gui_draw_osd()
{
#ifdef CAM_PERSISTENT_OSD
    // Replaced by gui_draw_persistent_osd() in gui.c. Returning here is the point
    // of the exercise, not a side effect: one of the element functions below
    // blocks spytask for most of a second on this camera, which is what stopped
    // the overlay being repainted often enough to stay on screen.
    //
    // The body is now #ifdef'd out rather than just returned past, because the
    // functions it calls are no longer compiled at all on this path.
}
#else
    if (half_disp_press)
        return;

#ifdef CAM_SCREEN_LOCK_DEBUG
    dbg_osd_draws++;
#endif

    OSD_TIME(0,  libhisto->gui_osd_draw_histo(0));
    gui_draw_osd_elements(0,0);

    OSD_TIME(12, gui_osd_draw_movie_time_left());

    OSD_TIME(13, gui_draw_debug_vals_osd());
}
#endif  // CAM_PERSISTENT_OSD

// GUI handler for normal shooting / playback modes
static void gui_default_draw(int force_redraw)
{
#if CAM_SWIVEL_SCREEN
    static int flashlight = 0;

    if (conf.flashlight && (camera_info.state.mode&MODE_SCREEN_OPENED) && (camera_info.state.mode&MODE_SCREEN_ROTATED))
    {
        flashlight = 1;
        draw_rectangle(0, 0, camera_screen.width-1, camera_screen.height-1, MAKE_COLOR(COLOR_WHITE, COLOR_WHITE), RECT_BORDER0|DRAW_FILLED);
        return;
    }
    else if (flashlight)
    {
        flashlight = 0;
        gui_set_need_restore();
        return;
    }
#endif

#ifdef CAM_SCREEN_LOCK_DEBUG
    // Count entries and each early exit, so we can see exactly which test is
    // throwing away 24 of every 25 passes instead of guessing at it.
    dbg_dd_enter++;
    if (canon_menu_active == (int)&canon_menu_active-4) dbg_cm_ok++;
    if (half_disp_press) dbg_dd_hdp++;
#endif

    if (half_disp_press)
        return;

    if (conf.zebra_draw && (canon_menu_active==(int)&canon_menu_active-4))
        if (libzebra->gui_osd_draw_zebra((conf.zebra_draw >= 1 && camera_info.state.is_shutter_half_press && camera_info.state.mode_photo)
            || (conf.zebra_draw == 2 && camera_info.state.mode_rec)
            || (conf.zebra_draw == 3)))
            return; // if zebra drawn, we're done

    // The gallery belongs to Canon. RAW EV's old playback mode drew over the
    // reviewed photograph and made it look like the persistent shooting OSD
    // had leaked through; always erase it outside record mode.
    librawevhisto->erase();

#ifdef CAM_SCREEN_LOCK_DEBUG
    if (!conf.show_osd) dbg_dd_noosd++;
    if (!(conf.show_osd && (canon_menu_active==(int)&canon_menu_active-4))) dbg_dd_gate++;
#endif

#ifdef CAM_OSD_ALWAYS_DRAW
    // Draw the OSD the same way <ALT> mode does - unconditionally. gui_chdk_draw()
    // calls gui_draw_osd() with no gating at all, which is exactly why the CHDK
    // menu sits on screen rock solid while this path flickers.
    //
    // The canon_menu_active test below is meant to hide the CHDK OSD while a Canon
    // menu is open. It depends on a Canon RAM variable whose address is guessed by
    // the signature finder, and on the a470 it almost never reads as "no menu":
    // measured on hardware, gui_draw_osd() ran once per second instead of ~25
    // times. So the overlay was never being erased - it was barely being drawn.
    //
    // Trading the hide-behind-Canon-menus behaviour for an OSD that actually
    // stays on screen. Cameras that already show the OSD over Canon's shooting
    // menu (this one does, see CAM_SHOW_OSD_IN_SHOOT_MENU) lose very little.
    if (!conf.show_osd) return;
#elif !CAM_SHOW_OSD_IN_SHOOT_MENU
    if (!(conf.show_osd && (canon_menu_active==(int)&canon_menu_active-4) && (canon_shoot_menu_active==0))) return;
#else
    if (!(conf.show_osd && (canon_menu_active==(int)&canon_menu_active-4) /*&& (canon_shoot_menu_active==0)*/ )) return;
#endif

    gui_draw_osd();

#if CAM_DRAW_EXPOSITION
    if (camera_info.state.is_shutter_half_press && camera_info.state.mode_rec && camera_info.state.mode_shooting!=MODE_VIDEO_STD && camera_info.state.mode_shooting!=MODE_VIDEO_COMPACT)
    {
        extern char* shooting_get_tv_str();
        extern char* shooting_get_av_str();

        strcpy(osd_buf,shooting_get_tv_str());
        strcat(osd_buf,"\"  F");
        strcat(osd_buf,shooting_get_av_str());
        draw_txt_string(22-strlen(osd_buf)/2, 14, osd_buf, user_color(conf.osd_color));
    }
#endif

    if (force_redraw && camera_info.state.state_kbd_script_run)
        libscriptapi->refresh_display();

    if (conf.console_show)
        console_draw(force_redraw);
}

//-------------------------------------------------------------------
// GUI/KBD handlers - Canon modes (not in CHDK <ALT> mode, menu etc)
gui_handler defaultGuiHandler = { GUI_MODE_NONE, gui_default_draw, gui_std_kbd_process, 0, 0, 0 };
//-------------------------------------------------------------------
