//-------------------------------------------------------------------
// The record screen's controls, drawn by CHDK instead of by Canon.
//
// Two things live here:
//
//   the arrow selectors  - flash on RIGHT, focus on LEFT, drive on DOWN, each
//                          a press-cycles-and-commits readout;
//   the FUNC menu        - SET, a full replacement for Canon's.
//
// Why none of this goes through Canon. The persistent overlay takes Canon's
// screen lock for as long as it is up and wipes the bitmap on the way in
// (posd_canon_osd_begin() in gui.c), which is what stops the stock OSD tearing
// through the plates. The side effect was that Canon's own popups stopped being
// drawn - they are Canon's to draw, and Canon has been told not to draw.
//
// The first attempt handed the screen back to Canon for five seconds after any
// press, because its popups are stateful selectors that commit on a timeout.
// That is gone. Reversing showed the popup is a way of writing the property,
// not a cache in front of it: the shoot sequence re-reads the property store as
// it starts and cannot tell who wrote it. See docs/A480_UI_REVERSING.md -
// SsFcsCtrl.c re-reads FOCUS_MODE / REAL_FOCUS_MODE at every capture, and
// SsShootCtrl.c, SsStrobeCtrl.c and SsExpCtrl.c do the same for FLASH_MODE and
// DRIVE_MODE.
//
// So every key on the record screen is taken before the firmware sees it, the
// properties are written here, and Canon never needs the bitmap back at all.
// There is no handover left, and nothing that needs to detect a Canon menu -
// which matters, because on this body there is no way to detect one. See the
// note on canon_shoot_menu_active at the foot of this file.
//-------------------------------------------------------------------

#include "platform.h"
#include "camera_info.h"
#include "stdlib.h"
#include "keyboard.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "theme.h"
#include "properties.h"
#include "shooting.h"
#include "gui_recui.h"

#ifdef CAM_RECUI

//-------------------------------------------------------------------
// How long an arrow readout stays up after the last press. Nothing depends on
// it - the value is committed on the press - so this is purely reading time.
#define RECUI_SHOW_MS    900

//-------------------------------------------------------------------
// Controls.
//
// Everything is a property write except ISO, which goes through CHDK's
// per-port iso_table (platform/a480/shooting.c) rather than a raw propcase, and
// EV, whose value is computed rather than looked up.
//
// Value maps are per propset, and that is a hard boundary rather than a
// tidiness one. A propcase *id* moves between propsets and the headers handle
// that; the values behind it do not follow, and nothing in the build would
// notice. Writing propset 2's idea of "flash off" into propset 1's FLASH_MODE
// produces a legal write of a number that means something else - which the
// shoot sequence then honours, because it cannot tell who wrote it.
//
// So the maps below are fenced by CAM_PROPSET and a port whose propset has no
// map fails to build. That is deliberate: for this menu the only failure worth
// having is the loud one. Which entries are confirmed and which are inferred
// is recorded in docs/A480_UI_REVERSING.md - the labels are the uncertain
// part, never the write, and a wrong label is cosmetic and reversible.

enum { RK_PROP, RK_EV, RK_ISO };

typedef struct {
    const char         *label;
    unsigned char       kind;
    unsigned short      prop;
    unsigned char       n;
    const unsigned short *vals;     // NULL for RK_EV / RK_ISO
    const char *const  *names;
} recui_ctl;

#if CAM_PROPSET == 2

//-- the three arrow controls ------------------------------------------------

static const unsigned short v_flash[] = { 0, 1, 2 };
static const char *const    n_flash[] = { "AUTO", "ON", "OFF" };

// Focus folds two properties in CHDK's reported space: FOCUS_MODE is the AF/MF
// flag and REAL_FOCUS_MODE carries normal/macro/infinity. This body has
// CAM_HAS_MANUAL_FOCUS undefined - no native MF - so the three offered here are
// the three the physical key offers, and the MF flag is only ever cleared.
static const unsigned short v_focus[] = { 0, 1, 3 };
static const char *const    n_focus[] = { "NORMAL", "MACRO", "INFINITY" };

//-- the FUNC menu -----------------------------------------------------------

// EXPCompController (FUN_ffd16874) bounds its index with `> 0xb` going up and
// `< 1` going down, and steps the property by 0x20 - so thirteen positions,
// 1/3 EV apart, centred on index 6. That is +/-2 EV, which is what the body
// offers. The property is EV_CORRECTION_2 (0xcf), written directly by that
// controller, so this is the same write Canon makes.
#define RECUI_EV_N      13
#define RECUI_EV_MID    6
#define RECUI_EV_STEP   32          // 1/3 EV in 1/96 EV units
#define RECUI_EV_PROP   PROPCASE_EV_CORRECTION_2

