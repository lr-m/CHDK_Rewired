//-------------------------------------------------------------------
// Host test for the read page table in core/mmu.c.
//
// gbc_mmu_remap() caches a host pointer for every 256-byte page of the Game
// Boy's address space, and mmu_read() trusts it. If an entry is wrong or stale
// the emulator reads the wrong memory and keeps running - no fault, no
// assertion, just a game that misbehaves in a way that looks like a CPU bug.
// That is a bad failure mode to ship on trust, so this checks the mapped path
// against an independently computed expectation for all 65536 addresses,
// across bank configurations.
//
// Build and run:
//   gcc -O2 -o /tmp/gbc_mmu_selftest tools/gbc_mmu_selftest.c
//       modules/gbc/gbc_rom.c -I modules/gbc -I modules/gbc/core
//       -DGBC_HOST_TEST -DGBC_CHDK_PORT -include modules/gbc/gbc_port.h
//   /tmp/gbc_mmu_selftest
//-------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gbc_rom.h"
#include "core/types.h"
#include "core/hwdefs.h"
#include "gbc_hooks.h"

//-------------------------------------------------------------------
// Stubs for the parts of the port this test does not exercise.

char gbc_panic_msg[64];
int  gbc_panicked = 0;
int  gbc_render_enabled = 1;
void gbc_panic(const char *what) { (void)what; gbc_panicked = 1; }

int  gbc_rom_memo_bank = -1;
unsigned char *gbc_rom_memo_win = 0;
void gbc_rom_memo_flush(void) { gbc_rom_memo_bank = -1; gbc_rom_memo_win = 0; }

unsigned char gbc_hook_cam_read(struct gb_state *s, unsigned a) { (void)s; (void)a; return 0xff; }
void gbc_hook_cam_write(struct gb_state *s, unsigned a, unsigned char v) { (void)s; (void)a; (void)v; }
int  gbc_hook_cam_rom_bank(struct gb_state *s) { (void)s; return 1; }
int  dbg_run_debugger(struct gb_state *s) { (void)s; return 0; }
void dbg_print_regs(struct gb_state *s) { (void)s; }

// mmu.c wants these from lcd.c/cpu.c; the test never reaches them.
void lcd_step(struct gb_state *s) { (void)s; }
void cpu_timers_step(struct gb_state *s) { (void)s; }

#include "core/mmu.c"

//-------------------------------------------------------------------

static int failures = 0;

static void check(int cond, const char *what)
{
    printf("  %-56s %s\n", what, cond ? "ok" : "FAIL");
    if (!cond) failures++;
}

//-------------------------------------------------------------------
// A synthetic 8-bank ROM on disk, each bank filled with a recognisable
// pattern so a wrong mapping is unambiguous rather than plausible.

#define NBANKS 8

static const char *make_rom(void)
{
    static char path[] = "/tmp/gbc_mmu_test_rom.gb";
    unsigned char *buf = malloc(NBANKS * GBC_ROM_BANKSIZE);
    int b, i;

    for (b = 0; b < NBANKS; b++)
        for (i = 0; i < GBC_ROM_BANKSIZE; i++)
            buf[b * GBC_ROM_BANKSIZE + i] = (unsigned char)(b * 17 + (i & 0x3f));

    FILE *f = fopen(path, "wb");
    fwrite(buf, 1, NBANKS * GBC_ROM_BANKSIZE, f);
    fclose(f);
    free(buf);
    return path;
}

static unsigned char rom_expect(int bank, int off)
{
    return (unsigned char)(bank * 17 + (off & 0x3f));
}

//-------------------------------------------------------------------

