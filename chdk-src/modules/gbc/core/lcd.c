/* CHDK port: see gbc_port.h */
/* CHDK port: see gbc_port.h */

#include "lcd.h"
#include "hwdefs.h"

static void lcd_render_current_line(struct gb_state *gb_state);

int lcd_init(struct gb_state *s) {
    s->emu_state->lcd_pixbuf =
        malloc(GB_LCD_WIDTH * GB_LCD_HEIGHT * sizeof(u16));
    if (!s->emu_state->lcd_pixbuf)
        return 1;
    memset(s->emu_state->lcd_pixbuf, 0,
            GB_LCD_WIDTH * GB_LCD_HEIGHT * sizeof(u16));
    return 0;
}

void lcd_step(struct gb_state *s) {
    /* The LCD goes through several states.
     * 0 = H-Blank, 1 = V-Blank, 2 = reading OAM, 3 = line render
     * For the first 144 (visible) lines the hardware first reads the OAM
     * (sprite data), then goes through each pixel on the line, and finally
     * H-Blanks. For the last few lines the screen is in V-Blank, where VRAM can
     * be freely accessed and we don't have the other 3 modes per line.
     * So the cycle goes like: 2330002330002330002330001111..1111233000...
     *                         OBBHHHOBBHHHOBBHHHOBBHHHVVVV..VVVVOBBHHH...
     * The entire cycle takes 70224 clks. (so that's about 60FPS)
     * H-Blank takes about 201-207 cycles. VBlank 4560 clks.
     * OAM reading takes about 77-83 and line rendering about 169-175 clks.
     */

    s->emu_state->lcd_entered_hblank = 0;
    s->emu_state->lcd_entered_vblank = 0;

    s->io_lcd_mode_cycles_left -= s->emu_state->last_op_cycles;

    if (s->io_lcd_mode_cycles_left < 0) {
        switch (s->io_lcd_STAT & 3) {
        case 0: /* H-Blank */
            if (s->io_lcd_LY == 143) { /* Go into V-Blank (1) */
                s->io_lcd_STAT = (s->io_lcd_STAT & 0xfc) | 1;
                s->io_lcd_mode_cycles_left = GB_LCD_MODE_1_CLKS;
                s->interrupts_request |= 1 << 0;
                s->emu_state->lcd_entered_vblank = 1;
            } else { /* Back into OAM (2) */
                s->io_lcd_STAT = (s->io_lcd_STAT & 0xfc) | 2;
                s->io_lcd_mode_cycles_left = GB_LCD_MODE_2_CLKS;
            }
            s->io_lcd_LY = (s->io_lcd_LY + 1) % (GB_LCD_LY_MAX + 1);
            s->io_lcd_STAT = (s->io_lcd_STAT & 0xfb) | (s->io_lcd_LY == s->io_lcd_LYC);

            /* We incremented line, check LY=LYC and set interrupt if needed. */
            if (s->io_lcd_STAT & (1 << 6) && s->io_lcd_LY == s->io_lcd_LYC)
                s->interrupts_request |= 1 << 1;
            break;
        case 1: /* VBlank, Back to OAM (2) */
            s->io_lcd_STAT = (s->io_lcd_STAT & 0xfc) | 2;
            s->io_lcd_mode_cycles_left = GB_LCD_MODE_2_CLKS;
            break;
        case 2: /* OAM, onto line drawing (OAM+VRAM busy) (3) */
            s->io_lcd_STAT = (s->io_lcd_STAT & 0xfc) | 3;
            s->io_lcd_mode_cycles_left = GB_LCD_MODE_3_CLKS;
            break;
        case 3: /* Line render (OAM+VRAM), let's H-Blank (0) */
            s->io_lcd_STAT = (s->io_lcd_STAT & 0xfc) | 0;
            s->io_lcd_mode_cycles_left = GB_LCD_MODE_0_CLKS;
            s->emu_state->lcd_entered_hblank = 1;
            break;
        }

        /* We switched mode, trigger interrupt if requested. */
        u8 newmode = s->io_lcd_STAT & 3;
        if (s->io_lcd_STAT & (1 << 5) && newmode == 2) /* OAM (2) int */
            s->interrupts_request |= 1 << 1;
        if (s->io_lcd_STAT & (1 << 4) && newmode == 1) /* V-Blank (1) int */
            s->interrupts_request |= 1 << 1;
        if (s->io_lcd_STAT & (1 << 3) && newmode == 0) /* H-Blank (0) int */
            s->interrupts_request |= 1 << 1;
    }

    /* CHDK port: gbc_render_enabled is a diagnostic switch. Turning the
     * rasteriser off leaves the emulated CPU running exactly as before, so
     * the difference in frame time is what lcd_render_current_line() costs -
     * which decides whether speed work belongs in the interpreter or in the
     * renderer. */
    if (s->emu_state->lcd_entered_hblank && gbc_render_enabled)
        lcd_render_current_line(s);
}