static const char *const n_ev[RECUI_EV_N] = {
    "-2", "-1 2/3", "-1 1/3", "-1", "-2/3", "-1/3", "0",
    "+1/3", "+2/3", "+1", "+1 1/3", "+1 2/3", "+2"
};

// ISO is the one row with no table here at all. It is not a propcase - it goes
// through shooting_set_iso_mode(), whose index space is the port's own
// iso_table[] in platform/<cam>/shooting.c - so the names and the count both
// come from there at runtime, via recui_count() and recui_label() below.
//
// It was a fixed list of seven, which is this body's table. That is a live
// hazard rather than an untidiness: the A410's iso_table[] has four entries, so
// the fixed count would have let the row walk to index 6 and hand
// shooting_set_iso_mode() an index two past the end of the table it indexes.

static const unsigned short v_wb[] = { 0, 1, 2, 3, 4, 5, 7 };
static const char *const    n_wb[] = { "AUTO", "DAYLIGHT", "CLOUDY", "TUNGSTEN",
                                       "FLUOR.", "FLUOR. H", "CUSTOM" };

static const unsigned short v_mycol[] = { 0, 1, 2, 3, 4 };
static const char *const    n_mycol[] = { "OFF", "VIVID", "NEUTRAL", "B/W", "SEPIA" };

#elif CAM_PROPSET == 1

//===========================================================================
// Propset 1 - the VxWorks bodies. Derived for the A410 from
// cameras/a410/ghidra/, and every entry below is here because something in
// that ROM or in CHDK's own cross-propset headers says so. The rows that
// could not be established are *absent*, not guessed: a missing row is a
// control you still have on Canon's UI, a wrong one is a silent mis-write.
//===========================================================================

//-- the three arrow controls ------------------------------------------------

// 0/1/2 = Auto/On/Off, and this one is nailed down three independent ways:
// every propsetN.h in the tree documents those three values identically;
// core/shooting.c's own shooting_is_flash_ready() tests `t != 2` for "flash
// not off" with no propset guard; and this body's ShootSeqTool.c agrees -
// FUN_ffd2f9f4 reads propcase 0x10 as the shot starts and returns `== 2`.
static const unsigned short v_flash[] = { 0, 1, 2 };
static const char *const    n_flash[] = { "AUTO", "ON", "OFF" };

// 0 = Normal, 1 = Macro, 3 = Infinity. Documented identically in propset 2, 6,
// 9 and 12 - the *id* moves between propsets (11 here, 6 there), the values do
// not. CAM_HAS_MANUAL_FOCUS is undefined on this body, so 4 (MF) is not
// offered and the MF flag is only ever cleared.
static const unsigned short v_focus[] = { 0, 1, 3 };
static const char *const    n_focus[] = { "NORMAL", "MACRO", "INFINITY" };

//-- the FUNC menu -----------------------------------------------------------

// EV writes EV_CORRECTION_**1**, not _2, and that is the one place this body
// genuinely differs from the A480 rather than merely being numbered
// differently.
//
// On the A480 the menu writes _2 because EXPCompController writes _2. Here,
// ShootController.c (FUN_ffd654b4) and FuncComponent.c (FUN_ffd98990) both do
//
//     GetPropertyCase(0x19, buf, 2);   // EV_CORRECTION_1
//     SetPropertyCase(0x1a, buf, 2);   // EV_CORRECTION_2
//
// as the shot starts - _2 is a copy of _1, refreshed at capture. A menu
// writing _2 here would be overwritten by that copy before the exposure was
// computed, and would appear to do nothing at all.
//
// The range is the inferred part: 13 positions 1/3 EV apart is what the body
// offers and 1/96 EV units are CHDK-wide, but no controller in this ROM writes
// 0x19 directly (only Index.c's defaults do), so the bounds are not reversed
// the way the A480's were. A wrong range here costs exposure compensation
// reach, which is visible, bounded and reversible.
#define RECUI_EV_N      13
#define RECUI_EV_MID    6
#define RECUI_EV_STEP   32          // 1/3 EV in 1/96 EV units
#define RECUI_EV_PROP   PROPCASE_EV_CORRECTION_1   // _1, not _2 - see above

static const char *const n_ev[RECUI_EV_N] = {
    "-2", "-1 2/3", "-1 1/3", "-1", "-2/3", "-1/3", "0",
    "+1/3", "+2/3", "+1", "+1 1/3", "+1 2/3", "+2"
};

