//-------------------------------------------------------------------
// Bend mode - the patchbay, on the shooting screen.
//
// See BENDING_DESIGN.md section 4. This is a GUI mode alongside <ALT>, entered
// by holding the ALT button (Print, by default) for a second - or, on a body
// that sets CAM_BEND_ENTER_UP, by the UP arrow on the shooting screen, which is
// a press rather than a hold and costs nothing because Canon has no use for
// that key here. MENU leaves either way. It draws a strip
// of the ten data pins across the top of the live view and lets the arrows
// repatch them, and - the point of the whole thing - the shutter still takes
// the picture while it is open. You do not dismiss anything to shoot.
//
// The strip is drawn in the OSD plane, which is composited by hardware and
// which nothing else writes, so it is rock steady even though the live view
// under it is not. See LIVEVIEW_NOTES.md for why that distinction matters.
//
// The arrows do not repatch a pin on their own. LEFT and RIGHT move the cursor
// along the strip; SET arms the pin under it, and only then do UP and DOWN walk
// the source list. With a segment layout on, the strip grows one more cell at
// its left end holding the region number, and it arms and turns exactly as a
// pin does - so which quarter of the sensor you are wiring is chosen with the
// same three presses as which source a pin is on, and the matrix it lands on
// is the one the rest of the strip edits and the one a reroll rewrites. SET again commits, MENU cancels back to what it was. Two
// stages rather than one because UP is also how the mode is entered on some
// bodies, and because a strip where every arrow press changes the sensor is one
// you cannot look through without changing.
//
// MENU opens the browser: a panel over the live view with two windows in it,
// walked with the arrows. Left and right change window, up and down move
// inside one. It is a panel rather than another field in the strip because a
// list needs more than one line, and because the things it is listing - a
// whole matrix, a whole profile - are not per-pin. Pressing MENU again puts it
// away; the way out of the mode is UP again where that is how it was entered,
// and the ALT hold everywhere.
//
//   SAVED         the bends on the card. What the strip above is editing.
//   EXPERIMENTAL  the profiles in bendx.c. Not a matrix at all: one named
//                 failure of the frame buffer, applied after the matrix.
//   SEG           how the frame is cut up, and which piece of it the strip is
//                 editing. Each region has its own matrix; the profiles are
//                 not cut up and run over the whole frame afterwards.
//
// SEG and SAVED are one loop when a layout is on: picking a region goes
// straight to the saved list for it, and picking a bend there comes back to
// the region list ready for the next one. So four corners is SET, down, SET,
// down, SET - not four trips out to the strip and back in through MENU.
//
// The three are orthogonal and every window says what is on, because "either,
// neither or both" is only usable if you can see which.
//-------------------------------------------------------------------

#include "platform.h"
#include "camera_info.h"
#include "stdlib.h"
#include "keyboard.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_osd.h"
#include "gui_lang.h"
#include "bend.h"
#include "bendx.h"
#include "bend_seg.h"
#include "bend_store.h"
#include "theme.h"

extern gui_handler bendGuiHandler;
#include "modules.h"
#include "module_load.h"

#ifdef CAM_BEND_MODE

//-------------------------------------------------------------------

#define BM_NBITS        CAM_SENSOR_BITS_PER_PIXEL

// Layout. Ten columns across 360 gives 36 each, which is exactly four
// characters of the 8px font with a 2px gutter - the reason source labels are
// capped at three characters plus an inversion tilde.
#define BM_COLW         (CAM_SCREEN_WIDTH / BM_NBITS)
#define BM_Y0           2                       // pin numbers
#define BM_Y1           (BM_Y0 + FONT_HEIGHT)   // sources
#define BM_H            (BM_Y1 + FONT_HEIGHT + 2)

// With a layout on, the strip grows a cell on the left holding the region
// number, and the pins give up three pixels each to pay for it. 33 still fits
// the four character source labels - it is the two pixel gutter that goes, not
// a character - and it only happens while segments are in use, so the strip
// everyone already knows is untouched until they ask for this.
//
// The cell is on the strip rather than only in the browser because the region
// is the first thing about a patch: ten pins mean nothing until you know which
// quarter of the sensor they are wired to. Same reason the pin numbers are on
// the strip and not in a menu.
#define BM_SEGW         28
#define BM_CUR_SEG      (-1)            // bm_cursor value for that cell

static int bm_seg_shown(void);
static int bm_segx(void);
static int bm_colw(void);
static void bm_erase_all(void);
static void bm_mask_erase(void);

// Status line at the foot of the screen rather than under the strip. The strip
// has to be at the top - it is the thing being edited and it should sit where
// it blocks least of the frame - but the status line is read once and then
// ignored, so keeping it up there only pushed the live view down twice over.
// At the bottom it also lands where the persistent OSD puts its plates, which
// is where the eye already goes for state on this camera.
#define BM_SY           (CAM_SCREEN_HEIGHT - FONT_HEIGHT - 2)
#define BM_SH           (FONT_HEIGHT + 4)

#define BM_SAVED_MS     1500    // how long a save/load confirmation is shown

static int bm_active;
// 0 .. BM_NBITS-1 indexes route[] directly. BM_CUR_SEG is the region cell,
// which only exists while a layout is on - see bm_move_cursor().
static int bm_cursor;

// Two-stage pin editing. The cursor picks which pin; SET arms it, and only an
// armed pin listens to the rocker of UP/DOWN. Without the arming step the two
// meanings of UP/DOWN - move along the list of sources, and do nothing because
// no pin is being edited - are indistinguishable, and worse, UP is now how the
// mode is entered: a press that opened the patchbay and then immediately
// repatched a pin is one gesture doing two things.
//
// The source is written live while armed, so what the sensor is doing and what
// the strip says never disagree. bm_arm_src is what it was before arming, which
// is what MENU puts back - SET commits, MENU cancels, and both are one press.
static int bm_armed;
static unsigned char bm_arm_src;
// The same two stages for the region cell: SET arms it, UP/DOWN walk the
// regions and the whole strip follows live, SET commits and MENU puts back the
// region that was showing when it was armed.
static int bm_arm_seg;
static volatile int bm_dirty;   // written by KBD task, read by GUI task
static volatile int bm_nav_only;// cursor/list highlight moved; underlying layers unchanged
static int bm_swallow_entry;    // suppress a key held across a GUI mode switch
static int bm_note_at;          // tick of the last save/load, for the message
static char bm_note[28];        // what that message says

// Browser state. Up here with the rest of the mode state rather than beside
// the browser code, because gui_bend_randomise() above it has to know whether
// the panel is up.
#define BM_TAB_SAVED    0
#ifdef CAM_BEND_EXPERIMENTAL
  #define BM_TAB_X      1
  #define BM_TAB_SEG    2
  #define BM_TAB_COUNT  3
#else
  #define BM_TAB_X      (-1)
  #define BM_TAB_SEG    1
  #define BM_TAB_COUNT  2
#endif

static int bm_browse;           // browser open
static int bm_tab;              // which window - BM_TAB_*
static int bm_bsel;             // selected row, 0 .. bm_browse_rows()-1
static int bm_btop;             // first list row scrolled into view
static int bm_br_drawn_h;       // height of the panel as last drawn, for the erase
#ifdef CAM_BEND_EXPERIMENTAL
// Which live-profile control the EXPERIMENTAL window is turning. Kept as UI
// state rather than config: it changes no photograph, only which knob the
// next Left/Right or rocker press reaches.
#define BM_X_AMOUNT  0
#define BM_X_REACH   1
#define BM_X_LANES   2
#define BM_X_MIX     3
#define BM_X_BANDS   4
#define BM_X_KNOBS   5
static int bm_x_knob;
static int bm_x_open = -1;
#endif

// Confirmation. Deleting a preset takes a file off the card, which is the only
// thing either window does that cannot be undone by pressing something else -
// so it is the only thing that asks first.
//
// Asked inside the panel rather than through gui_mbox_init(). A message box is
// modal and would take over the screen of a mode whose whole point is that the
// shutter still works while it is open; the panel is already drawn, already
// has the keys, and is already where the question is about.
#define BM_CONFIRM_NONE     0
#define BM_CONFIRM_DELETE   1

static int bm_confirm;          // BM_CONFIRM_*
static int bm_confirm_idx;      // preset index the question is about
static int bm_confirm_yes;      // 1 = Yes highlighted. Starts on No.

// Full preset inspector, opened by holding SET on a saved bend.
#define BM_DETAIL_ACTIONS 5
static int bm_detail;
static int bm_detail_idx;
static int bm_detail_action;
static bend_store_recipe_t bm_detail_recipe;
static bend_store_info_t bm_detail_info;
static char bm_detail_edit[BEND_STORE_DESC_MAX + 1];

//-------------------------------------------------------------------
// Strip geometry. Three lines rather than three constants because whether the
// region cell is there depends on the layout, which changes while the mode is
// open - from the SEG window, and from the menu between entries.

static int bm_seg_shown(void)
{
    return conf.bend_segs.layout != BSEG_OFF;
}

static int bm_segx(void)
{
    return bm_seg_shown() ? BM_SEGW : 0;
}

static int bm_colw(void)
{
    return (CAM_SCREEN_WIDTH - bm_segx()) / BM_NBITS;
}

// Left and right along the strip, which reads MSB first: the region cell, then
// pin 9 down to pin 0. route[] is indexed the other way, which is why moving
// left is counting up. With no layout on there is no cell and this is the wrap
// the strip has always had.
static void bm_move_cursor(int left)
{
    int cell = bm_seg_shown();

    if (left)
    {
        if (bm_cursor == BM_CUR_SEG)        bm_cursor = 0;
        else if (bm_cursor == BM_NBITS - 1) bm_cursor = cell ? BM_CUR_SEG : 0;
        else                                bm_cursor++;
    }
    else
    {
        if (bm_cursor == BM_CUR_SEG)        bm_cursor = BM_NBITS - 1;
        else if (bm_cursor == 0)            bm_cursor = cell ? BM_CUR_SEG : BM_NBITS - 1;
        else                                bm_cursor--;
    }
    bm_nav_only = 1;
}

// The layout can go away while the cursor is sitting on the cell - the SEG
// window is reachable without leaving the mode, and so is the menu. Called
// before anything reads bm_cursor, so the cell never outlives its layout.
static void bm_cursor_clamp(void)
{
    if (bm_cursor == BM_CUR_SEG && !bm_seg_shown()) bm_cursor = BM_NBITS - 1;
    if (bm_cursor >= BM_NBITS)                      bm_cursor = BM_NBITS - 1;
}

// The region knob. Walks the regions of the layout in use and makes each one
// active as it passes, so the pins below change with it and what is on the
// strip is always the matrix that would be applied there.
static void bm_step_seg(int delta)
{
    int n = bend_seg_n();
    int v;

    if (n < 2) return;
    v = bend_seg_active() + delta;
    while (v < 0)  v += n;
    while (v >= n) v -= n;
    bend_seg_set_active(v);

    // The card's cursor belongs to whichever region was loaded into last, so
    // it cannot go on claiming to describe this one.
    bend_store_detach();
}

//-------------------------------------------------------------------
// Source enumeration, for the up/down knob.
//
// The order is the order it makes sense to walk in, not the order of the
// encoding: all the data pins, then the same pins inverted, then the two
// rails, then the non-data buses, then those inverted.

#define BM_N_DATA       BM_NBITS
#define BM_I_NDATA      (BM_N_DATA)
#define BM_I_TIE        (BM_I_NDATA + BM_NBITS)
#define BM_I_BUS        (BM_I_TIE + 2)
#define BM_I_NBUS       (BM_I_BUS + BUS_COUNT)
#define BM_N_SRC        (BM_I_NBUS + BUS_COUNT)

// With the extra signals switched off the knob stops at the two rails, so a
// source that cannot be reached is also a source that cannot be selected. The
// alternative - letting it be selected and quietly ignoring it - is the thing
// that makes a setting feel broken.
static int bm_n_src(void)
{
    return conf.bitbend_simple ? BM_I_BUS : BM_N_SRC;
}

static unsigned char bm_src_at(int i)
{
    if (i < BM_I_NDATA) return (unsigned char)BSRC_DATA(i);
    if (i < BM_I_TIE)   return (unsigned char)BSRC_NDATA(i - BM_I_NDATA);
    if (i < BM_I_BUS)   return (unsigned char)((i == BM_I_TIE) ? BSRC_LOW : BSRC_HIGH);
    if (i < BM_I_NBUS)  return (unsigned char)BSRC_BUS(i - BM_I_BUS);
    return (unsigned char)BSRC_NBUS(i - BM_I_NBUS);
}

static int bm_index_of(unsigned char src)
{
    int idx = BSRC_IDX(src);
    switch (BSRC_CLASS(src))
    {
    case BSRC_C_DATA:   return (idx < BM_NBITS) ? idx : 0;
    case BSRC_C_NDATA:  return BM_I_NDATA + ((idx < BM_NBITS) ? idx : 0);
    case BSRC_C_TIE:    return BM_I_TIE + (idx & 1);
    case BSRC_C_BUS:    return BM_I_BUS + ((idx < BUS_COUNT) ? idx : 0);
    case BSRC_C_NBUS:   return BM_I_NBUS + ((idx < BUS_COUNT) ? idx : 0);
    }
    return 0;
}

// Class jumping used to live on SET. SET now opens the preset browser, so the
// only way along the source list is one step at a time - which auto-repeat
// makes fine: 22 sources with the extra signals off, 46 with them on, and the
// knob crosses either in a couple of seconds held down.

static void bm_step_source(int delta)
{
    int n = bm_n_src();
    int i = bm_index_of(bend_seg_cur()->route[bm_cursor]);

    if (i >= n) i = 0;      // sitting on a source this mode no longer offers
    i += delta;
    while (i < 0)   i += n;
    while (i >= n)  i -= n;
    bend_seg_cur()->route[bm_cursor] = bm_src_at(i);
    // The live matrix no longer matches the preset it came from. Do not keep
    // showing that preset as current; an arrow press establishes one again.
    bend_store_detach();
    bend_seg_detach();
}

//-------------------------------------------------------------------
// Entry and exit

// The status message belongs with the browser below, but the reroll gesture
// sits up here and now has something to say.
static void bm_note_set(const char *msg);

int gui_bend_active(void)
{
    return bm_active;
}

