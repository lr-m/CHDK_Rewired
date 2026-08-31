#include "camera_info.h"
#include "keyboard.h"
#include "lang.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "gui_batt.h"
#include "gui_mbox.h"
#include "modes.h"
#include "time.h"

#include "module_def.h"

void gui_game_menu_kbd_process();
int gui_mastermind_kbd_process();
void gui_mastermind_draw();

gui_handler GUI_MODE_MASTERMIND = 
    /*GUI_MODE_MASTERMIND*/ { GUI_MODE_MODULE, gui_mastermind_draw, gui_mastermind_kbd_process, gui_game_menu_kbd_process, 0, GUI_MODE_FLAG_NODRAWRESTORE };

int cell_size = 20;

#define COLOR_LIGHT_GRAY    MAKE_COLOR(COLOR_GREY_LT,COLOR_GREY_LT)
#define BK_COLOR            MAKE_COLOR(COLOR_GREY,COLOR_GREY)
#define TEXT_COLOR          MAKE_COLOR(COLOR_GREY,COLOR_BLACK)

int curr_x;
int curr_y;
int answer[4];
char colors[6];
int curr_color[4];
int GameGo;
static char buf[128];

static void choice_box(int x, int y, twoColors cl)
{
    x = camera_screen.disp_left+FONT_WIDTH+(x*cell_size);
    y = FONT_HEIGHT+FONT_HEIGHT/2+(y*cell_size);
    draw_rectangle(x+5, y+5, x+cell_size-5, y+cell_size-5, cl, RECT_BORDER1|DRAW_FILLED);
}

static void clue_box(int pos, color col)
{
    int x = camera_screen.disp_left+4*cell_size+10+(pos+2)*(FONT_WIDTH+2);
    int y = FONT_HEIGHT+FONT_HEIGHT/2+(curr_y*cell_size) + cell_size/2 - FONT_WIDTH/2 + 1;
	draw_rectangle(x, y, x+FONT_WIDTH-2, y+FONT_WIDTH-2, MAKE_COLOR(col,col), RECT_BORDER0|DRAW_FILLED);
}

char WinQuary()
{
    char pos = 0;
    int i, j;

    for (i = 0; i < 4; i++)
    {
        if (answer[i] == curr_color[i])
        {
            clue_box(pos, COLOR_BLACK);
            pos++;
        }
    }

    if (pos == 4)
    {
        GameGo = 0;
        return 1;
    }

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            if ((answer[i] == curr_color[j]) && (i != j))
            {
                clue_box(pos, COLOR_WHITE);
                pos++;
            }
        }
    }

    if (curr_y == 0)
    {
        GameGo = 0;
        return 2;
    }

    return 0;
}

static void info_line(char* msg)
{
    draw_string(camera_screen.disp_right-24*FONT_WIDTH, camera_screen.height-6*FONT_HEIGHT-FONT_HEIGHT/2, msg, TEXT_COLOR);
}

char CanNext()
{
    if (curr_color[0] == 99 || curr_color[1] == 99 || curr_color[2] == 99 || curr_color[3] == 99)
        return 0;
    else if (curr_color[0] == curr_color[1] || curr_color[0] == curr_color[2] || curr_color[0] == curr_color[3] ||
             curr_color[1] == curr_color[2] || curr_color[1] == curr_color[3] ||
             curr_color[2] == curr_color[3])
    {
        info_line(lang_str(LANG_MENU_GAMES_DCOLOR));
        return 0;
    }
    return 1;
}

void CreateColorCombo()
{
    char tmp = 0;
    int i = 0;

    for (i = 0; i < 4; i++)
        answer[i] = 99;
    i = 0;
    while (i < 4)
    {
        tmp = rand() % 6;
        if (answer[0] != tmp && answer[1] != tmp && answer[2] != tmp && answer[3] != tmp)
        {
            answer[i] = tmp;
            i++;
        }
    }
}

