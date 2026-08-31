#include "camera_info.h"
#include "keyboard.h"
#include "clock.h"
#include "conf.h"
#include "gui.h"
#include "font.h"
#include "gui_draw.h"
#include "gui_batt.h"
#include "gui_lang.h"
#include "time.h"

#include "gui_read.h"
#include "module_def.h"

//-------------------------------------------------------------------

extern void gui_read_kbd_process_menu_btn();
int gui_read_kbd_process();
void gui_read_draw();
void gui_read_kbd_leave();

gui_handler GUI_MODE_READ = 
/*GUI_MODE_READ*/   { GUI_MODE_MODULE, gui_read_draw, gui_read_kbd_process, gui_read_kbd_process_menu_btn, 0, 0 };

gui_handler *old_mode;

//-------------------------------------------------------------------
static int running = 0;
static int read_file;
static int read_file_size;
static int read_on_screen;
static int read_to_draw;
static coord x, y, h, w;
#define READ_BUFFER_SIZE  100
static char buffer[READ_BUFFER_SIZE+1];
static long last_time;
static int xx, yy;
static int pause;
static int busy_drawing = 0;
static int do_not_draw = 0;

static int reader_is_active;	// Flag raised when reader is succesfully runned
								// purpose: we shouldn't process "leave" sequence if we call unload module but reader was not runed yet