void gui_bend_randomise(void)
{
    if (!bm_active) return;

    // A reroll rewrites every pin, so an edit in progress has nothing left to
    // cancel back to - the source the arm remembered is gone with the rest of
    // the matrix. Disarm rather than leave MENU able to paste a stale source
    // over one pin of a freshly rolled bend.
    bm_armed = 0;

    // Inside the experimental window it rerolls the profile's knobs instead.
    // That is the whole way that window is meant to be used - pick the fault,
    // then hunt through what it can do - and it is the one place where a
    // reroll under an open panel is visible, because the panel is showing the
    // thing being rerolled.
#ifdef CAM_BEND_EXPERIMENTAL
    if (bm_browse && bm_tab == BM_TAB_X)
    {
        // The live slot only. A locked profile is locked - that is the whole
        // meaning of the second SET press - and a reroll that reached into the
        // committed entries would make locking worthless.
        bendx_t *live = conf.bendx_enable ? bendx_chain_live(&conf.bendx) : 0;

        if (live && !bendx_is_off(live))
        {
            bendx_random(live,
                         (unsigned)get_tick_count() ^ (live->seed * 1103515245u));
            bm_note_set("rerolled live profile");
        }
        else
            bm_note_set(live ? "nothing live to reroll" : "every slot locked");
        bm_dirty = 1;
        return;
    }
#endif

    // Not while the saved list is up. The panel covers the strip, so a reroll
    // there is a change you cannot see happening to a bend you are in the
    // middle of choosing a replacement for.
    if (bm_browse) return;

    // Reseeded from the clock so consecutive presses differ. The seed is kept
    // in the struct, so whatever lands is still reproducible and still
    // saveable once presets exist.
    bend_seg_cur()->seed = (unsigned)get_tick_count() ^ (bend_seg_cur()->seed * 1103515245u);
    bend_random(bend_seg_cur(), bend_seg_cur()->seed, bend_seg_cur()->depth,
                !conf.bitbend_simple);
    bend_store_detach();
    bend_seg_detach();
    bm_dirty = 1;
}

//-------------------------------------------------------------------
// The browser
//
// Opened with SET, over the live view. Two windows, changed with left and
// right; up and down walk the one that is showing; SET picks; MENU and the
// Back row both close.
//
//   SAVED         the bends on the card, plus a Save entry at the top. Picking
//                 one loads it and closes, so the common case - "show me what
//                 I saved and put that one on" - is SET, down, down, SET.
//
//   EXPERIMENTAL  the profiles in bendx.c. Picking one applies it and leaves
//                 the panel up, because this is a list you work through in
//                 turn against the same subject rather than one you take a
//                 single thing from. Picking it opens Amount, Reach, Data
//                 lanes, Mix and Row bands directly beneath it.
//
// In the saved window nothing is loaded until it is picked. Loading each entry
// as the cursor passed it was the first version and it is wrong here: the list
// covers the strip, so there is nothing to preview against, and Back would
// then have no way to mean "leave it as it was". The experimental window can
// afford the opposite rule because a profile is applied at capture and the
// panel is not in the way of anything you could have previewed.

#define BM_BR_SAVE      0                       // fixed rows, SAVED tab
#define BM_BR_IMAGE     1
#define BM_BR_OFF       2
#define BM_BR_ROWS      6                       // list rows visible at once

// A dotted rule between groups of rows. Four pixels, not a whole row: the
// panel is short and the separators are punctuation rather than content.
//
// They go where the list stops being a list - above Back in every window, and
// above the summary line in the two that have one - because those rows are not
// more of the thing above them and a cursor that walks off the end of a list
// into an unrelated row should be able to see it leaving.
#define BM_SEP_H        4

// Both windows have the same shape: some fixed rows at the top, a scrolling
// list, and Back. Saying it once here is what lets the cursor, the scrolling
// and the drawing be written once rather than twice.

static int bm_tab_fixed(void)
{
    // SAVED has three: Save, load from an image, and Off. The visual image
    // picker is a module because its JPEG thumbnail decoder does not belong in
    // the permanently resident capture core.
    // SEG has one: the mask. It is a fixed row rather than an entry in the
    // list because it is not a layout and not a region - it is a way of
    // looking at both, and it belongs at the top where it can be reached
    // without walking past seventeen layouts.
    if (bm_tab == BM_TAB_SAVED) return 3;
    if (bm_tab == BM_TAB_SEG)   return 1;
    return 0;
}

// The SEG window's list is two lists end to end: the layouts, then the regions
// the chosen layout has. One list rather than a list and a set of fixed rows
// because the second half changes length with the first, and because both
// halves are picked the same way - the cursor lands on a thing and SET makes
// it the one in use.
#define BM_SEG_LAYOUTS  BSEG_COUNT

// A region's bends, opened where the region is.
//
// Picking a region used to throw the panel over to the SAVED window, at the
// far end of the list, and picking there threw it back. Two windows and four
// presses for one thought - "this corner, that bend" - and in between them the
// row you were working on went off screen. So the list opens underneath the
// region instead, indented, the way a folder opens: the region stays visible
// with its bends under it, and picking one collapses it again.
//
// -1 when nothing is open. Only ever one at a time - two expanded regions in a
// five-row window would leave neither of them readable.
static int bm_seg_open = -1;
static int bm_seg_layout_open = -1;

// Off, then every preset on the card. Off is first because "take this region
// back to straight through" is the one entry that is always available and the
// one most often wanted after a bad roll.
static int bm_seg_inline_n(void)
{
    return 1 + bend_store_count();
}

#define BM_SEGROW_LAYOUT    0
#define BM_SEGROW_REGION    1
#define BM_SEGROW_INLINE    2
#define BM_SEGROW_SIZE      3

// What the i'th row of the SEG window is, and which one of its kind. The
// inline block is spliced in after its region's row, so everything below it
// shifts down while it is open - which is exactly what makes it read as
// belonging to that region rather than as a second list.
static int bm_seg_decode(int idx, int *val)
{
    int lay;
    for (lay = 0; lay < BM_SEG_LAYOUTS; lay++)
    {
        if (idx-- == 0) { *val = lay; return BM_SEGROW_LAYOUT; }
        if (lay != bm_seg_layout_open || lay == BSEG_OFF) continue;

        if (bend_seg_has_size(lay))
            if (idx-- == 0) { *val = lay; return BM_SEGROW_SIZE; }

        {
            int seg;
            for (seg = 0; seg < bend_seg_n(); seg++)
            {
                if (idx-- == 0) { *val = seg; return BM_SEGROW_REGION; }
                if (seg == bm_seg_open)
                {
                    int n = bm_seg_inline_n();
                    if (idx < n) { *val = idx; return BM_SEGROW_INLINE; }
                    idx -= n;
                }
            }
        }
    }
    *val = 0;
    return BM_SEGROW_LAYOUT;
}

// The row a region sits on with the block in whatever state it is in. Used to
// put the cursor back on the region after picking one of its bends.
static int bm_seg_region_row(int seg)
{
    int row = bm_tab_fixed() + bm_seg_layout_open + 1 + seg
            + bend_seg_has_size(bm_seg_layout_open);
    if (bm_seg_open >= 0 && seg > bm_seg_open) row += bm_seg_inline_n();
    return row;
}

#ifdef CAM_BEND_EXPERIMENTAL
#define BM_XROW_EFFECT  0
#define BM_XROW_KNOB    1

static int bm_x_decode(int idx, int *val)
{
    if (bm_x_open < 0 || idx <= bm_x_open)
    {
        *val = idx;
        return BM_XROW_EFFECT;
    }
    if (idx <= bm_x_open + BM_X_KNOBS)
    {
        *val = idx - bm_x_open - 1;
        return BM_XROW_KNOB;
    }
    *val = idx - BM_X_KNOBS;
    return BM_XROW_EFFECT;
}

static int bm_x_effect_row(int kind)
{
    return bm_tab_fixed() + kind + ((bm_x_open >= 0 && kind > bm_x_open) ? BM_X_KNOBS : 0);
}
#endif

static int bm_tab_items(void)
{
#ifdef CAM_BEND_EXPERIMENTAL
    if (bm_tab == BM_TAB_X) return BX_COUNT + ((bm_x_open >= 0) ? BM_X_KNOBS : 0);
#endif
    if (bm_tab == BM_TAB_SEG)
        return BM_SEG_LAYOUTS
             + ((bm_seg_layout_open > BSEG_OFF) ? bend_seg_n() : 0)
             + ((bm_seg_layout_open > BSEG_OFF && bend_seg_has_size(bm_seg_layout_open)) ? 1 : 0)
             + ((bm_seg_layout_open > BSEG_OFF && bm_seg_open >= 0) ? bm_seg_inline_n() : 0);
    return bend_store_count();
}

static int bm_browse_rows(void)
{
    return bm_tab_fixed() + bm_tab_items() + 1;     // + Back
}

// Row the cursor should sit on when a window is opened or switched to: on
// whatever that window currently has applied, so arriving somewhere always
// tells you where you already are.
static int bm_tab_home(void)
{
    if (bm_tab == BM_TAB_SAVED)
    {
        int cur = bend_store_cur();
        if (cur >= 0) return bm_tab_fixed() + cur;

        // Nothing loaded, so there is no row that is already right - and the
        // top row is Save, which writes a file rather than loading one. Landing
        // there and pressing SET is how a pick appears not to stick: it saves a
        // duplicate preset and leaves the matrix exactly as it was. Land on the
        // first preset instead whenever there is one to land on.
        return bend_store_count() > 0 ? bm_tab_fixed() : BM_BR_SAVE;
    }
    // The layout in use, not the region being edited. Arriving on the layout
    // is arriving on the decision the window is mostly about, and the regions
    // are one press below it.
    if (bm_tab == BM_TAB_SEG)
    {
        bm_seg_layout_open = (conf.bend_segs.layout == BSEG_OFF)
                           ? -1 : conf.bend_segs.layout;
        bm_seg_open = -1;
        return bm_tab_fixed() + conf.bend_segs.layout;
    }
#ifdef CAM_BEND_EXPERIMENTAL
    {
        // The slot the rocker is turning, so the window opens on the thing the
        // knob would move. With every slot locked there is nothing being
        // tuned, and Off is the honest place to be.
        const bendx_t *live = bendx_chain_live_const(&conf.bendx);
        bm_x_open = (conf.bendx_enable && live) ? live->kind : -1;
        return bm_tab_fixed() + ((conf.bendx_enable && live) ? live->kind : BX_OFF);
    }
#else
    return BM_BR_SAVE;
#endif
}

static void bm_note_set(const char *msg)
{
    int i;
    for (i = 0; i < (int)sizeof(bm_note) - 1 && msg[i]; i++) bm_note[i] = msg[i];
    bm_note[i] = 0;
    bm_note_at = get_tick_count();
}

static int bm_note_live(void)
{
    return bm_note_at && (int)(get_tick_count() - bm_note_at) < BM_SAVED_MS;
}

// Keep the selection inside the scrolled window. Shared by the cursor keys and
// by the tab switch, which lands on a row that may be a long way down.
static void bm_scroll_to_sel(void)
{
    int pi = bm_bsel - bm_tab_fixed();      // -1 on a fixed row, count on Back
    int n  = bm_tab_items();

    if (pi < bm_btop)               bm_btop = (pi < 0) ? 0 : pi;
    if (pi >= bm_btop + BM_BR_ROWS) bm_btop = pi - BM_BR_ROWS + 1;
    if (bm_btop > n - BM_BR_ROWS)   bm_btop = n - BM_BR_ROWS;
    if (bm_btop < 0)                bm_btop = 0;
}

static void bm_browse_open(void)
{
    // Re-read on every open rather than once per mode entry. Opening the
    // browser is exactly the moment the list is about to be looked at, and a
    // preset saved from the menu since last time should be in it.
    bend_store_rescan();
    bm_browse = 1;
    bm_detail = 0;
    bm_btop   = 0;

    // The window it was left on. Someone working through the experimental list
    // opens and closes this panel a lot, and reopening on SAVED every time
    // would put the tab switch in the middle of the only loop the mode has.
    bm_bsel = bm_tab_home();
    bm_scroll_to_sel();
    bm_dirty = 1;
}

static void bm_browse_close(void)
{
    bm_browse = 0;
    // The strip and the bar were covered by the panel, so the next redraw has
    // to be a full one - the change detection would otherwise see the same
    // matrix it drew before the browser opened and leave the panel on screen.
    bm_dirty = 1;
    // Closing the panel exposes pixels that it erased from the mask and grid.
    // A restore makes the next bend draw rebuild all three layers immediately,
    // instead of waiting for a later key to happen to dirty the screen.
    gui_set_need_restore();
}

static void bm_recipe_apply(const bend_store_recipe_t *r)
{
    conf.bitbend = r->bend;
    conf.bendx = r->bendx;
    conf.bend_segs = r->segs;
    conf.bitbend_enable = r->bend_on;
    conf.bendx_enable = r->bendx_on;
    bend_seg_prep(BM_NBITS);
}

static void bm_browse_save(void)
{
    int i;
    char msg[28];

    i = bend_store_save_current();

    if (i < 0)
    {
        bm_note_set("SAVE FAILED - card full?");
        return;
    }
    sprintf(msg, "saved BEND%02d.BND", bend_store_slot_at(i));
    bm_note_set(msg);

    // The region now holds a file, and the region list should say which. Saving
    // is the other way a region acquires a name, and leaving it unnamed after a
    // save would make the list disagree with the card.
    bend_seg_set_slot(bend_seg_active(), bend_store_slot_at(i));

    // The list just grew, and the new preset is the one that was saved, so put
    // the cursor on it. Leaving it on Save would mean a second press saves a
    // duplicate, which is the easiest mistake to make here.
    bm_bsel = bm_tab_fixed() + i;
    bm_scroll_to_sel();
}

#ifdef CAM_BEND_EXPERIMENTAL

