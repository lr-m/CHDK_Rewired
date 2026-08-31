#include "camera_info.h"
#include "color.h"
#include "keyboard.h"
#include "conf.h"
#include "lang.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"

#include "gui_palette.h"
#include "module_def.h"

#include "viewport.h"

void gui_module_menu_kbd_process();
int gui_palette_kbd_process();
void gui_palette_draw();

gui_handler GUI_MODE_PALETTE_MODULE = 
/*GUI_MODE_PALETTE*/    { GUI_MODE_PALETTE, gui_palette_draw, gui_palette_kbd_process, gui_module_menu_kbd_process, 0, 0 };

//-------------------------------------------------------------------
static int running = 0;
static chdkColor cl;
static int palette_mode;
static void (*palette_on_select)(chdkColor clr);
static int gui_palette_redraw;
static int test_page;

//-------------------------------------------------------------------
void gui_palette_init(int mode, chdkColor st_color, void (*on_select)(chdkColor clr))
{
    running = 1;
    cl = st_color;
    palette_mode = mode;
    palette_on_select = on_select;
    gui_palette_redraw = 1;
    test_page = 0;
    gui_set_mode(&GUI_MODE_PALETTE_MODULE);
}

//-------------------------------------------------------------------
int gui_palette_kbd_process()
{
    switch (kbd_get_autoclicked_key())
    {
        case KEY_DOWN:
            if (palette_mode != PALETTE_MODE_TEST)
            {
                if (cl.type)
                {
                    cl.type = 0;
                    if (cl.col > 15)
                        cl.col = 15;
                }
                else
                {
                    if ((cl.col & 0xF0) == 0xF0)
                    {
                        cl.type = 1;
                        cl.col &= 0x0F;
                    }
                    else
                    {
                        cl.col = (((cl.col+16)&0xf0)|(cl.col&0x0f));
                    }
                }
                gui_palette_redraw = 1;
            }
            break;
        case KEY_UP:
            if (palette_mode != PALETTE_MODE_TEST)
            {
                if (cl.type)
                {
                    cl.type = 0;
                    if (cl.col > 15)
                        cl.col = 15;
                    cl.col |= 0xF0;
                }
                else
                {
                    if ((cl.col & 0xF0) == 0x00)
                    {
                        cl.type = 1;
                    }
                    else
                    {
                        cl.col = (((cl.col-16)&0xf0)|(cl.col&0x0f));
                    }
                }
                gui_palette_redraw = 1;
            }
            break;
        case KEY_LEFT:
            if (palette_mode != PALETTE_MODE_TEST)
            {
                if (cl.type)
                {
                    if (cl.col-- == 0) cl.col = IDX_COLOR_MAX;
                }
                else
                {
                    cl.col = ((cl.col&0xf0)|((cl.col-1)&0x0f));
                }
            }
            else
            {
                if (--test_page < 0) test_page = 2;
            }
            gui_palette_redraw = 1;
            break;
        case KEY_RIGHT:
            if (palette_mode != PALETTE_MODE_TEST)
            {
                if (cl.type)
                {
                    if (cl.col++ == IDX_COLOR_MAX) cl.col = 0;
                }
                else
                {
                    cl.col = ((cl.col&0xf0)|((cl.col+1)&0x0f));
                }
            }
            else
            {
                if (++test_page > 2) test_page = 0;
            }
            gui_palette_redraw = 1;
            break;
        case KEY_SET:
            if (palette_mode == PALETTE_MODE_SELECT)
            {
                if (palette_on_select) 
                    palette_on_select(cl);
                gui_module_menu_kbd_process();
            }
            break;
    }
    return 0;
}

