#include "camera_info.h"
#include "gui.h"
#include "gui_draw.h"
#include "meminfo.h"
#include "module_load.h"
#include "simple_module.h"
#include "clock.h"
#include "cachebit.h"
#include "exmem.h"
#include "time.h"

// =========  MODULE INIT =================

static int running = 0;
static int sampling = 0;
static int shandle = 0; // HP timer handle

extern int basic_module_init();

/***************** BEGIN OF AUXILARY PART *********************
  ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
 **************************************************************/

int _run()
{
    basic_module_init();

    return 0;
}

int _module_can_unload()
{
    return (running==0 && shandle==0 && sampling==0);
}

int _module_exit_alt()
{
    //running = 0;
    return 0;
}

/******************** Module Information structure ******************/

libsimple_sym _librun =
{
    {
         0, 0, _module_can_unload, _module_exit_alt, _run
    }
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    {1,0},      // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,           // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int32_t)"EXMEM Inspector",
    MTYPE_TOOL,                 // Show list of EXMEM allocations

    &_librun.base,

    ANY_VERSION,                // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
//-------------------------------------------------------------------

#ifndef THUMB_FW
int sampling_proc_ARM(int, int);

void __attribute__((naked,noinline)) dummy_function1(void) {
    asm volatile (
    ".code 32\n"
"sampling_proc_ARM:\n"
    "adr    r3, sapr_thumb+1\n"
    "bx     r3\n"
    ".code 16\n"
"sapr_thumb:\n"
    "push   {lr}\n"
    "bl     sampling_proc\n"
    "pop    {pc}\n"
    );
}
#endif

typedef struct
{
    int            tim;
    unsigned int   typ;
    char          *adr;
    unsigned int   len;
} exmemlog_s;

#define LOG_LEN 128
static exmemlog_s exl[LOG_LEN];
static int exlpnt = 0;
static unsigned int extypcnt = 0;
static exmem_alloc_info exm_prev[42]; // should be enough
static exmem_alloc_info eaitmp;
static int speriod = 2000; // 2000usec

int sampling_proc(__attribute__ ((unused))int ttime, __attribute__ ((unused))int param) {
    // check if sampling is still enabled, return if not
    if (!sampling) {
        shandle = 0;
        return 0;
    }
    // set timer immediately
#ifndef THUMB_FW
    shandle = SetHPTimerAfterNow(speriod, sampling_proc_ARM, sampling_proc_ARM, 0);
#else
    shandle = SetHPTimerAfterNow(speriod, sampling_proc, sampling_proc, 0);
#endif
    // now look for changes
    int gtc = get_tick_count();
    unsigned int n;
    for (n=0; n<extypcnt; n++) {
        get_exmem_type_status(n, &eaitmp);
        if (exm_prev[n].addr!=eaitmp.addr || exm_prev[n].len!=eaitmp.len) {
            if (exlpnt<LOG_LEN) {
                exl[exlpnt].tim = gtc;
                exl[exlpnt].typ = n;
                exl[exlpnt].adr = eaitmp.addr;
                exl[exlpnt].len = eaitmp.len;
                exlpnt++;
            }
        }
        exm_prev[n].addr=eaitmp.addr;
        exm_prev[n].len=eaitmp.len;
    }
    return 0;
}

//-------------------------------------------------------------------
void write_log()
{
    FILE *f = NULL;
    char txt[80], name[18];
    int n = 0;
    do {
        sprintf(txt, "A/CHDK/LOGS/EXM_%04d.LOG", n++);
        if (stat(txt,0) != 0) {
            f=fopen(txt, "wb");
            break;
        }
    } while(n<9999);

    if (!f)
        return;

    struct tm *tms;
    time_t t;
    t = time(NULL);
    tms = localtime(&t);
    sprintf(txt,"%04d-%02d-%02d %02d:%02d:%02d\n",tms->tm_year+1900,tms->tm_mon+1,tms->tm_mday,
                                                  tms->tm_hour,tms->tm_min,tms->tm_sec);
    fwrite(txt,1,strlen(txt),f);
    sprintf(txt,"%s, %s\n\n",camera_info.platform,camera_info.platformsub);
    fwrite(txt,1,strlen(txt),f);
    sprintf(txt,"TIME,TYPE,ADDRESS,SIZE\n");
    fwrite(txt,1,strlen(txt),f);
    for (n=0; n<LOG_LEN; n++)
    {
        char *s = get_exmem_type_name(exl[n].typ);
        if (!s)
        {
            strcpy(name, "-");
        }
        else
        {
            s += 6; // chop off 'EXMEM_'
            strncpy(name, s, 17); name[17] = 0;
        }
        if (exl[n].tim)
        {
            sprintf(txt,"%d,%s,%08x,%08x\n", exl[n].tim, name, (unsigned int)exl[n].adr, exl[n].len);
            fwrite(txt,1,strlen(txt),f);
        }
    }
    fclose(f);
}

/*************** GUI MODULE *******************/

#include "gui_mbox.h"
#include "keyboard.h"

void gui_exmem_menu_kbd_process();
int gui_exmem_kbd_process();
void gui_exmem_draw();

gui_handler GUI_MODE_EXMEM_INSPECTOR = 
/*GUI_MODE_EXMEM_INSPECTOR*/   { GUI_MODE_MODULE, gui_exmem_draw, gui_exmem_kbd_process, gui_exmem_menu_kbd_process, 0, 0 };

static unsigned int log_disp_start = 0;
static int exmeminspect_mode = 0;
static int exmeminspect_column = 0;
static int disph = 0;
int exmeminspect_redraw;
gui_handler *exmeminspect_old_guimode;

int gui_exmem_kbd_process()
{
    switch (kbd_get_autoclicked_key())
    {
    case KEY_SET:
        sampling = !sampling;
        if (sampling) {
            // memset, reset, etc
            memset(exm_prev, 0, sizeof(exmem_alloc_info)*42);
            memset(exl, 0, sizeof(exmemlog_s)*LOG_LEN);
            exlpnt = 0;
        }
        sampling_proc(0,0); // TODO: could execute parallel to timer
        exmeminspect_redraw = 2;
        break;
    case KEY_UP:
        if (log_disp_start > 0) log_disp_start--;
        exmeminspect_redraw = 1;
        break;
    case KEY_DOWN:
        if (log_disp_start < LOG_LEN-1) log_disp_start++;
        exmeminspect_redraw = 1;
        break;
    case KEY_SHOOT_HALF:
        exmeminspect_mode = exmeminspect_mode==0?1:0;
        exmeminspect_redraw = 2;
        break;
    case KEY_LEFT:
        if (exlpnt>0)
            write_log();
        exmeminspect_redraw = 2;
        break;
    case KEY_RIGHT:
        exmeminspect_column = exmeminspect_column==0?1:0;
        exmeminspect_redraw = 2;
        break;
    }
    return 0;
}

//-------------------------------------------------------------------

int basic_module_init()
{
    if (!running)
    {
        running = 1;
        extypcnt = exmem_type_count;
        sampling = 0;
        shandle = 0;
        disph = camera_screen.height / FONT_HEIGHT;
    }
    exmeminspect_redraw = 2;
    exmeminspect_old_guimode = gui_set_mode(&GUI_MODE_EXMEM_INSPECTOR);
    return 1;
}

void gui_exmem_menu_kbd_process()
{
    running = 0;
    sampling = 0;
    if (shandle) {
        CancelHPTimer(shandle);
        shandle = 0;
    }

    gui_set_mode(exmeminspect_old_guimode);
}

void gui_exmem_draw()
{
    unsigned int idx, idxmax, showidx;

    if (exmeminspect_redraw) {

        if (exmeminspect_redraw == 2)
        {
            draw_rectangle(camera_screen.disp_left, 0, camera_screen.disp_right, camera_screen.height-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(camera_screen.disp_left, 0, "EXMEM Inspector", MAKE_COLOR(COLOR_WHITE, COLOR_BLACK));
            draw_string(camera_screen.disp_left, FONT_HEIGHT, sampling?"SET-stop  MENU-exit RIGHT-viewmode LEFT-save":"SET-start MENU-exit RIGHT-viewmode LEFT-save", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            if (exmeminspect_mode == 0) {
                draw_string(camera_screen.disp_left+17*FONT_WIDTH, 0, "LIST,  HALFSHOOT->LOG", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                if (exmeminspect_column == 0) {
                    draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Idx Name              Addr      Size    ", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
                else {
                    draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Idx Name              Addr      Top     ", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
            }
            else {
                draw_string(camera_screen.disp_left+17*FONT_WIDTH, 0, "LOG,  HALFSHOOT->LIST", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                if (exmeminspect_column == 0) {
                    draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Time    Name              Addr     Size    ", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
                else {
                    draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Time    Name              Addr     Top     ", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
            }
        }

        showidx=3*FONT_HEIGHT;
        if (exmeminspect_mode == 0)
        {
            idxmax = extypcnt;
            for ( idx=0; idx<idxmax; idx++)
            {
                char *s = get_exmem_type_name(idx);
                if (!s)
                    continue;
                exmem_alloc_info eai;
                if (!get_exmem_type_status(idx, &eai) || !eai.addr)
                    continue;

                char txt[50], name[18];
                s += 6; // chop off 'EXMEM_'
                strncpy(name, s, 17); name[17] = 0;
                if (exmeminspect_column == 0) {
                    sprintf(txt,"%02d: %-17s %08x  %8x", idx, name, (unsigned int)eai.addr, eai.len);
                }
                else {
                    sprintf(txt,"%02d: %-17s %08x  %08x", idx, name, (unsigned int)eai.addr, (unsigned int)eai.addr+eai.len);
                }
                draw_string(camera_screen.disp_left, showidx, txt, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                showidx += FONT_HEIGHT;
            }
        }
        else
        {
            idx = log_disp_start;
            while (showidx < (unsigned)camera_screen.height)
            {
                if (idx >= LOG_LEN || exl[idx].tim == 0)
                {
                    draw_string(camera_screen.disp_left, showidx, 
                    "                                             ",
                    MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
                else
                {
                    char *s = get_exmem_type_name(exl[idx].typ);
                    if (!s)
                        s = "      -";

                    char txt[50], name[18];
                    s += 6; // chop off 'EXMEM_'
                    strncpy(name, s, 17); name[17] = 0;
                    if (exmeminspect_column == 0) {
                        sprintf(txt,"%06d: %-17s %08x %8x", exl[idx].tim, name, (unsigned int)exl[idx].adr, exl[idx].len);
                    }
                    else {
                        sprintf(txt,"%06d: %-17s %08x %08x", exl[idx].tim, name, (unsigned int)exl[idx].adr, (unsigned int)exl[idx].adr+exl[idx].len);
                    }
                    draw_string(camera_screen.disp_left, showidx, txt, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
                }
                showidx += FONT_HEIGHT;
                idx++;
            }
        }
    }

    exmeminspect_redraw = 0;
}

