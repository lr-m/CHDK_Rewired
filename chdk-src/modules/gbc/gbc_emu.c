//-------------------------------------------------------------------
// Emulator driver: owns the gb_state, the ROM pager and the cartridge, and
// implements the hooks the patched core calls back into (gbc_hooks.h).
//
// Also the video and input frontend. Upstream's whole frontend contract is
// three functions (core/gui.h) - gui_lcd_init, gui_lcd_render_frame and
// gui_input_poll - so the CHDK side of the emulator really is this small.
//-------------------------------------------------------------------

#include "gbc_port.h"
#include "gbc_rom.h"
#include "gbc_cam.h"
#include "gbc_hooks.h"
#include "gbc_save.h"
#include "gbc_emu.h"

#include "core/types.h"
#include "core/state.h"
#include "core/hwdefs.h"
#include "core/cpu.h"
#include "core/mmu.h"
#include "core/lcd.h"
#include "core/player_input.h"

#include "keyboard.h"
#include "conf.h"
#include "camera_info.h"
#include "viewport.h"
#include "gui_draw.h"
#include "clock.h"        // get_tick_count, for the frame pacing and the HUD
#include "shooting.h"     // shooting_get_zoom / shooting_set_zoom, for lens mode

//-------------------------------------------------------------------
// Panic unwind. cpu_error()/mmu_error() upstream print and exit(1); neither
// exists here and a longjmp back to the module is the only sane answer inside
// a camera GUI task.

char gbc_panic_msg[64];
int  gbc_panicked = 0;

// Why the last gbc_emu_open() failed. Almost always memory: the camera
// cartridge asks for ~270K on top of the emulator's own buffers, so it can
// fail where an ordinary MBC title succeeds.
char gbc_open_error[64];

// Diagnostic: 0 skips the Game Boy's scanline rasteriser while leaving the
// emulated CPU untouched. See gui_gbc.c for the key that toggles it.
int gbc_render_enabled = 1;

// What the player asked for, as opposed to what the slice loop is doing on any
// given frame. gbc_render_enabled is driven per frame by gbc_emu_frame().
int gbc_render_user = 1;

// Whether the last slice ended on a rasterised frame - if not, the pixbuf is
// unchanged and there is nothing to blit.
static int rendered_this_slice = 0;

// Per-frame diagnostics, read by the HUD in gui_gbc.c.
unsigned gbc_last_frame_insns = 0;
int      gbc_last_frame_hit_guard = 0;

// Timing, all milliseconds. gap is wall-clock between draw callbacks; emu and
// blit are where the time inside one went.
unsigned gbc_ms_gap = 0;
unsigned gbc_ms_emu = 0;
unsigned gbc_ms_blit = 0;
int      gbc_frames_per_slice = 0;

// Emulated frames per second against the wall clock, in tenths.
//
// The three timings above say where a slice went; this says whether the thing
// is playable, which is a different question and the one asked first. It
// counts Game Boy frames actually emulated over a real second, so it is
// directly comparable to the 59.7 the hardware ran at - not frames the module
// drew, and not the draw callback's own rate.
//
// Tenths because the interesting range on this camera is likely single digits,
// where a whole number throws away most of what you are trying to measure.
int gbc_fps_x10 = 0;

static unsigned fps_window_start;       // tick the current second began
static int      fps_window_frames;      // frames emulated inside it

#define GBC_FPS_WINDOW_MS   1000

// Called once per slice with the frames it emulated. Kept out of the frame
// function so the reset path has one obvious place to clear it from.
static void gbc_fps_account(int emulated)
{
    unsigned now = (unsigned)get_tick_count();
    unsigned span;

    if (!fps_window_start) { fps_window_start = now; fps_window_frames = 0; }

    fps_window_frames += emulated;
    span = now - fps_window_start;

    if (span >= GBC_FPS_WINDOW_MS)
    {
        // frames * 1000 * 10 / ms. At any rate this port can reach the
        // multiply cannot overflow, and the divide happens once a second.
        gbc_fps_x10       = (int)(((unsigned)fps_window_frames * 10000u) / span);
        fps_window_start  = now;
        fps_window_frames = 0;
    }
}

void gbc_fps_reset(void)
{
    gbc_fps_x10       = 0;
    fps_window_start  = 0;
    fps_window_frames = 0;
}

void gbc_panic(const char *what)
{
    int i;
    for (i = 0; i < (int)sizeof(gbc_panic_msg) - 1 && what[i]; i++)
        gbc_panic_msg[i] = what[i];
    gbc_panic_msg[i] = 0;

    gbc_panicked = 1;
}

//-------------------------------------------------------------------
// The core's mmu_error()/mmu_assert()/cpu_error() macros all end in a call to
// dbg_run_debugger(). Upstream drops you at an interactive prompt; there is no
// prompt here, so every one of them becomes a panic unwind carrying the CPU's
// position, which is the one piece of context worth keeping.
//
// Returning non-zero would mean "quit" to the callers that check, but the
// longjmp means none of them get the chance.

int dbg_run_debugger(struct gb_state *s)
{
    static char buf[32];
    static const char hex[] = "0123456789abcdef";
    unsigned pc = s ? s->pc : 0;
    int i;

    for (i = 0; i < 11; i++)
        buf[i] = "core fault "[i];
    buf[11] = 'p'; buf[12] = 'c'; buf[13] = '=';
    buf[14] = hex[(pc >> 12) & 0xf];
    buf[15] = hex[(pc >>  8) & 0xf];
    buf[16] = hex[(pc >>  4) & 0xf];
    buf[17] = hex[(pc      ) & 0xf];
    buf[18] = 0;

    gbc_panic(buf);
    return 1;
}

void dbg_print_regs(struct gb_state *s)
{
    (void)s;
}

//-------------------------------------------------------------------
// Hooks called from the patched core.

// Backing store for the inline ROM read memo in gbc_hooks.h. The lookup lives
// in the header so it can be inlined into the unity build's decode loop -
// modules are -mlong-calls, and out of line this cost an indirect branch for
// every ROM byte the emulated CPU fetched. Only the state and the flush are
// here.
int            gbc_rom_memo_bank = -1;
unsigned char *gbc_rom_memo_win  = 0;

void gbc_rom_memo_flush(void)
{
    gbc_rom_memo_bank = -1;
    gbc_rom_memo_win  = 0;
}

unsigned char gbc_hook_cam_read(struct gb_state *s, unsigned addr)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    if (!c)
        return 0xff;
    return gbc_cam_read(c, (unsigned short)addr);
}

