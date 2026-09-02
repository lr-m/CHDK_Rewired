#ifndef BEND_TAG_H
#define BEND_TAG_H

//-------------------------------------------------------------------
// The recipe inside the photograph.
//
// A bent frame used to be recorded in a sidecar beside its JPEG. That works
// and it is still here, but it is a second file: copy the picture out of DCIM
// on its own and the record of what made it stays behind on the card. So the
// same record now goes *into* the picture, as a JPEG comment segment:
//
//   FFD8              start of image
//   FFE1 ...          Canon's EXIF, untouched
//   ...               the picture
//   FFD9              end of image
//   FFFE <len>        comment
//     @rewired_optics
//     bend 9:d0 8:d8 7:~d7 6:NZ 5:d5 4:LO ...
//     depth 3  trash noise
//     seg quarters  1:BEND07 2:bent 3:- 4:-
//     x bit slip 11 > row addr 3
//     BSH1:<hex>     the exact bytes a sidecar holds
//
// Past the EOI rather than up with the header, on every body, for two separate
// reasons that happen to want the same thing. Where CHDK owns the write path
// (CAM_BEND_TAG_INJECT) the tag makes the file longer than Canon believes it
// wrote, and only bytes after the EOI can be missed by a short read without
// costing the picture - see fwt_close(). Where it does not, the tag is appended
// to the finished file from spytask, and appending is the only way to add to a
// file without rewriting all of it - see bt_append(). One placement, one
// reader, and a picture tagged on any of these bodies reads back on any other.
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

// Called from CHDK's replacement file-write task (platform/generic/filewrite.c)
// in Canon's own task context: the first as Canon opens a file, the second when
// it closes one. Together they are how the tagger knows the picture is finished
// without opening it to find out. A copy and a compare - no allocation, no I/O.
void bend_tag_note_file_open(const char *name);
void bend_tag_note_file_closed(void);

// The comment segment to insert after the SOI as Canon writes the picture, or
// NULL if there is nothing to insert into this file. See core/bend_tag.c.
int bend_tag_segment(const unsigned char **hdr, int *hdrlen,
                     const char **text, int *textlen);
void bend_tag_segment_written(void);

// Read a tagged picture back. Returns 1 if the comment was there and held a
// record this build understands.
int bend_tag_read(const char *picture,
                  bend_t *b, int *bend_on,
                  bendx_chain_t *c, int *bendx_on,
                  bend_segs_t *s);

#endif
