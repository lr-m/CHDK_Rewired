//-------------------------------------------------------------------
// Demand-paged cartridge ROM - see gbc_rom.h.
//-------------------------------------------------------------------

#include "gbc_rom.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

//-------------------------------------------------------------------

static int gbc_rom_fetch(gbc_rom_t *r, unsigned char *dst, int bank)
{
    long off = (long)bank * GBC_ROM_BANKSIZE;

    if (lseek(r->fd, off, SEEK_SET) != off)
        goto fail;
    if (read(r->fd, dst, GBC_ROM_BANKSIZE) != GBC_ROM_BANKSIZE)
        goto fail;
    return 0;

fail:
    // A short read means a truncated or unreadable card. Zeroed ROM reads back
    // as NOP/NOP forever, which stalls the emulated CPU instead of faulting
    // the camera - the player can back out with MENU.
    memset(dst, 0, GBC_ROM_BANKSIZE);
    return -1;
}

//-------------------------------------------------------------------

int gbc_rom_open(gbc_rom_t *r, const char *path)
{
    int i;

    memset(r, 0, sizeof(*r));
    r->fd = -1;

    for (i = 0; i < GBC_ROM_CACHE_BANKS; i++)
        r->slot_bank[i] = -1;

    r->fd = open(path, O_RDONLY, 0777);
    if (r->fd < 0)
        return -1;

    r->size = lseek(r->fd, 0, 2 /* SEEK_END */);
    if (r->size < GBC_ROM_BANKSIZE * 2)
        goto fail;                              // smaller than the smallest cart

    r->num_banks = (int)(r->size / GBC_ROM_BANKSIZE);

    r->bank0 = (unsigned char *)malloc(GBC_ROM_BANKSIZE);
    if (!r->bank0)
        goto fail;
    if (gbc_rom_fetch(r, r->bank0, 0))
        goto fail;

    for (i = 0; i < GBC_ROM_CACHE_BANKS; i++)
    {
        r->slot[i] = (unsigned char *)malloc(GBC_ROM_BANKSIZE);
        if (!r->slot[i])
            goto fail;                          // partial alloc, close() frees
    }

    return 0;

fail:
    gbc_rom_close(r);
    return -1;
}

//-------------------------------------------------------------------

void gbc_rom_close(gbc_rom_t *r)
{
    int i;

    if (r->bank0) { free(r->bank0); r->bank0 = 0; }

    for (i = 0; i < GBC_ROM_CACHE_BANKS; i++)
        if (r->slot[i]) { free(r->slot[i]); r->slot[i] = 0; }

    if (r->fd >= 0) { close(r->fd); r->fd = -1; }
}

//-------------------------------------------------------------------

unsigned char *gbc_rom_bank(gbc_rom_t *r, int bank)
{
    int i, victim;
    unsigned oldest;

    // Masking rather than clamping: this is what the real MBCs do with an
    // out-of-range bank number, and some cartridges rely on the wrap.
    if (r->num_banks > 0)
        bank &= (r->num_banks - 1);

    if (bank == 0)
        return r->bank0;

    for (i = 0; i < GBC_ROM_CACHE_BANKS; i++)
    {
        if (r->slot_bank[i] == bank)
        {
            r->slot_used[i] = ++r->clock;
            r->hits++;
            return r->slot[i];
        }
    }

    // Miss. Take the least recently used slot - an empty one first, since its
    // stamp is 0 and nothing else can be lower.
    victim = 0;
    oldest = r->slot_used[0];
    for (i = 1; i < GBC_ROM_CACHE_BANKS; i++)
    {
        if (r->slot_used[i] < oldest)
        {
            oldest = r->slot_used[i];
            victim = i;
        }
    }

    // The caller's one-entry memo may be holding a pointer into the slot we
    // are about to overwrite. Drop it before the contents change, or a bank
    // switch away and back reads the wrong cartridge data with no symptom
    // beyond the game quietly misbehaving.
    gbc_rom_memo_flush();

    gbc_rom_fetch(r, r->slot[victim], bank);
    r->slot_bank[victim] = bank;
    r->slot_used[victim] = ++r->clock;
    r->misses++;

    return r->slot[victim];
}

//-------------------------------------------------------------------

unsigned char gbc_rom_byte(gbc_rom_t *r, long offset)
{
    int bank = (int)(offset / GBC_ROM_BANKSIZE);
    int off  = (int)(offset % GBC_ROM_BANKSIZE);

    return gbc_rom_bank(r, bank)[off];
}