// Pick the effect under the cursor into the live slot and open its controls
// immediately below it. A second press folds them away. Held SET commits the
// live effect, leaving ordinary SET free to mean open/close everywhere.
//
// Switching the live effect resets its knobs, because they are one pair of
// bytes meaning something different per effect and carrying "eleven bits of
// slip" over to become "address line eleven" is a setting that changed itself.
// Locking does not reset anything - the whole point is to keep what you tuned.
static void bm_x_pick(int kind)
{
    bendx_t *live = bendx_chain_live(&conf.bendx);
    char msg[28];

    if (kind == BX_OFF)
    {
        // Off is the whole chain, not just the live slot. A row that says Off
        // and leaves three locked profiles running would be lying, and there
        // is no other row that could mean "all of it".
        bendx_chain_reset(&conf.bendx);
        conf.bendx_enable = 0;
        bm_x_open = -1;
        bm_note_set("experimental off");
        return;
    }

    if (!live)
    {
        sprintf(msg, "chain full - %d of %d", BENDX_CHAIN_MAX, BENDX_CHAIN_MAX);
        bm_note_set(msg);
        return;
    }

    if (live->kind == kind)
    {
        bm_x_open = (bm_x_open == kind) ? -1 : kind;
        bm_note_set(bm_x_open < 0 ? "controls closed" : "controls open");
        return;
    }

    bendx_set_kind(live, kind);
    conf.bendx_enable = 1;
    bm_x_open = kind;
    sprintf(msg, "on: %s", bendx_name(kind));
    bm_note_set(msg);
}

static void bm_x_remove(int kind);
static void bm_x_lock(int kind)
{
    bendx_t *live = bendx_chain_live(&conf.bendx);
    char msg[28];

    if (!live || live->kind != kind) { bm_x_remove(kind); return; }
    if (bendx_chain_lock(&conf.bendx))
    {
        int n = conf.bendx.n;
        bm_x_open = -1;
        if (n >= BENDX_CHAIN_MAX) sprintf(msg, "locked - chain full");
        else sprintf(msg, "locked %d/%d - pick another", n, BENDX_CHAIN_MAX);
        bm_note_set(msg);
    }
}

// Take an entry out of the chain. Held SET, the same gesture that deletes a
// saved bend - but without the question, because nothing is destroyed: the
// profile is six bytes of settings you can pick again in two presses, where
// the preset is a file on the card that took twenty presses to dial in.
static void bm_x_remove(int kind)
{
    int i = bendx_chain_find(&conf.bendx, kind);
    char msg[28];

    if (i < 0) { bm_note_set("not in the chain"); return; }

    bendx_chain_remove(&conf.bendx, i);
    if (bendx_chain_count(&conf.bendx) == 0) conf.bendx_enable = 0;

    sprintf(msg, "removed %s", bendx_name(kind));
    bm_note_set(msg);
}

static const char * const bm_x_knob_names[BM_X_KNOBS] = {
    "Amount", "Reach", "Data lanes", "Mix", "Row bands"
};

// Turn whichever control the selector row names. This is deliberately the
// same five-value surface as the full menu, so the browser is not a reduced
// editor with a hidden second half.
static void bm_x_step(int delta)
{
    bendx_t *live = bendx_chain_live(&conf.bendx);
    int max, v;

    if (!conf.bendx_enable || !live || bendx_is_off(live))
    {
        bm_note_set(live ? "nothing live to tune" : "every slot locked");
        return;
    }

    switch (bm_x_knob)
    {
    case BM_X_AMOUNT:
        max = bendx_amount_max(live->kind);
        if (max <= 0) { bm_note_set("no amount on this one"); return; }
        v = (int)live->amount + delta;
        while (v < 0)    v += max + 1;
        while (v > max)  v -= max + 1;
        live->amount = (unsigned char)v;
        break;
    case BM_X_REACH:
        max = bendx_reach_max(live->kind);
        if (max <= 0) { bm_note_set("no reach on this one"); return; }
        v = (int)live->reach + delta;
        while (v < 0)    v += max + 1;
        while (v > max)  v -= max + 1;
        live->reach = (unsigned char)v;
        break;
    case BM_X_LANES:
        v = (int)live->lanes + delta;
        while (v < 0)   v += 256;
        while (v > 255) v -= 256;
        live->lanes = (unsigned char)v;
        break;
    case BM_X_MIX:
        v = (int)live->mix + delta;
        while (v < 0)            v += BXMIX_COUNT;
        while (v >= BXMIX_COUNT) v -= BXMIX_COUNT;
        live->mix = (unsigned char)v;
        break;
    default:
        v = (int)live->rowmod + delta;
        while (v < 0)  v += 65;
        while (v > 64) v -= 65;
        live->rowmod = (unsigned char)v;
        break;
    }
}

#endif // CAM_BEND_EXPERIMENTAL

//-------------------------------------------------------------------
// The segments window.
//
// Rows 0..BSEG_COUNT-1 are the layouts and the rest are the regions of the one
// in use. Picking a layout changes the shape of the frame; picking a region
// points the strip at it and closes the panel, because a region is chosen in
// order to go and edit it and the panel is over the thing being edited.

// Load one preset into one region, by index into the card's list. The inline
// block and the SAVED window both end up here.
static void bm_seg_step_size(int delta);
static void bm_seg_load_into(int seg, int idx)
{
    char msg[28];

    if (bend_store_load_at(idx, bend_seg_conf(seg)))
    {
        bend_sanitize(bend_seg_conf(seg), BM_NBITS);
        if (conf.bitbend_simple) bend_simplify(bend_seg_conf(seg));
        bend_seg_set_slot(seg, bend_store_slot_at(idx));
        sprintf(msg, "BEND%02d into region %d", bend_store_slot_at(idx), seg + 1);
        bm_note_set(msg);
    }
    else
        bm_note_set("unreadable preset");
}

static void bm_seg_pick(int idx)
{
    char msg[28];
    int val, kind = bm_seg_decode(idx, &val);

    if (kind == BM_SEGROW_LAYOUT)
    {
        int closing = (val == BSEG_OFF || bm_seg_layout_open == val);
        conf.bend_segs.layout = (unsigned char)val;
        // The region being edited may not exist any more, and the pin strip
        // reads it on the very next redraw.
        bend_seg_set_active(bend_seg_active());
        // A matrix in a slot no layout has reached before is whatever the
        // config block held. Gate it now rather than at the shutter, so what
        // the strip shows for it is what would be applied.
        bend_seg_prep(BM_NBITS);
        // A block belonging to a region of the old layout means nothing under
        // the new one, and may be hanging off a region that no longer exists.
        bm_seg_open = -1;
        bm_seg_layout_open = closing ? -1 : val;
        bm_bsel = bm_tab_fixed() + val;
        if (!closing)
        {
            bm_bsel++;
            bm_scroll_to_sel();
        }
        sprintf(msg, "%s", bend_seg_name(val));
        bm_note_set(msg);
        bm_dirty = 1;
        return;
    }

    if (kind == BM_SEGROW_SIZE)
    {
        bm_seg_step_size(1);
        bm_dirty = 1;
        return;
    }

    if (kind == BM_SEGROW_REGION)
    {
        // Second press on an open region closes it, so SET is one toggle and
        // there is no separate way to put the list away.
        if (bm_seg_open == val)
        {
            bm_seg_open = -1;
            bend_seg_set_active(val);
            bm_bsel = bm_seg_region_row(val);
            bm_scroll_to_sel();
            bm_dirty = 1;
            return;
        }

        bend_seg_set_active(val);

        // The saved list tracks one cursor for the whole camera, so the preset
        // it says is loaded belongs to whichever region was last loaded into.
        // Moving to another region makes that claim false, and an arrow press
        // or a pick establishes it again.
        bend_store_detach();

        // Open this region's bends under it, and put the cursor on the first
        // of them - the next press is going to be a pick, and it should not
        // also have to be a move.
        bm_seg_open = val;
        bm_bsel     = bm_seg_region_row(val) + 1;
        bm_scroll_to_sel();

        sprintf(msg, "region %d - pick a bend", val + 1);
        bm_note_set(msg);
        bm_dirty = 1;
        return;
    }

    // An inline row: Off, or one of the presets, into the region it hangs off.
    {
        int seg = bm_seg_open;

        if (seg < 0) return;

        if (val == 0)
        {
            bend_reset(bend_seg_conf(seg), BM_NBITS);
            bend_seg_set_slot(seg, -1);
            if (seg == bend_seg_active()) bend_store_detach();
            sprintf(msg, "%d straight through", seg + 1);
            bm_note_set(msg);
        }
        else
            bm_seg_load_into(seg, val - 1);

        // Collapse onto the region that was just filled. The list has done its
        // job, and the next thing anyone does here is move down one and fill
        // the next region - which is a row, not a window, away.
        bm_seg_open = -1;
        bm_bsel     = bm_seg_region_row(seg);
        bm_scroll_to_sel();
        bm_dirty    = 1;
    }
}

// Held SET on a region row. The same "get rid of the thing under the cursor"
// the other two windows have, and here the thing is that region's bend - so it
// goes back to straight through and the region stops doing anything, without
// touching the layout or the other regions.
// Returns 1 if the hold was taken here, 0 if the caller should ask the delete
// question about a preset instead - an inline row is a file on the card and
// gets the same protection it has in the SAVED window.
static int bm_seg_hold(int idx, int *preset)
{
    char msg[28];
    int val, kind = bm_seg_decode(idx, &val);

    if (kind == BM_SEGROW_LAYOUT)
    {
        bm_note_set("pick a region to clear");
        return 1;
    }

    if (kind == BM_SEGROW_SIZE)
    {
        bm_note_set("Left/Right adjusts size");
        return 1;
    }

    if (kind == BM_SEGROW_INLINE)
    {
        if (val == 0) { bm_note_set("nothing to delete"); return 1; }
        *preset = val - 1;
        return 0;
    }

    bend_reset(bend_seg_conf(val), BM_NBITS);
    bend_seg_set_slot(val, -1);
    if (val == bend_seg_active()) bend_store_detach();
    // A cleared region's open list is describing a region that no longer holds
    // any of it.
    if (bm_seg_open == val) bm_seg_open = -1;
    sprintf(msg, "%d cleared - straight", val + 1);
    bm_note_set(msg);
    return 1;
}

// The rocker is the size knob while this window is up, matching what it does
// in the experimental one. Only the round layouts have a size; the others say
// so rather than turning silently.
static void bm_seg_step_size(int delta)
{
    int v;

    if (!bend_seg_has_size(conf.bend_segs.layout))
    {
        bm_note_set("nothing to size here");
        return;
    }

    v = (int)conf.bend_segs.size + delta * 5;
    if (v < 5)   v = 5;
    if (v > 100) v = 100;
    conf.bend_segs.size = (unsigned char)v;
}

// The saved window's Off row. "No bend" is the identity matrix rather than the
// enable switch: entering the mode turns the engine on by design, so switching
// it back off here would be undone by the next entry and would leave the pin
// strip showing a bend that is not being applied. Straight-through says the
// same thing and the strip shows it - ten grey pins, every line on itself.
static void bm_saved_off(void)
{
    bend_reset(bend_seg_cur(), BM_NBITS);
    bend_store_detach();
    bend_seg_detach();
    bm_note_set("bend off");
}

static void bm_confirm_open(int idx)
{
    bm_confirm     = BM_CONFIRM_DELETE;
    bm_confirm_idx = idx;
    bm_confirm_yes = 0;         // No, the way every destructive prompt should
    bm_dirty       = 1;
}

static void bm_confirm_close(void)
{
    bm_confirm = BM_CONFIRM_NONE;
    bm_dirty   = 1;
}

static void bm_confirm_act(void)
{
    if (bm_confirm_yes)
    {
        int slot = bend_store_slot_at(bm_confirm_idx);
        char msg[28];

        if (slot >= 0 && bend_store_delete_at(bm_confirm_idx))
        {
            sprintf(msg, "deleted BEND%02d.BND", slot);
            bm_note_set(msg);
        }
        else
            bm_note_set("could not delete");

        // The list just got shorter under the cursor. Pull it back into range
        // rather than leaving it on a row that is now Back, or past the end.
        {
            int rows = bm_tab_fixed() + bend_store_count() + 1;
            if (bm_bsel >= rows) bm_bsel = rows - 1;
            bm_scroll_to_sel();
        }
    }
    bm_confirm_close();
}

static void bm_detail_name_cb(const char *s)
{
    if (s)
    {
        strncpy(bm_detail_info.name, s, BEND_STORE_NAME_MAX);
        bm_detail_info.name[BEND_STORE_NAME_MAX] = 0;
        if (!bend_store_set_info_at(bm_detail_idx, &bm_detail_info))
            bm_note_set("could not save name");
    }
    // SAVE is terminal: return to the inspector that opened the keyboard.
    // SET is still physically down at this point, so swallow it through its
    // release or the inspector will interpret the same press and reopen us.
    bm_swallow_entry = 1;
    gui_set_mode(&bendGuiHandler);
    gui_set_need_restore();
    bm_dirty = 1;
}

static void bm_detail_desc_cb(const char *s)
{
    if (s)
    {
        strncpy(bm_detail_info.description, s, BEND_STORE_DESC_MAX);
        bm_detail_info.description[BEND_STORE_DESC_MAX] = 0;
        if (!bend_store_set_info_at(bm_detail_idx, &bm_detail_info))
            bm_note_set("could not save description");
    }
    // Same release latch as the name editor above.
    bm_swallow_entry = 1;
    gui_set_mode(&bendGuiHandler);
    gui_set_need_restore();
    bm_dirty = 1;
}

static void bm_detail_open(int idx)
{
    if (!bend_store_peek_recipe_at(idx, &bm_detail_recipe))
    {
        bm_note_set("unreadable preset");
        return;
    }
    bend_store_get_info_at(idx, &bm_detail_info);
    bm_detail_idx = idx;
    bm_detail_action = 0;
    bm_detail = 1;
    bm_dirty = 1;
}

static void bm_detail_act(void)
{
    int i;
    switch (bm_detail_action)
    {
    case 0:
        strcpy(bm_detail_edit, bm_detail_info.name);
        libtextbox->textbox_init((int)"Bend name", (int)"Edit name",
                                 bm_detail_edit, BEND_STORE_NAME_MAX,
                                 bm_detail_name_cb, bm_detail_edit);
        break;
    case 1:
        strcpy(bm_detail_edit, bm_detail_info.description);
        libtextbox->textbox_init((int)"Bend description", (int)"Edit description",
                                 bm_detail_edit, BEND_STORE_DESC_MAX,
                                 bm_detail_desc_cb, bm_detail_edit);
        break;
    case 2:
        bm_detail = 0;
        bm_confirm_open(bm_detail_idx);
        break;
    case 3:
        i = bend_store_save_recipe(&bm_detail_recipe, &bm_detail_info);
        if (i >= 0)
        {
            bm_detail_idx = i;
            bm_bsel = bm_tab_fixed() + i;
            bm_scroll_to_sel();
            bm_note_set("saved bend as new preset");
        }
        else bm_note_set("SAVE FAILED - card full?");
        break;
    default:
        bm_detail = 0;
        gui_set_need_restore();
        break;
    }
    bm_dirty = 1;
}