void gbc_hook_cam_write(struct gb_state *s, unsigned addr, unsigned char val)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    if (!c)
        return;

    gbc_cam_write(c, (unsigned short)addr, val);

    // The camera cartridge keeps its own bank register, so a write to its
    // mapper range changes what 4000-7fff points at without going through any
    // of mmu.c's remap sites. Missing this reads the previous bank's code
    // until something else happens to rebuild the map.
    if (addr < 0x6000)
        gbc_mmu_remap(s);
}

int gbc_hook_cam_rom_bank(struct gb_state *s)
{
    gbc_cam_t *c = (gbc_cam_t *)s->gbc_cart;
    return c ? c->rom_bank : 1;
}

//-------------------------------------------------------------------
// Video.
//
// The Game Boy is 160x144 and CHDK's coordinate system is 360x240, so the
// screen takes a 1:1 blit with a 100/48-pixel border. Scaling up would need
// either a non-integer factor or a crop, and a 1:1 Game Boy screen on this
// panel is already about the same physical size as a real DMG's.
//
// Drawn through draw_pixel() with a shadow buffer so only changed pixels cost
// anything - see the long comment on gui_lcd_render_frame() for why the
// direct-to-framebuffer version did not work.

// Scaled to fill the screen's height.
//
// 144 -> 240 is exactly 5/3, and 160 * 240/144 is 266.67, so 266 wide keeps the
// Game Boy's aspect to within a third of a pixel. That leaves a 47px bar either
// side and nothing above or below.
//
// Scaling is by ratio table, not by division per pixel: dst_x0[] holds the
// first destination column each source column maps to, and the run length is
// the gap to the next entry. Every source pixel therefore covers a 1-2 x 1-2
// block, and no divide happens in the loop - ARM946 has no divide instruction
// (see the same trick in gbc_feed.c).
#define GBC_DST_W       266
#define GBC_DST_H       240
#define GBC_ORIGIN_X    ((360 - GBC_DST_W) / 2)
#define GBC_ORIGIN_Y    0

static short gbc_dst_x0[GB_LCD_W + 1];
static short gbc_dst_y0[GB_LCD_H + 1];

static void gbc_build_scale_tables(void)
{
    int i;
    for (i = 0; i <= GB_LCD_W; i++)
        gbc_dst_x0[i] = (short)((i * GBC_DST_W) / GB_LCD_W);
    for (i = 0; i <= GB_LCD_H; i++)
        gbc_dst_y0[i] = (short)((i * GBC_DST_H) / GB_LCD_H);
}

// Four greys from the CHDK palette, darkest first. Index order matches the
// Game Boy's own 0=white..3=black once inverted below.
static unsigned char gbc_shade[4];

// Defined with the shadow buffer below, but called from gui_lcd_init() - it
// depends on gbc_shade[], which that is what fills in.
static void gbc_pick_shadow_marker(void);
void gbc_strip_invalidate(void);

//-------------------------------------------------------------------
// Screen tint.
//
// Four tones, lightest first, and the Game Boy needs exactly four. They are
// built the way every other colour in this fork now is: a palette byte is two
// base entries blended 50/50, so a hue gives its own ramp - the hue with white
// is the highlight, the hue with itself is the midtone, the hue with black is
// the shadow, and black is black. See include/theme.h for where that comes
// from.
//
// That is also the fix for a tone that was wrong for a long time: the dark
// green was 0x25, which is red + green - olive, not dark green. It is 0x5f
// here, green + black.
//
// Grey is in the list because a Game Boy Pocket is a real thing and because it
// is the honest way to look at what the sensor is actually giving you.
#define GBC_HUE_COUNT   6

static const struct {
    const char    *name;    // for the menu
    const char    *tag;     // three characters, for the strip on screen
    unsigned char  tone[4]; // light, mid, dark, black
} gbc_hues[GBC_HUE_COUNT] = {
    { "Green",  "grn", { 0x51, 0x55, 0x5f, 0xff } },   // DMG, and the default
    { "Grey",   "gry", { 0x11, 0x13, 0x3f, 0xff } },   // Pocket
    { "Red",    "red", { 0x21, 0x22, 0x2f, 0xff } },
    { "Yellow", "yel", { 0x61, 0x66, 0x6f, 0xff } },
    { "Blue",   "blu", { 0xd1, 0xdd, 0xdf, 0xff } },
    { "Orange", "org", { 0xe1, 0xee, 0xef, 0xff } },
};

static int gbc_hue_now = -1;

// Fill the four tones from the chosen hue. Returns 1 if anything changed, so
// the caller knows to throw the shadow buffer away - every pixel on screen is
// now the wrong colour and the differ would happily leave them all alone.
static int gbc_set_hue(int h)
{
    int i;

    if (h < 0 || h >= GBC_HUE_COUNT) h = 0;
    if (h == gbc_hue_now) return 0;

    for (i = 0; i < 4; i++)
        gbc_shade[i] = gbc_hues[h].tone[i];
    gbc_hue_now = h;
    gbc_photo_set_hue(h);       // the exported BMPs follow the screen
    gbc_pick_shadow_marker();
    return 1;
}

int gui_lcd_init(int width, int height, int zoom, char *wintitle)
{
    (void)width; (void)height; (void)zoom; (void)wintitle;

    // The tones come from the hue table above rather than from four constants.
    //
    // The note this replaced is still worth keeping: COLOR_GREY_DK is not
    // opaque on these bodies - platform_palette.c gives both "Dark Grey" and
    // "Transparent Dark Grey" the same entry, 0x44 - so a tone that asks for
    // it gets the see-through one and the Canon live view shows through every
    // mid-dark pixel of the Game Boy screen. Nothing in the table above uses
    // it, and any hue added to that table has to avoid it too.
    gbc_hue_now = -1;
    gbc_set_hue(conf.gbc_hue);

    gbc_build_scale_tables();

    return 0;
}

// Previous frame's shades, so only changed pixels are pushed. 0xff means
// "unknown", which forces a full repaint.
static unsigned char *gbc_shadow = 0;

// Set when the area around the Game Boy screen needs repainting.
static int gbc_surround_dirty = 1;

// The "this pixel is unknown, repaint it" marker for the shadow buffer.
//
// It must not collide with any colour the blitter can actually emit, and 0xff
// did: include/palette.h defines COLOR_BLACK as 0xff, which is gbc_shade[3].
// Every black pixel therefore compared equal to "unknown" and was skipped as
// already-correct, so after an invalidate the black parts of the picture were
// never drawn at all - and undrawn OSD is transparent, so the Canon live view
// showed through exactly the dark areas of the Game Boy screen.
//
// Chosen at init from whatever byte the four shades do not use, so it cannot
// collide again if the palette changes. Four values out of 256 are taken, so
// this always finds one, and the hot loop is unchanged - no extra test.
static unsigned char gbc_shadow_none = 0xfe;

