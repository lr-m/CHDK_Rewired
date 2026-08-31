#ifndef GBC_CAM_H
#define GBC_CAM_H

//-------------------------------------------------------------------
// The Game Boy Camera cartridge - MAC-GBD mapper plus an M64282FP image
// sensor. Cartridge type 0xfc, which upstream koenk/gbc explicitly does not
// implement ("Camera not supported", core/state.c).
//
// This is the whole point of the exercise: the ROM asks its cartridge for a
// picture, and the cartridge hands back whatever the A480's live viewport was
// looking at, run through the sensor's own exposure, edge-enhancement and
// dithering hardware. The Canon sensor feeds the Nintendo sensor.
//
// Derived from AntonioND's reverse-engineering of the real cartridge
// (github.com/AntonioND/gbcam-rev-engineer, GPL-3, doc/sample_code.c and
// doc/gb_camera_doc_v1_1_1.pdf). CHDK is GPL, so the licences agree.
//
// Memory map, from that document:
//
//   0000-1fff  w   SRAM enable (0x0a enables)
//   2000-3fff  w   ROM bank number, 6 bits, banks 1..63
//   4000-5fff  w   bit 4 set -> camera registers at a000; else SRAM bank 0..15
//   a000-bfff  rw  SRAM bank, or the 54-byte register file mirrored every 0x80
//
// The captured image is written by the cartridge hardware into SRAM bank 0 at
// offset 0x0100, as 14x16 tiles of standard 2bpp Game Boy tile data.
//-------------------------------------------------------------------

#include "gbc_port.h"

#define GBCAM_NUM_REGS          54          // 6 control + 48 dither matrix
#define GBCAM_REG_MIRROR        0x80        // register file repeats every 0x80

#define GBCAM_SRAM_BANKS        16          // 128K
#define GBCAM_SRAM_BANKSIZE     0x2000

// Where the hardware deposits the finished picture in SRAM bank 0.
#define GBCAM_IMAGE_OFFSET      0x0100
#define GBCAM_IMAGE_TILES_X     16
#define GBCAM_IMAGE_TILES_Y     14
#define GBCAM_IMAGE_BYTES       (GBCAM_IMAGE_TILES_X * GBCAM_IMAGE_TILES_Y * 16)

// Photo slots in the camera cartridge's album. A property of the cartridge, so
// it lives here rather than in gbc_save.h - which includes this header and
// needs the number to describe where the slots are.
#define GBC_PHOTO_SLOTS         30

typedef struct
{
    unsigned char reg[GBCAM_NUM_REGS];

    int  sram_enabled;
    int  sram_bank;
    int  rom_bank;                          // 1..63, read back by the mmu's pager
    int  regs_selected;                     // a000-bfff shows registers, not SRAM

    // Non-zero while a capture is in flight. Register 0 bit 0 reads back as
    // busy until this drains, which is how the ROM's wait loop terminates.
    int  clocks_left;

    unsigned char *sram;                    // GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE

    // Sensor working buffers, heap rather than stack: the GUI task stack
    // cannot hold 128x120 of anything.
    short *retina;                          // GBCAM_SENSOR_W * GBCAM_SENSOR_H
    short *temp;

    unsigned long captures;                 // for the debug readout

    // Set whenever the ROM writes to cartridge RAM, cleared when the album is
    // committed to the card. See gbc_save.c: the photographs are only worth
    // anything if they outlive the emulator, and the player will as often
    // switch the camera off as back out cleanly.
    int dirty;

    // Raised by the mapper when the ROM signals it has finished writing, and
    // serviced later by gbc_save_service() from the module's own frame loop.
    //
    // The commit deliberately does NOT happen where it is requested. That point
    // is deep inside the emulated CPU - cpu_step, mmu_write, the mapper - and
    // doing a 128K card write there blocks the camera's GUI task for long
    // enough that DryOS shuts the body down, with the file buffers sitting at
    // maximum stack depth on a task whose stack is already the reason the
    // sensor buffers live on the heap.
    int commit_pending;
    int export_slot;                        // cursor for the staged export

    // What each slot held when it was last exported, as a cheap checksum.
    //
    // A commit re-walks all thirty slots, and without this every one of them
    // was decoded and written again on every photograph - thirty 43K files for
    // one new picture. It is also what makes the DCIM copy possible: a slot
    // whose checksum has changed is a new photograph, and a slot that has not
    // is the same one again.
    unsigned short slot_sum[GBC_PHOTO_SLOTS];

    // Where this cartridge's album lives, filled in by gbc_emu_open(). Held
    // here rather than passed down because the commit happens deep inside a
    // memory write, where the emulator's own state is not reachable.
    char save_path[128];

    // The emulator's work RAM, lent to the cartridge by gbc_emu_open().
    //
    // The ROM does not read its album index out of cartridge RAM every time it
    // wants to know how many pictures are left; it keeps a working copy in
    // WRAM and only writes it back when it saves. So trimming the index in
    // SRAM alone is invisible to the running ROM - the counter goes on down
    // until the next power-on. gbc_album_recycle() patches the working copy
    // too, which is the only reason the count changes on screen.
    unsigned char *wram;
    int            wram_bytes;
    int            wram_hits;   // copies patched by the last recycle, for the log
} gbc_cam_t;

//-------------------------------------------------------------------

int  gbc_cam_init(gbc_cam_t *c);
void gbc_cam_free(gbc_cam_t *c);

// Mapper hooks, called from the patched mmu.c.
void          gbc_cam_write(gbc_cam_t *c, unsigned short addr, unsigned char val);
unsigned char gbc_cam_read (gbc_cam_t *c, unsigned short addr);

// Advances the capture timer. Called once per emulated instruction batch.
void gbc_cam_step(gbc_cam_t *c, int cycles);

// Perform at most one unit of pending save work: the .SAV, or one exported
// photo. Returns non-zero while more remains. Call once per frame from the
// module's own loop - never from inside the emulated CPU.
int gbc_save_service(gbc_cam_t *c);

// Finish any pending work in one go. Only for shutdown, where blocking is
// acceptable because nothing is going to be drawn afterwards.
void gbc_save_flush(gbc_cam_t *c);

//-------------------------------------------------------------------
// The bridge to the real camera.
//
// Fills `dst` (GBCAM_SENSOR_W * GBCAM_SENSOR_H bytes, 0..255 luminance) from
// whatever the A480's live viewport currently holds, scaled and rotated to the
// sensor's orientation. Implemented in gbc_feed.c so that the sensor pipeline
// itself stays testable off-camera against a still image.
//
// Returns 0 on success; on failure the sensor sees mid-grey, which is more
// useful than a black frame when diagnosing.
int gbc_feed_capture(unsigned char *dst);

// Why the last gbc_feed_capture() returned what it did.
#define GBC_FEED_NEVER      0   // no capture has been attempted yet
#define GBC_FEED_OK         1
#define GBC_FEED_PLAYMODE   2   // in playback - no live sensor to read
#define GBC_FEED_NOBUF      3   // viewport accessor returned NULL
#define GBC_FEED_BADGEOM    4   // reported geometry failed its sanity check
extern int gbc_feed_status;

#endif
