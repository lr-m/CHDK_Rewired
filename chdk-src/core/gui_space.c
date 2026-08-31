#include "camera_info.h"
#include "conf.h"
#include "sd_card.h"
#include "gui_draw.h"
#include "gui_space.h"
//-------------------------------------------------------------------

unsigned long get_space_perc() {
    // accuracy reduced to support cards up to 2TB
    return (GetFreeCardSpaceKb()>>6)*100/(GetTotalCardSpaceKb()>>6);
}

// Local variables used by various space display functions, setup in space_color
static twoColors cl;
static coord xx, yy;
static int perc, width, height;

// Set up color and percent free variables for free space OSD
static void space_color()
{
    perc = get_space_perc();
    cl = user_color(conf.space_color);
    if (((conf.space_warn_type == 0) && (perc <= conf.space_perc_warn)) ||
        ((conf.space_warn_type == 1) && (GetFreeCardSpaceKb() <= (unsigned)conf.space_mb_warn*1024)))
    {
        cl = user_color(conf.osd_color_warn);
    }
}

// Setup position and size variables then draw free space bar, outer shape
static void spacebar_outer(OSD_pos pos, int w, int h)
{
    // Get color and percent free
    space_color();

    // space icon / bar position
    xx = pos.x;
    yy = pos.y;

    // space icon / bar size
    width = w;
    height = h;

    // Clamp co-ordinates to keep bar on screen
    if (xx > (camera_screen.width-width-4)) {
        xx = camera_screen.width-width-4;
    }
    if (yy > (camera_screen.height-height-4)) {
        yy = camera_screen.height-height-4;
    }

    draw_rectangle(xx, yy, xx+width+3, yy+height+3, MAKE_COLOR(COLOR_BLACK,COLOR_BLACK), RECT_BORDER1);     // Outer black rectangle
    draw_rectangle(xx+1, yy+1, xx+width+2, yy+height+2, cl, RECT_BORDER1);          // Inner white/red rectangle
}

static void gui_space_draw_spacebar_horizontal()
{
    coord x;

    // Setup and draw outer shape
    spacebar_outer(conf.space_hor_pos, (camera_screen.width / (4 >> conf.space_bar_size)) - 4, conf.space_bar_width);

    // space bar fill
    x = width - ((perc*width)/100);
    if (x < 1) x = 1;
    if (x >= width) x = width;
    else draw_rectangle(xx+x+2, yy+2, xx+width+1, yy+height+1, MAKE_COLOR(FG_COLOR(cl), FG_COLOR(cl)), RECT_BORDER0|DRAW_FILLED); // If not empty fill 'free' space area
    draw_rectangle(xx+2, yy+2, xx+x+1, yy+height+1, MAKE_COLOR(COLOR_TRANSPARENT, COLOR_BLACK), RECT_BORDER1|DRAW_FILLED);  // fill 'used' space area
}

static void gui_space_draw_spacebar_vertical() {
    coord y;

    // Setup and draw outer shape
    spacebar_outer(conf.space_ver_pos, conf.space_bar_width, (camera_screen.height / (4 >> conf.space_bar_size)) - 4);

    // space bar fill
    y = height - ((perc*height)/100);
    if (y < 1) y = 1;
    if (y >= height) y = height;
    else draw_rectangle(xx+2, yy+y+2, xx+width+1, yy+height+1, MAKE_COLOR(FG_COLOR(cl), FG_COLOR(cl)), RECT_BORDER0|DRAW_FILLED); // If not empty fill 'free' space area
    draw_rectangle(xx+2, yy+2, xx+width+1, yy+y+1, MAKE_COLOR(COLOR_TRANSPARENT, COLOR_BLACK), RECT_BORDER1|DRAW_FILLED);   // fill 'used' space area
}

