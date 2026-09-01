#include "platform.h"
#include "touchscreen.h"
#include "conf.h"
#include "font.h"
#include "lang.h"
#include "fileutil.h"
#include "gui.h"
#include "gui_lang.h"
#include "gui_draw.h"
#include "gui_menu.h"
#include "gui_user_menu.h"
#include "gui_mbox.h"
#include "gui_hexbox.h"
#include "gui_osd.h"
#include "gui_batt.h"
#include "shooting.h"
#include "console.h"
#include "gui_recui.h"
#include "raw.h"
#include "modules.h"
#include "levent.h"
#include "callfunc.h"
#ifdef CAM_HAS_GPS
#include "gps.h"
#endif
#include "usb_remote.h"
#include "module_load.h"
#include "clock.h"
#include "dirent.h"
#include "raw_ev_histo.h"
#include "histogram.h"
#include "viewport.h"
#include "bend_store.h"
#include "bend_shot.h"
#include "bend_seg.h"
#include "mexp.h"
#include "mexp_ghost.h"
#include "theme.h"
#include "bend_tag.h"
#include "stdio.h"

// splash screen time
#if defined(OPT_EXPIRE_TEST)
#warning OPT_EXPIRE_TEST enabled
#define SPLASH_TIME 40  // Displays for 3.2 seconds
#else
#define SPLASH_TIME 20  // Displays for 1.6 seconds
                        // gui_redraw called every 4th loop of core_spytask which has 20ms delay per loop = (4*20) * 20 = 1600ms
#endif

//-------------------------------------------------------------------

#define _XSTR(x) #x
#define STR(x) _XSTR(x)

#define TEXT_COUNT          3
#define LOGO_TEXT_HEIGHT    (TEXT_COUNT*FONT_HEIGHT+8)

static const char* build_info[TEXT_COUNT] =
{
    "CHDK Version '" HDK_VERSION " " BUILD_NUMBER "-" BUILD_SVNREV "'",
    "Build: " __DATE__ " " __TIME__,
    "Camera: " PLATFORM " - " PLATFORMSUB
};

// gcc version string defined at compile time rather than using sprintf to allow tools like CHIMP to indentify in binary
static const char* gcc_info =
#ifdef __GNUC__
# ifndef __GNUC_PATCHLEVEL__
# define __GNUC_PATCHLEVEL 0
# endif
    "GCC " STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__);
#else
    "UNKNOWN";
#endif

//-------------------------------------------------------------------

// for memory info, duplicated from lowlevel
extern const char _start,_end;

#ifdef OPT_DEBUGGING
#ifndef CAM_DRYOS
    int debug_tasklist_start;
#endif
    int debug_display_direction=1;
#endif

//-------------------------------------------------------------------

int script_run_on_alt_flag ;

static char buf[256];

//-------------------------------------------------------------------
// Menu definitions 
//-------------------------------------------------------------------

//-------------------------------------------------------------------

/*
common code for "enum" menu items that just take a list of string values and don't require any special setters
would be better to have another menu item type that does this by default
save memory by eliminating dupe code
*/
void gui_enum_value_change(int *value, int change, int num_items) {
    int v = *value + change;
    if (v < 0)
        v = num_items-1;
    else if (v >= num_items)
        v = 0;
    *value = v;
}

static const char* gui_change_simple_enum(int* value, int change, const char** items, int num_items) {
    gui_enum_value_change(value, change, num_items);
    return (const char*)lang_str((int)items[*value]);
}

//-------------------------------------------------------------------

const char* gui_bracket_values_modes[] = { "Off", "1/3 Ev","2/3 Ev", "1 Ev", "1 1/3Ev", "1 2/3Ev", "2 Ev", "2 1/3Ev", "2 2/3Ev", "3 Ev", "3 1/3Ev", "3 2/3Ev", "4 Ev" };
const char* gui_bracket_type_modes[] =   { "+/-", "-", "+", "-/+" };

static CMenuItem sd_bracket[2] = {
    MENU_ITEM   (0, 0,  MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.subj_dist_bracket_value,  MENU_MINMAX(0, 30000) ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.subj_dist_bracket_koef,   0 ),
};

static CMenuItem iso_bracket[2] = {
    MENU_ITEM   (0, 0,  MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.iso_bracket_value,        MENU_MINMAX(0, 10000) ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.iso_bracket_koef,         0 ),
};

static CMenuItem bracketing_in_continuous_submenu_items[] = {
    MENU_ENUM2  (0x63,LANG_MENU_TV_BRACKET_VALUE,           &conf.tv_bracket_value,         gui_bracket_values_modes ),
#if CAM_HAS_IRIS_DIAPHRAGM
    MENU_ENUM2  (0x62,LANG_MENU_AV_BRACKET_VALUE,           &conf.av_bracket_value,         gui_bracket_values_modes ),
#endif
    MENU_ITEM   (0x5e,LANG_MENU_SUBJ_DIST_BRACKET_VALUE,    MENUITEM_STATE_VAL_PAIR,        &sd_bracket,                        100 ),
    MENU_ITEM   (0x74,LANG_MENU_ISO_BRACKET_VALUE,          MENUITEM_STATE_VAL_PAIR,        &iso_bracket,                       10 ),
    MENU_ENUM2  (0x60,LANG_MENU_BRACKET_TYPE,               &conf.bracket_type,             gui_bracket_type_modes ),
    MENU_ITEM   (0x5b,LANG_MENU_CLEAR_BRACKET_VALUES,       MENUITEM_BOOL,                  &conf.clear_bracket,                0 ),
    MENU_ITEM   (0x5c,LANG_MENU_BRACKETING_ADD_RAW_SUFFIX,  MENUITEM_BOOL,                  &conf.bracketing_add_raw_suffix,    0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0,                                  0 ),
    {0}
};
static CMenu bracketing_in_continuous_submenu = {0x2c,LANG_MENU_BRACKET_IN_CONTINUOUS_TITLE, bracketing_in_continuous_submenu_items };

//-------------------------------------------------------------------
static const char* gui_USB_switch_types_enum(int change, __attribute__ ((unused))int arg)
{
    static const char* modes[] = { "None","OnePush", "TwoPush", "CA-1" };    // note : make sure # of entries less than NUM_USB_INPUT_DRV in usb_remote.c
    gui_enum_value_change(&conf.remote_switch_type,change,sizeof(modes)/sizeof(modes[0]));

    if (change) set_usb_remote_state();

    return modes[conf.remote_switch_type];
}

static const char* gui_USB_control_modes_enum(int change, __attribute__ ((unused))int arg)
{
    static const char* modes[] = { "None", "Normal", "Quick", "Burst", "Bracket","Zoom", "Video" }; // note : make sure # of entries less than NUM_USB_MODULES in usb_remote.c
    gui_enum_value_change(&conf.remote_control_mode,change,sizeof(modes)/sizeof(modes[0]));

    if (change) set_usb_remote_state();

    return modes[conf.remote_control_mode];
}

#if CAM_REMOTE_MULTICHANNEL
static const char* gui_remote_input_types_enum(int change, __attribute__ ((unused))int arg)
{
    static remote_input_desc_t remote_inputs[]={
        {"USB",    REMOTE_INPUT_USB},
#ifdef CAM_REMOTE_HDMI_HPD
        {"HDMI HP",REMOTE_INPUT_HDMI_HPD},
#endif
#ifdef CAM_REMOTE_ANALOG_AV
        {"ANLG AV",REMOTE_INPUT_ANALOG_AV},
#endif
#ifdef CAM_REMOTE_AtoD_CHANNEL
        {"A/D Ch", REMOTE_INPUT_AD_CHANNEL},
#endif
    };
#define NUM_REMOTE_INPUT_TYPES (int)(sizeof(remote_inputs)/sizeof(remote_inputs[0]))
    int i;
    for(i=0; i<NUM_REMOTE_INPUT_TYPES; i++) {
        if(remote_inputs[i].type == conf.remote_input_channel) {
            break;
        }
    }
    // will handle out of range if existing was invalid
    gui_enum_value_change(&i,change,NUM_REMOTE_INPUT_TYPES);

    conf.remote_input_channel=remote_inputs[i].type;

    return remote_inputs[i].name;
}
#endif

#ifndef CAM_REMOTE_USES_PRECISION_SYNC
static CMenuItem synch_delay[2] = {
    MENU_ITEM   (0, 0,  MENUITEM_INT|MENUITEM_F_UNSIGNED,   &conf.synch_delay_value,        0 ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                      &conf.synch_delay_enable,       0 ),
};
#endif

static CMenuItem remote_submenu_items[] = {
    MENU_ITEM   (0x71,LANG_MENU_REMOTE_ENABLE,              MENUITEM_BOOL|MENUITEM_ARG_CALLBACK, &conf.remote_enable, (int)set_usb_remote_state),
#if CAM_REMOTE_MULTICHANNEL
    MENU_ITEM   (0x5f,LANG_MENU_REMOTE_INPUT_CHANNEL,       MENUITEM_ENUM,                gui_remote_input_types_enum, 0),
#endif
    MENU_ITEM   (0x5f,LANG_MENU_REMOTE_DEVICE,              MENUITEM_ENUM,                gui_USB_switch_types_enum, 0),
    MENU_ITEM   (0x5f,LANG_MENU_REMOTE_LOGIC,               MENUITEM_ENUM,                gui_USB_control_modes_enum, 0),
    MENU_ITEM   (0x0, LANG_MENU_REMOTE_OPTIONS,             MENUITEM_SEPARATOR,           0, 0 ), 
    MENU_ITEM   (0x5c,LANG_MENU_SYNCH_ENABLE,               MENUITEM_BOOL,                &conf.synch_enable, 0),
 #ifndef CAM_REMOTE_USES_PRECISION_SYNC
    MENU_ITEM   (0x5e,LANG_MENU_SYNCH_DELAY_VALUE,          MENUITEM_STATE_VAL_PAIR,      &synch_delay,                       10 ),
 #endif
    MENU_ITEM   (0x5c,LANG_MENU_SCRIPT_START_ENABLE,        MENUITEM_BOOL,                &conf.remote_enable_scripts, 0),
    MENU_ITEM   (0x2c,LANG_MENU_BRACKET_IN_CONTINUOUS,      MENUITEM_SUBMENU,             &bracketing_in_continuous_submenu,  0 ),    
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                  0, 0),
    {0}
};
static CMenu remote_submenu = {0x86,LANG_MENU_REMOTE_PARAM_TITLE, remote_submenu_items };

//-------------------------------------------------------------------

static const char* gui_autoiso_shutter_modes[] = { "Auto", "1/2", "1/4", "1/6", "1/8", "1/15", "1/30", "1/60", "1/125", "1/250", "1/500", "1/1000", "1/2000" };

static const char* gui_autoiso2_shutter_modes[] = { "Off", "1", "1/2","1/4", "1/6", "1/8", "1/12", "1/15", "1/20", "1/25", "1/30",
                                                    "1/40", "1/50", "1/60", "1/80", "1/100", "1/125", "1/160", "1/250", "1/500", "1/1000", "1/2000" };

static const char* gui_overexp_ev_modes[] = { "Off", "-1/3 Ev", "-2/3 Ev", "-1 Ev", "-1 1/3Ev", "-1 2/3Ev", "-2 Ev" };

static CMenuItem autoiso_submenu_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_AUTOISO_ENABLED,            MENUITEM_BOOL,                                      &conf.autoiso_enable,           0 ),
    MENU_ENUM2  (0x5f,LANG_MENU_AUTOISO_MIN_SHUTTER,        &conf.autoiso_shutter_enum,                         gui_autoiso_shutter_modes ),
    MENU_ITEM   (0x5f,LANG_MENU_AUTOISO_USER_FACTOR,        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.autoiso_user_factor,      MENU_MINMAX(1, 8) ),

#if CAM_HAS_IS
    MENU_ITEM   (0x5f,LANG_MENU_AUTOISO_IS_FACTOR,          MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.autoiso_is_factor,        MENU_MINMAX(1, 8) ),
#endif

    MENU_ITEM   (0x5f,LANG_MENU_AUTOISO_MIN_ISO,            MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.autoiso_min_iso,          MENU_MINMAX(10, 200) ),
    MENU_ITEM   (0x5f,LANG_MENU_AUTOISO_MAX_ISO_AUTO,       MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.autoiso_max_iso_auto,     MENU_MINMAX(10, 3200) ),

#if CAM_HAS_HI_ISO_AUTO_MODE
    MENU_ITEM   (0x5f,LANG_MENU_AUTOISO_MAX_ISO_HI,         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.autoiso_max_iso_hi,       MENU_MINMAX(200, 3200) ),
#endif

    MENU_ENUM2  (0x5f,LANG_MENU_AUTOISO_MIN_SHUTTER2,       &conf.autoiso2_shutter_enum,                        gui_autoiso2_shutter_modes ), 
    MENU_ITEM   (0x5f,LANG_MENU_AUTOISO_MAX_ISO2,           MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.autoiso2_max_iso_auto,    MENU_MINMAX(100, 3200) ),

    MENU_ENUM2  (0x5f,LANG_MENU_AUTOISO_OVEREXP_EV,        &conf.overexp_ev_enum, gui_overexp_ev_modes ),
    MENU_ITEM   (0x57,LANG_MENU_ZEBRA_OVER,                MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.autoiso2_over,         MENU_MINMAX(0, 32) ),
    MENU_ITEM   (0x5f,LANG_MENU_AUTOISO_OVEREXP_THRES,     MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.overexp_threshold,     MENU_MINMAX(1, 20) ),

    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,    0,                                                              0 ),
    {0}
};

static CMenu autoiso_submenu = {0x2d,LANG_MENU_AUTOISO_TITLE, autoiso_submenu_items };

//-------------------------------------------------------------------
#if OPT_EXPIRE_TEST

static const char *exp_text = "Test version expired\nPlease post feedback to:\nchdk.setepontos.com\nYour comments are needed\nto finish this port.\nThanks";

#define EXP_TEXT_WIDTH   28
#define EXP_TEXT_HEIGHT  6

int get_expire_days_left(void) {
    time_t ts = time(NULL);
    if(ts > OPT_EXPIRE_TEST) {
        return 0;
    }
    return (OPT_EXPIRE_TEST - ts)/(60*60*24);
}

void do_expire_check() {
    if(get_expire_days_left()) {
        return;
    }
    static int in_splash = SPLASH_TIME;

    if(in_splash)
    {
        in_splash--;
        return;
    }

    twoColors cl;
    if (camera_info.state.gui_mode_alt) {
        cl=MAKE_COLOR(COLOR_RED, COLOR_WHITE);
#ifdef CAM_DISP_ALT_TEXT
        gui_reset_alt_helper(); // replace the helper with nag screen
#endif
    } else if (camera_info.state.gui_mode_none) {
        cl=MAKE_COLOR(COLOR_TRANSPARENT, COLOR_WHITE);
    } else {
        return;
    }

    coord x = (camera_screen.width  - EXP_TEXT_WIDTH*FONT_WIDTH) >> 1;
    coord y = (camera_screen.height - EXP_TEXT_HEIGHT*FONT_HEIGHT) >> 1;

    draw_text_justified(x, y, exp_text, cl, EXP_TEXT_WIDTH, camera_info.state.gui_mode_alt ? EXP_TEXT_HEIGHT : 1, TEXT_CENTER|TEXT_FILL);
}

void do_expire_splash(int x,int y) {
    static char under_dev_text[64];
    int days_left = get_expire_days_left();
    twoColors cl = MAKE_COLOR(COLOR_RED, COLOR_WHITE);
    if(days_left) {
        sprintf(under_dev_text, "TEST BUILD %d DAYS LEFT",days_left);
    } else {
        sprintf(under_dev_text, "TEST BUILD EXPIRED!");
    }
    draw_string(x-((strlen(under_dev_text)*FONT_WIDTH)>>1), y, under_dev_text, cl);
}

#endif

#ifdef OPT_DEBUGGING

