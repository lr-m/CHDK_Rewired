#ifndef BEND_STORE_H
#define BEND_STORE_H

//-------------------------------------------------------------------
// Bend presets on the card - see BENDING_DESIGN.md.
//
// A bend that took twenty presses to dial in is worth keeping, and the config
// block only ever holds the one that is loaded. This is the other half: a
// directory of numbered files, an index into it, and a step operation, so a
// patch can be saved and the saved ones walked through with the arrows.
//
// Separate from bend.c because that file is compiled on the host by
// tools/bend_selftest.c and must stay free of CHDK types; separate from
// gui_bend.c because the menu in gui.c reaches these too, and gui_bend.c is
// compiled out entirely on a camera without CAM_BEND_MODE.
//-------------------------------------------------------------------

#include "bend.h"
#include "bendx.h"

#define BEND_STORE_DIR      "A/CHDK/BENDS"
#define BEND_STORE_MAX      64      // presets tracked at once
#define BEND_STORE_SLOTS    100     // BEND00.BND .. BEND99.BND
#define BEND_STORE_NAME_MAX 24
#define BEND_STORE_DESC_MAX 80

typedef struct
{
    char name[BEND_STORE_NAME_MAX + 1];
    char description[BEND_STORE_DESC_MAX + 1];
} bend_store_info_t;

typedef struct
{
    bend_t bend;
    bendx_chain_t bendx;
    bend_segs_t segs;
    unsigned char bend_on, bendx_on, pad[2];
} bend_store_recipe_t;

// How many presets are on the card. Scans the directory on the first call and
// then answers from the cached list, so this is safe to call from a redraw.
int bend_store_count(void);

// Re-read the directory. Returns the new count. Call after anything that could
// have changed it from outside - card swap, entering bend mode.
int bend_store_rescan(void);

// Slot number (the NN in BENDNN.BND) of the i'th preset, or -1.
int bend_store_slot_at(int i);

// Where a slot number - the NN in BENDNN.BND - sits in the list, or -1 if the
// card no longer has it. The inverse of bend_store_slot_at().
int bend_store_index_of(int slot);

// Index of the preset currently loaded, or -1 if none has been. Only ever set
// by this module, so both UIs share one position in the list.
int bend_store_cur(void);

// The live matrix no longer matches the loaded preset. Left/right stepping
// starts from the first/last saved bend again after this.
void bend_store_detach(void);

// Move the cursor by delta, wrapping, and load what it lands on into b.
// Returns 1 if a preset was loaded. delta 0 reloads the current one.
int bend_store_step(bend_t *b, int delta);

// Load the i'th preset and put the cursor on it. Returns 1 if it loaded.
// The browser picks by index, and expressing that as a delta from the cursor
// is wrong when nothing is loaded yet - there is no position to count from.
int bend_store_load_at(int i, bend_t *b);

// Version 2 preset files carry optional user-facing text after the matrix.
// Version 1 files remain readable and simply return two empty strings.
int bend_store_get_info_at(int i, bend_store_info_t *info);
int bend_store_set_info_at(int i, const bend_store_info_t *info);

int bend_store_save_recipe(const bend_store_recipe_t *recipe,
                           const bend_store_info_t *info);
int bend_store_save_current(void);
int bend_store_peek_recipe_at(int i, bend_store_recipe_t *recipe);
int bend_store_load_recipe_at(int i, bend_store_recipe_t *recipe);

// Delete the preset at the cursor. Returns 1 if a file went away.
int bend_store_delete_cur(void);

// Delete the i'th preset, whether or not it is the one loaded. The browser
// deletes what the cursor is over, which is not the same thing as what is
// applied - and deleting the row you are looking at should not require
// loading it first.
int bend_store_delete_at(int i);

#endif