//-------------------------------------------------------------------
static void palette_test()
{
    unsigned int x, y, xl, xr, w, h;
    color c;

    xl = camera_screen.disp_left;
    xr = camera_screen.disp_right;

    if (gui_palette_redraw)
    {
        // Draw top text line - current color + instructions
        draw_rectangle(xl, 0, xr, camera_screen.height-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
        draw_string(xr-22*FONT_WIDTH, 0, "Use \x1b\x1a to change page", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

        color cols[3][20] = {
                {
                        COLOR_WHITE         ,COLOR_RED           ,COLOR_RED_DK        ,COLOR_RED_LT        ,
                        COLOR_GREEN         ,COLOR_BLUE          ,COLOR_BLUE_LT       ,COLOR_YELLOW        ,
                        COLOR_GREY          ,COLOR_GREY_DK       ,COLOR_GREY_LT       ,COLOR_TRANSPARENT
                },
                {
                        COLOR_RED       ,COLOR_GREEN     ,
                        COLOR_BLUE      ,COLOR_CYAN      ,
                        COLOR_MAGENTA   ,COLOR_YELLOW
                },
                {
                        3  ,6  ,9  ,12 ,15,
                        4  ,7  ,10 ,13 ,16,
                        5  ,8  ,11 ,14 ,17,
                        0  ,1  ,2  ,18 ,19
                }
        };

        char *nams[3][20] = {
                {
                        "white", "red", "dark red", "light red",
                        "green", "blue", "light blue", "yellow",
                        "grey", "dark grey", "light grey", "transparent"
                },
                {
                        "red", "green", "blue", "cyan", "magenta", "yellow",
                },
                {
                        "red", "green", "blue", "grey", "yellow",
                        "dk red", "dk green", "dk blue", "dk grey", "dk yellow",
                        "lt red", "lt green", "lt blue", "lt grey", "lt yellow",
                        "transp", "black", "white", "trns grey", "magenta"
                }
        };

        if (test_page == 0)
        {
            draw_string(xl, 0, "System Colors", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            c = 0;
            w = camera_screen.disp_width / 4;
            h = (camera_screen.height - (2 * FONT_HEIGHT)) / 3;
            for (y=0; y<3; y++)
            {
                for (x=0; x<4; x++, c++)
                {
                    draw_rectangle(xl+(x*w), (2*FONT_HEIGHT)+(y*h), xl+(x*w)+w-1, (2*FONT_HEIGHT)+(y*h)+h-FONT_HEIGHT-6, MAKE_COLOR(cols[test_page][c],cols[test_page][c]), RECT_BORDER0|DRAW_FILLED);
                    draw_string(xl+(x*w),(2*FONT_HEIGHT)+(y*h)+h-FONT_HEIGHT-3, nams[test_page][c], MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
            }
        }
        else if (test_page == 1)
        {
            draw_string(xl, 0, "Histogram Colors", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            c = 0;
            w = camera_screen.disp_width / 2;
            h = (camera_screen.height - (2 * FONT_HEIGHT)) / 3;
            for (y=0; y<3; y++)
            {
                for (x=0; x<2; x++, c++)
                {
                    draw_rectangle(xl+(x*w), (2*FONT_HEIGHT)+(y*h), xl+(x*w)+w-1, (2*FONT_HEIGHT)+(y*h)+h-FONT_HEIGHT-6, MAKE_COLOR(cols[test_page][c],cols[test_page][c]), RECT_BORDER0|DRAW_FILLED);
                    draw_string(xl+(x*w),(2*FONT_HEIGHT)+(y*h)+h-FONT_HEIGHT-3, nams[test_page][c], MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
            }
        }
        else if (test_page == 2)
        {
            draw_string(xl, 0, "Script/Icon Colors", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            c = 0;
            w = camera_screen.disp_width / 5;
            h = (camera_screen.height - (2 * FONT_HEIGHT)) / 4;
            for (y=0; y<4; y++)
            {
                for (x=0; x<5; x++, c++)
                {
                    draw_rectangle(xl+(x*w), (2*FONT_HEIGHT)+(y*h), xl+(x*w)+w-1, (2*FONT_HEIGHT)+(y*h)+h-FONT_HEIGHT-6,
                            MAKE_COLOR(get_script_color(cols[test_page][c]+256),get_script_color(cols[test_page][c]+256)), RECT_BORDER0|DRAW_FILLED);
                    draw_string(xl+(x*w),(2*FONT_HEIGHT)+(y*h)+h-FONT_HEIGHT-3, nams[test_page][c], MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
            }
        }

        gui_palette_redraw = 0;
    }
}

//-------------------------------------------------------------------
// sizes computed at runtime
#define CELL_SIZE           cellsize
#define BORDER_SIZE         6
#define CELL_ZOOM           BORDER_SIZE
#define DISP_TOP_CHDK       (FONT_HEIGHT + BORDER_SIZE)
#define DISP_TOP            disptop
#define DISP_LEFT           BORDER_SIZE
#define DISP_RIGHT          dispright
#define DISP_BOTTOM         dispbottom

static void palette_draw()
{
    int x, y, xl, xr;
    color c;
    static char buf[64];
    static int cellsize = 0, disptop, dispright, dispbottom;

    if (!cellsize)
    {
        // calculate these only once
        cellsize = (camera_screen.height - FONT_HEIGHT - 3 * BORDER_SIZE -1 ) / 17;
        disptop = camera_screen.height + DISP_TOP_CHDK - cellsize * 16 - FONT_HEIGHT - 2 * BORDER_SIZE - 1;
        dispright = DISP_LEFT + cellsize * 16;
        dispbottom = DISP_TOP + cellsize * 16;
    }

    xl = camera_screen.disp_left;
    xr = camera_screen.disp_right;
    int *pal = (int*)vid_get_bitmap_active_palette();

    if (gui_palette_redraw)
    {
        // Draw top text line - current color + instructions
        draw_rectangle(xl, 0, xr, FONT_HEIGHT-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
        draw_string(xr-29*FONT_WIDTH, 0, "    Use \x18\x19\x1b\x1a to change color ", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        if ( pal )
            sprintf(buf, " %s: 0x%02hX 0x%08X", lang_str(LANG_PALETTE_TEXT_COLOR), cl.col, pal[chdkColorToCanonColor(cl)]);
        else
            sprintf(buf, " %s: 0x%02hX", lang_str(LANG_PALETTE_TEXT_COLOR), cl.col );
        draw_string(xl, 0, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

        // Draw gray borders
        draw_rectangle(xl, DISP_TOP_CHDK-BORDER_SIZE, xr, camera_screen.height-1, MAKE_COLOR(COLOR_GREY, COLOR_GREY), RECT_BORDER6); // outer border
        draw_rectangle(xl+BORDER_SIZE, DISP_TOP_CHDK+CELL_SIZE+1, xr-BORDER_SIZE, DISP_TOP-1, MAKE_COLOR(COLOR_GREY, COLOR_GREY), RECT_BORDER0|DRAW_FILLED); //horiz divider
        draw_rectangle(xl+DISP_RIGHT+1, DISP_TOP, xl+DISP_RIGHT+BORDER_SIZE, DISP_BOTTOM, MAKE_COLOR(COLOR_GREY, COLOR_GREY), RECT_BORDER0|DRAW_FILLED); //vert divider
        draw_rectangle(xl+DISP_RIGHT+BORDER_SIZE+1, DISP_TOP, xr-BORDER_SIZE, DISP_TOP+3*CELL_SIZE-1, MAKE_COLOR(COLOR_GREY, COLOR_GREY), RECT_BORDER0|DRAW_FILLED); //above sample

        // Draw CHDK Palette color boxes
        c = 0;
        for (x=DISP_LEFT; x<DISP_LEFT+CELL_SIZE*(IDX_COLOR_MAX+1); x+=CELL_SIZE, c++)
        {
            draw_rectangle(xl+x, DISP_TOP_CHDK, xl+x+CELL_SIZE, DISP_TOP_CHDK+CELL_SIZE, MAKE_COLOR(chdk_colors[c],COLOR_BLACK), RECT_BORDER1|DRAW_FILLED);
        }
        draw_rectangle(xl+DISP_LEFT+CELL_SIZE*(IDX_COLOR_MAX+1)+1, DISP_TOP_CHDK, xr-BORDER_SIZE, DISP_TOP_CHDK+CELL_SIZE, MAKE_COLOR(COLOR_GREY, COLOR_GREY), RECT_BORDER0|DRAW_FILLED);
        draw_string(xl+DISP_LEFT+CELL_SIZE*(IDX_COLOR_MAX+1)+1, DISP_TOP_CHDK, " <-- CHDK", MAKE_COLOR(COLOR_GREY,COLOR_WHITE));

        // Draw Canon Palette color boxes
        c = 0;
        for (y=DISP_TOP; y<DISP_BOTTOM; y+=CELL_SIZE)
        {
            for (x=DISP_LEFT; x<DISP_RIGHT; x+=CELL_SIZE, c++)
            {
                draw_rectangle(xl+x, y, xl+x+CELL_SIZE, y+CELL_SIZE, MAKE_COLOR(c,COLOR_BLACK), RECT_BORDER1|DRAW_FILLED);
            }
        }
        draw_string(xl+DISP_RIGHT+BORDER_SIZE, DISP_TOP+10, "<-- Canon", MAKE_COLOR(COLOR_GREY,COLOR_WHITE));

        // Co-ordinate of selected color
        if (cl.type)
        {
            y = DISP_TOP_CHDK;
            x = DISP_LEFT + cl.col * CELL_SIZE;
        }
        else
        {
            y = DISP_TOP + ((cl.col>>4)&0x0F) * CELL_SIZE;
            x = DISP_LEFT + (cl.col&0x0F) * CELL_SIZE;
        }

        // Highlight selected color
        draw_rectangle(xl+x-CELL_ZOOM, y-CELL_ZOOM, xl+x+CELL_SIZE+CELL_ZOOM, y+CELL_SIZE+CELL_ZOOM, MAKE_COLOR(chdkColorToCanonColor(cl), COLOR_RED), RECT_BORDER2|DRAW_FILLED);

        // Fill 'sample' area with selected color
        draw_rectangle(xl+DISP_RIGHT+BORDER_SIZE+1, DISP_TOP+CELL_SIZE*3, xr-BORDER_SIZE, DISP_BOTTOM, MAKE_COLOR(chdkColorToCanonColor(cl), COLOR_WHITE), RECT_BORDER1|DRAW_FILLED);

        gui_palette_redraw = 0;
    }
}

//-------------------------------------------------------------------

void gui_palette_draw()
{
    if (palette_mode == PALETTE_MODE_TEST)
        palette_test();
    else
        palette_draw();
}

void gui_module_menu_kbd_process()
{
    running = 0;
    gui_default_kbd_process_menu_btn();
}

// =========  MODULE INIT =================

/***************** BEGIN OF AUXILARY PART *********************
ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
**************************************************************/

int _module_can_unload()
{
    return running == 0;
}

int _module_exit_alt()
{
    running = 0;
    return 0;
}

/******************** Module Information structure ******************/

libpalette_sym _libpalette =
{
    {
         0, 0, _module_can_unload, _module_exit_alt, 0
    },

    gui_palette_init
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    GUI_PALETTE_VERSION,		// Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,			// Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,		// Specify platform dependency

    (int32_t)"Palette",
    MTYPE_EXTENSION,

    &_libpalette.base,

    ANY_VERSION,                // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
