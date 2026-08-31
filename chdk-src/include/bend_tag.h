#ifndef BEND_TAG_H
#define BEND_TAG_H

//-------------------------------------------------------------------
// The recipe inside the photograph.
//
// A bent frame used to be recorded in a sidecar beside its JPEG. That works
// and it is still here, but it is a second file: copy the picture out of DCIM
// on its own and the record of what made it stays behind on the card. So the
// same record now goes *into* the picture, as a JPEG comment segment right
// after the SOI marker:
//
//   FFD8              start of image
//   FFFE <len>        comment
//     @rewired_optics
//     bend 9:d0 8:d8 7:~d7 6:NZ 5:d5 4:LO ...
//     depth 3  trash noise
//     seg quarters  1:BEND07 2:bent 3:- 4:-
//     x bit slip 11 > row addr 3
//     BSH1:<hex>     the exact bytes a sidecar holds
//   FFE1 ...          Canon's EXIF, untouched
//
// Two readerships, one segment. Everything above the last line is for a person
// - it shows up in any tool that displays a JPEG comment, and `strings` finds
// it - and the last line is for this camera, so a picture can be picked in
// playback and its bend put back on the sensor exactly.
//
// Why a comment and not EXIF. Writing into Canon's APP1 means understanding
// and rewriting a TIFF IFD whose offsets are all relative to itself, on a
// camera whose photographs matter. A COM segment is a length and some bytes,
// it is legal anywhere in the header, and every decoder in the world skips
// segments it does not care about.
//
// Why after the shot rather than during it. The only hook into the write path
// delivers chunks to a host, not to the card (see include/bend_shot.h), so
// there is no way to intercept the bytes on their way out. The file is
// therefore rewritten once it is closed - which means a copy, which is why
// this happens on the idle path (bend_tag_service) and only for frames that
// were actually bent.
//-------------------------------------------------------------------

#include "bend.h"
#include "bendx.h"
#include "bend_seg.h"

#define BEND_TAG_MARK       "@rewired_optics"
#define BEND_TAG_PATHLEN    64

// Remember that this shot needs tagging, and build its comment now - the
// matrix can be rerolled before the file finishes being written. dir and num
// are what raw_bend_shot() already has.
void bend_tag_queue(const char *dir, long num,
                    const bend_t *b, int bend_on,
                    const bendx_chain_t *c, int bendx_on,
                    const bend_segs_t *s);

// Called from the spy task. Does nothing at all unless a shot is queued and
// its JPEG is finished, so it is safe on every pass.
void bend_tag_service(void);

// Read a tagged picture back. Returns 1 if the comment was there and held a
// record this build understands.
int bend_tag_read(const char *picture,
                  bend_t *b, int *bend_on,
                  bendx_chain_t *c, int *bendx_on,
                  bend_segs_t *s);

#endif
