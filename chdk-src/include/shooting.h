#ifndef SHOOTING_H
#define SHOOTING_H

// CHDK shooting interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

/******************************************************************/

extern void shooting_init(); // startup initialization

/******************************************************************/

#define SET_NOW      1
#define SET_LATER    0

/******************************************************************/

typedef struct {
    short id;       // hacks id
    short prop_id;  // Canons id
    char *name;
    short shutter_dfs_value; // shutter speed to use dark frame subtraction
} ISOTable;

typedef struct {
    short id;       // hacks id
    short prop_id;  // Canons id
    char *name;
    long usec;
} ShutterSpeed;

typedef struct {
    short id;       // hacks id
    short prop_id;  // Canons id
    char *name;
} ApertureSize;

typedef struct {
    int subj_dist;
    int dsubj_dist;
    int subj_dist_step;
    short av96;
    short dav96;
    short av96_step;
    short tv96;
    short dtv96;
    short tv96_step;
    short sv96;
    short dsv96;
    short sv96_step;
    short iso;
    short diso;
    short iso_step;
    short shoot_counter;
} EXPO_BRACKETING_VALUES;

/******************************************************************/

extern int shooting_in_progress();
extern int shooting_is_flash();
extern int shooting_is_flash_ready();
extern short shooting_get_flash_mode();

/******************************************************************/

// returned by shooting_get_tv96_from_shutter_speed for invalid input
// and get_current_tv96 when imager not active
#define SHOOTING_TV96_INVALID -10000

extern int shooting_get_user_tv_id();
extern short shooting_get_tv96();
extern short shooting_get_tv96_from_shutter_speed(float t);
extern void shooting_set_tv96(short tv96, short is_now);
extern void shooting_set_tv96_direct(short tv96, short is_now);
extern short shooting_get_user_tv96();
extern void shooting_set_user_tv96(short tv96);
extern float shooting_get_shutter_speed_from_tv96(short tv96);
extern void shooting_set_user_tv_by_id(int v);
extern void shooting_set_user_tv_by_id_rel(int v);

/******************************************************************/

extern short shooting_get_aperture_sizes_table_size();
extern int shooting_get_aperture_from_av96(short av96);
extern short shooting_get_av96_from_aperture(int aperture);
extern int shooting_get_user_av_id();
extern void shooting_set_user_av_by_id(int v);
extern short shooting_get_av96();
extern short shooting_get_min_av96();
extern short shooting_get_max_av96();
extern void shooting_set_av96(short av96,short is_now);
extern void shooting_set_av96_direct(short av96, short is_now);
extern short shooting_get_user_av96();
extern void shooting_set_user_av96(short av96);
extern void shooting_set_user_av_by_id_rel(int v);
extern int shooting_get_real_aperture();
extern short shooting_get_av96_override_value();
extern void shooting_set_nd_filter_state(short v, short is_now);
extern short shooting_get_nd_value_ev96(void);
extern short shooting_get_nd_current_ev96(void);

/******************************************************************/

extern short shooting_get_is_mode();
extern short shooting_get_resolution();
extern short shooting_get_display_mode();
#define SHOOTING_CANON_FMT_JPG  1
#define SHOOTING_CANON_FMT_RAW  2
extern int shooting_get_canon_image_format();
extern int shooting_set_canon_image_format(int fmt);

/******************************************************************/

extern int shooting_get_zoom();
extern void shooting_set_zoom(int v);
extern void shooting_set_zoom_rel(int v);
extern void shooting_set_zoom_speed(int v);

// mode: 0 = off or standard, 2 or 3 digital tele
int shooting_get_digital_zoom_mode(void);
// state: 0 = off or digital tele, 1 = standard
int shooting_get_digital_zoom_state(void);

/******************************************************************/

extern void shooting_set_focus(int v, short is_now);
extern short shooting_get_focus_mode();
extern short shooting_get_real_focus_mode();
extern short shooting_get_focus_state();
extern short shooting_get_focus_ok();
extern void shooting_update_dof_values();
extern int shooting_get_hyperfocal_distance();
extern int shooting_get_hyperfocal_distance_1e3_f(int av, int fl);
extern int shooting_get_exif_subject_dist();
extern int shooting_get_subject_distance();
extern int shooting_get_subject_distance_override_value();
extern int shooting_get_lens_to_focal_plane_width();
extern short shooting_get_drive_mode();
extern short shooting_can_focus();
extern short shooting_get_common_focus_mode();
extern short shooting_is_infinity_distance();
extern void set_focus_bypass(int);

/******************************************************************/

