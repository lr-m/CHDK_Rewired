#include "camera_info.h"
#include "keyboard.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "gui_grid.h"
#include "fileutil.h"

#include "module_def.h"

//-------------------------------------------------------------------
#define GRID_REDRAW_INTERVAL        4

//-------------------------------------------------------------------
typedef enum {
    GRID_ELEM_LINE,
    GRID_ELEM_RECT,
    GRID_ELEM_FILLED_RECT,
    GRID_ELEM_ELLIPSE,
    GRID_ELEM_FILLED_ELLIPSE,
} grid_elem_type;

typedef struct _gline {
    grid_elem_type      type;
    coord               x0,y0,x1,y1;
    unsigned            clf, clb;
    struct _gline       *next;
} gline;

static int   resize;
static gline *head;
static gline *last;

static int grid_loading = 0;
static int interval = GRID_REDRAW_INTERVAL;

//-------------------------------------------------------------------
static void grid_lines_free_data() {
    while (head) {
        gline* p  = head;
        head = head->next;
        free(p);
    }
}

static void new_grid() {
    grid_lines_free_data();
    head = last = 0;
    resize = (camera_screen.width != 360) || (camera_screen.height != 240);
}

static gline* new_gline() {
    gline* p = malloc(sizeof(gline));
    if (p) {
        memset(p, 0, sizeof(gline));
        if (!head) head = p;
        if (last) last->next = p;
        last = p;
    }
    return p;
}

//-------------------------------------------------------------------
static char* skip_whitespace(char* p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == ',')) p += 1; // skip whitespace or comma
    return p;
}

static char* skip_to_eol(char* p) {
    while (*p && *p != '\n') p += 1; // skip to eol
    if (*p) p += 1;
    return p;
}

static char* skip_token(char* p) {
    while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n') p += 1;
    return p;
}

//-------------------------------------------------------------------
static void process_title(char *ptr) {
    register int i=0;

    ptr = skip_whitespace(ptr);
    while (i<(int)(sizeof(conf.grid_title)-1) && ptr[i] && ptr[i]!='\r' && ptr[i]!='\n') {
        conf.grid_title[i]=ptr[i];
        ++i;
    }
    conf.grid_title[i]=0;
}

//-------------------------------------------------------------------
// Check for script colors
static int get_color(char*p, char** np) {
    int rv = 0;
    if (strncmp(p,"trans",5) == 0)              rv = 255+1;
    else if (strncmp(p,"black",5) == 0)         rv = 255+2;
    else if (strncmp(p,"white",5) == 0)         rv = 255+3;
    else if (strncmp(p,"red_dark",8) == 0)      rv = 255+5;
    else if (strncmp(p,"red_light",9) == 0)     rv = 255+6;
    else if (strncmp(p,"red",3) == 0)           rv = 255+4;
    else if (strncmp(p,"green_dark",10) == 0)   rv = 255+8;
    else if (strncmp(p,"green_light",11) == 0)  rv = 255+9;
    else if (strncmp(p,"green",5) == 0)         rv = 255+7;
    else if (strncmp(p,"blue_dark",9) == 0)     rv = 255+11;
    else if (strncmp(p,"blue_light",10) == 0)   rv = 255+12;
    else if (strncmp(p,"blue",4) == 0)          rv = 255+10;
    else if (strncmp(p,"grey_trans",10) == 0)   rv = 255+19;
    else if (strncmp(p,"grey_dark",9) == 0)     rv = 255+14;
    else if (strncmp(p,"grey_light",10) == 0)   rv = 255+15;
    else if (strncmp(p,"grey",4) == 0)          rv = 255+13;
    else if (strncmp(p,"yellow_dark",11) == 0)  rv = 255+17;
    else if (strncmp(p,"yellow_light",12) == 0) rv = 255+18;
    else if (strncmp(p,"yellow",6) == 0)        rv = 255+16;
    else if (strncmp(p,"magenta",7) == 0)       rv = 255+20;
    if (rv > 0)
        while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n') p += 1;
    if (np) *np = p;
    return rv;
}

