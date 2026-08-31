#ifndef BEND_SEG_H
#define BEND_SEG_H

//-------------------------------------------------------------------
// Segments, config side - see the model and the geometry in include/bend.h.
//
// The geometry is pure and lives in bend.c so the host test can prove it. This
// is the other half: which matrix belongs to which region, given that segment
// zero's matrix is the camera's one bend and lives where it always did.
//
// Everything that edits a matrix - the pin strip, the menu, the randomiser -
// goes through bend_seg_cur(), so all of them follow the segment the UI is on
// without any of them knowing that segments exist. With the layout off that
// function returns &conf.bitbend and the whole feature is inert.
//-------------------------------------------------------------------

#include "bend.h"

// Matrix for one region. Segment 0 is conf.bitbend; 1..3 are in conf.bend_segs.
// Out of range comes back as segment 0 rather than null - every caller is on a
// path that has to write somewhere, and there is no useful failure here.
bend_t *bend_seg_conf(int seg);

// The matrix the UI is editing.
bend_t *bend_seg_cur(void);

// Regions in the current layout, and the one being edited.
int  bend_seg_n(void);
int  bend_seg_active(void);
void bend_seg_set_active(int seg);

// Which preset a region is showing, or -1 for none. Set it when a preset is
// loaded into or saved from a region, and clear it - bend_seg_detach() - the
// moment that region's matrix is edited, so a name on a row always means the
// matrix under it is still that file.
int  bend_seg_slot(int seg);
void bend_seg_set_slot(int seg, int slot);
void bend_seg_detach(void);

// Sanitize the layout and every matrix in it, and simplify them if the config
// says so. The gate on entry to bend mode and before a shot, in place of the
// bend_sanitize() call each of those used to make on conf.bitbend alone.
void bend_seg_prep(int nbits);

// Whether anything would actually be applied - any region with a matrix that
// is not straight through. What decides if a shot gets a sidecar.
int  bend_seg_any(void);

#endif
