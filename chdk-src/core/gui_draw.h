#ifndef GUI_DRAW_H
#define GUI_DRAW_H

#include "conf.h"       // load OSD_pos & OSD_scale
#include "palette.h"

//-------------------------------------------------------------------

extern  unsigned char   *chdk_colors;

#define IDX_COLOR_TRANSPARENT       0
#define IDX_COLOR_BLACK             1
#define IDX_COLOR_WHITE             2
#define IDX_COLOR_RED               3
#define IDX_COLOR_RED_DK            4
#define IDX_COLOR_RED_LT            5
#define IDX_COLOR_GREEN             6
#define IDX_COLOR_GREEN_DK          7
#define IDX_COLOR_GREEN_LT          8
#define IDX_COLOR_BLUE              9
#define IDX_COLOR_BLUE_DK           10
#define IDX_COLOR_BLUE_LT           11
#define IDX_COLOR_GREY              12
#define IDX_COLOR_GREY_DK           13
#define IDX_COLOR_GREY_LT           14
#define IDX_COLOR_YELLOW            15
#define IDX_COLOR_YELLOW_DK         16
#define IDX_COLOR_YELLOW_LT         17
#define IDX_COLOR_GREY_DK_TRANS     18
#define IDX_COLOR_MAGENTA           19
#define IDX_COLOR_CYAN              IDX_COLOR_BLUE_LT

#define IDX_COLOR_MAX               19

#define COLOR_WHITE             (chdk_colors[IDX_COLOR_WHITE])
#define COLOR_RED               (chdk_colors[IDX_COLOR_RED])
#define COLOR_RED_DK            (chdk_colors[IDX_COLOR_RED_DK])
#define COLOR_RED_LT            (chdk_colors[IDX_COLOR_RED_LT])
#define COLOR_GREEN             (chdk_colors[IDX_COLOR_GREEN])
#define COLOR_GREEN_DK          (chdk_colors[IDX_COLOR_GREEN_DK])
#define COLOR_GREEN_LT          (chdk_colors[IDX_COLOR_GREEN_LT])
#define COLOR_BLUE              (chdk_colors[IDX_COLOR_BLUE])
#define COLOR_BLUE_DK           (chdk_colors[IDX_COLOR_BLUE_DK])
#define COLOR_BLUE_LT           (chdk_colors[IDX_COLOR_BLUE_LT])
#define COLOR_GREY              (chdk_colors[IDX_COLOR_GREY])
#define COLOR_GREY_DK           (chdk_colors[IDX_COLOR_GREY_DK])
#define COLOR_GREY_LT           (chdk_colors[IDX_COLOR_GREY_LT])
#define COLOR_YELLOW            (chdk_colors[IDX_COLOR_YELLOW])
#define COLOR_YELLOW_DK         (chdk_colors[IDX_COLOR_YELLOW_DK])
#define COLOR_YELLOW_LT         (chdk_colors[IDX_COLOR_YELLOW_LT])
#define COLOR_GREY_DK_TRANS     (chdk_colors[IDX_COLOR_GREY_DK_TRANS])
#define COLOR_MAGENTA           (chdk_colors[IDX_COLOR_MAGENTA])
#define COLOR_CYAN              (chdk_colors[IDX_COLOR_CYAN])

//-------------------------------------------------------------------
#define FONT_REAL_WIDTH         8
#define FONT_REAL_HEIGHT        16

#ifndef CAM_DRAW_YUV

#define FONT_WIDTH              8
#define FONT_HEIGHT             16
// Name of default symbol file (for reset)
#define DEFAULT_SYMBOL_FILE     "A/CHDK/SYMBOLS/icon_10.rbf"

#else // CAM_DRAW_YUV

#define FONT_WIDTH              14
#define FONT_HEIGHT             32
// Name of default symbol file (for reset)
#define DEFAULT_SYMBOL_FILE     "A/CHDK/SYMBOLS/icon_16.rbf"

#endif // CAM_DRAW_YUV

// Text justification & options
#define TEXT_LEFT               0
#define TEXT_CENTER             1
#define TEXT_RIGHT              2
#define TEXT_FILL               16