static void gbc_pick_shadow_marker(void)
{
    int v;
    for (v = 0; v < 256; v++)
        if (v != gbc_shade[0] && v != gbc_shade[1] &&
            v != gbc_shade[2] && v != gbc_shade[3])
        {
            gbc_shadow_none = (unsigned char)v;
            return;
        }
}

void gbc_invalidate_screen(void)
{
    if (gbc_shadow)
        memset(gbc_shadow, gbc_shadow_none, GB_LCD_W * GB_LCD_H);
    gbc_surround_dirty = 1;
}

//-------------------------------------------------------------------
// Repainting over the firmware, without being told.
//
// Only the pixels that changed since the last frame are drawn, which is what
// makes the emulator fast enough to be worth having - but it also means that
// anything Canon draws over the top stays there. The differ compares against
// what the emulator last *sent*, not against what is on the screen, so a
// firmware repaint leaves the live view showing through and the emulator
// perfectly content that every pixel is already correct.
//
// A shutter press was handled by invalidating on the key (gui_input_poll), but
// that only covers the one cause anybody noticed. On the A470 the firmware
// draws once shortly after the module starts, with no key involved at all, and
// the lens image sits behind the Game Boy screen until something else forces a
// repaint.
//
// So the emulator stops relying on knowing why. It repaints everything on a
// timer: quickly for the first few seconds, while the firmware is still
// settling into the mode, and occasionally after that as a floor under any
// cause not yet found. Repairs are deliberately sparse after startup: a full
// scaled repaint is expensive on this camera even though ordinary frames only
// send changed pixels.
#define GBC_REPAINT_SETTLE_MS   2500    // how long the firmware is given
#define GBC_REPAINT_EARLY_MS     500    // repaint interval during that
#define GBC_REPAINT_IDLE_MS    10000    // and afterwards

static unsigned gbc_repaint_start;      // tick the module opened
static unsigned gbc_repaint_last;       // tick of the last forced repaint

static void gbc_repaint_tick(unsigned now)
{
    unsigned every = ((now - gbc_repaint_start) < GBC_REPAINT_SETTLE_MS)
                     ? GBC_REPAINT_EARLY_MS : GBC_REPAINT_IDLE_MS;

    if ((now - gbc_repaint_last) < every) return;
    gbc_repaint_last = now;
    // Invalidate only the emulated picture. gbc_invalidate_screen() also
    // marks the side bars dirty; using it on this timer filled those bars
    // black and redrew their labels every 250ms, which was the flashing lens /
    // colour strip seen on the camera.
    if (gbc_shadow)
        memset(gbc_shadow, gbc_shadow_none, GB_LCD_W * GB_LCD_H);

    // Do not refill the side bars on this timer. Unlike the Game Boy pixels,
    // a black fill followed by text is visible as a flash. Explicit events
    // that can damage the bars (startup, shutter and lens movement) already
    // set gbc_surround_dirty themselves.
}

// Paint the margins opaque.
//
// Anything this module does not draw stays COLOR_TRANSPARENT (palette.h: 0x00),
// and on a body in record mode transparent OSD means the Canon live viewport
// shows straight through. That is why the real lens image was visible around
// the Game Boy screen, and it is also the likely source of the coloured banding
// over it - live view bleeding through, not a palette fault.
//
// COLOR_BLACK is 0xff and palette.h documents it as one of the two entries that
// mean the same thing in every palette, so it is safe to use without knowing
// which palette this body ended up with.
//
// Only the two side bars are left now that the screen is scaled to full height.
static void gbc_paint_surround(void)
{
    if (GBC_ORIGIN_X > 0)
    {
        draw_rectangle(0, 0, GBC_ORIGIN_X - 1, 239,
                       MAKE_COLOR(COLOR_BLACK, COLOR_BLACK),
                       RECT_BORDER0 | DRAW_FILLED);
        draw_rectangle(GBC_ORIGIN_X + GBC_DST_W, 0, 359, 239,
                       MAKE_COLOR(COLOR_BLACK, COLOR_BLACK),
                       RECT_BORDER0 | DRAW_FILLED);
    }
    // The bars are what the strip is drawn on, so painting them black is
    // painting the strip out. Its own "only repaint what changed" cache would
    // otherwise believe it was still on screen.
    gbc_strip_invalidate();
    gbc_surround_dirty = 0;
}

void gui_lcd_render_frame(char use_colors, uint16_t *pixbuf)
{
    int x, y;

    if (!pixbuf)
        return;

    if (gbc_surround_dirty)
        gbc_paint_surround();

    // Why draw_pixel() and not a direct write to vid_get_bitmap_fb():
    //
    // the bitmap is double-buffered, and vid_get_bitmap_fb() does not return
    // the active buffer - platform/generic/wrappers.c says so in as many
    // words ("*** does not get the active buffer!"). Writing there produced a
    // perfectly correct frame that was never displayed, while the HUD, which
    // goes through draw_string -> draw_pixel, showed up fine. That difference
    // is the whole diagnosis.
    //
    // The buffers themselves (bitmap_buffer, active_bitmap_buffer) are not in
    // modules/exportlist.inc, and draw_dblpixel_raw - which would have been
    // the fast path - is an empty stub on 8-bit displays like this one
    // (core/gui_draw.c, under CAM_DRAW_8BIT). So draw_pixel is the only
    // correct route out of a module.
    //
    // 23040 calls a frame would be painful, so only pixels that actually
    // changed are pushed. A Game Boy screen is mostly static between frames,
    // so in practice this is a few hundred calls, not twenty-three thousand.
    // The HUD sits at y=2 and the screen starts at y=48, so the two never
    // overlap and the shadow stays honest.
    // The DMG/CGB test is hoisted out of the pixel loop. It is constant for
    // the whole run - a cartridge does not change type - and leaving it inside
    // costs a branch on each of 23040 pixels a frame on a core with no branch
    // prediction.
    // The shadow stays at the Game Boy's own 160x144, not the scaled size.
    // Comparing before scaling means the work is proportional to how much of
    // the *emulated* screen changed, and a source pixel that did change is
    // expanded to its 1-2 x 1-2 destination block on the spot. Holding the
    // shadow at 266x240 instead would cost 64K of RAM and compare 2.8x as many
    // bytes to learn the same thing.
    // Two loops rather than one with a test inside. use_colors is constant for
    // the whole run - a cartridge does not change type - and this is 23040
    // iterations a frame on a core with no branch prediction.
#define GBC_PUT_BLOCK(xx, cc)                                       \
    do {                                                            \
        int dx0 = GBC_ORIGIN_X + gbc_dst_x0[xx];                    \
        int dx1 = GBC_ORIGIN_X + gbc_dst_x0[(xx) + 1];              \
        int dx, dy;                                                 \
        for (dy = dy0; dy < dy1; dy++)                              \
            for (dx = dx0; dx < dx1; dx++)                          \
                draw_pixel(dx, dy, (cc));                           \
    } while (0)

    if (use_colors)
    {
        for (y = 0; y < GB_LCD_H; y++)
        {
            const uint16_t *src = pixbuf + y * GB_LCD_W;
            unsigned char *shadow = gbc_shadow ? gbc_shadow + y * GB_LCD_W : 0;
            int dy0 = GBC_ORIGIN_Y + gbc_dst_y0[y];
            int dy1 = GBC_ORIGIN_Y + gbc_dst_y0[y + 1];

            for (x = 0; x < GB_LCD_W; x++)
            {
                // CGB: 15-bit BGR555 reduced to the four greys the OSD palette
                // can address. Rec.601-ish weights, kept integer.
                unsigned v = src[x];
                unsigned l = ((v & 0x1f) * 77 + ((v >> 5) & 0x1f) * 151 +
                              ((v >> 10) & 0x1f) * 28) >> 8;     /* 0..31 */
                unsigned char c = gbc_shade[3 - (l >> 3)];

                if (shadow)
                {
                    if (shadow[x] == c)
                        continue;
                    shadow[x] = c;
                }
                GBC_PUT_BLOCK(x, c);
            }
        }
    }
    else
    {
        for (y = 0; y < GB_LCD_H; y++)
        {
            const uint16_t *src = pixbuf + y * GB_LCD_W;
            unsigned char *shadow = gbc_shadow ? gbc_shadow + y * GB_LCD_W : 0;
            int dy0 = GBC_ORIGIN_Y + gbc_dst_y0[y];
            int dy1 = GBC_ORIGIN_Y + gbc_dst_y0[y + 1];

            for (x = 0; x < GB_LCD_W; x++)
            {
                unsigned char c = gbc_shade[src[x] & 3];

                if (shadow)
                {
                    if (shadow[x] == c)
                        continue;
                    shadow[x] = c;
                }
                GBC_PUT_BLOCK(x, c);
            }
        }
    }
#undef GBC_PUT_BLOCK
}