// WB and MY COLORS carry over verbatim: propset1.h and propset2.h document
// the same value lists against different ids, so these two are the only maps
// in this file that did not need reversing.
static const unsigned short v_wb[] = { 0, 1, 2, 3, 4, 5, 7 };
static const char *const    n_wb[] = { "AUTO", "DAYLIGHT", "CLOUDY", "TUNGSTEN",
                                       "FLUOR.", "FLUOR. H", "CUSTOM" };

static const unsigned short v_mycol[] = { 0, 1, 2, 3, 4 };
static const char *const    n_mycol[] = { "OFF", "VIVID", "NEUTRAL", "B/W", "SEPIA" };

#else  // CAM_PROPSET

#error CAM_RECUI requires value maps for this camera propset; see docs/A480_UI_REVERSING.md

#endif // CAM_PROPSET

//===========================================================================
// Per-camera rows.
//
// Flash, focus, WB, MY COLOR and EV above are propset-wide - their values are
// documented identically across the propsets that share them, or were checked
// against the shoot sequence. These three are not, and are supplied by
// platform_camera.h instead:
//
//   DRIVE     the third position is "cont AF" on some bodies and the self
//             timer on others, and no header settles it per camera.
//   SIZE      RESOLUTION's values genuinely differ - 0/1/2/4 on this body,
//             0/1/2/3/4 on the A480 - so there is no propset answer at all.
//   QUALITY   no propsetN.h documents its positions.
//
// A camera that does not define a row does not get it, and that is a working
// menu with one fewer row rather than a build failure - unlike the propset
// maps above, which are wrong rather than missing if absent.
//
// Dimensions are optional on top of SIZE: supply CAM_RECUI_SIZE_DIMS only
// where a real <w,h> table was read out of that body's firmware. Without it
// the row still works and the detail column stays blank.
//===========================================================================

#ifndef CAM_RECUI_DRIVE_VALS
#error CAM_RECUI needs CAM_RECUI_DRIVE_VALS / _NAMES in platform_camera.h. \
DRIVE_MODE's third value is "continuous AF" on some bodies and the self timer \
on others; pick from this camera's firmware, not from another port.
#endif

static const unsigned short v_drive[] = CAM_RECUI_DRIVE_VALS;
static const char *const    n_drive[] = CAM_RECUI_DRIVE_NAMES;
#define RECUI_DRIVE_N   ((unsigned char)(sizeof(v_drive)/sizeof(v_drive[0])))

#ifdef CAM_RECUI_SIZE_VALS
static const unsigned short v_size[] = CAM_RECUI_SIZE_VALS;
static const char *const    n_size[] = CAM_RECUI_SIZE_NAMES;
#define RECUI_SIZE_N    ((unsigned char)(sizeof(v_size)/sizeof(v_size[0])))
#ifdef CAM_RECUI_SIZE_DIMS
static const unsigned short d_size[][2] = CAM_RECUI_SIZE_DIMS;
#define RECUI_HAVE_SIZE_TABLE   1
typedef char recui_size_dims_must_match[
    (sizeof(d_size)/sizeof(d_size[0]) == RECUI_SIZE_N) ? 1 : -1];
#endif
#endif

#ifdef CAM_RECUI_QUALITY_VALS
static const unsigned short v_qual[] = CAM_RECUI_QUALITY_VALS;
static const char *const    n_qual[] = CAM_RECUI_QUALITY_NAMES;
#define RECUI_QUAL_N    ((unsigned char)(sizeof(v_qual)/sizeof(v_qual[0])))
#endif

enum { RECUI_FLASH = 0, RECUI_FOCUS, RECUI_DRIVE, RECUI_NCTL };

static const recui_ctl recui_ctls[RECUI_NCTL] = {
    { "FLASH", RK_PROP, PROPCASE_FLASH_MODE,      3, v_flash, n_flash },
    { "FOCUS", RK_PROP, PROPCASE_REAL_FOCUS_MODE, 3, v_focus, n_focus },
    { "DRIVE", RK_PROP, PROPCASE_DRIVE_MODE,      RECUI_DRIVE_N, v_drive, n_drive },
};