// Held SET. On a saved preset it asks before taking a file off the card; on an
// experimental row it drops that effect out of the chain. Both are "get rid of
// the thing under the cursor", which is why they share a gesture.
static void bm_browse_hold(void)
{
    int fixed = bm_tab_fixed();
    int n     = bm_tab_items();
    int idx   = bm_bsel - fixed;

    if (bm_bsel < fixed || idx >= n) return;    // a fixed row, or Back

#ifdef CAM_BEND_EXPERIMENTAL
    if (bm_tab == BM_TAB_X)
    {
        int val, kind = bm_x_decode(idx, &val);
        if (kind == BM_XROW_KNOB) { bm_note_set("hold the effect to lock"); return; }
        bm_x_lock(val);
        bm_dirty = 1;
        return;
    }
#endif
    if (bm_tab == BM_TAB_SEG)
    {
        int preset = -1;
        if (bm_seg_hold(idx, &preset)) { bm_dirty = 1; return; }
        bm_confirm_open(preset);
        return;
    }

    bm_detail_open(idx);
}

static void bm_browse_pick(void)
{
    int fixed = bm_tab_fixed();
    int n     = bm_tab_items();
    int idx   = bm_bsel - fixed;

    if (bm_tab == BM_TAB_SAVED)
    {
        if (bm_bsel == BM_BR_SAVE) { bm_browse_save(); return; }
        if (bm_bsel == BM_BR_IMAGE)
        {
            bm_erase_all();
            module_run("bendpic.flt");
            return;
        }
        if (bm_bsel == BM_BR_OFF)  { bm_saved_off();   return; }
    }
    if (bm_tab == BM_TAB_SEG && bm_bsel < fixed)
    {
        conf.bend_seg_mask = !conf.bend_seg_mask;
        bm_note_set(conf.bend_seg_mask ? "mask on" : "mask off");
        bm_dirty = 1;
        return;
    }
    if (idx >= n) { bm_browse_close(); return; }                // Back

    if (bm_tab == BM_TAB_SEG) { bm_seg_pick(idx); return; }

#ifdef CAM_BEND_EXPERIMENTAL
    if (bm_tab == BM_TAB_X)
    {
        int val, kind = bm_x_decode(idx, &val);
        if (kind == BM_XROW_KNOB)
        {
            bm_x_knob = val;
            bm_x_step(1);
        }
        else
        {
            bm_x_pick(val);
            if (bm_x_open == val)
            {
                bm_bsel = bm_x_effect_row(val) + 1;
                bm_scroll_to_sel();
            }
        }
        // Not closed. The experimental list is a list of things to try in turn
        // against the same subject, and the saved list is a list of things to
        // pick one of - so this one stays up and that one gets out of the way.
        // With profiles stacking that matters more, not less: the second half
        // of the gesture is another press on this same list.
        bm_dirty = 1;
        return;
    }
#endif

    {
    bend_store_recipe_t r;
    if (bend_store_load_recipe_at(idx, &r))
    {
        char msg[28];
        bm_recipe_apply(&r);
        bend_seg_set_slot(0, bend_store_slot_at(idx));
        sprintf(msg, "loaded BEND%02d.BND", bend_store_slot_at(idx));
        bm_note_set(msg);
    }
    else
        bm_note_set("unreadable preset");

    // With a layout on, go back to the region list instead of getting out of
    // the way. Filling four corners is four rounds of "pick the region, pick
    // its bend", and closing the panel after each one would put two presses of
    // navigation between every round. Unsegmented there is only ever one round
    // and the old behaviour - pick it and the list is gone - is right.
    // Picking a bend is also the way out, so the list does not have to be
    // dismissed separately once it has done its job.
    bm_browse_close();
    }
}

// Bodies with no zoom pair in their keymap have nowhere to put the browser's
// tuning knob - the segment size and the selected experimental control live on the
// rocker everywhere else. It moves onto LEFT and RIGHT, but only while the
// cursor is on a row that actually owns a number: the layout in use when it is
// a round or repeating one, and the live profile when its effect has an
// selected control. Every other row keeps LEFT and RIGHT as the window switch.
//
// One row rather than the whole window, because a window where the side keys
// sometimes change window and sometimes do not would be unreadable. One row
// is a rule you can see: the row shows its value in arrows when it is
// selected, and it is the only row that does.
//
// Whether a row in the current window owns a number the side keys can move.
// A pure question - the drawing code asks it of every row it paints, and only
// the key handler below turns anything.
static int bm_row_is_tunable(int idx)
{
    if (idx < 0 || idx >= bm_tab_items()) return 0;

    if (bm_tab == BM_TAB_SEG)
    {
        int val, kind = bm_seg_decode(idx, &val);
        return kind == BM_SEGROW_SIZE && val == conf.bend_segs.layout;
    }

#ifdef CAM_BEND_EXPERIMENTAL
    if (bm_tab == BM_TAB_X)
    {
        const bendx_t *live = bendx_chain_live_const(&conf.bendx);
        int val, kind = bm_x_decode(idx, &val);
        return conf.bendx_enable && live && !bendx_is_off(live)
            && kind == BM_XROW_KNOB && live->kind == bm_x_open;
    }
#endif
    return 0;
}

// Returns 1 if it took the press.
static int bm_tune_row(int delta)
{
    int idx = bm_bsel - bm_tab_fixed();

    if (bm_bsel < bm_tab_fixed() || !bm_row_is_tunable(idx)) return 0;

    if (bm_tab == BM_TAB_SEG) bm_seg_step_size(delta);
#ifdef CAM_BEND_EXPERIMENTAL
    else
    {
        int val;
        bm_x_decode(idx, &val);
        bm_x_knob = val;
        bm_x_step(delta);
    }
#endif
    return 1;
}

static void bm_browse_move(int delta)
{
    int n = bm_browse_rows();

    bm_bsel += delta;
    while (bm_bsel < 0)  bm_bsel += n;
    while (bm_bsel >= n) bm_bsel -= n;

    bm_scroll_to_sel();
    bm_nav_only = 1;
    bm_dirty = 1;
}

static void bm_tab_move(int delta)
{
    // Leaving the window closes anything it had open. Coming back to a block
    // still hanging off a region, with the cursor now somewhere else entirely,
    // is a list you have to work out rather than read.
    bm_seg_open = -1;
    bm_seg_layout_open = -1;
#ifdef CAM_BEND_EXPERIMENTAL
    bm_x_open = -1;
#endif

    bm_tab += delta;
    while (bm_tab < 0)             bm_tab += BM_TAB_COUNT;
    while (bm_tab >= BM_TAB_COUNT) bm_tab -= BM_TAB_COUNT;

    // The row index means something different in each window, so it cannot
    // carry across. Land on what that window has applied - see bm_tab_home().
    bm_btop = 0;
    bm_bsel = bm_tab_home();
    bm_scroll_to_sel();
    bm_dirty = 1;
}

extern gui_handler bendGuiHandler;

// Requested from the KBD task, acted on in the GUI task - the same split ALT
// makes with gui_set_alt_mode_state / gui_activate_alt_mode. Switching GUI mode
// erases and repaints the screen, and doing that from the key handler is a
// cross-task write to the bitmap buffer that CHDK goes out of its way to avoid.
#define BM_REQ_NONE     0
#define BM_REQ_ENTER    1
#define BM_REQ_LEAVE    2
#define BM_REQ_SHOT     3       // playback: ask about a picture's bend

static int bm_request;

void gui_bend_enter(void)   { if (!bm_active) bm_request = BM_REQ_ENTER; }
void gui_bend_exit(void)    { if (bm_active)  bm_request = BM_REQ_LEAVE; }

// Returns 0 if the request was refused, so the caller can let the button press
// mean what it normally means instead of swallowing it.
int gui_bend_toggle(void)
{
    if (bm_active) { gui_bend_exit(); return 1; }

    // Record mode only for the patchbay - there is nothing to bend in
    // playback, and the strip would just be sitting on top of a picture that
    // has already been taken.
    //
    // But the gesture is not wasted there. In playback the same hold asks
    // about the picture rather than about the sensor: every bent frame was
    // saved with a record of what made it, and this is where you get it back.
    // Same button, same hold, and in both modes it means "the bend, please".
    if (!camera_info.state.mode_rec || camera_info.state.mode_play)
    {
        bm_request = BM_REQ_SHOT;
        return 1;
    }

#ifdef CAM_BEND_ENTER_UP
    // On this body the way in is the UP arrow on the shooting screen, handled
    // in core/gui_recui.c. The hold keeps its other two jobs - leaving the mode
    // above, and the playback shot prompt - but no longer opens the patchbay,
    // so refuse here and let the button mean what it otherwise means.
    return 0;
#else
    gui_bend_enter();
    return 1;
#endif
}

// Called from the GUI task, once per redraw, before the mode handler runs.
void gui_bend_activate(void)
{
    // Playback can be entered with the patchbay still active (the PLAY
    // button is handled by Canon, not by this mode). Do not leave a record-
    // screen UI stranded over the gallery: turn it into the ordinary leave
    // request before handling the mode switch below.
    if (bm_active && (!camera_info.state.mode_rec || camera_info.state.mode_play))
        bm_request = BM_REQ_LEAVE;

    switch (bm_request)
    {
    case BM_REQ_ENTER:
        // Every region, not just the one being edited - the strip shows one
        // matrix but the shutter applies all of them, and the gate has to
        // cover what will be applied rather than what is on screen.
        bend_seg_prep(BM_NBITS);
#ifdef CAM_BEND_EXPERIMENTAL
        // Same gate for the chain, and for the same reason: it can arrive from
        // a zeroed config block or from a build with a shorter effect list,
        // and every kind in it indexes a jump table.
        bendx_chain_sanitize(&conf.bendx);
#endif

        // One directory read per entry into the mode. The count is cached from
        // here on, so the redraw path never touches the card - and a preset
        // saved from the menu, or a card swapped since last time, still shows
        // up without needing a way to ask for a rescan.
        bend_store_rescan();

        // Opening the patchbay means you want the bend applied. Making someone
        // hold a button to get here and then find a separate Enable switch in
        // the menu would be a bad joke.
        conf.bitbend_enable = 1;

        // The key that asked for the mode is very likely still down, and on a
        // body where that key is UP it is also a key the strip binds. Without
        // this the press that opened the patchbay would immediately step the
        // source under the cursor - one gesture doing two things. Swallowed
        // until everything is physically released; see bm_kbd_process().
        bm_swallow_entry = 1;

        bm_active = 1;
        bm_cursor = BM_NBITS - 1;   // the top bit, where a change shows most
        bm_armed  = 0;
        bm_browse = 0;
        bm_seg_open = -1;
        bm_seg_layout_open = -1;
#ifdef CAM_BEND_EXPERIMENTAL
        bm_x_open = -1;
#endif
        bm_note_at = 0;
        bm_br_drawn_h = 0;      // the screen was just repainted by the mode switch
        bm_dirty  = 1;
        gui_set_mode(&bendGuiHandler);
        break;

    case BM_REQ_LEAVE:
        // Take the whole UI off the screen before handing the mode back - see
        // bm_erase_all(), below the panel geometry it needs.
        bm_erase_all();

        bm_active = 0;
        conf_save();                // keep the patch across a battery pull
        gui_set_need_restore();
        gui_set_mode(&defaultGuiHandler);
        break;

    case BM_REQ_SHOT:
        // Requested from the KBD task in playback, acted on here for the same
        // reason entering the mode is: this puts a message box on screen, and
        // drawing from the key handler is the cross-task write to the bitmap
        // buffer CHDK goes out of its way to avoid.
        gui_bend_shot_prompt();
        break;
    }
    bm_request = BM_REQ_NONE;
}

//-------------------------------------------------------------------
// Drawing

// Panel geometry. Centred horizontally, under the pin strip, sized to the rows
// it actually has so a directory with two presets does not get a panel sized
// for eight.
//
// 28 characters rather than the 24 the single-window version used: an
// experimental row carries a name, a slow marker and an amount, and 24 was
// exactly two characters short of the longest of them.
#define BM_BR_W     (28 * FONT_WIDTH)
#define BM_BR_X     ((CAM_SCREEN_WIDTH - BM_BR_W) / 2)
#define BM_BR_Y     (BM_H + 6)
#define BM_BR_TX    (BM_BR_X + 6)                       // left edge of row text
#define BM_BR_RX    (BM_BR_X + BM_BR_W - 6)             // right edge of it

// Dotted rules this window draws - see BM_SEP_H.
static int bm_browse_seps(void)
{
    if (bm_confirm) return 0;
    return (bm_tab == BM_TAB_SAVED) ? 1 : 2;
}

// Everything this mode has drawn, gone.
//
// gui_set_mode() repaints what the next mode draws, which is not the same thing
// as erasing what this one drew: the band at the top, the status line at the
// foot and the browser panel are ours, they are in the OSD plane, and nothing
// else in record mode writes those rows. So whatever was still up - most
// visibly the status bar along the bottom - stayed there over the live view
// until something happened to repaint it, which on this screen could be a long
// time.
//
// Erased to transparent rather than to black, because black is a colour this
// plane shows and the live view has to come back through.
static void bm_erase_all(void)
{
    draw_rectangle(0, 0, CAM_SCREEN_WIDTH - 1, BM_H,
                   MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                   RECT_BORDER0 | DRAW_FILLED);
    draw_rectangle(0, BM_SY - 2, CAM_SCREEN_WIDTH - 1, BM_SY + BM_SH - 3,
                   MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                   RECT_BORDER0 | DRAW_FILLED);
    if (bm_br_drawn_h)
    {
        draw_rectangle(BM_BR_X, BM_BR_Y, BM_BR_X + BM_BR_W,
                       BM_BR_Y + bm_br_drawn_h,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0 | DRAW_FILLED);
        bm_br_drawn_h = 0;
    }
    bm_mask_erase();
    bm_browse = 0;
    bm_armed  = 0;
}