//-------------------------------------------------------------------
// Input.
//
// The A480 has no shoulder buttons, so A and B go on SET and DISP - the two
// keys the thumb already rests on - and START/SELECT share the shutter and
// the display toggle. MENU is reserved for leaving, and is deliberately not
// mappable: a game that swallowed MENU would strand the player.

//-------------------------------------------------------------------
// Lens mode.
//
// The Game Boy has four directions and the camera has a zoom lens, and on this
// body they are the same four keys - the keymap says so in as many words, UP
// is also ZOOM_IN. The ROM needs the d-pad, so the lens cannot simply take it;
// and the lens is the one thing this port has that a real Game Boy Camera
// never did, so it cannot not be reachable either.
//
// So the shutter button switches which of the two the arrows are talking to.
// A full press is the one key on this body that no Game Boy button is on -
// START is the half press - and it is a deliberate gesture rather than
// something a thumb finds by accident on the d-pad.
//
// In lens mode the ROM sees nothing at all from the arrows. Not "sees them and
// we also zoom": a camera ROM that gets a d-pad press while you are zooming
// walks its own menus underneath you, and the picture you were framing is
// gone.
//
//   UP / DOWN     zoom the lens, in and out
//   LEFT / RIGHT  the screen tint
//
// Tint is here rather than in a menu because it is a thing you try against a
// subject and change your mind about, which is exactly what the arrows are
// for once they are free.
#define GBC_MODE_PAD    0
#define GBC_MODE_LENS   1

static int gbc_mode;
static int gbc_zoom_want = -1;      // -1 = follow the lens, else where to go

// Auto-repeat for the two meta axes. The d-pad does not want repeat - the ROM
// does its own - but a zoom step per press would make crossing the range a
// finger exercise.
#define GBC_REPEAT_MS   180

static int gbc_meta_step(long key, int *held_key, int *held_at)
{
    int t = (int)get_tick_count();

    if (!kbd_is_key_pressed(key))
        return 0;
    if (*held_key != (int)key)
    {
        *held_key = (int)key;
        *held_at  = t;
        return 1;                   // the press itself
    }
    if ((t - *held_at) >= GBC_REPEAT_MS)
    {
        *held_at = t;
        return 1;
    }
    return 0;
}

// What the strip on the left shows, so it is only repainted when it changes.
static char gbc_strip_shown[5][8];

// Rows 0..2 sit at the top of the bar; rows 3 and 4 are pinned to the bottom.
//
// The top three are readouts - the mode, the zoom, the tint - and they change
// as you use them, so they belong together where the eye already is. "hold
// PRINT" is not a readout: it is a reminder of the one control nothing on
// screen would otherwise hint at, and it never changes. Out of the way in the
// corner is exactly where that belongs.
#define GBC_STRIP_TOP_ROWS  3

static int gbc_strip_y(int row)
{
    if (row < GBC_STRIP_TOP_ROWS)
        return 4 + row * FONT_HEIGHT;
    // 240, the same literal the origin above and the bar fills already use.
    return 240 - 2 - (5 - row) * FONT_HEIGHT;
}

static void gbc_strip_row(int row, const char *text, color fg)
{
    int y = gbc_strip_y(row);

    if (strncmp(gbc_strip_shown[row], text, sizeof(gbc_strip_shown[0]) - 1) == 0)
        return;

    draw_rectangle(0, y, GBC_ORIGIN_X - 1, y + FONT_HEIGHT - 1,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0 | DRAW_FILLED);
    draw_string(2, y, text, MAKE_COLOR(COLOR_BLACK, fg));

    strncpy(gbc_strip_shown[row], text, sizeof(gbc_strip_shown[0]) - 1);
    gbc_strip_shown[row][sizeof(gbc_strip_shown[0]) - 1] = 0;
}

// The left bar is 47 pixels - five characters - and it was black. It is the
// only space on this screen that is not the Game Boy, so the three things the
// arrows are currently doing go there.
static void gbc_strip_draw(void)
{
    char buf[8];
    int z, n;

    // "PAD" and "LENS" name the mode; the row under it is a reminder of the
    // gesture, because a held key is the one kind of control nothing on screen
    // would otherwise hint at.
    gbc_strip_row(0, (gbc_mode == GBC_MODE_LENS) ? "LENS" : "PAD",
                  (gbc_mode == GBC_MODE_LENS) ? COLOR_YELLOW : COLOR_GREY);

    z = shooting_get_zoom();
    n = zoom_points;
    if (n < 1) n = 1;
    sprintf(buf, "z%d/%d", z, n - 1);
    gbc_strip_row(1, buf, (gbc_mode == GBC_MODE_LENS) ? COLOR_WHITE : COLOR_GREY);

    gbc_strip_row(2, gbc_hues[(gbc_hue_now >= 0) ? gbc_hue_now : 0].tag,
                  gbc_shade[1]);

    gbc_strip_row(3, "hold", COLOR_GREY);
    gbc_strip_row(4, "PRINT", COLOR_GREY);
}