static void gui_compare_props(int arg)
{
    #define NUM_PROPS 512
    // never freed, but not allocated unless prop compare is used once
    static int *props = NULL;
    static int prev_arg = 0;
    char buf[64];
    int i;
    int p;
    int c;

    if( props && (arg==prev_arg) )
    { // we have previous data (of same kind) set! do a comparison
        c = 0;
        for( i = 0; i < NUM_PROPS; ++i )
        {
            p = (arg==0)?shooting_get_prop(i):get_uiprop_value(i);
            if( props[i] != p )
            {
                ++c;
                sprintf(buf,"%4d is %8d was %8d",i,p,props[i]);
                draw_string(16,FONT_HEIGHT*c,buf,MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
            }
            props[i] = p;
            if( c == 12 )
            {
                ++c;
                sprintf(buf,"%s","Waiting 15 Seconds");
                draw_string(16,FONT_HEIGHT*c,buf,MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
                msleep(15000);
                c = 0;
            }
        }
        ++c;
        sprintf(buf,"%s","Press <ALT> to leave");
        draw_string(16,FONT_HEIGHT*c,buf,MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
    }
    else
    {
    // no previous data (of same kind) was set so we save the data initially
        if (!props) {
            // allocate this only once
            props = (int *)malloc(NUM_PROPS*sizeof(int));
        }
        if(props) {
            for( i = 0; i < NUM_PROPS; ++i )
            {
                props[i] = (arg==0)?shooting_get_prop(i):get_uiprop_value(i);
            }
        }
    }
    prev_arg = arg;
}

// Save camera romlog to A/ROMLOG.LOG file
static void save_romlog(__attribute__ ((unused))int arg)
{
    extern unsigned _ExecuteEventProcedure(const char *name,...);

    if (stat("A/ROMLOG.LOG",0)    == 0) remove("A/ROMLOG.LOG");
    if (stat("A/RomLogErr.txt",0) == 0) remove("A/RomLogErr.txt");

    unsigned args[3];
    args[0] = (unsigned)"SystemEventInit";
    if (call_func_ptr(_ExecuteEventProcedure,args,1) == (unsigned)-1)
    {
        args[0] = (unsigned)"System.Create";
        if (call_func_ptr(_ExecuteEventProcedure,args,1) == (unsigned)-1)
        {
            gui_mbox_init(LANG_ERROR, LANG_SAVE_ROMLOG_INIT_ERROR, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
            return;
        }
    }

    args[0] = (unsigned)"GetLogToFile";
    args[1] = (unsigned)"A/ROMLOG.LOG";
    args[2] = 1;
    if (call_func_ptr(_ExecuteEventProcedure,args,3) == (unsigned)-1)
    {
        gui_mbox_init(LANG_ERROR, LANG_SAVE_ROMLOG_FAIL, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
    }
    else
    {
        gui_mbox_init(LANG_INFORMATION, LANG_SAVE_ROMLOG_OK, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
    }
}

// Dump the camera firmware ROM to A/PRIMARY.BIN
// Same dump as modules/firmware_crc.c, but reachable without a failing CRC check.
static void dump_rom_to_card(__attribute__ ((unused))int arg)
{
    // skip the final word to avoid address wraparound, matching dump_rom()
    unsigned size = 0xfffffffc - camera_info.rombaseaddr;
    if (size > 0x1fffffc) size = 0x1fffffc;    // no known ROM is bigger than 32MB

    if (stat("A/PRIMARY.BIN",0) == 0) remove("A/PRIMARY.BIN");

    FILE *fh = fopen("A/PRIMARY.BIN","wb");
    if (!fh)
    {
        gui_mbox_init(LANG_ERROR, (int)"Could not create PRIMARY.BIN", MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }

    // write in blocks so the progress bar reflects real progress
    const char *src = (const char *)camera_info.rombaseaddr;
    unsigned done = 0;
    int failed = 0;
    while (done < size)
    {
        unsigned n = size - done;
        if (n > 0x10000) n = 0x10000;
        if (fwrite(src + done, n, 1, fh) <= 0)
        {
            failed = 1;
            break;
        }
        done += n;
        draw_progress_bar("Write PRIMARY.BIN", done * 100 / size);
    }
    fclose(fh);

    if (failed)
        gui_mbox_init(LANG_ERROR, (int)"Write failed - card full?", MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
    else
        gui_mbox_init(LANG_INFORMATION, (int)"Wrote A/PRIMARY.BIN", MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
}

static const char* gui_debug_shortcut_modes[] =             { "None", "DmpRAM", "Page", "CmpProps", "CmpUIP"};
#ifdef CAM_DRYOS
static const char* gui_debug_display_modes[] =              { "None", "Props", "Params", "None",  "UIProps" };
#else
static const char* gui_debug_display_modes[] =              { "None", "Props", "Params", "Tasks", "UIProps"};
#endif

static const char* gui_firmware_crc_modes[] =              { "Never", "Next", "Always"};

static int memdmp_delay = 0; // delay in seconds

static void gui_menu_edit_hexa_value(int arg) {
    // 'arg' parameter is pointer to int variable
    libhexbox->hexbox_init( (int*)arg, lang_str(LANG_MENU_DEBUG_MEMDMP_START), HEXBOX_FLAG_WALIGN );
}

static CMenuItem memdmp_submenu_items[] = {
    MENU_ITEM   (0x2a,LANG_MENU_DEBUG_MEMDMP_START,         MENUITEM_PROC,                  gui_menu_edit_hexa_value,           &conf.memdmp_start),
    MENU_ITEM   (0x2a,LANG_MENU_DEBUG_MEMDMP_SIZE,          MENUITEM_PROC,                  gui_menu_edit_hexa_value,           &conf.memdmp_size),
    MENU_ITEM   (0x2a,LANG_MENU_DEBUG_MEMDMP_DELAY,         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &memdmp_delay, MENU_MINMAX(0, 10)   ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0,                                  0 ),
    {0}
};

static CMenu memdmp_submenu = {0x2a,LANG_MENU_DEBUG_MEMDMP, memdmp_submenu_items };

// Dump what the file system actually reports, to A/MODTEST.TXT in the card root.
//
// Kept because it is the only thing that found the volume label bug: a card
// formatted with the label "CHDK" has a root entry of that name with the
// volume-id attribute, which the a460's FAT driver matches ahead of the A/CHDK
// directory, so opendir("A/CHDK") returns NULL and nothing under it can be
// opened. Every symptom of that pointed somewhere else - "missing modules",
// badpixel doing nothing, settings not persisting - and no amount of reading
// stub addresses was going to find it. Asking the camera what it can actually
// see took one build and settled it.
//
// Writes to the root deliberately: if a subdirectory is the thing that is
// broken, a diagnostic that writes into one cannot report it.
static void module_path_test(__attribute__ ((unused))int arg)
{
    static const char * const paths[] = {
        "A/DISKBOOT.BIN",
        "A/CHDK",
        "A/CHDK/MODULES",
        "A/CHDK/MODULES/zebra.flt",
        "A/CHDK/CCHDK4.CFG",
    };
    char line[160];
    int i;

    FILE *fh = fopen("A/MODTEST.TXT","wb");
    if (!fh)
    {
        gui_mbox_init(LANG_ERROR, (int)"Cannot write A/MODTEST.TXT", MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }

    for (i = 0; i < (int)(sizeof(paths)/sizeof(paths[0])); i++)
    {
        int fd = open(paths[i], O_RDONLY, 0777);
        sprintf(line, "open %-26s = %d\r\n", paths[i], fd);
        fwrite(line, 1, strlen(line), fh);
        if (fd >= 0) close(fd);
    }

    {
        static const char * const dirs[] = { "A", "A/CHDK", "A/CHDK/MODULES" };
        int k;
        for (k = 0; k < 3; k++)
        {
            DIR *d = opendir(dirs[k]);
            sprintf(line, "opendir %-18s = %s\r\n", dirs[k], d ? "ok" : "NULL");
            fwrite(line, 1, strlen(line), fh);
            if (d)
            {
                struct dirent *e;
                int n = 0;
                while (((e = readdir(d)) != 0) && (n < 45))
                {
                    sprintf(line, "  [%s]\r\n", e->d_name);
                    fwrite(line, 1, strlen(line), fh);
                    n++;
                }
                closedir(d);
                sprintf(line, "  -- %d entries\r\n", n);
                fwrite(line, 1, strlen(line), fh);
            }
        }
    }

    fclose(fh);
    gui_mbox_init(LANG_INFORMATION, (int)"Wrote A/MODTEST.TXT", MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
}

static CMenuItem debug_submenu_items[] = {
    MENU_ENUM2  (0x5c,LANG_MENU_DEBUG_DISPLAY,              &conf.debug_display,            gui_debug_display_modes ),
    MENU_ITEM   (0x2a,LANG_MENU_DEBUG_PROPCASE_PAGE,        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &conf.debug_propcase_page, MENU_MINMAX(0, 128) ),
#ifndef CAM_DRYOS
    MENU_ITEM   (0x2a,LANG_MENU_DEBUG_TASKLIST_START,       MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &debug_tasklist_start, MENU_MINMAX(0, 63) ),
#endif
    MENU_ITEM   (0x5c,LANG_MENU_DEBUG_SHOW_MISC_VALS,       MENUITEM_BOOL,                  &conf.debug_misc_vals_show,         0 ),
    MENU_ENUM2  (0x5c,LANG_MENU_DEBUG_SHORTCUT_ACTION,      &conf.debug_shortcut_action,    gui_debug_shortcut_modes ),
    MENU_ITEM   (0x2a,LANG_MENU_DEBUG_MEMDMP,               MENUITEM_SUBMENU,               &memdmp_submenu,                    0 ),
    MENU_ITEM   (0x2a,LANG_SAVE_ROMLOG,                     MENUITEM_PROC,                  save_romlog,                        0 ),
    MENU_ITEM   (0x2a,(int)"Dump ROM to card",              MENUITEM_PROC,                  dump_rom_to_card,                   0 ),
    MENU_ITEM   (0x2a,(int)"Module path test",              MENUITEM_PROC,                  module_path_test,                   0 ),
    MENU_ENUM2  (0x5c,LANG_FIRMWARE_CRC_BOOT_MENU,          &conf.check_firmware_crc,       gui_firmware_crc_modes ),

    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0,                                  0 ),
    {0}
};

static CMenu debug_submenu = {0x2a,LANG_MENU_DEBUG_TITLE, debug_submenu_items };

#endif

//-------------------------------------------------------------------

#ifdef CAM_HAS_GPS

#define GPS_START 0
#define GPS_STOP 1

// Index of adjustable entries in gps_submenu_items
// Make sure these are updated if changes made to gps_submenu_items
#define GPS_IDX_SHOW_COMPASS    2
#define GPS_IDX_NAV_TO_IMAGE    3
#define GPS_IDX_NAV_TO_HOME     4
#define GPS_IDX_START_STOP      5

// forward reference
static CMenuItem gps_submenu_items[];

static void gpx_start_stop(__attribute__ ((unused))int arg)
{
    if( conf.gps_on_off ) {  
        if( gps_submenu_items[GPS_IDX_START_STOP].text == LANG_MENU_GPS_TRACK_START ) {      //toggle text
            gps_submenu_items[GPS_IDX_START_STOP].text = LANG_MENU_GPS_TRACK_STOP;
            init_gps_logging_task(GPS_START);
        } else {
            gps_submenu_items[GPS_IDX_START_STOP].text = LANG_MENU_GPS_TRACK_START;
            init_gps_logging_task(GPS_STOP); 
        }
    }
}

static void navigate_to_home(int);
static void navigate_to_image(int);

static void show_compass(__attribute__ ((unused))int arg)
{
    if( conf.gps_on_off ) {      
        if( gps_submenu_items[GPS_IDX_SHOW_COMPASS].text == LANG_MENU_GPS_COMPASS_SHOW ) {     //toggle text
            init_gps_compass_task(GPS_START);
            gps_submenu_items[GPS_IDX_SHOW_COMPASS].text = LANG_MENU_GPS_COMPASS_HIDE;
            gps_submenu_items[GPS_IDX_NAV_TO_HOME].text = LANG_MENU_GPS_NAVI_HOME;
            gps_submenu_items[GPS_IDX_NAV_TO_IMAGE].text = LANG_MENU_GPS_NAVI_SHOW;
        } else {
            gps_submenu_items[GPS_IDX_SHOW_COMPASS].text = LANG_MENU_GPS_COMPASS_SHOW;
            init_gps_compass_task(GPS_STOP);        
        }
    }
}

static void navigate_to_image(__attribute__ ((unused))int arg)
{
    int i = 0;
    if( conf.gps_on_off ) {      
        if( gps_submenu_items[GPS_IDX_NAV_TO_IMAGE].text == LANG_MENU_GPS_NAVI_SHOW ) {         //toggle text
            if( init_gps_navigate_to_photo(GPS_START) )
            {
                gps_submenu_items[GPS_IDX_NAV_TO_IMAGE].text = LANG_MENU_GPS_NAVI_HIDE;
                gps_submenu_items[GPS_IDX_SHOW_COMPASS].text = LANG_MENU_GPS_COMPASS_SHOW; 
                gps_submenu_items[GPS_IDX_NAV_TO_HOME].text = LANG_MENU_GPS_NAVI_HOME;
            }
        } else {
            gps_submenu_items[GPS_IDX_NAV_TO_IMAGE].text = LANG_MENU_GPS_NAVI_SHOW;
            init_gps_navigate_to_photo(GPS_STOP);        
        }
    }
}

static void navigate_to_home(__attribute__ ((unused))int arg)
{
    if( conf.gps_on_off ) {      
        if( gps_submenu_items[GPS_IDX_NAV_TO_HOME].text == LANG_MENU_GPS_NAVI_HOME ) {         //toggle text
            if( init_gps_navigate_to_home(GPS_START))
            {
                gps_submenu_items[GPS_IDX_NAV_TO_HOME].text = LANG_MENU_GPS_NAVI_HOME_END;
                gps_submenu_items[GPS_IDX_SHOW_COMPASS].text = LANG_MENU_GPS_COMPASS_SHOW; 
                gps_submenu_items[GPS_IDX_NAV_TO_IMAGE].text = LANG_MENU_GPS_NAVI_SHOW;
            }
        } else {
            gps_submenu_items[GPS_IDX_NAV_TO_HOME].text = LANG_MENU_GPS_NAVI_HOME;
            init_gps_navigate_to_home(GPS_STOP);        
        }
    }
}

static void cb_gps_menu_reset()
{
    if( conf.gps_on_off == 0 ) {  
        gps_submenu_items[GPS_IDX_START_STOP].text = LANG_MENU_GPS_TRACK_START;
        gps_submenu_items[GPS_IDX_NAV_TO_HOME].text = LANG_MENU_GPS_NAVI_HOME;
        gps_submenu_items[GPS_IDX_SHOW_COMPASS].text = LANG_MENU_GPS_COMPASS_SHOW;
    }
}

static CMenuItem gps_logging_items[] = {
    MENU_ITEM   (0x5f,LANG_MENU_GPS_TRACK_TIME,             MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_track_time,       MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_TRACK_SYMBOL,           MENUITEM_BOOL,                                      &conf.gps_track_symbol,     0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_REC_PLAY_SET_1,         MENUITEM_BOOL,                                      &conf.gps_rec_play_set_1,   0 ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_REC_PLAY_TIME_1,        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_rec_play_time_1,  MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_PLAY_DARK_SET_1,        MENUITEM_BOOL,                                      &conf.gps_play_dark_set_1,  0 ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_PLAY_DARK_TIME_1,       MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_play_dark_time_1, MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                                        0,                          0 ),
    {0}
};

static CMenu gps_logging_submenu = {0x86,LANG_MENU_GPS_LOGGING, gps_logging_items };

static CMenuItem gps_tagging_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_GPS_WAYPOINT_SAVE,          MENUITEM_BOOL,                                      &conf.gps_waypoint_save,    0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_WAIT_FOR_SIGNAL,        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_wait_for_signal,  MENU_MINMAX(1, 599) ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_WAIT_FOR_SIGNAL_TIME,   MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_wait_for_signal_time, MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_REC_PLAY_SET,           MENUITEM_BOOL,                                      &conf.gps_rec_play_set,     0 ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_REC_PLAY_TIME,          MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_rec_play_time,    MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_PLAY_DARK_SET,          MENUITEM_BOOL,                                      &conf.gps_play_dark_set,    0 ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_PLAY_DARK_TIME,         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_play_dark_time,   MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_COUNTDOWN,              MENUITEM_BOOL,                                      &conf.gps_countdown ,       0 ),
//     MENU_ITEM   (0x5c,LANG_MENU_GPS_COUNTDOWN_BLINK,        MENUITEM_BOOL,                                      &conf.gps_countdown_blink,  0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                                        0,                          0 ),
    {0}
};

static CMenu gps_tagging_submenu = {0x86,LANG_MENU_GPS_TAGGING, gps_tagging_items };

static CMenuItem gps_navigation_items[] = {
    MENU_ITEM   (0x5f,LANG_MENU_GPS_COMPASS_SMOOTH,         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_compass_smooth,   MENU_MINMAX(1, 40) ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_COMPASS_TIME,           MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_compass_time,     MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x5f,LANG_MENU_GPS_NAVI_TIME,              MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_navi_time,        MENU_MINMAX(1, 60) ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x2a,LANG_MENU_GPS_MARK_HOME,              MENUITEM_PROC,                                      gps_write_home,             0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                                        0,                          0 ),
    {0}
};

static CMenu gps_navigation_submenu = {0x86,LANG_MENU_GPS_NAVIGATION, gps_navigation_items };

static const char* gui_gps_sat_fix[] =                  { "immer", "2D", "3D", "2D/3D" };

static CMenuItem gps_values_items[] = {
    MENU_ITEM   (0x5f,LANG_MENU_GPS_BATT,                   MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.gps_batt,             MENU_MINMAX(0, 99) ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_BATT_WARNING,           MENUITEM_BOOL,                                      &conf.gps_batt_warn,        0 ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_BEEP_WARNING,           MENUITEM_BOOL,                                      &conf.gps_beep_warn,        0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ENUM2  (0x5f,LANG_MENU_GPS_2D_3D_FIX,              &conf.gps_2D_3D_fix,                                gui_gps_sat_fix ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_SYMBOL_SHOW,            MENUITEM_BOOL,                                      &conf.gps_show_symbol,      0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x5c,LANG_MENU_GPS_TEST_TIMEZONE,          MENUITEM_BOOL,                                      &conf.gps_test_timezone,    0 ),
    MENU_ITEM   (0x2a,LANG_MENU_GPS_MARK_TIMEZONE,          MENUITEM_PROC,                                      gps_write_timezone,         0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                                 0,                          0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                                        0,                          0 ),
    {0}
};

static CMenu gps_values_submenu = {0x86,LANG_MENU_GPS_VALUES, gps_values_items };

// Be sure to update GPS_IDX_??? values above if gps_submenu_items is changed
static CMenuItem gps_submenu_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_GPS_ON_OFF,                 MENUITEM_BOOL | MENUITEM_ARG_CALLBACK,              &conf.gps_on_off,  (int)cb_gps_menu_reset  ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,             0,                                  0 ),
    MENU_ITEM   (0x2a,LANG_MENU_GPS_COMPASS_SHOW,           MENUITEM_PROC,                  show_compass,                       0 ),
    MENU_ITEM   (0x2a,LANG_MENU_GPS_NAVI_SHOW,              MENUITEM_PROC,                  navigate_to_image,                  0 ),
    MENU_ITEM   (0x2a,LANG_MENU_GPS_NAVI_HOME,              MENUITEM_PROC,                  navigate_to_home,                   0 ),
    MENU_ITEM   (0x2a,LANG_MENU_GPS_TRACK_START,            MENUITEM_PROC,                  gpx_start_stop,                     0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,             0,                                  0 ),
    MENU_ITEM   (0x28,LANG_MENU_GPS_VALUES,                 MENUITEM_SUBMENU,               &gps_values_submenu,                0 ),
    MENU_ITEM   (0x28,LANG_MENU_GPS_LOGGING,                MENUITEM_SUBMENU,               &gps_logging_submenu,               0 ),
    MENU_ITEM   (0x28,LANG_MENU_GPS_TAGGING,                MENUITEM_SUBMENU,               &gps_tagging_submenu,               0 ),
    MENU_ITEM   (0x28,LANG_MENU_GPS_NAVIGATION,             MENUITEM_SUBMENU,               &gps_navigation_submenu,            0 ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,             0,                                  0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0,                                  0 ),
    {0}
};

static CMenu gps_submenu = {0x86,LANG_MENU_GPS, gps_submenu_items };

#endif

//-------------------------------------------------------------------

static void gui_draw_read_selected(const char *fn)
{
    if (fn)
    {
        libtxtread->read_file(fn);
    }
}

static void gui_draw_read(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_SELECT_TEXT_FILE, conf.reader_file, "A/CHDK/BOOKS", gui_draw_read_selected);
}

static void gui_draw_read_last(int arg)
{
    if (stat(conf.reader_file,0) == 0)
        gui_draw_read_selected(conf.reader_file);
    else
        gui_draw_read(arg);
}

static void gui_draw_rbf_selected(const char *fn)
{
    if (fn) {
        strcpy(conf.reader_rbf_file, fn);
    }
}

static void gui_draw_load_rbf(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_SELECT_FONT_FILE, conf.reader_rbf_file, "A/CHDK/FONTS", gui_draw_rbf_selected);
}

static const char* gui_reader_codepage_cps[] = { "Win1251", "DOS"};
static CMenuItem reader_submenu_items[] = {
    MENU_ITEM(0x35,LANG_MENU_READ_OPEN_NEW,           MENUITEM_PROC,    gui_draw_read, 0 ),
    MENU_ITEM(0x35,LANG_MENU_READ_OPEN_LAST,          MENUITEM_PROC,    gui_draw_read_last, 0 ),
    MENU_ITEM(0x35,LANG_MENU_READ_SELECT_FONT,        MENUITEM_PROC,    gui_draw_load_rbf, 0 ),
    MENU_ENUM2(0x5f,LANG_MENU_READ_CODEPAGE,          &conf.reader_codepage, gui_reader_codepage_cps ),
    MENU_ITEM(0x5c,LANG_MENU_READ_WORD_WRAP,          MENUITEM_BOOL,    &conf.reader_wrap_by_words, 0 ),
    MENU_ITEM(0x5c,LANG_MENU_READ_AUTOSCROLL,         MENUITEM_BOOL,    &conf.reader_autoscroll, 0 ),
    MENU_ITEM(0x5f,LANG_MENU_READ_AUTOSCROLL_DELAY,   MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.reader_autoscroll_delay, MENU_MINMAX(0, 60) ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                    MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu reader_submenu = {0x37,LANG_MENU_READ_TITLE, reader_submenu_items };

//-------------------------------------------------------------------

static CMenu games_submenu = {0x38,LANG_MENU_MISC_GAMES, 0 };
static CMenu tools_submenu = {0x28,LANG_MENU_MISC_TOOLS, 0 };

int submenu_sort(const void* v1, const void* v2)
{
    CMenuItem *mi1 = (CMenuItem*)v1;
    CMenuItem *mi2 = (CMenuItem*)v2;

    return strcmp(lang_str(mi1->text), lang_str(mi2->text));
}

static CMenuItem* create_module_menu(int mtype, char symbol)
{
    DIR             *d;
    struct dirent   *de;
    int             mcnt = 0;
    ModuleInfo      mi;
    char            modName[33];
    char            *nm;

    // Open directory & count # of modules matching mtype
    d = opendir("A/CHDK/MODULES");

    if (d)
    {
        while ((de = readdir(d)))
        {
            if ((de->d_name[0] != 0xE5) && (strcmp(de->d_name,".") != 0) && (strcmp(de->d_name,"..") != 0))
            {
                get_module_info(de->d_name, &mi, 0, 0);
                if ((mi.moduleType & MTYPE_MASK) == mtype)
                    mcnt++;
            }
        }

        closedir(d);
    }

    // Allocate memory for menu
    CMenuItem *submenu = malloc((mcnt+2) * sizeof(CMenuItem));
    memset(submenu, 0, (mcnt+2) * sizeof(CMenuItem));

    // Re-open directory & create game menu
    d = opendir("A/CHDK/MODULES");

    if (d)
    {
        mcnt = 0;
        while ((de = readdir(d)))
        {
            if ((de->d_name[0] != 0xE5) && (strcmp(de->d_name,".") != 0) && (strcmp(de->d_name,"..") != 0))
            {
                get_module_info(de->d_name, &mi, modName, 33);
                if ((mi.moduleType & MTYPE_MASK) == mtype)
                {
                    submenu[mcnt].symbol = (mi.symbol != 0) ? mi.symbol : symbol;
                    if (mi.moduleType & MTYPE_SUBMENU_TOOL)
                        submenu[mcnt].type = MENUITEM_SUBMENU_PROC;
                    else
                        submenu[mcnt].type = MENUITEM_PROC;
                    if (mi.moduleName < 0)
                        submenu[mcnt].text = -mi.moduleName;    // LANG string
                    else
                    {
                        nm = malloc(strlen(modName)+1);
                        strcpy(nm, modName);
                        submenu[mcnt].text = (int)nm;
                    }
                    submenu[mcnt].menu_function = (menu_proc)module_run;
                    nm = malloc(strlen(de->d_name)+1);
                    strcpy(nm, de->d_name);
                    submenu[mcnt].arg = (int)nm;
                    mcnt++;
                }
            }
        }

        closedir(d);

        submenu[mcnt].symbol = 0x51;
        submenu[mcnt].type = MENUITEM_UP;
        submenu[mcnt].text = LANG_MENU_BACK;
    }

    if (mcnt > 0)
    {
        extern int submenu_sort_arm(const void* v1, const void* v2);
        qsort(submenu, mcnt, sizeof(CMenuItem), submenu_sort_arm);
    }

    return submenu;
}

static void gui_module_menu(CMenu *m, int type)
{
    if (m->menu == 0)
        m->menu = create_module_menu(type, m->symbol);
    gui_activate_sub_menu(m);
}

static void gui_games_menu(__attribute__ ((unused))int arg)
{
    gui_module_menu(&games_submenu, MTYPE_GAME);
}

static void gui_tools_menu(__attribute__ ((unused))int arg)
{
    gui_module_menu(&tools_submenu, MTYPE_TOOL);
}

//-------------------------------------------------------------------

static void gui_menuproc_mkbootdisk(__attribute__ ((unused))int arg)
{
    mark_filesystem_bootable();
    gui_mbox_init(LANG_INFORMATION, LANG_CONSOLE_TEXT_FINISHED, MBOX_BTN_OK|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, NULL);
}

#if CAM_MULTIPART

static void card_break_proc(unsigned int btn)
{
    if (btn==MBOX_BTN_YES) create_partitions();
}

static void gui_menuproc_break_card(__attribute__ ((unused))int arg)
{
    gui_mbox_init(LANG_WARNING, LANG_PARTITIONS_CREATE_WARNING, MBOX_BTN_YES_NO|MBOX_DEF_BTN2|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, card_break_proc);
}

static char* partitions_enum=NULL;

static const char* gui_menuproc_swap_partitions_enum(int change, __attribute__ ((unused))int arg)
{
    int new_partition;
    int partition_count = get_part_count();
    char vBuf[16];
    if(partitions_enum)
    {
      free(partitions_enum);
      partitions_enum=NULL;
    }
    new_partition= get_active_partition()+change;
    if( new_partition <=0)
    {
      new_partition = partition_count;
    }
    else if( new_partition > partition_count)
    {
      new_partition = 1;
    }  
    sprintf(vBuf,"%d/%d",new_partition, partition_count);
    partitions_enum=malloc((strlen(vBuf)+1)*sizeof(char));
    strcpy(partitions_enum,vBuf);

    if(change != 0)
    {
      swap_partitions(new_partition);
    }
    return partitions_enum;
}

#endif

static CMenuItem sdcard_submenu_items[] = {
    MENU_ITEM   (0x33,LANG_MENU_DEBUG_MAKE_BOOTABLE,        MENUITEM_PROC,                  gui_menuproc_mkbootdisk, 0 ),
#if CAM_MULTIPART
    MENU_ITEM   (0x33,LANG_MENU_DEBUG_CREATE_MULTIPART ,    MENUITEM_PROC,                  gui_menuproc_break_card,            0 ),
    MENU_ITEM   (0x33,LANG_MENU_DEBUG_SWAP_PART,            MENUITEM_ENUM,                  gui_menuproc_swap_partitions_enum,  0 ),
#endif
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0,                                  0 ),
    {0},
};

static CMenu sdcard_submenu = {0x33,LANG_SD_CARD, sdcard_submenu_items };

//-------------------------------------------------------------------

static void gui_delete_module_log_callback(unsigned int btn)
{
    if (btn == MBOX_BTN_YES)
        module_log_clear();
}

static void gui_delete_module_log(__attribute__ ((unused))int arg)
{
    gui_mbox_init(LANG_WARNING, LANG_MENU_DELETE_MODULE_LOG, MBOX_BTN_YES_NO|MBOX_DEF_BTN2|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, gui_delete_module_log_callback);
}

static CMenuItem module_submenu_items[] = {
    MENU_ITEM   (0x80,LANG_MENU_MODULE_INSPECTOR,           MENUITEM_PROC,                  module_run, "modinsp.flt" ),
    MENU_ITEM   (0x5c,LANG_MENU_MODULE_LOGGING,             MENUITEM_BOOL,                  &conf.module_logging, 0 ),
    MENU_ITEM   (0x2b,LANG_MENU_DELETE_MODULE_LOG,          MENUITEM_PROC,                  gui_delete_module_log, 0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0, 0 ),
    {0},
};

static CMenu module_submenu = {0x28,LANG_MENU_MODULES, module_submenu_items };

//-------------------------------------------------------------------

static void gui_draw_fselect(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_FILE_BROWSER, "A", "A", NULL);
}

static void gui_show_build_info(__attribute__ ((unused))int arg)
{
    sprintf(buf, lang_str(LANG_MSG_BUILD_INFO_TEXT),
                camera_info.chdk_ver, camera_info.build_number, camera_info.build_svnrev,
                camera_info.build_date, camera_info.build_time, camera_info.platform, camera_info.platformsub,
                gcc_info);
    gui_mbox_init(LANG_MSG_BUILD_INFO_TITLE, (int)buf, MBOX_FUNC_RESTORE|MBOX_TEXT_LEFT, NULL);
}

static void gui_show_memory_info(__attribute__ ((unused))int arg)
{
    sprintf(buf, lang_str(LANG_MSG_MEMORY_INFO_TEXT), core_get_free_memory(), camera_info.memisosize, &_start, &_end);
    gui_mbox_init(LANG_MSG_MEMORY_INFO_TITLE, (int)buf, MBOX_FUNC_RESTORE|MBOX_TEXT_CENTER, NULL);
}

#if !defined(OPT_FORCE_LUA_CALL_NATIVE)
static void lua_native_call_warning(unsigned int btn)
{
    if (btn==MBOX_BTN_NO)
        conf.script_allow_lua_native_calls = 0;
}

static void gui_lua_native_call_warning()
{
    if (conf.script_allow_lua_native_calls)
        gui_mbox_init(LANG_WARNING, LANG_MENU_LUA_NATIVE_CALLS_WARNING, MBOX_BTN_YES_NO|MBOX_DEF_BTN2|MBOX_TEXT_CENTER, lua_native_call_warning);
}
#endif

#if defined(CAM_IS_VID_REC_WORKS)
static void unsafe_io_warning(unsigned int btn)
{
    if (btn==MBOX_BTN_NO)
        conf.allow_unsafe_io = 0;
}

static void gui_unsafe_io_warning()
{
    if (conf.allow_unsafe_io)
        gui_mbox_init(LANG_WARNING, LANG_MENU_ALLOW_UNSAFE_IO_WARNING, MBOX_BTN_YES_NO|MBOX_DEF_BTN2|MBOX_TEXT_CENTER, unsafe_io_warning);
}
#endif

//-------------------------------------------------------------------
static const char* gui_console_show_enum[]={ "ALT", "Always" };

static void gui_console_clear(__attribute__ ((unused))int arg)
{
    console_close();
    gui_mbox_init(LANG_MENU_CONSOLE_CLEAR, LANG_MENU_CONSOLE_RESET, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
}

extern void display_console();

static CMenuItem console_settings_submenu_items[] = {
    MENU_ENUM2(0x5f,LANG_MENU_CONSOLE_SHOWIN,       &conf.console_show,         gui_console_show_enum ),
    MENU_ITEM(0x58,LANG_MENU_CONSOLE_TIMEOUT,       MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.console_timeout, MENU_MINMAX(3, 30) ),
    MENU_ITEM(0x35,LANG_MENU_CONSOLE_SHOW,          MENUITEM_PROC,              display_console, 0 ),
    MENU_ITEM(0x35,LANG_MENU_CONSOLE_CLEAR,         MENUITEM_PROC,              gui_console_clear, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                  MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu console_settings_submenu = {0x28,LANG_MENU_CONSOLE_SETTINGS, console_settings_submenu_items };

static CMenuItem misc_submenu_items[] = {
    MENU_ITEM   (0x35,LANG_MENU_MISC_FILE_BROWSER,          MENUITEM_PROC,                  gui_draw_fselect,                   0 ),
    MENU_ITEM   (0x28,LANG_MENU_MODULES,                    MENUITEM_SUBMENU,               &module_submenu,                    0 ),
    MENU_ITEM   (0x37,LANG_MENU_MISC_TEXT_READER,           MENUITEM_SUBMENU,               &reader_submenu,                    0 ),
    MENU_ITEM   (0x38,LANG_MENU_MISC_GAMES,                 MENUITEM_SUBMENU_PROC,          gui_games_menu,                     0 ),
    MENU_ITEM   (0x28,LANG_MENU_MISC_TOOLS,                 MENUITEM_SUBMENU_PROC,          gui_tools_menu,                     0 ),
    MENU_ITEM   (0x28,LANG_MENU_CONSOLE_SETTINGS,           MENUITEM_SUBMENU,               &console_settings_submenu, 0 ),
#if CAM_SWIVEL_SCREEN
    MENU_ITEM   (0x5c,LANG_MENU_MISC_FLASHLIGHT,            MENUITEM_BOOL,                  &conf.flashlight, 0 ),
#endif
    MENU_ITEM   (0x80,LANG_MENU_MISC_BUILD_INFO,            MENUITEM_PROC,                  gui_show_build_info, 0 ),
    MENU_ITEM   (0x80,LANG_MENU_MISC_MEMORY_INFO,           MENUITEM_PROC,                  gui_show_memory_info, 0 ),
#if !defined(OPT_FORCE_LUA_CALL_NATIVE)
    MENU_ITEM   (0x5c,LANG_MENU_ENABLE_LUA_NATIVE_CALLS,    MENUITEM_BOOL|MENUITEM_ARG_CALLBACK, &conf.script_allow_lua_native_calls, (int)gui_lua_native_call_warning ),
#endif
#if defined(CAM_IS_VID_REC_WORKS)
    MENU_ITEM   (0x5c,LANG_MENU_ENABLE_UNSAFE_IO,           MENUITEM_BOOL|MENUITEM_ARG_CALLBACK, &conf.allow_unsafe_io, (int)gui_unsafe_io_warning ),
#endif
#if defined(CAM_DRYOS)
    MENU_ITEM   (0x5c,LANG_MENU_DISABLE_LFN_SUPPORT,        MENUITEM_BOOL,                  &conf.disable_lfn_parser_ui, 0 ),
#endif
    MENU_ITEM   (0x33,LANG_SD_CARD,                         MENUITEM_SUBMENU,               &sdcard_submenu,                    0 ),
#ifdef OPT_DEBUGGING
    MENU_ITEM   (0x2a,LANG_MENU_MAIN_DEBUG,                 MENUITEM_SUBMENU,               &debug_submenu,                     0 ),
#endif
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0,                                  0 ),
    {0},
};

static CMenu misc_submenu = {0x29,LANG_MENU_MISC_TITLE, misc_submenu_items };

//-------------------------------------------------------------------

static void cb_perc()
{
    conf.batt_volts_show=0;
}

static void cb_volts()
{
    conf.batt_perc_show=0;
}

static void cb_batt_max()
{
    if (conf.batt_volts_max < conf.batt_volts_min + 25)
        conf.batt_volts_min = conf.batt_volts_max - 25;
}

static void cb_batt_min()
{
    if (conf.batt_volts_min > conf.batt_volts_max - 25)
        conf.batt_volts_max = conf.batt_volts_min + 25;
}

static CMenuItem battery_submenu_items[] = {
    MENU_ITEM   (0x66,LANG_MENU_BATT_VOLT_MAX,              MENUITEM_INT|MENUITEM_ARG_CALLBACK,     &conf.batt_volts_max,   (int)cb_batt_max ),
    MENU_ITEM   (0x67,LANG_MENU_BATT_VOLT_MIN,              MENUITEM_INT|MENUITEM_ARG_CALLBACK,     &conf.batt_volts_min,   (int)cb_batt_min ),
    MENU_ITEM   (0x0 ,(int)"",                              MENUITEM_SEPARATOR,                     0,                      0 ),
    MENU_ITEM   (0x73,LANG_MENU_BATT_SHOW_PERCENT,          MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,    &conf.batt_perc_show,   (int)cb_perc ),
    MENU_ITEM   (0x73,LANG_MENU_BATT_SHOW_VOLTS,            MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,    &conf.batt_volts_show,  (int)cb_volts ),
    MENU_ITEM   (0x32,LANG_MENU_BATT_SHOW_ICON,             MENUITEM_BOOL,                          &conf.batt_icon_show,   0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                            0,                      0 ),
    {0}
};

static CMenu battery_submenu = {0x32,LANG_MENU_BATT_TITLE, battery_submenu_items };

//-------------------------------------------------------------------

void cb_space_perc()
{
    conf.space_mb_show=0;
}

void cb_space_mb()
{
    conf.space_perc_show=0;
}

static const char* gui_space_bar_modes[] =                  { "Don't", "Horizontal", "Vertical"};
static const char* gui_space_bar_size_modes[] =             { "1/4", "1/2", "1"};
static const char* gui_space_bar_width_modes[] =            { "1", "2", "3","4","5","6","7","8","9","10"};
static const char* gui_space_warn_type_modes[] =            { "Percent", "MB", "Don't"};

static CMenuItem space_submenu_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_SPACE_SHOW_ICON,            MENUITEM_BOOL,                          &conf.space_icon_show,  0 ),
    MENU_ENUM2  (0x69,LANG_MENU_SPACE_SHOW_BAR,             &conf.space_bar_show,                   gui_space_bar_modes ),
    MENU_ENUM2  (0x6a,LANG_MENU_SPACE_BAR_SIZE,             &conf.space_bar_size,                   gui_space_bar_size_modes ),
    MENU_ENUM2  (0x6b,LANG_MENU_SPACE_BAR_WIDTH,            &conf.space_bar_width,                  gui_space_bar_width_modes ),
    MENU_ITEM   (0x5c,LANG_MENU_SPACE_SHOW_PERCENT,         MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,    &conf.space_perc_show, (int)cb_space_perc ),
    MENU_ITEM   (0x5c,LANG_MENU_SPACE_SHOW_MB,              MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,    &conf.space_mb_show,   (int)cb_space_mb ),
#if CAM_MULTIPART
    MENU_ITEM   (0x5c,LANG_MENU_SHOW_PARTITION_NR,          MENUITEM_BOOL,                          &conf.show_partition_nr, 0 ),
#endif
    MENU_ENUM2  (0x5f,LANG_MENU_SPACE_WARN_TYPE,            &conf.space_warn_type,                  gui_space_warn_type_modes ),
    MENU_ITEM   (0x58,LANG_MENU_SPACE_WARN_PERCENT,         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &conf.space_perc_warn,    MENU_MINMAX(1, 99) ),
    MENU_ITEM   (0x58,LANG_MENU_SPACE_WARN_MB,              MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &conf.space_mb_warn,      MENU_MINMAX(1, 4000) ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,            0,                      0 ),
    {0}
};

static CMenu space_submenu = {0x33,LANG_MENU_OSD_SPACE_PARAMS_TITLE, space_submenu_items};

//-------------------------------------------------------------------

static const char* gui_dof_show_value_modes[] =             { "Don't", "Separate", "+Separate", "In Misc", "+In Misc" };

static CMenuItem dof_submenu_items[] = {
    MENU_ENUM2  (0x5f,LANG_MENU_OSD_SHOW_DOF_CALC,          &conf.show_dof,     gui_dof_show_value_modes ),
    MENU_ITEM   (0x5c,LANG_MENU_DOF_SUBJ_DIST_AS_NEAR_LIMIT,MENUITEM_BOOL,      &conf.dof_subj_dist_as_near_limit,  0 ),
    MENU_ITEM   (0x5c,LANG_MENU_DOF_USE_EXIF_SUBJ_DIST,     MENUITEM_BOOL,      &conf.dof_use_exif_subj_dist,       0 ),
    MENU_ITEM   (0x5c,LANG_MENU_DOF_SUBJ_DIST_IN_MISC,      MENUITEM_BOOL,      &conf.dof_subj_dist_in_misc,        0 ),
    MENU_ITEM   (0x5c,LANG_MENU_DOF_NEAR_LIMIT_IN_MISC,     MENUITEM_BOOL,      &conf.dof_near_limit_in_misc,       0 ),
    MENU_ITEM   (0x5c,LANG_MENU_DOF_FAR_LIMIT_IN_MISC,      MENUITEM_BOOL,      &conf.dof_far_limit_in_misc,        0 ),
    MENU_ITEM   (0x5c,LANG_MENU_DOF_HYPERFOCAL_IN_MISC,     MENUITEM_BOOL,      &conf.dof_hyperfocal_in_misc,       0 ),
    MENU_ITEM   (0x5c,LANG_MENU_DOF_DEPTH_LIMIT_IN_MISC,    MENUITEM_BOOL,      &conf.dof_depth_in_misc,            0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,        0,                                  0 ),
    {0}
};

static CMenu dof_submenu = {0x31,LANG_MENU_DOF_TITLE, dof_submenu_items };

//-------------------------------------------------------------------

static const char* gui_zoom_value_modes[] =                 { "X", "FL", "EFL" };
static const char* gui_show_values_modes[] =                { "Don't", "Always", "Shoot" };

static CMenuItem values_submenu_items[] = {
    MENU_ENUM2  (0x5f,LANG_MENU_OSD_SHOW_MISC_VALUES,       &conf.show_values,  gui_show_values_modes ),
    MENU_ITEM   (0x5c,LANG_MENU_SHOW_VALUES_IN_VIDEO,       MENUITEM_BOOL,      &conf.show_values_in_video,                 0 ),
    MENU_ITEM   (0x5c,LANG_MENU_VALUES_SHOW_ZOOM,           MENUITEM_BOOL,      &conf.values_show_zoom,                     0 ),
    MENU_ENUM2  (0x5f,LANG_MENU_OSD_ZOOM_VALUE,             &conf.zoom_value,   gui_zoom_value_modes ),
    MENU_ITEM   (0x60,LANG_MENU_OSD_ZOOM_SCALE,             MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.zoom_scale,   MENU_MINMAX(0, 1000) ),
    MENU_ITEM   (0x62,LANG_MENU_VALUES_SHOW_REAL_APERTURE,  MENUITEM_BOOL,      &conf.values_show_real_aperture,            0 ),
    MENU_ITEM   (0x74,LANG_MENU_VALUES_SHOW_REAL_ISO,       MENUITEM_BOOL,      &conf.values_show_real_iso,                 0 ),
    MENU_ITEM   (0x74,LANG_MENU_VALUES_SHOW_MARKET_ISO,     MENUITEM_BOOL,      &conf.values_show_market_iso,               0 ),
    MENU_ITEM   (0x2d,LANG_MENU_SHOW_ISO_ONLY_IN_AUTOISO_MODE, MENUITEM_BOOL,   &conf.values_show_iso_only_in_autoiso_mode, 0 ),
    MENU_ITEM   (0x5c,LANG_MENU_VALUES_SHOW_EV_SETED,       MENUITEM_BOOL,      &conf.values_show_ev_seted,                 0 ),
    MENU_ITEM   (0x5c,LANG_MENU_VALUES_SHOW_EV_MEASURED,    MENUITEM_BOOL,      &conf.values_show_ev_measured,              0 ),
    MENU_ITEM   (0x5c,LANG_MENU_VALUES_SHOW_BV_SETED,       MENUITEM_BOOL,      &conf.values_show_bv_seted,                 0 ),
    MENU_ITEM   (0x5c,LANG_MENU_VALUES_SHOW_BV_MEASURED,    MENUITEM_BOOL,      &conf.values_show_bv_measured,              0 ),
    MENU_ITEM   (0x5c,LANG_MENU_VALUES_SHOW_OVEREXPOSURE,   MENUITEM_BOOL,      &conf.values_show_overexposure,             0 ),
    MENU_ITEM   (0x5c,LANG_MENU_SHOW_CANON_OVEREXPOSURE,    MENUITEM_BOOL,      &conf.values_show_canon_overexposure,       0 ),
    MENU_ITEM   (0x5c,LANG_MENU_VALUES_SHOW_LUMINANCE,      MENUITEM_BOOL,      &conf.values_show_luminance,                0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,        0,                                          0 ),
    {0}
};

static CMenu values_submenu = {0x28,LANG_MENU_OSD_VALUES_TITLE, values_submenu_items };

//-------------------------------------------------------------------

static const char* gui_show_clock_modes[]=                  { "Don't", "Normal", "Seconds"};
static const char* gui_clock_format_modes[] =               { "24h", "12h"};
static const char* gui_clock_indicator_modes[] =            { "PM", "P", "."};
static const char* gui_clock_halfpress_modes[] =            { "Full", "Seconds", "Don't"};

static CMenuItem clock_submenu_items[] = {
    MENU_ENUM2  (0x5f,LANG_MENU_OSD_SHOW_CLOCK,             &conf.show_clock,       gui_show_clock_modes ),
    MENU_ENUM2  (0x6d,LANG_MENU_OSD_CLOCK_FORMAT,           &conf.clock_format,     gui_clock_format_modes ),
    MENU_ENUM2  (0x6c,LANG_MENU_OSD_CLOCK_INDICATOR,        &conf.clock_indicator,  gui_clock_indicator_modes ),
    MENU_ENUM2  (0x6e,LANG_MENU_OSD_CLOCK_HALFPRESS,        &conf.clock_halfpress,  gui_clock_halfpress_modes ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,            0,                              0 ),
    {0}
};

static CMenu clock_submenu = {0x34,LANG_MENU_OSD_CLOCK_PARAMS_TITLE, clock_submenu_items };

//-------------------------------------------------------------------

#if !CAM_VIDEO_QUALITY_ONLY
const char* gui_video_bitrate_enum(int change, __attribute__ ((unused))int arg)
{
    static const char *modes[]={ "0.25x", "0.5x","0.75x", "1x", "1.25x", "1.5x", "1.75x", "2x", "2.5x", "3x"};
    gui_enum_value_change(&conf.video_bitrate,change,sizeof(modes)/sizeof(modes[0]));

    if (change)
        shooting_video_bitrate_change(conf.video_bitrate);

    return modes[conf.video_bitrate];
}
#endif
#ifdef CAM_MOVIEREC_NEWSTYLE
const char* gui_video_min_bitrate_enum(int change, __attribute__ ((unused))int arg)
{
    gui_enum_value_change(&conf.video_quality,change,10);

    if (change)
        shooting_video_minbitrate_change(conf.video_quality);

    sprintf(buf, "%d%%", (conf.video_quality+1)*10);
    return buf;
}
#endif
#if CAM_AF_SCAN_DURING_VIDEO_RECORD
static const char* gui_video_af_key_enum(int change, __attribute__ ((unused))int arg)
{
    static const char* names[] = CAM_VIDEO_AF_BUTTON_NAMES; 
    static const int keys[] = CAM_VIDEO_AF_BUTTON_OPTIONS; 
    int i; 
 
    for (i=0; i<(int)(sizeof(names)/sizeof(names[0])); ++i) { 
        if (conf.video_af_key==keys[i]) { 
            break; 
        } 
    } 
 
    i+=change; 
    if (i<0) 
        i=(sizeof(names)/sizeof(names[0]))-1; 
    else if (i>=(int)(sizeof(names)/sizeof(names[0]))) 
        i=0; 
 
    conf.video_af_key = keys[i]; 
    return names[i]; 
}
#endif

static const char* gui_show_movie_time_modes[] =            { "Don't", "hh:mm:ss", "KB/s","both"};
#if CAM_CHDK_HAS_EXT_VIDEO_MENU
#if !CAM_VIDEO_QUALITY_ONLY
    #ifndef CAM_MOVIEREC_NEWSTYLE
        static const char* gui_video_mode_modes[] =             { "Bitrate", "Quality"};
    #else
        static const char* gui_video_mode_modes[] =             { "Default", "CBR", "VBR HI", "VBR MID", "VBR LOW"};
    #endif
#else
    static const char* gui_video_mode_modes[] =             { "Default", "Quality"};
#endif
#endif // HAS_EXT_VIDEO_MENU

#ifdef CAM_CLEAN_OVERLAY
    static const char* gui_clean_overlay_modes[] =          { "Never", "Rec", "MviRec"};
#endif

static CMenuItem video_submenu_items[] = {
#if CAM_CHDK_HAS_EXT_VIDEO_MENU
    MENU_ENUM2  (0x23,LANG_MENU_VIDEO_MODE,                 &conf.video_mode,       gui_video_mode_modes ),
#if !CAM_VIDEO_QUALITY_ONLY
    MENU_ITEM   (0x5e,LANG_MENU_VIDEO_BITRATE,              MENUITEM_ENUM,          gui_video_bitrate_enum,             0 ),
#endif
#ifndef CAM_MOVIEREC_NEWSTYLE
    MENU_ITEM   (0x60,LANG_MENU_VIDEO_QUALITY,              MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.video_quality, MENU_MINMAX(1, 99) ),
#else
    MENU_ITEM   (0x60,LANG_MENU_VIDEO_VBR_MIN,              MENUITEM_ENUM,          gui_video_min_bitrate_enum,         0 ),
#endif
#if CAM_CHDK_HAS_EXT_VIDEO_TIME
    MENU_ITEM   (0x5c,LANG_MENU_VIDEO_EXT_TIME,             MENUITEM_BOOL,          &conf.ext_video_time,               0 ),
#endif
    MENU_ITEM   (0x5c,LANG_MENU_CLEAR_VIDEO_VALUES,         MENUITEM_BOOL,          &conf.clear_video,                  0 ),
#endif
#if CAM_VIDEO_CONTROL
    MENU_ITEM   (0x5c,LANG_MENU_FAST_SWITCH_VIDEO,          MENUITEM_BOOL,          &conf.fast_movie_control,           0 ),
#endif
#if CAM_CHDK_HAS_EXT_VIDEO_MENU && !defined(CAM_MOVIEREC_NEWSTYLE)
    MENU_ITEM   (0x5c,LANG_MENU_FAST_SWITCH_QUALITY_VIDEO,  MENUITEM_BOOL,          &conf.fast_movie_quality_control,   0 ),
#endif
#if CAM_CAN_UNLOCK_OPTICAL_ZOOM_IN_VIDEO
    MENU_ITEM   (0x5c,LANG_MENU_OPTICAL_ZOOM_IN_VIDEO,      MENUITEM_BOOL,          &conf.unlock_optical_zoom_for_video, 0 ),
#endif
#if CAM_CAN_MUTE_MICROPHONE
    MENU_ITEM   (0x83,LANG_MENU_MUTE_ON_ZOOM,               MENUITEM_BOOL,          &conf.mute_on_zoom,                 0 ),
#endif
#if CAM_AF_SCAN_DURING_VIDEO_RECORD
    MENU_ITEM   (0x82,LANG_MENU_VIDEO_AF_KEY,               MENUITEM_ENUM,          gui_video_af_key_enum,              0 ),
#endif
    MENU_ENUM2  (0x5c,LANG_MENU_OSD_SHOW_VIDEO_TIME,        &conf.show_movie_time,  gui_show_movie_time_modes ),
    MENU_ITEM   (0x60,LANG_MENU_OSD_SHOW_VIDEO_REFRESH,     MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.show_movie_refresh,   MENU_MINMAX(1, 20) ),
#ifdef CAM_CLEAN_OVERLAY
    MENU_ENUM2  (0x7f,LANG_MENU_CLEAN_OVERLAY,              &conf.clean_overlay,    gui_clean_overlay_modes ),
#endif
#ifdef CAM_UNLOCK_ANALOG_AV_IN_REC
    MENU_ITEM   (0x83,LANG_MENU_UNLOCK_AV_OUT_IN_REC,       MENUITEM_BOOL,          &conf.unlock_av_out_in_rec,         0 ),
#endif
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,            0,                                  0 ),
    {0}
};

static CMenu video_submenu = {0x23,LANG_MENU_VIDEO_PARAM_TITLE, video_submenu_items };

//-------------------------------------------------------------------
// "Extra Photo Operations" Menu

static const char* tv_override[]={
#ifdef CAM_EXT_TV_RANGE
    // add very long time exposures as approximately powers of 2, adding 15 exposures
    "2048","1625","1290","1024","812","645","512","406","322","256","203","161","128","101","80",
#endif
    "64","50.8", "40.3", "32", "25.4","20","16", "12.7", "10","8", "6.3","5","4","3.2", "2.5","2", 
    "1.6", "1.3", "1", "0.8", "0.6", "0.5", "0.4", "0.3", "1/4", "1/5", "1/6", "1/8", "1/10", "1/13", 
    "1/15", "1/20", "1/25", "1/30", "1/40", "1/50", "1/60", "1/80", "1/100", "1/125", "1/160", "1/200", 
    "1/250", "1/320", "1/400", "1/500", "1/640","1/800", "1/1000", "1/1250", "1/1600","1/2000","1/2500",
    "1/3200","1/4000", "1/5000", "1/6400", "1/8000", "1/10000", "1/12500", "1/16000", "1/20000", "1/25000", 
    "1/32000", "1/40000", "1/50000", "1/64000","1/80000", "1/100k"
};

const char* tv_override_value_string()
{
    return tv_override[conf.tv_override_value]; 
}

static const char* gui_tv_enum_type[] = {
    "Ev Step", "ShrtExp"
#ifdef CAM_EXT_TV_RANGE
    , "LongExp"
#endif
};

#if CAM_HAS_IRIS_DIAPHRAGM
const char* gui_av_override_enum(int change, __attribute__ ((unused))int arg)
{
    conf.av_override_value+=change;
    if (conf.av_override_value<0) conf.av_override_value=shooting_get_aperture_sizes_table_size()+CAM_EXT_AV_RANGE-1;
    else if (conf.av_override_value>shooting_get_aperture_sizes_table_size()+CAM_EXT_AV_RANGE-1) conf.av_override_value=0;

    short prop_id = shooting_get_aperture_from_av96(shooting_get_av96_override_value())/10;
    sprintf(buf, "%d.%02d", (int)prop_id/100, (int)prop_id%100 );
    return buf;
}
#endif

const char* gui_subj_dist_override_value_enum(int change, __attribute__ ((unused))int arg)
{
    if (conf.subj_dist_override_koef == SD_OVERRIDE_INFINITY)  // Infinity selected
        strcpy(buf,"   Inf.");
    else
    {
        // Increment / decrement the SD value, wrapping around from CAMERA_MIN_DIST to CAMERA_MAX_DIST
        conf.subj_dist_override_value += (change/**koef*/);
        if (conf.subj_dist_override_value < CAMERA_MIN_DIST)
            conf.subj_dist_override_value = CAMERA_MAX_DIST;
        else if (conf.subj_dist_override_value > CAMERA_MAX_DIST)
            conf.subj_dist_override_value = CAMERA_MIN_DIST;
        // philmoz 19/6/2014 - if SD override is < distance from sensor to front of lens (for current zoom) then adjust SD override
        if (conf.subj_dist_override_value < shooting_get_lens_to_focal_plane_width())
            conf.subj_dist_override_value = shooting_get_lens_to_focal_plane_width();
        sprintf(buf, "%7d", shooting_get_subject_distance_override_value());
    }

    return buf; 
}

const char* gui_subj_dist_override_koef_enum(int change, __attribute__ ((unused))int arg)
{
    static const char* modes[] = { "Off", "On", "Inf" };
    return gui_change_simple_enum(&conf.subj_dist_override_koef,change,modes,sizeof(modes)/sizeof(modes[0]));
}

#if defined(OPT_CURVES)

const char* gui_conf_curve_enum(int change, __attribute__ ((unused))int arg) {
    static const char* modes[]={ "None", "Custom", "+1EV", "+2EV", "Auto DR" };

    gui_enum_value_change(&conf.curve_enable,change,sizeof(modes)/sizeof(modes[0]));

    if (change)
        libcurves->curve_init_mode();

    return modes[conf.curve_enable];
}

static void gui_load_curve_selected(const char *fn)
{
    if (fn) {
        // TODO we could sanity check here, but curve_set_type should fail gracefullish
        strcpy(conf.curve_file,fn);
        if (conf.curve_enable == 1)
            libcurves->curve_init_mode();
    }
}

static void gui_load_curve(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_SELECT_CURVE_FILE, conf.curve_file, CURVE_DIR, gui_load_curve_selected);
}

static CMenuItem curve_submenu_items[] = {
    MENU_ITEM(0x5f,LANG_MENU_CURVE_ENABLE,        MENUITEM_ENUM,      gui_conf_curve_enum, &conf.curve_enable ),
    MENU_ITEM(0x35,LANG_MENU_CURVE_LOAD,          MENUITEM_PROC,      gui_load_curve, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu curve_submenu = {0x85,LANG_MENU_CURVE_PARAM_TITLE, curve_submenu_items };

#endif

// Display & edit an int value as a decimal.
// Value ranges from 0 - 999999; but display shows as 0.00000 - 9.99999
const char* gui_decimal_enum(int change, int arg)
{
    int *v = (int*)arg;

    *v += change;
    if (*v < 0) *v = 0;
    if (*v > 999999) *v = 999999;

    sprintf(buf, "%01d.%05d", (int)(*v / 100000), (int)(*v % 100000));

    return buf;
}

// Modify and display a value as H:MM:SS
// For storing a value as a number of seconds internally; but displaying as a time value
const char* gui_hhmss_enum(int change, int arg)
{
    int *v = (int*)arg;

    int h, m, s;
    h = *v / 3600;
    m = (*v % 3600) / 60;
    s = *v % 60;

    switch (change)
    {
    case 1:
    case -1:
        s += change;
        if (s < 0) s = 59;
        if (s > 59) s = 0;
        break;
    case 10:
    case -10:
        m += change / 10;
        if (m < 0) m = 59;
        if (m > 59) m = 0;
        break;
    default:
        h += change /100;
        if (h < 0) h = 1;
        if (h > 1) h = 0;
        break;
    }
    *v = (h * 3600) + (m * 60) + s;

    sprintf(buf, "%1d:%02d:%02d", h, m, s);

    return buf;
}

static const char* gui_override_disable_modes[] =           { "No", "Yes" };
#if CAM_HAS_ND_FILTER
static const char* gui_nd_filter_state_modes[] =            { "Off", "In", "Out" };
#endif
static const char* gui_fast_ev_step_modes[] =               { "1/6 Ev","1/3 Ev","1/2 Ev", "2/3 Ev","5/6 Ev","1 Ev","1 1/6Ev","1 1/3Ev","1 1/2Ev", "1 2/3Ev","1 5/6Ev","2 Ev","2 1/6Ev","2 1/3Ev","2 1/2Ev", "2 2/3Ev","2 5/6Ev","3 Ev","3 1/6Ev","3 1/3Ev","3 1/2Ev", "3 2/3Ev","3 5/6Ev","4 Ev"};
#if CAM_QUALITY_OVERRIDE
const char* gui_fast_image_quality_modes[] =         { "Sup.Fine", "Fine", "Normal", "Off" };
#endif

#ifdef CAM_HOTSHOE_OVERRIDE
static const char* gui_hotshoe_override_modes[] = { (char*)LANG_MENU_HOTSHOE_OVERRIDE_OFF, (char*)LANG_MENU_HOTSHOE_EMPTY, (char*)LANG_MENU_HOTSHOE_USED };
#endif

static const char* gui_flash_power_modes[] = { "Min", "Med", "Max" };

const char* flash_power_mode_string()
{
    return gui_flash_power_modes[conf.flash_video_override_power];
}

static const char* gui_flash_exp_comp_modes[] = { "-3", "-2.6", "-2.3", "-2", "-1.6", "-1.3", "-1", "-2/3", "-1/3", "0", "+1/3", "+2/3", "+1", "+1.3", "+1.6", "+2", "+2.3", "+2.6", "+3" };

const char* flash_exp_comp_modes_string()
{
    return gui_flash_exp_comp_modes[conf.flash_exp_comp];
}

static void cb_change_flash_power()
{
    if (conf.flash_manual_override) conf.flash_enable_exp_comp = 0;
}

static void cb_change_flash_exp_comp()
{
    if (conf.flash_enable_exp_comp) conf.flash_manual_override = 0;
}

static CMenuItem tv_override_evstep[2] = {
    MENU_ENUM2  (0, LANG_MENU_OVERRIDE_TV_VALUE,  &conf.tv_override_value,  tv_override ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.tv_override_enabled,          0 ),
};

#ifdef CAM_EXT_TV_RANGE
static CMenuItem tv_override_long_exp[2] = {
    MENU_ITEM   (0, LANG_MENU_OVERRIDE_TV_LONG_EXP,  MENUITEM_ENUM|MENUITEM_HHMMSS, gui_hhmss_enum,             &conf.tv_override_long_exp ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.tv_override_enabled,          0 ),
};
#endif

static CMenuItem tv_override_short_exp[2] = {
    MENU_ITEM   (0, LANG_MENU_OVERRIDE_TV_SHORT_EXP, MENUITEM_ENUM|MENUITEM_DECIMAL, gui_decimal_enum,          &conf.tv_override_short_exp ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.tv_override_enabled,          0 ),
};

static CMenuItem iso_override_items[2] = {
    MENU_ITEM   (0, 0,  MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.iso_override_value,           MENU_MINMAX(0, 10000) ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.iso_override_koef,            0),
};

static CMenuItem fast_ev_switch[2] = {
    MENU_ENUM2  (0, 0,                                                      &conf.fast_ev_step,                 gui_fast_ev_step_modes ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.fast_ev,                      0 ),
};

static CMenuItem manual_flash[2] = {
    MENU_ENUM2  (0, 0,  &conf.flash_video_override_power,                   gui_flash_power_modes ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,                &conf.flash_manual_override,        (int)cb_change_flash_power ),
};

static CMenuItem flash_exp_comp[2] = {
    MENU_ENUM2  (0, 0,  &conf.flash_exp_comp,                               gui_flash_exp_comp_modes ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,                &conf.flash_enable_exp_comp,        (int)cb_change_flash_exp_comp ),
};

#if CAM_HAS_IRIS_DIAPHRAGM
static CMenuItem av_override_items[2] = {
    MENU_ITEM   (0, 0,  MENUITEM_ENUM,                                      gui_av_override_enum,               0 ),
    MENU_ITEM   (0, 0,  MENUITEM_BOOL,                                      &conf.av_override_enabled,          0 ),
};
#endif

static CMenuItem sd_override_items[2] = {
    MENU_ITEM   (0, 0,   MENUITEM_ENUM|MENUITEM_SD_INT,                     gui_subj_dist_override_value_enum,  CAMERA_MAX_DIST ),
    MENU_ITEM   (0, 0,   MENUITEM_ENUM,                                     gui_subj_dist_override_koef_enum,   0 ),
};

static const char* gui_raw_nr_modes[] =                     { "Auto", "Off", "On"};

static CMenuItem operation_submenu_items[] = {
    MENU_ENUM2  (0x5f,LANG_MENU_OVERRIDE_DISABLE,           &conf.override_disable, gui_override_disable_modes ),
    MENU_ITEM   (0x5c,LANG_MENU_OVERRIDE_DISABLE_ALL,       MENUITEM_BOOL,          &conf.override_disable_all,         0 ),
    MENU_ENUM2  (0x59,LANG_MENU_TV_ENUM_TYPE,               &conf.tv_enum_type,     gui_tv_enum_type ),
    MENU_ITEM   (0x61,LANG_MENU_OVERRIDE_TV_VALUE,          MENUITEM_STATE_VAL_PAIR,&tv_override_evstep,                0 ),
#if CAM_HAS_IRIS_DIAPHRAGM
    MENU_ITEM   (0x62,LANG_MENU_OVERRIDE_AV_VALUE,          MENUITEM_STATE_VAL_PAIR,&av_override_items,                 0 ),
#endif
    MENU_ITEM   (0x74,LANG_MENU_OVERRIDE_ISO_VALUE,         MENUITEM_STATE_VAL_PAIR,&iso_override_items,                10 ),
    MENU_ITEM   (0x5e,LANG_MENU_OVERRIDE_SUBJ_DIST_VALUE,   MENUITEM_STATE_VAL_PAIR,&sd_override_items,                 0 ),
    MENU_ITEM   (0x5c,LANG_MENU_MISC_FAST_EV,               MENUITEM_STATE_VAL_PAIR,&fast_ev_switch,                    0 ),
    MENU_ITEM   (0x5c, LANG_MENU_FLASH_EXP_COMP,            MENUITEM_STATE_VAL_PAIR,&flash_exp_comp,                    0 ),
    MENU_ITEM   (0x5c, LANG_MENU_FLASH_MANUAL_OVERRIDE,     MENUITEM_STATE_VAL_PAIR,&manual_flash,                      0 ),
#if CAM_HAS_VIDEO_BUTTON
    MENU_ITEM   (0x5c, LANG_MENU_FLASH_VIDEO_OVERRIDE,      MENUITEM_BOOL,          &conf.flash_video_override,         0 ),
#endif
#if CAM_REAR_CURTAIN
    MENU_ITEM   (0x5c, LANG_MENU_REAR_CURTAIN,              MENUITEM_BOOL,          &conf.flash_sync_curtain,           0 ),
#endif
#ifdef CAM_HOTSHOE_OVERRIDE
    MENU_ENUM2  (0x5c, LANG_MENU_HOTSHOE_OVERRIDE,          &conf.hotshoe_override, gui_hotshoe_override_modes ),
#endif
#if CAM_HAS_ND_FILTER
    MENU_ENUM2  (0x5f,LANG_MENU_OVERRIDE_ND_FILTER,         &conf.nd_filter_state,  gui_nd_filter_state_modes ),
#endif
    MENU_ENUM2  (0x5f,LANG_MENU_RAW_NOISE_REDUCTION,        &conf.raw_nr,       gui_raw_nr_modes ), // Dark Frame Subtraction - despite label has nothing to do with RAW
#if CAM_QUALITY_OVERRIDE
    MENU_ENUM2  (0x5c,LANG_MENU_MISC_IMAGE_QUALITY,         &conf.fast_image_quality, gui_fast_image_quality_modes ),
#endif
    MENU_ITEM   (0x2c,LANG_MENU_BRACKET_IN_CONTINUOUS,      MENUITEM_SUBMENU,       &bracketing_in_continuous_submenu,  0 ),
    MENU_ITEM   (0x2d,LANG_MENU_AUTOISO,                    MENUITEM_SUBMENU,       &autoiso_submenu,                   0 ),
#ifdef OPT_CURVES
    MENU_ITEM   (0x85,LANG_MENU_CURVE_PARAM,                MENUITEM_SUBMENU,       &curve_submenu,                     0 ),
#endif
    MENU_ITEM   (0x5b,LANG_MENU_CLEAR_OVERRIDE_VALUES,      MENUITEM_BOOL,          &conf.clear_override,               0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,        0,                                  0 ),
    {0}
};

static CMenu operation_submenu = {0x21,LANG_MENU_OPERATION_PARAM_TITLE, operation_submenu_items };

void set_tv_override_menu(CMenuItem *mi)
{
    if (mi->text == LANG_MENU_OVERRIDE_TV_VALUE)
    {
        switch (conf.tv_enum_type)
        {
        case 0:     // Ev Step
            mi->state_val_items = tv_override_evstep;
            mi->arg = 1;
            break;
        case 1:     // Short exposure
            mi->state_val_items = tv_override_short_exp;
            mi->arg = 100;
            break;
#ifdef CAM_EXT_TV_RANGE
        case 2:     // Long exposure
            mi->state_val_items = tv_override_long_exp;
            mi->arg = 1;
            break;
#endif
        }
    }
}

//-------------------------------------------------------------------

static void gui_load_edge_selected( const char* fn )
{
    if (fn)
        libedgeovr->load_edge_overlay(fn);
}

static void gui_menuproc_edge_save(__attribute__ ((unused))int arg)
{
    libedgeovr->save_edge_overlay();
}

static void gui_menuproc_edge_load(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_MENU_EDGE_LOAD, EDGE_SAVE_DIR, EDGE_SAVE_DIR, gui_load_edge_selected);
}

static const char* gui_edge_pano_modes[] =                  { "Off", "Right", "Down", "Left", "Up", "Free" };

static CMenuItem edge_overlay_submenu_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_EDGE_OVERLAY_ENABLE,        MENUITEM_BOOL,          &conf.edge_overlay_enable,  0 ),
    MENU_ITEM   (0x5c,LANG_MENU_EDGE_FILTER,                MENUITEM_BOOL,          &conf.edge_overlay_filter,  0 ),
    MENU_ENUM2  (0x5f,LANG_MENU_EDGE_PANO,                  &conf.edge_overlay_pano, gui_edge_pano_modes ),
    MENU_ITEM   (0x5e,LANG_MENU_EDGE_PANO_OVERLAP,          MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.edge_overlay_pano_overlap, MENU_MINMAX(0, 100) ),
    MENU_ITEM   (0x5c,LANG_MENU_EDGE_SHOW,                  MENUITEM_BOOL,          &conf.edge_overlay_show,    0 ),
    MENU_ITEM   (0x5e,LANG_MENU_EDGE_OVERLAY_TRESH,         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.edge_overlay_thresh, MENU_MINMAX(0, 255) ),
    MENU_ITEM   (0x5c,LANG_MENU_EDGE_PLAY,                  MENUITEM_BOOL,          &conf.edge_overlay_play,    0 ), //does not work on cams like s-series, which dont have a real "hardware" play/rec switch, need a workaround, probably another button
    MENU_ITEM   (0x33,LANG_MENU_EDGE_SAVE,                  MENUITEM_PROC,          gui_menuproc_edge_save,     0 ),
    MENU_ITEM   (0x5c,LANG_MENU_EDGE_ZOOM,                  MENUITEM_BOOL,          &conf.edge_overlay_zoom,    0 ),
    MENU_ITEM   (0x33,LANG_MENU_EDGE_LOAD,                  MENUITEM_PROC,          gui_menuproc_edge_load,     0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,            0,                          0 ),
    {0}
};

static CMenu edge_overlay_submenu = {0x7f,LANG_MENU_EDGE_OVERLAY_TITLE, edge_overlay_submenu_items };

//-------------------------------------------------------------------

static void gui_grid_lines_load_selected(const char *fn)
{
    if (fn)
    {
        libgrids->grid_lines_load(fn);
    }
}

static void gui_grid_lines_load(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_SELECT_GRID_FILE, conf.grid_lines_file, "A/CHDK/GRIDS", gui_grid_lines_load_selected);
}

static CMenuItem grid_submenu_items[] = {
    MENU_ITEM(0x2f,LANG_MENU_SHOW_GRID,         MENUITEM_BOOL,      &conf.show_grid_lines, 0 ),
    MENU_ITEM(0x35,LANG_MENU_GRID_LOAD,         MENUITEM_PROC,      gui_grid_lines_load, 0 ),
    MENU_ITEM(0x0,LANG_MENU_GRID_CURRENT,       MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM(0x0,(int)conf.grid_title,         MENUITEM_TEXT,      0, 0 ),
    MENU_ITEM(0x0,(int)"",                      MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM(0x5c,LANG_MENU_GRID_FORCE_COLOR,  MENUITEM_BOOL,      &conf.grid_force_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_GRID_COLOR_LINE,   MENUITEM_COLOR_FG,  &conf.grid_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_GRID_COLOR_FILL,   MENUITEM_COLOR_BG,  &conf.grid_color, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,              MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu grid_submenu = {0x2f,LANG_MENU_GRID_TITLE, grid_submenu_items };

//-------------------------------------------------------------------

static void gui_menu_run_palette(__attribute__ ((unused))int arg)
{
    libpalette->show_palette(PALETTE_MODE_DEFAULT, (chdkColor){0,0}, NULL);
}

static void gui_menu_test_palette(__attribute__ ((unused))int arg)
{
    libpalette->show_palette(PALETTE_MODE_TEST, (chdkColor){0,0}, NULL);
}

// Write the live palette to the card - see docs/OVERLAY_DESIGN.md.
//
// The bitmap layer is 8bpp into a palette Canon owns and loads at runtime, and
// nothing in CHDK knows what is in it: the twenty IDX_COLOR_* bytes in
// platform_palette.c are byte-identical across all seven ports here, which
// means they are a template nobody ever checked against a body. That is why
// "Magenta" and "Light Yellow" are the same byte, and why a theme asking for
// pink got yellow.
//
// The palette itself is the only authority, and it is in RAM. This writes it
// out so it can be read on a computer, with each entry decoded as AYUV - the
// usual Canon layout - and the bytes CHDK claims for each colour listed
// underneath, so a wrong claim is visible by comparison.
static void gui_menu_dump_palette(__attribute__ ((unused))int arg)
{
    static char msg[64];
    unsigned char *pal = (unsigned char *)vid_get_bitmap_active_palette();
    int size = vid_get_palette_size();
    int fd, i;
    char line[96];

    if (!pal || size <= 0 || size > 4096)
    {
        sprintf(msg, "No palette to read (%d bytes)", size);
        gui_mbox_init(LANG_INFORMATION, (int)msg, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }

    mkdir("A/CHDK/LOGS");
    fd = open("A/CHDK/LOGS/PALETTE.TXT", O_WRONLY|O_CREAT|O_TRUNC, 0777);
    if (fd < 0)
    {
        gui_mbox_init(LANG_INFORMATION, (int)"Could not write A/CHDK/LOGS/PALETTE.TXT",
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }

    sprintf(line, "palette type %d, %d bytes, at %p\n",
            vid_get_palette_type(), size, pal);
    write(fd, line, strlen(line));
    sprintf(line, "mode: %s\n\n", camera_info.state.mode_rec ? "record" : "playback");
    write(fd, line, strlen(line));

    write(fd, "entry  bytes        AYUV -> RGB\n", 32);
    for (i = 0; i * 4 + 3 < size; i++)
    {
        int a = pal[i*4], y = pal[i*4+1];
        int u = pal[i*4+2], v = pal[i*4+3];
        int su = (u > 127) ? u - 256 : u;
        int sv = (v > 127) ? v - 256 : v;
        int r = y + (359 * sv) / 256;
        int g = y - (88 * su) / 256 - (183 * sv) / 256;
        int b = y + (454 * su) / 256;

        if (r < 0) r = 0;
        if (r > 255) r = 255;
        if (g < 0) g = 0;
        if (g > 255) g = 255;
        if (b < 0) b = 0;
        if (b > 255) b = 255;

        sprintf(line, "  %02x   %02x %02x %02x %02x   %02x%02x%02x%s\n",
                i, a, y, u, v, r, g, b, a ? "" : "   (transparent)");
        write(fd, line, strlen(line));
    }

    // What CHDK believes, for comparison. Every one of these is a claim that
    // can now be checked against the table above.
    write(fd, "\nwhat CHDK claims, this mode:\n", 30);
    {
        static const char * const nm[] = {
            "transparent","black","white","red","red dk","red lt","green",
            "green dk","green lt","blue","blue dk","cyan","grey","grey dk",
            "grey lt","yellow","yellow dk","yellow lt","grey dk trans","magenta" };
        for (i = 0; i <= IDX_COLOR_MAX; i++)
        {
            sprintf(line, "  %-14s byte %02x\n", nm[i], chdk_colors[i]);
            write(fd, line, strlen(line));
        }
    }

    close(fd);
    sprintf(msg, "Wrote A/CHDK/LOGS/PALETTE.TXT (%d entries)", size / 4);
    gui_mbox_init(LANG_INFORMATION, (int)msg, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
}

static void gui_menu_reset_colors_selected(unsigned int btn)
{
    if (btn==MBOX_BTN_YES)
        resetColors();
}

static void gui_menu_reset_colors(__attribute__ ((unused))int arg)
{
    gui_mbox_init(LANG_MSG_RESET_COLORS_TITLE,
                  LANG_MSG_RESET_COLORS_TEXT,
                  MBOX_FUNC_RESTORE|MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, gui_menu_reset_colors_selected);
}

static CMenuItem visual_submenu_items[] = {
    MENU_ITEM(0x65,LANG_MENU_MISC_PALETTE,            MENUITEM_PROC,      gui_menu_run_palette, 0 ),
    MENU_ITEM(0x65,LANG_MENU_COLOR_TEST,              MENUITEM_PROC,      gui_menu_test_palette, 0 ),
    MENU_ITEM(0x65,(int)"Dump palette to card",       MENUITEM_PROC,      gui_menu_dump_palette, 0 ),
    MENU_ITEM(0x65,LANG_MSG_RESET_COLORS_TITLE,       MENUITEM_PROC,      gui_menu_reset_colors, 0 ),
    MENU_ITEM(0x0,LANG_MENU_VIS_COLORS,               MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_OSD_TEXT,            MENUITEM_COLOR_FG,  &conf.osd_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_OSD_BKG,             MENUITEM_COLOR_BG,  &conf.osd_color, 0 ),
    // First, because it sets most of what the rows below then override by
    // hand - and because it is the one entry here that changes the whole
    // camera rather than one element of it.
    MENU_ENUM2a(0x5f,(int)"Theme",                    &conf.theme, theme_names, THEME_COUNT ),
    MENU_ITEM(0x65,LANG_MENU_VIS_OSD_WARNING,         MENUITEM_COLOR_FG,  &conf.osd_color_warn, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_OSD_WARNING_BKG,     MENUITEM_COLOR_BG,  &conf.osd_color_warn, 0 ),
    MENU_ITEM(0x65,LANG_MENU_EDGE_OVERLAY_COLOR,      MENUITEM_COLOR_FG,  &conf.edge_overlay_color,   0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_HISTO,               MENUITEM_COLOR_FG,  &conf.histo_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_HISTO_BKG,           MENUITEM_COLOR_BG,  &conf.histo_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_HISTO_BORDER,        MENUITEM_COLOR_FG,  &conf.histo_color2, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_HISTO_MARKERS,       MENUITEM_COLOR_BG,  &conf.histo_color2, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_ZEBRA_UNDER,         MENUITEM_COLOR_BG,  &conf.zebra_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_ZEBRA_OVER,          MENUITEM_COLOR_FG,  &conf.zebra_color, 0 ),
    //MENU_ITEM(0x65,LANG_MENU_VIS_BATT_ICON,           MENUITEM_COLOR_FG,  &conf.batt_icon_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_SPACE_ICON,          MENUITEM_COLOR_FG,  &conf.space_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_SPACE_ICON_BKG,      MENUITEM_COLOR_BG,  &conf.space_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_TEXT,           MENUITEM_COLOR_FG,  &conf.menu_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_BKG,            MENUITEM_COLOR_BG,  &conf.menu_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_TITLE_TEXT,     MENUITEM_COLOR_FG,  &conf.menu_title_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_TITLE_BKG,      MENUITEM_COLOR_BG,  &conf.menu_title_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_CURSOR_TEXT,    MENUITEM_COLOR_FG,  &conf.menu_cursor_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_CURSOR_BKG,     MENUITEM_COLOR_BG,  &conf.menu_cursor_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_SYMBOL_TEXT,    MENUITEM_COLOR_FG,  &conf.menu_symbol_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_MENU_SYMBOL_BKG,     MENUITEM_COLOR_BG,  &conf.menu_symbol_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_READER_TEXT,         MENUITEM_COLOR_FG,  &conf.reader_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_READER_BKG,          MENUITEM_COLOR_BG,  &conf.reader_color, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_OSD_OVERRIDE,         MENUITEM_COLOR_FG,  &conf.osd_color_override, 0 ),
    MENU_ITEM(0x65,LANG_MENU_VIS_OSD_OVERRIDE_BKG,     MENUITEM_COLOR_BG,  &conf.osd_color_override, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                    MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu visual_submenu = {0x28,LANG_MENU_VIS_TITLE, visual_submenu_items };

#ifdef CAM_CUSTOM_BOOT_IMAGE
static void gui_boot_screen_import(__attribute__ ((unused))int arg)
{
    module_run("bootimg.flt");
}

static CMenuItem boot_screen_submenu_items[] = {
    MENU_ITEM(0x28,(int)"Import JPG/PNG",       MENUITEM_PROC, gui_boot_screen_import, 0),
    MENU_ITEM(0x51,LANG_MENU_BACK,              MENUITEM_UP,   0, 0),
    {0}
};
static CMenu boot_screen_submenu = {0x28,(int)"Boot screen",boot_screen_submenu_items};
#endif

//-------------------------------------------------------------------

static CMenuItem raw_state_submenu_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_OSD_SHOW_RAW_STATE,         MENUITEM_BOOL,      &conf.show_raw_state,       0 ),
    MENU_ITEM   (0x5c,LANG_MENU_OSD_SHOW_REMAINING_RAW,     MENUITEM_BOOL,      &conf.show_remaining_raw,   0 ),
    MENU_ITEM   (0x60,LANG_MENU_OSD_RAW_TRESHOLD,           MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.remaining_raw_treshold,   MENU_MINMAX(0, 200) ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,        0,                          0 ),
    {0}
};

static CMenu raw_state_submenu = {0x24,LANG_MENU_OSD_RAW_STATE_PARAMS_TITLE, raw_state_submenu_items };

//-------------------------------------------------------------------

#ifdef  CAM_TOUCHSCREEN_UI

static const char* gui_touchscreen_disable_modes[]=         { "Enable", "Disable" };

static CMenuItem touchscreen_submenu_items[] = {
    MENU_ENUM2  (0x5f,LANG_MENU_TS_VIDEO_AE_DISABLE,        &conf.touchscreen_disable_video_controls,    gui_touchscreen_disable_modes ),
    MENU_ENUM2  (0x5f,LANG_MENU_TS_ALT_SHORTCUTS_DISABLE,   &conf.touchscreen_disable_shortcut_controls, gui_touchscreen_disable_modes ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,        0, 0 ),
    {0}
};

static CMenu touchscreen_submenu = {0x28,LANG_MENU_TOUCHSCREEN_VALUES, touchscreen_submenu_items };

#endif

//-------------------------------------------------------------------
static void cb_change_rotate_osd()
{
    update_draw_proc();
    gui_menu_erase_and_redraw();
}

#ifdef CAM_HAS_CMOS
    static const char* gui_temp_mode_modes[] =              { "Off", "Optical", "CMOS", "Battery", "all" };
#else
    static const char* gui_temp_mode_modes[] =              { "Off", "Optical", "CCD", "Battery", "all" };
#endif
static const char* gui_hide_osd_modes[] =                   { "Don't", "In Playback", "On Disp Press", "Both" };
static const char* gui_show_usb_info_modes[] =              { "Off", "Icon", "Text" };

static CMenuItem osd_submenu_items[] = {
    MENU_ITEM(0x5c,LANG_MENU_OSD_SHOW,              MENUITEM_BOOL,          &conf.show_osd, 0 ),
    MENU_ENUM2(0x5c,LANG_MENU_OSD_HIDE_PLAYBACK,                            &conf.hide_osd, gui_hide_osd_modes ),
    MENU_ITEM(0x5c,LANG_MENU_OSD_ROTATE,            MENUITEM_BOOL | MENUITEM_ARG_CALLBACK, &conf.rotate_osd, (int)cb_change_rotate_osd ),
    MENU_ITEM(0x5f,LANG_MENU_OSD_SHOW_STATES,       MENUITEM_BOOL,          &conf.show_state, 0 ),
    MENU_ENUM2(0x5f,LANG_MENU_OSD_SHOW_TEMP,                                &conf.show_temp, gui_temp_mode_modes ),
    MENU_ITEM(0x59,LANG_MENU_OSD_TEMP_FAHRENHEIT,   MENUITEM_BOOL,          &conf.temperature_unit, 0 ),
    MENU_ENUM2(0x71,LANG_MENU_USB_SHOW_INFO,                                &conf.usb_info_enable, gui_show_usb_info_modes ),
    MENU_ITEM(0x22,LANG_MENU_OSD_VALUES,            MENUITEM_SUBMENU,       &values_submenu, 0 ),
    MENU_ITEM(0x31,LANG_MENU_OSD_DOF_CALC,          MENUITEM_SUBMENU,       &dof_submenu, 0 ),
    MENU_ITEM(0x24,LANG_MENU_OSD_RAW_STATE_PARAMS,  MENUITEM_SUBMENU,       &raw_state_submenu, 0 ),
    MENU_ITEM(0x32,LANG_MENU_OSD_BATT_PARAMS,       MENUITEM_SUBMENU,       &battery_submenu, 0 ),
    MENU_ITEM(0x33,LANG_MENU_OSD_SPACE_PARAMS,      MENUITEM_SUBMENU,       &space_submenu, 0 ),
    MENU_ITEM(0x34,LANG_MENU_OSD_CLOCK_PARAMS,      MENUITEM_SUBMENU,       &clock_submenu, 0 ),
    MENU_ITEM(0x59,LANG_MENU_OSD_SHOW_IN_REVIEW,    MENUITEM_BOOL,          &conf.show_osd_in_review, 0 ),
    MENU_ITEM(0x59,LANG_MENU_OSD_SHOW_HIDDENFILES,  MENUITEM_BOOL,          &conf.show_hiddenfiles, 0 ),
#ifdef  CAM_TOUCHSCREEN_UI
    MENU_ITEM   (0x22,LANG_MENU_TOUCHSCREEN_VALUES,         MENUITEM_SUBMENU,   &touchscreen_submenu,       0 ),
#endif
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP, 0,                                 0 ),
    {0}
};

static CMenu osd_submenu = {0x22,LANG_MENU_OSD_TITLE, osd_submenu_items };

//-------------------------------------------------------------------

// Display & edit an int value as a decimal.
// Value ranges from 1 - 20; but display shows as N.N (0.1 - 2.0)
static const char* gui_raw_ev_ettr_enum(int change, int arg)
{
    int *v = (int*)arg;

    *v += change;
    if (*v < 1) *v = 1;
    if (*v > 20) *v = 20;

    sprintf(buf, "%d.%d", (int)(*v / 10), (int)(*v % 10));

    return buf;
}

static const char* gui_raw_ev_enable_enum(int change, __attribute__ ((unused))int arg)
{
    static const char* modes[]={ "Don't", "ALT", "Play", "Both" };

    gui_enum_value_change(&conf.raw_ev_histo_enable,change,sizeof(modes)/sizeof(modes[0]));

    librawevhisto->load(conf.raw_ev_histo_enable);

    return modes[conf.raw_ev_histo_enable];
}

static CMenuItem raw_ev_histo_submenu_items[] = {
    MENU_ITEM(0x5f,LANG_MENU_RAW_EV_HISTO_ENABLE,       MENUITEM_ENUM,      gui_raw_ev_enable_enum, &conf.raw_ev_histo_enable ),
    MENU_ITEM(0x58,LANG_MENU_RAW_EV_HISTO_UNDER_THRESH, MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.raw_ev_histo_under_threshold, MENU_MINMAX(1, 16) ),
    MENU_ITEM(0x57,LANG_MENU_RAW_EV_HISTO_OVER_THRESH,  MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.raw_ev_histo_over_threshold,  MENU_MINMAX(1, 8) ),
    MENU_ITEM(0x57,LANG_MENU_RAW_EV_HISTO_ETTR_PCT,     MENUITEM_ENUM|MENUITEM_DECIMAL,gui_raw_ev_ettr_enum, &conf.raw_ev_histo_ettr_pct ),
    MENU_ITEM(0x5c,LANG_MENU_RAW_EV_HISTO_SAVE_LOG,     MENUITEM_BOOL,      &conf.raw_ev_histo_save_log, 0 ),
#ifdef CAM_HAS_PLAYBACK_IMAGE_NO
    MENU_ITEM(0x5c,LANG_MENU_RAW_EV_HISTO_SAVE_FOR_IMG, MENUITEM_BOOL,      &conf.raw_ev_histo_save_for_image, 0 ),
#endif
    MENU_ITEM(0x0,LANG_MENU_RAW_EV_HISTO_SAMPLE_AREA,   MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM(0x5e,LANG_MENU_RAW_EV_HISTO_WIDTH,        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.raw_ev_histo_width,   MENU_MINMAX(10, 100) ),
    MENU_ITEM(0x5e,LANG_MENU_RAW_EV_HISTO_HEIGHT,       MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.raw_ev_histo_height,  MENU_MINMAX(10, 100) ),
    MENU_ITEM(0x5e,LANG_MENU_RAW_EV_HISTO_XSTEP,        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX|MENUITEM_F_EVEN,  &conf.raw_ev_histo_xstep,   MENU_MINMAX(2, 64) ),
    MENU_ITEM(0x5e,LANG_MENU_RAW_EV_HISTO_YSTEP,        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX|MENUITEM_F_EVEN,  &conf.raw_ev_histo_ystep,   MENU_MINMAX(2, 64) ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                      MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu raw_ev_histo_submenu = {0x25,LANG_MENU_RAW_EV_HISTO_TITLE, raw_ev_histo_submenu_items };

//-------------------------------------------------------------------

static const char* gui_histo_show_modes[] = { "Don't", "Always", "Rec", "Shoot" };
static const char* gui_histo_view_modes[] = { "RGB", "Y", "RGB Y",  "R G B", "RGB all", "Y all", "Blend", "Blend Y"};
static const char* gui_histo_transform_modes[] = { "Linear", "Log" };

static CMenuItem histo_submenu_items[] = {
    MENU_ENUM2(0x5f,LANG_MENU_HISTO_SHOW,             &conf.show_histo,     gui_histo_show_modes ),
    MENU_ENUM2(0x6f,LANG_MENU_HISTO_LAYOUT,           &conf.histo_layout,   gui_histo_view_modes ),
    MENU_ENUM2(0x5f,LANG_MENU_HISTO_MODE,             &conf.histo_mode,     gui_histo_transform_modes ),
    MENU_ITEM(0x5c,LANG_MENU_HISTO_EXP,               MENUITEM_BOOL,       &conf.show_overexp, 0 ),
    MENU_ITEM(0x70,LANG_MENU_HISTO_IGNORE_PEAKS,      MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.histo_ignore_boundary,   MENU_MINMAX(0, 32) ),
    MENU_ITEM(0x5c,LANG_MENU_HISTO_MAGNIFY,           MENUITEM_BOOL,       &conf.histo_auto_ajust, 0 ),
    MENU_ITEM(0x5c,LANG_MENU_HISTO_SHOW_EV_GRID,      MENUITEM_BOOL,       &conf.histo_show_ev_grid, 0 ),
    MENU_ITEM(0x25,LANG_MENU_RAW_EV_HISTO_TITLE,      MENUITEM_SUBMENU,    &raw_ev_histo_submenu, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                    MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu histo_submenu = {0x25,LANG_MENU_HISTO_TITLE, histo_submenu_items };

//-------------------------------------------------------------------

static CMenuItem raw_exceptions_submenu_items[] = {
#if defined CAM_HAS_VIDEO_BUTTON
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_VIDEO,          MENUITEM_BOOL,      &conf.save_raw_in_video,        0 ),
#endif
#if defined(CAM_HAS_SPORTS_MODE)
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_SPORTS,         MENUITEM_BOOL,      &conf.save_raw_in_sports,       0 ),
#endif
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_BURST,          MENUITEM_BOOL,      &conf.save_raw_in_burst,        0 ),
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_TIMER,          MENUITEM_BOOL,      &conf.save_raw_in_timer,        0 ),
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_EDGEOVERLAY,    MENUITEM_BOOL,      &conf.save_raw_in_edgeoverlay,  0 ),
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_AUTO,           MENUITEM_BOOL,      &conf.save_raw_in_auto,         0 ),
#if defined(CAM_HAS_CANON_RAW)
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_CANON_RAW,      MENUITEM_BOOL,      &conf.save_raw_in_canon_raw,    0 ),
#endif
#if CAM_BRACKETING
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE_IN_EV_BRACKETING,  MENUITEM_BOOL,      &conf.save_raw_in_ev_bracketing, 0 ),
#endif
    MENU_ITEM   (0x5c,LANG_MENU_RAW_WARN,                   MENUITEM_BOOL,      &conf.raw_exceptions_warn,      0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,        0,                              0 ),
    {0}
};

static CMenu raw_exceptions_submenu = {0x59,LANG_MENU_OSD_RAW_EXCEPTIONS_PARAMS_TITLE, raw_exceptions_submenu_items };

//-------------------------------------------------------------------

const char* gui_raw_type_string()
{
    return conf.save_raw ? (conf.dng_raw ? "DNG" : "RAW") : "Off";
}

static void cb_change_dng()
{
    int old=conf.dng_version;
    conf_change_dng();
    if ((old==1) && (conf.dng_version==0)) gui_mbox_init(LANG_ERROR, LANG_CANNOT_OPEN_BADPIXEL_FILE, MBOX_BTN_OK|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, NULL);
}

static void cb_change_save_raw()
{
    if ( conf.enable_raw_shortcut )
    {
        conf.save_raw = !conf.save_raw;
        cb_change_dng();
        gui_set_need_restore();
        if ( conf.enable_raw_shortcut == 2 ) conf.show_raw_state = conf.save_raw;
    }
}

static const char* gui_dng_version(int change, __attribute__ ((unused))int arg)
{
    static const char* modes[]={ "1.3", "1.1" };

    gui_enum_value_change(&conf.dng_version,change,sizeof(modes)/sizeof(modes[0]));
    cb_change_dng();

    return modes[conf.dng_version];
}

static const char* gui_dng_crop_size_modes[] = { "JPEG", "Active", "Full" };

static void gui_menuproc_badpixel_create(__attribute__ ((unused))int arg)
{
    libdng->create_badpixel_bin();
}

static void raw_fselect_cb(const char * filename)
{
    raw_prepare_develop(filename, 1);
}

static void gui_raw_develop(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_RAW_DEVELOP_SELECT_FILE, "A/DCIM", "A", raw_fselect_cb);
}

const char* gui_bad_pixel_removal_modes[] = { (char*)LANG_MENU_BAD_PIXEL_OFF, (char*)LANG_MENU_BAD_PIXEL_INTERPOLATION, (char*)LANG_MENU_BAD_PIXEL_RAW_CONVERTER};

#if defined (DNG_EXT_FROM)
extern void cb_change_dng_usb_ext();
#endif

// Bit bending - the software equivalent of rerouting the ADC data lines, plus
// the non-data signals you would clip onto in a hardware bend. Names are
// deliberately in hardware terms because that is what each one is: "Tie low"
// is a data line shorted to ground, "Swap" is two lines crossed over, "Black
// level" is the optical black clamp landed on a data pin.
//
// This menu is the keyboard-and-list way in. The direct way in is bend mode -
// see BENDING_DESIGN.md - which is what this is meant to be replaced by for
// everyday use; the menu stays because it can reach the knobs that do not fit
// on a ten column pin strip.

// The four Bayer positions. Which one is red and which are the greens depends
// on the sensor's CFA phase, so they are named by position rather than colour -
// finding out which is which is a matter of taking a shot and looking.
static const char* gui_bitbend_channels[] = {
    "All", "Even/Even", "Odd/Even", "Even/Odd", "Odd/Odd"
};
static const char* gui_bitbend_buses[] = {
    "Pixel clk", "Pixel clk/N", "Line clk", "Line clk/N", "Frame",
    "Diagonal", "Black level", "Noise", "Sample+hold", "Line memory",
    "Bus parity", "Tap"
};
static const char* gui_bitbend_trash[] = {
    "White", "Blocky", "Ramp", "Burst", "Sensor seed"
};

// Menu items bind to int*, and the bend struct is packed bytes so that it can
// be written to a preset file as-is. These shadow the fields it needs to edit.
static int bend_ui_a = CAM_SENSOR_BITS_PER_PIXEL - 1;
static int bend_ui_b = 0;
static int bend_ui_bus = BUS_OB;
static int bend_ui_depth = 3;
static int bend_ui_trash_type = 0;
static int bend_ui_trash_rate = 0;
static int bend_ui_hdiv = 3;
static int bend_ui_vdiv = 3;
static int bend_ui_tapx = 17;
static int bend_ui_tapy = -3;
static int bend_ui_bayer = 0;
static int bend_ui_period = 0;

static int bend_ui_clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void bend_ui_pull();

// shadows -> bend. Called from the ARG_CALLBACK on every edited item, and once
// per redraw for the enum items, which cannot carry a callback.
static void bend_ui_push()
{
    const int n = CAM_SENSOR_BITS_PER_PIXEL;
    static int inited = 0;

    // First call wins the other way round. The shadows hold their static
    // initialisers until something has been loaded into them, and pushing
    // those over a bend restored from CCHDK.CFG would silently reset the
    // trash and scope settings of a saved bend on the first redraw after boot.
    if (!inited)
    {
        inited = 1;
        bend_sanitize(bend_seg_cur(), CAM_SENSOR_BITS_PER_PIXEL);
        bend_ui_pull();
        return;
    }
    bend_ui_a          = bend_ui_clamp(bend_ui_a, 0, n - 1);
    bend_ui_b          = bend_ui_clamp(bend_ui_b, 0, n - 1);
    bend_ui_bus        = bend_ui_clamp(bend_ui_bus, 0, BUS_COUNT - 1);
    bend_ui_depth      = bend_ui_clamp(bend_ui_depth, 1, n);
    bend_ui_trash_type = bend_ui_clamp(bend_ui_trash_type, 0, TRASH_COUNT - 1);
    bend_ui_trash_rate = bend_ui_clamp(bend_ui_trash_rate, 0, 12);
    bend_ui_hdiv       = bend_ui_clamp(bend_ui_hdiv, 0, 12);
    bend_ui_vdiv       = bend_ui_clamp(bend_ui_vdiv, 0, 12);
    bend_ui_tapx       = bend_ui_clamp(bend_ui_tapx, -128, 127);
    bend_ui_tapy       = bend_ui_clamp(bend_ui_tapy, -128, 127);
    bend_ui_bayer      = bend_ui_clamp(bend_ui_bayer, 0, 4);
    bend_ui_period     = bend_ui_clamp(bend_ui_period, 0, 64);

    bend_seg_cur()->trash_type = (unsigned char)bend_ui_trash_type;
    bend_seg_cur()->trash_rate = (unsigned char)bend_ui_trash_rate;
    bend_seg_cur()->hdiv       = (unsigned char)bend_ui_hdiv;
    bend_seg_cur()->vdiv       = (unsigned char)bend_ui_vdiv;
    bend_seg_cur()->tap_dx     = (signed char)bend_ui_tapx;
    bend_seg_cur()->tap_dy     = (signed char)bend_ui_tapy;
    bend_seg_cur()->bayer      = (unsigned char)bend_ui_bayer;
    bend_seg_cur()->row_period = (unsigned char)bend_ui_period;
    bend_seg_cur()->depth      = (unsigned char)bend_ui_depth;
}

// bend -> shadows. Called after anything that rewrites the whole struct.
static void bend_ui_pull()
{
    bend_ui_trash_type = bend_seg_cur()->trash_type;
    bend_ui_trash_rate = bend_seg_cur()->trash_rate;
    bend_ui_hdiv       = bend_seg_cur()->hdiv;
    bend_ui_vdiv       = bend_seg_cur()->vdiv;
    bend_ui_tapx       = bend_seg_cur()->tap_dx;
    bend_ui_tapy       = bend_seg_cur()->tap_dy;
    bend_ui_bayer      = bend_seg_cur()->bayer;
    bend_ui_period     = bend_seg_cur()->row_period;
    bend_ui_depth      = bend_seg_cur()->depth;
}

static void bend_ui_prep()
{
    bend_sanitize(bend_seg_cur(), CAM_SENSOR_BITS_PER_PIXEL);
    // Everything below this calls prep before changing the matrix, so this is
    // the one place that has to say the region no longer holds the file it was
    // loaded from - otherwise the region list would keep a preset's name on a
    // row whose bend had been edited out from under it.
    bend_seg_detach();
    bend_ui_push();
}

static void gui_bend_swap(int arg)      { bend_ui_prep(); bend_preset_swap(bend_seg_cur(), bend_ui_a, bend_ui_b); }
static void gui_bend_invert(int arg)    { bend_ui_prep(); bend_seg_cur()->route[bend_ui_a] = bend_src_invert(bend_seg_cur()->route[bend_ui_a]); }
static void gui_bend_low(int arg)       { bend_ui_prep(); bend_preset_tie(bend_seg_cur(), bend_ui_a, 0); }
static void gui_bend_high(int arg)      { bend_ui_prep(); bend_preset_tie(bend_seg_cur(), bend_ui_a, 1); }
static void gui_bend_clear_pin(int arg) { bend_ui_prep(); bend_seg_cur()->route[bend_ui_a] = (unsigned char)BSRC_DATA(bend_ui_a); }
static void gui_bend_patch_bus(int arg) { bend_ui_prep(); bend_seg_cur()->route[bend_ui_a] = (unsigned char)BSRC_BUS(bend_ui_bus); }
static void gui_bend_rotate(int arg)    { bend_ui_prep(); bend_preset_rotate(bend_seg_cur(), bend_ui_a); }
static void gui_bend_reverse(int arg)   { bend_ui_prep(); bend_preset_reverse(bend_seg_cur()); }

static void gui_bend_random(int arg)
{
    bend_ui_prep();
    // Reseed from the clock so consecutive presses give different bends. The
    // seed is stored in the struct, so whatever comes out is still reproducible
    // and still saveable.
    bend_seg_cur()->seed = (unsigned)get_tick_count() ^ (bend_seg_cur()->seed * 1103515245u);
    bend_random(bend_seg_cur(), bend_seg_cur()->seed, bend_ui_depth, !conf.bitbend_simple);
    bend_ui_pull();
}

static void gui_bend_mutate(int arg)
{
    bend_ui_prep();
    bend_seg_cur()->seed = (unsigned)get_tick_count() ^ (bend_seg_cur()->seed * 1103515245u);
    bend_mutate(bend_seg_cur(), bend_seg_cur()->seed, !conf.bitbend_simple);
}

static void gui_bend_reset(int arg)
{
    bend_reset(bend_seg_cur(), CAM_SENSOR_BITS_PER_PIXEL);
    bend_store_detach();
    bend_seg_detach();
    bend_ui_pull();
}

// Applied the moment the switch is thrown rather than at the next shot, so the
// pin strip and this menu agree with what will actually be used.
//
// Turning it on discards any bus patches outright. Keeping them somewhere to
// restore on the way back was considered and dropped: there is nowhere to keep
// them that survives a config save, and a matrix that silently springs back
// into the slow path is worse than one that lost an edge you can see it lost.
// Every region, not just the one being edited. The switch is a property of the
// camera rather than of a matrix - it says which sources this body is willing
// to pay for - and leaving three segments patched to buses while the fourth
// was cleaned up would make the shot slow for a reason nothing on screen said.
static void gui_bend_simple_cb(int arg)
{
    if (conf.bitbend_simple) bend_seg_prep(CAM_SENSOR_BITS_PER_PIXEL);
}

//-------------------------------------------------------------------
// Segments - see include/bend.h. The list way in; the direct one is the SEG
// window of the browser in bend mode.
//
// Enums rather than int shadows because both values are named things with a
// count that depends on the other one, which is what a shadow int cannot say
// and a callback can.

static const char* gui_bend_seg_layout_enum(int change, __attribute__ ((unused))int arg)
{
    int v = conf.bend_segs.layout;

    if (change)
    {
        v += change;
        while (v < 0)           v += BSEG_COUNT;
        while (v >= BSEG_COUNT) v -= BSEG_COUNT;
        conf.bend_segs.layout = (unsigned char)v;
        // The region being edited may not exist in the new layout, and every
        // other part of the UI reads it without checking further.
        bend_seg_set_active(bend_seg_active());
        // A matrix in a slot that no layout has reached yet is whatever the
        // config block happened to hold, so it goes through the gate the
        // moment the layout makes it reachable rather than at the next shot.
        bend_seg_prep(CAM_SENSOR_BITS_PER_PIXEL);
        bend_ui_pull();
    }
    return bend_seg_name(conf.bend_segs.layout);
}

static char bend_seg_msg[32];

static const char* gui_bend_seg_active_enum(int change, __attribute__ ((unused))int arg)
{
    int n = bend_seg_n();

    if (change)
    {
        int v = bend_seg_active() + change;
        while (v < 0)  v += n;
        while (v >= n) v -= n;
        bend_seg_set_active(v);
        // The shadows above hold the scope and trash knobs of whichever matrix
        // is being edited, so moving to another region has to reload them or
        // the next menu press would push this one's settings onto that one.
        bend_ui_pull();
    }

    if (n < 2) return "n/a - one region";
    sprintf(bend_seg_msg, "%d - %s", bend_seg_active() + 1,
            bend_seg_label(conf.bend_segs.layout, bend_seg_active()));
    return bend_seg_msg;
}

// The round layouts have a radius and the repeating ones have a cell; the rest
// are cut by the frame's own geometry. Said out loud on those rather than left
// as a knob that turns and does nothing.
static const char* gui_bend_seg_size_enum(int change, __attribute__ ((unused))int arg)
{
    if (!bend_seg_has_size(conf.bend_segs.layout)) return "n/a - fixed shape";

    if (change)
    {
        int v = (int)conf.bend_segs.size + change * 5;
        if (v < 5)   v = 5;
        if (v > 100) v = 100;
        conf.bend_segs.size = (unsigned char)v;
    }
    sprintf(bend_seg_msg, "%d%%", conf.bend_segs.size);
    return bend_seg_msg;
}

// gui_mbox_init takes the message as an int that may be a pointer, and the box
// is drawn long after this returns - so the buffer cannot be on the stack.
static char bend_store_msg[40];

static void gui_bend_store_save(int arg)
{
    int i = bend_store_save_current();

    if (i < 0)
    {
        gui_mbox_init(LANG_ERROR, (int)"Could not write preset - card full?",
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }
    bend_seg_set_slot(bend_seg_active(), bend_store_slot_at(i));
    sprintf(bend_store_msg, "Saved as BEND%02d.BND", bend_store_slot_at(i));
    gui_mbox_init(LANG_INFORMATION, (int)bend_store_msg,
                  MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
}

static void gui_bend_store_step(int delta)
{
    // Re-scanned here rather than trusted, because the menu is reachable
    // straight after a card swap and this is the one path that has no other
    // reason to have looked at the directory.
    if (bend_store_rescan() <= 0)
    {
        gui_mbox_init(LANG_INFORMATION, (int)"No saved bends on the card",
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }
    if (!bend_store_step(bend_seg_cur(), delta))
    {
        gui_mbox_init(LANG_ERROR, (int)"Preset unreadable - wrong version?",
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }
    if (conf.bitbend_simple) bend_simplify(bend_seg_cur());
    bend_seg_set_slot(bend_seg_active(), bend_store_slot_at(bend_store_cur()));
    bend_ui_pull();
}

static void gui_bend_store_next(int arg) { gui_bend_store_step(1); }
static void gui_bend_store_prev(int arg) { gui_bend_store_step(-1); }

static void gui_bend_store_delete_cb(unsigned int btn)
{
    if (btn == MBOX_BTN_YES) bend_store_delete_cur();
}

static void gui_bend_store_delete(int arg)
{
    int cur = bend_store_cur();

    if (cur < 0)
    {
        gui_mbox_init(LANG_INFORMATION, (int)"No preset loaded to delete",
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }
    sprintf(bend_store_msg, "Delete BEND%02d.BND?", bend_store_slot_at(cur));
    gui_mbox_init(LANG_WARNING, (int)bend_store_msg,
                  MBOX_BTN_YES_NO|MBOX_DEF_BTN2|MBOX_TEXT_CENTER,
                  gui_bend_store_delete_cb);
}

//-------------------------------------------------------------------
#ifdef CAM_BEND_EXPERIMENTAL
//-------------------------------------------------------------------
// Experimental profiles - see include/bendx.h and docs/EXPERIMENTAL_EFFECTS.md
//
// The other engine. Not the bus between the sensor and DIGIC but the frame
// buffer it writes into, failing one named way at a time - a stuck address
// line, a lost pixel clock, a foreign buffer on the data lanes.
//
// The direct way in is the EXPERIMENTAL window of the browser in bend mode,
// where the rocker is the amount knob. This is the keyboard-and-list way in,
// and it is the only place the four quieter knobs are reachable: the ones that
// do not change what the fault is, only how much of the picture it reaches.
//
// These bind through enum callbacks rather than through int shadows the way
// the matrix above does, because every one of them is a value whose range and
// whose printed form depend on which effect is selected - which is exactly
// what a shadow int cannot express and a callback can.

static char bendx_ui_buf[40];

// Every item below edits the live slot - the one the rocker turns in bend
// mode. The locked entries are deliberately not reachable from here: locking
// is a commitment made with a deliberate second press, and a menu that could
// quietly retune a committed profile would make that press mean nothing. To
// change one, drop it from the chain in the browser and pick it again.
//
// Null when all four slots are locked. Every callback checks, because a menu
// row is drawn before anyone presses anything.
static bendx_t *bendx_ui_live(void)
{
    bendx_chain_sanitize(&conf.bendx);
    return bendx_chain_live(&conf.bendx);
}

static const char* gui_bendx_effect_enum(int change, __attribute__ ((unused))int arg)
{
    bendx_t *x = bendx_ui_live();
    int k;

    if (!x) return "chain full";
    k = x->kind;

    if (change)
    {
        k += change;
        while (k < 0)         k += BX_COUNT;
        while (k >= BX_COUNT) k -= BX_COUNT;

        // Switching effect resets the knobs - see bendx_set_kind(), which the
        // browser in bend mode goes through too so the two cannot disagree.
        if (k != x->kind) bendx_set_kind(x, k);
    }
    bendx_sanitize(x);
    return bendx_name(x->kind);
}

// How many are stacked, and whether the stack is a slow shot. The chain is
// built in the browser, so from here it is a read-out rather than a control -
// but it is the one number that changes what pressing the shutter costs.
static const char* gui_bendx_chain_enum(__attribute__ ((unused))int change,
                                        __attribute__ ((unused))int arg)
{
    int n;
    bendx_chain_sanitize(&conf.bendx);
    n = bendx_chain_count(&conf.bendx);
    if (!n) return "empty";
    sprintf(bendx_ui_buf, "%d of %d%s", n, BENDX_CHAIN_MAX,
            bendx_chain_slow(&conf.bendx) ? ", slow" : "");
    return bendx_ui_buf;
}

// What the selected effect actually is, in one line. Worth a menu row of its
// own: the names have to fit a 12 character column in the picker, so on their
// own several of them are a hint rather than a description.
static const char* gui_bendx_desc_enum(__attribute__ ((unused))int change,
                                       __attribute__ ((unused))int arg)
{
    const bendx_t *x = bendx_chain_live_const(&conf.bendx);
    return x ? bendx_desc(x->kind) : "Lock one fewer to tune another";
}

static const char* gui_bendx_amount_enum(int change, __attribute__ ((unused))int arg)
{
    bendx_t *x = bendx_ui_live();
    int max, v;

    if (!x) return "-";
    max = bendx_amount_max(x->kind);
    v   = x->amount;

    if (max <= 0) return "-";
    v += change;
    while (v < 0)   v += max + 1;
    while (v > max) v -= max + 1;
    x->amount = (unsigned char)v;

    bendx_label(x, bendx_ui_buf);
    return bendx_ui_buf;
}

static const char* gui_bendx_reach_enum(int change, __attribute__ ((unused))int arg)
{
    bendx_t *x = bendx_ui_live();
    int max, v;

    if (!x) return "-";
    max = bendx_reach_max(x->kind);
    v   = x->reach;

    if (max <= 0) return "-";
    v += change;
    while (v < 0)   v += max + 1;
    while (v > max) v -= max + 1;
    x->reach = (unsigned char)v;

    sprintf(bendx_ui_buf, "%d", v);
    return bendx_ui_buf;
}

static const char* gui_bendx_mix_enum(int change, __attribute__ ((unused))int arg)
{
    bendx_t *x = bendx_ui_live();
    int v;

    if (!x) return "-";
    v = x->mix + change;

    while (v < 0)             v += BXMIX_COUNT;
    while (v >= BXMIX_COUNT)  v -= BXMIX_COUNT;
    x->mix = (unsigned char)v;

    return bendx_mix_names[x->mix];
}

// The eight data lanes of the frame buffer's memory bus, as a mask. Printed in
// binary because that is the only form in which "which lanes" is readable -
// "129" does not say that it is the top one and the bottom one.
static const char* gui_bendx_lanes_enum(int change, __attribute__ ((unused))int arg)
{
    bendx_t *x = bendx_ui_live();
    int v, i;

    if (!x) return "-";
    v = (int)x->lanes + change;

    while (v < 0)    v += 256;
    while (v > 255)  v -= 256;
    x->lanes = (unsigned char)v;

    if (!v) return "all";
    for (i = 0; i < 8; i++)
        bendx_ui_buf[i] = (v & (0x80 >> i)) ? '1' : '0';
    bendx_ui_buf[8] = 0;
    return bendx_ui_buf;
}

static const char* gui_bendx_rowmod_enum(int change, __attribute__ ((unused))int arg)
{
    bendx_t *x = bendx_ui_live();
    int v;

    if (!x) return "-";
    v = (int)x->rowmod + change;

    while (v < 0)   v += 65;
    while (v > 64)  v -= 65;
    x->rowmod = (unsigned char)v;

    if (v < 2) return "every row";
    sprintf(bendx_ui_buf, "%d on, %d off", v, v);
    return bendx_ui_buf;
}

static void gui_bendx_random(__attribute__ ((unused))int arg)
{
    bendx_t *x = bendx_ui_live();

    if (!x || bendx_is_off(x))
    {
        gui_mbox_init(LANG_INFORMATION,
                      (int)(x ? "Choose an effect first"
                              : "Every slot is locked - drop one first"),
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }
    bendx_random(x, (unsigned)get_tick_count() ^ (x->seed * 1103515245u));
}

// Locks the live slot from here too, so the chain can be built without going
// into bend mode at all. The browser's second SET press is the fast way; this
// is the discoverable one.
static void gui_bendx_lock(__attribute__ ((unused))int arg)
{
    bendx_chain_sanitize(&conf.bendx);
    if (!bendx_chain_lock(&conf.bendx))
    {
        gui_mbox_init(LANG_INFORMATION,
                      (int)"Nothing to lock, or the chain is full",
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }
    conf.bendx_enable = 1;
}

static void gui_bendx_drop(__attribute__ ((unused))int arg)
{
    bendx_chain_sanitize(&conf.bendx);
    if (conf.bendx.n == 0)
    {
        gui_mbox_init(LANG_INFORMATION, (int)"No locked profiles to drop",
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }
    // The last one locked. Dropping from the end is the undo of locking, and
    // picking an arbitrary one out of the middle is what the browser is for.
    bendx_chain_remove(&conf.bendx, conf.bendx.n - 1);
}

static void gui_bendx_reset(__attribute__ ((unused))int arg)
{
    bendx_chain_reset(&conf.bendx);
    conf.bendx_enable = 0;
}

#endif // CAM_BEND_EXPERIMENTAL

//-------------------------------------------------------------------
// Reloading a bend out of a picture - see include/bend_shot.h.
//
// Every bent frame writes a sidecar next to its JPEG holding the matrix and
// the chain it was taken with. This is the other half: pick the picture, get
// the recipe back.
//
// Reached two ways. From this menu, and - the way it is meant to be used -
// by holding the mode button in playback, where the gesture that opens the
// patchbay in record mode instead asks about the pictures you are looking at.
//
// It has to go through the file browser rather than acting on whatever
// playback is displaying, because this camera cannot say what that is.
// CAM_HAS_PLAYBACK_IMAGE_NO is a DIGIC 4 and later thing; the equivalent on
// this body would mean reversing the ImgPlayDrv task, and the ROM's own path
// builder takes the folder and file numbers as arguments rather than reading
// them from anywhere findable. A wrong address there loads someone else's
// bend at best. The browser opens on A/DCIM with the newest folder in front,
// so it is one press more than the ideal and it is always right.

// Long enough for the longest of the messages below with a full path-less file
// name in it - the layout line is the new worst case, and the "no bend
// recorded" line was already over the sixty-four this used to be.
static char bend_shot_msg[128];

static void bend_shot_selected(const char *fn)
{
    bend_t        b;
    bendx_chain_t c;
    bend_segs_t   sg;
    int bend_on = 0, bendx_on = 0;
    const char *name;
    // Named rather than counted, because "4 segments" and "Quarters" are the
    // same fact and only one of them tells you where to look. Declared up here
    // with the rest because the block it is used in starts with a statement on
    // a build without the second engine.
    const char *lay;

    if (!fn) return;

    // Report the name rather than the path - the path is most of a line on
    // this screen and the part that identifies the frame is the end of it.
    for (name = fn + strlen(fn); name > fn && name[-1] != '/'; name--) ;

    if (!bend_tag_read(fn, &b, &bend_on, &c, &bendx_on, &sg))
    {
        sprintf(bend_shot_msg,
                "No bend recorded for %s - it was not taken with one, or the "
                "record has been stripped out of it", name);
        gui_mbox_init(LANG_INFORMATION, (int)bend_shot_msg,
                      MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        return;
    }

    conf.bitbend        = b;
    conf.bitbend_enable = bend_on;
    // The layout comes back with the matrices, because on a segmented frame
    // neither half is the record on its own - the same four bends under a
    // different layout are a different picture. A version 1 sidecar carries
    // the layout switched off, which is what the camera that wrote it had.
    conf.bend_segs      = sg;
#ifdef CAM_BEND_EXPERIMENTAL
    conf.bendx        = c;
    conf.bendx_enable = bendx_on;
#else
    // No second engine in this build, so the profiles the frame was taken with
    // cannot be restored. Said out loud below rather than silently dropped.
    (void)c; (void)bendx_on;
#endif

    // Through the same gates a preset goes through, and in the same order -
    // simplify only after sanitize, because it reads nbits.
    bend_seg_prep(CAM_SENSOR_BITS_PER_PIXEL);
#ifdef CAM_BEND_EXPERIMENTAL
    bendx_chain_sanitize(&conf.bendx);
#endif
    bend_ui_pull();

    // The matrix no longer came from a preset on the card, so nothing in the
    // saved list should still be claiming to be what is loaded.
    bend_store_detach();

    {
#ifdef CAM_BEND_EXPERIMENTAL
        int n = bendx_chain_count(&conf.bendx);
#else
        int n = 0;
        if (bendx_on)
        {
            sprintf(bend_shot_msg,
                    "Loaded the bend from %s. It also used experimental "
                    "profiles, which this build does not have", name);
            gui_mbox_init(LANG_INFORMATION, (int)bend_shot_msg,
                          MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
            return;
        }
#endif
        lay = (conf.bend_segs.layout != BSEG_OFF)
            ? bend_seg_name(conf.bend_segs.layout) : 0;

        if (bend_on && lay)    sprintf(bend_shot_msg, "Loaded the bend from %s - %s, %d segments", name, lay, bend_seg_n());
        else if (bend_on && n) sprintf(bend_shot_msg, "Loaded the bend and %d profile%s from %s", n, (n == 1) ? "" : "s", name);
        else if (bend_on)      sprintf(bend_shot_msg, "Loaded the bend from %s", name);
        else if (n)            sprintf(bend_shot_msg, "Loaded %d profile%s from %s", n, (n == 1) ? "" : "s", name);
        else                   sprintf(bend_shot_msg, "%s was taken with nothing applied", name);
    }
    gui_mbox_init(LANG_INFORMATION, (int)bend_shot_msg,
                  MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
}

static void gui_bend_shot_browse(__attribute__ ((unused))int arg)
{
    module_run("bendpic.flt");
}

// Held the mode button in playback. Asks first, because the browser is a
// full-screen takeover and arriving in one by accident - the button is also
// the one that opens <ALT> - is a worse surprise than one extra press.
static void gui_bend_shot_prompt_cb(unsigned int btn)
{
    if (btn == MBOX_BTN_YES) gui_bend_shot_browse(0);
}

void gui_bend_shot_prompt(void)
{
    gui_mbox_init((int)"Reload bend from picture",
                  (int)"Load the bend and profiles a picture was taken with?",
                  MBOX_BTN_YES_NO|MBOX_DEF_BTN2|MBOX_TEXT_CENTER,
                  gui_bend_shot_prompt_cb);
}

#ifdef CAM_BEND_EXPERIMENTAL

static CMenuItem bendx_submenu_items[] = {
    MENU_ITEM   (0x5c,(int)"Enable",            MENUITEM_BOOL,      &conf.bendx_enable, 0 ),
    MENU_ITEM   (0x5f,(int)"Stacked",           MENUITEM_ENUM,      gui_bendx_chain_enum, 0 ),

    // Everything below edits the live slot. The locked ones are frozen by
    // design - see bendx_ui_live().
    MENU_ITEM   (0x0 ,(int)"Live profile",      MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM   (0x5f,(int)"Effect",            MENUITEM_ENUM,      gui_bendx_effect_enum, 0 ),
    MENU_ITEM   (0x0 ,(int)"",                  MENUITEM_ENUM,      gui_bendx_desc_enum, 0 ),
    MENU_ITEM   (0x5f,(int)"Amount",            MENUITEM_ENUM,      gui_bendx_amount_enum, 0 ),
    MENU_ITEM   (0x5f,(int)"Reach",             MENUITEM_ENUM,      gui_bendx_reach_enum, 0 ),
    MENU_ITEM   (0x2a,(int)"Reroll the knobs",  MENUITEM_PROC,      gui_bendx_random, 0 ),

    MENU_ITEM   (0x0 ,(int)"Stack",             MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM   (0x2a,(int)"Lock it, start another", MENUITEM_PROC, gui_bendx_lock, 0 ),
    MENU_ITEM   (0x2b,(int)"Drop the last locked",   MENUITEM_PROC, gui_bendx_drop, 0 ),

    // These three do not change what the fault is, only how much of the
    // picture it reaches - which is the difference between an effect that is
    // interesting and one that has covered the photograph.
    MENU_ITEM   (0x0 ,(int)"Reach into the picture", MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM   (0x5f,(int)"Data lanes",        MENUITEM_ENUM,      gui_bendx_lanes_enum, 0 ),
    MENU_ITEM   (0x5f,(int)"Mix",               MENUITEM_ENUM,      gui_bendx_mix_enum, 0 ),
    MENU_ITEM   (0x5f,(int)"Row bands",         MENUITEM_ENUM,      gui_bendx_rowmod_enum, 0 ),

    MENU_ITEM   (0x2a,(int)"Reset to off",      MENUITEM_PROC,      gui_bendx_reset, 0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,           MENUITEM_UP,        0, 0 ),
    {0}
};
static CMenu bendx_submenu = {0x39,(int)"Experimental", bendx_submenu_items };

#endif // CAM_BEND_EXPERIMENTAL

// Where the recipe goes. "In JPEG" writes a comment segment into the picture
// (include/bend_tag.h); "Sidecar" writes the file beside it that this used to
// do and nothing else (include/bend_shot.h). The first costs a rewrite of the
// photograph after the shot and the second costs a second file per frame,
// which is the whole of the trade.
static const char* gui_bend_shot_where[] = { "Off", "In JPEG" };

static CMenuItem bitbend_submenu_items[] = {
    MENU_ITEM   (0x5c,(int)"Enable",            MENUITEM_BOOL,      &conf.bitbend_enable, 0 ),
    MENU_ITEM   (0x5c,(int)"Simple signals only", MENUITEM_BOOL|MENUITEM_ARG_CALLBACK, &conf.bitbend_simple, (int)gui_bend_simple_cb ),

    // Segments. Above everything else in this menu because they change what
    // the rest of it is editing: every item below acts on the region named
    // here, and reading them in the other order would be reading them wrong.
    MENU_ITEM   (0x0 ,(int)"Segments",          MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM   (0x5f,(int)"Layout",            MENUITEM_ENUM,      gui_bend_seg_layout_enum, 0 ),
    MENU_ITEM   (0x5f,(int)"Editing region",    MENUITEM_ENUM,      gui_bend_seg_active_enum, 0 ),
    MENU_ITEM   (0x5f,(int)"Size",              MENUITEM_ENUM,      gui_bend_seg_size_enum, 0 ),
    MENU_ITEM   (0x5c,(int)"Show pattern mask", MENUITEM_BOOL,      &conf.bend_seg_mask, 0 ),

    // The second engine, one level down. It is not part of the matrix and it
    // does not belong inside its sections, but it is the other half of what
    // this menu is for and it should not be somewhere else in the tree.
#ifdef CAM_BEND_EXPERIMENTAL
    MENU_ITEM   (0x59,(int)"Experimental profiles", MENUITEM_SUBMENU, &bendx_submenu, 0 ),
#endif

    // What a frame was taken with, recorded beside it - see bend_shot.h. The
    // direct way in is holding the mode button in playback; this is the way
    // in that can be found by reading the menu.
    MENU_ITEM   (0x0 ,(int)"Pictures",          MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ENUM2a (0x5f,(int)"Record bend with shot", &conf.bend_shot_save, gui_bend_shot_where, BEND_SHOT_WHERE_N ),
    MENU_ITEM   (0x2a,(int)"Load bend from image",       MENUITEM_PROC, gui_bend_shot_browse, 0 ),

    MENU_ITEM   (0x0 ,(int)"Presets on card",   MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM   (0x2a,(int)"Save bend to card", MENUITEM_PROC,      gui_bend_store_save, 0 ),
    MENU_ITEM   (0x2a,(int)"Load next preset",  MENUITEM_PROC,      gui_bend_store_next, 0 ),
    MENU_ITEM   (0x2a,(int)"Load prev preset",  MENUITEM_PROC,      gui_bend_store_prev, 0 ),
    MENU_ITEM   (0x2b,(int)"Delete preset",     MENUITEM_PROC,      gui_bend_store_delete, 0 ),

    MENU_ITEM   (0x0 ,(int)"Patch one pin",     MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM   (0x60,(int)"Pin A",             MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_a, (int)bend_ui_push ),
    MENU_ITEM   (0x60,(int)"Pin B",             MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_b, (int)bend_ui_push ),
    MENU_ITEM   (0x2a,(int)"Swap A <-> B",      MENUITEM_PROC,      gui_bend_swap, 0 ),
    MENU_ITEM   (0x2a,(int)"Invert A",          MENUITEM_PROC,      gui_bend_invert, 0 ),
    MENU_ITEM   (0x2a,(int)"Tie A low",         MENUITEM_PROC,      gui_bend_low, 0 ),
    MENU_ITEM   (0x2a,(int)"Tie A high",        MENUITEM_PROC,      gui_bend_high, 0 ),
    // These two, and the whole Signals block below, do nothing while "Simple
    // signals only" is on - bend_simplify() puts the pin straight back. Left
    // reachable rather than hidden, because a menu that changes shape is
    // harder to learn than one where a switch above explains the dead items.
    MENU_ENUM2a (0x5f,(int)"Non-data bus",      &bend_ui_bus,       gui_bitbend_buses, BUS_COUNT ),
    MENU_ITEM   (0x2a,(int)"Patch bus -> A",    MENUITEM_PROC,      gui_bend_patch_bus, 0 ),
    MENU_ITEM   (0x2a,(int)"Unpatch A",         MENUITEM_PROC,      gui_bend_clear_pin, 0 ),

    MENU_ITEM   (0x0 ,(int)"Whole matrix",      MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ITEM   (0x2a,(int)"Rotate by A",       MENUITEM_PROC,      gui_bend_rotate, 0 ),
    MENU_ITEM   (0x2a,(int)"Reverse",           MENUITEM_PROC,      gui_bend_reverse, 0 ),
    MENU_ITEM   (0x60,(int)"Random depth",      MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_depth, (int)bend_ui_push ),
    MENU_ITEM   (0x2a,(int)"Randomise",         MENUITEM_PROC,      gui_bend_random, 0 ),
    MENU_ITEM   (0x2a,(int)"Mutate one edge",   MENUITEM_PROC,      gui_bend_mutate, 0 ),
    MENU_ITEM   (0x2a,(int)"Reset to straight", MENUITEM_PROC,      gui_bend_reset, 0 ),

    MENU_ITEM   (0x0 ,(int)"Signals",           MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ENUM2a (0x5f,(int)"Trash type",        &bend_ui_trash_type, gui_bitbend_trash, TRASH_COUNT ),
    MENU_ITEM   (0x60,(int)"Trash rate",        MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_trash_rate, (int)bend_ui_push ),
    MENU_ITEM   (0x60,(int)"Pixel clk divider", MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_hdiv, (int)bend_ui_push ),
    MENU_ITEM   (0x60,(int)"Line clk divider",  MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_vdiv, (int)bend_ui_push ),
    MENU_ITEM   (0x60,(int)"Tap dx",            MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_tapx, (int)bend_ui_push ),
    MENU_ITEM   (0x60,(int)"Tap dy",            MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_tapy, (int)bend_ui_push ),

    MENU_ITEM   (0x0 ,(int)"Scope",             MENUITEM_SEPARATOR, 0, 0 ),
    MENU_ENUM2a (0x5f,(int)"Bayer position",    &bend_ui_bayer,     gui_bitbend_channels, 5 ),
    MENU_ITEM   (0x60,(int)"Every Nth row",     MENUITEM_INT|MENUITEM_ARG_CALLBACK, &bend_ui_period, (int)bend_ui_push ),

    MENU_ITEM   (0x51,LANG_MENU_BACK,           MENUITEM_UP,        0, 0 ),
    {0}
};
static CMenu bitbend_submenu = {0x39,(int)"Circuit Bending", bitbend_submenu_items };

//-------------------------------------------------------------------
// Multiple exposure - see include/mexp.h and docs/MULTI_EXPOSURE.md.

// Named the way the bodies that have this feature name them, in the order the
// modes are numbered in mexp.h. "Additive" and "Average" are the two a film
// camera can do by not winding on; the other two need a computer to have seen
// both frames, which is what this is.
static const char* gui_mexp_modes[] = {
    "Additive", "Lighten", "Darken", "Average"
};

static void gui_mexp_cancel(__attribute__ ((unused))int arg)
{
    extern void raw_mexp_cancel(void);
    raw_mexp_cancel();
}

// Switching the feature off part way through a sequence has to end it here
// too, not only at the next shot: the ghost is on screen now, and leaving it
// there over a camera that is no longer combining anything is a lie about what
// the next press of the shutter is going to do.
static void gui_mexp_enable_cb(__attribute__ ((unused))int arg)
{
    if (!conf.mexp_enable) gui_mexp_cancel(0);
}

static CMenuItem mexp_submenu_items[] = {
    MENU_ITEM   (0x5c,(int)"Enable",            MENUITEM_BOOL|MENUITEM_ARG_CALLBACK, &conf.mexp_enable, (int)gui_mexp_enable_cb ),
    MENU_ENUM2a (0x5f,(int)"Blend",             &conf.mexp_mode,    gui_mexp_modes, MEXP_MODE_COUNT ),
    MENU_ITEM   (0x60,(int)"Exposures",         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.mexp_frames, MENU_MINMAX(MEXP_FRAMES_MIN, MEXP_FRAMES_MAX) ),
    MENU_ITEM   (0x5c,(int)"Ghost on screen",   MENUITEM_BOOL,      &conf.mexp_ghost, 0 ),
    MENU_ITEM   (0x5c,(int)"Bend each exposure",MENUITEM_BOOL,      &conf.mexp_bend_each, 0 ),

    // A sequence is remembered between shots and nothing else ends it early -
    // not the mode dial, not playback, not the menu. This is the way out of
    // one that was started by accident or abandoned.
    MENU_ITEM   (0x2a,(int)"Cancel sequence",   MENUITEM_PROC,      gui_mexp_cancel, 0 ),

    MENU_ITEM   (0x51,LANG_MENU_BACK,           MENUITEM_UP,        0, 0 ),
    {0}
};
static CMenu mexp_submenu = {0x39,(int)"Multiple exposure", mexp_submenu_items };

static CMenuItem raw_submenu_items[] = {
    MENU_ITEM   (0x5c,LANG_MENU_RAW_SAVE,                   MENUITEM_BOOL | MENUITEM_ARG_CALLBACK, &conf.save_raw, (int)cb_change_dng ),
    MENU_ITEM   (0x59,LANG_MENU_OSD_RAW_EXCEPTIONS_PARAMS,  MENUITEM_SUBMENU,   &raw_exceptions_submenu, 0 ),
    MENU_ITEM   (0x5c,LANG_MENU_RAW_FIRST_ONLY,             MENUITEM_BOOL,      &conf.raw_save_first_only, 0 ),
    MENU_ENUM2a (0x5f,LANG_MENU_RAW_SAVE_IN_DIR,            &conf.raw_in_dir,   img_folders, NUM_IMG_FOLDER_NAMES ),
    MENU_ENUM2a (0x5f,LANG_MENU_RAW_PREFIX,                 &conf.raw_prefix,   img_prefixes, NUM_IMG_PREFIXES ),
    MENU_ENUM2a (0x5f,LANG_MENU_RAW_EXTENSION,              &conf.raw_ext,      img_exts, NUM_IMG_EXTS ),
    MENU_ENUM2a (0x5f,LANG_MENU_SUB_PREFIX,                 &conf.sub_batch_prefix, img_prefixes, NUM_IMG_PREFIXES ),
    MENU_ENUM2a (0x5f,LANG_MENU_SUB_EXTENSION,              &conf.sub_batch_ext, img_exts, NUM_IMG_EXTS ),
//  MENU_ITEM   (0x60,LANG_MENU_SUB_IN_DARK_VALUE,          MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.sub_in_dark_value, MENU_MINMAX(0, 1023) ),
//  MENU_ITEM   (0x60,LANG_MENU_SUB_OUT_DARK_VALUE,         MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.sub_out_dark_value, MENU_MINMAX(0, 1023) ),
    MENU_ITEM   (0x2a,LANG_MENU_RAW_DEVELOP,                MENUITEM_PROC,      gui_raw_develop, 0 ),
    MENU_ENUM2  (0x5c,LANG_MENU_BAD_PIXEL_REMOVAL,          &conf.bad_pixel_removal, gui_bad_pixel_removal_modes ),
    MENU_ITEM   (0x5c,LANG_MENU_RAW_CACHED,                 MENUITEM_BOOL,      &conf.raw_cache,            0 ),
#ifdef OPT_DEBUGGING
    MENU_ITEM   (0x5c,LANG_MENU_RAW_TIMER,                  MENUITEM_BOOL,      &conf.raw_timer,            0 ),
#endif
    MENU_ITEM   (0x0 ,(int)"DNG",                           MENUITEM_SEPARATOR, 0,                          0 ),
    MENU_ITEM   (0x5c,LANG_MENU_DNG_FORMAT,                 MENUITEM_BOOL | MENUITEM_ARG_CALLBACK, &conf.dng_raw , (int)cb_change_dng ),
    MENU_ITEM   (0x5c,LANG_MENU_RAW_DNG_EXT,                MENUITEM_BOOL,      &conf.raw_dng_ext, 0 ),
    MENU_ITEM   (0x5f,LANG_MENU_DNG_VERSION,                MENUITEM_ENUM,      gui_dng_version, 0),
    MENU_ENUM2  (0x5f,LANG_MENU_DNG_CROP_SIZE,              &conf.dng_crop_size,gui_dng_crop_size_modes),
    MENU_ITEM   (0x2a,LANG_MENU_BADPIXEL_CREATE,            MENUITEM_PROC,      gui_menuproc_badpixel_create, 0 ),
#if defined (DNG_EXT_FROM)
    MENU_ITEM   (0x71,LANG_MENU_DNG_VIA_USB,                MENUITEM_BOOL | MENUITEM_ARG_CALLBACK, &conf.dng_usb_ext , (int)cb_change_dng_usb_ext ),
#endif
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,        0,                          0 ),
    {0}
};

static CMenu raw_submenu = {0x24,LANG_MENU_RAW_TITLE, raw_submenu_items };

//-------------------------------------------------------------------

void cb_zebra_restore_screen()
{
    if (!conf.zebra_restore_screen)
        conf.zebra_restore_osd = 0;
}

void cb_zebra_restore_osd()
{
    if (conf.zebra_restore_osd)
        conf.zebra_restore_screen = 1;
}

static const char* gui_zebra_mode_modes[] = { "Blink 1", "Blink 2", "Blink 3", "Solid", "Zebra 1", "Zebra 2" };
#ifndef CAM_DRAW_YUV
static const char* gui_zebra_draw_osd_modes[] = { "Nothing", "Histo", "OSD" };
#endif
static const char* gui_zebra_draw_modes[] = { "Don't", "Shoot", "Rec", "Always" };

static CMenuItem zebra_submenu_items[] = {
    MENU_ENUM2(0x5f,LANG_MENU_ZEBRA_DRAW,             &conf.zebra_draw, gui_zebra_draw_modes ),
    MENU_ENUM2(0x5f,LANG_MENU_ZEBRA_MODE,             &conf.zebra_mode, gui_zebra_mode_modes ),
    MENU_ITEM(0x58,LANG_MENU_ZEBRA_UNDER,             MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.zebra_under,   MENU_MINMAX(0, 32) ),
    MENU_ITEM(0x57,LANG_MENU_ZEBRA_OVER,              MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,  &conf.zebra_over,    MENU_MINMAX(0, 32) ),
    MENU_ITEM(0x28,LANG_MENU_ZEBRA_RESTORE_SCREEN,    MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,      &conf.zebra_restore_screen,     cb_zebra_restore_screen ),
    MENU_ITEM(0x5c,LANG_MENU_ZEBRA_RESTORE_OSD,       MENUITEM_BOOL|MENUITEM_ARG_CALLBACK,      &conf.zebra_restore_osd,        cb_zebra_restore_osd ),
#ifndef CAM_DRAW_YUV
    MENU_ENUM2(0x5f,LANG_MENU_ZEBRA_DRAW_OVER,        &conf.zebra_draw_osd, gui_zebra_draw_osd_modes ),
#endif
    MENU_ITEM(0x5c,LANG_MENU_ZEBRA_MULTICHANNEL,      MENUITEM_BOOL,                            &conf.zebra_multichannel, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                    MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu zebra_submenu = {0x26,LANG_MENU_ZEBRA_TITLE, zebra_submenu_items };

//-------------------------------------------------------------------

static void gui_draw_lang_selected(const char *fn)
{
    if (fn) {
        strcpy(conf.lang_file, fn);
        lang_load_from_file(conf.lang_file);
        gui_menu_init(NULL);
    }
}

static void gui_draw_load_lang(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_SELECT_LANG_FILE, conf.lang_file, "A/CHDK/LANG", gui_draw_lang_selected);
}

static const char* gui_font_enum(int change, __attribute__ ((unused))int arg)
{
    extern int num_codepages;
    extern char* codepage_names[];

    gui_enum_value_change(&conf.font_cp,change,num_codepages);

    if (change != 0) {
        font_set(conf.font_cp);
        rbf_load_from_file(conf.menu_rbf_file, FONT_CP_WIN);
        gui_menu_init(NULL);
    }

    return codepage_names[conf.font_cp];
}

static void gui_draw_menu_rbf_selected(const char *fn)
{
    if (fn) {
        strcpy(conf.menu_rbf_file, fn);
        rbf_load_from_file(conf.menu_rbf_file, FONT_CP_WIN);
        gui_menu_init(NULL);
    }
}

static void gui_draw_load_menu_rbf(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_SELECT_FONT_FILE, conf.menu_rbf_file, "A/CHDK/FONTS", gui_draw_menu_rbf_selected);
}

static void gui_draw_symbol_rbf_selected(const char *fn)
{
    if (fn) {
        strcpy(conf.menu_symbol_rbf_file, fn);
        if(!rbf_load_symbol(conf.menu_symbol_rbf_file)) conf.menu_symbol_enable=0;      //AKA
        gui_menu_init(NULL);
    }
}

static void gui_draw_load_symbol_rbf(__attribute__ ((unused))int arg)
{
    libfselect->file_select(LANG_STR_SELECT_SYMBOL_FILE, conf.menu_symbol_rbf_file, "A/CHDK/SYMBOLS", gui_draw_symbol_rbf_selected);
}

static void gui_menuproc_reset_files(__attribute__ ((unused))int arg)
{
    conf.lang_file[0] = 0;
    strcpy(conf.menu_symbol_rbf_file,DEFAULT_SYMBOL_FILE);
    conf.menu_rbf_file[0] = 0;
    conf_save();
    gui_mbox_init(LANG_INFORMATION, LANG_MENU_RESTART_CAMERA, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
}

static const char* gui_text_box_charmap[] = { "Default", "German", "Russian" };

static CMenuItem menu_font_submenu_items[] = {
    MENU_ITEM(0x35,LANG_MENU_VIS_LANG,                MENUITEM_PROC,      gui_draw_load_lang, 0 ),
    MENU_ITEM(0x5f,LANG_MENU_VIS_OSD_FONT,            MENUITEM_ENUM,      gui_font_enum, &conf.font_cp ),
    MENU_ITEM(0x35,LANG_MENU_VIS_MENU_FONT,           MENUITEM_PROC,      gui_draw_load_menu_rbf, 0 ),
    MENU_ITEM(0x64,LANG_MENU_VIS_SYMBOL,              MENUITEM_BOOL,      &conf.menu_symbol_enable, 0 ),
    MENU_ITEM(0x35,LANG_MENU_VIS_MENU_SYMBOL_FONT,    MENUITEM_PROC,      gui_draw_load_symbol_rbf, 0 ),
    MENU_ENUM2(0x5f,LANG_MENU_VIS_CHARMAP,            &conf.tbox_char_map, gui_text_box_charmap ),
    MENU_ITEM(0x80,LANG_MENU_RESET_FILES,             MENUITEM_PROC,      gui_menuproc_reset_files, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                    MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu menu_font_submenu = {0x28,LANG_MENU_FONT_SETTINGS, menu_font_submenu_items };

//-------------------------------------------------------------------

static const char* gui_user_menu_show_modes[]={ "Off", "On", "On Direct" };

static CMenuItem menu_settings_submenu_items[] = {
    MENU_ENUM2(0x5f,LANG_MENU_USER_MENU_ENABLE,     &conf.user_menu_enable, gui_user_menu_show_modes ),
    MENU_ITEM(0x5c,LANG_MENU_USER_MENU_AS_ROOT,     MENUITEM_BOOL,          &conf.user_menu_as_root, 0 ),
    MENU_ITEM(0x72,LANG_MENU_USER_MENU_EDIT,        MENUITEM_PROC,          module_run, "useredit.flt" ),
    MENU_ITEM(0x81,LANG_MENU_VIS_MENU_CENTER,       MENUITEM_BOOL,          &conf.menu_center, 0 ),
    MENU_ITEM(0x81,LANG_MENU_SELECT_FIRST_ENTRY,    MENUITEM_BOOL,          &conf.menu_select_first_entry, 0 ),
    MENU_ITEM(0x5c,LANG_MENU_SHOW_ALT_HELP,         MENUITEM_BOOL,          &conf.show_alt_helper, 0 ),
    MENU_ITEM(0x58,LANG_MENU_SHOW_ALT_HELP_DELAY,   MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX, &conf.show_alt_helper_delay, MENU_MINMAX(0, 10) ),
    MENU_ITEM(0x28,LANG_MENU_FONT_SETTINGS,         MENUITEM_SUBMENU,       &menu_font_submenu, 0 ),
    MENU_ITEM(0x51,LANG_MENU_BACK,                  MENUITEM_UP, 0, 0 ),
    {0}
};

static CMenu menu_settings_submenu = {0x28,LANG_MENU_MENU_SETTINGS, menu_settings_submenu_items };

//-------------------------------------------------------------------

#if CAM_ADJUSTABLE_ALT_BUTTON

const char* gui_alt_mode_button_enum(int change, __attribute__ ((unused))int arg)
{
#if defined(CAM_ALT_BUTTON_NAMES) && defined(CAM_ALT_BUTTON_OPTIONS)
    static const char* names[] = CAM_ALT_BUTTON_NAMES;
    static const int keys[] = CAM_ALT_BUTTON_OPTIONS;
#else
#error Make sure CAM_ALT_BUTTON_NAMES and CAM_ALT_BUTTON_OPTIONS are defined in platform_camera.h
#endif
    int i;

    for (i=0; i<(int)(sizeof(names)/sizeof(names[0])); ++i) {
        if (conf.alt_mode_button==keys[i]) {
            break;
        }
    }

    i+=change;
    if (i<0)
        i=(sizeof(names)/sizeof(names[0]))-1;
    else if (i>=(int)((sizeof(names)/sizeof(names[0]))))
        i=0;

    conf.alt_mode_button = keys[i];
    return names[i];
}
#endif

#if CAM_OPTIONAL_EXTRA_BUTTON
static const char* gui_extra_button_enum(int change, __attribute__ ((unused))int arg)
{
#if defined(CAM_EXTRA_BUTTON_NAMES) && defined(CAM_EXTRA_BUTTON_OPTIONS)
    static const char* names[] = CAM_EXTRA_BUTTON_NAMES;
    static const int keys[] = CAM_EXTRA_BUTTON_OPTIONS;
#else
#error Make sure CAM_EXTRA_BUTTON_NAMES and CAM_EXTRA_BUTTON_OPTIONS are defined in platform_camera.h
#endif
    int i;

    for (i=0; i<(int)(sizeof(names)/sizeof(names[0])); ++i) {
        if (conf.extra_button==keys[i]) {
            break;
        }
    }

    i+=change;
    if (i<0)
        i=(sizeof(names)/sizeof(names[0]))-1;
    else if (i>=(int)(sizeof(names)/sizeof(names[0])))
        i=0;

    conf.extra_button = keys[i];
    kbd_set_extra_button((short)conf.extra_button);
    return names[i];
}
#endif //CAM_OPTIONAL_EXTRA_BUTTON

static const char* gui_raw_toggle_modes[] = { "Off", "On", "On+OSD" };

// Script option is retained even if scripting is disabled, otherwise conf values will change
// Equivalent to ALT
static const char* gui_alt_power_modes[]= { "Never", "Alt", "Script", "Always" };

static void gui_menuproc_reset_selected(unsigned int btn)
{
    if (btn==MBOX_BTN_YES)
        conf_load_defaults();
}

static void gui_menuproc_reset(__attribute__ ((unused))int arg)
{
    gui_mbox_init(LANG_MSG_RESET_OPTIONS_TITLE, 
                  LANG_MSG_RESET_OPTIONS_TEXT,
                  MBOX_FUNC_RESTORE|MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, gui_menuproc_reset_selected);
}

static CMenuItem chdk_settings_menu_items[] = {
    MENU_ITEM   (0x22,LANG_MENU_MAIN_OSD_PARAM,             MENUITEM_SUBMENU,   &osd_submenu, 0 ),
    MENU_ITEM   (0x72,LANG_MENU_OSD_LAYOUT_EDITOR,          MENUITEM_PROC,      module_run, "_osd_le.flt" ),
    MENU_ITEM   (0x28,LANG_MENU_MAIN_VISUAL_PARAM,          MENUITEM_SUBMENU,   &visual_submenu, 0 ),
#ifdef CAM_CUSTOM_BOOT_IMAGE
    MENU_ITEM   (0x28,(int)"Boot screen",                   MENUITEM_SUBMENU,   &boot_screen_submenu, 0 ),
#endif
    MENU_ITEM   (0x28,LANG_MENU_MENU_SETTINGS,              MENUITEM_SUBMENU,   &menu_settings_submenu, 0 ),
    MENU_ITEM   (0x2f,LANG_MENU_OSD_GRID_PARAMS,            MENUITEM_SUBMENU,   &grid_submenu, 0 ),
#ifdef CAM_HAS_GPS
    MENU_ITEM   (0x28,LANG_MENU_GPS,                        MENUITEM_SUBMENU,   &gps_submenu,       0 ),
#endif
#if CAM_REMOTE
    MENU_ITEM   (0x86,LANG_MENU_REMOTE_PARAM,               MENUITEM_SUBMENU,   &remote_submenu, 0 ),
#endif
    MENU_ITEM   (0x5c,LANG_MENU_MISC_ENABLE_SHORTCUTS,      MENUITEM_BOOL,      &conf.enable_shortcuts, 0 ),
    MENU_ENUM2  (0x5c,LANG_MENU_MISC_ENABLE_RAW_SHORTCUT,   &conf.enable_raw_shortcut, gui_raw_toggle_modes ),
    MENU_ITEM   (0x5c,LANG_MENU_MISC_SHOW_SPLASH,           MENUITEM_BOOL,      &conf.splash_show, 0 ),
    MENU_ITEM   (0x5c,LANG_MENU_MISC_START_SOUND,           MENUITEM_BOOL,      &conf.start_sound, 0 ),
#if CAM_USE_ZOOM_FOR_MF
    MENU_ITEM   (0x59,LANG_MENU_MISC_ZOOM_FOR_MF,           MENUITEM_BOOL,      &conf.use_zoom_mf, 0 ),
#endif
#if CAM_ADJUSTABLE_ALT_BUTTON
    MENU_ITEM   (0x22,LANG_MENU_MISC_ALT_BUTTON,            MENUITEM_ENUM,      gui_alt_mode_button_enum, 0 ),
#endif
#if CAM_OPTIONAL_EXTRA_BUTTON
    MENU_ITEM   (0x22,LANG_MENU_MISC_EXTRA_BUTTON,          MENUITEM_ENUM,      gui_extra_button_enum, 0 ),
#endif
#if defined(CAM_ZOOM_ASSIST_BUTTON_CONTROL)
    MENU_ITEM   (0x5c,LANG_MENU_MISC_ZOOM_ASSIST,           MENUITEM_BOOL,      &conf.zoom_assist_button_disable, 0 ),
#endif
    MENU_ENUM2  (0x5d,LANG_MENU_MISC_DISABLE_LCD_OFF,       &conf.alt_prevent_shutdown, gui_alt_power_modes ),
    MENU_ITEM   (0x2b,LANG_MENU_MAIN_RESET_OPTIONS,         MENUITEM_PROC,      gui_menuproc_reset, 0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP, 0, 0 ),
    {0}
};

CMenu chdk_settings_menu = {0x20,LANG_MENU_CHDK_SETTINGS, chdk_settings_menu_items };

//-------------------------------------------------------------------

extern CMenu script_submenu;

static CMenuItem root_menu_items[] = {
    MENU_ITEM   (0x21,LANG_MENU_OPERATION_PARAM,            MENUITEM_SUBMENU,   &operation_submenu, 0 ),
    MENU_ITEM   (0x23,LANG_MENU_VIDEO_PARAM,                MENUITEM_SUBMENU,   &video_submenu,     0 ),
    MENU_ITEM   (0x24,LANG_MENU_MAIN_RAW_PARAM,             MENUITEM_SUBMENU,   &raw_submenu,       0 ),
    // Its own top-level entry rather than a corner of the RAW menu. It is not a
    // RAW setting - it rewrites the sensor word before Canon develops the JPEG,
    // so it changes every shot whether RAW is being saved or not, and burying it
    // under RAW said the opposite. Symbol 0x39 was a blank slot in both shipped
    // icon fonts; see CHDK/SYMBOLS and BENDING_DESIGN.md.
    MENU_ITEM   (0x39,(int)"Circuit Bending",               MENUITEM_SUBMENU,   &bitbend_submenu,   0 ),
    // Beside Circuit Bending and above the RAW menu for the same reason: it
    // acts on the sensor buffer before Canon develops the JPEG, so it changes
    // the photograph whether RAW is being saved or not.
    MENU_ITEM   (0x39,(int)"Multiple exposure",             MENUITEM_SUBMENU,   &mexp_submenu,      0 ),
    MENU_ITEM   (0x7f,LANG_MENU_EDGE_OVERLAY,               MENUITEM_SUBMENU,   &edge_overlay_submenu, 0 ),
    MENU_ITEM   (0x25,LANG_MENU_MAIN_HISTO_PARAM,           MENUITEM_SUBMENU,   &histo_submenu, 0 ),
    MENU_ITEM   (0x26,LANG_MENU_MAIN_ZEBRA_PARAM,           MENUITEM_SUBMENU,   &zebra_submenu,     0 ),
    MENU_ITEM   (0x27,LANG_MENU_MAIN_SCRIPT_PARAM,          MENUITEM_SUBMENU,   &script_submenu,    0 ),
    // Top level rather than buried in Misc -> Games. It is not a diversion
    // between shots - it drives the lens and writes to DCIM, so it belongs
    // beside the other things that take photographs. The module is loaded by
    // filename, and carries MTYPE_EXTENSION so it does not also appear in the
    // Games menu.
    MENU_ITEM   (0x38,(int)"Game Boy emulator",             MENUITEM_PROC,      module_run, (int)"gbc.flt" ),
    MENU_ITEM   (0x22,LANG_MENU_CHDK_SETTINGS,              MENUITEM_SUBMENU,   &chdk_settings_menu, 0 ),
    MENU_ITEM   (0x29,LANG_MENU_MAIN_MISC,                  MENUITEM_SUBMENU,   &misc_submenu,      0 ),
    MENU_ITEM   (0x2e,LANG_MENU_USER_MENU,                  MENUITEM_SUBMENU,   &user_submenu, 0 ),             // Must be last item so it can be disabled
    {0}
};

CMenu root_menu = {0x20,LANG_MENU_MAIN_TITLE, root_menu_items };

//-------------------------------------------------------------------

const char* gui_on_off_enum(int change, int *conf_val)
{
    static const char* modes[]={ "Off", "On"};
    return gui_change_simple_enum(conf_val,change,modes,sizeof(modes)/sizeof(modes[0]));
}

#ifdef  CAM_TOUCHSCREEN_UI

const char* gui_override_disable_enum(int change, __attribute__ ((unused))int arg)
{
    return gui_change_simple_enum(&conf.override_disable,change,gui_override_disable_modes,sizeof(gui_override_disable_modes)/sizeof(gui_override_disable_modes[0]));
}

const char* gui_nd_filter_state_enum(int change, __attribute__ ((unused))int arg)
{
    return gui_change_simple_enum(&conf.nd_filter_state,change,gui_nd_filter_state_modes,sizeof(gui_nd_filter_state_modes)/sizeof(gui_nd_filter_state_modes[0]));
}

const char* gui_histo_show_enum(int change, __attribute__ ((unused))int arg)
{
    return gui_change_simple_enum(&conf.show_histo,change,gui_histo_show_modes,sizeof(gui_histo_show_modes)/sizeof(gui_histo_show_modes[0]));
}

#endif

//-------------------------------------------------------------------
// Splash screen handling


#if defined(VER_CHDK)
#define LOGO_WIDTH  149
#define LOGO_HEIGHT 84
#else
#define LOGO_WIDTH  169
#define LOGO_HEIGHT 74
#endif

static int gui_splash;
static char *logo = NULL;
static int logo_size, logo_text_width;

static void init_splash()
{
#ifdef CAM_PERSISTENT_OSD
    // No *version* splash on this camera. Erasing it reliably needs a repaint
    // that never comes: the refresh gate is held shut in record mode, so whatever
    // the splash draws stays on screen next to the persistent OSD indefinitely.
    // The custom Canon boot screen already confirms CHDK loaded, and the version
    // is still on the build info page in the CHDK menu, so nothing is lost.
    // Leaving gui_splash at 0 makes the whole splash path inert - no draw, so no
    // erase to get wrong.
    //
    // The boot credit is deliberately NOT run from here. gui_init() is called
    // while Canon's boot screen is still up, so anything counted down from this
    // point spends itself before the shooting screen exists. It lives with the
    // persistent OSD instead - see posd_credit_draw().
    gui_splash = 0;
#else
    gui_splash = (conf.splash_show) ? SPLASH_TIME : 0;
#endif

    if (gui_splash)
    {
#if defined(VER_CHDK)
        const char *logo_name="A/CHDK/DATA/logo.dat";
#else   // CHDK-DE
        const char *logo_name="A/CHDK/DATA/logo_de.dat";
#endif
        logo = load_file(logo_name, &logo_size, 0);

        logo_text_width = 0;

        int i;
        for (i=0; i<TEXT_COUNT; ++i)
        {
            int l = strlen(build_info[i]);
            if (l > logo_text_width) logo_text_width = l;
        }

        logo_text_width = logo_text_width * FONT_WIDTH + 10;
    }
}

static void gui_draw_logo_data(const char *data_buf, int data_size, int clear_first)
{
    const color logo_colors[8] =
    {
        COLOR_BLACK, COLOR_RED_DK, COLOR_RED, COLOR_GREY,
        COLOR_GREY_LT, COLOR_RED_LT, COLOR_TRANSPARENT, COLOR_WHITE
    };
    int pos, i, mx = 0, my = 0;
    int offset_x = (camera_screen.width-LOGO_WIDTH)>>1;
#ifdef CAM_DRAW_YUV
    int offset_y = ((camera_screen.height-LOGO_HEIGHT)>>1) - 66;
#else
    int offset_y = ((camera_screen.height-LOGO_HEIGHT)>>1) - 42;
#endif

    if (clear_first)
        draw_rectangle(offset_x, offset_y, offset_x+LOGO_WIDTH,
                       offset_y+LOGO_HEIGHT-1,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0|DRAW_FILLED);

    for (pos = 0; pos < data_size && my < LOGO_HEIGHT; pos++)
    {
        unsigned char d = (unsigned char)data_buf[pos];
        color c = logo_colors[(d>>5) & 0x07];
        for (i = 0; i < (d&0x1F)+1 && my < LOGO_HEIGHT; i++)
        {
            if (c != COLOR_TRANSPARENT)
                draw_pixel(offset_x+mx, offset_y+my, c);
            if (mx == LOGO_WIDTH) { mx = 0; my++; }
            else mx++;
        }
    }
}

#ifdef CAM_PERSISTENT_OSD
// Height of the strip the version line occupies, so it can be erased again.
#define SPLASH_STRIP    (FONT_HEIGHT + 10)

static void gui_draw_splash()
{
    // Just the version, on a plate at the bottom. The build date, camera model
    // and GCC lines are developer detail and were only ever in the way.
    const char *v = build_info[0];
    int tw = (int)strlen(v) * FONT_WIDTH;
    int tx = (camera_screen.width - tw) / 2;
    int ty = camera_screen.height - FONT_HEIGHT - 6;

    if (tx < 4) tx = 4;

    draw_rectangle(tx-4, ty-2, tx+tw+4, ty+FONT_HEIGHT+2,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK),
                   RECT_BORDER0|DRAW_FILLED|RECT_ROUND_CORNERS);
    draw_string(tx, ty, v, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
}

// Wipe the strip when the splash expires. gui_set_need_restore() is not enough
// on its own here: it asks Canon to repaint, and the refresh gate is held off in
// record mode, so the version line would simply stay on screen forever.
static void gui_erase_splash()
{
    draw_rectangle(0, camera_screen.height - SPLASH_STRIP,
                   camera_screen.width-1, camera_screen.height-1,
                   MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                   RECT_BORDER0|DRAW_FILLED);
}
#else
static void gui_draw_splash()
{
    coord x, y;
    int i;
    twoColors cl = MAKE_COLOR(COLOR_RED, COLOR_WHITE);

    x = (camera_screen.width-logo_text_width)>>1; 
    y = ((camera_screen.height-LOGO_TEXT_HEIGHT)>>1) + 20;

    draw_rectangle(x, y, x+logo_text_width, y+LOGO_TEXT_HEIGHT, MAKE_COLOR(COLOR_RED, COLOR_RED), RECT_BORDER0|DRAW_FILLED|RECT_ROUND_CORNERS);
    for (i=0; i<TEXT_COUNT; ++i)
    {
        draw_string_justified(x, y+i*FONT_HEIGHT+4, build_info[i], cl, 0, logo_text_width, TEXT_CENTER);
    }

#if OPT_EXPIRE_TEST
    do_expire_splash(x+((logo_text_width)>>1),y+(i+1)*FONT_HEIGHT+4);
#endif

    if (logo)
        gui_draw_logo_data(logo, logo_size, 0);
}

#endif  // CAM_PERSISTENT_OSD - end of the gui_draw_splash variants

#ifdef CAM_PERSISTENT_OSD
//-------------------------------------------------------------------
// Overlay drawn exactly the way the startup version box is drawn.
//
// This is a deliberate copy of gui_draw_splash()'s approach, because that box
// is the only thing that ever stayed on screen reliably on this camera. The
// stock OSD could not: measured on the a470, spytask runs at 50Hz in the CHDK
// menu (which draws no OSD) and at 1Hz on the shooting screen (which does),
// because one of the stock element functions blocks for most of a second.
//
// Keep the *drawing* cheap and keep it dumb: filled rectangles and strings and
// nothing else. The rule that has to hold is not "few stats", it is "no Canon
// call at frame rate" - every time a firmware call has been put on the per-pass
// path the flicker has come straight back. So everything that costs anything is
// sampled on one of the two timers below and cached as a finished string, and
// the per-pass work is blitting text that is already formatted.
//
// Both bodies run this same code, deliberately. The a470 holds Canon's refresh
// gate shut in record mode and the a480 does not, but that difference is not
// visible from here, and one implementation is what keeps the two cameras
// looking the same.
//-------------------------------------------------------------------

// Two plates in the bottom corners, three rows each, two independently
// coloured fields per row - left field flush left, right field flush right.
// 18 characters is 144px, so the pair leaves a 64px gap down the middle of the
// frame and takes nothing off the top, where the bend strip lives.
#define POSD_PLATES     2
#define POSD_ROWS       3
#define POSD_COLS       3
#define POSD_L          0                       // left field of a row
#define POSD_R          1                       // right field of a row
#define POSD_C          2                       // centred field of a row
#define POSD_CHARS      19
#define POSD_W          (FONT_WIDTH * POSD_CHARS)
#define POSD_H          (POSD_ROWS * FONT_HEIGHT + 4)
#define POSD_MARGIN     0
#define POSD_HIST_MARGIN 0

// Sampling timers. Nothing that reaches into Canon firmware runs outside these.
#define POSD_FAST_MS    500     // battery, exposure, raw state, clock
#define POSD_SLOW_MS    5000    // card space, frames left, sensor temperature
#define POSD_REPAIR_MS  200     // re-blit cached text after partial Canon repaints

// Thresholds at which a value stops being green and starts being a warning.
#define POSD_BATT_LOW   35      // percent
#define POSD_BATT_BAD   15
#define POSD_FREE_LOW   100     // MB

// One fixed colour per element, grouped by what the element is about rather
// than picked for variety: green for the battery, blue for the card, yellow for
// exposure, red for raw, magenta for the bend. Colours never cycle - cycling
// hues made every repaint visible, which is noise rather than information. The
// three colours that do vary vary with the value (battery level, card space,
// bend armed or not), never with time, so a change on screen really is a
// change in the thing.
//
// White is the neutral, used for plain readouts that need no interpretation.
// Grey is for the two static labels, which should not compete with any of it.
//
// Two aliases in this palette to watch for, both verified identical across all
// three ports' platform_palette.c. COLOR_CYAN is COLOR_BLUE_LT. And in *record*
// mode - the only mode this overlay draws in - COLOR_YELLOW_LT and
// COLOR_MAGENTA are both 0x66, so the shutter speed cannot be light yellow
// while the bend line is magenta. It is white instead, which it should have
// been anyway: it is a plain readout, and white is what plain readouts get.
// Two groups, and the split is the whole design.
//
// **Meaning colours are fixed.** Battery low and flat, card nearly full, raw
// armed, an override in force - these change with the *value*, so a change on
// screen has to mean a change in the thing. They are never shuffled and they
// are always the obvious colour: red for bad, yellow for warning, grey for off.
//
// **Readout colours are just identity.** Battery percent, voltage, ISO, shutter
// speed, temperature, zoom - the colour only says "this is that field". Nothing
// is lost by picking a different one, so these are the ones that get varied.
//
// This is what makes randomising safe here. OVERLAY_DESIGN.md records that
// cycling hues was tried and reverted, and it was right to: the flicker fix
// repaints only the rows whose text changed, so a colour that varies with time
// makes every repaint visible and turns the overlay back into noise. The
// shuffle below therefore happens **once per boot** and is then fixed for the
// session - variety every time the camera is switched on, and zero extra
// repaints while it is running.

enum {
    PE_BATT = 0, PE_VOLT, PE_TEMP, PE_FREE, PE_SHOTS, PE_ISO,
    PE_TV, PE_MODE, PE_ZOOM, PE_RAM, PE_PRESET, PE_BEND,
    PE_N                            // shuffled elements, above this line only
};

static color posd_pal[PE_N];

#define POSD_C_BATT     posd_pal[PE_BATT]
#define POSD_C_VOLT     posd_pal[PE_VOLT]
#define POSD_C_TEMP     posd_pal[PE_TEMP]
#define POSD_C_FREE     posd_pal[PE_FREE]
#define POSD_C_SHOTS    posd_pal[PE_SHOTS]
#define POSD_C_ISO      posd_pal[PE_ISO]
#define POSD_C_TV       posd_pal[PE_TV]
#define POSD_C_MODE     posd_pal[PE_MODE]
#define POSD_C_ZOOM     posd_pal[PE_ZOOM]
#define POSD_C_RAM      posd_pal[PE_RAM]
#define POSD_C_PRESET   posd_pal[PE_PRESET]
#define POSD_C_BEND     posd_pal[PE_BEND]

// Fixed - these carry meaning, see above.
// The states, as opposed to the readouts above. These say something is wrong,
// or costly, or on - so they go through the theme by meaning rather than by
// colour, the same slots the bend UI and the record menu use. They were fixed
// COLOR_* values, which is why a camera set to Frutiger Aero still had a red
// DNG+JPG marker sitting in the middle of a screen with no other red on it.
#define POSD_C_BATT_LOW theme_color(TC_WARN)
#define POSD_C_BATT_BAD theme_color(TC_BAD)
#define POSD_C_FREE_LOW theme_color(TC_BAD)
// Raw is not a fault - it is the expensive setting being on, which is what
// TC_SPECIAL means everywhere else in this fork.
#define POSD_C_RAW      theme_color(TC_SPECIAL)
// JPG is still the active output format, not an off/disabled state. Using DIM
// made it disappear into Gunmetal's dark-grey plate backing.
#define POSD_C_JPG      theme_color(TC_TEXT)
#define POSD_C_BEND_OFF theme_color(TC_DIM)
#define POSD_C_OVR      theme_color(TC_BAD)
#define POSD_C_WARN     theme_color(TC_BAD)
// A sequence in progress is a state the camera is in rather than a reading, so
// it takes the theme's "this is on" colour rather than one of the per-field
// ones the shuffle hands out.
#define POSD_C_MEXP     theme_color(TC_ACCENT)
#define POSD_C_CCD      theme_color(TC_BAD)

// The per-field assignment now comes from the theme - see core/theme.c, which
// carries the note about why these are spread across the palette rather than
// the near-monochrome they replaced, and why they are IDX_COLOR_* rather than
// COLOR_*.
//
// Every distinct colour this camera has in record mode that is worth reading
// text in - twelve of them, which is exactly PE_N, so a shuffle is a
// permutation and no two fields can come out the same.
//
// The greys are not in here: they are what the static labels use, and a readout
// that randomly turned grey would look disabled. The aliases are collapsed -
// COLOR_MAGENTA and COLOR_YELLOW_LT are one entry because they are one colour,
// and so are COLOR_CYAN and COLOR_BLUE_LT.
static const unsigned char posd_pal_pool[PE_N] = {
    IDX_COLOR_WHITE,        // 0x11
    IDX_COLOR_RED_LT,       // 0x21
    IDX_COLOR_RED,          // 0x22
    IDX_COLOR_RED_DK,       // 0x2E
    IDX_COLOR_GREEN_DK,     // 0x25
    IDX_COLOR_GREEN_LT,     // 0x51
    IDX_COLOR_GREEN,        // 0x55
    IDX_COLOR_YELLOW_LT,    // 0x66  (== COLOR_MAGENTA here)
    IDX_COLOR_YELLOW_DK,    // 0x6F
    IDX_COLOR_YELLOW,       // 0xEE
    IDX_COLOR_CYAN,         // 0xDD  (== COLOR_BLUE_LT here)
    IDX_COLOR_BLUE,         // 0xDF
};

// Called once, from the first sampling pass. Fisher-Yates over the pool, so
// the result is a permutation - every field gets a colour and no colour is
// used twice.
static void posd_pal_init(void)
{
    int i;

    for (i = 0; i < PE_N; i++)
        posd_pal[i] = theme_osd_color(i);

#ifdef CAM_PERSISTENT_OSD_RANDOM_COLORS
    {
        // get_tick_count() at the first overlay pass is the only entropy on
        // this path, and it differs between boots because the time from
        // power-on to that pass is not constant.
        unsigned seed = (unsigned)get_tick_count() | 1;
        unsigned char pool[PE_N];

        for (i = 0; i < PE_N; i++)
            pool[i] = posd_pal_pool[i];

        for (i = PE_N - 1; i > 0; i--)
        {
            int j;
            unsigned char t;
            seed = seed * 1103515245u + 12345u;     // plain LCG, no libc rand()
            j = (int)((seed >> 16) % (unsigned)(i + 1));
            t = pool[i]; pool[i] = pool[j]; pool[j] = t;
        }

        for (i = 0; i < PE_N; i++)
            posd_pal[i] = chdk_colors[pool[i]];
    }
#endif
}

#define POSD_BEND_CELL_W 10
#define POSD_BEND_W      POSD_W
#define POSD_BEND_Y      0

// The plate alternates two frames.
//
// One is the patchbay graphic, unchanged. The other takes it away and answers
// the three questions the graphic cannot: is an experimental profile on and
// which, is a segment layout on and which, and is this bend one that was
// saved. Three lines, one question each, always in the same order and always
// in the same place - a readout you have to search is not a readout.
//
// Deliberately not counters. An earlier version said "1/4" and "2/4" for the
// region and the chain position, which answers "where in the list is this" -
// a question nobody framing a photograph is asking. The name is the answer.
//
// The bend's name is the plate's heading - centred, on top - and the graphic
// is drawn thin under it, 28 pixels rather than the 36 it had. The name is on
// this frame rather than the other because this is the matrix it names:
// "BEND07" beside a picture of the routing means the picture you are looking
// at, and one frame away it was just a fact.
//
// Heading plus graphic is the whole height, with no padding rows - the plate
// is a heading and a picture, and space around them only pushes the live view
// down. The histogram box opposite follows this number, see POSD_HBOX_H.
#define POSD_BEND_MS     2500       // how long each frame holds
#define POSD_BEND_NAME_Y (POSD_BEND_Y)
#define POSD_BEND_GFX_Y  (POSD_BEND_Y + FONT_HEIGHT)
#define POSD_BEND_GFX_H  28
#define POSD_BEND_H      (FONT_HEIGHT + POSD_BEND_GFX_H)

// The stats frame's two rows, centred in the same box.
#define POSD_BEND_ROW(r) (POSD_BEND_Y + (POSD_BEND_H - 2 * FONT_HEIGHT) / 2 \
                          + (r) * FONT_HEIGHT)

#define PB_FRAME_MATRIX  0
#define PB_FRAME_STATS   1
#define PB_FRAME_N       2


static int posd_bend_was_shown;

// Lifted out of gui_draw_persistent_osd() so posd_hide_now() below can reset it
// from another task. posd_hide_until is the deadline that keeps the plates down
// once that has happened.
static int posd_plates_shown = 0;
static int posd_hide_until = 0;
#ifdef CAM_PERSISTENT_OSD_TRACK_REVIEW_EDGE
static volatile int posd_waiting_for_review;
static int posd_review_seen;
static int posd_review_deadline;
#endif

// True while a shot is holding everything CHDK draws off the screen. Read by
// posd_screen_active() for the plates and by gui_redraw() for the grid, so the
// two cannot get out of step - the grid was left drawing through the hold once
// and came back on the review just like the plates had.
static int posd_review_on_screen(void);

static int posd_shot_hold(void)
{
#ifdef CAM_PERSISTENT_OSD_TRACK_REVIEW_EDGE
    if (posd_waiting_for_review)
    {
        int now = get_tick_count();

        if (posd_review_on_screen())
        {
            posd_review_seen = 1;
        }
        else if (posd_review_seen)
        {
            // Canon has just handed the LCD back to live view. JPEG/card work
            // may continue (orange LED), but it no longer owns the bitmap.
            // This is the edge the whole mechanism exists for: it is the only
            // signal here that tracks the *picture leaving the screen* rather
            // than the save finishing behind it.
            posd_waiting_for_review = 0;
            posd_hide_until = 0;
        }
        else if (!posd_hide_until || ((now - posd_hide_until) >= 0))
        {
            // The hide window has run out and no review ever appeared. Either
            // review is switched off or this capture path does not start one,
            // so there is no falling edge coming and waiting for one would
            // strand the overlay until the next shot.
            //
            // Gated on the exposure being over rather than on the save being
            // over. Waiting for SHOOTING_PROGRESS to leave PROCESSING is what
            // the old version did, and that is exactly the JPEG/card tail this
            // is here to stop waiting for - it made the delay proportional to
            // how long the picture took to write. A long exposure can outlast
            // the hide window, though, and releasing during one would put the
            // overlay back on screen in time for Canon to freeze it into the
            // review, so STARTED - the exposure itself - still holds.
            if (camera_info.state.state_shooting_progress != SHOOTING_PROGRESS_STARTED)
                posd_waiting_for_review = 0;
        }
        else if (posd_review_deadline && ((now - posd_review_deadline) >= 0))
        {
            // Backstop. Everything above trusts recreview_hold, which is a
            // per-firmware address; if it is wrong and reads non-zero forever
            // the overlay would never come back at all. Give up rather than
            // hang. Long enough that a review the user is holding open ends
            // first on any of these bodies.
            posd_waiting_for_review = 0;
        }
    }
    if (posd_waiting_for_review)
        return 1;
#endif
#ifdef CAM_PERSISTENT_OSD_HOLD_THROUGH_PROCESSING
    // Still developing or writing the picture - see the define in camera.h.
    // Without this the hide timer expires mid-capture, raw_service_ui() repaints
    // the plates from inside the bend loops, and Canon freezes the bitmap for
    // the review with them already on it.
    if (camera_info.state.state_shooting_progress == SHOOTING_PROGRESS_PROCESSING)
        return 1;
#endif
    return posd_hide_until && (get_tick_count() < posd_hide_until);
}

// The grid, repainted from inside the bend redraw - see the call in bm_draw().
//
// Same guards as the paint in gui_redraw() below, minus the ones the caller
// has already settled: bm_draw() only runs in bend mode and only when it has
// decided to repaint.
void gui_bend_grid_repaint(void)
{
    extern int canon_menu_active;
    int canon_menu_hidden = (canon_menu_active != (int)&canon_menu_active-4)
                         || canon_shoot_menu_active;

    // The review as well: the grid must never land on Canon's reviewed
    // photograph, and unlike the plates it is not erased ahead of the review.
    //
    // This used to test recreview_hold == 0, and that is why the grid appeared
    // on top of the review in Bend UI mode while ordinary shooting hid it
    // correctly: ordinary shooting goes through posd_grid_active(), which was
    // corrected, and Bend UI comes through here, which was missed. On this
    // firmware recreview_hold is the review *hold* flag and reads 0 during an
    // ordinary review - see posd_review_on_screen().
    {
        if (camera_info.state.mode_rec && !canon_menu_hidden &&
            conf.show_grid_lines && !posd_review_on_screen() &&
            !posd_shot_hold() && libgrids)
            libgrids->gui_grid_draw_osd(1);
    }
}

#ifndef CAM_RECUI
// The handover, kept for cameras that do NOT have the record UI.
//
// Removing it was justified by gui_recui.c taking LEFT/RIGHT/UP/DOWN before
// Canon sees them - read the comment in posd_screen_active() below, which says
// exactly that. On a body without CAM_RECUI that premise is simply false:
// nothing takes the keys, so Canon's flash/macro/drive selectors still open,
// and the overlay redraws over a popup the firmware believes is on screen and
// being navigated. The two fight for the bitmap for as long as the selector is
// up, which is not a state any of these ports were ever meant to reach.
//
// So the rule is the premise, stated in code: CHDK owns the record screen
// while it owns the keys, and hands it back for POSD_CANON_MS after any key
// Canon's selectors use when it does not.
#define POSD_CANON_MS 5000
static int posd_canon_until = 0;

static int posd_canon_owns(void)
{
    if (kbd_is_key_pressed(KEY_LEFT) || kbd_is_key_pressed(KEY_RIGHT)
     || kbd_is_key_pressed(KEY_UP)   || kbd_is_key_pressed(KEY_DOWN)
     || kbd_is_key_pressed(KEY_SET))
        posd_canon_until = get_tick_count() + POSD_CANON_MS;
    return posd_canon_until && (get_tick_count() < posd_canon_until);
}
#else
static int posd_canon_owns(void) { return 0; }
#endif

// Whether Canon's post-shot review still blocks the overlay.
//
// Normally it does: recreview_hold is Canon's own "a review is on screen" flag
// and CHDK must stay off the bitmap while it is set.
//
// CAM_PERSISTENT_OSD_TIMEOUT_OWNS_REVIEW inverts who decides, for a body whose
// review *replaces* the bitmap rather than sharing it (see
// CAM_PERSISTENT_OSD_CANON_REVIEW_OWNS_ERASE, its partner). There the flag can
// stay asserted into the JPEG/card tail long after the picture has left the
// screen, and gating on it holds the overlay down for the whole save. The
// posd_hide_until deadline set by posd_hide_now() already covers the review
// window, so on those bodies the timeout is the more accurate of the two.
// "Is one of Canon's post-shot reviews on screen right now."
//
// CHDK's recreview_hold is the default answer and is wrong on at least one body
// here. Reversed out of the A470 102c ROM: 0x5b64, which finsig labels
// recreview_hold, is written in exactly two places - ShtCon_StartReview clears
// it to 0, and ShootCon_NotifyStartReviewHold (FUN_ffc6432c, the function
// finsig found it in) sets it to 1. It is the review *hold* flag, as its name
// says: it tracks the shutter being held to keep the review up. During an
// ordinary review it reads 0, so CHDK concluded no review was on screen and
// painted the overlay onto the photograph; and when the hold path did fire it
// stayed 1 until NotifyCompleteReviewHold at the end of the sequence, so the
// falling edge arrived only once the save had finished - the delay that grew
// with the bend.
//
// CAM_REVIEW_ACTIVE_FLAG is the address of the real one, where a port has
// reversed it. On the A470 that is 0x5b4c, the ShootCon state field set to 1 in
// ShtCon_StartReview beside the _EntryActionReview log and cleared in
// _ExitActionReview, and tested as a state guard in five other places.
//
// This is firmware-specific, not camera-specific. Do not copy the number.
static int posd_review_on_screen(void)
{
#ifdef CAM_REVIEW_ACTIVE_FLAG
    return *(volatile int*)CAM_REVIEW_ACTIVE_FLAG != 0;
#else
    extern int recreview_hold;
    return recreview_hold != 0;
#endif
}

// The same test, for code outside this file - see gui_bend.c. The Bend UI is
// drawn by its own gui_handler and had no idea a review was on screen.
int posd_review_active(void)
{
    return posd_review_on_screen();
}

static int posd_review_blocks(void)
{
#if defined(CAM_PERSISTENT_OSD_TIMEOUT_OWNS_REVIEW) && !defined(CAM_REVIEW_ACTIVE_FLAG)
    // Only meaningful while the flag being read is the wrong one. With a
    // signal that actually tracks the review, its level is the right test and
    // there is nothing to hand to a timeout.
    return 0;
#else
    return posd_review_on_screen();
#endif
}

// Exactly the screens on which CHDK owns the display. Keeping this test in one
// place makes the overlay, Canon-OSD gate and grid clipping change together.
static int posd_screen_active(void)
{
    extern int canon_menu_active;

    // There used to be a handover here. Any press of LEFT/RIGHT/UP/DOWN/SET
    // gave the bitmap back to Canon for POSD_CANON_MS, because Canon's flash
    // and macro popups are stateful selectors - the first press opens one and
    // later presses move the selection - so suppressing the popup made the keys
    // appear dead. The overlay went off screen for five seconds after every
    // arrow press, and the two UIs fought over the bitmap on the way in and out.
    //
    // It is gone because the premise stopped being true. The popup is a way of
    // writing the property, not a cache in front of it: SsFcsCtrl.c re-reads
    // FOCUS_MODE and REAL_FOCUS_MODE as each capture starts, and SsShootCtrl.c,
    // SsStrobeCtrl.c and SsExpCtrl.c do the same for FLASH_MODE and DRIVE_MODE.
    // See docs/A480_UI_REVERSING.md. So core/gui_recui.c takes those keys away
    // from Canon before the firmware sees them, writes the property itself, and
    // draws its own readout - and Canon never needs the bitmap back.
    //
    // POSD_CANON_MS went with it - but only where CAM_RECUI makes that true.
    // posd_canon_owns() above is the handover, still in force everywhere else.
    return !posd_canon_owns()
        && camera_info.state.mode_rec
        && !camera_info.state.mode_play
        && !posd_review_blocks()
        &&  camera_info.state.gui_mode_none
        && !camera_info.state.gui_mode_alt
        && (canon_menu_active == (int)&canon_menu_active-4)
        && !canon_shoot_menu_active
        && !camera_info.state.is_shutter_half_press
        && !kbd_is_key_pressed(KEY_SHOOT_HALF)
        && !kbd_is_key_pressed(KEY_SHOOT_FULL)
        // Canon needs bitmap refresh back before it handles the press that
        // opens its menu; waiting for canon_menu_active is one frame too late
        // on the A480 and leaves the menu with no visible first draw.
        && !kbd_is_key_pressed(KEY_MENU)
        // Set by posd_hide_now(), which runs ahead of Canon on the shutter.
        && !posd_shot_hold();
}

// The framing grid stays useful during half-press even though the large plates
// do not. Keep every other ownership guard identical to posd_screen_active():
// full press and review still clear it before Canon freezes the bitmap.
static int posd_grid_active(void)
{
    extern int canon_menu_active;

    // The grid keeps the review-flag gate even where the plates give it up.
    //
    // CAM_PERSISTENT_OSD_TIMEOUT_OWNS_REVIEW exists because on the A470 the
    // plates are erased before Canon's review and Canon's review owns the
    // bitmap from then on, so gating them on recreview_hold only held them down
    // into the JPEG/card tail. The grid is not erased the same way: measured on
    // the body, dropping this gate drew the framing grid straight onto the
    // reviewed photograph. So the timeout owns the plates, and Canon's flag
    // still owns the grid.
    return !posd_canon_owns()
        && camera_info.state.mode_rec
        && !camera_info.state.mode_play
        && !posd_review_on_screen()
        && camera_info.state.gui_mode_none
        && !camera_info.state.gui_mode_alt
        && (canon_menu_active == (int)&canon_menu_active-4)
        && !canon_shoot_menu_active
        && !kbd_is_key_pressed(KEY_MENU)
        && !kbd_is_key_pressed(KEY_SHOOT_FULL)
        && !posd_shot_hold();
}

// On-screen readout of every condition that decides whether the overlay and
// grid may paint. Drawn straight from spytask, unconditionally, so it survives
// exactly the situation it is diagnosing - the one where nothing else draws.
//
// This exists because the post-shot delay was guessed at twice and both guesses
// were wrong. The gates involved are spread across Canon firmware variables,
// CHDK state and a timer, and which of them is actually false during the stall
// is not deducible from the source - it has to be read off the body.
//
//   R  recreview_hold          Canon's "a review is on screen" flag
//   H  posd_shot_hold()        CHDK's own post-shutter hide
//   W  posd_waiting_for_review the review-edge wait inside posd_shot_hold()
//   S  state_shooting_progress 0 none 1 started 2 processing 3 done
//   A  posd_screen_active()    may the plates paint
//   G  posd_grid_active()      may the grid paint
//   M  gui_mode_none           no CHDK GUI is up
//   C  canon_menu_active       Canon thinks a menu is open
//   E  mode_rec                Canon reports record mode (from playrec_mode)
//   P  mode_play               Canon reports play mode
//   F  is_shutter_half_press
//
// Read it during the stall. Whichever of A/G is 0 names the layer that is
// blocked, and the terms around it say which gate did it.
//
// R/H/S/M/C were the whole line once, and that was not enough: a stall with
// all five reading "clear" and A still 0 leaves nothing named, which is the
// position the A470 was in. Every term posd_screen_active() actually tests is
// on the line now - in particular mode_rec, which is derived from Canon's
// playrec_mode and is the one gate here that is neither CHDK state nor a flag
// this file sets.
#ifdef CAM_POSD_GATE_DEBUG
unsigned posd_spy_loops;    // spytask main-loop passes; see the readout below

void posd_gate_debug_draw(void)
{
    extern int canon_menu_active;
    extern int recreview_hold;
    static char buf[64];
    int canon_menu = (canon_menu_active != (int)&canon_menu_active-4)
                  || canon_shoot_menu_active;

    {
        extern unsigned posd_spy_loops;
        extern int raw_stage;
        // L climbs only while spytask is running its own loop. Frozen L during
        // the stall means spytask is inside raw_process(), and Z says where.
        sprintf(buf, "L%u Z%d ", posd_spy_loops % 1000u, raw_stage);
    }
    sprintf(buf + strlen(buf), "R%d H%d W%d S%d A%d G%d M%d C%d E%d P%d F%d",
            recreview_hold ? 1 : 0,
            posd_shot_hold() ? 1 : 0,
#ifdef CAM_PERSISTENT_OSD_TRACK_REVIEW_EDGE
            posd_waiting_for_review ? 1 : 0,
#else
            0,
#endif
            (int)camera_info.state.state_shooting_progress,
            posd_screen_active() ? 1 : 0,
            posd_grid_active() ? 1 : 0,
            camera_info.state.gui_mode_none ? 1 : 0,
            canon_menu ? 1 : 0,
            camera_info.state.mode_rec ? 1 : 0,
            camera_info.state.mode_play ? 1 : 0,
            camera_info.state.is_shutter_half_press ? 1 : 0);
    draw_string(0, camera_screen.height - FONT_HEIGHT, buf,
                MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
}
#endif

// Shared by every full-screen assist layer. Shutter handling clears the bitmap
// before Canon sees the key; no background or Bend layer may put pixels back
// until that hold has elapsed.
int gui_shot_ui_hidden(void)
{
    return posd_shot_hold() || kbd_is_key_pressed(KEY_SHOOT_FULL);
}

// Acquire before grids and the overlay draw. Release only after both have
// erased themselves, so Canon's forced repaint is the last writer when the
// shutter or a menu needs the normal shooting display back.
static int posd_canon_osd_locked;
static void posd_canon_osd_begin(int active)
{
#ifdef CAM_PERSISTENT_OSD_CANON_REVIEW_OWNS_ERASE
    (void)active;
#else
    if (!active) return;
    if (!posd_canon_osd_locked)
    {
        // One balanced lock for the whole ownership interval. This used to
        // call vid_turn_off_updates() on every redraw but release it only once
        // in posd_canon_osd_end(), piling up Canon's recursive screen-lock
        // counter until menus and playback could no longer repaint at all.
        vid_turn_off_updates();
        draw_rectangle(0, 0, camera_screen.width-1, camera_screen.height-1,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0|DRAW_FILLED);
        posd_canon_osd_locked = 1;
    }
#endif
}

static void posd_canon_osd_end(int active)
{
#ifdef CAM_PERSISTENT_OSD_CANON_REVIEW_OWNS_ERASE
    (void)active;
#else
    if (!active && posd_canon_osd_locked)
    {
        posd_canon_osd_locked = 0;
        vid_turn_on_updates();
    }
#endif
}

// Short forms of bend_trash_names. The long ones ("Sensor seed") do not fit a
// plate next to "BEND d9 ", and the strip in bend mode is where the full names
// belong anyway.
// The formatted overlay. Written by posd_sample() on the timers above, read by
// the drawing pass every time it runs.
static char  posd_txt[POSD_PLATES][POSD_ROWS][POSD_COLS][POSD_CHARS+1];
static color posd_col[POSD_PLATES][POSD_ROWS][POSD_COLS];

static void posd_cell(int plate, int row, int side, color c, const char *s)
{
    char *d = posd_txt[plate][row][side];
    int i;
    for (i=0; (i<POSD_CHARS) && s[i]; i++) d[i] = s[i];
    d[i] = 0;
    posd_col[plate][row][side] = c;
}

// Shutter speed the way a camera writes it, because "1/125" is the number
// people think in and a decimal is not.
static void posd_fmt_tv(char *buf, short tv96)
{
    float t = shooting_get_shutter_speed_from_tv96(tv96);
    if (t <= 0.0f)
    {
        buf[0] = 0;
    }
    else if (t >= 1.0f)
    {
        int tenths = (int)(t * 10.0f + 0.5f);
        sprintf(buf, "%d.%ds", tenths / 10, tenths % 10);
    }
    else
    {
        int denom = (int)(1.0f / t + 0.5f);
        sprintf(buf, "1/%d", denom);
    }
}

// Card space in whatever unit keeps it to four significant characters, so the
// field does not change width as the card fills.
static void posd_fmt_free(char *buf, unsigned mb)
{
    if (mb >= 10240)        sprintf(buf, "FREE %uG", mb >> 10);
    else if (mb >= 1024)    sprintf(buf, "FREE %u.%uG", mb >> 10, ((mb & 1023) * 10) >> 10);
    else                    sprintf(buf, "FREE %uM", mb);
}

// Rebuild every cell. Called only from the timers, never from the draw pass.
static void posd_sample(int slow)
{
    static unsigned free_mb   = 0;
    static unsigned shots     = 0;
    static int      temp      = 0;
    static int      ccd_temp  = 0;
    static int      free_ram  = 0;
    char buf[POSD_CHARS+8];
    int raw_on, perc;
    static int pal_done;
    static int pal_theme = -1;

    // Once on the first pass - the shuffle needs get_tick_count() to have
    // moved, so it cannot be done at static-init time - and again whenever the
    // theme changes under it.
    //
    // These twelve are the one set of colours that is *resolved* rather than
    // looked up per draw: the readout fields are assigned once so a shuffle can
    // be a permutation. That caching is why picking a theme used to change
    // half the screen and leave the plates alone until the next boot.
    if (!pal_done || pal_theme != conf.theme)
    {
        pal_done  = 1;
        pal_theme = conf.theme;
        posd_pal_init();
    }

    if (slow)
    {
        // Three Canon calls, at 0.2Hz. GetRawCount() already folds in the free
        // space and the JPEG estimate, so when raw is armed it is the honest
        // number - a DNG on the a480 is 10MB and a JPEG is not.
        free_mb = GetFreeCardSpaceKb() >> 10;
        shots   = (conf.save_raw && is_raw_enabled()) ? GetRawCount() : GetJpgCount();
        temp    = get_optical_temp();
        ccd_temp = get_ccd_temp();
        free_ram = core_get_free_memory() >> 10;
    }

    //-- left plate: what the camera has left ------------------------------

    perc = (int)get_batt_perc();
    sprintf(buf, "BATT %d%%", perc);
    posd_cell(0, 0, POSD_L,
              (perc <= POSD_BATT_BAD) ? POSD_C_BATT_BAD :
              (perc <= POSD_BATT_LOW) ? POSD_C_BATT_LOW : POSD_C_BATT, buf);

    {
        long mv = stat_get_vbatt();
        sprintf(buf, "%ld.%02ldV", mv / 1000, (mv % 1000) / 10);
        posd_cell(0, 0, POSD_R, POSD_C_VOLT, buf);
    }
    posd_cell(0, 0, POSD_C, POSD_C_VOLT, "");

    sprintf(buf, "ISO %d", shooting_get_iso_real());
    posd_cell(0, 1, POSD_L, POSD_C_ISO, buf);

    posd_fmt_tv(buf, shooting_get_tv96());
    posd_cell(0, 1, POSD_R, POSD_C_TV, buf);

    posd_fmt_free(buf, free_mb);
    posd_cell(0, 2, POSD_L,
              (free_mb <= POSD_FREE_LOW) ? POSD_C_FREE_LOW : POSD_C_FREE, buf);

    sprintf(buf, "RAM %dK", free_ram);
    posd_cell(0, 2, POSD_R, POSD_C_RAM, buf);
    posd_cell(0, 2, POSD_C, POSD_C_RAM, "");

    //-- right plate: what CHDK is doing to the shot -----------------------

    raw_on = conf.save_raw && is_raw_enabled();
    posd_cell(1, 0, POSD_L, raw_on ? POSD_C_RAW : POSD_C_JPG,
              !raw_on ? "JPG" : (conf.dng_raw ? "DNG+JPG" : "RAW+JPG"));
    sprintf(buf, "PIC %u", shots);
    posd_cell(1, 0, POSD_R, POSD_C_SHOTS, buf);
    posd_cell(1, 0, POSD_C, POSD_C_SHOTS, "");

    // "TEMP" shortened to "T" to make room for the mode field beside it. The
    // two temperatures are still both there and still labelled by position -
    // body first, sensor second - which is what the old label said with eight
    // more characters.
    sprintf(buf, "T %dC/%dC", temp, ccd_temp);
    posd_cell(1, 1, POSD_L, POSD_C_TEMP, buf);
    posd_cell(1, 1, POSD_C, POSD_C_TEMP, "");

    // Flash and focus mode, because Canon cannot draw its own any more.
    //
    // The overlay takes Canon's screen lock for the whole time it is up (see
    // posd_canon_osd_begin) and wipes the bitmap once on the way in. That is
    // what stops the stock OSD tearing through the plates, and it is also why
    // the flash, macro and infinity icons stopped appearing: they are Canon's
    // to draw and Canon has been told not to draw. Giving the lock back for
    // them would bring the tearing back with them, so they are drawn here
    // instead, from the same propcases Canon reads.
    //
    // shooting_get_real_focus_mode() folds the MF flag (PROPCASE_FOCUS_MODE)
    // and the mode dial (PROPCASE_REAL_FOCUS_MODE) into one value in CHDK's
    // documented space. Normal focus prints nothing, the way Canon shows no
    // icon for it - an indicator that is always lit is not an indicator.
    {
        short fl = shooting_get_flash_mode();
        short fo = shooting_get_real_focus_mode();
        char *p = buf;

        if      (fl == 0) { *p++ = 'F'; *p++ = 'A'; }    // auto
        else if (fl == 1) { *p++ = 'F'; *p++ = '+'; }    // forced on
        else if (fl == 2) { *p++ = 'F'; *p++ = '-'; }    // off
        else              { *p++ = 'F'; *p++ = '?'; }

        switch (fo)
        {
        case 0: break;                                          // normal AF
        case 1: *p++ = ' '; *p++ = 'M'; *p++ = 'F';   break;
        case 3: *p++ = ' '; *p++ = 'I'; *p++ = 'N'; *p++ = 'F'; break;
        case 4: *p++ = ' '; *p++ = 'M'; *p++ = 'A'; *p++ = 'C'; break;
        case 5: *p++ = ' '; *p++ = 'S'; *p++ = 'M'; *p++ = 'C'; break;
        default:
            // Not a value this camera was expected to report. Show it rather
            // than swallow it - a number on screen can be read out and mapped,
            // a silently wrong label cannot.
            *p++ = ' '; *p++ = '?';
            *p++ = (char)('0' + (fo % 10));
            break;
        }
        *p = 0;
        posd_cell(1, 1, POSD_R, POSD_C_MODE, buf);
    }

    {
        int zp = shooting_get_zoom();
        int efl = get_effective_focal_length(zp) / 1000;
        long focus = lens_get_focus_pos();
        if (focus < 0 || focus >= 65500)
            sprintf(buf, "Z%d/%d %dmm F INF", zp, zoom_points - 1, efl);
        else if (focus >= 1000)
            sprintf(buf, "Z%d/%d %dmm F%ld.%ldm", zp, zoom_points - 1, efl,
                    focus / 1000, (focus % 1000) / 100);
        else
            sprintf(buf, "Z%d/%d %dmm F%ldcm", zp, zoom_points - 1, efl,
                    focus / 10);
        posd_cell(1, 2, POSD_L, POSD_C_ZOOM, buf);
        posd_cell(1, 2, POSD_C, POSD_C_ZOOM, "");
        posd_cell(1, 2, POSD_R, POSD_C_ZOOM, "");
    }

}

static int posd_plate_x(int plate)
{
    return (plate == 0) ? POSD_MARGIN : (camera_screen.width - POSD_W - POSD_MARGIN);
}

// Paint scanlines y0..y1 of a plate whose top edge is at ytop. Text is drawn
// over this with a solid black cell background, so the stripes show in the
// padding and in the gap between the two fields of a row - which is most of
// what reads as "the black box" - while the characters themselves keep an
// opaque backing and stay legible against a bright scene.
//
// ytop is passed separately from y0 so the stripe phase is anchored to the
// plate rather than to the band being painted: repainting one row has to land
// its black lines on the same rows the full-plate paint would have.
static void posd_clear_w(int bx, int w, int y0, int y1)
{
    draw_rectangle(bx, y0, bx+w, y1,
                   MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                   RECT_BORDER0|DRAW_FILLED);
}

static void posd_clear_backing(int bx, int y0, int y1)
{
    posd_clear_w(bx, POSD_W, y0, y1);
}

// Match the CHDK menu background. On the A480 the selected dark grey palette
// entry is blended by Canon, giving the menu its translucent smoked-glass look.
// Going through user_color also keeps the overlay in step with menu colour
// changes instead of hard-coding a camera palette index here.
static color posd_background_color(void)
{
    return BG_COLOR(user_color(conf.menu_color));
}

// Backing for the stat rows. Keeping it confined to the text rows avoids the
// oversized plates used by the first persistent-OSD version.
static void posd_stat_backing(int bx, int w, int y0, int y1)
{
    color bg = posd_background_color();
    draw_rectangle(bx, y0, bx+w, y1,
                   MAKE_COLOR(bg, bg),
                   RECT_BORDER0|DRAW_FILLED);
}

//-------------------------------------------------------------------
// Luminance histogram, computed and drawn here rather than by libhisto.
//
// The stock histogram lives in a module (histo.flt) and is reached through
// module_load(). That works on the a470 and a480; on the a460 module loading is
// currently broken, so the module route silently draws nothing at all - the
// default stub tries to load, fails, and returns. Doing it in core means the
// overlay looks the same on all three bodies whatever the loader is doing, and
// it costs nothing on the two where the module does work, because this replaces
// the call rather than adding to it.
//
// The sampling is the same walk libhisto does. The viewport is UYVYYY - six
// bytes per four pixels, luma at p[1] of each group - so stepping six bytes
// takes one luma sample per four pixels. That is far more than 64 bins need,
// and every row is skipped in fours as well, which keeps the pass short enough
// to sit on the same timer as everything else here.

#define POSD_HB         64                      // bins
#define POSD_HBARW      2                       // pixels per bin
#define POSD_HW         (POSD_HB * POSD_HBARW)  // 128
// Both boxes in the top band are the same height, and that is a constraint
// rather than a coincidence: they sit either side of the multiple-exposure
// counter and read as one band across the screen, so one growing without the
// other breaks the line they make. The height is set by the taller need of the
// two - the bend plate's stats frame, which is three rows of text - and the
// histogram's bars simply grow to fill it. See POSD_BEND_H.
#define POSD_HH         (POSD_BEND_H - 4)        // bar height
#define POSD_HBOX_W     POSD_W
#define POSD_HBOX_H     (POSD_HH + 4)

// The bars are a gradient - see theme_histo(). The clipped bin is not, and
// takes the warning colour whole: the comment below has always said that bin
// "gets its own colour" and it never did - every bar was white - which meant
// the one bar in the histogram worth being told about looked like all the
// others.
#define POSD_C_HISTO_HI theme_color(TC_WARN)

static unsigned char posd_histo[POSD_HB];       // bar height per bin, 0..POSD_HH
static unsigned char posd_histo_shown[POSD_HB];
static int posd_histo_valid;

static void posd_histo_sample(void)
{
    unsigned bins[POSD_HB];
    unsigned char *img, *p_row, *p_max;
    int bw, vw, h, i, peak;

    posd_histo_valid = 0;

    img = (unsigned char *)vid_get_viewport_active_buffer();
    if (!img) return;
    img += vid_get_viewport_image_offset();

    bw = vid_get_viewport_byte_width();
    h  = vid_get_viewport_height_proper();
    vw = (vid_get_viewport_width_proper() * 3) / 2;     // visible bytes per row
    if (bw <= 0 || h <= 0 || vw <= 0) return;

    for (i = 0; i < POSD_HB; i++) bins[i] = 0;

    // Every 4th row, one luma sample per 4 pixels along it.
    p_row = img;
    p_max = img + bw * h;
    for (; p_row < p_max; p_row += bw * 4)
    {
        unsigned char *p   = p_row;
        unsigned char *end = p_row + vw;
        for (; p < end; p += 6)
            bins[p[1] >> 2]++;                          // 256 levels -> 64 bins
    }

    // Scale to the box. Normalising to the peak rather than to a fixed count is
    // what makes it readable across scenes - an absolute scale is either flat or
    // clipped depending only on how much of the frame is sky.
    peak = 1;
    for (i = 0; i < POSD_HB; i++) if ((int)bins[i] > peak) peak = bins[i];
    for (i = 0; i < POSD_HB; i++)
    {
        unsigned v = (bins[i] * POSD_HH) / peak;
        posd_histo[i] = (unsigned char)((v > POSD_HH) ? POSD_HH : v);
    }
    posd_histo_valid = 1;
}

static int posd_histo_changed(void)
{
    int i;
    for (i = 0; i < POSD_HB; i++)
        if (posd_histo[i] != posd_histo_shown[i]) return 1;
    return 0;
}

// Top left, aligned with the left plate below it.
static void posd_histo_draw(void)
{
    int bx = POSD_HIST_MARGIN, by = POSD_HIST_MARGIN, i;
    int bars_x = bx + (POSD_HBOX_W - POSD_HW) / 2;
    color bg = posd_background_color();

    draw_rectangle(bx, by, bx + POSD_HBOX_W, by + POSD_HBOX_H,
                   MAKE_COLOR(bg, bg),
                   RECT_BORDER0|DRAW_FILLED);

    // The gradient runs up the box, not up each bar: a pixel's colour comes
    // from where it is, so the bars read as one field of colour with a
    // gradient across it rather than as sixty-four independently coloured
    // sticks. Each bar is drawn as its intersection with the three bands,
    // which is at most three rectangles and usually one or two.
    for (i = 0; i < POSD_HB; i++)
    {
        int hgt  = posd_histo[i];
        int x    = bars_x + i * POSD_HBARW;
        int top  = by + 2 + POSD_HH - hgt;
        int foot = by + 2 + POSD_HH - 1;
        int band;

        posd_histo_shown[i] = (unsigned char)hgt;
        if (hgt <= 0) continue;

        // Top bin is the clipped one, and clipping is the thing you actually
        // want to be told about, so it gets its own colour and stays out of
        // the gradient entirely.
        if (i == POSD_HB - 1)
        {
            color c = POSD_C_HISTO_HI;
            draw_rectangle(x, top, x + POSD_HBARW - 1, foot,
                           MAKE_COLOR(c, c), RECT_BORDER0|DRAW_FILLED);
            continue;
        }

        for (band = 0; band < THEME_HISTO_BANDS; band++)
        {
            // Band 0 is the bottom of the box, which is where the gradient's
            // first colour goes - so the bands are walked from the bottom up.
            int bh = POSD_HH / THEME_HISTO_BANDS;
            int b1 = foot - band * bh;
            int b0 = (band == THEME_HISTO_BANDS - 1) ? (by + 2) : (b1 - bh + 1);
            color c = theme_histo(band);

            if (b1 < top) break;                // the bar ends below this band
            if (b0 < top) b0 = top;
            draw_rectangle(x, b0, x + POSD_HBARW - 1, b1,
                           MAKE_COLOR(c, c), RECT_BORDER0|DRAW_FILLED);
        }
    }
}

// What is currently on screen, as opposed to what posd_sample() last worked
// out. The two are compared row by row and only the rows that differ are
// repainted - see the note on tearing above gui_draw_persistent_osd().
static char  posd_shown[POSD_PLATES][POSD_ROWS][POSD_COLS][POSD_CHARS+1];
static color posd_shown_col[POSD_PLATES][POSD_ROWS][POSD_COLS];

static int posd_row_changed(int i, int j)
{
    int k;
    for (k=0; k<POSD_COLS; k++)
    {
        if (posd_shown_col[i][j][k] != posd_col[i][j][k]) return 1;
        if (strcmp(posd_shown[i][j][k], posd_txt[i][j][k]) != 0) return 1;
    }
    return 0;
}

static void posd_draw_field(int bx, int y, const char *s, color fg, int align)
{
    draw_string_justified(bx+4, y, s,
                          MAKE_COLOR(posd_background_color(), fg),
                          0, POSD_W-8, align);
}

static void posd_row_draw(int i, int j, int bx, int y)
{
    int ry = y + j*FONT_HEIGHT + 2;
    int k;

    posd_clear_backing(bx, ry, ry + FONT_HEIGHT - 1);
    posd_stat_backing(bx, POSD_W, ry, ry + FONT_HEIGHT - 1);

    // Left field first, then the right one over the same span. Neither asks for
    // TEXT_FILL, so the second does not erase the first.
    if (posd_txt[i][j][POSD_L][0])
        posd_draw_field(bx, ry, posd_txt[i][j][POSD_L],
                        posd_col[i][j][POSD_L], TEXT_LEFT);
    if (posd_txt[i][j][POSD_R][0])
        posd_draw_field(bx, ry, posd_txt[i][j][POSD_R],
                        posd_col[i][j][POSD_R], TEXT_RIGHT);
    if (posd_txt[i][j][POSD_C][0])
        posd_draw_field(bx, ry, posd_txt[i][j][POSD_C],
                        posd_col[i][j][POSD_C], TEXT_CENTER);

    for (k=0; k<POSD_COLS; k++)
    {
        strcpy(posd_shown[i][j][k], posd_txt[i][j][k]);
        posd_shown_col[i][j][k] = posd_col[i][j][k];
    }
}

// Repair text without clearing/striping the whole row first. This closes the
// partial-repaint hole without bringing back the visible blank-then-text tear
// that full unconditional redraws caused on the A480.
static void posd_row_repair(int i, int j, int bx, int y)
{
    int ry = y + j*FONT_HEIGHT + 2;
    posd_stat_backing(bx, POSD_W, ry, ry + FONT_HEIGHT - 1);
    if (posd_txt[i][j][POSD_L][0])
        posd_draw_field(bx, ry, posd_txt[i][j][POSD_L],
                        posd_col[i][j][POSD_L], TEXT_LEFT);
    if (posd_txt[i][j][POSD_R][0])
        posd_draw_field(bx, ry, posd_txt[i][j][POSD_R],
                        posd_col[i][j][POSD_R], TEXT_RIGHT);
    if (posd_txt[i][j][POSD_C][0])
        posd_draw_field(bx, ry, posd_txt[i][j][POSD_C],
                        posd_col[i][j][POSD_C], TEXT_CENTER);
}

// Erase the two plates. Needed when hiding on shutter press: on the a470 Canon's
// refresh is held off in record mode, so nothing else will clear what we drew.
static void posd_clear(int y)
{
    int i;
    for (i=0; i<POSD_PLATES; i++)
    {
        int bx = posd_plate_x(i);
        draw_rectangle(bx, y, bx+POSD_W, y+POSD_H,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0|DRAW_FILLED);
    }
    // The warning line is drawn by the same function, on the row above the
    // plates, so it has to come off with them. Nothing else will clear it -
    // with the refresh gate held there is no repaint to fall back on.
    draw_rectangle(0, y - FONT_HEIGHT - 4, camera_screen.width-1, y - 1,
                   MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                   RECT_BORDER0|DRAW_FILLED);
    if (posd_bend_was_shown)
    {
        int bx = camera_screen.width - POSD_BEND_W - POSD_MARGIN;
        draw_rectangle(bx, POSD_BEND_Y, bx + POSD_BEND_W, POSD_BEND_Y + POSD_BEND_H,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0|DRAW_FILLED);
        posd_bend_was_shown = 0;
    }
}

// Take the plates off the screen NOW, from whoever is calling, and hold them
// down for a moment afterwards.
//
// This exists because erasing on the next redraw pass is too late. The plates
// are gone from the buffer CHDK draws into within 40ms of the shutter moving,
// and that is still late enough for Canon to have frozen the bitmap with them
// still in it - which is what the review then shows, out of a buffer we never
// write again. Erasing after that point cannot work however often it is
// repeated, and repeating it 25 times a second for four seconds was measured
// doing exactly nothing.
//
// So the erase moves ahead of Canon instead. kbd_update_key_state() calls this
// from the physw task on the shutter keys, before the firmware is handed the
// key state at all, and capt_seq_hook_set_nr() calls it at exposure start to
// cover the shots no key press starts - self timer, remote, scripted.
void posd_hide_now(int hold_ms, int clear_all)
{
    int y = camera_screen.height - POSD_H - POSD_MARGIN;
    int t;

    if (clear_all)
    {
#ifdef CAM_PERSISTENT_OSD_TRACK_REVIEW_EDGE
        // Rising edge only. kbd_update_key_state() calls this on the *level* -
        // once per key scan for as long as the shutter is held - and clearing
        // posd_review_seen on every one of those is a race the user loses by
        // holding the button down: Canon can raise and drop recreview_hold
        // inside the hold, and each scan wipes the fact that the rise was seen.
        // The edge is then never completed, posd_waiting_for_review stays set,
        // and the overlay does not come back until the *next* shot re-arms the
        // whole thing - which is exactly "sometimes pressing the shutter brings
        // it back".
        //
        // The hold deadline below still moves forward on every call, which is
        // the wanted level behaviour. Only the edge state is latched.
        if (!posd_waiting_for_review)
        {
            posd_waiting_for_review = 1;
            posd_review_seen = 0;
            posd_review_deadline = get_tick_count() + CAM_PERSISTENT_OSD_REVIEW_MAX_MS;
            if (posd_review_deadline == 0) posd_review_deadline = 1;
        }
#endif
        // The plates are not the only thing CHDK has on the screen. The grid is
        // drawn from gui_redraw() on a test of its own and posd_clear() has
        // never touched it, so on a shot it stayed up and got frozen into the
        // review exactly as the plates did. Everything CHDK drew has to be gone
        // before Canon takes its copy, and the cheapest way to be sure of that
        // is not to enumerate it - it is the same full-screen wipe that
        // posd_canon_osd_begin() already does when CHDK takes the screen.
        //
        // Canon's own OSD goes with it. That is why this is the shot path only:
        // posd_canon_osd_end() releases the refresh gate as soon as the overlay
        // reports inactive, and Canon repaints its side within the frame.
        draw_rectangle(0, 0, camera_screen.width-1, camera_screen.height-1,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0|DRAW_FILLED);
        posd_bend_was_shown = 0;
        posd_plates_shown = 0;
    }
    else if (posd_plates_shown)
    {
        posd_clear(y);
        posd_plates_shown = 0;
    }

    t = get_tick_count() + hold_ms;
    if (t == 0) t = 1;                  // 0 is the "no deadline" value
    posd_hide_until = t;

}

static color posd_bend_source_color(unsigned char src)
{
    static const unsigned char pin_color_idx[] = {
        IDX_COLOR_BLUE_LT, IDX_COLOR_GREEN_LT, IDX_COLOR_YELLOW, IDX_COLOR_MAGENTA,
        IDX_COLOR_CYAN, IDX_COLOR_GREEN, IDX_COLOR_RED_LT, IDX_COLOR_GREY_LT
    };
    // The eight pin colours stay a fixed spread whatever the theme is. They are
    // identity - "this trace is that pin" - and ten traces crossing in 150
    // pixels are only followable because no two of them look alike, which is
    // exactly what a theme collapsing them to one hue would take away.
    //
    // Everything that is not a pin does follow the theme: the rails and the
    // injected buses are states, and they were the red and yellow that made a
    // Frutiger Aero screen look like it had a fault on it.
    unsigned cls = BSRC_CLASS(src);
    if (src == BSRC_LOW)  return COLOR_BLACK;
    if (src == BSRC_HIGH) return theme_color(TC_WARN);
    if (cls == BSRC_C_DATA || cls == BSRC_C_NDATA)
        return chdk_colors[pin_color_idx[BSRC_IDX(src) & 7]];
    if (cls == BSRC_C_BUS || cls == BSRC_C_NBUS)
        return theme_color(TC_SPECIAL);
    return theme_color(TC_DIM);
}

// A miniature patchbay. Source pins run across the top and output pins across
// the bottom. Straight routes are quiet vertical traces; swaps visibly cross;
// GND and HI enter from the left and right rails; buses enter from the centre.
// The order is MSB on the left, matching the bend editor.
// Truncating copy into a POSD_CHARS + 1 buffer. Everything on this plate is
// built into a working buffer and then cut to the line, because several of the
// things named here are longer than it: a profile name is up to twelve
// characters and a region label is words.
static void pb_line(char *out, const char *src)
{
    int i;
    for (i = 0; i < POSD_CHARS && src[i]; i++) out[i] = src[i];
    out[i] = 0;
}

// Centred, the way the name is on the other frame. The two frames are the same
// plate a beat apart, so text that changed sides as it flipped would read as
// the plate moving rather than as the plate turning over.
static void pb_row(int bx, color bg, int row, const char *text, color fg)
{
    char buf[POSD_CHARS + 1];
    pb_line(buf, text);
    draw_string_justified(bx, POSD_BEND_ROW(row), buf, MAKE_COLOR(bg, fg),
                          0, POSD_BEND_W, TEXT_CENTER);
}

// Whether what is applied is a thing you can get back, written under the
// graphic that draws it. A matrix that has been rerolled or hand-tuned since it
// was loaded is not the file any more, which is exactly when it is worth
// knowing.
static void posd_bend_name_row(int bx, color bg, int bent)
{
    char tmp[96];
    int cur = bend_store_cur();

    if (cur >= 0)  sprintf(tmp, "SAVED BEND%02d", bend_store_slot_at(cur));
    else if (bent) sprintf(tmp, "UNSAVED bend");
    else           sprintf(tmp, "no bend");

    {
        char buf[POSD_CHARS + 1];
        pb_line(buf, tmp);
        draw_string_justified(bx, POSD_BEND_NAME_Y, buf,
                              MAKE_COLOR(bg, (cur >= 0) ? theme_color(TC_ACCENT)
                                            : (bent ? theme_color(TC_WARN)
                                                    : theme_color(TC_DIM))),
                              0, POSD_BEND_W, TEXT_CENTER);
    }
}

// The stats frame. Two lines, one question each.
//
// Every line is present whether or not its engine is on, and says "off" when
// it is not. A row that disappears when the answer is no makes the other two
// move, and then the line you are looking for is wherever it happens to be
// this second rather than always in the same place.
static void posd_bend_stats_frame(int bx, color bg, int xon)
{
    char tmp[96];
    int on;

    // Which fault, or that there are several. A chain has no name of its own -
    // that is what makes it a chain - so it says how many and the bend menu
    // has the running order.
#ifdef CAM_BEND_EXPERIMENTAL
    {
        int cn = xon ? bendx_chain_count(&conf.bendx) : 0;

        if (cn > 1)      sprintf(tmp, "X  chain of %d", cn);
        else if (cn == 1) sprintf(tmp, "X  %s", bendx_name(conf.bendx.item[0].kind));
        else              sprintf(tmp, "X  off");
        pb_row(bx, bg, 0, tmp,
               cn ? (bendx_chain_slow(&conf.bendx) ? theme_color(TC_WARN)
                                                   : POSD_C_BEND)
                  : theme_color(TC_TEXT));
    }
#else
    (void)xon;
    pb_row(bx, bg, 0, "X  off", theme_color(TC_TEXT));
#endif

    // Which layout, by name. Not which region of it: which quarter the strip
    // happens to be editing is a fact about the menu, not about the picture,
    // and it used to sit in the corner of the graphic as a bare "2/4".
    on = conf.bend_segs.layout != BSEG_OFF;
    sprintf(tmp, "SEG %s", on ? bend_seg_name(conf.bend_segs.layout) : "off");
    pb_row(bx, bg, 1, tmp, on ? theme_color(TC_ACCENT2) : theme_color(TC_TEXT));
}

static int posd_bend_frame_now;

// Whether a frame has anything to show right now.
static int posd_bend_frame_has(int frame, int bent, int xon)
{
    // The graphic only when there is a matrix to draw; the stats whenever the
    // plate is up at all, which is the point of them - "no bend, chain of 2"
    // is a complete answer and one worth having on screen.
    if (frame == PB_FRAME_MATRIX) return bent;
    if (frame == PB_FRAME_STATS)  return bent || xon;
    return 0;
}

// Advance the cycle if it is due, and settle on a frame that exists - the one
// showing may have been switched off since the last pass. Called before the
// repaint test, because the plate turns on a timer rather than on a state
// change: the frame number has to be in the hash or the first frame would sit
// there until something unrelated moved.
static int posd_bend_frame_tick(int bent, int xon)
{
    static int frame_at;
    int t = get_tick_count();
    int i, avail = 0;

    for (i = 0; i < PB_FRAME_N; i++)
        if (posd_bend_frame_has(i, bent, xon)) avail++;

    if (avail == 0) { posd_bend_frame_now = -1; return -1; }

    if (!posd_bend_frame_has(posd_bend_frame_now, bent, xon) ||
        (avail > 1 && (unsigned)(t - frame_at) >= POSD_BEND_MS))
    {
        for (i = 1; i <= PB_FRAME_N; i++)
        {
            int f = (posd_bend_frame_now + i) % PB_FRAME_N;
            if (posd_bend_frame_has(f, bent, xon))
            {
                posd_bend_frame_now = f;
                break;
            }
        }
        frame_at = t;
    }
    return posd_bend_frame_now;
}

static void posd_bend_draw(int force)
{
    static unsigned shown_hash;
    unsigned hash = 2166136261u;
    // Any region, because with a layout on it is perfectly ordinary for the
    // camera's own matrix to be straight through and the interesting one to be
    // in the corner.
    int bent = conf.bitbend_enable && bend_seg_any();
    // The patchbay can only draw one matrix, and the one worth drawing is the
    // one being edited - the plate is read while the strip is being turned.
    // Which region that is gets said in the corner below.
    const bend_t *pb = bend_seg_cur();
    // An experimental profile is not a matrix and has nothing to draw in the
    // patchbay, but it changes the picture just as much - and one of them
    // changes how long the shot takes. So it brings the plate up on its own,
    // and its frame is the whole of it.
#ifdef CAM_BEND_EXPERIMENTAL
    int xon = conf.bendx_enable && bendx_chain_count(&conf.bendx) > 0;
#else
    const int xon = 0;
#endif
    int active = bent || xon;
    int bx = camera_screen.width - POSD_BEND_W - POSD_MARGIN;
    int n = pb->nbits;
    int i, x0, sy, dy, uses_bus = 0;

    if (!active)
    {
        if (posd_bend_was_shown)
        {
            draw_rectangle(bx, POSD_BEND_Y, bx + POSD_BEND_W, POSD_BEND_Y + POSD_BEND_H,
                           MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                           RECT_BORDER0|DRAW_FILLED);
            posd_bend_was_shown = 0;
        }
        return;
    }
    if (n < 1 || n > BEND_MAX_BITS) n = CAM_SENSOR_BITS_PER_PIXEL;
    for (i = 0; i < n; i++)
    {
        unsigned cls = BSRC_CLASS(pb->route[i]);
        hash = (hash ^ pb->route[i]) * 16777619u;
        if (cls == BSRC_C_BUS || cls == BSRC_C_NBUS) uses_bus = 1;
    }
    hash = (hash ^ n) * 16777619u;
    hash = (hash ^ (unsigned)bent) * 16777619u;
    // The stats frame names the loaded preset, and saving one sets that without
    // touching the matrix - so without this the SAVED line would not repaint.
    hash = (hash ^ ((unsigned)(bend_store_cur() + 2) << 20)) * 16777619u;
    hash = (hash ^ ((unsigned)conf.bend_segs.layout << 4)
                 ^  (unsigned)bend_seg_active()) * 16777619u;
#ifdef CAM_BEND_EXPERIMENTAL
    if (xon)
    {
        int k, kn = bendx_chain_count(&conf.bendx);
        hash = (hash ^ (unsigned)kn) * 16777619u;
        for (k = 0; k < kn; k++)
            hash = (hash ^ ((unsigned)conf.bendx.item[k].kind << 8)
                         ^  (unsigned)conf.bendx.item[k].amount) * 16777619u;
    }
#endif
    // The info line turns on a timer of its own, so what it would say has to be
    // part of "has anything changed" - otherwise the first page would sit there
    // until an unrelated state change repainted the plate.
    // The frame turns on a timer rather than on a state change, so which one is
    // showing is part of "has anything changed".
    hash = (hash ^ ((unsigned)(posd_bend_frame_tick(bent, xon) + 2) << 12))
                 * 16777619u;

    if (!force && posd_bend_was_shown && hash == shown_hash) return;

    {
        color bg = posd_background_color();
        draw_rectangle(bx, POSD_BEND_Y, bx + POSD_BEND_W,
                       POSD_BEND_Y + POSD_BEND_H,
                       MAKE_COLOR(bg, bg), RECT_BORDER0|DRAW_FILLED);
    }
    x0 = bx + (POSD_BEND_W - n * POSD_BEND_CELL_W) / 2;
    // Both rows of the graphic are measured from POSD_BEND_GFX_Y, which is the
    // heading's height - everything below is the picture. The sockets are drawn
    // from dy - 1 to dy + 5, which is what fixes the bottom margin here.
    sy = POSD_BEND_GFX_Y + 3;
    dy = POSD_BEND_GFX_Y + POSD_BEND_GFX_H - 7;

    // Not the patchbay's turn: the graphic is out of the way and the three
    // questions have the plate. Also the whole of it when there is no matrix
    // at all - a chain on its own used to draw ten grey traces saying nothing,
    // with the fault's name squeezed over them.
    if (posd_bend_frame_now != PB_FRAME_MATRIX)
    {
        posd_bend_stats_frame(bx, posd_background_color(), xon);
        shown_hash = hash;
        posd_bend_was_shown = 1;
        return;
    }

    // The heading: the name of the bend this frame is a picture of.
    posd_bend_name_row(bx, posd_background_color(), bent);

    // GND and VCC remain as edge rails, with their traces anchored at the
    // vertical centre rather than appearing to emerge from the top or bottom.
    // BUS lives in the free gutter left of the pin row, so it never sits on top
    // of the traces it feeds.
    {
        int my = (sy + dy) / 2;
        draw_rectangle(bx + 2, sy + 4, bx + 5, dy,
                       MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)),
                       RECT_BORDER1|DRAW_FILLED);
        if (uses_bus)
        {
            draw_rectangle(bx + 8, sy-1, bx + 12, sy+3,
                           MAKE_COLOR(theme_color(TC_SPECIAL), theme_color(TC_LIGHT)),
                           RECT_BORDER1|DRAW_FILLED);
        }
        draw_rectangle(bx + POSD_BEND_W - 5, sy + 4,
                       bx + POSD_BEND_W - 2, dy,
                       MAKE_COLOR(theme_color(TC_WARN), theme_color(TC_LIGHT)),
                       RECT_BORDER1|DRAW_FILLED);
    }

    // Traces first, so the pin nodes land cleanly on top of their endpoints.
    for (i = 0; i < n; i++)
    {
        int pin = n - 1 - i;
        unsigned char src = pb->route[pin];
        unsigned cls = BSRC_CLASS(src);
        color c = posd_bend_source_color(src);
        int dx = x0 + i * POSD_BEND_CELL_W + POSD_BEND_CELL_W/2;
        int sx;
        int source_y = sy + 3;

        if (src == BSRC_LOW) {
            sx = bx + 4;
            source_y = (sy + dy) / 2;
        }
        else if (src == BSRC_HIGH) {
            sx = bx + POSD_BEND_W - 4;
            source_y = (sy + dy) / 2;
        }
        else if (cls == BSRC_C_DATA || cls == BSRC_C_NDATA)
            sx = x0 + (n - 1 - (BSRC_IDX(src) % n)) * POSD_BEND_CELL_W
               + POSD_BEND_CELL_W/2;
        else {
            sx = bx + 10;
            source_y = sy + 1;
        }

        // Identity routes are context, not the bend: keep them grey so crossed
        // or rail-fed traces are what the eye sees first.
        if (src == BSRC_DATA(pin)) c = theme_color(TC_DIM);
        draw_line(sx, source_y, dx, dy - 2, MAKE_COLOR(c, c));
        if (BSRC_CLASS(src) == BSRC_C_NDATA || BSRC_CLASS(src) == BSRC_C_NBUS)
        {
            int mx = (sx + dx) / 2;
            int my = (source_y + dy) / 2;
            draw_line(mx - 2, my - 2, mx + 2, my + 2,
                      MAKE_COLOR(theme_color(TC_LIGHT), theme_color(TC_LIGHT)));
            draw_line(mx - 2, my + 2, mx + 2, my - 2,
                      MAKE_COLOR(theme_color(TC_LIGHT), theme_color(TC_LIGHT)));
        }
    }

    // Source and destination sockets. Destination fill repeats its routed
    // signal colour, while the white border keeps GND sockets visible.
    for (i = 0; i < n; i++)
    {
        int pin = n - 1 - i;
        unsigned char src = pb->route[pin];
        color c = posd_bend_source_color(src);
        int x = x0 + i * POSD_BEND_CELL_W + 2;
        draw_rectangle(x, sy, x + 5, sy + 5,
                       MAKE_COLOR(posd_bend_source_color(BSRC_DATA(pin)),
                                  theme_color(TC_DIM)),
                       RECT_BORDER1|DRAW_FILLED);
        draw_rectangle(x, dy - 1, x + 5, dy + 5,
                       MAKE_COLOR(c, (src == BSRC_LOW) ? theme_color(TC_DIM)
                                                       : theme_color(TC_LIGHT)),
                       RECT_BORDER1|DRAW_FILLED);
    }

    // Both engines on. There is no room in a 36 pixel plate for the patchbay
    // and a name, so the profile gets the same treatment the bus gutter gets:
    // a marker that says something else is patched in, in the colour the
    // profile is named in everywhere else.
#ifdef CAM_BEND_EXPERIMENTAL
    if (xon)
        draw_rectangle(bx + 2, POSD_BEND_GFX_Y + 1, bx + 6, POSD_BEND_GFX_Y + 5,
                       MAKE_COLOR(POSD_C_BEND, theme_color(TC_LIGHT)),
                       RECT_BORDER1|DRAW_FILLED);
#endif

    shown_hash = hash;
    posd_bend_was_shown = 1;
}

//-------------------------------------------------------------------
// Multiple exposure counter - "1/2" while a sequence is part way through.
//
// Top centre, in the gap between the histogram box on the left and the bend
// plate on the right, both of which occupy the same band of rows. It is the
// one place on this screen wide enough for it that nothing else claims.
//
// Its own element rather than a row of a plate: it is not a camera reading, it
// is the state of an operation that spans several shots, and it has to be
// visible on a screen where the plates may be hidden and unmissable when it is
// - a sequence you have forgotten you started is a ruined next photograph.
//
// Repaints on the same terms as the plate rows: only when the text changes, or
// on the force/repair pass. See the note above about why that matters.

// "1/2", and "9/9!" at its widest. The word that used to be in front of it
// said nothing the numbers did not - a fraction at the top of the shooting
// screen while a sequence is running is not ambiguous - and it cost half the
// field's width in a gap that is only wide enough because the two boxes either
// side of it leave it that.
#define POSD_MEXP_CHARS 5
#define POSD_MEXP_W     (POSD_MEXP_CHARS * FONT_WIDTH)

static char posd_mexp_shown[POSD_MEXP_CHARS + 1];

static void posd_mexp_draw(int force)
{
    char buf[POSD_MEXP_CHARS + 1];
    int x = (camera_screen.width - POSD_MEXP_W) / 2;
    int y = POSD_BEND_Y + (POSD_BEND_H - FONT_HEIGHT) / 2;

    // Up for the whole sequence, including before the first frame of it.
    //
    // The number is the exposure you are about to take, not the count already
    // taken. "EXP 1/2" before the first press and "EXP 2/2" before the second
    // is what the finder should say, because the question being asked of it is
    // always "which one am I on" - and the old reading answered it with a 1
    // that only appeared once the 1 was already spent. It also means the
    // indicator is on screen before the sequence starts, which is the moment
    // it matters most: multiple exposure left switched on from yesterday is a
    // ruined photograph, and nothing said so until the shutter had gone.
    //
    // mexp_target rather than conf.mexp_frames while a sequence is running.
    // The count a sequence was begun with is fixed at mexp_begin(); changing
    // the menu mid-sequence must not make the display disagree with what the
    // combine is actually going to do.
    if (conf.mexp_enable)
    {
        int n = mexp_in_progress() ? mexp_target : conf.mexp_frames;
        int i = mexp_in_progress() ? mexp_count + 1 : 1;

        if (i > n) i = n;

        // The trailing "!" means the sequence is running but there is no ghost
        // tile to paint - see mexp_ghost_have(). It costs one character and it
        // is the difference between two faults that look the same on the
        // camera and live in different files. With the ghost switched off in
        // the menu there is nothing to report, and neither is there before the
        // first frame, when there is nothing yet to have a ghost of.
        sprintf(buf, "%d/%d%s", i, n,
                (mexp_in_progress() && conf.mexp_ghost && !mexp_ghost_have())
                    ? "!" : "");
    }
    else
        buf[0] = 0;

    if (!force && strcmp(buf, posd_mexp_shown) == 0) return;

    if (buf[0] == 0)
    {
        // Gone. Clear the field rather than leaving the last count on a camera
        // that is no longer counting.
        if (posd_mexp_shown[0])
        {
            color bg = posd_background_color();
            draw_rectangle(x, y, x + POSD_MEXP_W, y + FONT_HEIGHT,
                           MAKE_COLOR(bg, bg), RECT_BORDER0|DRAW_FILLED);
        }
    }
    else
    {
        // Drawn with a background rather than over the live view: this sits in
        // the middle of the frame being composed, and a count that is only
        // legible against a dark subject is a count you cannot rely on.
        draw_string_justified(x, y, buf,
                              MAKE_COLOR(posd_background_color(), POSD_C_MEXP),
                              0, POSD_MEXP_W, TEXT_CENTER);
    }

    strcpy(posd_mexp_shown, buf);
}

// Repainting the plates unconditionally every pass is what made this flicker on
// the a480, and it is worth being precise about why, because the obvious
// explanation is the wrong one.
//
// It is not Canon repainting over us. The plates are erased and redrawn 25
// times a second, each repaint blanks ~8000 pixels per plate and then draws the
// text back, and the display is scanned out of that same buffer the whole time
// with no synchronisation. So a few frames a second are caught in the gap
// between "background painted" and "text painted" and show a plate with missing
// or partial text. That is the flash, and it is entirely self-inflicted: the
// stock OSD never showed it because its elements are small enough that the same
// gap is sub-millisecond, and bend mode's pin strip never showed it because
// bm_draw() has always hashed its state and skipped the repaint when nothing
// moved.
//
// So: compare against what is actually on screen and repaint only the rows that
// differ. In the steady state that is one row a second, when the clock ticks.
// A full repaint happens only on the transitions where the screen really has
// been erased under us.

//-------------------------------------------------------------------
// Boot credit.
//
// Timed from the first frame the overlay actually draws, not from gui_init().
// gui_init() runs while Canon's boot screen is still up, so a counter started
// there spends itself before the shooting screen exists and the line is gone by
// the time anyone could read it - which is exactly what the first version did.
// posd_plates_shown is the signal: it goes 1 on the pass where the plates first
// reach the screen, and that is the first frame the credit could share with the
// overlay it is supposed to appear over.
//
// Redrawn every pass while it is up, rather than once. No other element of the
// overlay claims these rows, but the grid does paint across them, and Canon
// repaints the bitmap on its own schedule - a single draw survives neither.
// Three draw calls per pass for two seconds and then nothing at all, which
// stays inside the "no Canon call at frame rate" rule the rest of this overlay
// is built on - draw_rectangle and draw_string are buffer writes.
//
// Timed in ticks rather than passes because the pass rate is not a constant:
// spytask runs this at 50Hz in the CHDK menu and as low as 1Hz on the shooting
// screen, so a pass count would have made the duration depend on which screen
// the camera happened to come up on.
//-------------------------------------------------------------------

#define CREDIT_L1       "CHDK_rewired"
#define CREDIT_L2       "@rewired_optics"
#define CREDIT_MS       2000
#define CREDIT_LINES    2

// Two thirds down the frame, less 20px because two thirds sat too low to read
// as a title - so 140 rather than 160 on a 240px screen. Nothing else in the
// overlay lives there: the histogram stops at 36, the bend strip at 20, and the
// plates do not start until height-POSD_H (212). It is inside the grid, but the
// grid is drawn earlier in gui_redraw() and this paints over it - see the call
// site.
#define CREDIT_Y        (((camera_screen.height * 2) / 3) - 20)
#define CREDIT_H        (CREDIT_LINES * FONT_HEIGHT)

static int credit_start = 0;    // tick the overlay first drew, 0 = not yet
static int credit_done  = 0;

static void posd_credit_draw(void)
{
    const char *l1 = CREDIT_L1;
    const char *l2 = CREDIT_L2;
    int w1, w2, tw, tx, t;

    // Once per boot. A trip through playback and back does not bring it back.
    if (credit_done) return;

    // Nothing to time against until the overlay is on screen to appear over.
    if (!posd_plates_shown) return;

    t = get_tick_count();
    if (!credit_start) credit_start = t;

    if ((t - credit_start) >= CREDIT_MS)
    {
        credit_done = 1;

        // Clearing the strip is all that is needed here. These rows belong to
        // nothing else, so there is no owner to hand them back to and no
        // repaint to force. Transparent, so the live view comes through - and
        // the grid, which is redrawn from its own call site every pass.
        draw_rectangle(0, CREDIT_Y - 4,
                       camera_screen.width - 1, CREDIT_Y + CREDIT_H + 4,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0|DRAW_FILLED);
        return;
    }

    // One plate sized to the longer line, with both lines centred inside it, so
    // the block reads as one object rather than two stacked labels.
    //
    // White rather than the cyan this started as. COLOR_WHITE is 0x11, the
    // lightest byte in the record palette on all three ports - there is no
    // paler cyan to pick, because CHDK exposes exactly one (0xDD, an alias of
    // COLOR_BLUE_LT) and 20 entries in total.
    w1 = (int)strlen(l1) * FONT_WIDTH;
    w2 = (int)strlen(l2) * FONT_WIDTH;
    tw = (w1 > w2) ? w1 : w2;

    tx = (camera_screen.width - tw) / 2;
    if (tx < 4) tx = 4;

    draw_rectangle(tx-4, CREDIT_Y-4, tx+tw+4, CREDIT_Y+CREDIT_H+3,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK),
                   RECT_BORDER0|DRAW_FILLED|RECT_ROUND_CORNERS);

    draw_string(tx + (tw - w1) / 2, CREDIT_Y,
                l1, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    draw_string(tx + (tw - w2) / 2, CREDIT_Y + FONT_HEIGHT,
                l2, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
}

static void gui_draw_persistent_osd(int force)
{
    static int slow_tick = 0, fast_tick = 0;
    static int repair_tick = 0;
    static int primed = 0;
    int i, j, y, t, hide, full, repair, sampled = 0;

    y = camera_screen.height - POSD_H - POSD_MARGIN;

#ifdef CAM_SUPPRESS_LOW_BATTERY_ICON
    // Retry until Canon's display object exists, then leave its independent
    // voltage monitoring and emergency shutdown behavior alone.
    {
        static int low_batt_icon_suppressed;
        if (!low_batt_icon_suppressed)
            low_batt_icon_suppressed = camera_suppress_low_battery_icon();
    }
#endif

    // The overlay belongs to the plain shooting screen and nothing else. Canon's
    // menus, playback, review, <ALT> and the CHDK menus all get the screen to
    // themselves - the plates take up a lot of room and there is nothing worth
    // monitoring on those screens. Also gets out of the way while the shutter is
    // pressed so the overlay never covers the shot being framed.
    //
    // Note this is decided here rather than at the call site: the caller has to
    // keep calling us on the screens where we are hidden, otherwise the erase
    // below never runs and the plates are stranded on top of whatever opened.
    // With the refresh gate held there is no repaint that would clean them up.
    hide = !posd_screen_active();
    if (hide)
    {
        if (posd_plates_shown)
        {
            posd_clear(y);
            posd_plates_shown = 0;
        }
        return;
    }
    // A full repaint is needed after a hidden state or draw_restore(). There is
    // deliberately no plate backing to probe now; the periodic text repair
    // below restores any characters Canon overwrites.
    full = !posd_plates_shown || force;
    posd_plates_shown = 1;

    // A theme change repaints everything once. Every element here decides for
    // itself whether it has changed - the plate rows compare their text, the
    // histogram its bars, the bend plate its hash - and none of those tests
    // includes the colour the thing is drawn in, so switching theme used to
    // leave whatever had not otherwise moved sitting there in the old colours
    // until the next boot.
    {
        static int drawn_theme = -1;
        if (drawn_theme != conf.theme)
        {
            drawn_theme = conf.theme;
            full = 1;
        }
    }

    // Resample on the timers only. Everything below this point is buffer writes.
    t = get_tick_count();
    repair = !repair_tick || ((t - repair_tick) >= POSD_REPAIR_MS);
    if (repair) repair_tick = t;
    if (!primed || ((t - fast_tick) >= POSD_FAST_MS))
    {
        int slow = (!primed || ((t - slow_tick) >= POSD_SLOW_MS));
        if (slow) slow_tick = t;
        fast_tick = t;
        primed = 1;
        sampled = 1;
        posd_sample(slow);
        if (conf.show_histo) posd_histo_sample();
    }

    // Repaint only what moved. Clear the old backing on a full redraw, but do
    // not replace it: the overlay is text directly over the live view.
    if (full)
    {
        for (i=0; i<POSD_PLATES; i++)
            posd_clear_backing(posd_plate_x(i), y, y+POSD_H);
    }

    for (i=0; i<POSD_PLATES; i++)
    {
        int bx = posd_plate_x(i);
        for (j=0; j<POSD_ROWS; j++)
            if (full || posd_row_changed(i, j))
                posd_row_draw(i, j, bx, y);
            else if (repair)
                posd_row_repair(i, j, bx, y);
    }


    // Live histogram, from posd_histo_sample() above - no module involved.
    //
    // Repainted only when the bars actually move, like the plate rows, so a
    // static scene costs nothing. It is a small enough region that repainting it
    // when it does change is nothing like the full-plate repaint that made the
    // a480 tear.
    if (conf.show_histo && posd_histo_valid && (full || posd_histo_changed()))
        posd_histo_draw();

    posd_bend_draw(full || repair);

    posd_mexp_draw(full || repair);

    // Warnings that used to be drawn straight from spytask with the old method,
    // which on the shooting screen meant they were painted at whatever rate the
    // loop happened to be running at. Drawn here instead, with everything else.
    {
        extern int no_modules_flag;
        if (no_modules_flag == 1)
        {
            const char *msg = lang_str(LANG_ERROR_MISSING_MODULES);
            int mw = (int)strlen(msg) * FONT_WIDTH;
            int mx = (camera_screen.width - mw) / 2;
            if (mx < 0) mx = 0;
            draw_string(mx, y - FONT_HEIGHT - 4, msg,
                        MAKE_COLOR(posd_background_color(), POSD_C_WARN));
        }
    }
}
#endif

static void gui_handle_splash(int force_redraw)
{
    if (gui_splash)
    {
        if (camera_info.state.gui_mode_none || camera_info.state.gui_mode_alt)
            if (force_redraw || (gui_splash == SPLASH_TIME))
                gui_draw_splash();

        // on half shoot or zoom, cancel splash screen
        if(kbd_is_key_pressed(KEY_SHOOT_HALF) || kbd_is_key_pressed(KEY_ZOOM_IN) || kbd_is_key_pressed(KEY_ZOOM_OUT)) {
            gui_splash = 1;
        }
        if (--gui_splash == 0)
        {
#ifdef CAM_PERSISTENT_OSD
            // Unconditional. This used to sit inside the gui_mode test below, but
            // gui_mode_none measures 0 on this camera's shooting screen, so the
            // erase never ran and the version line stayed on screen for good.
            gui_erase_splash();
#endif
            if (camera_info.state.gui_mode_none || camera_info.state.gui_mode_alt)
            {
                gui_set_need_restore();
            }
            if (logo)
                free(logo);
            logo = NULL;
        }
    }
}

//-------------------------------------------------------------------
// Dummy for startup to avoid null gui_mode pointer
static gui_handler startupGuiHandler = { GUI_MODE_STARTUP, 0, 0, 0, 0, GUI_MODE_FLAG_NODRAWRESTORE | GUI_MODE_FLAG_NORESTORE_ON_SWITCH };

static gui_handler *gui_mode = &startupGuiHandler;  // current gui mode. pointer to gui_handler structure

static int gui_osd_need_restore = 0;    // Set when screen needs to be erase and redrawn
static int gui_mode_need_redraw = 0;    // Set if current mode needs to redraw itself

//-------------------------------------------------------------------

void gui_set_need_restore()
{
    gui_osd_need_restore = 1;
}

void gui_cancel_need_restore()
{
    gui_osd_need_restore = 0;
    gui_mode_need_redraw = 0;
}

void gui_set_need_redraw()
{
    gui_mode_need_redraw = 1;
}

//-------------------------------------------------------------------
void gui_init()
{
    gui_set_mode(&defaultGuiHandler);
#ifdef CAM_CUSTOM_SOUNDS
    extern int platform_load_custom_sounds(void);
    platform_load_custom_sounds();
#endif
    // CHDK's own start sound. Suppressed on a body whose ROM plays its own
    // My Camera startup sound, because there it would double up - which is
    // why this used to be gated on CAM_CUSTOM_SOUNDS alone.
    //
    // That gate was wrong: it assumed every camera with usable My Camera slots
    // also plays a startup sound from them. The A470 and A460 do not, so
    // turning their sound slots on silently took their boot sound away.
    // Whether the ROM covers it is a property of the body, so the body says.
#if !defined(CAM_CUSTOM_SOUNDS) || defined(CAM_CHDK_START_SOUND)
    if (conf.start_sound > 0)
    {
        play_sound(4);
    }
#endif

    init_splash();

    draw_init();

    process_file( "A/CHDK/badpixel", make_pixel_list, 1 );
    process_file( "A/CHDK/badpixel.txt", make_pixel_list, 1 );
}

//-------------------------------------------------------------------
// Set new GUI mode, returns old mode
#ifdef CAM_OSD_FORCE_DRAW_IN_SPYTASK
int dbg_mode_changes = 0;   // counted per second in spytask
int dbg_loops = 0;          // raw spytask loop iterations, counted before any gate
#endif

gui_handler* gui_set_mode(gui_handler *mode)
{
#ifdef CAM_OSD_FORCE_DRAW_IN_SPYTASK
    if (camera_info.state.gui_mode != mode->mode)
        dbg_mode_changes++;
#endif
    // Set up gui mode & state variables
    camera_info.state.gui_mode = mode->mode;
    camera_info.state.gui_mode_none = (camera_info.state.gui_mode == GUI_MODE_NONE);
    camera_info.state.gui_mode_alt = (camera_info.state.gui_mode == GUI_MODE_ALT);

    if ( gui_mode == mode )
        return gui_mode;

#ifdef CAM_TOUCHSCREEN_UI
    if (((gui_mode->mode == GUI_MODE_NONE) != (mode->mode == GUI_MODE_NONE)) || // Change from GUI_MODE_NONE to any other or vice-versa
        ((gui_mode->mode >  GUI_MODE_MENU) != (mode->mode >  GUI_MODE_MENU)))   // Switch in & out of menu mode
        redraw_buttons = 1;
#endif

    gui_handler *old_mode = gui_mode;
    gui_mode = mode;

    gui_osd_need_restore = 0;

    // Flag for screen erase/redraw unless mode is marked not to (e.g. menu box popup)
    if (((gui_mode->flags & (GUI_MODE_FLAG_NODRAWRESTORE|GUI_MODE_FLAG_NORESTORE_ON_SWITCH)) == 0) &&
        ((old_mode->flags & GUI_MODE_FLAG_NORESTORE_ON_SWITCH) == 0))
        gui_set_need_restore();
    // If old mode did not erase screen on exit then force current mode to redraw itself (e.g. exit menu popup back to file select)
    if ((old_mode->flags & (GUI_MODE_FLAG_NORESTORE_ON_SWITCH)) != 0)
        gui_set_need_redraw();

#ifdef CAM_DISP_ALT_TEXT
    if (camera_info.state.gui_mode_alt)
        gui_reset_alt_helper();
#endif

    return old_mode;
}

//-------------------------------------------------------------------

#ifdef CAM_DISP_ALT_TEXT

static int is_menu_shortcut = 0;

static char* gui_shortcut_text(int button)
{
    switch (button)
    {
    case KEY_DISPLAY:
        return CAM_DISP_BUTTON_NAME;
    case KEY_UP:
        return "UP";
    case KEY_DOWN:
        return "DOWN";
    case KEY_LEFT:
        return "LEFT";
    case KEY_RIGHT:
        return "RIGHT";
    case KEY_ERASE:
        return "ERASE";
    case KEY_MENU:
        is_menu_shortcut = 1;
        return "MENU*";
    case KEY_VIDEO:
        return "VIDEO";
    default:
        return "?";
    }
}

static int shortcut_text(int x, int y, int button, int func_str, const char *state, twoColors col)
{
    buf[0] = 0;
    if (state)
    {
        sprintf(buf,"%-5s %20s",gui_shortcut_text(button),lang_str(func_str));
        buf[26] = 0;
        sprintf(buf+strlen(buf)," [%6s",state);
        buf[34] = 0;
        strcat(buf,"]");
    }
    else if (button)
    {
        sprintf(buf,"%-5s %29s",gui_shortcut_text(button),lang_str(func_str));
    }
    else
    {
        sprintf(buf,"%-35s",lang_str(func_str));
    }
    buf[35] = 0;
    draw_string(x, y, buf, col);
    return y + FONT_HEIGHT;
}

static int gui_helper_displayat = 0;

void gui_reset_alt_helper()
{
    gui_helper_displayat = get_tick_count() + (conf.show_alt_helper_delay * 1000);
}

static void gui_draw_alt_helper()
{
    if ((camera_info.state.state_kbd_script_run != 0) || (console_displayed != 0))
    {
        if (gui_helper_displayat <= get_tick_count())
            gui_set_need_restore();
        gui_reset_alt_helper();
    }

    if ((conf.show_alt_helper == 0) || (gui_helper_displayat > get_tick_count()))
    {
        gui_draw_osd();
        return;
    }

    is_menu_shortcut = 0;

    int y = FONT_HEIGHT;
    int x = ((camera_screen.width/2)-(FONT_WIDTH*35/2));

    twoColors col = user_color(conf.menu_color);
    twoColors hdr_col = user_color(conf.menu_title_color);

    sprintf(buf,lang_str(LANG_HELP_HEADER),
            lang_str(LANG_HELP_ALT_SHORTCUTS),
            (conf.user_menu_enable && conf.user_menu_as_root)?lang_str(LANG_HELP_USER_MENU):lang_str(LANG_HELP_CHDK_MENU)); 
    buf[35] = 0;
    draw_string(x, y, buf, hdr_col);
    y += FONT_HEIGHT;

    if (conf.user_menu_enable)
    {
        sprintf(buf,lang_str(LANG_HELP_HEADER),
                lang_str(LANG_HELP_HALF_PRESS),
                (conf.user_menu_enable && conf.user_menu_as_root)?lang_str(LANG_HELP_CHDK_MENU):lang_str(LANG_HELP_USER_MENU)); 
        buf[35] = 0;
        draw_string(x, y, buf, col);
        y += FONT_HEIGHT;
    }

    draw_string(x, y, lang_str(LANG_HELP_SCRIPTS), col);
    y += FONT_HEIGHT;

#if !defined(CAM_HAS_MANUAL_FOCUS) && defined(SHORTCUT_MF_TOGGLE)
    y = shortcut_text(x, y, SHORTCUT_MF_TOGGLE,LANG_HELP_MANUAL_FOCUS,gui_on_off_enum(0,&conf.subj_dist_override_koef), col);
#endif

    if (shooting_get_common_focus_mode())           // Check in manual focus mode
    {
        sprintf(buf,lang_str(LANG_HELP_FOCUS),gui_shortcut_text(SHORTCUT_SET_INFINITY),gui_shortcut_text(SHORTCUT_SET_HYPERFOCAL));
        draw_string(x, y, buf, col);
        y += FONT_HEIGHT;
    }

#if !CAM_HAS_ERASE_BUTTON
#ifdef OPT_DEBUGGING
    if (conf.debug_shortcut_action)
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_RAW,LANG_MENU_DEBUG_SHORTCUT_ACTION,gui_debug_shortcut_modes[conf.debug_shortcut_action], col);
    else
#endif
    if (shooting_get_common_focus_mode())           // Check in manual focus mode
    {
#if CAM_HAS_ZOOM_LEVER
        if (SHORTCUT_TOGGLE_RAW != SHORTCUT_SET_INFINITY)
            y = shortcut_text(x, y, SHORTCUT_TOGGLE_RAW, LANG_HELP_INF_FOCUS, 0, col);
#else
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_RAW, LANG_HELP_CHG_FOCUS_FACTOR, 0, col);
#endif
    }
    else
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_RAW,LANG_MENU_RAW_SAVE, gui_raw_type_string(), col);
#else
#ifdef OPT_DEBUGGING
    if (conf.debug_shortcut_action)
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_RAW,LANG_MENU_DEBUG_SHORTCUT_ACTION,gui_debug_shortcut_modes[conf.debug_shortcut_action], col);
    else
#endif
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_RAW,LANG_MENU_RAW_SAVE, gui_raw_type_string(), col);
#endif

    y = shortcut_text(x, y, 0 ,LANG_HELP_HALF_PRESS, 0, hdr_col);

    if ( conf.enable_shortcuts)
    {
        y = shortcut_text(x, y, SHORTCUT_DISABLE_OVERRIDES,LANG_MENU_OVERRIDE_DISABLE,gui_override_disable_modes[conf.override_disable], col);
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_HISTO,LANG_MENU_HISTO_SHOW,gui_histo_show_modes[conf.show_histo], col);
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_ZEBRA,LANG_MENU_ZEBRA_DRAW,gui_zebra_draw_modes[conf.zebra_draw], col);
        y = shortcut_text(x, y, SHORTCUT_TOGGLE_OSD,LANG_MENU_OSD_SHOW,gui_on_off_enum(0,&conf.show_osd), col);
    }
    else
    {
        y = shortcut_text(x, y, 0,LANG_HELP_SHORTCUTS_DISABLED, 0, col);
    }

    if (conf.hide_osd == 0)
        y = shortcut_text(x, y, KEY_DISPLAY, LANG_HELP_HIDE_OSD, 0, col);

    if (is_menu_shortcut)
        y = shortcut_text(x, y, 0 ,LANG_HELP_NOT_ALT, 0, col);
}

#endif

//-------------------------------------------------------------------

void gui_chdk_draw(int force_redraw)
{
#ifdef CAM_DISP_ALT_TEXT
    gui_draw_alt_helper();
#else
    gui_draw_osd();
#endif

    if (camera_info.state.osd_title_line) 
    {
        int w = camera_screen.disp_width;
#ifdef CAM_DISP_ALT_TEXT
        script_get_alt_text(buf);
        w = draw_string_justified(camera_screen.disp_left, camera_screen.height-FONT_HEIGHT,
                                  buf, MAKE_COLOR(COLOR_RED, COLOR_WHITE), 0, w, TEXT_CENTER) - camera_screen.disp_left;
#endif

        if ((camera_info.state.mode_rec || camera_info.state.mode_play) && (camera_info.state.osd_title_line & 1))
        {
#ifdef CAM_PERSISTENT_OSD
            // Bottom right, on a solid plate. The <ALT> text is centred on this
            // same line, so centring the title put the two on top of each other;
            // and drawn unfilled at bottom left it flickered against live view.
            {
                int tw = (int)strlen(script_title) * FONT_WIDTH;
                int tx = camera_screen.width - tw - 4;
                if (tx < 0) tx = 0;
                draw_string_clipped(tx, camera_screen.height-FONT_HEIGHT, script_title,
                                    MAKE_COLOR(COLOR_BLACK, COLOR_YELLOW), tw);
            }
#else
            // Draw script title (up to <ALT> text, if shown)
            draw_string_clipped(camera_screen.disp_left, camera_screen.height-FONT_HEIGHT, script_title, user_color(conf.menu_color), w);
#endif
        }
    }

    if (conf.raw_ev_histo_enable & 1)
    {
        librawevhisto->draw(force_redraw);
    }
    else
    {
        librawevhisto->erase();
    }

    console_draw(force_redraw);
}

//-------------------------------------------------------------------
static void gui_debug_shortcut(void) 
{
#ifdef OPT_DEBUGGING
    static int lastcall = -1;
    int t=get_tick_count();
    if ( lastcall != -1) {
        if (t-lastcall <= 400)
            debug_display_direction = -debug_display_direction;
    }
    lastcall=t;
    switch(conf.debug_shortcut_action) {
            case 1:
                {
                extern void schedule_memdump(int memdmp_delay);
                schedule_memdump(memdmp_delay);
                }
                break;
            case 2:
                gui_update_debug_page();
                break;
            case 3:
                gui_compare_props(0); // compare properties
                break;
            case 4:
                gui_compare_props(1); // compare UI properties
                break;
    }
#endif
}

//-------------------------------------------------------------------
// Handler for Menu button press default - enter Menu mode
void gui_default_kbd_process_menu_btn()
{
    gui_set_mode(&menuGuiHandler);
}

// Change SD override factor, direction = 1 to increase, -1 to decrease
// Only applies if camera has a Zoom lever
#if CAM_HAS_ZOOM_LEVER
static void sd_override_koef(int direction)
{
    if (direction > 0)
    {
        if (conf.subj_dist_override_koef==SD_OVERRIDE_OFF)
        {
            conf.subj_dist_override_koef = SD_OVERRIDE_ON;
            menu_set_increment_factor(1);
        }
        else if (menu_get_increment_factor() < menu_calc_max_increment_factor(CAMERA_MAX_DIST))
        {
            menu_set_increment_factor(menu_get_increment_factor() * 10);
        }
        else
        {
            conf.subj_dist_override_koef = SD_OVERRIDE_INFINITY;
        }
    }
    else if (direction < 0)
    {
        if (conf.subj_dist_override_koef==SD_OVERRIDE_INFINITY)
        {
            conf.subj_dist_override_koef = SD_OVERRIDE_ON;
            menu_set_increment_factor(menu_calc_max_increment_factor(CAMERA_MAX_DIST));
        }
        else if (menu_get_increment_factor() > 1)
        {
            menu_set_increment_factor(menu_get_increment_factor() / 10);
        }
        else
        {
            conf.subj_dist_override_koef = SD_OVERRIDE_OFF;
        }
    }
    shooting_set_focus(shooting_get_subject_distance_override_value(), SET_NOW);
}
#endif

// Change SD override by factor amount, direction = 1 to increase (zoom in), -1 to decrease (zoom out)
static void sd_override(int direction)
{
    if (conf.subj_dist_override_koef == SD_OVERRIDE_ON)
    {
        gui_subj_dist_override_value_enum(direction*menu_get_increment_factor(),0);
        shooting_set_focus(shooting_get_subject_distance_override_value(),SET_NOW);
    }
}

static int alt_mode_script_run()
{
    int remote_script_start_ready = 0;

    // Start the current script if script_start is enabled, we are in <ALT> mode and there is a pulse longer than 100mSec on USB port
    if (conf.remote_enable && conf.remote_enable_scripts && get_usb_power(SINGLE_PULSE) > 5)
        remote_script_start_ready=1;

    // Start a script if the shutter button pressed in <ALT> mode (kdb_blocked) or USB remote sequence not running
    //  or if script start on <ALT> enabled and this is the first pass through <ALT> mode
    if ( kbd_is_key_clicked(KEY_SHOOT_FULL) || remote_script_start_ready || script_run_on_alt_flag ) 
    {
        script_run_on_alt_flag = 0 ;
        script_start_gui(0);
        return 1;
    }

    return 0;
}

// Main button processing for CHDK Alt mode (not in MENU mode)
// This needs to be cleaned up, re-organised and commented !!!!
int gui_chdk_kbd_process()
{
    if (alt_mode_script_run()) return 0;

    // Process Shutter Half Press + BUTTON shortcuts
    gui_kbd_shortcuts();
    if (camera_info.state.is_shutter_half_press) return 0;

    int reset_helper = 0;

#if !CAM_HAS_ERASE_BUTTON                              // ALT RAW toggle kbd processing if camera has SD override but no erase button
    if (kbd_is_key_clicked(SHORTCUT_TOGGLE_RAW))
    {
        if (conf.debug_shortcut_action > 0)
        {
            gui_debug_shortcut();
        }
        // Check in manual focus mode
        else if (!shooting_get_common_focus_mode())
        {
            // Not manual focus mode so just update RAW save setting
            cb_change_save_raw();
        }
        else
        {
            // In manual focus mode so update shooting distance
#if CAM_HAS_ZOOM_LEVER
            conf.subj_dist_override_value=CAMERA_MAX_DIST;
            shooting_set_focus(shooting_get_subject_distance_override_value(), SET_NOW);
#else
            gui_subj_dist_override_koef_enum(1,0);
#endif
            reset_helper = 1;
        }
    }
#else                                                   // ALT RAW toggle kbd processing if can't SD override or has erase button
    if (kbd_is_key_clicked(SHORTCUT_TOGGLE_RAW))
    {
        if (conf.debug_shortcut_action > 0)
        {
            gui_debug_shortcut();
        }
        else
        {
            // Change RAW save state
            cb_change_save_raw();
        }
    }
#endif
    else if (kbd_is_key_clicked(KEY_SET))
    {
        gui_menu_init(&script_submenu);
        gui_default_kbd_process_menu_btn();
    }
    else
    {
#if !CAM_HAS_MANUAL_FOCUS
        if (kbd_is_key_clicked(SHORTCUT_MF_TOGGLE))     // Camera does not have manual focus
        {
            if (conf.subj_dist_override_koef>SD_OVERRIDE_OFF)
                conf.subj_dist_override_koef=SD_OVERRIDE_OFF;
            else conf.subj_dist_override_koef=SD_OVERRIDE_ON;
            reset_helper = 1;
        }
        else
#endif
        if (shooting_get_common_focus_mode())           // Check in manual focus mode
        {
#if CAM_HAS_ZOOM_LEVER                                  // Camera has zoom lever, use left & right to change factor,up to set infinity
            if (kbd_is_key_clicked(KEY_RIGHT))
            {
                sd_override_koef(1);
                reset_helper = 1;
            }
            else if (kbd_is_key_clicked(KEY_LEFT))
            {
                sd_override_koef(-1);
                reset_helper = 1;
            }
            else if (kbd_is_key_clicked(SHORTCUT_SET_INFINITY))
            {
                conf.subj_dist_override_value=CAMERA_MAX_DIST;
                shooting_set_focus(shooting_get_subject_distance_override_value(), SET_NOW);
                reset_helper = 1;
            }
            else
#endif
            if (kbd_is_key_clicked(SHORTCUT_SET_HYPERFOCAL))    // Set hyperfocal distance if down pressed
            {
                if ((camera_info.state.mode_shooting==MODE_M) || (camera_info.state.mode_shooting==MODE_AV))
                    conf.subj_dist_override_value=(int)shooting_get_hyperfocal_distance_1e3_f(shooting_get_aperture_from_av96(shooting_get_user_av96()),get_focal_length(lens_get_zoom_point()))/1000;
                else conf.subj_dist_override_value=(int)shooting_get_hyperfocal_distance();
                shooting_set_focus(shooting_get_subject_distance_override_value(), SET_NOW);
                reset_helper = 1;
            }
            else
            {
                switch (kbd_get_autoclicked_key())
                {
#if CAM_HAS_ZOOM_LEVER
                case KEY_ZOOM_IN:
#else
                case KEY_RIGHT:
#endif
                    sd_override(1);
                    reset_helper = 1;
                    break;
#if CAM_HAS_ZOOM_LEVER
                case KEY_ZOOM_OUT:
#else
                case KEY_LEFT:
#endif
                    sd_override(-1);
                    reset_helper = 1;
                    break;
                }
            }
        }
    }

    if (reset_helper)
    {
        gui_set_need_restore();
#ifdef CAM_DISP_ALT_TEXT
        gui_reset_alt_helper();
#endif
    }

    return 0;
}

//-------------------------------------------------------------------
// Handler for Menu button press in CHDK Alt mode (not in Menu mode)
// Enter main menu or user menu based on configuration
void gui_chdk_kbd_process_menu_btn()
{
    if (conf.user_menu_enable &&
        ((conf.user_menu_as_root && !camera_info.state.is_shutter_half_press) ||
         (!conf.user_menu_as_root && camera_info.state.is_shutter_half_press)))
        gui_menu_init(&user_submenu);
    else
        gui_menu_init(&root_menu);

    gui_default_kbd_process_menu_btn();
}

//-------------------------------------------------------------------
// GUI handler for <ALT> mode
gui_handler altGuiHandler = { GUI_MODE_ALT, gui_chdk_draw, gui_chdk_kbd_process, gui_chdk_kbd_process_menu_btn, 0, 0, };

//-------------------------------------------------------------------
// Main GUI redraw function, perform common initialisation then calls the redraw handler for the mode
void gui_redraw()
{
    int flag_gui_enforce_redraw = 0;

#ifdef CAM_PERSISTENT_OSD
    // Menus are interactive screens, so Canon's normal idle timeout must not
    // blank them.  This is deliberately temporary: as soon as the last Canon
    // or CHDK menu closes, put the user's normal "Disable LCD off" policy back
    // in charge.  TurnOnBackLight also recovers a menu which was opened at the
    // same instant Canon's timeout fired.
    {
        extern int canon_menu_active;
        static int menu_was_active;
        static int menu_backlight_tick;
        int now = get_tick_count();
        int menu_active = !camera_info.state.gui_mode_none
                       || (canon_menu_active != (int)&canon_menu_active-4)
                       || canon_shoot_menu_active;

        if (menu_active)
        {
            disable_shutdown();
            if (!menu_was_active || (now - menu_backlight_tick) >= 1000)
            {
                TurnOnBackLight();
                menu_backlight_tick = now;
            }
        }
        else if (menu_was_active)
        {
            conf_update_prevent_shutdown();
        }
        menu_was_active = menu_active;
    }
#endif

    // The bend menu's enum items bind to shadow ints, because the bend struct
    // is packed bytes so it can be written to a preset file as-is and menu
    // items bind to int*. MENUITEM_ENUM2 uses its arg slot for the string list
    // and so cannot carry an ARG_CALLBACK, so the enums are pushed from here
    // rather than on edit. Twelve clamps and nine stores - cheap enough to do
    // unconditionally, and it cannot go stale.
    bend_ui_push();

#ifdef CAM_BEND_MODE
    // Bend mode entry/exit requested by the key handler. Done here, in the GUI
    // task, because switching mode erases and repaints - see gui_bend.c.
    gui_bend_activate();

    // A shot wipes the whole bitmap (posd_hide_now) and Canon's review holds
    // it. The plates come back on their own - they repaint whenever they are
    // not hidden - but the patchbay repaints only when its own state changes,
    // and a photograph changes none of it. So the moment the screen is ours
    // again, tell it to draw. Edge-triggered: one repaint, not one per frame.
    //
    // This comment described the mechanism and no code implemented it, which is
    // why the Bend UI stayed off the screen after a review until MENU was
    // pressed - MENU forces a redraw, which is exactly what was missing.
    {
        extern void gui_bend_force_redraw(void);
        static int bend_was_hidden;
        int hidden = posd_shot_hold() || posd_review_on_screen();
        if (bend_was_hidden && !hidden) gui_bend_force_redraw();
        bend_was_hidden = hidden;
    }
#endif

#ifdef CAM_DRAW_RGBA
    // If switched to play mode or opened canon menu then erase CHDK UI in case it does not need to be redrawn
    static int last_canon_menu, last_mode_play;
    int canon_menu = (canon_menu_active != (int)&canon_menu_active-4);
    if ((canon_menu && !last_canon_menu) || (camera_info.state.mode_play && !last_mode_play))
        gui_set_need_restore();
    last_canon_menu = canon_menu;
    last_mode_play = camera_info.state.mode_play;
#endif

    if (!draw_test_guard() && (!camera_info.state.gui_mode_none || gui_splash))     // Attempt to detect screen erase in <Alt> mode, redraw if needed
    {
        draw_set_guard();
        flag_gui_enforce_redraw = 1;
#ifdef CAM_TOUCHSCREEN_UI
        redraw_buttons = 1;
#endif
    }

#ifdef CAM_TOUCHSCREEN_UI
    extern void virtual_buttons();
    virtual_buttons();
#endif

    // Erase screen if needed
    if (gui_osd_need_restore)
    {
        draw_restore();
        gui_osd_need_restore = 0;
        flag_gui_enforce_redraw = 1;
    }

    // Force mode redraw if needed
    if (gui_mode_need_redraw)
    {
        gui_mode_need_redraw = 0;
        flag_gui_enforce_redraw = 1;
    }

#ifdef OPT_EXPIRE_TEST
    do_expire_check();
#endif

    gui_handle_splash(flag_gui_enforce_redraw);

#ifdef CAM_PERSISTENT_OSD
    // Paint full-screen assist images as the first bitmap layer. The multiple
    // exposure ghost is refreshed in slices; drawing those slices from inside
    // the OSD let later slices overwrite unchanged plates and menus. Whenever
    // a background slice changes, force every foreground layer below to paint
    // over it again. This also preserves Bend's mask -> grid -> controls order.
    // Canon menus are excluded because their foreground is not ours to redraw.
    {
        extern int canon_menu_active;
        static int ghost_tick;
        int t = get_tick_count();
        int canon_menu_hidden = (canon_menu_active != (int)&canon_menu_active-4)
                             || canon_shoot_menu_active;

        // The ghost is a shooting-screen aid, not a generic background layer.
        // In particular, Bend owns GUI_MODE_MODULE and draws its segment mask
        // over the same image area; repainting the ghost there makes the two
        // masks fight on every repair tick.
        if (camera_info.state.gui_mode_none &&
            camera_info.state.mode_rec && !camera_info.state.mode_play &&
            !canon_menu_hidden && !kbd_is_key_pressed(KEY_SHOOT_FULL) &&
            !posd_shot_hold() &&
            mexp_in_progress() && mexp_ghost_have() &&
            (flag_gui_enforce_redraw || !ghost_tick || (t - ghost_tick) >= 200))
        {
            mexp_ghost_draw(flag_gui_enforce_redraw);
            ghost_tick = t;
            flag_gui_enforce_redraw = 1;
        }
    }
#endif

#ifdef CAM_PERSISTENT_OSD
    {
    int posd_active = conf.show_osd && posd_screen_active();

    // Hand the refresh gate back before doing any erase or menu drawing.  The
    // old end-of-pass release was visually too late when a menu was entered:
    // its first frame could be drawn while the shooting-screen lock was still
    // held, leaving an apparently dead/black menu until another refresh.
    if (!posd_active)
        posd_canon_osd_end(0);
    posd_canon_osd_begin(posd_active);

    // CAM_PERSISTENT_OSD bypasses gui_draw_osd(), whose first element is the
    // grid module. Draw it only over the shooting screen and Bend mode. The
    // active GUI redraw below then puts Bend controls above it, while ALT and
    // menu modes never receive grid pixels at all.
    {
        extern int canon_menu_active;
        int canon_menu_hidden = (canon_menu_active != (int)&canon_menu_active-4)
                             || canon_shoot_menu_active;
        // posd_shot_hold(): the grid is wiped along with everything else when a
        // shot starts, and must stay off until the review is done. Redrawing it
        // during the hold would put it straight back into the frame Canon is
        // about to freeze, which is the whole bug.
        // posd_active: while Canon owns the bitmap - a flash or macro popup,
        // the FUNC menu - CHDK draws nothing at all.
        //
        // recui_active(): the same thing again for the UI that *replaced*
        // those popups. With CAM_RECUI the screen never goes back to Canon, so
        // posd_active stays true while CHDK's own FUNC menu or an arrow
        // readout is up - and the grid, which is drawn from here on a
        // condition of its own, kept painting straight over them. These grid
        // files carry opaque bands, so it is not a cosmetic overlap: the bands
        // land on top of the menu. Compiles to a constant 0 without CAM_RECUI.
        //
        // This test was missing, and it is what put "the grid and a white box"
        // on screen every time one of those buttons was pressed. The plates
        // erase themselves when the overlay goes inactive, but the grid was
        // drawn from here on a condition of its own that never looked at the
        // overlay, so it kept painting over Canon's popup for the whole
        // handover - and these grid files carry opaque bands, which is the
        // box. The popup underneath never had a clean frame to appear in.
        if (posd_grid_active() && camera_info.state.mode_rec && !canon_menu_hidden &&
            conf.show_grid_lines && camera_info.state.gui_mode_none &&
            !posd_shot_hold() && !recui_active())
        {
            libgrids->gui_grid_draw_osd(flag_gui_enforce_redraw);
        }
        else if (flag_gui_enforce_redraw && camera_info.state.mode_rec &&
                 !canon_menu_hidden && conf.show_grid_lines &&
                 !posd_shot_hold() && !posd_review_on_screen() &&
                 gui_bend_active())
        {
            // One grid paint on Bend entry; its opaque bands are drawn later.
            libgrids->gui_grid_draw_osd(1);
        }
    }

    // Same call site as the splash. This is where it was in the build that
    // worked - do not move it without testing that single change on its own.
    //
    // Called unconditionally apart from the user's own show_osd setting. Which
    // screens the overlay is allowed on is decided inside the function, because
    // it also has to notice the transition and erase itself on the way out.
    if (conf.show_osd)
    {
        // force is gui_redraw()'s own enforce-redraw flag: draw_restore() has
        // just erased the screen, or the mode asked to be repainted. Either way
        // the plates are gone and the overlay cannot tell from its own state.
        gui_draw_persistent_osd(flag_gui_enforce_redraw);

        // Inside the same lock as the plates, so it goes up and comes down
        // while CHDK still owns the bitmap. After the grid, which is drawn
        // further up this function - the credit sits at two thirds height,
        // inside the grid, and has to paint over its lines rather than be cut
        // by them. Before recui_draw(), so an arrow readout or the FUNC menu
        // still wins over the credit - a menu the user opened in the first two
        // seconds matters more than the credit does.
        posd_credit_draw();
    }

    // After the plates, so the readout sits on top of them rather than being
    // erased by the row repaints underneath it. Inside the lock, so it goes up
    // and comes down while CHDK still owns the bitmap - which is the whole
    // reason it can exist at all.
    recui_draw(flag_gui_enforce_redraw);

    posd_canon_osd_end(posd_active);
    }
#endif

// DEBUG: uncomment if you want debug values always on top
//gui_draw_debug_vals_osd();

    // Call redraw handler
    if (gui_mode->redraw)
        gui_mode->redraw(flag_gui_enforce_redraw);

}

//-------------------------------------------------------------------
// Main kbd processing for GUI modes
// Return:
//          0 = normal
//          1 = block buttons pressed from Camera firmware
int gui_kbd_process()
{
    if (gui_mode)
    {
        // Call menu button handler if menu button pressed
        if (gui_mode->kbd_process_menu_btn)
        {
            if (kbd_is_key_clicked(KEY_MENU))
            {
                gui_mode->kbd_process_menu_btn();
                return 0;
            }
        }

        // Call mode handler for other buttons
        if (gui_mode->kbd_process) return gui_mode->kbd_process();
    }
    return 0;
}

// Handle touch screen presses
int gui_touch_process(int x, int y)
{
    if (gui_mode && gui_mode->touch_handler)
        return gui_mode->touch_handler(x, y);
    return 0;
}

//------------------------------------------------------------------- 
static int gui_current_alt_state = ALT_MODE_NORMAL;

// Called from the KBD task code to change ALT mode state
void gui_set_alt_mode_state(int new_state)
{
    gui_current_alt_state = new_state;
}

// Called from the GUI task code to set the ALT mode state
void gui_activate_alt_mode()
{
    extern gui_handler scriptGuiHandler;

    switch (gui_current_alt_state)
    {
    case ALT_MODE_ENTER:
    case ALT_MODE_ENTER_SCRIPT:
        
        gui_set_mode((gui_current_alt_state == ALT_MODE_ENTER_SCRIPT) ? &scriptGuiHandler : &altGuiHandler);

        conf_update_prevent_shutdown();
        
        vid_turn_off_updates();

        // If user menu set to start automatically when <ALT> mode entered 
        // then enter user menu mode, unless a script was paused by exiting 
        // <ALT> mode when the script was running.
        extern int gui_user_menu_flag;
        gui_user_menu_flag = 0;
        if ((conf.user_menu_enable == 2) && !camera_info.state.state_kbd_script_run) {
            gui_menu_init(&user_submenu);
            gui_set_mode(&menuGuiHandler);
            gui_user_menu_flag = 1;
        }
        break;

    case ALT_MODE_LEAVE:
        conf_save();

        // Unload all modules which are marked as safe to unload, or loaded for menus
        module_exit_alt();

        rbf_set_codepage(FONT_CP_WIN);
        vid_turn_on_updates();
        gui_set_mode(&defaultGuiHandler);

        conf_update_prevent_shutdown();
        break;
    }

    // Reset to stable state
    gui_current_alt_state = ALT_MODE_NORMAL;
}