int gui_mastermind_init()
{
    int i, j;

    if (camera_screen.height * 3 > camera_screen.width * 2)
    {
        // worst case scenario (640x480)
        cell_size = 8 * (camera_screen.height - FONT_HEIGHT * 2) / (9 * 9);
    }
    else
    {
        cell_size = (camera_screen.height - FONT_HEIGHT * 2) / 10;
    }

    curr_x = 0;
    curr_y = 7;

    draw_rectangle(camera_screen.disp_left, 0, camera_screen.disp_right, camera_screen.height - 1, BK_COLOR, RECT_BORDER0 | DRAW_FILLED);

    for (i = 0; i < 4; i++)
        for (j = 0; j < 8; j++)
            choice_box(i, j, COLOR_LIGHT_GRAY);

    draw_hline(camera_screen.disp_left + FONT_WIDTH / 2, FONT_HEIGHT * 2 + 8 * cell_size, 4 * cell_size + FONT_WIDTH, COLOR_WHITE);
    draw_vline(camera_screen.disp_left + 4 * cell_size + FONT_WIDTH * 2, FONT_HEIGHT, 8 * cell_size, COLOR_WHITE);
    draw_vline(camera_screen.disp_left + 4 * cell_size + FONT_WIDTH * 2 + 3, FONT_HEIGHT, 8 * cell_size, COLOR_WHITE);

    for (i = 0; i < 6; i++)
        draw_rectangle(camera_screen.disp_right - (24 - i * 4) * FONT_WIDTH, 2 * FONT_HEIGHT + FONT_HEIGHT / 2, camera_screen.disp_right - (24 - i * 4) * FONT_WIDTH + FONT_HEIGHT, 3 * FONT_HEIGHT + FONT_HEIGHT / 2, MAKE_COLOR(colors[i], colors[i]),
                       RECT_BORDER0 | DRAW_FILLED);

    draw_rectangle(camera_screen.disp_right - 19 * FONT_WIDTH - FONT_HEIGHT - 8, 5 * FONT_HEIGHT, camera_screen.disp_right - 19 * FONT_WIDTH - 8, 6 * FONT_HEIGHT, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0 | DRAW_FILLED);
    draw_string(camera_screen.disp_right - 19 * FONT_WIDTH, 5 * FONT_HEIGHT, lang_str(LANG_MENU_GAMES_RIGHT_PLACE), TEXT_COLOR);

    draw_rectangle(camera_screen.disp_right - 19 * FONT_WIDTH - FONT_HEIGHT - 8, 6 * FONT_HEIGHT + FONT_HEIGHT / 2, camera_screen.disp_right - 19 * FONT_WIDTH - 8, 7 * FONT_HEIGHT + FONT_HEIGHT / 2, MAKE_COLOR(COLOR_WHITE, COLOR_WHITE),
                   RECT_BORDER0 | DRAW_FILLED);
    draw_string(camera_screen.disp_right - 19 * FONT_WIDTH, 6 * FONT_HEIGHT + FONT_HEIGHT / 2, lang_str(LANG_MENU_GAMES_C_IN_ANSWER), TEXT_COLOR);

    draw_string(camera_screen.disp_right - 24 * FONT_WIDTH, FONT_HEIGHT + FONT_HEIGHT / 2, lang_str(LANG_MENU_GAMES_AC_COLORS), TEXT_COLOR);

    draw_string(camera_screen.disp_right - 24 * FONT_WIDTH, camera_screen.height - 3 * FONT_HEIGHT, lang_str(LANG_MENU_GAMES_CURSOR1), TEXT_COLOR);
    draw_string(camera_screen.disp_right - 24 * FONT_WIDTH, camera_screen.height - 4 * FONT_HEIGHT, lang_str(LANG_MENU_GAMES_CURSOR2), TEXT_COLOR);
    draw_string(camera_screen.disp_right - 24 * FONT_WIDTH, camera_screen.height - 5 * FONT_HEIGHT, lang_str(LANG_MENU_GAMES_CURSOR3), TEXT_COLOR);

    for (i = 0; i < 4; i++)
        curr_color[i] = 99;

    gui_set_mode(&GUI_MODE_MASTERMIND);
    return 1;
}