void gbc_strip_invalidate(void)
{
    int i;
    for (i = 0; i < 5; i++) gbc_strip_shown[i][0] = 0;
}

// A is SET, and the shutter is not a button this mode can have.
//
// It was A for one build, because on a camera the shutter should take the
// picture. It cannot be: the shutter is a physical switch the firmware reads
// for itself, not through the key mask CHDK swaps, so blocking it in
// gui_gbc_kbd_process() does not stop Canon acting on it - it starts a real
// capture and its live view and OSD come up underneath the Game Boy screen.
// That is a property of the body, not something this module can decide.
//
// So the emulator repaints over it instead: a shutter press invalidates the
// screen and the bars, and the next frame puts the Game Boy back. An accidental
// press costs a flicker rather than a wrecked screen.
static void gbc_face_buttons(struct player_input *input)
{
    input->button_a      = kbd_is_key_pressed(KEY_SET);
    input->button_b      = kbd_is_key_pressed(KEY_DISPLAY);
    input->button_start  = kbd_is_key_pressed(KEY_SHOOT_HALF);
    input->special_quit  = kbd_is_key_pressed(KEY_MENU);
    // SELECT is filled in by the caller: it shares its key with the mode
    // switch and has to know how long that key has been held.
}

// How long SELECT has to be held to mean "switch mode" rather than "SELECT".
#define GBC_MODE_HOLD_MS    600

// Frames a short SELECT is asserted for after the key comes up.
//
// It cannot be sent while the key is down - that is the gesture that might
// still turn into a mode switch - so it is sent on release, and it has to last
// long enough for the ROM to sample it. Several Game Boy frames run per
// callback, so four callbacks is comfortably more than one poll of the joypad
// register however the ROM is written.
#define GBC_SELECT_FRAMES   4

int gui_input_poll(struct player_input *input)
{
    static int print_down, print_at, print_fired, select_pulse;
    static int held_key, held_at;
    int print = kbd_is_key_pressed(KEY_PRINT);
    int t = (int)get_tick_count();

    memset(input, 0, sizeof(*input));

    // The mode switch is a held SELECT.
    //
    // Every other key is spoken for: four are the d-pad, SET and DISP are A
    // and B, the half press is START, MENU leaves, and the shutter belongs to
    // Canon whatever this module does with it. A hold is the only gesture left
    // that does not cost the ROM a button - and SELECT is the one the Game Boy
    // Camera uses least.
    if (print)
    {
        if (!print_down) { print_down = 1; print_at = t; print_fired = 0; }
        else if (!print_fired && (t - print_at) >= GBC_MODE_HOLD_MS)
        {
            print_fired = 1;
            gbc_mode = (gbc_mode == GBC_MODE_PAD) ? GBC_MODE_LENS : GBC_MODE_PAD;
            held_key = 0;
        }
    }
    else
    {
        // Came up without reaching the threshold, so it was a SELECT after all.
        if (print_down && !print_fired) select_pulse = GBC_SELECT_FRAMES;
        print_down = 0;
    }

    // A shutter press means Canon has just drawn on our screen - see
    // gbc_face_buttons(). Repaint over it.
    if (kbd_is_key_pressed(KEY_SHOOT_HALF) || kbd_is_key_pressed(KEY_SHOOT_FULL))
    {
        gbc_invalidate_screen();
        gbc_surround_dirty = 1;
    }

    if (gbc_mode == GBC_MODE_LENS)
    {
        int moved = 0;

        // The lens is driven by asking for a position rather than by stepping
        // it here: shooting_set_zoom() is a firmware call that takes as long as
        // the motor does, and this runs inside the frame callback.
        if (gbc_meta_step(KEY_UP, &held_key, &held_at))
        {
            if (gbc_zoom_want < 0) gbc_zoom_want = shooting_get_zoom();
            gbc_zoom_want++;
            moved = 1;
        }
        else if (gbc_meta_step(KEY_DOWN, &held_key, &held_at))
        {
            if (gbc_zoom_want < 0) gbc_zoom_want = shooting_get_zoom();
            gbc_zoom_want--;
            moved = 1;
        }
        else if (gbc_meta_step(KEY_RIGHT, &held_key, &held_at))
        {
            conf.gbc_hue = (conf.gbc_hue + 1) % GBC_HUE_COUNT;
            moved = 1;
        }
        else if (gbc_meta_step(KEY_LEFT, &held_key, &held_at))
        {
            conf.gbc_hue = (conf.gbc_hue + GBC_HUE_COUNT - 1) % GBC_HUE_COUNT;
            moved = 1;
        }
        else if (!kbd_is_key_pressed(KEY_UP) && !kbd_is_key_pressed(KEY_DOWN) &&
                 !kbd_is_key_pressed(KEY_LEFT) && !kbd_is_key_pressed(KEY_RIGHT))
            held_key = 0;

        (void)moved;

        // A and B still work - they are not the ROM's navigation, they are its
        // shutter - so a picture can be taken at the end of a zoom without
        // switching back first.
        gbc_face_buttons(input);
        if (select_pulse) { input->button_select = 1; select_pulse--; }
        return 0;
    }

    input->button_up     = kbd_is_key_pressed(KEY_UP);
    input->button_down   = kbd_is_key_pressed(KEY_DOWN);
    input->button_left   = kbd_is_key_pressed(KEY_LEFT);
    input->button_right  = kbd_is_key_pressed(KEY_RIGHT);

    gbc_face_buttons(input);
    if (select_pulse) { input->button_select = 1; select_pulse--; }

    return 0;
}

// Audio is not wired up. The single-channel implementation upstream calls
// "limited and its timing is off", and the A480's speaker is a piezo buzzer
// driven by Canon's own task; taking it over for square waves is a separate
// piece of work with its own reversing. Stubbed so the core links.
int gui_audio_init(int sample_rate, int channels, size_t sndbuf_size, uint8_t *sndbuf)
{
    (void)sample_rate; (void)channels; (void)sndbuf_size; (void)sndbuf;
    return 0;
}

//-------------------------------------------------------------------
// The three functions this port needs out of upstream's emu.c.
//
// emu.c itself is not built: the rest of it is save-state serialisation and
// ROM loading through hosted stdio, both of which this port replaces. What is
// left is small enough to restate here, minus the debugger and savestate
// branches that had nowhere to go anyway.

