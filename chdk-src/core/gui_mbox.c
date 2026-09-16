#include "camera_info.h"
#include "lang.h"
#include "modes.h"
#include "keyboard.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "gui_mbox.h"

//-------------------------------------------------------------------
void gui_mbox_draw();
int gui_mbox_kbd_process();
int gui_mbox_touch_handler(int,int);

static gui_handler mboxGuiHandler = { GUI_MODE_MBOX, gui_mbox_draw, gui_mbox_kbd_process, 0, gui_mbox_touch_handler, GUI_MODE_FLAG_NORESTORE_ON_SWITCH };

static gui_handler      *gui_mbox_mode_old;
static const char*      mbox_title;
static const char*      mbox_msg;
static char             mbox_to_draw;
static unsigned int     mbox_flags;
#define MAX_LINES       8
#define MAX_WIDTH       35
#define SPACING_TITLE   4
#define SPACING_BTN     4
static struct {
        unsigned int    flag;
        int             text;
} buttons[] = {
        { MBOX_BTN_OK,      LANG_MBOX_BTN_OK    },
        { MBOX_BTN_YES,     LANG_MBOX_BTN_YES   },
        { MBOX_BTN_NO,      LANG_MBOX_BTN_NO    },
        { MBOX_BTN_CANCEL,  LANG_MBOX_BTN_CANCEL}
};
#define BUTTON_SIZE     6
#define BUTTONSNUM      (int)(sizeof(buttons)/sizeof(buttons[0]))
#define MAX_BUTTONS     3
static int      mbox_buttons[MAX_BUTTONS], mbox_buttons_num, mbox_button_active;
static coord    mbox_buttons_x, mbox_buttons_y;
#define BUTTON_SEP      18
static void (*mbox_on_select)(unsigned int btn);

// The key a box has already acted on, held until it is physically released.
//
// A press spans many physw passes; a click edge is one. Blocking only the edge
// leaves every later pass handing the still-down key straight to Canon, which
// is what made the first attempt at this look like it had changed nothing.
// Same shape as recui_swallow in core/gui_recui.c, and for the same reason.
static long mbox_swallow;

// Called from kbd_process() every pass, including after the box has closed -
// gui_mbox_kbd_process() stops being called the moment gui_set_mode() switches
// away, and the SET that dismissed the box is still down at that point.
int gui_mbox_kbd_blocking(void)
{
    if (mbox_swallow && !kbd_is_key_pressed(mbox_swallow))
        mbox_swallow = 0;
    return mbox_swallow != 0;
}

//-------------------------------------------------------------------
void gui_mbox_init(int title, int msg, const unsigned int flags, void (*on_select)(unsigned int btn))
{
    int i;

    mbox_buttons_num = 0;
    for (i=0; i<BUTTONSNUM && mbox_buttons_num<MAX_BUTTONS; ++i)
    {
        if (flags & MBOX_BTN_MASK & buttons[i].flag)
            mbox_buttons[mbox_buttons_num++] = i;
    }
    if (mbox_buttons_num == 0)
        mbox_buttons[mbox_buttons_num++] = 0; // Add button "Ok" if there is no buttons

    switch (flags & MBOX_DEF_MASK)
    {
        case MBOX_DEF_BTN2:
            mbox_button_active = 1;
            break;
        case MBOX_DEF_BTN3:
            mbox_button_active = 2;
            break;
        case MBOX_DEF_BTN1:
        default:
            mbox_button_active = 0;
            break;
    }

    mbox_title = lang_str(title);
    mbox_msg = lang_str(msg);
    mbox_to_draw = 3;
    mbox_flags = flags;
    mbox_on_select = on_select;

    mbox_swallow = 0;
    gui_mbox_mode_old = gui_set_mode(&mboxGuiHandler);
}