static const recui_ctl recui_menu[] = {
    { "EV",       RK_EV,   RECUI_EV_PROP,        RECUI_EV_N,    0,       n_ev    },
    { "ISO",      RK_ISO,  0,                    0,             0,       0       },
    { "WB",       RK_PROP, PROPCASE_WB_MODE,     7,             v_wb,    n_wb    },
    { "MY COLOR", RK_PROP, PROPCASE_MY_COLORS,   5,             v_mycol, n_mycol },
    { "DRIVE",    RK_PROP, PROPCASE_DRIVE_MODE,  RECUI_DRIVE_N, v_drive, n_drive },
#ifdef CAM_RECUI_SIZE_VALS
    { "SIZE",     RK_PROP, PROPCASE_RESOLUTION,  RECUI_SIZE_N,  v_size,  n_size  },
#endif
#ifdef CAM_RECUI_QUALITY_VALS
    { "QUALITY",  RK_PROP, PROPCASE_QUALITY,     RECUI_QUAL_N,  v_qual,  n_qual  },
#endif
};

#define RECUI_MENU_N ((int)(sizeof(recui_menu)/sizeof(recui_menu[0])))

// Which selector an arrow opens on the record screen, or -1 for a key this
// body does not take.
//
// On a camera with no separate zoom control the up and down arrows *are* the
// zoom rocker - see CAM_RECUI_UPDOWN_IS_ZOOM - and neither can be taken for
// anything, however free they look. DRIVE is not lost with them: it is in the
// FUNC menu below, which is where every control lives whether or not it also
// has an arrow.
static int recui_ctl_for_key(long key)
{
    if (key == KEY_RIGHT) return RECUI_FLASH;
    if (key == KEY_LEFT)  return RECUI_FOCUS;
#ifndef CAM_RECUI_UPDOWN_IS_ZOOM
    if (key == KEY_DOWN)  return RECUI_DRIVE;
#endif
    return -1;
}

//-------------------------------------------------------------------
// State.

static int recui_sel_ctl = -1;      // arrow readout on screen, -1 = none
static int recui_sel_idx;
static int recui_shown_until;
static int recui_shown;
static int recui_dirty;

static int recui_menu_open;

// The key that closed or opened something, held until it is physically
// released. Returning 1 from recui_kbd() blocks the keyboard for *that tick*
// only; a press that acts on its click edge and then stops blocking leaves the
// key still down on the ticks after, and Canon acts on it. That is what made
// closing the FUNC menu with SET sometimes open Canon's own menu underneath.
//
// The arrows below are blocked unconditionally while down. SET and MENU cannot
// be - MENU is how Canon's menu is reached and taking it outright would strand
// the camera - so they are swallowed only for the remainder of the press this
// code consumed.
static long recui_swallow;
#ifdef CAM_BEND_ENTER_UP
static int  recui_up_held;      // UP still down from the press that left bend mode
#endif
static int recui_menu_row;
static int recui_menu_dirty;

//-------------------------------------------------------------------
// Reading and writing a control.

// How many positions this control has, and what each is called. Everything but
// ISO answers from its own table; ISO answers from the port's iso_table[], so
// neither the count nor the labels can drift from the index space that
// shooting_set_iso_mode() actually uses.
static int recui_count(const recui_ctl *c)
{
    extern const ISOTable iso_table[];
    extern const unsigned int ISO_SIZE;
    (void)iso_table;
    return (c->kind == RK_ISO) ? (int)ISO_SIZE : c->n;
}

static const char *recui_label(const recui_ctl *c, int idx)
{
    extern const ISOTable iso_table[];
    if (c->kind == RK_ISO)
        return iso_table[idx].name;
    return c->names[idx];
}

static int recui_get(const recui_ctl *c)
{
    int i;

    switch (c->kind)
    {
    case RK_EV:
    {
        // Read back the property the row *writes*, not a fixed accessor. On
        // propset 2 that is EV_CORRECTION_2 and this is what
        // shooting_get_ev_correction2() did; on propset 1 the row writes
        // EV_CORRECTION_1, and reading _2 there would report a value the shot
        // sequence had overwritten from _1 rather than the one just set.
        int ev = (short)shooting_get_prop(c->prop);
        i = RECUI_EV_MID + (ev / RECUI_EV_STEP);
        break;
    }
    case RK_ISO:
        i = shooting_get_iso_mode();
        break;
    default:
    {
        short v = shooting_get_prop(c->prop);
        for (i = 0; i < c->n; i++)
            if (c->vals[i] == v)
                return i;
        i = 0;
        break;
    }
    }

    // Anything the camera reports that is not in the list clamps rather than
    // returning -1, so the next press stays meaningful whatever it came up in.
    if (i < 0)       i = 0;
    if (i >= recui_count(c)) i = recui_count(c) - 1;
    return i;
}