// gbc_step()/gbc_step_frame() used to live here. They are now
// gbc_core_step_frame() in core/gbccore_unity.c, so that cpu_step, lcd_step,
// mmu_step and cpu_timers_step can be inlined into the loop instead of being
// four -mlong-calls indirects per emulated instruction. This file now makes
// one call per frame.
extern unsigned gbc_core_step_frame(struct gb_state *s, unsigned guard,
                                    int *hit_guard);

static void gbc_step_frame(struct gb_state *s)
{
    // 200000 is comfortably more than one real frame (~17500 instructions)
    // and short enough that a wedged ROM gives the GUI task back promptly.
    gbc_last_frame_insns = gbc_core_step_frame(s, 200000,
                                               &gbc_last_frame_hit_guard);
}

static void gbc_apply_input(struct gb_state *s, struct player_input *in)
{
#define BTN(type, button, bit) \
    do { \
        if (in->button_ ## button) \
            s->io_buttons_ ## type &= ~(1 << (bit)); \
        else \
            s->io_buttons_ ## type |= 1 << (bit); \
    } while (0)

    BTN(buttons, start,  3);
    BTN(buttons, select, 2);
    BTN(buttons, b,      1);
    BTN(buttons, a,      0);
    BTN(dirs,    down,   3);
    BTN(dirs,    up,     2);
    BTN(dirs,    left,   1);
    BTN(dirs,    right,  0);

#undef BTN
}

//-------------------------------------------------------------------
// Diagnostic line. Built by hand because snprintf is stubbed out in this port
// (gbc_port.h) - there is no vsnprintf worth linking into a module.

// Both take the end of the buffer and stop there. They used to run to the end
// of what they were given: the line is built from a dozen fragments whose
// widths depend on a frame counter, a capture count and two flag words, and
// the worst case has been longer than the sixty-four bytes the caller passes
// for some time. Nothing showed it because only the first forty-odd characters
// fit across the screen, so the damage was past the visible end of a
// diagnostic that is off by default.

static char *hud_num(char *p, char *end, unsigned v)
{
    char tmp[12];
    int n = 0;

    if (v == 0)
    {
        if (p < end) *p++ = '0';
        return p;
    }
    while (v && n < 11) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n && p < end) *p++ = tmp[--n];
    return p;
}

static char *hud_str(char *p, char *end, const char *s)
{
    while (*s && p < end) *p++ = *s++;
    return p;
}

void gbc_hud_line(gbc_emu_t *e, char *buf, int len)
{
    static const char * const feed_name[] =
        { "none", "ok", "PLAY", "nobuf", "geom" };
    char *p   = buf;
    // One byte reserved for the terminator, which is written unconditionally
    // at the end - so every fragment below stops one short of the real end.
    char *end = buf + len - 1;

    if (len < 16) { buf[0] = 0; return; }

    // First, because it is the question everything else on this line is a
    // follow-up to: is it running at a usable speed. Real hardware is 59.7.
    // Only the first forty-odd characters fit across the screen, so what goes
    // at the front is what gets read.
    p = hud_str(p, end, "fps=");  p = hud_num(p, end, (unsigned)(gbc_fps_x10 / 10));
    p = hud_str(p, end, ".");     p = hud_num(p, end, (unsigned)(gbc_fps_x10 % 10));
    p = hud_str(p, end, " f=");   p = hud_num(p, end, (unsigned)e->frames);
    // gap is the wall-clock between callbacks, emu/blit the split inside one.
    // If gap is much bigger than emu+blit the ceiling is CHDK's redraw rate,
    // not the interpreter.
    //
    // n is frames emulated in the last slice against the budget it was given.
    // If n is stuck at the budget, the cap is what is limiting speed and the
    // budget can go up; if n is below it, emulation is the limit.
    p = hud_str(p, end, " n=");   p = hud_num(p, end, (unsigned)gbc_frames_per_slice);
    p = hud_str(p, end, "/");     p = hud_num(p, end, (unsigned)e->frame_budget);
    p = hud_str(p, end, " gap="); p = hud_num(p, end, gbc_ms_gap);
    p = hud_str(p, end, " emu="); p = hud_num(p, end, gbc_ms_emu);
    p = hud_str(p, end, " blt="); p = hud_num(p, end, gbc_ms_blit);

    if (!gbc_render_user)
        p = hud_str(p, end, " NORENDER");

    // Camera cartridge only: how many exposures the ROM has asked for, and
    // what the sensor bridge handed back for the last one. "cap" climbing with
    // feed=PLAY means the cartridge is working and the camera is in playback,
    // where there is no live sensor to read.
    if (e->cart)
    {
        p = hud_str(p, end, " cap=");  p = hud_num(p, end, (unsigned)e->cart->captures);
        p = hud_str(p, end, " feed=");
        p = hud_str(p, end, feed_name[(gbc_feed_status >= 0 && gbc_feed_status <= 4)
                                 ? gbc_feed_status : 0]);
    }

    if (gbc_last_frame_hit_guard)
        p = hud_str(p, end, " GUARD");

    *p = 0;
}

//-------------------------------------------------------------------
// Lifecycle.

#define GBC_FAIL(msg) do { strcpy(gbc_open_error, msg); goto fail; } while (0)