static int get_value(char* p, int* failed, int is_color) {
    int rv = 0;
    int fnd = 0;
    char *np = p;
    rv = strtol(p, &np, 0);
    if (np != p) fnd = 1;
    if (!fnd && is_color) {
        char* np = p;
        rv = get_color(p, &np);
        if (np != p) fnd = 1;
    }
    *failed = !fnd;
    return rv;
}

static void process_element(char *ptr, int n, grid_elem_type type) {
    int i, failed;
    long nums[6];

    for (i=0; i<(int)(sizeof(nums)/sizeof(nums[0])); ++i) {
        if(i<n) {
            ptr = skip_whitespace(ptr);
            nums[i] = get_value(ptr, &failed, i >= 4);
            if (failed) // error
                return;
            ptr = skip_token(ptr);
        } else { // ensure unused args have a predictable value
            nums[i] = 0;
        }
    }

    gline* gptr = new_gline();
   
    if (gptr) {
        gptr->type=type;
        if (resize) {
            // Scale values for later resizing
            nums[0] = ((nums[0] << 14) + 180) / 360;
            nums[1] = ((nums[1] << 14) + 120) / 240;
            nums[2] = ((nums[2] << 14) + 180) / 360;
            nums[3] = ((nums[3] << 14) + 120) / 240;
        }
        gptr->x0=nums[0];  gptr->y0=nums[1];
        gptr->x1=nums[2];  gptr->y1=nums[3];
        gptr->clf=nums[4]; gptr->clb=nums[5];
    }
}

//-------------------------------------------------------------------
static int parse_grid_file(char *ptr, int size)
{
    conf.grid_title[0]=0;

    if (size > 0)
    {
        new_grid();

        while (ptr[0])
        {
            ptr = skip_whitespace(ptr);
            if (ptr[0]=='@')
            {
                if (strncmp("@title", ptr, 6)==0) {
                    ptr+=6;
                    process_title(ptr);
                } else if (strncmp("@line", ptr, 5)==0) {
                    ptr+=5;
                    process_element(ptr, 5, GRID_ELEM_LINE);
                } else if (strncmp("@rectf", ptr, 6)==0) {
                    ptr+=6;
                    process_element(ptr, 6, GRID_ELEM_FILLED_RECT);
                } else if (strncmp("@rect", ptr, 5)==0) {
                    ptr+=5;
                    process_element(ptr, 5, GRID_ELEM_RECT);
                } else if (strncmp("@elpsf", ptr, 6)==0) {
                    ptr+=6;
                    process_element(ptr, 5, GRID_ELEM_FILLED_ELLIPSE);
                } else if (strncmp("@elps", ptr, 5)==0) {
                    ptr+=5;
                    process_element(ptr, 5, GRID_ELEM_ELLIPSE);
                }
            }
            ptr = skip_to_eol(ptr);
        }
    }

    return size;
}

//-------------------------------------------------------------------
void grid_lines_load(const char *fn)
{
    grid_loading = 1;
    if (process_file(fn, parse_grid_file, 1) > 0)  // non-zero length file found?
    {
        if (conf.grid_title[0]==0)                 // use filename if no @title string found
        {
            char* c = strrchr(fn, '/');
            strncpy(conf.grid_title, (c)?c+1:fn, sizeof(conf.grid_title));
            conf.grid_title[sizeof(conf.grid_title)-1] = 0;
        }
        strcpy(conf.grid_lines_file, fn);
    }
    grid_loading = 0;
}