static void recui_set(const recui_ctl *c, int idx)
{
    switch (c->kind)
    {
    case RK_EV:
        shooting_set_prop(c->prop, (idx - RECUI_EV_MID) * RECUI_EV_STEP);
        break;

    case RK_ISO:
        shooting_set_iso_mode(idx);
        break;

    default:
        shooting_set_prop(c->prop, c->vals[idx]);
        // Focus only. shooting_get_real_focus_mode() reports macro and infinity
        // out of REAL_FOCUS_MODE, but only while FOCUS_MODE says AF - if the MF
        // flag is set it wins and the selection would not read back. Clearing
        // it costs nothing on a body with no native MF.
        if (c->prop == PROPCASE_REAL_FOCUS_MODE)
            shooting_set_prop(PROPCASE_FOCUS_MODE, 0);
        break;
    }
}

// The right-hand column: what the value on this row actually means, for the
// rows where the name alone does not say. Empty for the rest - a column of
// mostly-noise would cost more than it gives.
//
// Only ever called from the menu repaint, which happens on a keypress, so the
// firmware calls in here are not on the per-pass path.
static void recui_detail(const recui_ctl *c, int idx, char *buf)
{
    buf[0] = 0;

    if (c->kind == RK_ISO)
    {
        // Only for AUTO: on a fixed setting the name is already the number, and
        // repeating it would just be clutter.
        if (idx == 0)
            sprintf(buf, "= %d", (int)shooting_get_iso_real());
    }
#ifdef RECUI_HAVE_SIZE_TABLE
    else if (c->prop == PROPCASE_RESOLUTION &&
             idx < (int)(sizeof(d_size)/sizeof(d_size[0])))
    {
        sprintf(buf, "%dx%d", d_size[idx][0], d_size[idx][1]);
    }
#endif
}

//-------------------------------------------------------------------
// Keys.

// Ask for an erase-and-repaint the moment this UI appears.
//
// Gating the grid on recui_active() stops it being *re*drawn over the menu,
// but the pixels already on screen from earlier passes stay there. The plates
// and the menu box repaint themselves; the grid is drawn from gui.c and has no
// idea anything happened. One restore on the way in clears the frame, and the
// grid then stays off because its condition is now false.
//
// Only on the transition - a restore every pass would erase the menu as fast
// as it was drawn.
static void recui_became_active(void)
{
    if (!recui_active())
        gui_set_need_restore();
}

int recui_active(void)
{
    return recui_menu_open || recui_sel_ctl >= 0;
}

// The interlock, and it is the one Canon uses. StrobeController's guard
// (FUN_ffd11674, which logs "UI_StrobeCon_BusyOfShootSeq") refuses a mode
// change while the shoot sequence is running, and writing a property mid-
// capture would land in the middle of the sequence that is reading it.
static int recui_can_change(void)
{
    return !shooting_in_progress()
        && !camera_info.state.is_shutter_half_press
        && !kbd_is_key_pressed(KEY_SHOOT_HALF)
        && !kbd_is_key_pressed(KEY_SHOOT_FULL);
}

static int recui_on_record_screen(void)
{
    extern int  canon_menu_active;
    extern char canon_shoot_menu_active;

    // The last two are Canon's menus, and they are not covered by gui_mode_none
    // above - that reports CHDK's own GUI mode, and Canon opening a menu does
    // not change it. Without them this took LEFT/RIGHT/UP/DOWN back off the
    // Canon menu the MENU key had just opened, so RIGHT raised the flash
    // readout instead of moving the selection, and the menu could not be
    // navigated at all.
    //
    // Same expression as posd_screen_active() in core/gui.c, deliberately: the
    // overlay and this have to agree about who owns the record screen, and the
    // two drifting apart is what produced the bug. `== &canon_menu_active-4` is
    // CHDK's standing idiom for "no menu", used the same way in core/gui_osd.c.
    //
    // canon_shoot_menu_active is inert on the A480 - stubs_min.S DEFs it to a
    // ROM address that reads a constant 0 - so it costs nothing there and works
    // on the A410, where it is real RAM at 0x7A11.
    return camera_info.state.mode_rec
        && !camera_info.state.mode_play
        &&  camera_info.state.gui_mode_none
        && !camera_info.state.gui_mode_alt
        && !gui_bend_active()
        && (canon_menu_active == (int)&canon_menu_active - 4)
        && !canon_shoot_menu_active;
}