int gbc_emu_open(gbc_emu_t *e, const char *rom_path)
{
    unsigned char *bank0;

    memset(e, 0, sizeof(*e));
    gbc_rom_memo_flush();
    // The rate belongs to the ROM that was running, not to the module. Without
    // this the first second of a newly opened cartridge reports the last one's
    // speed, which is the one moment anyone is actually watching the number.
    gbc_fps_reset();

    {
        int i;
        for (i = 0; i < (int)sizeof(e->rom_path) - 1 && rom_path[i]; i++)
            e->rom_path[i] = rom_path[i];
        e->rom_path[i] = 0;
    }

    gbc_open_error[0] = 0;

    e->pager = (gbc_rom_t *)malloc(sizeof(gbc_rom_t));
    e->state = (struct gb_state *)malloc(sizeof(struct gb_state));
    if (!e->pager || !e->state)
        GBC_FAIL("Out of memory (state)");

    memset(e->state, 0, sizeof(struct gb_state));

    if (gbc_rom_open(e->pager, rom_path))
        GBC_FAIL("Cannot read ROM / no memory");

    // The header parse only ever touches bank 0, which the pager pins, so it
    // can be handed the resident window directly instead of a whole copy.
    bank0 = gbc_rom_bank(e->pager, 0);

    // Order matters and is upstream's (emu.c): the ROM header decides the
    // machine's shape before any of the emulator-side state is built.
    if (state_new_from_rom(e->state, bank0, (size_t)e->pager->size))
        GBC_FAIL("Unsupported cart or no memory");

    // The state a real Game Boy is in after its boot ROM has run: pc=0x0100,
    // sp=0xfffe, the documented register values, and - the part that matters
    // here - every io_lcd_* register, LCDC included.
    //
    // There is no boot ROM in this port, so without this the machine starts
    // with pc=0 and LCDC=0: the CPU executes the interrupt vector table as if
    // it were code, and the LCD stays switched off, so lcd_render_current_line
    // fills the framebuffer with zeros forever. That renders as a plain white
    // rectangle on DMG and a plain black one on CGB, while the pc counter
    // ticks over convincingly the whole time.
    //
    // Easy to miss because the name suggests a reset you would only need after
    // startup, and because cpu_init_emu_cpu_state() - which sounds like the
    // one that initialises the CPU - only builds the register lookup tables.
    cpu_reset_state(e->state);

    e->state->gbc_pager = e->pager;

    // Must be wired up before the first instruction runs, since the very
    // first ROM read goes through the pager.
    if (e->state->mbc == GBC_MBC_CAM)
    {
        e->cart = (gbc_cam_t *)malloc(sizeof(gbc_cam_t));
        if (!e->cart || gbc_cam_init(e->cart))
            GBC_FAIL("Out of memory (camera cart)");
        e->state->gbc_cart = e->cart;
        e->is_camera = 1;

        // Restore the album before the ROM's first instruction. It checks
        // cartridge RAM during its own boot, so this has to be in place by then
        // or it decides the cartridge is empty.
        gbc_save_load(e->cart, rom_path);

        // Where to put it back. The cartridge holds its own path because the
        // commit fires from inside a memory write, deep in the mapper, where
        // none of the emulator's state is reachable.
        gbc_save_path(rom_path, e->cart->save_path);
        e->cart->dirty = 0;
    }
    else if (e->state->has_extram && e->state->mem_EXTRAM &&
             e->state->mem_num_banks_extram > 0)
    {
        // An ordinary cartridge with a battery. Restore before the first
        // instruction, for the same reason the album is restored before the
        // camera ROM's: a game checks its save RAM during its own boot and
        // decides there is no save if it is not there yet.
        e->sav_len = (int)EXTRAM_BANKSIZE * e->state->mem_num_banks_extram;
        gbc_save_path(rom_path, e->sav_path);
        gbc_save_ram_read(e->sav_path, e->state->mem_EXTRAM, e->sav_len);
        e->sav_at = (unsigned)get_tick_count();
    }

    init_emu_state(e->state);
    cpu_init_emu_cpu_state(e->state);
    if (!e->state->emu_state || !e->state->emu_cpu_state)
        GBC_FAIL("Out of memory (emu state)");

    if (lcd_init(e->state))
        GBC_FAIL("Out of memory (framebuffer)");

    // Everything the map depends on - banks, memory pointers, the cartridge -
    // is in place by now, so build it once before the first instruction runs.
    gbc_mmu_remap(e->state);

    // Lend the cartridge the work RAM. The album recycler needs it: the ROM
    // keeps its picture list in WRAM and only writes it to the cartridge when
    // it saves, so trimming the cartridge copy on its own never reaches the
    // counter on screen. See gbc_cam.h.
    if (e->cart && e->state->mem_WRAM)
    {
        e->cart->wram       = e->state->mem_WRAM;
        e->cart->wram_bytes = 0x1000 * e->state->mem_num_banks_wram;
    }

    gbc_shadow = (unsigned char *)malloc(GB_LCD_W * GB_LCD_H);
    if (!gbc_shadow)
        GBC_FAIL("Out of memory (shadow)");

    // Before gbc_invalidate_screen(), not after: gui_lcd_init() is what fills
    // gbc_shade[], and the shadow's "unknown" marker is picked from whatever
    // those four values leave free. Invalidating first would fill the buffer
    // using a marker chosen against an all-zero palette.
    gui_lcd_init(GB_LCD_W, GB_LCD_H, 1, "gbc");

    gbc_invalidate_screen();
    gbc_repaint_start = gbc_repaint_last = (unsigned)get_tick_count();

    e->open = 1;
    return 0;

fail:
    gbc_emu_close(e);
    return -1;
}

static void gbc_save_extram_service(gbc_emu_t *e, int force);

// Set by gbc_emu_close() so the picker can report what was written.
int gbc_last_saved_photos = -1;

void gbc_emu_close(gbc_emu_t *e)
{
    if (gbc_shadow) { free(gbc_shadow); gbc_shadow = 0; }

    // Finish any pending album write before anything is freed. Blocking is
    // acceptable here and only here - the emulator is going away, so there is
    // no display left to starve. gbc_cam_free() is about to hand the cartridge
    // RAM back to the heap, and that is where every photograph used to go.
    if (e->cart)
    {
        if (e->cart->dirty)
            e->cart->commit_pending = 1;
        gbc_save_flush(e->cart);
    }

    // The last chance for an ordinary cartridge's battery RAM: the buffer it
    // lives in is freed a few lines below.
    gbc_save_extram_service(e, 1);

    if (e->cart)  { gbc_cam_free(e->cart); free(e->cart); e->cart = 0; }
    if (e->pager) { gbc_rom_close(e->pager); free(e->pager); e->pager = 0; }

    if (e->state)
    {
        if (e->state->mem_WRAM)   free(e->state->mem_WRAM);
        if (e->state->mem_VRAM)   free(e->state->mem_VRAM);
        if (e->state->mem_EXTRAM) free(e->state->mem_EXTRAM);
        if (e->state->emu_state)
        {
            if (e->state->emu_state->lcd_pixbuf)
                free(e->state->emu_state->lcd_pixbuf);
            free(e->state->emu_state);
        }
        free(e->state);
        e->state = 0;
    }

    e->open = 0;
}

//-------------------------------------------------------------------

// Write the cartridge's battery RAM if anything asks for it. `force` is the
// close path, which writes whenever the RAM is dirty at all.
static void gbc_save_extram_service(gbc_emu_t *e, int force)
{
    struct emu_state *es;

    if (!e->sav_len || !e->state || !e->state->mem_EXTRAM) return;

    es = e->state->emu_state;
    if (!es) return;

    if (!es->extram_dirty)
    {
        es->flush_extram = 0;
        return;
    }

    if (!force && !es->flush_extram &&
        (unsigned)(get_tick_count() - e->sav_at) < GBC_SAV_MAX_MS)
        return;

    es->flush_extram = 0;
    if (gbc_save_ram_write(e->sav_path, e->state->mem_EXTRAM, e->sav_len) == 0)
        es->extram_dirty = 0;
    // A card that refused keeps the dirty flag, so the next trigger tries
    // again - the RAM is still the only copy.
    e->sav_at = (unsigned)get_tick_count();
}

