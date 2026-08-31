#include "camera_info.h"
#include "keyboard.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "conf.h"

#include "module_def.h"
#include "simple_module.h"

//-------------------------------------------------------------------
void gui_debug_draw();

//-------------------------------------------------------------------

void gui_module_menu_kbd_process();
int gui_debug_kbd_process();

gui_handler GUI_MODE_DEBUG = 
/*GUI_MODE_DEBUG*/  { GUI_MODE_MODULE, gui_debug_draw, gui_debug_kbd_process, gui_module_menu_kbd_process, 0, 0 };

//-------------------------------------------------------------------
static int running = 0;
static void *addr;
static char debug_to_draw;
static char debug_cont_update;
static char buf[32];
static char *bad_address = "Invalid Address               ";
static unsigned int step;

//-------------------------------------------------------------------
void gui_debug_init(void *st_addr)
{
    running = 1;
    addr = st_addr;
    debug_to_draw = 1;
    debug_cont_update = 1;
    step = 4;
    gui_set_mode(&GUI_MODE_DEBUG);
}

//-------------------------------------------------------------------
static void gui_debug_draw_values(const coord y, void* addr) {
    int i;

    if (addr<=(void*)camera_info.maxramaddr || (addr>=(void*)camera_info.rombaseaddr && (unsigned)addr <= 0xfffffffc))
    {
        sprintf(buf, "0x%08X (%10u)", *((unsigned int*)addr), *((unsigned int*)addr));
        draw_string(10*FONT_WIDTH, y, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

        sprintf(buf, "0x%04X     (     %5hu)", *((unsigned short*)addr), *((unsigned short*)addr));
        draw_string(10*FONT_WIDTH, y+FONT_HEIGHT, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

        sprintf(buf, "0x%02X       (       %3hu)", *((unsigned char*)addr), *((unsigned char*)addr));
        draw_string(10*FONT_WIDTH, y+2*FONT_HEIGHT, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

        for (i=0; i<30; ++i) {
            if (*((char*)(addr+i))) buf[i]=*((char*)(addr+i));
            else break;
        }
        while (i<30) {
            buf[i++]=' ';
        }
        buf[i]=0;
        draw_string(10*FONT_WIDTH, y+3*FONT_HEIGHT, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    }
    else
    {
        draw_string(10*FONT_WIDTH, y, bad_address, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        draw_string(10*FONT_WIDTH, y+FONT_HEIGHT, bad_address, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        draw_string(10*FONT_WIDTH, y+2*FONT_HEIGHT, bad_address, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        draw_string(10*FONT_WIDTH, y+3*FONT_HEIGHT, bad_address, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    }
}

//-------------------------------------------------------------------
void gui_debug_draw() {
    switch (debug_to_draw) {
        case 1:
            draw_rectangle(camera_screen.disp_left, 0, camera_screen.disp_right, camera_screen.height-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(1*FONT_WIDTH, 0*FONT_HEIGHT, "Address:", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(22*FONT_WIDTH,0*FONT_HEIGHT, "Incr:", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(39*FONT_WIDTH,0*FONT_HEIGHT, "Auto:", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(1*FONT_WIDTH, 1*FONT_HEIGHT, "Values:",  MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 2*FONT_HEIGHT, "DWORD  :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 3*FONT_HEIGHT, "WORD   :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 4*FONT_HEIGHT, "BYTE   :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 5*FONT_HEIGHT, "STRING :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(1*FONT_WIDTH, 7*FONT_HEIGHT, "Dereferencing:",  MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 8*FONT_HEIGHT, "DWORD  :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 9*FONT_HEIGHT, "WORD   :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 10*FONT_HEIGHT,"BYTE   :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_string(2*FONT_WIDTH, 11*FONT_HEIGHT,"STRING :", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

            debug_to_draw = 2;
            break;
        case 2:
            sprintf(buf, "0x%08X", addr);
            draw_string(10*FONT_WIDTH, 0, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            sprintf(buf, "0x%08X", step);
            draw_string(28*FONT_WIDTH, 0, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            sprintf(buf,"%0d",debug_cont_update);
            draw_string(44*FONT_WIDTH, 0, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            gui_debug_draw_values(2*FONT_HEIGHT, addr);
            if (addr<=(void*)camera_info.maxramaddr || (addr>=(void*)camera_info.rombaseaddr && (unsigned)addr <= 0xfffffffc))
                gui_debug_draw_values(8*FONT_HEIGHT, *((void**)addr));
            else
                gui_debug_draw_values(8*FONT_HEIGHT, addr);
            conf.mem_view_addr_init = (long)addr;

            if (debug_cont_update==0) debug_to_draw = 0;
            break;
        default:
            debug_to_draw = 0;
            break;
    }
}

//-------------------------------------------------------------------
int gui_debug_kbd_process() {
    switch (kbd_get_autoclicked_key()) {
    case KEY_DOWN:
        break;
    case KEY_UP:
        debug_cont_update = !debug_cont_update;
        debug_to_draw = 2;
        break;
    case KEY_LEFT:
        addr-=step;
        debug_to_draw = 2;
        break;
    case KEY_RIGHT:
        addr+=step;
        debug_to_draw = 2;
        break;
    case KEY_DISPLAY:
    case KEY_SHOOT_HALF:
        switch (step) {
            case 0x00000004: step = 0x00000010; break;
            case 0x10000000: step = 0x00000004; break;
            default: step<<=4; break;
        }
        debug_to_draw = 2;
        break;
    case KEY_ZOOM_IN:    // reduce step
        switch (step) {
            case 0x00000010: step = 0x0000004; break;
            case 0x00000004: break;
            default: step>>=4; break;
        }
        debug_to_draw = 2;
        break;
    case KEY_ZOOM_OUT:    // increase step
        switch (step) {
            case 0x00000004: step = 0x00000010; break;
            case 0x10000000: break;
            default: step<<=4; break;
        }
        debug_to_draw = 2;
        break;
    }
    return 0;
}

//-------------------------------------------------------------------

void gui_module_menu_kbd_process()
{
    running = 0;
    gui_default_kbd_process_menu_btn();
}


// =========  MODULE INIT =================

/***************** BEGIN OF AUXILARY PART *********************
ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
**************************************************************/

int _run()
{
    void* adr =(char*)(conf.mem_view_addr_init);
    gui_debug_init(adr);

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
    SIMPLE_MODULE_VERSION,		// Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,			// Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,		// Specify platform dependency

    -LANG_MENU_DEBUG_MEMORY_BROWSER,
    MTYPE_TOOL,             //Simple memory content browser

    &_librun.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    CAM_INFO_VERSION,           // CAM INFO version

    0x2a,                       // Menu symbol
};

/*************** END OF AUXILARY PART *******************/
