//-------------------------------------------------------------------
// Host test for the photo decoder and BMP writer in gbc_save.c.
//
// The camera writes these files once, at the moment the player leaves the
// emulator, onto a card that then goes into a card reader. There is no way to
// notice a wrong header or a mirrored image from inside the camera, and a
// corrupt photo is not recoverable - the album it came from has already been
// freed. So the format is checked here instead.
//
// Build and run:
//   gcc -O2 -o /tmp/gbc_save_selftest tools/gbc_save_selftest.c
//       modules/gbc/gbc_save.c -I modules/gbc -I modules/gbc/core
//       -DGBC_HOST_TEST -DGBC_CHDK_PORT -include modules/gbc/gbc_port.h
//   /tmp/gbc_save_selftest
//-------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbc_save.h"

char gbc_panic_msg[64];
int  gbc_panicked = 0;
void gbc_panic(const char *what) { (void)what; gbc_panicked = 1; }
int  gbc_feed_status = 0;
int  gbc_feed_capture(unsigned char *d) { (void)d; return -1; }

static int failures = 0;
static void check(int cond, const char *what)
{
    printf("  %-58s %s\n", what, cond ? "ok" : "FAIL");
    if (!cond) failures++;
}

static unsigned rd32(const unsigned char *p)
{ return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }
static unsigned rd16(const unsigned char *p)
{ return p[0] | (p[1] << 8); }