//-------------------------------------------------------------------
void gui_grid_draw_osd(int force)
{
    if (camera_info.state.mode_rec_or_review && conf.show_grid_lines)
    {
        int xs=0, xo=0, ys=0, yo=0;
        gline* ptr;
        twoColors ucol = user_color(conf.grid_color);

        if (force || --interval==0) {
            int protect_stats = conf.show_osd &&
                                camera_info.state.gui_mode_none &&
                                !camera_info.state.is_shutter_half_press &&
                                !kbd_is_key_pressed(KEY_SHOOT_HALF) &&
                                !kbd_is_key_pressed(KEY_SHOOT_FULL);
            draw_set_grid_clip(protect_stats);
            if (resize) {
                // Grid aspect ratio is 4:3 - fit to 3:2 or 16:9 as needed and center on screen
                // Note: assumes aspect ratio of screen dimensions matches display aspect ratio
                xs = (camera_screen.height * 4) / 3;
                xo = (camera_screen.width - xs) / 2;
                ys = camera_screen.height;
                yo = 0;
            }
            for (ptr = head; ptr; ptr = ptr->next) {
                // Note: elements expect a twoColors even if only one color arg is accepted
                // in this case clb will be 0
                twoColors col =
#ifdef CAM_GRID_ALWAYS_USER_COLOR
                    ucol;
#else
                    (conf.grid_force_color) ? ucol : MAKE_COLOR(get_script_color(ptr->clb), get_script_color(ptr->clf));
#endif
                int x0 = ptr->x0;
                int y0 = ptr->y0;
                int x1 = ptr->x1;
                int y1 = ptr->y1;
                if (resize) {
                    x0 = ((x0 * xs + 8192) >> 14) + xo;
                    y0 = ((y0 * ys + 8192) >> 14) + yo;
                    x1 = ((x1 * xs + 8192) >> 14);
                    y1 = ((y1 * ys + 8192) >> 14);
                }
                switch (ptr->type) {
                    case GRID_ELEM_LINE:
                        draw_line(x0, y0, x1+xo, y1+yo, col);
                        break;
                    case GRID_ELEM_RECT:
                        draw_rectangle(x0, y0, x1+xo, y1+yo, col, RECT_BORDER1);
                        break;
                    case GRID_ELEM_FILLED_RECT:
                        draw_rectangle(x0, y0, x1+xo, y1+yo, col, RECT_BORDER1|DRAW_FILLED);
                        break;
                    case GRID_ELEM_ELLIPSE:
                        draw_ellipse(x0, y0, (unsigned int)x1, (unsigned int)y1, col, 0);
                        break;
                    case GRID_ELEM_FILLED_ELLIPSE:
                        draw_ellipse(x0, y0, (unsigned int)x1, (unsigned int)y1, col, DRAW_FILLED);
                        break;
                }
            }
            draw_set_grid_clip(0);
            interval = GRID_REDRAW_INTERVAL;
        }
    }
}

// =========  MODULE INIT =================

/***************** BEGIN OF AUXILARY PART *********************
  ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
 **************************************************************/

//---------------------------------------------------------
// PURPOSE:   Perform on-load initialisation
// RETURN VALUE: 1 error, 0 ok
//---------------------------------------------------------
int _module_loader( __attribute__ ((unused))unsigned int* chdk_export_list )
{
    grid_lines_load(conf.grid_lines_file);
    return 0;
}

//---------------------------------------------------------
// PURPOSE: Finalize module operations (close allocs, etc)
// RETURN VALUE: 0-ok, 1-fail
//---------------------------------------------------------
int _module_unloader()
{
    grid_lines_free_data();
    return 0;
}

int _module_can_unload()
{
    return (conf.show_grid_lines == 0 && !grid_loading);
}

/******************** Module Information structure ******************/

libgrids_sym _libgrids =
{
    {
         _module_loader, _module_unloader, _module_can_unload, 0, 0
    },

    gui_grid_draw_osd,
    grid_lines_load
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    GUI_GRID_VERSION,			// Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,			// Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,		// Specify platform dependency

    (int32_t)"Grids (dll)",
    MTYPE_EXTENSION,            //Grid Display

    &_libgrids.base,

    CONF_VERSION,               // CONF version
    ANY_VERSION,                // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