//-------------------------------------------------------------------
static void gui_read_draw_batt() {
    sprintf(buffer, "Batt:%3d%%", get_batt_perc());
    draw_string(camera_screen.disp_right-11*FONT_WIDTH, 0, buffer, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
}

//-------------------------------------------------------------------
static void gui_read_draw_clock() {
    static struct tm *ttm;

    ttm = get_localtime();
    sprintf(buffer, "%2u:%02u", ttm->tm_hour, ttm->tm_min);
    draw_string(camera_screen.disp_right-17*FONT_WIDTH, 0, buffer, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
}

//-------------------------------------------------------------------
static void gui_read_draw_scroll_indicator() {
    draw_char(camera_screen.disp_right-FONT_WIDTH, 0,
                  (conf.reader_autoscroll)?((pause)?'\x05':'\x04'):'\x03', MAKE_COLOR(COLOR_BLACK, COLOR_WHITE)); //title infoline
}

//-------------------------------------------------------------------
int gui_read_init(const char* file)
{
    running = 1;

    static struct stat   st;

    read_file = open(file, O_RDONLY, 0777);
    if (strcmp(file, conf.reader_file)!=0) {
        conf.reader_pos = 0;
        strcpy(conf.reader_file, file);
    }
    read_on_screen = 0;
    read_file_size = (read_file>=0 && stat((char*)file, &st)==0)?st.st_size:0;
    if (read_file_size<=conf.reader_pos) {
        conf.reader_pos = 0;
    }
    pause = 0;
    read_to_draw = 1;
    x=camera_screen.disp_left+6; 
    y=FONT_HEIGHT;
    w=camera_screen.disp_width-6-6-8;
    h=camera_screen.height-y;
    last_time = get_tick_count();
    
	reader_is_active=1;    
    old_mode = gui_set_mode(&GUI_MODE_READ);

    return (read_file >= 0);
}

//-------------------------------------------------------------------
static void read_goto_next_line() {
    twoColors col = user_color(conf.reader_color);
    draw_rectangle(xx, yy, x+w-1, yy+rbf_font_height()-1, MAKE_COLOR(BG_COLOR(col), BG_COLOR(col)), RECT_BORDER0|DRAW_FILLED);
    xx  = x;
    yy += rbf_font_height();
}

//-------------------------------------------------------------------
static int read_fit_next_char(int ch) {
    return (xx+rbf_char_width(ch) < x+w);
}

//-------------------------------------------------------------------
void gui_read_draw(int force_redraw)
{
    if (do_not_draw)
        return;
    busy_drawing = 1;
    twoColors col = user_color(conf.reader_color);

    static int first_draw = 1;
    if (first_draw || force_redraw)
    {
        if (first_draw)
        {
            // font has to be loaded from here (SpyTask)
            rbf_load_from_file(conf.reader_rbf_file, conf.reader_codepage);
        }
        draw_rectangle(camera_screen.disp_left, 0,
                       camera_screen.disp_right, y-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
        draw_rectangle(camera_screen.disp_left, y,
                       camera_screen.disp_right, camera_screen.height-1, MAKE_COLOR(BG_COLOR(col), BG_COLOR(col)), RECT_BORDER0|DRAW_FILLED);
        gui_read_draw_scroll_indicator();
        read_to_draw = 1;
        first_draw = 0;
    }

    if (conf.reader_autoscroll && !pause && get_tick_count()-last_time >= conf.reader_autoscroll_delay*1000 && (conf.reader_pos+read_on_screen)<read_file_size)
    {
        conf.reader_pos += read_on_screen;
        read_to_draw = 1;
    }
    if (read_to_draw)
    {
        int n, m, i, ii, ll, new_word=1;

        xx=x; yy=y;

        lseek(read_file, conf.reader_pos, SEEK_SET);
        read_on_screen=0;

        while (yy<=y+h-rbf_font_height()) {
            n=read(read_file, buffer, READ_BUFFER_SIZE);
            if (n==0) {
                 read_goto_next_line();
                 if (yy < y+h)
                     draw_rectangle(x, yy, x+w-1, y+h-1, MAKE_COLOR(BG_COLOR(col), BG_COLOR(col)), RECT_BORDER0|DRAW_FILLED);
                 break;
            }
            else if (n<0) {
                // read failed
                do_not_draw = 1;
                busy_drawing = 0;
                return;
            }
            i=0;
            while (i<n && yy<=y+h-rbf_font_height()) {
                switch (buffer[i]) {
                    case '\r':
                        new_word = 1;
                        break;
                    case '\n':
                        read_goto_next_line();
                        new_word = 1;
                        break;
                    case '\t':
                        buffer[i] = ' ';
                        // no break here
                    default:
                        if (conf.reader_wrap_by_words) {
                            if (buffer[i] == ' ') {
                                new_word = 1;
                                if (xx==x) //ignore leading spaces
                                    break;
                            } else if (new_word) {
                                new_word = 0;
                                for (ii=i, ll=0; ii<n && buffer[ii]!=' ' && buffer[ii]!='\t' && buffer[ii]!='\r' && buffer[ii]!='\n'; ++ii) {
                                    ll+=rbf_char_width(buffer[ii]);
                                }
                                if (ii==n) {
                                    memcpy(buffer, buffer+i, n-i);
                                    n=ii=n-i;
                                    read_on_screen+=i;
                                    i=0;
                                    m=read(read_file, buffer+n, READ_BUFFER_SIZE-n);
                                    if (m<0) {
                                        // read failed
                                        do_not_draw = 1;
                                        busy_drawing = 0;
                                        return;
                                    }
                                    n+=m;
                                    for (; ii<n && buffer[ii]!=' ' && buffer[ii]!='\t' && buffer[ii]!='\r' && buffer[ii]!='\n'; ++ii) {
                                        ll+=rbf_char_width(buffer[ii]);
                                    }
                                }
                                if (xx+ll>=x+w && ll<w) {
                                    read_goto_next_line();
                                    continue;
                                }
                            }
                        }
                        if (!read_fit_next_char(buffer[i])) {
                            read_goto_next_line();
                            continue;
                        }
                        xx+=rbf_draw_char(xx, yy, buffer[i], col);
                        break;
                }
                ++i;
                if (xx >= x+w) {
                    xx  = x;
                    yy += rbf_font_height();
                }
            }
            read_on_screen+=i;
        }
    
        sprintf(buffer, "(%3d%%) %d/%d", (read_file_size)?(conf.reader_pos*100/read_file_size):0, conf.reader_pos, read_file_size);
        draw_string_justified(camera_screen.disp_left, 0, buffer, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE), 0, 20*FONT_WIDTH, TEXT_LEFT|TEXT_FILL); //title infoline

        // scrollbar
        if (read_file_size) {
            i=h-1 -1;           // full height
            n=i*read_on_screen/read_file_size;           // bar height
            if (n<20) n=20;
            i=(i-n)*conf.reader_pos/read_file_size;   // top pos
            draw_rectangle(x+w+6+2, y+1,   x+w+6+6, y+1+i,   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_rectangle(x+w+6+2, y+i+n, x+w+6+6, y+h-1-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_rectangle(x+w+6+2, y+1+i, x+w+6+6, y+i+n,   MAKE_COLOR(COLOR_WHITE, COLOR_WHITE), RECT_BORDER0|DRAW_FILLED);
        } else {
            draw_rectangle((x+w)*FONT_WIDTH+2, y*FONT_HEIGHT+1,
                           (x+w)*FONT_WIDTH+6, (y+h)*FONT_HEIGHT-1-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
        }

        read_to_draw = 0;
        last_time = get_tick_count();
    }
    gui_read_draw_batt();
    gui_read_draw_clock();
    busy_drawing = 0;
}

//-------------------------------------------------------------------
int gui_read_kbd_process() {
    switch (kbd_get_autoclicked_key() | get_jogdial_direction()) {
        case JOGDIAL_LEFT:
        case KEY_ZOOM_OUT:
        case KEY_UP:
        case KEY_LEFT:
            if (conf.reader_pos>0) {
                conf.reader_pos -= 45*15;
                if (conf.reader_pos<0) conf.reader_pos=0;
                read_to_draw = 1;
            }
            break;
        case JOGDIAL_RIGHT:
        case KEY_ZOOM_IN:
        case KEY_DOWN:
        case KEY_RIGHT:
        case KEY_SHOOT_HALF:
            if ((conf.reader_pos+read_on_screen)<read_file_size) {
                conf.reader_pos += read_on_screen;
                read_to_draw = 1;
            }
            break;
        case KEY_SET:
            break;
        case KEY_DISPLAY:
            pause = !pause;
            gui_read_draw_scroll_indicator();
            last_time = get_tick_count();
            break;
        case KEY_MENU:
			gui_read_kbd_leave();
            break;
    }
    return 0;
}

// =========  MODULE INIT =================

//-------------------------------------------------------------------
// Menu button handled for text reader
void gui_read_kbd_process_menu_btn()
{
    gui_read_kbd_process();
    gui_set_mode(old_mode);
}

void gui_read_kbd_leave()
{
    if ( !reader_is_active )
        return;

    reader_is_active = 0;

    do_not_draw = 1;

    while (busy_drawing)
    {
        msleep(10);
    }

    if (read_file >= 0)
    {
        close(read_file);
        read_file=-1;
    }
    rbf_load_from_file(conf.menu_rbf_file, FONT_CP_WIN);

    running = 0;
}

// =========  MODULE INIT =================

/***************** BEGIN OF AUXILARY PART *********************
ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
**************************************************************/

//---------------------------------------------------------
// PURPOSE: Finalize module operations (close allocs, etc)
// RETURN VALUE: 0-ok, 1-fail
//---------------------------------------------------------
int _module_unloader()
{
    // We should make "leave sequence" to restore font settings
    gui_read_kbd_leave();

    return 0;
}

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

libtxtread_sym _libtxtread =
{
    {
         0, _module_unloader, _module_can_unload, _module_exit_alt, 0
    },

    gui_read_init
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    GUI_READ_VERSION,			// Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,			// Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,		// Specify platform dependency

    (int32_t)"Text reader",
    MTYPE_EXTENSION,

    &_libtxtread.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