extern int shooting_get_iso_mode();
extern void shooting_set_iso_mode(int v);
extern short shooting_get_sv96_real();
extern short shooting_get_sv96_market();
extern short shooting_get_iso_market();
extern short shooting_get_iso_real();
extern void shooting_set_iso_real(short iso, short is_now);
extern void shooting_set_sv96(short sv96, short is_now);
extern short shooting_get_iso_override_value();

extern short shooting_iso_market_to_real(short isom);
extern short shooting_iso_real_to_market(short isor);
extern short shooting_sv96_market_to_real(short isom);
extern short shooting_sv96_real_to_market(short isor);
extern short shooting_get_sv96_from_iso(short iso);
extern short shooting_get_iso_from_sv96(short sv96);

/******************************************************************/

extern short shooting_get_canon_overexposure_value();
extern short shooting_get_bv96();
extern int shooting_get_luminance();

extern short shooting_get_ev_correction1();
extern short shooting_get_ev_correction2();

/******************************************************************/

extern void shooting_expo_param_override();
extern void shooting_bracketing(void);

/******************************************************************/

extern void DoAELock(void);
extern void UnlockAE(void);

extern int EngDrvRead(int gpio_reg);

extern void PutInNdFilter();
extern void PutOutNdFilter();
extern long shooting_get_current_av96();
extern long shooting_get_current_tv96();
extern long shooting_get_current_delta_sv96();
extern long shooting_get_current_base_sv96();
extern long IsStrobeChargeCompleted();
extern void SetCurrentCaptureModeType();
extern int shooting_get_imager_active();
extern int shooting_get_analog_video_standard(void);
extern int get_ev_video_avail(void);
extern void set_ev_video_avail(int);
extern int get_ev_video(void);
extern void set_ev_video(int);

/******************************************************************/

// capture mode functions
// return a CHDK mode enum for a PROPCASE_SHOOTING_MODE value, or 0 if not found
extern int shooting_mode_canon2chdk(int canonmode);
// return a PROPCASE_SHOOTING_MODE value for a CHDK mode enum value, or -1 if not found. 0 is a valid mode on some cameras.
extern int shooting_mode_chdk2canon(int hackmode);
// set capture mode from CHDK mode, returns 0 if mode not found or not in rec, otherwise 1
extern int shooting_set_mode_chdk(int mode);
// set capture mode from PROPCASE_SHOOTING_MODE value. return 0 if not in rec or mode is -1, otherwise 1
extern int shooting_set_mode_canon(int mode);

// set switch between play (0) / rec (1) modes, using USB function if needed
void shooting_set_playrec_mode(int mode);

/******************************************************************/

#define INFINITY_DIST               0xFFFFFFFF          // Value to send to firmware to select 'infinity' focus

int get_focal_length(int zp);
int get_effective_focal_length(int zp);
int get_zoom_x(int zp);

/******************************************************************/

// Video recording current status constants, see movie_status variable 
// values actually used in pre-DIGIC 6 firmware
#define VIDEO_RECORD_NEVER_STARTED  0  
#define VIDEO_RECORD_STOPPED        1  
#define VIDEO_RECORD_IN_PROGRESS    4
#define VIDEO_RECORD_STOPPING       5

//Optical & digital zoom status constants, see zoom_status variable 
#define ZOOM_OPTICAL_MIN            1
#define ZOOM_OPTICAL_MAX            2
#define ZOOM_OPTICAL_MEDIUM         3
//#define ZOOM_OPTICAL_IN             4
//#define ZOOM_OPTICAL_OUT            5
//#define ZOOM_OPTICAL_REACHED_MIN    7

//#define ZOOM_DIGITAL_MAX            9
//#define ZOOM_DIGITAL_MEDIUM         10
//#define ZOOM_DIGITAL_IN             11
//#define ZOOM_DIGITAL_OUT            12
//#define ZOOM_DIGITAL_REACHED_MAX    13

// return whether video is actually being recorded
extern int is_video_recording();
// set movie status, not effective on CAM_SIMPLE_MOVIE_STATUS cams
extern void set_movie_status(int status);
// get movie status, emulated on CAM_SIMPLE_MOVIE_STATUS cams
extern int get_movie_status();


extern void change_video_tables(int a, int b);
extern void shooting_video_bitrate_change(int v);

// for CAM_MOVIEREC_NEWSTYLE:
void shooting_video_minbitrate_change(int);
unsigned int shooting_get_video_recorded_size_kb();
extern void change_video_minbitrate(int, int);
extern unsigned int get_video_recorded_size_kb();

extern int zoom_status;
extern const int zoom_points;
extern int recreview_hold;

/******************************************************************/

#endif

