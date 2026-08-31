#ifndef CAMERA_INFO_H
#define CAMERA_INFO_H

// CHDK camera info interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

#include "stdlib.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"

//==========================================================

// For Depth of Field, and Hyperfocal calculations
typedef struct
{
    short hyperfocal_valid;
    short distance_valid;
    int   hyperfocal_distance_1e3;
    int   aperture_value;
    int   focal_length;
    int   subject_distance;
    int   near_limit;
    int   far_limit;
    int   hyperfocal_distance;
    int   depth_of_field;
    int   min_stack_distance;
} DOF_TYPE;

//==========================================================
// Data Structure to store camera specific information
// Used by modules to ensure module code is platform independent

// Note: if these structures are changed update the corresponding version
//      _cam_sensor --> CAM_SENSOR_VERSION
//      _cam_screen --> CAM_SCREEN_VERSION
//      _cam_info   --> CAM_INFO_VERSION
#define CAM_SCREEN_VERSION      {2,0}       // Version for camera_screen
#define CAM_SENSOR_VERSION      {1,0}       // Version for camera_sensor
#define CAM_INFO_VERSION        {5,0}       // Version for camera_info

typedef struct
{
    int bits_per_pixel;
    int black_level;
    int white_level;
    unsigned raw_rows, raw_rowpix, raw_rowlen, raw_size;
    union                       // DNG JPEG info
    {
        struct
        {
            int x, y;           // DNG JPEG top left corner
            int width, height;  // DNG JPEG size
        } jpeg;
        struct
        {
            int origin[2];
            int size[2];
        } crop;
    };
    union                       // DNG active sensor area (Y1, X1, Y2, X2)
    {
        struct
        {
            int y1, x1, y2, x2;
        } active_area;
        int dng_active_area[4];
    };
    int lens_info[8];           // DNG Lens Info
    int exposure_bias[2];       // DNG Exposure Bias
    int cfa_pattern;
    int calibration_illuminant1;
    int color_matrix1[18];      // DNG Color Matrix
    int calibration_illuminant2;
    int color_matrix2[18];      // DNG Color Matrix 2
    int has_calibration1;
    int camera_calibration1[18];// DNG Camera Calibration Matrix 1
    int has_calibration2;
    int camera_calibration2[18];// DNG Camera Calibration Matrix 2
    int has_forwardmatrix1;
    int forward_matrix1[18];    // DNG Camera Forward Matrix 1
    int has_forwardmatrix2;
    int forward_matrix2[18];    // DNG Camera Forward Matrix 1
    int dng_badpixel_value_limit;
    short cfa_offsets[4][2];
} _cam_sensor;

// cfa_offsets indexes
#define CFA_R  0
#define CFA_G1 1
#define CFA_G2 2
#define CFA_B  3

extern _cam_sensor camera_sensor;

typedef struct 
{
    int     width, height, size;                        // Size of bitmap screen in CHDK co-ordinates
    int     physical_width;                             // Actual width of bitmap screen in pixels
    int     buffer_width, buffer_height, buffer_size;   // Physical size of bitmap screen
    int     disp_left, disp_right, disp_width;          // Usable horizontal size (after excluding touch screen buttons)
    int     edge_hmargin;                               // edge overlay top & bottom margin to skip
    int     zebra_nobuf, zebra_aspect_adjust;           // zebra feature settings
    int     has_variable_aspect;                        // zebra feature settings
    int     ts_menu_border;                             // touch screen top & bottom margin to skip for menus (area that is less sensitive)
    int     menu_border_width;                          // Width of border on each side of CHDK menu
    int     fselect_name_size;                          // file select window file name column width
    int     fselect_size_size;                          // file select window file size column width
    int     fselect_time_size;                          // file select window file date column width
#ifdef CAM_DRAW_RGBA
    int     yuvbm_width, yuvbm_height, yuvbm_buffer_width, yuvbm_buffer_size; // Size of YUV bitmap screen
#endif
} _cam_screen;

extern _cam_screen camera_screen;

// Some macros to simplify code
#ifdef  CAM_DRAW_RGBA
    #define CAMERA_SCREEN_BM_WIDTH         camera_screen.yuvbm_width
    #define CAMERA_SCREEN_BM_PHYS_WIDTH    camera_screen.yuvbm_width
    #define CAMERA_SCREEN_BM_HEIGHT        camera_screen.yuvbm_height
    #define CAMERA_SCREEN_BM_BUFFER_WIDTH  camera_screen.yuvbm_buffer_width
#else
    #define CAMERA_SCREEN_BM_WIDTH         camera_screen.width
    #define CAMERA_SCREEN_BM_PHYS_WIDTH    camera_screen.physical_width
    #define CAMERA_SCREEN_BM_HEIGHT        camera_screen.height
    #define CAMERA_SCREEN_BM_BUFFER_WIDTH  camera_screen.buffer_width
#endif

typedef struct
{
    unsigned int    tick_count;         // tick count of last call to function
    unsigned int    last, min, max, sum, count;
} time_counter;