// Drawing flag options for rectangle and ellipse
#define RECT_BORDER0            0       // Border widths
#define RECT_BORDER1            1
#define RECT_BORDER2            2
#define RECT_BORDER3            3
#define RECT_BORDER4            4
#define RECT_BORDER5            5
#define RECT_BORDER6            6
#define RECT_BORDER7            7
#define RECT_BORDER_MASK        7
#define DRAW_FILLED             8       // Filled rectangle or ellipse
#define RECT_SHADOW0            0       // Drop shadow widths
#define RECT_SHADOW1            0x10
#define RECT_SHADOW2            0x20
#define RECT_SHADOW3            0x30
#define RECT_SHADOW_MASK        0x30
#define RECT_ROUND_CORNERS      0x40    // Round corners on rectangle

//-------------------------------------------------------------------
extern void draw_init();
extern void draw_set_draw_proc(void (*pixel_proc)(unsigned int offset, color cl));
extern void update_draw_proc();

extern void draw_set_guard();
extern int draw_test_guard();

extern color draw_get_pixel(coord x, coord y);
extern void draw_pixel(coord x, coord y, color cl);
extern void draw_set_grid_clip(int enable);
extern void draw_or_erase_edge_pixel(coord x, coord y, color cl, int is_draw);

extern void draw_dblpixel_raw(unsigned int offset, unsigned int px, unsigned int op);
extern void erase_zebra();
extern unsigned int color_to_rawpx(color cl, unsigned int *op);

extern void draw_line(coord x1, coord y1, coord x2, coord y2, color cl);
extern void draw_hline(coord x, coord y, int len, color cl);
extern void draw_vline(coord x, coord y, int len, color cl);

// draw shapes
extern void draw_rectangle(coord x1, coord y1, coord x2, coord y2, twoColors cl, int flags);
extern void draw_ellipse(coord xc, coord yc, unsigned int a, unsigned int b, color cl, int flags);

// draw text
extern int  text_dimensions(const char *s, int width, int max_chars, int *max_lines);
extern void draw_char(coord x, coord y, const char ch, twoColors cl);
extern int  draw_string_clipped(coord x, coord y, const char *s, twoColors cl, int max_width);
extern int  draw_string(coord x, coord y, const char *s, twoColors cl);
extern int  draw_string_justified(coord x, coord y, const char *s, twoColors cl, int xo, int max_width, int justification);
extern int  draw_text_justified(coord x, coord y, const char *s, twoColors cl, int max_chars, int max_lines, int justification);
extern void draw_string_scaled(coord x, coord y, const char *s, twoColors cl, int xsize, int ysize);
extern void draw_osd_string(OSD_pos pos, int xo, int yo, const char *s, twoColors c, OSD_scale scale);
extern void draw_button(int x, int y, int w, int str_id, int active);

extern void draw_txt_string(coord col, coord row, const char *str, twoColors cl);

extern void draw_progress_bar(const char* msg, const unsigned int perc);

extern void draw_suspend(int ms);
extern int draw_is_suspended(void);
extern void draw_restore();

extern color get_script_color(int cl);

extern color chdkColorToCanonColor(chdkColor c);
extern twoColors user_color(confColor c);

//-------------------------------------------------------------------
// Icon rendering using an array of drawing actions

// Allowed drawing actions for icons
enum icon_actions
{
    IA_END,
    IA_HLINE,
    IA_VLINE,
    IA_LINE,
    IA_RECT,
    IA_FILLED_RECT,
    IA_ROUND_RECT,
    IA_FILLED_ROUND_RECT
};

// Structure for a drawing action
typedef struct
{
    unsigned char   action;
    unsigned char   x1, y1;
    unsigned char   x2, y2;
    color           cb, cf;     // Note these should be IDX_COLOR_xxx values, converted to actual colors at runtime
} icon_cmd;

// Draw an icon from a list of actions
extern void draw_icon_cmds(coord x, coord y, icon_cmd *cmds);

//-------------------------------------------------------------------

// Swap two integer values
#define swap(v1, v2)   {v1^=v2; v2^=v1; v1^=v2;}

//-------------------------------------------------------------------
#endif