// Called from kbd_process(). Returns 1 when it has taken the keys, which makes
// kbd_process() return non-zero and CHDK overwrite physw_status - so the
// firmware never sees the press.
static inline __attribute__((always_inline)) int recui_menu_kbd(long key)
{
    // Half press closes and shoots, the way it does in Canon's menu. The keys
    // go back to Canon on this pass so the shot is not swallowed.
    if (kbd_is_key_pressed(KEY_SHOOT_HALF) || kbd_is_key_pressed(KEY_SHOOT_FULL))
    {
        recui_menu_open  = 0;
        recui_menu_dirty = 1;
        return 0;
    }

    if (key == KEY_SET || key == KEY_MENU)
    {
        recui_menu_open  = 0;
        recui_menu_dirty = 1;
        recui_swallow    = key;
    }
    else if (key == KEY_UP || key == KEY_DOWN)
    {
        int step = (key == KEY_UP) ? RECUI_MENU_N - 1 : 1;
        recui_menu_row = (recui_menu_row + step) % RECUI_MENU_N;
        recui_menu_dirty = 1;
    }
    else if (key == KEY_LEFT || key == KEY_RIGHT)
    {
        if (recui_can_change())
        {
            const recui_ctl *c = &recui_menu[recui_menu_row];
            int count = recui_count(c);
            int step = (key == KEY_RIGHT) ? 1 : count - 1;
            recui_set(c, (recui_get(c) + step) % count);
        }
        recui_menu_dirty = 1;
    }

    return 1;
}

static inline __attribute__((always_inline)) void recui_open_menu(long key)
{
    recui_became_active();
    recui_menu_open  = 1;
    recui_menu_dirty = 1;
    recui_sel_ctl    = -1;
    recui_swallow    = key;
}

static inline __attribute__((always_inline)) void recui_select_ctl(int ctl)
{
    const recui_ctl *c = &recui_ctls[ctl];
    int idx = recui_get(c);

    if (recui_can_change())
    {
        idx = (idx + 1) % recui_count(c);
        recui_set(c, idx);
    }
    recui_became_active();
    recui_sel_ctl     = ctl;
    recui_sel_idx     = idx;
    recui_dirty       = 1;
    recui_shown_until = get_tick_count() + RECUI_SHOW_MS;
}

static inline __attribute__((always_inline)) int recui_control_key_down(void)
{
    if (recui_swallow && !kbd_is_key_pressed(recui_swallow))
        recui_swallow = 0;

    if (recui_swallow || kbd_is_key_pressed(KEY_LEFT) ||
        kbd_is_key_pressed(KEY_RIGHT))
        return 1;
#ifndef CAM_RECUI_UPDOWN_IS_ZOOM
    if (kbd_is_key_pressed(KEY_DOWN) || kbd_is_key_pressed(KEY_UP))
        return 1;
#endif
    return 0;
}

int recui_kbd(void)
{
    long key;
    int  ctl;

    if (!recui_on_record_screen())
    {
        recui_sel_ctl   = -1;
        recui_menu_open = 0;
        recui_swallow   = 0;
#ifdef CAM_BEND_ENTER_UP
        // Leaving bend mode lands here on the way back to the record screen,
        // and the UP that closed it is very likely still down. Remember that so
        // the press cannot be read a second time as a request to reopen - one
        // press, one thing, however the click edges fall around the mode
        // switch.
        recui_up_held = kbd_is_key_pressed(KEY_UP);
#endif
        return 0;
    }

#ifdef CAM_BEND_ENTER_UP
    if (recui_up_held)
    {
        if (kbd_is_key_pressed(KEY_UP))
            return 1;               // still down: block it, act on nothing
        recui_up_held = 0;
    }
#endif

    key = kbd_get_clicked_key();

    //-- the FUNC menu ------------------------------------------------------
    if (recui_menu_open)
        return recui_menu_kbd(key);

    //-- opening it ---------------------------------------------------------
    if (key == KEY_SET)
    {
        recui_open_menu(key);
        return 1;
    }

#ifdef CAM_BEND_ENTER_UP
    //-- bend mode ----------------------------------------------------------
    // UP is the free key on this screen - see the note by the block list at
    // the bottom of this function - so bend mode takes it, and a press is a
    // better gesture for the thing you reach for most than a second-long hold
    // on the ALT button was.
    //
    // Refused mid-capture for the same reason the selectors are: entering
    // switches GUI mode, which erases and repaints the frame, and the shoot
    // sequence is the one time that is not ours to do.
    if (key == KEY_UP)
    {
        if (recui_can_change())
        {
            recui_sel_ctl   = -1;       // drop any readout on the way out
            recui_menu_open = 0;
            gui_bend_enter();
        }
        return 1;
    }
#endif

    //-- the arrow selectors ------------------------------------------------
    ctl = recui_ctl_for_key(key);
    if (ctl >= 0)
        recui_select_ctl(ctl);

    if (recui_sel_ctl >= 0 && (int)(get_tick_count() - recui_shown_until) >= 0)
        recui_sel_ctl = -1;

    // Block only while a control key is physically down. Returning 1 hides the
    // *whole* keyboard from Canon, so holding it for the readout's two seconds
    // would take the shutter with it. The hold has to cover the press and not
    // just the click edge, because Canon opens its own selector off the key
    // still being down on the ticks after it.
    //
    // KEY_UP is in the list but bound to nothing. Left to Canon it opens a
    // selector that never dismisses itself; CHDK then wipes the bitmap and
    // Canon repaints only the frame it believes is still there, which is an
    // empty white box that stays on screen. It is a free key for the taking.
    //
    // Except where it is the zoom. On a body with no separate zoom control the
    // up and down arrows drive the lens, and blocking them here is not taking
    // a free key - it is taking the zoom off the camera on the one screen it
    // is for. Both go to Canon untouched there.
    //
    // Only on the record screen. The FUNC menu returns above this with the
    // whole keyboard swallowed, so while it is open the arrows still walk its
    // rows and the lens stays where it is.
    return recui_control_key_down();
}

