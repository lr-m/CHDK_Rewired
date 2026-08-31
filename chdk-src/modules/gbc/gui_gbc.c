//-------------------------------------------------------------------
// Game Boy / Game Boy Color player - the CHDK module.
//
// Appears under Miscellaneous -> Games as "Game Boy". Lists whatever is in
// A/CHDK/GBC, runs it, and gets out of the way.
//
// There is no separate "camera filter" entry, deliberately. The cartridge
// type in the ROM header decides what the machine is: drop the Game Boy
// Camera in and the emulated cartridge quietly starts pulling frames off the
// Canon sensor (gbc_feed.c), so taking a picture inside the ROM photographs
// whatever the lens is pointed at. That is the same thing the real hardware
// does, and it needs no menu of its own.
//
// See docs/GBC_PORT.md.
//-------------------------------------------------------------------

#include "camera_info.h"
#include "keyboard.h"
#include "lang.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "gui_mbox.h"
#include "modes.h"
#include "dirent.h"

#include "module_def.h"
#include "simple_module.h"

#include "gbc_port.h"
#include "gbc_emu.h"

int  gui_gbc_kbd_process();
void gui_gbc_draw();
static void gui_gbc_menu_kbd_process();

gui_handler GUI_MODE_GBC =
    /*GUI_MODE_GBC*/ { GUI_MODE_MODULE, gui_gbc_draw, gui_gbc_kbd_process,
                       gui_gbc_menu_kbd_process, 0, GUI_MODE_FLAG_NODRAWRESTORE };

//-------------------------------------------------------------------

#define GBC_MAX_ROMS    24
#define GBC_NAME_LEN    40

static int  running = 0;
static int  redraw  = 1;

// Raised by gui_gbc_menu_kbd_process() in the keyboard task, acted on by
// gui_gbc_draw() in spytask. See the comment on gui_gbc_menu_kbd_process().
static volatile int quit_request = 0;

enum { ST_PICKER, ST_RUNNING, ST_ERROR };
static int state = ST_PICKER;

static char rom_name[GBC_MAX_ROMS][GBC_NAME_LEN];
static int  rom_count = 0;
static int  rom_sel   = 0;

static gbc_emu_t emu;
static char message[64];
// Off by default now that the port runs on hardware. DISP still cycles it back
// on: off -> HUD -> HUD with the rasteriser bypassed. The HUD sits at y=2 and
// the Game Boy screen now starts at y=0, so the two overlap - turning it off
// invalidates the shadow buffer to repaint what it covered.
static int  show_hud = 0;

//-------------------------------------------------------------------

static int has_gb_ext(const char *n)
{
    int len = strlen(n);
    if (len < 4)
        return 0;

    // ".gb" or ".gbc", either case. strcasecmp is not available here.
    if (n[len-3] == '.' &&
        (n[len-2] == 'g' || n[len-2] == 'G') &&
        (n[len-1] == 'b' || n[len-1] == 'B'))
        return 1;

    if (len >= 5 && n[len-4] == '.' &&
        (n[len-3] == 'g' || n[len-3] == 'G') &&
        (n[len-2] == 'b' || n[len-2] == 'B') &&
        (n[len-1] == 'c' || n[len-1] == 'C'))
        return 1;

    return 0;
}

static void scan_roms(void)
{
    DIR *d;
    struct dirent *e;

    rom_count = 0;
    rom_sel = 0;

    d = opendir(GBC_ROM_DIR);
    if (!d)
        return;

    while ((e = readdir(d)) != 0 && rom_count < GBC_MAX_ROMS)
    {
        if (e->d_name[0] == '.')
            continue;
        if (!has_gb_ext(e->d_name))
            continue;

        strncpy(rom_name[rom_count], e->d_name, GBC_NAME_LEN - 1);
        rom_name[rom_count][GBC_NAME_LEN - 1] = 0;
        rom_count++;
    }

    closedir(d);
}

//-------------------------------------------------------------------

static void start_selected(void)
{
    char path[160];

    strcpy(path, GBC_ROM_DIR);
    strcat(path, "/");
    strcat(path, rom_name[rom_sel]);

    if (gbc_emu_open(&emu, path))
    {
        state = ST_ERROR;
        // gbc_emu_open leaves a reason behind - almost always memory. The
        // camera cartridge asks for ~330K (cartridge SRAM, sensor buffers,
        // ROM cache) and an ordinary MBC title for far less, so the two fail
        // in different places and the distinction is worth showing.
        strncpy(message, gbc_open_error[0] ? gbc_open_error : "Could not load ROM",
                sizeof(message) - 1);
        message[sizeof(message) - 1] = 0;
        redraw = 1;
        return;
    }

    state = ST_RUNNING;
    redraw = 1;
}

