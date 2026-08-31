#ifndef GBC_SAVE_H
#define GBC_SAVE_H

//-------------------------------------------------------------------
// Cartridge RAM persistence, and photo export.
//
// The camera cartridge's 128K of SRAM is where the ROM keeps its album. Until
// this file existed it was a malloc'd buffer that gbc_cam_free() handed back to
// the heap, so every photograph was lost the moment the player left the
// emulator - despite a comment in core/state.c cheerfully claiming the cart was
// "battery-backed, so the photographs survive a power cycle". They did not.
//
// Two artifacts are written, deliberately:
//
//   .SAV    The raw 128K of cartridge RAM, byte for byte. This is the same
//           format every Game Boy emulator uses for battery-backed saves, so
//           the album can be opened by any of the existing Game Boy Camera
//           extraction tools, injected back into a real cartridge, or moved to
//           another emulator. It is the authoritative artifact - the photos
//           live here, and the .BMPs below are a convenience.
//
//   .BMP    Each stored photo decoded to an image, in the Game Boy's own four
//           tones, so they can be looked at without any extra software.
//
// Album layout, from the format the extraction-tool community documents:
// thirty photo slots, slot N at SRAM offset 0x2000 + N * 0x1000, each holding
// 0xe00 bytes of standard 2bpp tile data - 16x14 tiles, tiles in reading order.
// That is exactly the geometry and packing gbc_cam.c already produces for the
// live capture, so the same decoder serves both.
//-------------------------------------------------------------------

#include "gbc_cam.h"

#define GBC_PHOTO_SLOT_BASE     0x2000      // first slot, offset into SRAM
#define GBC_PHOTO_SLOT_STRIDE   0x1000      // per slot, image + thumbnail

// Build the .SAV path for a ROM: "A/CHDK/GBC/GBCAMERA.GB" becomes
// "A/CHDK/GBC/SAVE/GBCAMERA.SAV". `out` needs 128 bytes.
void gbc_save_path(const char *rom_path, char *out);

// Load the album from the card into c->sram. Missing or short file is not an
// error - a first run simply starts with an empty cartridge.
void gbc_save_load(gbc_cam_t *c, const char *rom_path);

// Write the album back. Returns 0 on success.
int  gbc_save_store(gbc_cam_t *c, const char *rom_path);

//-------------------------------------------------------------------
// Ordinary cartridges.
//
// Everything above is the camera cartridge, whose SRAM is an album. A normal
// game's battery-backed RAM is the same idea with none of the structure: a
// blob of 8K to 128K that the cartridge kept alive with a coin cell, and that
// every emulator on earth stores as a .SAV of exactly those bytes. Crystal's
// save file written here opens in any of them, and one from any of them opens
// here.
//
// The core raises flush_extram / extram_dirty for us (core/mmu.c) and the
// module acts on them in gbc_emu_frame(), for the same reason the camera's
// album write happens there: a card write from inside an emulated memory
// access blocked the GUI task long enough for DryOS to switch the body off.

// Both return 0 on success. A missing file is not an error on read - a game
// that has never been saved starts with the RAM the cartridge powered up with.
int gbc_save_ram_read(const char *save_path, unsigned char *ram, int len);
int gbc_save_ram_write(const char *save_path, const unsigned char *ram, int len);

//-------------------------------------------------------------------
// The album index, and why the thirty-photo counter can be made to stop
// counting down.
//
// Bank 0 of the cartridge holds the album as a list rather than as a count:
// "Magic" markers, each followed by two bytes and a thirty-byte table whose
// entries are slot numbers in album order, 0xff for the unused tail. The
// ROM's "photos left" is thirty minus the entries in it. Read straight off a
// real cartridge save:
//
//   10d2  Magic Xr   fe fe fe ...            (not the album)
//   11ab  Magic Xr   00 01 .. 0a ff ff ...   the album
//   11d0  Magic S.   00 01 .. 0a ff ff ...   its mirror
//   11f5  Magic S.   00 00 00 ...            (not the album)
//
// Eleven entries, and the camera said nineteen left.
//
// The two bytes after each marker are part of the marker and not a checksum
// over the table: two blocks with identical tables carry different bytes, and
// two blocks with different tables carry the same ones. So the table can be
// rewritten on its own.
//
// Taking the oldest entries out of it - after their pictures are safely in
// DCIM - hands their slots back to the ROM, and the album becomes a rolling
// window over the most recent photographs instead of a hard limit.
#define GBC_ALBUM_TAB    0x11b2     // the table, past "Magic" + 2
#define GBC_ALBUM_TAB2   0x11d7     // its mirror
#define GBC_ALBUM_LEN    30
// Small on purpose. The ROM's "pictures remaining" is 30 minus the length of
// this table, so whatever is left here is what the counter counts down from.
// Keeping four means the counter sits at 26 and never walks towards zero,
// while the album still holds the last few shots for reviewing on the body.
// Everything older is already a file in DCIM/GBC.
#define GBC_ALBUM_KEEP   4          // photographs left in the album afterwards

// Free the oldest exported photos if the album has more than GBC_ALBUM_KEEP in
// it. Returns how many slots were handed back. Only ever frees a slot whose
// picture this session has already written to DCIM.
int gbc_album_recycle(gbc_cam_t *c);

// Which of the six screen tints the exported BMPs are coloured with. Called
// from gbc_set_hue() so the file matches what was on the screen.
void gbc_photo_set_hue(int h);

// Decode one slot to A/CHDK/GBC/PHOTOS. Returns 1 if a file was written.
// Blank slots and card errors return 0.
int  gbc_save_export_slot(gbc_cam_t *c, int slot);

//-------------------------------------------------------------------
// Exposed for the host test.

// Decode one slot's 2bpp tile data to GBCAM_W * GBCAM_H shade indices, 0
// lightest to 3 darkest, one byte per pixel.
void gbc_photo_decode(const unsigned char *tiles, unsigned char *out);

// Build a 24-bit BMP of a decoded photo into `buf`, which must hold
// GBC_BMP_BYTES. Returns the length used.
#define GBC_BMP_HEADER  54
#define GBC_BMP_BYTES   (GBC_BMP_HEADER + GBCAM_W * GBCAM_H * 3)
int  gbc_photo_to_bmp(const unsigned char *shades, unsigned char *buf);

#endif