int main(void)
{
    static unsigned char tiles[GBCAM_IMAGE_BYTES];
    static unsigned char shades[GBCAM_W * GBCAM_H];
    static unsigned char bmp[GBC_BMP_BYTES];
    int len, i, x, y;

    printf("gbc_save_selftest\n\n");

    //---------------------------------------------------------------
    printf("geometry\n");
    check(GBCAM_W == 128 && GBCAM_H == 112, "photo is 128x112, as the hardware makes it");
    check(GBCAM_IMAGE_BYTES == 3584,        "3584 bytes of 2bpp tile data per photo");
    check((GBCAM_W * 3) % 4 == 0,           "BMP rows need no padding at this width");
    check(GBC_PHOTO_SLOT_BASE + 29 * GBC_PHOTO_SLOT_STRIDE + GBCAM_IMAGE_BYTES
              <= GBCAM_SRAM_BANKS * GBCAM_SRAM_BANKSIZE,
          "all 30 slots fit inside the 128K cartridge");

    //---------------------------------------------------------------
    // A known tile: top row of tile 0 set to shade pattern 0,1,2,3,0,1,2,3.
    // low plane  = 0 1 0 1 0 1 0 1 -> 0x55
    // high plane = 0 0 1 1 0 0 1 1 -> 0x33
    printf("\n2bpp decode\n");
    memset(tiles, 0, sizeof(tiles));
    tiles[0] = 0x55;
    tiles[1] = 0x33;
    gbc_photo_decode(tiles, shades);

    {
        static const unsigned char want[8] = { 0, 1, 2, 3, 0, 1, 2, 3 };
        int ok = 1;
        for (i = 0; i < 8; i++)
            if (shades[i] != want[i]) ok = 0;
        check(ok, "planes combine low|high<<1, leftmost pixel in bit 7");
    }

    // Tile (tx=1, ty=0) must land at x=8..15, and tile (0,1) at y=8.
    memset(tiles, 0, sizeof(tiles));
    tiles[1 * 16 + 0] = 0xff;                       /* tile 1, row 0, low plane */
    tiles[GBCAM_IMAGE_TILES_X * 16 + 0] = 0xff;     /* tile below tile 0        */
    gbc_photo_decode(tiles, shades);
    check(shades[8] == 1 && shades[7] == 0,
          "tiles run left to right across the row");
    check(shades[8 * GBCAM_W] == 1,
          "the next tile row starts 8 pixels down");

    //---------------------------------------------------------------
    printf("\nBMP container\n");
    for (i = 0; i < GBCAM_W * GBCAM_H; i++)
        shades[i] = (unsigned char)(i & 3);
    len = gbc_photo_to_bmp(shades, bmp);

    check(len == GBC_BMP_BYTES,             "writes exactly GBC_BMP_BYTES");
    check(bmp[0] == 'B' && bmp[1] == 'M',   "starts with the BM signature");
    check(rd32(bmp + 2) == (unsigned)len,   "file size field matches the real size");
    check(rd32(bmp + 10) == GBC_BMP_HEADER, "pixel data offset is past the header");
    check(rd32(bmp + 14) == 40,             "40-byte BITMAPINFOHEADER");
    check(rd32(bmp + 18) == GBCAM_W,        "width is 128");
    check(rd32(bmp + 22) == GBCAM_H,        "height is 112");
    check(rd16(bmp + 26) == 1,              "one colour plane");
    check(rd16(bmp + 28) == 24,             "24 bits per pixel");
    check(rd32(bmp + 30) == 0,              "uncompressed (BI_RGB)");
    check(rd32(bmp + 34) == (unsigned)(GBCAM_W * GBCAM_H * 3), "image size correct");

    //---------------------------------------------------------------
    // Positive height means bottom-up, so the FIRST row in the file is the
    // LAST row of the picture. Getting this backwards flips every photo.
    printf("\norientation and palette\n");
    memset(shades, 0, sizeof(shades));
    for (x = 0; x < GBCAM_W; x++)
        shades[(GBCAM_H - 1) * GBCAM_W + x] = 3;    /* darkest bottom row */
    gbc_photo_to_bmp(shades, bmp);
    check(bmp[GBC_BMP_HEADER + 0] == 0x0f &&
          bmp[GBC_BMP_HEADER + 1] == 0x38 &&
          bmp[GBC_BMP_HEADER + 2] == 0x0f,
          "bottom-up: first stored row is the picture's bottom row");

    memset(shades, 0, sizeof(shades));
    gbc_photo_to_bmp(shades, bmp);
    check(bmp[GBC_BMP_HEADER + 0] == 0x0f &&
          bmp[GBC_BMP_HEADER + 1] == 0xbc &&
          bmp[GBC_BMP_HEADER + 2] == 0x9b,
          "shade 0 is the lightest Game Boy green, stored B,G,R");

    // Every shade must map to a distinct colour, or the photo loses tones.
    {
        int distinct = 1;
        for (i = 0; i < 4; i++)
            for (int j = i + 1; j < 4; j++)
            {
                memset(shades, (unsigned char)i, sizeof(shades));
                gbc_photo_to_bmp(shades, bmp);
                unsigned a = (bmp[GBC_BMP_HEADER] << 16) |
                             (bmp[GBC_BMP_HEADER + 1] << 8) | bmp[GBC_BMP_HEADER + 2];
                memset(shades, (unsigned char)j, sizeof(shades));
                gbc_photo_to_bmp(shades, bmp);
                unsigned b = (bmp[GBC_BMP_HEADER] << 16) |
                             (bmp[GBC_BMP_HEADER + 1] << 8) | bmp[GBC_BMP_HEADER + 2];
                if (a == b) distinct = 0;
            }
        check(distinct, "all four tones are distinct colours");
    }

    //---------------------------------------------------------------
    // A viewable sample, so the result can be eyeballed rather than trusted.
    {
        FILE *f;
        for (y = 0; y < GBCAM_H; y++)
            for (x = 0; x < GBCAM_W; x++)
                shades[y * GBCAM_W + x] =
                    (unsigned char)(((x >> 4) + (y >> 4)) & 3);
        len = gbc_photo_to_bmp(shades, bmp);
        f = fopen("/tmp/gbc_photo_sample.bmp", "wb");
        if (f) { fwrite(bmp, 1, len, f); fclose(f);
                 printf("\nwrote /tmp/gbc_photo_sample.bmp\n"); }
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "passed",
           failures, failures == 1 ? "" : "s");
    return failures != 0;
}