//-------------------------------------------------------------------

void gui_gbc_draw()
{
    if (state == ST_RUNNING)
    {
        // The MENU button, arriving from the keyboard task. Torn down here
        // rather than there because everything below - and everything
        // gbc_emu_close() frees - belongs to this task.
        if (quit_request)
        {
            quit_request = 0;
            gbc_emu_close(&emu);
            state = ST_PICKER;
            redraw = 1;
            return;
        }

        // One frame per draw callback. gbc_emu_frame() does its own blit
        // straight into the bitmap buffer, so nothing else is drawn here -
        // the picker's text would otherwise be composited over the game.
        int r = gbc_emu_frame(&emu);

        // DISP toggles a diagnostic line over the top of the game. Worth the
        // few hundred bytes: a black screen and a responsive camera is what
        // both "the CPU is wedged" and "this is running at a frame a minute"
        // look like from the outside, and they need completely different
        // fixes. insn is instructions retired in the last frame (~17500 is a
        // real frame's worth), GUARD means vblank was never reached, and
        // hit/miss is the ROM pager - a miss is a 16K read off the SD card,
        // so a four-figure miss count per frame is the whole answer on its own.
        if (show_hud && emu.open)
        {
            static char line[64];
            gbc_hud_line(&emu, line, sizeof(line));
            draw_string(2, 2, line, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        }

        if (r == 1)                     // player pressed MENU
        {
            gbc_emu_close(&emu);
            state = ST_PICKER;
            redraw = 1;
        }
        else if (r < 0)                 // fault, unwound from the core
        {
            gbc_emu_close(&emu);
            state = ST_ERROR;
            strncpy(message, gbc_panic_msg[0] ? gbc_panic_msg : "Emulator fault",
                    sizeof(message) - 1);
            message[sizeof(message) - 1] = 0;
            redraw = 1;
        }
        return;
    }

    if (!redraw)
        return;
    redraw = 0;

    draw_rectangle(0, 0, 359, 239, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK),
                   RECT_BORDER0 | DRAW_FILLED);

    if (state == ST_ERROR)
    {
        draw_string(20, 100, message, MAKE_COLOR(COLOR_BLACK, COLOR_RED));
        draw_string(20, 130, "MENU to go back", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        return;
    }

    draw_string(20, 12, "Game Boy", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

    if (rom_count == 0)
    {
        draw_string(20, 60,  "No ROMs found in", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        draw_string(20, 85,  GBC_ROM_DIR, MAKE_COLOR(COLOR_BLACK, COLOR_YELLOW));
        draw_string(20, 120, "Copy .gb / .gbc files there.",
                    MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        draw_string(20, 200, "MENU to exit", MAKE_COLOR(COLOR_BLACK, COLOR_GREY));
        return;
    }

    {
        int i;
        for (i = 0; i < rom_count; i++)
        {
            int y = 45 + i * 18;
            if (y > 195)
                break;
            draw_string(30, y, rom_name[i],
                        (i == rom_sel) ? MAKE_COLOR(COLOR_WHITE, COLOR_BLACK)
                                       : MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
        }
    }

    draw_string(20, 210, "SET play    MENU exit", MAKE_COLOR(COLOR_BLACK, COLOR_GREY));
}

//-------------------------------------------------------------------

int gui_gbc_kbd_process()
{
    if (state == ST_RUNNING)
    {
        // The emulator polls the keyboard itself inside gbc_emu_frame(), so
        // keys are otherwise swallowed here. MENU is handled there too, as a
        // quit rather than a mode change, which is why it is not special-cased.
        //
        // DISP is the exception: it is B to the emulator and the HUD toggle
        // here. Sharing it is deliberate - there is no spare key on this body,
        // and a diagnostic overlay is not worth spending one of the eight the
        // Game Boy needs.
        // DISP cycles: off -> HUD -> HUD with the rasteriser bypassed.
        // The third state is the measurement that says whether speed work
        // belongs in the interpreter or in the Game Boy's scanline renderer:
        // the emulated CPU runs identically either way, so the change in
        // "emu=" is exactly what rendering costs. The picture freezes while
        // it is on, which is expected.
        if (kbd_get_autoclicked_key() == KEY_DISPLAY)
        {
            if (!show_hud)              { show_hud = 1; gbc_render_user = 1; }
            else if (gbc_render_user)   { gbc_render_user = 0; }
            else                        { show_hud = 0; gbc_render_enabled = 1;
                                          gbc_invalidate_screen(); }
        }

        // Blocked from Canon, all of it, for as long as a game is up.
        //
        // This used to return 0, which passes every key through to the
        // firmware after the emulator has read it - and the firmware has its
        // own ideas about most of them. UP and DOWN are the zoom lever on this
        // body, so the d-pad drove the lens and Canon drew its zoom bar over
        // the Game Boy screen; the shutter started an actual capture, which is
        // the Canon live view and the Canon OSD taking the screen back.
        //
        // The emulator reads the keys itself in gui_input_poll(), so nothing
        // is lost by blocking them here - and the lens is still reachable,
        // deliberately, through lens mode.
        return 1;
    }

    switch (kbd_get_autoclicked_key())
    {
    case KEY_UP:
        if (rom_sel > 0) { rom_sel--; redraw = 1; }
        break;
    case KEY_DOWN:
        if (rom_sel < rom_count - 1) { rom_sel++; redraw = 1; }
        break;
    case KEY_SET:
        if (state == ST_ERROR) { state = ST_PICKER; redraw = 1; }
        else if (rom_count > 0) start_selected();
        break;
    }

    return 0;
}

//-------------------------------------------------------------------

// The MENU button. This runs in the keyboard task; gui_gbc_draw() runs in
// spytask, and while a game is up that is a task sitting inside
// gbc_emu_frame().
//
// So a running emulator is never torn down from here. It used to be, and that
// is what took the camera down on the way out of the Game Boy Camera: this
// handler called gbc_emu_close() - freeing the machine state, the ROM pager and
// the cartridge - while spytask was part way through emulating a frame out of
// exactly those allocations. gbc_emu_frame()'s own MENU check (special_quit)
// closes it a second time from the other side, so the same press ran the
// teardown twice, from two tasks, with no ordering between them.
//
// It showed up on the camera cartridge rather than on an ordinary ROM because
// gbc_emu_close() flushes the album first: up to GBC_PHOTO_SLOTS files written
// to the card, synchronously, which both widens the window to seconds and
// blocks the keyboard task for the whole of it - the thing gbc_emu_frame()'s
// closing comment warns gets the camera switched off by DryOS.
//
// Raising a flag instead leaves one teardown, in the task that owns the
// buffers. In the picker there is no frame in flight and nothing to close, so
// MENU leaves the module as it always did.
//
// gui_module_menu_kbd_process() would be the natural thing to call and is what
// the stock games use, but it is not in modules/exportlist.inc;
// gui_default_kbd_process_menu_btn() is, and does the same job.
static void gui_gbc_menu_kbd_process()
{
    if (state == ST_RUNNING)
    {
        quit_request = 1;
        return;
    }

    if (emu.open)
        gbc_emu_close(&emu);
    running = 0;
    gui_default_kbd_process_menu_btn();
}

//-------------------------------------------------------------------
// Module glue.

int _run()
{
    running = 1;
    state = ST_PICKER;
    redraw = 1;
    quit_request = 0;
    message[0] = 0;
    gbc_panic_msg[0] = 0;

    scan_roms();

    gui_set_mode(&GUI_MODE_GBC);
    return 0;
}

int _module_unloader()
{
    if (emu.open)
        gbc_emu_close(&emu);
    return 0;
}

int _module_can_unload()
{
    return running == 0;
}

// Leaving <ALT> with a game still up. Called from module_exit_alt() in
// gui_activate_alt_mode(), which is spytask - the same task as gui_gbc_draw() -
// so closing directly is safe here in a way it is not in the keyboard handler
// above.
int _module_exit_alt()
{
    if (emu.open)
        gbc_emu_close(&emu);
    state = ST_PICKER;
    quit_request = 0;
    running = 0;
    return 0;
}

libsimple_sym _librun =
{
    {
        0, _module_unloader, _module_can_unload, _module_exit_alt, _run
    }
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SIMPLE_MODULE_VERSION,

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,
    ANY_PLATFORM_ALLOWED,

    (int32_t)"Game Boy",
    // Not MTYPE_GAME. The Games menu is built by scanning MODULES for that
    // type, so anything carrying it lands there; this one is launched from its
    // own entry on the root menu instead (gui.c, module_run("gbc.flt")).
    MTYPE_EXTENSION,

    &_librun.base,

    ANY_VERSION,
    CAM_SCREEN_VERSION,
    ANY_VERSION,
    ANY_VERSION,

    0,
};