// Height the panel will occupy, so the erase on close matches the draw exactly.
static int bm_browse_h(void)
{
    int n, shown, rows;

    // The question replaces the panel rather than sitting on top of it, so it
    // has its own height: one line to ask and two to answer.
    if (bm_confirm) return 4 * FONT_HEIGHT + 8;

    n     = bm_tab_items();
    shown = (n < BM_BR_ROWS) ? n : BM_BR_ROWS;
    // Tab header + the fixed rows this window has + the list + Back, and an
    // empty list still needs its one line to say so - without that the panel
    // is drawn a row shorter than it is filled and Back lands on the border.
    rows  = 1 + bm_tab_fixed() + ((n == 0) ? 1 : shown) + 1;
    // The experimental window carries a chain summary under its list. It is
    // the only place the locked entries can be counted without reading every
    // row, and with four of them the row markers alone do not say the order.
#ifdef CAM_BEND_EXPERIMENTAL
    if (bm_tab == BM_TAB_X) rows++;
#endif
    // The segments window carries its own summary line for the same reason -
    // the cost of the layout is not on any of the rows.
    if (bm_tab == BM_TAB_SEG) rows++;
    // One rule above Back everywhere, and a second above the summary line in
    // the two windows that have one. Counted here so the erase on close and
    // the box border match what is actually drawn.
    return rows * FONT_HEIGHT + bm_browse_seps() * BM_SEP_H + 8;
}

// One row-drawing shape for every kind of row, so the cursor looks the same on
// Save and Back as it does on a preset or an effect. `right`, when given, is
// drawn against the right edge in the same colours - which is how an
// experimental row carries its amount without the name having to be padded to
// a fixed width by hand.
static void bm_row(int y, int sel, const char *text, const char *right, color fgc)
{
    color bg = sel ? theme_color(TC_HILITE) : COLOR_BLACK;
    color fg = sel ? theme_color(TC_HILITE_FG) : fgc;

    draw_rectangle(BM_BR_X + 2, y, BM_BR_X + BM_BR_W - 2, y + FONT_HEIGHT - 1,
                   MAKE_COLOR(bg, bg), RECT_BORDER0 | DRAW_FILLED);
    draw_string(BM_BR_TX, y, text, MAKE_COLOR(bg, fg));
    if (right && right[0])
        draw_string_justified(0, y, right, MAKE_COLOR(bg, fg),
                              0, BM_BR_RX, TEXT_RIGHT);
}

static void bm_sep(int y)
{
    int x;
    // Dots rather than a line, and inset from the border, so it reads as a
    // break in the list rather than as another edge of the panel.
    for (x = BM_BR_X + 8; x < BM_BR_X + BM_BR_W - 8; x += 4)
        draw_line(x, y + BM_SEP_H / 2, x + 1, y + BM_SEP_H / 2,
                  theme_color(TC_DIM));
}

// The two window names, with the arrows that say the two side keys move
// between them. Drawn as a row of the panel rather than as a border label so
// that it scrolls with nothing and is always the first thing read.
static void bm_tabs_draw(int y)
{
    // Columns are counted from two characters in, past the left arrow. The
    // panel is 28 wide and the text area 27, so the far end of SEG lands at
    // column 23 and the right arrow still has its own room - a tab whose
    // highlight runs under the border is how it reads as clipped. That is also
    // why the third window is called SEG and not SEGMENTS: the full word is
    // five characters more than the row has left.
#ifdef CAM_BEND_EXPERIMENTAL
    static const char * const names[BM_TAB_COUNT] = { "SAVED", "EXPERIMENTAL", "SEG" };
    static const unsigned char cols[BM_TAB_COUNT] = { 0, 7, 20 };
    static const unsigned char lens[BM_TAB_COUNT] = { 5, 12, 3 };
#else
    static const char * const names[BM_TAB_COUNT] = { "SAVED", "SEG" };
    static const unsigned char cols[BM_TAB_COUNT] = { 0, 7 };
    static const unsigned char lens[BM_TAB_COUNT] = { 5, 3 };
#endif
    int t;

    draw_rectangle(BM_BR_X + 2, y, BM_BR_X + BM_BR_W - 2, y + FONT_HEIGHT - 1,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0 | DRAW_FILLED);

    draw_string(BM_BR_TX, y, "<", MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)));
    draw_string_justified(0, y, ">", MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)),
                          0, BM_BR_RX, TEXT_RIGHT);

    for (t = 0; t < BM_TAB_COUNT; t++)
    {
        int  sel = (t == bm_tab);
        int  x   = BM_BR_TX + (2 + cols[t]) * FONT_WIDTH;
        color bg = sel ? theme_color(TC_TEXT) : COLOR_BLACK;
        color fg = sel ? COLOR_BLACK : theme_color(TC_DIM);

        draw_rectangle(x - 2, y, x + lens[t] * FONT_WIDTH + 1, y + FONT_HEIGHT - 1,
                       MAKE_COLOR(bg, bg), RECT_BORDER0 | DRAW_FILLED);
        draw_string(x, y, names[t], MAKE_COLOR(bg, fg));
    }
}

#ifdef CAM_BEND_EXPERIMENTAL

// One row of the experimental window.
//
// The left marker is the whole state of the chain, read one row at a time:
//
//   ' '   not in the chain
//   '>'   live - the slot the rocker turns, applied last
//   '1'..'4'  locked, and its position in the order things run
//
// so a chain of "row address line, then a decaying refresh, still tuning a bit
// slip" reads as 1, 2, > down the list. The amount is each entry's own, and
// the slow ones are marked, because that is the difference between a shot that
// saves and a shot you wait out and it should not be discovered by waiting.
static void bm_x_row(int y, int idx, int sel)
{
    char left[20], right[16];
    int val, rowkind = bm_x_decode(idx, &val);
    int kind = (rowkind == BM_XROW_KNOB) ? bm_x_open : val;
    const char *name = bendx_name(kind);
    const bendx_t *live = bendx_chain_live_const(&conf.bendx);
    int on = conf.bendx_enable;
    int pos = on ? bendx_chain_find(&conf.bendx, kind) : -1;
    int is_live = on && live && !bendx_is_off(live) && live->kind == kind;
    int locked = (pos >= 0) && !is_live;
    int i = 0, j;
    color fg;

    if (rowkind == BM_XROW_KNOB)
    {
        const bendx_t *p = live;
        int k;
        sprintf(left, "    %s", bm_x_knob_names[val]);
        right[0] = 0;
        if (!p || p->kind != bm_x_open) strcpy(right, "-");
        else switch (val)
        {
        case BM_X_AMOUNT:
            if (bendx_amount_max(kind) > 0) bendx_label(p, right);
            else strcpy(right, "-");
            break;
        case BM_X_REACH:
            if (bendx_reach_max(kind) > 0) sprintf(right, "%d", p->reach);
            else strcpy(right, "-");
            break;
        case BM_X_LANES:
            if (!p->lanes) strcpy(right, "all");
            else
            {
                for (k = 0; k < 8; k++) right[k] = (p->lanes & (0x80 >> k)) ? '1' : '0';
                right[8] = 0;
            }
            break;
        case BM_X_MIX: sprintf(right, "%s", bendx_mix_names[p->mix]); break;
        default:
            if (p->rowmod < 2) strcpy(right, "all");
            else sprintf(right, "%d", p->rowmod);
            break;
        }
#ifdef CAM_BEND_NO_ROCKER
        j = strlen(right);
        if (sel && j && j + 3 < (int)sizeof(right) && bm_row_is_tunable(idx))
        {
            for (k = j; k > 0; k--) right[k] = right[k - 1];
            right[0] = '<'; right[j + 1] = '>'; right[j + 2] = 0;
        }
#endif
        bm_row(y, sel, left, right, theme_color(TC_ACCENT));
        return;
    }

    if (kind == BX_OFF)
    {
        // Off is marked when the chain is genuinely empty, and only then.
        pos     = -1;
        is_live = 0;
        locked  = 0;
        if (!on || bendx_chain_count(&conf.bendx) == 0) is_live = 1;
    }

    if      (is_live) left[i++] = (bm_x_open == kind) ? 'v' : '>';
    else if (locked)  left[i++] = (char)('1' + pos);
    else              left[i++] = ' ';
    left[i++] = ' ';
    while (*name && i < (int)sizeof(left) - 1) left[i++] = *name++;
    left[i] = 0;

    right[0] = 0;
    j = 0;
    if (kind != BX_OFF)
    {
        // Each entry carries its own knobs, so every row that is in the chain
        // shows its own amount - not just the live one. That is the difference
        // between a chain you can read and a chain you have to remember.
        if (is_live) bendx_label(live, right);
        else if (locked) bendx_label(&conf.bendx.item[pos], right);
        j = (int)strlen(right);
#ifdef CAM_BEND_NO_ROCKER
        // As in the segments window: with no rocker the side keys turn the
        // live row, so the live row shows its amount in arrows and no other
        // row does.
        if (sel && j && j + 3 < (int)sizeof(right) && bm_row_is_tunable(idx))
        {
            int k;
            for (k = j; k > 0; k--) right[k] = right[k - 1];
            right[0]     = '<';
            right[j + 1] = '>';
            right[j + 2] = 0;
            j += 2;
        }
#endif
        if (bendx_is_slow(kind) && j + 2 < (int)sizeof(right))
        {
            // A trailing mark rather than the word: at eight pixels a
            // character the word costs a third of the row.
            if (j) right[j++] = ' ';
            right[j++] = '*';
            right[j] = 0;
        }
    }

    // Locked entries are the committed ones and the live entry is the one
    // still moving, so they are not the same colour - the eye should be able
    // to find what the rocker will change without reading the markers.
    if (is_live)     fg = theme_color(TC_ACCENT);
    else if (locked) fg = theme_color(TC_ACCENT2);
    else             fg = theme_color(TC_TEXT);

    bm_row(y, sel, left, right, fg);
}

// The chain, under its list. Four markers scattered down eighteen rows do not
// say how many there are or what the whole thing costs, and both of those are
// what you want to know before pressing the shutter.
static void bm_x_summary(int y)
{
    char s[32];
    int n = conf.bendx_enable ? bendx_chain_count(&conf.bendx) : 0;
    color fg = theme_color(TC_DIM);

    if (n == 0)
        sprintf(s, "chain empty");
    else
    {
        sprintf(s, "chain %d/%d%s", n, BENDX_CHAIN_MAX,
                bendx_chain_slow(&conf.bendx) ? "  * slow shot" : "");
        fg = bendx_chain_slow(&conf.bendx) ? theme_color(TC_WARN) : theme_color(TC_ACCENT);
    }
    draw_rectangle(BM_BR_X + 2, y, BM_BR_X + BM_BR_W - 2, y + FONT_HEIGHT - 1,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0 | DRAW_FILLED);
    draw_string(BM_BR_TX, y, s, MAKE_COLOR(COLOR_BLACK, fg));
}

#endif // CAM_BEND_EXPERIMENTAL

// One row of the segments window - a layout in the top half of the list, a
// region of the layout in use in the bottom half.
//
// The markers are the same alphabet the other two windows use: '>' is what is
// in use, and it means the layout on a layout row and the region the strip is
// editing on a region row. A region says whether it has a bend in it, because
// four rows that all look alike is exactly the state this window exists to
// stop you being in.
static void bm_seg_row(int y, int idx, int sel)
{
    char left[28], right[10];
    color fg;
    int val, kind = bm_seg_decode(idx, &val);

    if (kind == BM_SEGROW_LAYOUT)
    {
        int on = (conf.bend_segs.layout == val);
        sprintf(left, "%c %s", on ? '>' : ' ', bend_seg_name(val));
        right[0] = 0;
        if (bm_seg_layout_open == val) left[0] = 'v';
        fg = on ? theme_color(TC_ACCENT) : theme_color(TC_TEXT);
    }
    else if (kind == BM_SEGROW_SIZE)
    {
        strcpy(left, "    Size");
#ifdef CAM_BEND_NO_ROCKER
        if (sel) sprintf(right, "<%d%%>", conf.bend_segs.size);
        else
#endif
        sprintf(right, "%d%%", conf.bend_segs.size);
        fg = theme_color(TC_ACCENT);
    }
    else if (kind == BM_SEGROW_INLINE)
    {
        // Indented under the region it belongs to, which is the whole of what
        // says it belongs to it. Off carries the same '>' marker a loaded
        // preset does, because with nothing loaded Off is what is in effect.
        int seg  = bm_seg_open;
        int bent = (seg >= 0) && !bend_is_identity(bend_seg_conf(seg));
        int slot = (seg >= 0) ? bend_seg_slot(seg) : -1;

        if (val == 0)
        {
            sprintf(left, "        %c Off", bent ? ' ' : '>');
            fg = bent ? theme_color(TC_DIM) : theme_color(TC_ACCENT);
        }
        else
        {
            int s2 = bend_store_slot_at(val - 1);
            sprintf(left, "        %c BEND%02d.BND",
                    (bent && slot == s2) ? '>' : ' ', s2);
            fg = (bent && slot == s2) ? theme_color(TC_ACCENT) : theme_color(TC_TEXT);
        }
        right[0] = 0;
    }
    else
    {
        int seg  = val;
        int on   = (seg == bend_seg_active());
        int bent = !bend_is_identity(bend_seg_conf(seg));
        int slot = bend_seg_slot(seg);

        // The marker doubles as the open/closed state, the way a file browser
        // marks an expanded folder: 'v' while this region's bends are showing
        // under it, '>' when it is simply the region in use.
        sprintf(left, "    %c %d %s",
                (bm_seg_open == seg) ? 'v' : (on ? '>' : ' '),
                seg + 1, bend_seg_label(conf.bend_segs.layout, seg));

        // The file if the region was loaded from one, "bent" if it holds a
        // matrix that came from the strip or the randomiser, "-" if it is
        // straight through. Three states rather than two because "there is
        // something here and it is not on the card" is the state a hand-tuned
        // corner is in, and it should not read the same as an empty one.
        if (slot >= 0 && bent) sprintf(right, "BEND%02d", slot);
        else                   sprintf(right, "%s", bent ? "bent" : "-");

        fg = on ? theme_color(TC_ACCENT) : (bent ? theme_color(TC_ACCENT2) : theme_color(TC_DIM));
    }

    bm_row(y, sel, left, right, fg);
}