static icon_cmd space_icon[] =
{
        { IA_HLINE,        0,  0, 30,  0, IDX_COLOR_GREY_LT,     IDX_COLOR_GREY_LT     },
        { IA_VLINE,        0,  0,  0, 13, IDX_COLOR_GREY_LT,     IDX_COLOR_GREY_LT     },
        { IA_VLINE,       31,  0,  0, 19, IDX_COLOR_GREY,        IDX_COLOR_GREY        },
        { IA_LINE,         1, 13,  5, 17, IDX_COLOR_GREY,        IDX_COLOR_GREY        },
        { IA_HLINE,        6, 18, 24,  0, IDX_COLOR_GREY,        IDX_COLOR_GREY        },
        { IA_FILLED_RECT,  1,  1, 30, 13, IDX_COLOR_GREY_DK,     IDX_COLOR_GREY_DK     },
        { IA_FILLED_RECT,  5, 14, 30, 17, IDX_COLOR_GREY_DK,     IDX_COLOR_GREY_DK     },
        { IA_FILLED_RECT,  3, 14,  6, 15, IDX_COLOR_GREY_DK,     IDX_COLOR_GREY_DK     },
        { IA_FILLED_RECT,  2,  2,  6,  4, IDX_COLOR_YELLOW_DK,   IDX_COLOR_YELLOW_DK   },
        { IA_FILLED_RECT,  2,  6,  6,  7, IDX_COLOR_YELLOW_DK,   IDX_COLOR_YELLOW_DK   },
        { IA_FILLED_RECT,  2,  9,  6, 10, IDX_COLOR_YELLOW_DK,   IDX_COLOR_YELLOW_DK   },
        { IA_FILLED_RECT,  2, 12,  6, 13, IDX_COLOR_YELLOW_DK,   IDX_COLOR_YELLOW_DK   },
        { IA_FILLED_RECT,  5, 15,  9, 13, IDX_COLOR_YELLOW_DK,   IDX_COLOR_YELLOW_DK   },
        { IA_HLINE,        8,  0,  2,  0, IDX_COLOR_TRANSPARENT, IDX_COLOR_TRANSPARENT },
        { IA_HLINE,       11,  0,  3,  0, IDX_COLOR_WHITE,       IDX_COLOR_WHITE       },
        { IA_HLINE,       11, 18,  2,  0, IDX_COLOR_TRANSPARENT, IDX_COLOR_TRANSPARENT },
        { IA_RECT,         9,  5, 28, 13, 0,                     0                     },
        { IA_FILLED_RECT, 27,  6, 27, 12, 0,                     0                     },
        { IA_END }
};

static void gui_space_draw_icon()
{
    space_color();

    color cl1 = IDX_COLOR_GREEN_DK;
    color cl2 = IDX_COLOR_GREEN;
    if (((conf.space_warn_type == 0) && (perc <= conf.space_perc_warn)) ||
        ((conf.space_warn_type == 1) && (GetFreeCardSpaceKb() <= (unsigned)conf.space_mb_warn*1024)))
    {
        cl1 = IDX_COLOR_RED_DK;
        cl2 = IDX_COLOR_RED;
    } 
  
    // Set dynamic properties for space left
    space_icon[16].cb = cl1;
    space_icon[17].x1 = 27 - (17 * perc / 100);
    space_icon[17].cf = space_icon[17].cb = cl2;

    // Draw icon
    draw_icon_cmds(conf.space_icon_pos.x, conf.space_icon_pos.y, space_icon);
}

//-------------------------------------------------------------------
static void gui_space_draw_value(int force)
{
    char buf[32];
    int offset = 0;

    space_color();

    if ((conf.show_partition_nr) && (get_part_count() > 1))
    {
        sprintf(buf, "%1d:\0", get_active_partition());
        offset = 2;
    }

    if (is_partition_changed())
    {
        sprintf(buf+offset, "???\0");
    }
    else
    {
        if (conf.space_perc_show || force)
        {
            sprintf(buf+offset, "%3d%%\0", perc);
        }
        else if (conf.space_mb_show)
        {
            unsigned int freemb = GetFreeCardSpaceKb()/1024;
            if (freemb < 10000) sprintf(buf+offset, "%4dM\0",freemb);
            else sprintf(buf+offset, "%4dG\0",freemb/1024);   // if 10 GiB or more free, print in GiB instead of MiB
        }
    }

    draw_osd_string(conf.space_txt_pos, 0, 0, buf, cl, conf.space_txt_scale);
}

//-------------------------------------------------------------------

void gui_space_draw_osd(int is_osd_edit)
{
    if (conf.space_icon_show || is_osd_edit)
        gui_space_draw_icon();

    if (conf.space_perc_show || conf.space_mb_show || is_osd_edit)
        gui_space_draw_value(is_osd_edit);

    if ((conf.space_bar_show == 1) || is_osd_edit)
        gui_space_draw_spacebar_horizontal();

    if (conf.space_bar_show == 2 || is_osd_edit)
        gui_space_draw_spacebar_vertical();
}