int main(void)
{
    struct gb_state st;
    gbc_rom_t pager;
    unsigned addr;
    int errors, bank;

    printf("gbc_mmu_selftest\n\n");

    memset(&st, 0, sizeof(st));
    st.emu_state = calloc(1, sizeof(struct emu_state));

    if (gbc_rom_open(&pager, make_rom()))
    {
        printf("could not open synthetic ROM\n");
        return 1;
    }

    st.gbc_pager = &pager;
    st.mem_num_banks_rom  = NBANKS;
    st.mem_num_banks_wram = 8;
    st.mem_num_banks_vram = 2;
    st.mbc = 1;
    st.mem_bank_rom = 1;
    st.mem_WRAM = malloc(WRAM_BANKSIZE * 8);
    st.mem_VRAM = malloc(VRAM_BANKSIZE * 2);

    // Distinct patterns again - WRAM and VRAM must not be confusable.
    for (addr = 0; addr < WRAM_BANKSIZE * 8; addr++)
        st.mem_WRAM[addr] = (unsigned char)(0x40 + (addr & 0x1f));
    for (addr = 0; addr < VRAM_BANKSIZE * 2; addr++)
        st.mem_VRAM[addr] = (unsigned char)(0x80 + (addr & 0x1f));

    //---------------------------------------------------------------
    printf("page table covers only side-effect-free regions\n");

    gbc_mmu_remap(&st);

    {
        int mapped_io = 0, mapped_extram = 0, unmapped_rom = 0;
        for (addr = 0xa0; addr < 0xc0; addr++) if (st.rdmap[addr]) mapped_extram++;
        for (addr = 0xe0; addr < 0x100; addr++) if (st.rdmap[addr]) mapped_io++;
        for (addr = 0x00; addr < 0x80; addr++) if (!st.rdmap[addr]) unmapped_rom++;
        check(mapped_extram == 0, "a000-bfff left to the slow path");
        check(mapped_io == 0,     "e000-ffff left to the slow path");
        check(unmapped_rom == 0,  "0000-7fff fully mapped");
    }

    //---------------------------------------------------------------
    printf("\nmapped reads match the ROM contents, every bank\n");

    errors = 0;
    for (bank = 1; bank < NBANKS; bank++)
    {
        st.mem_bank_rom = bank;
        gbc_mmu_remap(&st);

        for (addr = 0x0000; addr < 0x4000; addr++)
            if (mmu_read(&st, (u16)addr) != rom_expect(0, addr))
                errors++;
        for (addr = 0x4000; addr < 0x8000; addr++)
            if (mmu_read(&st, (u16)addr) != rom_expect(bank, addr - 0x4000))
                errors++;
    }
    check(errors == 0, "ROM reads correct across all banks");
    if (errors) printf("    %d mismatches\n", errors);

    //---------------------------------------------------------------
    printf("\nbank switch invalidates the map\n");

    st.mem_bank_rom = 1;
    gbc_mmu_remap(&st);
    unsigned char before = mmu_read(&st, 0x4000);
    mmu_write(&st, 0x2000, 5);          // MBC1 ROM bank select -> 5
    unsigned char after = mmu_read(&st, 0x4000);
    check(before == rom_expect(1, 0), "bank 1 visible before the switch");
    check(after  == rom_expect(5, 0), "bank 5 visible immediately after");
    check(before != after,             "the map did not go stale");

    //---------------------------------------------------------------
    printf("\nWRAM and VRAM\n");

    errors = 0;
    for (addr = 0xc000; addr < 0xd000; addr++)
        if (mmu_read(&st, (u16)addr) != st.mem_WRAM[addr - 0xc000]) errors++;
    check(errors == 0, "c000-cfff maps to WRAM bank 0");

    st.mem_bank_wram = 3;
    gbc_mmu_remap(&st);
    errors = 0;
    for (addr = 0xd000; addr < 0xe000; addr++)
        if (mmu_read(&st, (u16)addr) !=
            st.mem_WRAM[3 * WRAM_BANKSIZE + (addr - 0xd000)]) errors++;
    check(errors == 0, "d000-dfff follows the WRAM bank register");

    st.mem_bank_vram = 1;
    gbc_mmu_remap(&st);
    errors = 0;
    for (addr = 0x8000; addr < 0xa000; addr++)
        if (mmu_read(&st, (u16)addr) !=
            st.mem_VRAM[VRAM_BANKSIZE + (addr - 0x8000)]) errors++;
    check(errors == 0, "8000-9fff follows the VRAM bank register");

    //---------------------------------------------------------------
    printf("\nBIOS overlay disables the map\n");

    st.in_bios = 1;
    gbc_mmu_remap(&st);
    {
        int mapped = 0;
        for (addr = 0; addr < 256; addr++) if (st.rdmap[addr]) mapped++;
        check(mapped == 0, "no pages mapped while in_bios is set");
    }
    st.in_bios = 0;
    gbc_mmu_remap(&st);

    gbc_rom_close(&pager);
    unlink("/tmp/gbc_mmu_test_rom.gb");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "passed",
           failures, failures == 1 ? "" : "s");
    return failures != 0;
}
