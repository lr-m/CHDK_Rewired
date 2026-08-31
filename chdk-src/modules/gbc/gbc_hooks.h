#ifndef GBC_HOOKS_H
#define GBC_HOOKS_H

//-------------------------------------------------------------------
// The seam between the vendored koenk/gbc core and this port.
//
// Two things the upstream core cannot do and that both need to reach into its
// hot paths:
//
//   paged ROM     Upstream mallocs the whole cartridge and indexes it
//                 flat. A 2 MB Crystal cannot be resident on a body whose
//                 entire CHDK image lives in 0x32000 bytes, so ROM reads are
//                 redirected through gbc_rom.c's LRU.
//
//   the camera    Cartridge type 0xfc is a mapper upstream rejects outright.
//                 Its bank register, its SRAM and its sensor registers all
//                 live behind these hooks in gbc_cam.c.
//
// Keeping both behind a header rather than inlining them into mmu.c means the
// vendored files carry a handful of one-line diffs and stay legible against
// upstream when it moves.
//-------------------------------------------------------------------

#define GBC_MBC_CAM     0xfc        // sentinel stored in gb_state.mbc

#include "gbc_rom.h"
#include "core/types.h"

// Rebuilds the read page table in mmu.c. Must be called after anything that
// changes what an address maps to.
void gbc_mmu_remap(struct gb_state *s);

// Diagnostic switch, see lcd.c. 1 = normal.
extern int gbc_render_enabled;

// ROM reads, inline.
//
// This is the single hottest path in the emulator - every instruction fetch is
// one to three of these - and it is the last one that crossed a translation
// unit. Modules build -mlong-calls, so out of line it cost a literal-pool load
// and an indirect branch per byte; inlined into the unity build's decode loop
// it is a compare and an index.
//
// The memo holds the last resolved 16K window. Consecutive reads are nearly
// always from the same bank, so the pager's LRU scan is reached only on a bank
// change. gbc_rom_bank() calls gbc_rom_memo_flush() whenever it evicts a slot,
// so the remembered pointer can never outlive the bank it refers to.
extern int            gbc_rom_memo_bank;
extern unsigned char *gbc_rom_memo_win;

static inline unsigned char gbc_hook_rom_read(struct gb_state *s, int bank,
                                              unsigned addr)
{
    gbc_rom_t *r = (gbc_rom_t *)s->gbc_pager;

    if (!r)
        return 0xff;

    if (bank != gbc_rom_memo_bank || !gbc_rom_memo_win)
    {
        gbc_rom_memo_win  = gbc_rom_bank(r, bank);
        gbc_rom_memo_bank = bank;
    }

    return gbc_rom_memo_win[addr & (GBC_ROM_BANKSIZE - 1)];
}

// Camera cartridge, a000-bfff and the mapper control ranges.
unsigned char gbc_hook_cam_read (struct gb_state *s, unsigned addr);
void          gbc_hook_cam_write(struct gb_state *s, unsigned addr, unsigned char val);

// Which ROM bank the camera mapper currently has in the 4000-7fff window.
int           gbc_hook_cam_rom_bank(struct gb_state *s);

#endif
