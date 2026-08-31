#include "camera_info.h"
#include "keyboard.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "lang.h"
#include "theme.h"
#include "gui_tbox.h"
#include "module_def.h"

/* Themed, modal grid keyboard. Replaces the old arrow-key multi-tap editor. */
#define COLS 10
#define ROWS 5
#define CHAR_ROWS 4
#define KEY_TOP 78
#define KEY_H 23
#define FIELD_Y 25
#define FIELD_H 43
#define VIEW_CHARS 80

typedef void (*select_cb)(const char *);
static gui_handler *old_mode;
static int running, dirty, row, col, page;
static unsigned int limit;
static char *text, local_text[MAX_TEXT_SIZE + 1];
static const char *title, *prompt;
static select_cb callback;

static const char keypage[3][CHAR_ROWS][COLS + 1] = {
    { "abcdefghij", "klmnopqrst", "uvwxyz.,!?", "0123456789" },
    { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ.,!?", "0123456789" },
    { "@#&-_+/\\()", "[]{}:;'\"`~", "$%^*=<>|  ", "0123456789" }
};

static color bg(void) { return COLOR_BLACK; }
static color tc(int n) { return theme_color(n); }

static void finish(int save)
{
    select_cb cb = callback;
    callback = 0; running = 0;
    gui_set_mode(old_mode);
    gui_set_need_restore();
    if (cb) cb(save ? text : 0);
}

static void append(char c)
{
    unsigned int n = strlen(text);
    if (n < limit) { text[n] = c; text[n + 1] = 0; dirty = 1; }
}

static void erase(void)
{
    unsigned int n = strlen(text);
    if (n) { text[n - 1] = 0; dirty = 1; }
}

static void choose(void)
{
    if (row < CHAR_ROWS) {
        char c = keypage[page][row][col];
        if (c != ' ') append(c);
    } else if (col < 3) append(' ');
    else if (col < 5) erase();
    else if (col < 7) { page = (page + 1) % 3; dirty = 2; }
    else if (col < 9) finish(1);
    else finish(0);
}

static void draw_field(void)
{
    int i, n = strlen(text), start = n > VIEW_CHARS ? n - VIEW_CHARS : 0;
    int x = 8, y = FIELD_Y;
    char count[12];
    draw_rectangle(5, FIELD_Y - 3, camera_screen.width - 6, FIELD_Y + FIELD_H,
                   MAKE_COLOR(bg(), tc(TC_ACCENT)), RECT_BORDER1 | DRAW_FILLED);
    for (i = start; i < n; i++) {
        draw_char(x, y, text[i], MAKE_COLOR(bg(), tc(TC_TEXT)));
        x += FONT_WIDTH;
        if (x + FONT_WIDTH > camera_screen.width - 8) { x = 8; y += FONT_HEIGHT; }
    }
    draw_line(x, y + FONT_HEIGHT - 2, x + FONT_WIDTH - 2,
              y + FONT_HEIGHT - 2, tc(TC_ACCENT));
    sprintf(count, "%d/%d", n, limit);
    draw_string(camera_screen.width - strlen(count) * FONT_WIDTH - 8,
                FIELD_Y + FIELD_H - FONT_HEIGHT, count, MAKE_COLOR(bg(), tc(TC_DIM)));
}

static void draw_key(int r, int c)
{
    int w = camera_screen.width / COLS;
    int x0 = c * w + 2, x1 = (c + 1) * w - 2;
    int y0 = KEY_TOP + r * KEY_H, y1 = y0 + KEY_H - 2;
    int sel = r == row && c == col;
    color kb = sel ? tc(TC_HILITE) : bg();
    color kf = sel ? tc(TC_HILITE_FG) : tc(TC_TEXT);
    char s[2] = { 0, 0 };
    draw_rectangle(x0, y0, x1, y1,
                   MAKE_COLOR(kb, sel ? tc(TC_ACCENT) : tc(TC_DIM)),
                   RECT_BORDER1 | DRAW_FILLED);
    s[0] = keypage[page][r][c];
    if (s[0] != ' ')
        draw_string(x0 + (x1 - x0 - FONT_WIDTH) / 2,
                    y0 + (KEY_H - FONT_HEIGHT) / 2, s, MAKE_COLOR(kb, kf));
}

static int action_start(int c)
{
    if (c < 3) return 0;
    if (c < 5) return 3;
    if (c < 7) return 5;
    if (c < 9) return 7;
    return 9;
}

static void draw_action(int start, int span, const char *label)
{
    int w = camera_screen.width / COLS;
    int x0 = start * w + 2, x1 = (start + span) * w - 2;
    int y0 = KEY_TOP + CHAR_ROWS * KEY_H, y1 = y0 + KEY_H - 2;
    int sel = row == CHAR_ROWS && action_start(col) == start;
    color kb = sel ? tc(TC_HILITE) : bg();
    color kf = sel ? tc(TC_HILITE_FG) : tc(TC_TEXT);
    draw_rectangle(x0, y0, x1, y1,
                   MAKE_COLOR(kb, sel ? tc(TC_ACCENT) : tc(TC_DIM)),
                   RECT_BORDER1 | DRAW_FILLED);
    draw_string(x0 + (x1 - x0 - strlen(label) * FONT_WIDTH) / 2,
                y0 + (KEY_H - FONT_HEIGHT) / 2, label, MAKE_COLOR(kb, kf));
}

static void keyboard_draw(int force)
{
    int r, c;
    if (!dirty && !force) return;
    if (dirty == 2 || force) {
        draw_rectangle(0, 0, camera_screen.width - 1, camera_screen.height - 1,
                       MAKE_COLOR(bg(), bg()), RECT_BORDER0 | DRAW_FILLED);
        draw_string(6, 3, title, MAKE_COLOR(bg(), tc(TC_ACCENT)));
        draw_string(camera_screen.width - strlen(prompt) * FONT_WIDTH - 6, 3,
                    prompt, MAKE_COLOR(bg(), tc(TC_DIM)));
    }
    draw_field();
    for (r = 0; r < CHAR_ROWS; r++) for (c = 0; c < COLS; c++) draw_key(r, c);
    draw_action(0, 3, "SPACE");
    draw_action(3, 2, "DELETE");
    draw_action(5, 2, "Aa/#");
    draw_action(7, 2, "SAVE");
    draw_action(9, 1, "X");
    draw_string(6, camera_screen.height - FONT_HEIGHT - 2,
                "Arrows move  SET choose  MENU cancel", MAKE_COLOR(bg(), tc(TC_DIM)));
    dirty = 0;
}

static int keyboard_kbd(void)
{
    switch (kbd_get_autoclicked_key() | get_jogdial_direction()) {
    case KEY_LEFT: case JOGDIAL_LEFT:
        if (row == CHAR_ROWS) {
            int s = action_start(col);
            col = s == 0 ? 9 : s == 3 ? 0 : s == 5 ? 3 : s == 7 ? 5 : 7;
        } else col = (col + COLS - 1) % COLS;
        dirty = 1; break;
    case KEY_RIGHT: case JOGDIAL_RIGHT:
        if (row == CHAR_ROWS) {
            int s = action_start(col);
            col = s == 0 ? 3 : s == 3 ? 5 : s == 5 ? 7 : s == 7 ? 9 : 0;
        } else col = (col + 1) % COLS;
        dirty = 1; break;
    case KEY_UP: row = (row + ROWS - 1) % ROWS; dirty = 1; break;
    case KEY_DOWN:
        row = (row + 1) % ROWS;
        if (row == CHAR_ROWS) col = action_start(col);
        dirty = 1; break;
    case KEY_SET: choose(); break;
    case KEY_ZOOM_OUT: erase(); break;
    case KEY_ZOOM_IN: append(' '); break;
    }
    return 1; /* block the complete keyboard from Canon while this is open */
}

static void keyboard_menu(void) { finish(0); }
static gui_handler keyboard_mode = {
    GUI_MODE_MODULE, keyboard_draw, keyboard_kbd, keyboard_menu, 0,
    GUI_MODE_FLAG_NORESTORE_ON_SWITCH
};

static int textbox_init(int t, int msg, const char *initial, unsigned int size,
                        void (*cb)(const char *), char *buf)
{
    if (!buf && size > MAX_TEXT_SIZE) size = MAX_TEXT_SIZE;
    text = buf ? buf : local_text;
    /* Bend passes its edit buffer as both initial value and destination. */
    if (initial != text) {
        memset(text, 0, size + 1);
        if (initial) strncpy(text, initial, size);
    }
    text[size] = 0;
    limit = size; title = lang_str(t); prompt = lang_str(msg); callback = cb;
    row = col = page = 0; dirty = 2; running = 1;
    old_mode = gui_set_mode(&keyboard_mode);
    return 1;
}

int _module_unloader(void) { if (callback) finish(0); return 0; }
int _module_can_unload(void) { return !running; }
int _module_exit_alt(void) { running = 0; callback = 0; return 0; }

libtextbox_sym _libtextbox = {
    { 0, _module_unloader, _module_can_unload, _module_exit_alt, 0 }, textbox_init
};

ModuleInfo _module_info = {
    MODULEINFO_V1_MAGICNUM, sizeof(ModuleInfo), GUI_TBOX_VERSION,
    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE, ANY_PLATFORM_ALLOWED,
    (int32_t)"Themed keyboard", MTYPE_EXTENSION, &_libtextbox.base,
    CONF_VERSION, CAM_SCREEN_VERSION, ANY_VERSION, ANY_VERSION, 0
};
