#ifndef GBC_ROM_H
#define GBC_ROM_H

//-------------------------------------------------------------------
// Demand-paged cartridge ROM.
//
// Neither ROM this was written for can be resident. The Game Boy Camera is
// 1 MB (64 banks) and Pokemon Crystal is 2 MB (128 banks), against a CHDK
// binary that lives in 0x32000 bytes of ARAM on the A480. So the ROM stays on
// the card and only a handful of 16K banks are held in RAM at a time.
//
// Bank 0 is pinned, because the emulated CPU reads 0000-3fff constantly - the
// interrupt vectors, the header, and on this cartridge most of the hot code.
// The rest live in an LRU set of GBC_ROM_CACHE_BANKS slots.
//
// The cost is an SD read on a cache miss, which is why the cache is sized to
// hold a working set rather than a single bank: MBC3 bank-switches inside
// loops and a one-slot cache would thrash on every other instruction.
//-------------------------------------------------------------------

#include "gbc_port.h"

#define GBC_ROM_BANKSIZE      0x4000        // 16K, fixed by the hardware

// Four paged slots plus the pinned bank 0 = 80K resident.
//
// Briefly raised to eight on the theory that a 16K SD read per miss would
// dominate. Measurement said otherwise - misses ran at about 55 for a whole
// session, so the working set already fits in four and the extra 64K bought
// nothing. Back to four, because the camera cartridge needs the memory far
// more: it also allocates 128K of cartridge SRAM and 60K of sensor buffers,
// and total footprint is a likelier reason for a body to fall over than pager
// thrash ever was.
//
// The first dial to turn if gbc_emu_open() starts failing.
#define GBC_ROM_CACHE_BANKS   4

typedef struct
{
    int  fd;
    int  num_banks;
    long size;

    unsigned char *bank0;                       // pinned, always valid
    unsigned char *slot[GBC_ROM_CACHE_BANKS];
    int  slot_bank[GBC_ROM_CACHE_BANKS];        // which bank is in the slot, -1 empty
    unsigned slot_used[GBC_ROM_CACHE_BANKS];    // LRU stamp
    unsigned clock;

    unsigned long hits, misses;                 // for the debug readout
} gbc_rom_t;

// Drops the one-entry read memo in gbc_emu.c. Called by the pager whenever it
// evicts a slot, so the memo cannot outlive the bank it points at.
void gbc_rom_memo_flush(void);

// Opens the ROM and pins bank 0. Returns 0 on success.
int  gbc_rom_open(gbc_rom_t *r, const char *path);
void gbc_rom_close(gbc_rom_t *r);

// Returns a pointer to the 16K window for `bank`, paging it in if needed.
// Never returns NULL once open() has succeeded - on a read error the slot is
// zero-filled, which the emulated CPU sees as a ROM full of NOPs rather than
// a crash.
unsigned char *gbc_rom_bank(gbc_rom_t *r, int bank);

// Single byte at a flat ROM offset. Used by the header parser, which reads
// scattered bytes once and does not deserve a whole bank fetch each time.
unsigned char gbc_rom_byte(gbc_rom_t *r, long offset);

#endif