// Under the list, the way the chain summary sits under the experimental one.
// It carries the one thing the rows cannot say: what the layout costs. A
// horizontal split leaves every row whole and runs at full speed; anything
// that cuts a row across drops those rows onto the slow per-pixel path, which
// is the same warning the experimental window's asterisk gives.
static void bm_seg_summary(int y)
{
    char s[32];
    int lay = conf.bend_segs.layout;
    int cuts = bend_seg_cuts_rows(lay);
    color fg = theme_color(TC_DIM);

    if (lay == BSEG_OFF)
        sprintf(s, "segments off");
    else
    {
        sprintf(s, "%d regions%s", bend_seg_n(), cuts ? "  * slow shot" : "");
        fg = cuts ? theme_color(TC_WARN) : theme_color(TC_ACCENT);
    }

    draw_rectangle(BM_BR_X + 2, y, BM_BR_X + BM_BR_W - 2, y + FONT_HEIGHT - 1,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0 | DRAW_FILLED);
    draw_string(BM_BR_TX, y, s, MAKE_COLOR(COLOR_BLACK, fg));
}

// The delete question, in place of the panel.
static void bm_confirm_draw(void)
{
    int h = bm_browse_h();
    int y = BM_BR_Y + 4;
    int slot = bend_store_slot_at(bm_confirm_idx);
    char q[32];

    draw_rectangle(BM_BR_X, BM_BR_Y, BM_BR_X + BM_BR_W, BM_BR_Y + h,
                   MAKE_COLOR(COLOR_BLACK, theme_color(TC_BAD)), RECT_BORDER1 | DRAW_FILLED);

    sprintf(q, "Delete BEND%02d.BND", (slot >= 0) ? slot : 0);
    draw_string(BM_BR_TX, y, q, MAKE_COLOR(COLOR_BLACK, theme_color(TC_TEXT)));
    y += FONT_HEIGHT;
    draw_string(BM_BR_TX, y, "from the card?",
                MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)));
    y += FONT_HEIGHT;

    bm_row(y, bm_confirm_yes == 0, "  No", 0, theme_color(TC_TEXT));
    y += FONT_HEIGHT;
    bm_row(y, bm_confirm_yes == 1, "  Yes, delete it", 0, theme_color(TC_BAD));
}

static const char * const bm_detail_actions[BM_DETAIL_ACTIONS] = {
    "Edit name", "Edit description", "Delete bend", "Save bend as", "Back"
};

static void bm_detail_menu_draw(void)
{
    int i, y = CAM_SCREEN_HEIGHT - BM_DETAIL_ACTIONS * FONT_HEIGHT - 3;
    color text = theme_color(TC_TEXT);

    draw_rectangle(CAM_SCREEN_WIDTH/2, y - 2, CAM_SCREEN_WIDTH - 1,
                   CAM_SCREEN_HEIGHT - 1,
                   MAKE_COLOR(COLOR_BLACK, text), RECT_BORDER1 | DRAW_FILLED);
    for (i = 0; i < BM_DETAIL_ACTIONS; i++, y += FONT_HEIGHT)
    {
        color bg = (i == bm_detail_action) ? theme_color(TC_HILITE) : COLOR_BLACK;
        color fg = (i == bm_detail_action) ? theme_color(TC_HILITE_FG)
                                           : (i == 2 ? theme_color(TC_BAD) : text);
        draw_rectangle(CAM_SCREEN_WIDTH/2 + 3, y, CAM_SCREEN_WIDTH - 3,
                       y + FONT_HEIGHT - 1,
                       MAKE_COLOR(bg, bg), RECT_BORDER0 | DRAW_FILLED);
        draw_string(CAM_SCREEN_WIDTH/2 + 8, y, bm_detail_actions[i], MAKE_COLOR(bg, fg));
    }
}

static void bm_detail_draw(void)
{
    char s[96];
    int pin, col, len, y = 0, i;
    int labelw = 28;
    int colw = (CAM_SCREEN_WIDTH - labelw) / BM_NBITS;
    color text = theme_color(TC_TEXT);

    draw_rectangle(0, 0, CAM_SCREEN_WIDTH - 1, CAM_SCREEN_HEIGHT - 1,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0 | DRAW_FILLED);
    if (bm_detail_info.name[0])
        sprintf(s, "%s  [BEND%02d]", bm_detail_info.name,
                bend_store_slot_at(bm_detail_idx));
    else
        sprintf(s, "BEND%02d.BND", bend_store_slot_at(bm_detail_idx));
    s[43] = 0;
    draw_string(3, y, s, MAKE_COLOR(COLOR_BLACK, text));
    y += FONT_HEIGHT;

    // Same pin/source matrix header as the live Bend UI and image inspector.
    draw_string(1, y, "BEND", MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)));
    for (pin = BM_NBITS - 1; pin >= 0; pin--)
    {
        col = BM_NBITS - 1 - pin;
        s[0] = (char)((pin < 10) ? '0' + pin : 'a' + pin - 10); s[1] = 0;
        draw_string(labelw + col * colw + (colw - FONT_WIDTH) / 2, y, s,
                    MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)));
    }
    y += FONT_HEIGHT;
    {
    int mn = bm_detail_recipe.segs.layout
           ? bend_seg_count(bm_detail_recipe.segs.layout) : 1;
    int mi;
    for (mi = 0; mi < mn; mi++)
    {
        const bend_t *m = mi ? &bm_detail_recipe.segs.b[mi-1]
                             : &bm_detail_recipe.bend;
        sprintf(s, "B%d", mi + 1);
        draw_string(5, y, s, MAKE_COLOR(COLOR_BLACK, theme_color(TC_ACCENT)));
        for (pin = BM_NBITS - 1; pin >= 0; pin--)
        {
            col = BM_NBITS - 1 - pin;
            bend_src_name(m->route[pin], s);
            len = strlen(s);
            draw_string(labelw + col * colw + (colw - len * FONT_WIDTH) / 2, y, s,
                        MAKE_COLOR(COLOR_BLACK,
                          m->route[pin] == (unsigned char)BSRC_DATA(pin)
                            ? theme_color(TC_DIM) : text));
        }
        y += FONT_HEIGHT;
    }
    // Everything below shares the screen vertically: recipe facts occupy the
    // left half, while description and actions occupy the right. No spacer is
    // used here so four matrices still leave exactly nine readable rows.

    if (bm_detail_recipe.segs.layout)
    {
        draw_string(3, y, bend_seg_name(bm_detail_recipe.segs.layout),
                    MAKE_COLOR(COLOR_BLACK, theme_color(TC_ACCENT2)));
        for (i = 0; i < mn; i++)
        {
            sprintf(s, "%s: bend %d",
                    bend_seg_label(bm_detail_recipe.segs.layout, i), i + 1);
            draw_string(3, y + (i + 1) * FONT_HEIGHT, s,
                        MAKE_COLOR(COLOR_BLACK, text));
        }
        y += (mn + 1) * FONT_HEIGHT;
    }
#ifdef CAM_BEND_EXPERIMENTAL
    if (bm_detail_recipe.bendx_on)
    {
        int xn = bendx_chain_count(&bm_detail_recipe.bendx);
        for (i = 0; i < xn; i++)
        {
            char amt[16];
            bendx_label(&bm_detail_recipe.bendx.item[i], amt);
            sprintf(s, "%s%d %s %s", i ? "" : "EXP ", i + 1,
                    bendx_name(bm_detail_recipe.bendx.item[i].kind), amt);
            s[21] = 0;
            draw_string(3, y + i * FONT_HEIGHT, s,
                        MAKE_COLOR(COLOR_BLACK, text));
        }
        y += xn * FONT_HEIGHT;
    }
#endif
    }

    // Description gets the upper-right space between the matrix and menu.
    y = FONT_HEIGHT * (2 + (bm_detail_recipe.segs.layout
                         ? bend_seg_count(bm_detail_recipe.segs.layout) : 1));
    draw_string(CAM_SCREEN_WIDTH/2 + 4, y, "DESCRIPTION:",
                MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)));
    y += FONT_HEIGHT;
    if (!bm_detail_info.description[0])
        draw_string(CAM_SCREEN_WIDTH/2 + 4, y, "(none)",
                    MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)));
    else
    {
        const char *p = bm_detail_info.description;
        for (i = 0; *p && i < 2; i++, y += FONT_HEIGHT)
        {
            int n = strlen(p);
            if (n > 21) n = 21;
            memcpy(s, p, n); s[n] = 0;
            draw_string(CAM_SCREEN_WIDTH/2 + 4, y, s, MAKE_COLOR(COLOR_BLACK, text));
            p += n;
        }
    }

    bm_detail_menu_draw();
}

static void bm_browse_draw(void)
{
    int n     = bm_tab_items();
    int fixed = bm_tab_fixed();
    int shown = (n < BM_BR_ROWS) ? n : BM_BR_ROWS;
    int h     = bm_browse_h();
    int y     = BM_BR_Y + 4;
    int i;

    if (bm_detail) { bm_detail_draw(); return; }
    if (bm_confirm) { bm_confirm_draw(); return; }

    draw_rectangle(BM_BR_X, BM_BR_Y, BM_BR_X + BM_BR_W, BM_BR_Y + h,
                   MAKE_COLOR(COLOR_BLACK, theme_color(TC_TEXT)), RECT_BORDER1 | DRAW_FILLED);

    bm_tabs_draw(y);
    y += FONT_HEIGHT;

    if (bm_tab == BM_TAB_SAVED)
    {
        // Which region this list is loading into and saving from. Right
        // justified on the two fixed rows only: they are the two that act on
        // the region rather than on the file, and the region name in full is
        // one window across on SEG and along the bottom of the screen. Without
        // it, arriving here from a region row would give no sign that the
        // eight identical file names are about to land somewhere particular.
        char into[8];

        into[0] = 0;
        if (conf.bend_segs.layout != BSEG_OFF)
            sprintf(into, "-> %d", bend_seg_active() + 1);

        bm_row(y, bm_bsel == BM_BR_SAVE, "Save current bend", into, theme_color(TC_ACCENT));
        y += FONT_HEIGHT;

        bm_row(y, bm_bsel == BM_BR_IMAGE, "Load bend from image", 0, theme_color(TC_ACCENT));
        y += FONT_HEIGHT;

        // Marked the way a loaded preset is marked, and for the same reason:
        // with no bend loaded this is what is in effect, and the list should
        // say so rather than leaving every row unmarked.
        bm_row(y, bm_bsel == BM_BR_OFF,
               bend_is_identity(bend_seg_cur()) ? "> Off"
                                               : "  Off",
               into, bend_is_identity(bend_seg_cur()) ? theme_color(TC_ACCENT) : theme_color(TC_TEXT));
        y += FONT_HEIGHT;

        if (n == 0)
        {
            draw_string(BM_BR_TX, y, "(no saved bends)",
                        MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)));
            y += FONT_HEIGHT;
        }
        for (i = bm_btop; i < bm_btop + shown && i < n; i++)
        {
            char row[26];
            bend_store_info_t info;
            // The arrow marks the preset that is currently loaded, which is
            // not necessarily the one under the cursor - without it, reopening
            // the browser gives no clue which of eight near-identical names
            // is on.
            bend_store_get_info_at(i, &info);
            if (info.name[0])
                sprintf(row, "%c %-20.20s", (i == bend_store_cur()) ? '>' : ' ',
                        info.name);
            else
                sprintf(row, "%c BEND%02d.BND",
                        (i == bend_store_cur()) ? '>' : ' ', bend_store_slot_at(i));
            bm_row(y, bm_bsel == fixed + i, row, 0, theme_color(TC_TEXT));
            y += FONT_HEIGHT;
        }
    }
    else if (bm_tab == BM_TAB_SEG)
    {
        // The mask switch, above the layouts. Greyed with the layout off,
        // where there is one region over the whole frame and a mask of it
        // would be a screen of dots saying nothing.
        {
            int can = bm_seg_shown();
            bm_row(y, bm_bsel == 0,
                   conf.bend_seg_mask ? "Mask: showing regions"
                                      : "Mask: off",
                   0,
                   !can ? theme_color(TC_DIM)
                        : (conf.bend_seg_mask ? theme_color(TC_ACCENT)
                                              : theme_color(TC_DIM)));
            y += FONT_HEIGHT;
        }

        for (i = bm_btop; i < bm_btop + shown && i < n; i++)
        {
            bm_seg_row(y, i, bm_bsel == fixed + i);
            y += FONT_HEIGHT;
        }
        bm_sep(y);
        y += BM_SEP_H;
        bm_seg_summary(y);
        y += FONT_HEIGHT;
    }
#ifdef CAM_BEND_EXPERIMENTAL
    else
    {
        for (i = bm_btop; i < bm_btop + shown && i < n; i++)
        {
            bm_x_row(y, i, bm_bsel == fixed + i);
            y += FONT_HEIGHT;
        }
        bm_sep(y);
        y += BM_SEP_H;
        bm_x_summary(y);
        y += FONT_HEIGHT;
    }
#endif

    // Back is not another entry in the list above it, and a cursor arriving
    // there should be able to see that it has left.
    bm_sep(y);
    y += BM_SEP_H;
    bm_row(y, bm_bsel == fixed + n, "Back", 0, theme_color(TC_DIM));

    // Scroll indicator, only when the list does not fit and only while the
    // cursor is actually in it - "0/9" on the Save row would be a lie.
    if (n > BM_BR_ROWS && bm_bsel >= fixed && bm_bsel < fixed + n)
    {
        char sc[16];
        sprintf(sc, "%d/%d", bm_bsel - fixed + 1, n);
        draw_string_justified(0, BM_BR_Y + h - FONT_HEIGHT, sc,
                              MAKE_COLOR(COLOR_BLACK, theme_color(TC_DIM)),
                              0, BM_BR_RX, TEXT_RIGHT);
    }
}