//-------------------------------------------------------------------
void gui_mbox_draw()
{
    if (mbox_to_draw & 1)
    {
        int bw=(mbox_buttons_num*BUTTON_SIZE*FONT_WIDTH+(mbox_buttons_num-1)*BUTTON_SEP);

        int h = MAX_LINES;
        int w = text_dimensions(mbox_msg, strlen(mbox_title), MAX_WIDTH, &h) + 2;

        if (bw+BUTTON_SEP > w*FONT_WIDTH)
            w = (bw+BUTTON_SEP)/FONT_WIDTH+1;
    
        coord x = (camera_screen.width - w * FONT_WIDTH) >> 1;
        coord y = (camera_screen.height - (h+2) * FONT_HEIGHT) >> 1;
        draw_rectangle(x-4, y-4, x+w*FONT_WIDTH+4, y+(h+2)*FONT_HEIGHT+SPACING_BTN+2+SPACING_TITLE+7,
                       MAKE_COLOR(COLOR_GREY, COLOR_WHITE), RECT_BORDER3|DRAW_FILLED|RECT_SHADOW3); // main box
        draw_rectangle(x-2, y-2, x+w*FONT_WIDTH+2, y+FONT_HEIGHT+2, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE), RECT_BORDER1|DRAW_FILLED); //title
    
        draw_string_justified(x, y, mbox_title, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE), 0, w*FONT_WIDTH, TEXT_CENTER); //title text
        y += FONT_HEIGHT+2+SPACING_TITLE;

        int justification = TEXT_LEFT;
        switch (mbox_flags & MBOX_TEXT_MASK)
        {
            case MBOX_TEXT_CENTER:  justification = TEXT_CENTER; break;
            case MBOX_TEXT_RIGHT:   justification = TEXT_RIGHT; break;
        }
        draw_text_justified(x+FONT_WIDTH, y, mbox_msg, MAKE_COLOR(COLOR_GREY, COLOR_WHITE), w-1, MAX_LINES, justification); // text

        mbox_buttons_x = x+((w*FONT_WIDTH-bw)>>1);
        mbox_buttons_y = y+h*FONT_HEIGHT+SPACING_BTN;
    }

    if (mbox_to_draw & 2)
    {
        int i;
        for (i=0; i<mbox_buttons_num; ++i)
        {
            draw_button(mbox_buttons_x + i * (BUTTON_SIZE*FONT_WIDTH+BUTTON_SEP), mbox_buttons_y, BUTTON_SIZE, buttons[mbox_buttons[i]].text, (mbox_button_active == i));
        }
    }

    mbox_to_draw = 0;
}

//-------------------------------------------------------------------
// Returns 1 when the box actually used the key, so Canon never sees it.
//
// This used to return 0 unconditionally, and for as long as every message box
// was raised from inside <ALT> that was harmless: kbd_process() ends in
// `return kbd_blocked`, which is already 1 there, so the press was blocked by
// the ALT state rather than by anything this function said.
//
// The playback bend prompt (gui_bend_shot_prompt(), held mode button over a
// picture) is raised OUTSIDE <ALT>, where kbd_blocked is 0. There the missing
// return let both things happen on one press: the box moved its selection from
// NO to YES, and Canon - which still saw the arrow - stepped to the previous
// picture and repainted over the box. Unusable, and it looked like the box had
// no key handling at all when in fact it had too little authority.
//
// Only the keys actually consumed are claimed. Anything else still falls
// through exactly as before, so a box cannot strand a body by swallowing the
// shutter.
int gui_mbox_kbd_process()
{
    long key = kbd_get_clicked_key() | get_jogdial_direction();
    int used = 0;

    switch (key)
    {
    case JOGDIAL_LEFT:
    case KEY_LEFT:
        if (--mbox_button_active < 0) mbox_button_active = mbox_buttons_num - 1;
        mbox_to_draw = 2;
        used = 1;
        break;
    case JOGDIAL_RIGHT:
    case KEY_RIGHT:
        if (++mbox_button_active >= mbox_buttons_num) mbox_button_active = 0;
        mbox_to_draw = 2;
        used = 1;
        break;
    case KEY_SET:
        gui_set_mode(gui_mbox_mode_old);
        if (mbox_flags & MBOX_FUNC_RESTORE)
            gui_set_need_restore();
        if (mbox_on_select) 
            mbox_on_select(buttons[mbox_buttons[mbox_button_active]].flag);
        used = 1;
        break;
    }

    // Latch the real keys, so the block outlives both the click edge and the
    // box itself. Jogdial directions are events rather than held keys, so
    // there is nothing to wait for a release on.
    if (used && (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_SET))
        mbox_swallow = key;

    // Level, not edge. While the box is up it owns these keys outright - it is
    // modal, and the thing underneath must not act on them. This is what stops
    // the playback bend prompt's arrows also stepping Canon to the previous
    // picture.
    if (mbox_swallow || kbd_is_key_pressed(KEY_LEFT) ||
        kbd_is_key_pressed(KEY_RIGHT) || kbd_is_key_pressed(KEY_SET))
        used = 1;

    // Same handoff core/gui_recui.c uses: returning 1 is what makes
    // kbd_update_key_state() rebuild physw_status from kbd_mod_state, and the
    // release is what puts this key into that state as "not pressed".
    if (used)
        kbd_key_release_all();

    return used;
}

int gui_mbox_touch_handler(int sx, int sy)
{
    int i;
    coord x = mbox_buttons_x;

    if ((sy >= mbox_buttons_y-2) && (sy <= mbox_buttons_y+FONT_HEIGHT+2))
    {
        for (i=0; i<mbox_buttons_num; ++i)
        {
            if ((sx >= x) && (sx <= x+BUTTON_SIZE*FONT_WIDTH+3))
            {
                if (mbox_button_active != i)
                {
                    mbox_to_draw = 2;
                    mbox_button_active = i;
                }
                return KEY_SET;
            }
            x += BUTTON_SIZE*FONT_WIDTH+BUTTON_SEP;
        }
    }
    return 0;
}
