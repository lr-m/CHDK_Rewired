// Checks gbc_album_recycle() against a real cartridge save.
//
// This one writes into the album index of a Game Boy Camera cartridge, which
// is the ROM's own bookkeeping - get it wrong and the ROM offers to erase every
// photograph. So it is checked against a save that came off a camera rather
// than against a made-up one, and it checks the things that would be
// destructive: that the survivors are the newest, that the mirror copy matches,
// that the "Magic" markers either side are untouched, and that a photograph
// which has not yet been written to DCIM is never freed.
//
//   gcc -O2 -o /tmp/gbc_album_selftest tools/gbc_album_selftest.c \
//       modules/gbc/gbc_save.c -I modules/gbc -I modules/gbc/core \
//       -DGBC_HOST_TEST -DGBC_CHDK_PORT -include modules/gbc/gbc_port.h
//   /tmp/gbc_album_selftest <a GBCAMERA.SAV>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gbc_save.h"

static gbc_cam_t cart;

static int used(const unsigned char *t)
{
    int i, n = 0;
    for (i = 0; i < GBC_ALBUM_LEN; i++) if (t[i] != 0xff) n++;
    return n;
}

static void show(const char *what)
{
    int i;
    printf("%-22s used=%2d  ", what, used(cart.sram + GBC_ALBUM_TAB));
    for (i = 0; i < 14; i++) printf("%02x ", cart.sram[GBC_ALBUM_TAB + i]);
    printf("...\n");
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    int freed, i, fail = 0;

    cart.sram = malloc(GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE);
    memset(cart.sram, 0, GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE);
    fread(cart.sram, 1, GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE, f);
    fclose(f);

    show("as it came off card");

    // Nothing on this card has been exported yet, so however far over the keep
    // line the album is, not one slot may be handed back.
    freed = gbc_album_recycle(&cart);
    printf("  nothing exported yet: freed %d (want 0) %s\n", freed,
           freed == 0 ? "ok" : "FAIL");
    if (freed) fail = 1;

    // Fill the album to thirty, all exported.
    for (i = 0; i < GBC_ALBUM_LEN; i++) {
        cart.sram[GBC_ALBUM_TAB + i]  = (unsigned char)i;
        cart.sram[GBC_ALBUM_TAB2 + i] = (unsigned char)i;
    }
    for (i = 0; i < GBC_PHOTO_SLOTS; i++) cart.slot_sum[i] = 0x1234;
    show("filled to thirty");

    freed = gbc_album_recycle(&cart);
    printf("  recycle freed %d (want %d) %s\n", freed,
           GBC_ALBUM_LEN - GBC_ALBUM_KEEP,
           freed == GBC_ALBUM_LEN - GBC_ALBUM_KEEP ? "ok" : "FAIL");
    if (freed != GBC_ALBUM_LEN - GBC_ALBUM_KEEP) fail = 1;
    show("after recycle");

    if (used(cart.sram + GBC_ALBUM_TAB) != GBC_ALBUM_KEEP) {
        printf("  FAIL: album should hold %d\n", GBC_ALBUM_KEEP); fail = 1; }
    if (memcmp(cart.sram + GBC_ALBUM_TAB, cart.sram + GBC_ALBUM_TAB2,
               GBC_ALBUM_LEN) != 0) {
        printf("  FAIL: the mirror does not match\n"); fail = 1; }
    for (i = 0; i < GBC_ALBUM_KEEP; i++)
        if (cart.sram[GBC_ALBUM_TAB + i] !=
            (unsigned char)(i + GBC_ALBUM_LEN - GBC_ALBUM_KEEP)) {
            printf("  FAIL: survivors are not the newest %d\n", GBC_ALBUM_KEEP);
            fail = 1; break; }
    for (i = GBC_ALBUM_KEEP; i < GBC_ALBUM_LEN; i++)
        if (cart.sram[GBC_ALBUM_TAB + i] != 0xff) {
            printf("  FAIL: tail is not 0xff\n"); fail = 1; break; }

    // The "Magic" markers must be untouched.
    if (memcmp(cart.sram + 0x11ab, "Magic", 5) ||
        memcmp(cart.sram + 0x11d0, "Magic", 5)) {
        printf("  FAIL: a Magic marker was overwritten\n"); fail = 1; }

    // A slot that has never been exported must not be freed.
    for (i = 0; i < GBC_ALBUM_LEN; i++) {
        cart.sram[GBC_ALBUM_TAB + i]  = (unsigned char)i;
        cart.sram[GBC_ALBUM_TAB2 + i] = (unsigned char)i;
    }
    cart.slot_sum[0] = 0;               // oldest never written to DCIM
    freed = gbc_album_recycle(&cart);
    printf("  oldest unexported: freed %d (want 0) %s\n", freed,
           freed == 0 ? "ok" : "FAIL");
    if (freed != 0) fail = 1;

    // The ROM's working copy in WRAM. This is what the counter on screen is
    // actually reading, and it is found by its contents rather than by a known
    // address, so the search has to hit the real copy and leave everything
    // else alone.
    for (i = 0; i < GBC_PHOTO_SLOTS; i++) cart.slot_sum[i] = 0x1234;
    cart.wram_bytes = 0x2000;
    cart.wram = malloc(cart.wram_bytes);
    memset(cart.wram, 0xa5, cart.wram_bytes);
    for (i = 0; i < GBC_ALBUM_LEN; i++) {
        cart.sram[GBC_ALBUM_TAB + i]  = (unsigned char)i;
        cart.sram[GBC_ALBUM_TAB2 + i] = (unsigned char)i;
        cart.wram[0x400 + i]          = (unsigned char)i;   // the ROM's copy
    }
    freed = gbc_album_recycle(&cart);
    printf("  wram copies patched %d (want 1) %s\n", cart.wram_hits,
           cart.wram_hits == 1 ? "ok" : "FAIL");
    if (cart.wram_hits != 1) fail = 1;
    for (i = 0; i < GBC_ALBUM_KEEP; i++)
        if (cart.wram[0x400 + i] !=
            (unsigned char)(i + GBC_ALBUM_LEN - GBC_ALBUM_KEEP)) {
            printf("  FAIL: the wram copy was not trimmed\n"); fail = 1; break; }
    for (i = 0; i < cart.wram_bytes; i++)
        if ((i < 0x400 || i >= 0x400 + GBC_ALBUM_LEN) && cart.wram[i] != 0xa5) {
            printf("  FAIL: wram clobbered at %04x\n", i); fail = 1; break; }

    // Nothing that looks like the album: the search must write nothing.
    memset(cart.wram, 0xa5, cart.wram_bytes);
    for (i = 0; i < GBC_PHOTO_SLOTS; i++) cart.slot_sum[i] = 0x1234;
    for (i = 0; i < GBC_ALBUM_LEN; i++) {
        cart.sram[GBC_ALBUM_TAB + i]  = (unsigned char)i;
        cart.sram[GBC_ALBUM_TAB2 + i] = (unsigned char)i;
    }
    gbc_album_recycle(&cart);
    printf("  no copy present: patched %d (want 0) %s\n", cart.wram_hits,
           cart.wram_hits == 0 ? "ok" : "FAIL");
    if (cart.wram_hits != 0) fail = 1;
    for (i = 0; i < cart.wram_bytes; i++)
        if (cart.wram[i] != 0xa5) {
            printf("  FAIL: wram written with no match\n"); fail = 1; break; }

    printf(fail ? "\nFAILURES\n" : "\nall checks passed\n");
    return fail;
}
