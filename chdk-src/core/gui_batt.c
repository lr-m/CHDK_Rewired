#include "camera_info.h"
#include "keyboard.h"
#include "battery.h"
#include "conf.h"
#include "gui_draw.h"
#include "gui_batt.h"
//-------------------------------------------------------------------

static long get_batt_average() {
    #define VOLTS_N         100
    static unsigned short   volts[VOLTS_N] = {0};
    static unsigned int     n = 0, rn = 0;
    static unsigned long    volt_aver = 0;

    volt_aver-=volts[n];
    volts[n]=(unsigned short)stat_get_vbatt();
    volt_aver+=volts[n];
    if (++n>rn) rn=n;
    if (n>=VOLTS_N) n=0;
    return volt_aver/rn;
}

//-------------------------------------------------------------------
unsigned long get_batt_perc() {
    long v;

    v = get_batt_average();
    if (v>conf.batt_volts_max) v=conf.batt_volts_max;
    if (v<conf.batt_volts_min) v=conf.batt_volts_min;
    return (v-conf.batt_volts_min)*100/(conf.batt_volts_max-conf.batt_volts_min);
}

//-------------------------------------------------------------------
static icon_cmd batt_icon[] =
{
        { IA_ROUND_RECT,   0,  3,  3,  9, IDX_COLOR_GREY,        IDX_COLOR_GREY        },
        { IA_RECT,         3,  0, 31, 12, IDX_COLOR_GREY,        IDX_COLOR_GREY        },
        { IA_FILLED_RECT,  1,  4,  2,  8, IDX_COLOR_YELLOW_DK,   IDX_COLOR_YELLOW_DK   },
        { IA_FILLED_RECT,  4,  6, 30, 11, IDX_COLOR_GREY_DK,     IDX_COLOR_GREY_DK     },
        { IA_FILLED_RECT,  4,  1, 30,  5, IDX_COLOR_GREY_LT,     IDX_COLOR_GREY_LT     },
        { IA_RECT,         4,  2, 30, 10, 0,                     0                     },
        { IA_FILLED_RECT, 29,  6, 29,  9, 0,                     0                     },
        { IA_FILLED_RECT, 29,  3, 29,  5, 0,                     0                     },
        { IA_END }
};

static void gui_batt_draw_icon ()
{
    int perc = get_batt_perc();

    // set bar color depending percent
    color cl1 = (perc>50) ? IDX_COLOR_GREEN_DK :(perc<=20) ? IDX_COLOR_RED_DK : IDX_COLOR_YELLOW_DK;
    color cl2 = (perc>50) ? IDX_COLOR_GREEN    :(perc<=20) ? IDX_COLOR_RED    : IDX_COLOR_YELLOW;
    color cl3 = (perc>50) ? IDX_COLOR_GREEN_LT :(perc<=20) ? IDX_COLOR_RED_LT : IDX_COLOR_YELLOW_LT;

    // Set dynamic properties for battery level
    batt_icon[5].cb = cl1;
    batt_icon[6].x1 = batt_icon[7].x1 = 29 - (25 * perc / 100);
    batt_icon[6].cb = batt_icon[6].cf = cl2;
    batt_icon[7].cb = batt_icon[7].cf = cl3;

    // Draw icon
    draw_icon_cmds(conf.batt_icon_pos.x, conf.batt_icon_pos.y, batt_icon);
}

//-------------------------------------------------------------------
static void gui_batt_draw_charge(){
    char buf[16];

    int perc = get_batt_perc();
    twoColors cl = user_color((perc<=20) ? conf.osd_color_warn : conf.osd_color);
    sprintf(buf, "%3d%%", perc);
    buf[5]=0;
    draw_osd_string(conf.batt_txt_pos, 0, 0, buf, cl, conf.batt_txt_scale);
}

//-------------------------------------------------------------------
static void gui_batt_draw_volts() {
    unsigned long v;
    char buf[16];

    v = get_batt_average();
    sprintf(buf, "%ld.%03ld", v/1000, v%1000);
    buf[5]=0;
    draw_osd_string(conf.batt_txt_pos, 0, 0, buf, user_color(conf.osd_color), conf.batt_txt_scale);
}

//-------------------------------------------------------------------
void gui_batt_draw_osd(int is_osd_edit)
{
    if (conf.batt_perc_show || is_osd_edit)
        gui_batt_draw_charge();

    if (conf.batt_volts_show || is_osd_edit)
        gui_batt_draw_volts();
    
    if (conf.batt_icon_show || is_osd_edit)
        gui_batt_draw_icon();
}

//-------------------------------------------------------------------
