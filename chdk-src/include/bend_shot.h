#ifndef BEND_SHOT_H
#define BEND_SHOT_H

//-------------------------------------------------------------------
// What a picture was taken with - see docs/EXPERIMENTAL_EFFECTS.md.
//
// A bend that produced a good frame is worth more than a bend you saved on
// purpose, because you did not know it was good until you saw the picture. By
// then the matrix has been rerolled twice and the chain has moved on, and the
// only record of what made it is the frame itself.
//
// So every shot writes a small file beside its JPEG holding the whole state -
// the routing matrix and the profile chain, exactly as they were applied - and
// picking that picture again loads both back.
//
//   A/DCIM/100CANON/IMG_0123.JPG    the picture
//   A/DCIM/100CANON/IMG_0123.BND    what it was taken with
//
// Beside the image rather than in a folder of its own so that copying the
// DCIM directory off the card brings the recipes with it, and so that a
// directory listing shows at a glance which frames were bent. CHDK already
// writes DNGs into the same directory on this camera, so a non-Canon file
// there is not a new idea.
//
// Not inside the JPEG. That was the first design and it is not safe here: the
// only hook this port has into file writing is the PTP chunk path, which
// delivers data to a host rather than intercepting the card write, so
// embedding an APP segment would mean reversing Canon's writer and getting it
// exactly right on a camera whose photographs matter. A sidecar costs one file
// per frame and cannot corrupt a picture.
//-------------------------------------------------------------------

#include "bend.h"
#include "bendx.h"

#define BEND_SHOT_EXT       ".BND"
#define BEND_SHOT_PATHLEN   64

//-------------------------------------------------------------------
// File layout. Fixed size, like the preset files, and for the same reason:
// these are packed blobs with no internal length markers, so a file written by
// a build with different structs cannot be read half way.
//
//   0   magic  'B','S','H','1'
//   4   ver    2 bytes
//   6   bsize  2 bytes, sizeof(bend_t) as written
//   8   xsize  2 bytes, sizeof(bendx_chain_t) as written
//   10  flags  2 bytes, which engines were switched on
//   12  ssize  2 bytes, sizeof(bend_segs_t) as written    - version 2 on
//   14  pad    2 bytes
//   16  bend_t
//   ..  bendx_chain_t
//   ..  bend_segs_t                                       - version 2 on
//
// Version 2 added the segments. Version 3 compacts the experimental effect
// numbers after five retired profiles; the reader migrates surviving version
// 1 and 2 recipes and drops retired members from their chains.
// The header grew rather than the segments being
// appended silently, because the three sizes are the whole safety story here -
// a file whose structs are not this build's structs is refused, and a fourth
// blob with no declared size would be the one part that was not.
//
// Version 1 files still load, with the layout switched off, which is exactly
// what they were taken with. Files this build writes are not readable by a
// build that predates segments - it checks the version - and that is the right
// way round: refusing to read is safe, reading nine tenths of a record is not.
//
// Deliberately a different magic from the BND1 of a saved preset, even though
// the extension is shared. The two live in different directories and mean
// different things, and a preset file dropped in here - or one of these
// dropped into A/CHDK/BENDS - should be refused rather than half read.

#define BEND_SHOT_F_BEND    1
#define BEND_SHOT_F_BENDX   2

// The record on its own, without a file around it.
//
// Lifted out when the same bytes started being written somewhere other than a
// sidecar - they are now also carried inside the photograph, in a JPEG comment
// segment (core/bend_tag.c). One packer and one unpacker, so the two places a
// recipe can live cannot drift apart: a picture tagged by this build and a
// sidecar written by it hold byte-identical records.
#define BEND_SHOT_REC_MAX   (16 + sizeof(bend_t) + sizeof(bendx_chain_t) \
                                + sizeof(bend_segs_t))

// Returns the number of bytes written into out, or 0 if it would not fit.
int bend_shot_pack(unsigned char *out, int outlen,
                   const bend_t *b, int bend_on,
                   const bendx_chain_t *c, int bendx_on,
                   const bend_segs_t *s);

// Read one back out of a buffer. Any output may be null. Returns 1 if the
// record was understood - same version handling, same sanitizing, as reading
// a sidecar off the card.
int bend_shot_unpack(const unsigned char *rec, int len,
                     bend_t *b, int *bend_on,
                     bendx_chain_t *c, int *bendx_on,
                     bend_segs_t *s);

// Write the state beside the picture that is about to be saved. dir is the
// target directory from get_target_dir_name(); num is get_target_file_num().
// Returns 1 on success.
int bend_shot_save(const char *dir, long num,
                   const bend_t *b, int bend_on,
                   const bendx_chain_t *c, int bendx_on,
                   const bend_segs_t *s);

// Sidecar path for a picture. Any extension is replaced; a path that is
// already the sidecar comes back unchanged. Returns 0 if it would not fit.
int bend_shot_path(const char *picture, char *out, int outlen);

// Read one back. Any output may be null. Returns 1 if the file was read. A
// version 1 file fills s with the layout switched off rather than failing.
int bend_shot_load(const char *picture,
                   bend_t *b, int *bend_on,
                   bendx_chain_t *c, int *bendx_on,
                   bend_segs_t *s);

#endif