struct __attribute__((__packed__)) OAMentry {
    u8 y;
    u8 x;
    u8 tile;
    u8 flags;
};

u16 palette_get_col(u8 *palettedata, u8 palidx, u8 colidx) {
    u8 idx = palidx * 8 + colidx * 2;
    return palettedata[idx] | (palettedata[idx + 1] << 8);
}

u8 palette_get_gray(u8 palette, u8 colidx) {
    return (palette >> (colidx << 1)) & 0x3;
}

static void lcd_render_current_line(struct gb_state *gb_state) {
    /*
     * Tile Data @ 8000-8FFF or 8800-97FF defines the pixels per Tile, which can
     * be used for the BG, window or sprite/object. 192 tiles max, 8x8px, 4
     * colors. Foreground tiles (sprites/objects) may only have 3 colors (0
     * being transparent). Each tile thus is 16 byte, 2 byte per line (2 bit per
     * px), first byte is lsb of color, second byte msb of color.
     *
     *
     * BG Map @ 9800-9BFF or 9C00-9FFF. 32 rows of 32 bytes each, each byte is
     * number of tile to be displayed (index into Tile Data, see below
     *
     * Window works similarly to BG
     *
     * Sprites or objects come from the Sprite Attribute table (OAM: Object
     * Attribute Memory) @ FE00-FE9F, 40 entries of 4 byte (max 10 per hline).
     *  byte 0: Y pos - 16
     *  byte 1: X pos - 8
     *  byte 2: Tile number, index into Tile data (see above)
     *
     */

    int y = gb_state->io_lcd_LY;
    u16 *pixbuf = gb_state->emu_state->lcd_pixbuf;

    if (y >= GB_LCD_HEIGHT) /* VBlank */
        return;

    /* CHDK port: the row being drawn, hoisted out of every x + y*W in this
     * function. */
    u16 *pixline = &pixbuf[y * GB_LCD_WIDTH];

    u8 use_col = gb_state->gb_type == GB_TYPE_CGB;

    u8 winmap_high       = (gb_state->io_lcd_LCDC & (1<<6)) ? 1 : 0;
    u8 win_enable        = (gb_state->io_lcd_LCDC & (1<<5)) ? 1 : 0;
    u8 bgwin_tilemap_low = (gb_state->io_lcd_LCDC & (1<<4)) ? 1 : 0;
    u8 bgmap_high        = (gb_state->io_lcd_LCDC & (1<<3)) ? 1 : 0;
    u8 obj_8x16          = (gb_state->io_lcd_LCDC & (1<<2)) ? 1 : 0;
    u8 obj_enable        = (gb_state->io_lcd_LCDC & (1<<1)) ? 1 : 0;
    u8 bg_enable         = (gb_state->io_lcd_LCDC & (1<<0)) ? 1 : 0;
    u8 bgwin_tilemap_unsigned = bgwin_tilemap_low;

    if (use_col)
        bg_enable = 1;

    u16 bgwin_tilemap_addr = bgwin_tilemap_low ? 0x8000 : 0x9000;
    u16 bgmap_addr = bgmap_high ? 0x9c00 : 0x9800;
    u16 winmap_addr = winmap_high ? 0x9c00 : 0x9800;
    u16 obj_tiledata_addr = 0x8000;
    u16 vram_addr = 0x8000;

    u8 *bgwin_tiledata = &gb_state->mem_VRAM[bgwin_tilemap_addr - vram_addr];
    u8 *obj_tiledata = &gb_state->mem_VRAM[obj_tiledata_addr - vram_addr];
    u8 *bgmap = &gb_state->mem_VRAM[bgmap_addr - vram_addr];
    u8 *winmap = &gb_state->mem_VRAM[winmap_addr - vram_addr];

    u8 bg_scroll_x = gb_state->io_lcd_SCX;
    u8 bg_scroll_y = gb_state->io_lcd_SCY;
    u8 win_pos_x = gb_state->io_lcd_WX;
    u8 win_pos_y = gb_state->io_lcd_WY;

    u8 bgwin_palette = gb_state->io_lcd_BGP;
    u8 obj_palette1 = gb_state->io_lcd_OBP0;
    u8 obj_palette2 = gb_state->io_lcd_OBP1;

    u8 obj_tile_height = obj_8x16 ? 16 : 8;

    /* OAM scan - gather (max 10) objects on this line in cache */
    /* TODO: sort the objs so those with smaller x coord have higher prio */
    struct OAMentry *OAM = (struct OAMentry*)&gb_state->mem_OAM[0];
    struct OAMentry objs[10];
    int num_objs = 0;
    if (obj_enable)
        for (int i = 0; i < 40; i++) {
            if (y >= OAM[i].y - 16 && y < OAM[i].y - 16 + obj_tile_height)
                objs[num_objs++] = OAM[i];
            if (num_objs == 10)
                break;
        }


    /* Draw all background pixels of this line.
     *
     * CHDK port: tile-outer, where upstream is pixel-outer.
     *
     * Everything upstream recomputes per pixel is either constant for the whole
     * scanline or constant for the tile:
     *
     *   per line   bg_y, the tile row it falls in, and the row within the tile
     *              - none of which depend on x at all
     *   per tile   the tilemap byte, the CGB attribute byte, the two bitplane
     *              bytes, and the four resolved palette colours
     *
     * Only the shift is genuinely per pixel. So upstream does 160 tilemap
     * reads, 320 tiledata reads and 160 palette resolutions per line where 20,
     * 40 and 20 will do - each tile's two bytes were being fetched eight times
     * over. This walks tiles and expands each one's run of up to 8 pixels.
     *
     * Note the run can be shorter than 8 at either end: SCX puts the first tile
     * partly off the left edge, and the last tile partly off the right.
     *
     * Verified pixel-for-pixel against the original by framebuffer hash over
     * 2000 frames of both ROMs (tools/gbc_bench.c --check).
     */
    if (bg_enable) {
        int bg_y = (y + bg_scroll_y) & 255;
        int bgmap_row = (bg_y >> 3) * 32;   /* bg_tile_y * 32   */
        int toff_y2   = (bg_y & 7) * 2;     /* bg_tileoff_y * 2 */
        int x = 0;

        while (x < GB_LCD_WIDTH) {
            int bg_x  = (x + bg_scroll_x) & 255;
            int first = bg_x & 7;           /* first pixel used in this tile */
            int run   = 8 - first;
            if (run > GB_LCD_WIDTH - x)
                run = GB_LCD_WIDTH - x;

            int bg_idx = (bg_x >> 3) + bgmap_row;

            u8 tile_idx_raw = bgmap[bg_idx];
            s16 tile_idx = bgwin_tilemap_unsigned ? (s16)(u16)tile_idx_raw :
                                                    (s16)(s8)tile_idx_raw;

            /* BG tile attrs are only available on CGB, and are at same location
             * as tile numbers but in bank 1 instead of 0. */
            u8 attr = use_col ? bgmap[bg_idx + VRAM_BANKSIZE] : 0;

            /* Packed 2-bit colour indices: two bytes per tile row, one bit of
             * each per pixel. bg_tileoff/8*2 upstream reduces to the row
             * offset, because the x part is always below 8. */
            int tiledata_off = tile_idx * 16 + toff_y2;
            if (attr & (1<<3))              /* VRAM bank, CGB only */
                tiledata_off += VRAM_BANKSIZE;
            u8 b1 = bgwin_tiledata[tiledata_off];
            u8 b2 = bgwin_tiledata[tiledata_off + 1];

            u16 col[4];
            if (use_col) {
                u8 palidx = attr & 7;
                for (int k = 0; k < 4; k++)
                    col[k] = palette_get_col(gb_state->io_lcd_BGPD, palidx, k);
            } else {
                for (int k = 0; k < 4; k++)
                    col[k] = palette_get_gray(bgwin_palette, k);
            }

            for (int p = 0; p < run; p++) {
                int shift = 7 - (first + p);
                u8 colidx = ((b1 >> shift) & 1) |
                           (((b2 >> shift) & 1) << 1);
                pixline[x + p] = col[colidx];
            }

            x += run;
        }
    } else {
        /* Background disabled - set all pixels to 0 */
        for (int x = 0; x < GB_LCD_WIDTH; x++)
            pixbuf[x + y * GB_LCD_WIDTH] = 0;
    }

    /* Draw the window for this line. */
    if (win_enable) {
        for (int x = 0; x < GB_LCD_WIDTH; x++) {
            int win_x = x - win_pos_x + 7,
                win_y = y - win_pos_y;
            int tile_x = win_x / 8,
                tile_y = win_y / 8;
            int tileoff_x = win_x % 8,
                tileoff_y = win_y % 8;

            if (win_x < 0 || win_y < 0)
                continue;

            u8 tile_idx_raw = winmap[tile_x + tile_y * 32];
            s16 tile_idx = bgwin_tilemap_unsigned ? (s16)(u16)tile_idx_raw :
                                                    (s16)(s8)tile_idx_raw;

            /* We have packed 2-bit color indices here, so the bits look like:
            * (each bit denoted by the pixel index in tile)
            * 01234567 01234567 89abcdef 89abcdef ...
            * So for the 9th pixel (which is px 1,1) we need bytes 2+3 (9/8*2 [+1])
            * and then shift both by 7 (8-9%8).
            */
            int tileoff = tileoff_x + tileoff_y * 8;
            int shift = 7 - tileoff % 8;
            u8 b1 = bgwin_tiledata[tile_idx * 16 + tileoff/8*2];
            u8 b2 = bgwin_tiledata[tile_idx * 16 + tileoff/8*2 + 1];
            u8 colidx = ((b1 >> shift) & 1) |
                       (((b2 >> shift) & 1) << 1);

            u16 col = 0;
            if (use_col)
                col = palette_get_col(gb_state->io_lcd_BGPD, 0, colidx);
            else
                col = palette_get_gray(bgwin_palette, colidx);
            pixbuf[x + y * GB_LCD_WIDTH] = col;
        }
    }

    /* Draw any sprites (objects) on this line.
     *
     * CHDK port: object-outer, where upstream is pixel-outer.
     *
     * Upstream walks all 160 pixels and, at each one, loops over every object
     * on the line - 1600 iterations to place at most 80 pixels, re-reading the
     * same two tile bytes out of VRAM and recomputing the same tile offset on
     * every single (pixel, object) pair. An object covers exactly 8 columns, so
     * iterating objects and walking their own 8 pixels does identical work in a
     * twentieth of the iterations, reads each tile row once, and resolves the
     * palette once per object instead of once per pixel.
     *
     * Ordering is preserved exactly. Objects are still applied in list order,
     * so a later object still overwrites an earlier one at a shared pixel, and
     * the OBJ-to-BG priority test still observes whatever earlier objects have
     * already written to pixbuf. Verified pixel-for-pixel against the original
     * by framebuffer hash over 2000 frames (tools/gbc_bench.c --check).
     */
    for (int i = 0; i < num_objs; i++) {
        u8 flags = objs[i].flags;
        int obj_x = objs[i].x - 8;
        int row = y - (objs[i].y - 16);

        if (flags & (1<<6)) /* Flip y */
            row = obj_tile_height - 1 - row;

        int tiledata_off = objs[i].tile * 16 + row * 2;
        if (use_col && (flags & (1<<3)))
            tiledata_off += VRAM_BANKSIZE;

        u8 b1 = obj_tiledata[tiledata_off];
        u8 b2 = obj_tiledata[tiledata_off + 1];

        /* Nothing to place: all four bitplane pairs are colour 0. */
        if ((b1 | b2) == 0)
            continue;

        int flip_x = (flags & (1<<5)) ? 1 : 0;
        int prio   = (flags & (1<<7)) ? 1 : 0;

        /* Palette resolved once per object rather than once per pixel.
         * Entry 0 is never used - colour 0 is transparent for objects. */
        u16 col[4];
        if (use_col) {
            u8 palidx = flags & 7;
            for (int k = 1; k < 4; k++)
                col[k] = palette_get_col(gb_state->io_lcd_OBPD, palidx, k);
        } else {
            u8 pal = (flags & (1<<4)) ? obj_palette2 : obj_palette1;
            for (int k = 1; k < 4; k++)
                col[k] = palette_get_gray(pal, k);
        }

        for (int px = 0; px < 8; px++) {
            int x = obj_x + px;
            if (x < 0 || x >= GB_LCD_WIDTH)
                continue;

            int shift = flip_x ? px : 7 - px;
            u8 colidx = ((b1 >> shift) & 1) |
                       (((b2 >> shift) & 1) << 1);

            if (colidx == 0)
                continue;
            if (prio && pixline[x] > 0)   /* OBJ-to-BG prio */
                continue;

            pixline[x] = col[colidx];
        }
    }
}