//-------------------------------------------------------------------
// Drawing.

#define RECUI_PAD       6

// Arrow readout. Sized to the widest value name so the box does not change
// width as the selection cycles - a box that resizes under a repeated press
// reads as two popups rather than one.
#define RECUI_CHARS     14
#define RECUI_W         (FONT_WIDTH * RECUI_CHARS + RECUI_PAD * 2)
#define RECUI_H         (FONT_HEIGHT * 2 + RECUI_PAD * 2)

// FUNC menu. Label column, value column, detail column.
#define RECUI_M_LBL     9
#define RECUI_M_VAL     11
#define RECUI_M_DET     10
#define RECUI_M_CHARS   (RECUI_M_LBL + RECUI_M_VAL + RECUI_M_DET)
#define RECUI_M_W       (FONT_WIDTH * RECUI_M_CHARS + RECUI_PAD * 2)
#define RECUI_M_H       (FONT_HEIGHT * RECUI_MENU_N + RECUI_PAD * 2)

static int recui_x(void)   { return (camera_screen.width - RECUI_W) / 2; }
static int recui_y(void)   { return FONT_HEIGHT * 3; }
// Centred, both ways. It was pinned to the left margin when it was the only
// thing on the screen; it is not - the arrow readouts, the bend strip and the
// overlay plates all sit against one edge or another, and a menu hard against
// the left with a hand's width of gap on the right reads as one of them rather
// than as the thing that has taken over the screen.
static int recui_m_x(void) { return (camera_screen.width - RECUI_M_W) / 2; }
static int recui_m_y(void) { return (camera_screen.height - RECUI_M_H) / 2; }

static void recui_pad(char *d, const char *s, int w)
{
    int i = 0;
    while (s[i] && i < w) { d[i] = s[i]; i++; }
    while (i < w) d[i++] = ' ';
    d[w] = 0;
}

static void recui_draw_menu(void)
{
    color bg = BG_COLOR(user_color(conf.menu_color));
    color fg = FG_COLOR(user_color(conf.menu_color));
    int x = recui_m_x(), y = recui_m_y();
    int i;
    char buf[RECUI_M_CHARS + 1];

    draw_rectangle(x, y, x + RECUI_M_W, y + RECUI_M_H,
                   MAKE_COLOR(bg, fg), RECT_BORDER1|DRAW_FILLED);

    for (i = 0; i < RECUI_MENU_N; i++)
    {
        const recui_ctl *c = &recui_menu[i];
        int ty  = y + RECUI_PAD + i * FONT_HEIGHT;
        int sel = (i == recui_menu_row);
        int idx = recui_get(c);
        color rb = sel ? theme_color(TC_HILITE) : bg;
        char det[RECUI_M_DET + 12];

        recui_pad(buf, c->label, RECUI_M_LBL);
        draw_string(x + RECUI_PAD, ty, buf,
                    MAKE_COLOR(rb, sel ? theme_color(TC_HILITE_FG) : fg));

        recui_pad(buf, recui_label(c, idx), RECUI_M_VAL);
        draw_string(x + RECUI_PAD + FONT_WIDTH * RECUI_M_LBL, ty, buf,
                    MAKE_COLOR(rb, sel ? theme_color(TC_HILITE_FG)
                                       : theme_color(TC_ACCENT)));

        // Grey, so it reads as annotation rather than as another value that
        // might be editable.
        recui_detail(c, idx, det);
        recui_pad(buf, det, RECUI_M_DET);
        draw_string(x + RECUI_PAD + FONT_WIDTH * (RECUI_M_LBL + RECUI_M_VAL), ty,
                    buf, MAKE_COLOR(rb, sel ? theme_color(TC_HILITE_FG)
                                            : theme_color(TC_DIM)));
    }
}