//-------------------------------------------------------------------

static void draw_box(color border)
{
    if (curr_color[curr_x] == 99) curr_color[curr_x] = 0;
    choice_box(curr_x, curr_y, MAKE_COLOR(colors[curr_color[curr_x]],border));
}

static void end_game(int msg)
{
    int i;
    info_line(lang_str(msg));
	for (i=0; i<4;i++)
	    choice_box(i, 9, MAKE_COLOR(colors[answer[i]],colors[answer[i]]));
}

static void chg_box(int inc_box, int inc_val)
{
    draw_box(colors[curr_color[curr_x]]);
    curr_x = (curr_x + inc_box) & 3;
    if (curr_color[curr_x] == 99) curr_color[curr_x] = 0;
    curr_color[curr_x] = (curr_color[curr_x] + inc_val);
    if (curr_color[curr_x] < 0) curr_color[curr_x] = 5;
    else if (curr_color[curr_x] > 5) curr_color[curr_x] = 0;
    draw_box(COLOR_BLACK);
}

int gui_mastermind_kbd_process()
{
    int i = 0;
    rand();
    long key = kbd_get_autoclicked_key();
    if (key)
    {
        if (GameGo == 1)
        {
            info_line("                       ");
            switch (key)
            {
                case KEY_SET:
                    if (CanNext())
                    {
                        if (WinQuary() == 1)
                        {
                            end_game(LANG_MENU_GAMES_RIGHT);
                        }
                        else if (WinQuary() == 2)
                        {
                            end_game(LANG_MENU_GAMES_GAME_OVER);
                        }
                        else
                        {
                            draw_box(colors[curr_color[curr_x]]);
                            for (i = 0; i < 4; i++)
                                curr_color[i] = 99;
                            curr_y--;
                            curr_x = 0;
                            draw_box(COLOR_BLACK);
                        }
                    }
                    break;
                case KEY_LEFT:
                    chg_box(-1, 0);
                    break;
                case KEY_RIGHT:
                    chg_box(1, 0);
                    break;
                case KEY_UP:
                    chg_box(0, 1);
                    break;
                case KEY_DOWN:
                    chg_box(0, -1);
                    break;
            }
        }
        else if (key == KEY_SET)
        {
            gui_mastermind_init();
            CreateColorCombo();
            draw_box(COLOR_BLACK);
            GameGo = 1;
        }
    }
    return 0;
}
//-------------------------------------------------------------------
void gui_mastermind_draw() {
    static struct tm *ttm;

    draw_string(camera_screen.disp_left+15*FONT_WIDTH, 0, lang_str(LANG_MENU_GAMES_MASTERMIND), MAKE_COLOR(COLOR_GREY, COLOR_WHITE));

    ttm = get_localtime();
    sprintf(buf, "Time: %2u:%02u  Batt:%3d%%", ttm->tm_hour, ttm->tm_min, get_batt_perc());
    draw_string_justified(camera_screen.disp_left, camera_screen.height-FONT_HEIGHT,
                          buf, TEXT_COLOR, 0, camera_screen.disp_width-FONT_WIDTH, TEXT_RIGHT);
}

int basic_module_init()
{
    colors[0] = COLOR_RED;
    colors[1] = COLOR_GREEN;
    colors[2] = COLOR_BLUE;
    colors[3] = COLOR_YELLOW;
    colors[4] = COLOR_WHITE;
    colors[5] = COLOR_BLACK;

    return gui_mastermind_init();
}

#include "simple_game.c"

/******************** Module Information structure ******************/

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SIMPLE_MODULE_VERSION,		// Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,			// Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,		// Specify platform dependency

    -LANG_MENU_GAMES_MASTERMIND,// Module name
    MTYPE_GAME,

    &_librun.base,

    ANY_VERSION,                // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};