int gbc_emu_frame(gbc_emu_t *e)
{
    struct player_input in;
    unsigned t0, t1, now;
    int emulated = 0;

    if (!e->open)
        return -1;

    // Wall-clock since the previous call. If this is much larger than the time
    // actually spent emulating, the limit is how often CHDK calls the draw
    // handler, not the emulator - and no amount of optimising the interpreter
    // will show up on screen.
    now = (unsigned)get_tick_count();
    gbc_ms_gap = e->last_call ? (now - e->last_call) : 0;
    e->last_call = now;

    // Before the input poll, so a repaint this frame asks for is honoured by
    // the render below rather than a frame late.
    gbc_repaint_tick(now);

    gui_input_poll(&in);
    if (in.special_quit)
        return 1;

    gbc_apply_input(e->state, &in);

    // Run as many Game Boy frames as fit in the budget rather than exactly one
    // per callback, and rasterise only the frame that will actually be shown.
    //
    // The rasteriser is the expensive half. lcd_render_current_line() runs 144
    // times a frame doing per-pixel tile and palette lookups - comparable to
    // the cost of dispatching the frame's ~17500 instructions. An earlier
    // version of this loop claimed to skip work on frames it would not display
    // and did not: it skipped the *blit* but still rasterised every frame into
    // a pixbuf it then threw away.
    //
    // So: emulate with the rasteriser off, and switch it on for the last frame
    // of the slice. The emulated CPU runs identically either way - the Game
    // Boy's own LCD state machine still advances, interrupts still fire, only
    // the pixel work is skipped - so this costs accuracy nothing.
    //
    // GBC_RENDER_EVERY additionally rasterises only every Nth displayed frame.
    // The Game Boy runs at 60Hz and nothing on this screen needs that; at N=2
    // the emulated machine keeps full speed and half the pixel work vanishes.
    t0 = (unsigned)get_tick_count();

    // How many Game Boy frames this slice is allowed, paced against the wall
    // clock so the emulator can catch up to 60Hz but never overshoot it.
    //
    // gbc_ms_gap is the whole interval since the previous callback - including
    // the time this function spent emulating last time - so using it as the
    // budget is self-balancing. If emulation is the slow part the gap is large
    // and the budget is generous; if the body ever gets fast enough to finish
    // early the gap shrinks and so does the budget, settling at real speed
    // rather than running games fast.
    //
    // The division is fine here: once per callback, a few dozen times a second,
    // not per pixel.
    {
        int paced = ((int)gbc_ms_gap * GBC_TARGET_FPS) / 1000;
        if (paced < 1)
            paced = 1;                          // always make progress
        if (paced > GBC_MAX_FRAMES_PER_SLICE)
            paced = GBC_MAX_FRAMES_PER_SLICE;
        e->frame_budget = paced;
    }

    for (;;)
    {
        int is_last;

        // This frame is shown if the budget is spent, the cap is reached, or
        // it is the Nth since the last rasterised one.
        emulated++;
        e->render_phase++;

        is_last = (emulated >= e->frame_budget) ||
                  ((unsigned)(get_tick_count() - t0) >= GBC_SLICE_MS);

        gbc_render_enabled = gbc_render_user &&
                             (is_last && (e->render_phase >= GBC_RENDER_EVERY));

        if (gbc_render_enabled)
            e->render_phase = 0;

        gbc_panicked = 0;
        gbc_step_frame(e->state);

        if (gbc_panicked)
        {
            gbc_render_enabled = gbc_render_user;
            e->panicked = 1;
            return -1;
        }

        if (e->cart)
            gbc_cam_step(e->cart, GB_FREQ / 60);

        e->frames++;

        if (is_last)
            break;
    }

    // Leave the flag as the user set it so the HUD reflects the real setting.
    rendered_this_slice = gbc_render_enabled;
    gbc_render_enabled = gbc_render_user;

    t1 = (unsigned)get_tick_count();
    gbc_ms_emu = t1 - t0;
    gbc_frames_per_slice = emulated;
    gbc_fps_account(emulated);

    // Nothing new was drawn into the pixbuf unless the last frame rasterised,
    // so there is nothing to push.
    if (rendered_this_slice)
        gui_lcd_render_frame(e->state->gb_type == GB_TYPE_CGB,
                             e->state->emu_state->lcd_pixbuf);

    gbc_ms_blit = (unsigned)get_tick_count() - t1;

    // The meta controls are applied here rather than where the key was read.
    //
    // Both of them are slow in their own way: shooting_set_zoom() is a
    // firmware call that returns when the motor has finished, and a tint
    // change throws away the shadow buffer and forces a full repaint of every
    // pixel. Neither belongs inside gui_input_poll(), which runs from the
    // frame callback with the interpreter's stack under it.
    if (gbc_set_hue(conf.gbc_hue))
        gbc_invalidate_screen();

    if (gbc_zoom_want >= 0)
    {
        int want = gbc_zoom_want;
        int max  = zoom_points - 1;

        if (max < 0) max = 0;
        if (want < 0)   want = 0;
        if (want > max) want = max;

        gbc_zoom_want = -1;
        if (want != shooting_get_zoom())
        {
            shooting_set_zoom(want);
            // Moving the lens is the one thing here that can make the firmware
            // draw: it has its own zoom readout and its own reasons to repaint.
            // Throw both the screen and the side bars away so the next frame
            // puts the Game Boy back over whatever landed.
            gbc_invalidate_screen();
            gbc_surround_dirty = 1;
        }
        // Nothing to invalidate afterwards: gbc_feed_capture() reads the live
        // viewport at the moment the ROM asks for a picture, so the next
        // capture already sees whatever the lens is now pointed at.
    }

    gbc_strip_draw();

    // Pending album writes, one unit per frame.
    //
    // Deliberately last, and deliberately here rather than where the request
    // was raised. The mapper only sets a flag; doing the card write inside the
    // emulated memory access blocked the GUI task long enough for DryOS to
    // switch the camera off, from a stack already several frames deep in the
    // interpreter. Out here it is the module's own stack, after the frame has
    // been drawn, and each call writes at most one file.
    if (e->cart)
        gbc_save_service(e->cart);

    // And the ordinary kind: the core raises flush_extram when the ROM turns
    // cartridge RAM back off, which is what a cartridge uses as its commit
    // signal, and extram_dirty when anything was actually written.
    //
    // The timer is the backstop. A game that leaves its RAM enabled - or that
    // is switched off at the body rather than saved and quit - would otherwise
    // have nothing to trigger the write, and a save you have to quit cleanly
    // to keep is not a battery.
    gbc_save_extram_service(e, 0);

    return 0;
}