typedef struct
{
    // Canon PARAMS indexes
    struct
    {
        short camera_name;
        short owner_name;
        short artist_name;
        short copyright;
    } params;

    // Canon PROPCASE indexes
    struct
    {
        short propset;
        short gps;
        short orientation_sensor;
        short tv;
        short av;
        short min_av;
        short ev_correction_1;
        short ev_correction_2;
        short flash_mode;
        short flash_fire;
        short metering_mode;
        short wb_adj;
        short aspect_ratio;
        short shooting;
        short resolution;
        short quality;
        short af_lock;
    } props;

    int rombaseaddr, maxramaddr, memisosize;
    int cam_uncached_bit, exmem;
    int text_start, data_start, bss_start, bss_end;     // Link values (used for debug)
    int tick_count_offset;      // get_tick_count value at which the clock ticks over 1 second
    char* platform;
    char* platformsub;
    char* chdk_ver;
    char* build_number;
    char* chdk_dng_ver;
    char* build_svnrev;
    char* build_date;
    char* build_time;
    char* os;
    short cam_ev_in_video;            // Can CHDK can change exposure in video mode?
    short cam_has_nd_filter;
    short cam_has_iris_diaphragm;
    short cam_has_video_button, cam_has_zoom_lever;
    short cam_has_manual_focus;
    short cam_has_multipart;
    int   cam_remote_sync_status_led;
    short cam_key_press_delay, cam_key_release_delay;
    short cam_af_led;
    short circle_of_confusion;
    short cam_digic; // digic number from makefile. Number x10, so digic 2 = 20,  digic IV = 40, digic IV+ = 41.
    short cam_canon_raw; // cam has native raw support
    short sd_override_modes;  // Available SD override modes for camera

    // Miscellaneous variables to record state information
    // Used to control communication between various tasks and modules
    struct
    {
        int     is_shutter_half_press;      // State of Shutter Half Press button
        int     auto_started;               // Set to 1 if script auto-started
        int     user_menu_has_changed;      // not saved to config file, used to tell code that file needs to be saved
        int     kbd_last_clicked;           // For scripts
        long    kbd_last_clicked_time;      // For scripts
        long    kbd_last_checked_time;      // For scripts
        int     script_dial_control;        // For scripts to control dial handling
        long    shutter_open_time;          // For DNG EXIF creation
        long    shutter_open_tick_count;    // For DNG EXIF creation
        int     state_shooting_progress;    // Holds current state when still image being processed
        int     state_kbd_script_run;       // Script execution state
        int     osd_title_line;             // Display state of CHDK OSD title line
                                            //  0 - don't display Script name or <ALT> text
                                            //  1 - display Script name and <ALT> text (including script exit key name)
                                            //  2 - don't display Script name, display <ALT> text (including script exit key name)

        int     gui_mode;                   // Current GUI mode (from gui_mode->mode)
        int     gui_mode_none;              // Current GUI mode == GUI_MODE_NONE
        int     gui_mode_alt;               // Current GUI mode == GUI_MODE_ALT

        int     mode;                       // Camera mode info
        int     mode_shooting;              // Camera shooting mode
        int     mode_video;                 // Are we in camera video mode
        int     mode_rec;                   // Are we in camera record mode (still image)
        int     mode_rec_or_review;         // Are we in camera record mode (still image) or review hold & OSD displayed in review hold
        int     mode_play;                  // Are we in camera playback mode
        int     mode_photo;                 // Are we in camera photo mode (playback or not video and not stitch)
                                            //  - why is some functionality disabled in MODE_STITCH; but not MODE_SCN_STITCH (e.g. bracketing) ?????
    } state;

    // Performance counters
    struct
    {
        unsigned int    md_detect_tick;     // Tick count of last motion detection
        int             md_af_tuning;       // Flag to enable/disable the Motion Detect tuning code using the AF LED
        int             af_led_on;          // Counter to time AF led turn on (for AF LED MD tuning)
        time_counter    af_led;             // Counters for AF led MD timing
        int             md_af_on_flag;      // Flag to turn on AF led after MD detects motion (to calculate delay from detect to capture)
        unsigned int    md_af_on_delay;     // How long after MD triggers to wait before turning on AF led
        unsigned int    md_af_on_time;      // How long to leave AF led on for
    } perf;

    // Depth of Field, Hyperfocal distance, etc
    DOF_TYPE dof_values;

    // Storage and interface for edge overlay 'image' buffer.
    // This is so the previous overlay can survive if the module gets unloaded
    struct
    {
        short   state_draw;         // Current state of overlay (Live/Frozen/Pano)
        short   edge_state;
        void*   edge_buf;
    } edge;

    struct
    {
        short   target_support;     // Remote capture supported modes
        short   file_target;        // Currently selected remotecap target(s)
    } remotecap;

#if defined(OPT_FILEIO_STATS)
    // Optional stats from file IO functions (open, close, read, write, etc)
    struct
    {
        short   fileio_semaphore_errors;
        short   max_semaphore_timeout;
        short   close_badfile_count;
        short   write_badfile_count;
        short   open_count;
        short   close_count;
        short   open_fail_count;
        short   close_fail_count;
    } fileio_stats;
#endif
} _cam_info;

extern _cam_info camera_info;

extern void camera_info_init();

//==========================================================

#endif /* CAMERA_INFO_H */