//-------------------------------------------------------------------
// The segment mask.
//
// The layout drawn over the live view, so the pattern can be seen before it is
// shot rather than discovered in the picture. Same idea as the multiple
// exposure ghost: this is the one thing about a segmented bend that a list of
// region names cannot tell you, because "Truchet at 40%" is not a picture.
//
// Drawn from the same geometry the capture uses - bend_seg_spans() over the
// screen's own width and height rather than the sensor's - so what is on
// screen is the layout that will be applied, at the frame's aspect, including
// the pattern layouts' current roll. There is no second implementation of the
// shapes to drift out of step.
//
// Dithered rather than filled, and only the regions that are not region zero.
// A filled overlay is a mask you cannot frame through; leaving region zero
// clear means the pattern reads as figure against ground, which is what the
// two-region layouts actually are. The region being edited is drawn at twice
// the density, so "which one am I on" is answerable from the picture and not
// only from the list.
static int bm_mask_shown;

// Below the strip and above the status line: those two are ours and are
// repainted on their own terms, and dots landing in them would be erased by
// the next repaint of either, leaving holes in the pattern.
#define BM_MASK_Y0  (BM_H + 1)
#define BM_MASK_Y1  (BM_SY - 3)

static void bm_mask_draw(void)
{
    // 576 bytes of stack, and the reason the sizes are exactly these is in
    // include/bend.h - bend_seg_spans() writes up to BEND_SEG_SPANS entries
    // and a shorter array would lose the right-hand end of a fine pattern.
    unsigned char  seg[BEND_SEG_SPANS];
    unsigned short xend[BEND_SEG_SPANS];
    int y, i, n, active = bend_seg_active();

    for (y = BM_MASK_Y0; y < BM_MASK_Y1; y += 2)
    {
        int x0 = 0;

        n = bend_seg_spans(conf.bend_segs.layout, conf.bend_segs.size,
                           y, CAM_SCREEN_WIDTH, CAM_SCREEN_HEIGHT, seg, xend);

        for (i = 0; i < n; i++)
        {
            int x1 = xend[i];
            int r  = seg[i];

            if (r != 0)
            {
                // The dither is diagonal - (x + y) rather than x alone - so it
                // reads as a texture instead of as vertical pinstripes, which
                // is what a column-aligned pattern looks like over a moving
                // live view.
                // Both densities are powers of two so the test is a mask
                // rather than a division - this runs over every pixel of the
                // frame and a divide per pixel is forty thousand of them.
                int mask = (r == active) ? 1 : 3;
                color c  = (r == 1) ? theme_color(TC_ACCENT)
                         : (r == 2) ? theme_color(TC_ACCENT2)
                                    : theme_color(TC_SPECIAL);
                int x;
                for (x = x0; x < x1 && x < CAM_SCREEN_WIDTH; x++)
                    if ((((unsigned)(x + y)) & mask) == 0)
                        draw_pixel(x, y, c);
            }
            x0 = x1;
        }
    }
    bm_mask_shown = 1;
}

static void bm_mask_erase(void)
{
    if (!bm_mask_shown) return;
    draw_rectangle(0, BM_MASK_Y0, CAM_SCREEN_WIDTH - 1, BM_MASK_Y1,
                   MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                   RECT_BORDER0 | DRAW_FILLED);
    bm_mask_shown = 0;
}

// On when it is switched on and there is a layout to show. With the layout off
// there is one region over the whole frame and a mask of it would be a screen
// full of dots saying nothing.
static int bm_mask_on(void)
{
    return conf.bend_seg_mask && bm_seg_shown();
}

//-------------------------------------------------------------------
// The status line, foot of the screen.
//
// What is applied, in the three terms the mode has: which bend, which region,
// which fault. It used to read "d3 pure" - the depth and the trash generator -
// which are settings of the randomiser rather than a description of what the
// camera is doing, and which are now on the matrix page beside the matrix they
// belong to.
//
// Named things rather than counts wherever there is a name. A saved bend has
// one, a layout has one, an experimental profile has one; a chain of them does
// not, so a chain says "chain" and its own page carries the members.
static void bm_status_draw(void)
{
    char st[40];

    if (bm_note_live())
    {
        // Save and load confirm here rather than in a message box: a modal box
        // would take over the screen of a mode whose entire point is that the
        // shutter still works while it is open.
        draw_string(2, BM_SY, bm_note, MAKE_COLOR(COLOR_BLACK, theme_color(TC_ACCENT)));
        return;
    }

    // Left: the bend itself. The file if the matrix is still the file's, and
    // otherwise the honest two words for what it is - a hand-tuned or rerolled
    // matrix is not "BEND07" any more and saying so is the whole reason
    // bend_store_detach() exists.
    {
        int cur = bend_store_cur();
        color fg = theme_color(TC_ACCENT);

        if (cur >= 0)
            sprintf(st, "BEND%02d", bend_store_slot_at(cur));
        else if (!bend_is_identity(bend_seg_cur()))
        {
            sprintf(st, "unsaved");
            fg = theme_color(TC_WARN);
        }
        else
        {
            sprintf(st, "straight");
            fg = theme_color(TC_DIM);
        }
        draw_string(2, BM_SY, st, MAKE_COLOR(COLOR_BLACK, fg));
    }

    // Middle: which region of which layout the strip is on. Blank with the
    // layout off, when the strip is the whole sensor and there is nothing to
    // qualify.
    if (bm_seg_shown())
    {
        sprintf(st, "%s %d %s", bend_seg_name(conf.bend_segs.layout),
                bend_seg_active() + 1,
                bend_seg_label(conf.bend_segs.layout, bend_seg_active()));
        draw_string_justified(0, BM_SY, st, MAKE_COLOR(COLOR_BLACK, theme_color(TC_ACCENT2)),
                              0, CAM_SCREEN_WIDTH, TEXT_CENTER);
    }

    // Right: the second engine. One profile is named; more than one is a
    // chain, and the chain's own page says which and in what order.
#ifdef CAM_BEND_EXPERIMENTAL
    {
        int n = conf.bendx_enable ? bendx_chain_count(&conf.bendx) : 0;

        if (n == 1)
            sprintf(st, "%s%s", bendx_name(conf.bendx.item[0].kind),
                    bendx_chain_slow(&conf.bendx) ? " *" : "");
        else if (n > 1)
            sprintf(st, "chain %d%s", n,
                    bendx_chain_slow(&conf.bendx) ? " *" : "");
        else
            st[0] = 0;

        if (st[0])
            draw_string_justified(0, BM_SY, st,
                                  MAKE_COLOR(COLOR_BLACK, theme_color(TC_SPECIAL)),
                                  0, CAM_SCREEN_WIDTH - 2, TEXT_RIGHT);
    }
#endif
}

static void bm_draw(int force)
{
    static int last_hash = -1;
    char buf[8];
    int i, hash, nav_only;

    if (!bm_active) return;

    // The shutter path has already wiped the complete bitmap. Do not rebuild
    // the region mask (or any Bend controls) into Canon's capture/review copy.
    // Mark every cached layer absent so the first live-view redraw rebuilds the
    // normal mask -> grid -> controls stack from the bottom.
    if (gui_shot_ui_hidden())
    {
        bm_mask_shown = 0;
        bm_br_drawn_h = 0;
        bm_dirty = 1;
        return;
    }

    // The layout can have been switched off since the last redraw, from the
    // SEG window or from the menu, taking the cell the cursor was sitting on
    // with it. Done here rather than at every place the layout can change
    // because this is the one function all of them are followed by.
    bm_cursor_clamp();

    // Repaint only when something moved. The strip sits over a live view that
    // is being rewritten continuously, but the OSD plane is a separate buffer -
    // nothing overwrites what we draw, so redrawing every frame is pure cost.
    hash = bm_cursor * 977 + bend_seg_cur()->depth * 31 + bend_seg_cur()->trash_type;
    // Arming changes the cursor's colour and nothing else, so without this the
    // pin would stay yellow until the first source step repainted it.
    hash = hash * 3 + bm_armed;
    hash = hash * 7 + conf.bitbend_simple;
    // Everything the panel and the bar show is part of what "something moved"
    // means. The timed message especially: nothing else changes when it expires,
    // and left out of the hash it would sit there until the next key press.
    hash = hash * 101 + bend_store_cur() * 13 + bend_store_count();
    hash = hash * 31 + bm_browse * 7 + bm_bsel * 3 + bm_btop;
    hash = hash * 3 + bm_note_live();
    // The experimental profile is on the status line and on every row of its
    // window, and the rocker changes it without moving the cursor - so it has
    // to be in here or turning the knob would show nothing until the next
    // unrelated press.
    hash = hash * 37 + bm_tab * 11;
    // The layout changes what every region row says and the active region
    // changes the whole strip, and both move without the cursor moving - the
    // rocker turns the size, and picking a region closes the panel onto a
    // different matrix.
    hash = hash * 53 + conf.bend_segs.layout * 7 + bend_seg_active() * 3
                     + conf.bend_segs.size;
    // The preset name on each region row, which changes without the cursor
    // moving every time one is loaded, saved or edited away.
    for (i = 0; i < BEND_SEG_MAX; i++)
        hash = hash * 29 + conf.bend_segs.slot[i];
    hash = hash * 43 + bm_confirm * 3 + bm_confirm_yes;
    hash = hash * 67 + conf.bend_seg_mask;
#ifdef CAM_BEND_EXPERIMENTAL
    hash = hash * 5 + conf.bendx_enable + bm_x_knob * 7;
    hash = hash * 17 + conf.bendx.n;
    for (i = 0; i < BENDX_CHAIN_MAX; i++)
        hash = hash * 41 + conf.bendx.item[i].kind * 7
             + conf.bendx.item[i].amount * 3 + conf.bendx.item[i].reach
             + conf.bendx.item[i].lanes * 5 + conf.bendx.item[i].mix * 11
             + conf.bendx.item[i].rowmod * 13;
#endif
    for (i = 0; i < BM_NBITS; i++) hash = hash * 33 + bend_seg_cur()->route[i];
    if (!force && !bm_dirty && hash == last_hash) return;
    last_hash = hash;
    nav_only = !force && bm_nav_only;
    bm_nav_only = 0;
    bm_dirty  = 0;

    // Plain list navigation changes only the highlight. Repainting the panel
    // is cheap and, crucially, does not erase/rebuild the live-view mask, grid,
    // patch strip and status bar underneath it.
    if (nav_only && bm_browse)
    {
        if (bm_detail) bm_detail_menu_draw();
        else           bm_browse_draw();
        bm_br_drawn_h = bm_browse_h();
        return;
    }

    draw_rectangle(0, 0, CAM_SCREEN_WIDTH - 1, BM_H,
                   MAKE_COLOR(COLOR_BLACK, COLOR_BLACK),
                   RECT_BORDER0 | DRAW_FILLED);

    if (!nav_only)
    {
        draw_rectangle(0, BM_SY - 2, CAM_SCREEN_WIDTH - 1, BM_SY + BM_SH - 3,
                       MAKE_COLOR(COLOR_BLACK, COLOR_BLACK),
                       RECT_BORDER0 | DRAW_FILLED);

    // Remove the old panel footprint before rebuilding the layers underneath
    // it. This used to happen at the end of the function: the mask and grid
    // were correctly repainted, then this transparent erase wiped them out
    // again until the next key caused another redraw.
    if (!bm_browse && bm_br_drawn_h)
    {
        draw_rectangle(BM_BR_X, BM_BR_Y, BM_BR_X + BM_BR_W,
                       BM_BR_Y + bm_br_drawn_h,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0 | DRAW_FILLED);
        bm_br_drawn_h = 0;
    }
    else if (bm_browse && bm_br_drawn_h > bm_browse_h())
    {
        int h = bm_browse_h();
        draw_rectangle(BM_BR_X, BM_BR_Y + h, BM_BR_X + BM_BR_W,
                       BM_BR_Y + bm_br_drawn_h,
                       MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT),
                       RECT_BORDER0 | DRAW_FILLED);
    }

    // The mask goes down before anything else, so the strip, the status line
    // and the panel all sit on top of it.
    if (bm_mask_on()) { bm_mask_erase(); bm_mask_draw(); }
    else              bm_mask_erase();

    // Then the grid, over the mask and under the controls.
    //
    // gui_redraw() paints the grid once on entry to this mode and the controls
    // go over it, which was the right order while the mask did not exist. The
    // mask is drawn from here, so it landed on the grid and buried the very
    // thing the reticle is for - the mask shows the pattern and the grid says
    // where the middle of the frame is, and framing a shot needs both. Redrawn
    // here rather than clipped, because the rest of this function paints over
    // the grid's opaque bands anyway.
    gui_bend_grid_repaint();
    }

    // The region cell, leftmost, when a layout is on. Drawn with the same two
    // rows and the same three colours a pin has, because it is the same kind
    // of thing: something the cursor lands on, SET takes hold of, and the
    // rocker turns. The top row names it and the bottom row is its value, so
    // the strip reads as one row of cells and not as a widget beside a strip.
    if (bm_seg_shown())
    {
        int sel = (bm_cursor == BM_CUR_SEG);
        color fg, bg;
        char v[8];

        if (sel && bm_armed) { bg = theme_color(TC_EDIT);   fg = theme_color(TC_TEXT); }
        else if (sel)        { bg = theme_color(TC_HILITE); fg = theme_color(TC_HILITE_FG); }
        else                 { bg = COLOR_BLACK;          fg = theme_color(TC_ACCENT2); }

        if (sel)
            draw_rectangle(0, BM_Y0, BM_SEGW - 2, BM_Y1 + FONT_HEIGHT - 1,
                           MAKE_COLOR(bg, bg), RECT_BORDER0 | DRAW_FILLED);

        draw_string((BM_SEGW - 3 * FONT_WIDTH) / 2, BM_Y0, "SEG",
                    MAKE_COLOR(bg, fg));
        sprintf(v, "%d/%d", bend_seg_active() + 1, bend_seg_n());
        draw_string((BM_SEGW - (int)strlen(v) * FONT_WIDTH) / 2, BM_Y1, v,
                    MAKE_COLOR(bg, fg));
    }

    for (i = 0; i < BM_NBITS; i++)
    {
        // Drawn most significant first, left to right, because that is how the
        // word is written down and how the bits are numbered when talking
        // about them. route[] is indexed the other way.
        int pin  = BM_NBITS - 1 - i;
        int colw = bm_colw();
        int x    = bm_segx() + i * colw;
        int sel  = (pin == bm_cursor);
        color fg, bg;

        // Yellow is where the cursor is, unchanged. Red is that same pin armed
        // for editing - a different colour rather than a marker beside it,
        // because the difference is which keys are live and that has to be
        // readable from the same glance that reads the source name.
        if (sel && bm_armed) { bg = theme_color(TC_EDIT);   fg = theme_color(TC_TEXT); }
        else if (sel)        { bg = theme_color(TC_HILITE); fg = theme_color(TC_HILITE_FG); }
        else                 { bg = COLOR_BLACK;          fg = theme_color(TC_TEXT); }

        if (sel)
            draw_rectangle(x, BM_Y0, x + colw - 2, BM_Y1 + FONT_HEIGHT - 1,
                           MAKE_COLOR(bg, bg), RECT_BORDER0 | DRAW_FILLED);

        buf[0] = (char)((pin < 10) ? ('0' + pin) : ('a' + pin - 10));
        buf[1] = 0;
        draw_string(x + (colw - FONT_WIDTH) / 2, BM_Y0, buf, MAKE_COLOR(bg, fg));

        bend_src_name(bend_seg_cur()->route[pin], buf);
        {
            // A pin still on its own data line is the unbent state, so it is
            // greyed - what stands out should be what has been changed.
            int len = (int)strlen(buf);
            color sfg = fg;
            if (!sel && bend_seg_cur()->route[pin] == (unsigned char)BSRC_DATA(pin))
                sfg = theme_color(TC_DIM);
            draw_string(x + (colw - len * FONT_WIDTH) / 2, BM_Y1, buf,
                        MAKE_COLOR(bg, sfg));
        }
    }


    if (!nav_only) bm_status_draw();

    // Last, so it sits over the strip rather than under it.
    //
    // The panel is the one thing this mode draws outside the two bands erased
    // above, so closing it has to wipe its own footprint - otherwise it stays
    // on screen over the live view until something else repaints that area,
    // which in this mode is nothing. The height is remembered rather than
    // recomputed because the list may have grown while the panel was open.
    if (!nav_only && bm_browse)
    {
        int h = bm_browse_h();

        bm_browse_draw();
        bm_br_drawn_h = h;
    }
}