static void recui_erase(int x, int y, int w, int h)
{
    draw_rectangle(x, y, x + w, y + h,
                   MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                   RECT_BORDER0|DRAW_FILLED);
}

// Called from gui_redraw() every pass. Cheap by construction: it repaints only
// when something changed or it has just appeared, which is the rule the
// overlay's rows follow, and for the same reason - see the tearing note above
// gui_draw_persistent_osd().
//
// The menu is the exception and reads its properties back on each repaint. It
// only repaints on a keypress, so that is a handful of firmware calls when a
// key moves, not a per-pass cost.
void recui_draw(int force)
{
    const recui_ctl *c;
    color bg, fg;
    int x, y, i, idx;
    char buf[RECUI_CHARS + 1];

    if (recui_menu_open)
    {
        if (force || recui_menu_dirty)
        {
            recui_draw_menu();
            recui_menu_dirty = 0;
            recui_shown = 0;        // any arrow readout is gone behind it
        }
        return;
    }

    if (recui_menu_dirty)
    {
        recui_erase(recui_m_x(), recui_m_y(), RECUI_M_W, RECUI_M_H);
        recui_menu_dirty = 0;
        gui_set_need_restore();     // let the overlay repaint under it
    }

    if (recui_sel_ctl < 0)
    {
        if (recui_shown)
        {
            recui_erase(recui_x(), recui_y(), RECUI_W, RECUI_H);
            recui_shown = 0;
            // Same restore the menu box asks for on its way out, and for the
            // same reason now that the grid is gated on recui_active(): this
            // is the moment the grid is allowed back, and it will not repaint
            // itself without being asked.
            gui_set_need_restore();
        }
        return;
    }

    if (!force && recui_shown && !recui_dirty)
        return;

    c   = &recui_ctls[recui_sel_ctl];
    idx = recui_sel_idx;        // cached by recui_kbd(), never read back here
    x   = recui_x();
    y   = recui_y();

    // Do not inherit Canon/menu palette background bytes here. On the A480 a
    // record-palette mismatch resolved that background as opaque white, which
    // looked exactly like the Canon popup this selector replaces and remained
    // over the grid until its timeout. Black is stable in every palette; the
    // text and border still follow the selected theme.
    bg = COLOR_BLACK;
    fg = theme_color(TC_TEXT);

    draw_rectangle(x, y, x + RECUI_W, y + RECUI_H,
                   MAKE_COLOR(bg, fg), RECT_BORDER1|DRAW_FILLED);

    for (i = 0; c->label[i] && i < RECUI_CHARS; i++) buf[i] = c->label[i];
    buf[i] = 0;
    draw_string(x + RECUI_PAD, y + RECUI_PAD, buf, MAKE_COLOR(bg, fg));

    {
        const char *nm = recui_label(c, idx);
        for (i = 0; nm[i] && i < RECUI_CHARS; i++) buf[i] = nm[i];
    }
    buf[i] = 0;
    draw_string(x + RECUI_PAD, y + RECUI_PAD + FONT_HEIGHT, buf,
                MAKE_COLOR(bg, theme_color(TC_ACCENT)));

    recui_shown = 1;
    recui_dirty = 0;
}

//-------------------------------------------------------------------
// On not being able to detect a Canon menu on this body.
//
// Kept because it will be tempting to try again. canon_shoot_menu_active is
// DEF'd to 0xFFC00414 in platform/a480/sub/100b/stubs_min.S, which is a ROM
// address - it reads a constant and never reports anything. The signature
// finder's candidate, commented out in stubs_entry.S at 0xffd16804, is
// EXPCompController (propcase 0xcf, events 0x81c/0x81d), not a menu flag.
//
// RecFuncContainer.c looked like the answer: FUN_ffcf2398 writes 1 to
// [literal 0xffcf2518 + 0x38] and FUN_ffcf2434 writes 0 to the same field. The
// literal reads 0x5d6c, giving 0x5da4. On hardware that address never changes
// while the FUNC menu is open, and neither does anything else in 0x5000-0x6800
// - a RAM differ over that window, triggered on the SET press, reported nothing.
//
// It stopped mattering: SET opens this file's menu now, so there is no Canon
// menu on the record screen to detect. Anything that needs the answer in future
// starts from the fact that the window above is already ruled out.
//-------------------------------------------------------------------

#endif // CAM_RECUI