//-------------------------------------------------------------------
// Keys
//
// Returning 1 blocks the key from Canon, returning 0 lets it through. Only the
// keys this mode actually uses are consumed, so the shutter, the mode switch
// and the zoom rocker keep working exactly as they do on the plain shooting
// screen - which is the whole point of the mode.

#define BM_REPEAT_DELAY 400     // ms held before auto-repeat starts
#define BM_REPEAT_RATE  110     // ms between repeats

// SET held this long means "get rid of the thing under the cursor" rather than
// "pick it". Longer than the ALT hold that opens the mode (CAM_BEND_HOLD_MS,
// 1s on this body) would make it feel stuck; much shorter and the second press
// of the SET, SET that locks a profile starts tripping it. 600ms sits clear of
// both - a deliberate hold, but not a wait.
#define BM_HOLD_MS      600

static inline __attribute__((always_inline)) void bm_confirm_key(long key)
{
    switch (key)
    {
    case KEY_UP:
    case KEY_DOWN:  bm_confirm_yes = !bm_confirm_yes; bm_nav_only = 1; break;
    case KEY_SET:   bm_confirm_act();                 break;
    case KEY_LEFT:
    case KEY_RIGHT: bm_confirm_close();               break;
    }
}

static inline __attribute__((always_inline)) void bm_detail_key(long key)
{
    switch (key)
    {
    case KEY_UP:
        bm_detail_action = (bm_detail_action + BM_DETAIL_ACTIONS - 1) % BM_DETAIL_ACTIONS;
        bm_nav_only = 1;
        break;
    case KEY_DOWN:
        bm_detail_action = (bm_detail_action + 1) % BM_DETAIL_ACTIONS;
        bm_nav_only = 1;
        break;
    case KEY_LEFT:
    case KEY_MENU:
        bm_detail = 0;
        gui_set_need_restore();
        break;
    case KEY_SET:
        bm_detail_act();
        break;
    }
}

static inline __attribute__((always_inline)) void bm_browse_key(long key)
{
    switch (key)
    {
    case KEY_UP:    bm_browse_move(-1); break;
    case KEY_DOWN:  bm_browse_move(1);  break;
    case KEY_SET:   bm_browse_pick();   break;
#if BM_TAB_COUNT > 1
#ifdef CAM_BEND_NO_ROCKER
    case KEY_LEFT:  if (!bm_tune_row(-1)) bm_tab_move(-1); break;
    case KEY_RIGHT: if (!bm_tune_row(1))  bm_tab_move(1);  break;
#else
    case KEY_LEFT:  bm_tab_move(-1); break;
    case KEY_RIGHT: bm_tab_move(1);  break;
#endif
#else
    case KEY_LEFT:
    case KEY_RIGHT: bm_browse_close(); break;
#endif
    case KEY_ZOOM_IN:
        bm_tune_row(1);
        break;
    case KEY_ZOOM_OUT:
        bm_tune_row(-1);
        break;
    }
}

static inline __attribute__((always_inline)) void bm_armed_key(long key)
{
    if (bm_cursor == BM_CUR_SEG)
    {
        switch (key)
        {
        case KEY_UP:   bm_step_seg(1);  break;
        case KEY_DOWN: bm_step_seg(-1); break;
        case KEY_SET:  bm_armed = 0;    break;
        }
        return;
    }

    switch (key)
    {
    case KEY_UP:   bm_step_source(1);  break;
    case KEY_DOWN: bm_step_source(-1); break;
    case KEY_SET:  bm_armed = 0;       break;
    }
}

static void bm_act(long key)
{
    if (bm_detail)
        bm_detail_key(key);
    else if (bm_confirm)
        bm_confirm_key(key);
    else if (bm_browse)
        bm_browse_key(key);
    else if (bm_armed)
        bm_armed_key(key);
    else
    {

    switch (key)
    {
#ifdef CAM_BEND_PRESET_ARROWS
    // The zoom rocker walks saved bends. Editing or randomising detaches the
    // live matrix; the next rocker press enters the saved sequence again.
    case KEY_ZOOM_OUT:
        if (bend_store_step(bend_seg_cur(), -1))
        {
            bend_sanitize(bend_seg_cur(), BM_NBITS);
            if (conf.bitbend_simple) bend_simplify(bend_seg_cur());
            // The rocker is the third way a region acquires a preset, so it
            // records one like the other two.
            bend_seg_set_slot(bend_seg_active(),
                              bend_store_slot_at(bend_store_cur()));
            bm_note_set("previous saved bend");
        }
        else bm_note_set("no saved bends");
        break;
    case KEY_ZOOM_IN:
        if (bend_store_step(bend_seg_cur(), 1))
        {
            bend_sanitize(bend_seg_cur(), BM_NBITS);
            if (conf.bitbend_simple) bend_simplify(bend_seg_cur());
            bend_seg_set_slot(bend_seg_active(),
                              bend_store_slot_at(bend_store_cur()));
            bm_note_set("next saved bend");
        }
        else bm_note_set("no saved bends");
        break;
    // Left/right select pins, and the region cell when there is one - see
    // bm_move_cursor() for why moving left counts up through route[].
    case KEY_LEFT:  bm_move_cursor(1);                                     break;
    case KEY_RIGHT: bm_move_cursor(0);                                     break;
#else
    case KEY_LEFT:  bm_move_cursor(1);                                     break;
    case KEY_RIGHT: bm_move_cursor(0);                                     break;
#endif
#ifdef CAM_BEND_ENTER_UP
    // The key that opened the mode closes it again. A gesture you have to
    // reverse with a different, longer one is a gesture you have to remember,
    // and UP is free here precisely because nothing else on the strip wants it
    // while no pin is armed. The ALT hold still works, and is still the only
    // way out on a body that enters by holding.
    case KEY_UP:    gui_bend_exit();                                       break;
#endif

    // DOWN is dead while nothing is armed. Deliberately, rather than falling
    // back to moving the cursor: the pair reads as one rocker, and half a
    // rocker doing something on its own is worse than neither half doing
    // anything until there is a pin to turn.

    // SET arms the pin under the cursor. It used to open the preset browser,
    // which is on MENU now - the browser is a thing you visit, and picking a
    // source is the thing this mode is for, so the nearer key belongs to it.
    case KEY_SET:
        bm_armed   = 1;
        if (bm_cursor == BM_CUR_SEG) bm_arm_seg = bend_seg_active();
        else                         bm_arm_src = bend_seg_cur()->route[bm_cursor];
        break;
    }
    }
    bm_dirty = 1;
}

static int bm_kbd_process(void)
{
    // The rocker is listed unconditionally now. CAM_BEND_PRESET_ARROWS still
    // decides what it does on the strip, but inside the browser it is the
    // selected experimental knob on every body - and a key that is not in this list is a key
    // the mode never sees.
    static const long keys[] = {
        KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_SET, KEY_MENU,
        KEY_ZOOM_IN, KEY_ZOOM_OUT,
    };

    // Being in the list is not the same as consuming it. On a body without
    // CAM_BEND_PRESET_ARROWS the rocker means nothing to the strip, so outside
    // the browser it has to reach Canon and go on zooming - blocking a key in
    // order to ignore it is how a mode ends up feeling broken.
    #ifdef CAM_BEND_PRESET_ARROWS
      #define BM_KEY_USED(k)    1
    #else
      #define BM_KEY_USED(k)    (bm_browse || \
                                 ((k) != KEY_ZOOM_IN && (k) != KEY_ZOOM_OUT))
    #endif
    static long held;
    static int  held_since, last_repeat;
    static int  set_hold_fired;     // the hold already acted for this press
    long down = 0;
    unsigned i;

    if (!bm_active) { held = 0; set_hold_fired = 0; return 0; }

    for (i = 0; i < sizeof(keys)/sizeof(keys[0]); i++)
        if (kbd_is_key_pressed(keys[i]) && BM_KEY_USED(keys[i]))
        { down = keys[i]; break; }
    #undef BM_KEY_USED

    // The press that opened the mode, still down. Block it - the mode is on
    // screen and the key must not reach Canon either - but act on nothing
    // until it is released and the next press starts clean.
    if (bm_swallow_entry)
    {
        if (down) return 1;
        bm_swallow_entry = 0;
    }

    if (!down)
    {
        // SET acts here, on release, not on the down edge - see the note on
        // BM_HOLD_MS below. A press that already fired its hold action is
        // spent, and must not also do the short one on the way up.
        if (held == KEY_SET && !set_hold_fired) bm_act(KEY_SET);
        held = 0;
        set_hold_fired = 0;
        // Nothing of ours is down, so everything - the shutter above all -
        // goes to Canon untouched. This is what lets you shoot without
        // leaving the mode.
        return 0;
    }

    if (held != down)
    {
        held           = down;
        held_since     = get_tick_count();
        last_repeat    = held_since;
        set_hold_fired = 0;
        // SET is the one key that does not act on the down edge. It has two
        // meanings now - pick, and get rid of - and the only way to tell them
        // apart is how long it is held, which is not known yet. Every other
        // key still fires immediately and auto-repeats, because none of them
        // has a second meaning to wait for.
        if (down == KEY_SET) return 1;
        if (down == KEY_MENU)
        {
            // Handled here rather than through the mode's menu button hook,
            // because that hook returns 0 and Canon would see the press and
            // open its own menu behind us on the way out.
            //
            // MENU is the browser now - press to open the panel, press again
            // to put it away - and it reads as one ladder of "back" from the
            // deepest thing on screen outwards. The delete question is the
            // deepest, and backing out of it has to mean "no" rather than "no,
            // and also close the list". An armed pin is the same shape of
            // thing: cancel the edit, put back the source it had when SET
            // armed it, and stay where you are.
            //
            // The way out of the mode is the hold on the ALT button, which is
            // where it already was. MENU cannot be both, and it is the popup
            // that gets pressed twenty times a session.
            if (bm_detail)      { bm_detail = 0; gui_set_need_restore(); }
            else if (bm_confirm) bm_confirm_close();
            else if (bm_browse) bm_browse_close();
            else if (bm_armed)
            {
                // Cancel puts back what was there when SET armed it, whether
                // that was a source on a pin or which region the strip was on.
                if (bm_cursor == BM_CUR_SEG) bend_seg_set_active(bm_arm_seg);
                else bend_seg_cur()->route[bm_cursor] = bm_arm_src;
                bm_armed = 0;
                bm_dirty = 1;
            }
            else                bm_browse_open();
            return 1;
        }
        bm_act(down);
    }
    else if (down == KEY_SET)
    {
        // Held long enough to mean the other thing. Fires once, at the
        // threshold rather than on release, so the panel changes under your
        // thumb and the gesture confirms itself while the key is still down -
        // a delete prompt that appears only after you let go gives no feedback
        // that holding was the right thing to do.
        // Only counted as spent where the hold actually means something. On the
        // strip SET has no second meaning, and marking it fired there would
        // make a slow, deliberate press do nothing at all - which matters more
        // now that SET is how a pin is armed and committed rather than an
        // occasional way into the browser.
        int t = get_tick_count();
        if (!set_hold_fired && (t - held_since) >= BM_HOLD_MS
            && bm_browse && !bm_confirm && !bm_detail)
        {
            set_hold_fired = 1;
            bm_browse_hold();
            bm_dirty = 1;
        }
    }
    else if (down != KEY_MENU)
    {
        // Auto-repeat. There are 46 sources to walk per pin, so stepping one
        // press at a time would make the far end of the list unreachable in
        // practice.
        int t = get_tick_count();
        if ((t - held_since) >= BM_REPEAT_DELAY && (t - last_repeat) >= BM_REPEAT_RATE)
        {
            last_repeat = t;
            bm_act(down);
        }
    }

    // Blocked from Canon for as long as one of ours is down. Blocking is all
    // or nothing per tick - the platform swaps the whole masked key set for
    // CHDK's shadow copy - which is the same deal every other CHDK key grab
    // on the shooting screen makes.
    return 1;
}

gui_handler bendGuiHandler =
    { GUI_MODE_MODULE, bm_draw, bm_kbd_process, 0, 0, 0 };

#endif // CAM_BEND_MODE
