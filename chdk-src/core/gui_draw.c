#include "platform.h"
#include "touchscreen.h"
#include "conf.h"
#include "font.h"
#include "lang.h"
#include "gui_draw.h"

#define GET_FONT_COMPRESSION_MODE 1
#include "../lib/font/font_8x16_uni_packed.h"
#undef  GET_FONT_COMPRESSION_MODE

#if defined(CAM_DRAW_RGBA) || defined(CAM_DRAW_YUV) 

// macros for computing palette from a single byte color value
#define CALC_YUV_LUMA_OPACITY_FOR_COLOR(color,luma,opacity) \
    { \
        luma = ((color-1)^0xffffffff)&0xf0; \
        opacity = (color&0xf)?255:color; \
    }

#define CALC_YUV_CHROMA_FOR_COLOR(color,u,v) \
    { \
        v = color; \
        u = (v+1); \
        if (!(u&2)) \
        { \
            u = 128; \
            v = 128; \
        } \
        else \
        { \
            u *= 3; \
            v <<= 3; \
        } \
    }

extern volatile char *opacity_buffer[];
extern char* bitmap_buffer[];
extern int active_bitmap_buffer;

#endif

//-------------------------------------------------------------------

// Calculated offset for handling rotated display
unsigned int rotate_base;

//-------------------------------------------------------------------
// Font handling

#pragma pack(1)
// Format of header block for each character in the 'font_data' array
// This is immediately followed by '16 - top - bottom' bytes of character data.
typedef struct {
    unsigned char skips;    // Top and Bottom skip counts for blank rows (4 bits each - ((top << 4) | bottom))
} FontData;
#pragma pack()

static unsigned char* get_cdata(unsigned int *offset, unsigned int *size, const char ch)
{
    FontData *f = (FontData*)get_current_font_data(ch);

    *offset = f->skips >> 4;            // # of blank lines at top
    *size = 16 - (f->skips & 0xF);      // last line of non-blank data
    if (*size == *offset)               // special case for blank char (top == 15 && bottom == 1)
        *offset += 1;

    return (unsigned char*)f + sizeof(FontData) - *offset;
}

//-------------------------------------------------------------------

// Low level routines that are highly dependant on hardware drawing method (8 bit, YUV or RGBA)

#if defined(CAM_DRAW_8BIT)
    #include "gui_draw_8bit.c"
#elif defined(CAM_DRAW_RGBA)
    #include "gui_draw_rgba.c"
#else // CAM_DRAW_YUV
    #include "gui_draw_yuv.c"
#endif

//-------------------------------------------------------------------

// Initialise drawing system
void draw_init()
{
#if defined(CAM_DRAW_8BIT) && !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
    frame_buffer[0] = vid_get_bitmap_fb();
    frame_buffer[1] = frame_buffer[0] + camera_screen.buffer_size;
#endif

    draw_set_draw_proc(NULL);

    draw_set_guard();
}

//-------------------------------------------------------------------

// used to prevent draw_restore from being called until tick reached
// to avoid crashes on video out change
int draw_restore_suspend_tick;

// disable draw_restore for the ms milliseconds
void draw_suspend(int ms)
{
    int t=get_tick_count() + ms;
    // only change if not already suspended to a later time
    if(t > draw_restore_suspend_tick) {
        draw_restore_suspend_tick = t;
    }
}

// Check if drawing is suspended or not
int draw_is_suspended(void)
{
    return (draw_restore_suspend_tick > get_tick_count());
}

//-------------------------------------------------------------------

#ifdef CAM_SCREEN_LOCK_DEBUG
int dbg_restores = 0;       // TEMPORARY - counted per second in spytask
#endif

// Restore CANON_OSD
void draw_restore()
{
#ifdef CAM_SCREEN_LOCK_DEBUG
    dbg_restores++;
#endif
#ifdef CAM_DRAW_RGBA
    extern void vid_bitmap_erase();
    vid_bitmap_erase();
#endif

    if(draw_is_suspended()) {
        return;
    }

    vid_bitmap_refresh();
    draw_set_guard();

#ifdef CAM_TOUCHSCREEN_UI
    redraw_buttons = 1;
#endif
}

//-------------------------------------------------------------------

// Pixel drawing with rotation handling and function override support

void    (*draw_pixel_proc)(unsigned int offset, color cl);
void    (*draw_pixel_proc_norm)(unsigned int offset, color cl);

void draw_pixel_proc_rotated(unsigned int offset, color cl)
{
    draw_pixel_proc_norm(rotate_base - offset, cl);
}

void draw_set_draw_proc(void (*pixel_proc)(unsigned int offset, color cl))
{
    draw_pixel_proc_norm = (pixel_proc)?pixel_proc:draw_pixel_std;
    if (conf.rotate_osd)
    {
        rotate_base = (camera_screen.height - 1) * camera_screen.buffer_width + ASPECT_XCORRECTION(camera_screen.width) - 1;
        draw_pixel_proc = draw_pixel_proc_rotated;
    }
    else
    {
        draw_pixel_proc = draw_pixel_proc_norm;
    }
}

void update_draw_proc()
{
    draw_set_draw_proc(draw_pixel_proc_norm);
}

static int draw_grid_clip;

void draw_set_grid_clip(int enable)
{
    draw_grid_clip = enable;
}

static int draw_grid_pixel_blocked(coord x, coord y)
{
#ifdef CAM_PERSISTENT_OSD
    const int w = FONT_WIDTH * 19;
    const int h = 3 * FONT_HEIGHT + 4;
    const int top = camera_screen.height - h;
    if (!draw_grid_clip) return 0;
    if (y >= top && y <= top + h &&
        ((x >= 0 && x <= w) ||
         (x >= camera_screen.width - w && x <= camera_screen.width)))
        return 1;
    // Top diagrams share a compact 152x36 footprint.
    if (conf.show_histo && x >= 0 && x <= FONT_WIDTH * 19 &&
        y >= 0 && y <= 36)
        return 1;
    // Persistent bend patchbay: fixed maximum width keeps this independent of
    // the sensor bit depth and prevents reticle pixels leaking between cells.
    // An experimental profile brings the same plate up on its own, so it has
    // to hold the grid off the same rectangle - see posd_bend_draw().
    if (((conf.bitbend_enable && !bend_is_identity(&conf.bitbend))
#ifdef CAM_BEND_EXPERIMENTAL
         || (conf.bendx_enable && bendx_chain_count(&conf.bendx) > 0)
#endif
        ) &&
        x >= camera_screen.width - (FONT_WIDTH * 19) &&
        y >= 0 && y <= 36)
        return 1;
    return 0;
#else
    return 0;
#endif
}

void draw_pixel(coord x, coord y, color cl)
{
    if (draw_grid_pixel_blocked(x, y)) return;
    // Make sure pixel is on screen. Skip top left pixel if screen erase detection is on to avoid triggering the detector.
    if ((x < 0) || (y < 0) || (x >= camera_screen.width) || (y >= camera_screen.height) || ((x == 0) && (y == 0))) return;
    else
    {
        register unsigned int offset = y * camera_screen.buffer_width + ASPECT_XCORRECTION(x);
        draw_pixel_proc(offset,   cl);
#if CAM_USES_ASPECT_CORRECTION
        draw_pixel_proc(offset+1, cl);  // Draw second pixel if screen scaling is needed
#endif
   }
}

//-------------------------------------------------------------------

// Read one pixel back out of the bitmap buffer.
//
// Enabled for CAM_PERSISTENT_OSD, which uses it to notice that something has
// wiped the overlay out from under it - see gui_draw_persistent_osd(). It reads
// frame_buffer[0] because draw_pixel_std() writes both buffers identically on
// these ports, so either one answers the question.
color draw_get_pixel(coord x, coord y)
{
#ifdef CAM_DRAW_8BIT
    if ((x < 0) || (y < 0) || (x >= camera_screen.width) || (y >= camera_screen.height)) return 0;
    if (conf.rotate_osd)
    {
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
        return bitmap_buffer[0][rotate_base - (y * camera_screen.buffer_width + ASPECT_XCORRECTION(x))];
#else
        return frame_buffer[0][rotate_base - (y * camera_screen.buffer_width + ASPECT_XCORRECTION(x))];
#endif
    }
    else
    {
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
        return bitmap_buffer[0][y * camera_screen.buffer_width + ASPECT_XCORRECTION(x)];
#else
        return frame_buffer[0][y * camera_screen.buffer_width + ASPECT_XCORRECTION(x)];
#endif
    }
#else // CAM_DRAW_YUV || CAM_DRAW_RGBA
    // DIGIC 6 not supported
    (void)x; (void)y;
    return 0;
#endif
}

//-------------------------------------------------------------------
// Zebra

#if defined(CAM_DRAW_8BIT)

// not implemented for earlier DIGICs
unsigned int color_to_rawpx(__attribute__ ((unused))color cl, __attribute__ ((unused))unsigned int *op) { return 0; }
void draw_dblpixel_raw(__attribute__ ((unused))unsigned int offset, __attribute__ ((unused))unsigned int px, __attribute__ ((unused))unsigned int op) {}
void erase_zebra() {}

#else // CAM_DRAW_RGBA or CAM_DRAW_YUV

// direct drawing functions for YUV overlay, currently used by the zebra module
// ATTENTION: these functions do not support guard pixels or rotation

// Erase the pixels drawn by the zebra code
void erase_zebra()
{
    int x, y;
    int active_buffer_index =  active_bitmap_buffer & 1;
    unsigned short *obu0 = (unsigned short *)(&opacity_buffer[active_buffer_index][0]);
    unsigned int *bbu0 = (unsigned int *)(&bitmap_buffer[active_buffer_index][0]);
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
    unsigned short *obu1 = (unsigned short *)(&opacity_buffer[active_buffer_index^1][0]);
    unsigned int *bbu1 = (unsigned int *)(&bitmap_buffer[active_buffer_index^1][0]);
#endif
    for (y = 0; y < vid_get_viewport_height(); y += 1) {
        for (x = 0; x < vid_get_viewport_width() / 2; x += 1) {
            if (obu0[x] == 0xfdfd) {
                obu0[x] = 0;
                bbu0[x] = 0x00800080;
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
                obu1[x] = 0;
                bbu1[x] = 0x00800080;
#endif
            }
        }
        obu0 += vid_get_viewport_buffer_width_proper() / 2;
        bbu0 += vid_get_viewport_buffer_width_proper() / 2;
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
        obu1 += vid_get_viewport_buffer_width_proper() / 2;
        bbu1 += vid_get_viewport_buffer_width_proper() / 2;
#endif
    }
}

// translate single byte CHDK color to a whole yuv unit (2 pixels)
// opacity is returned via the second argument
unsigned int color_to_rawpx(color cl, unsigned int *op)
{
#ifdef CAM_DRAW_RGBA
    cl = yuv_colors[cl&31];
#endif
    unsigned int y,u,v,o;
    CALC_YUV_CHROMA_FOR_COLOR(cl,u,v);
    CALC_YUV_LUMA_OPACITY_FOR_COLOR(cl,y,o);
    if (o == 255) o = 253;
    if (op) *op = o | (o << 8);
    return (u&255)+((y&255)<<8)+((v&255)<<16)+(y<<24);
}

// function for drawing a whole yuv unit (2 pixels, aligned)
// opacity is a single byte, used for both pixels
// drawing offset is pixel offset, calculated by caller
// Does not overwrite Canon UI
void draw_dblpixel_raw(unsigned int offset, unsigned int px, unsigned int op)
{
    offset >>= 2;
    int active_buffer_index =  active_bitmap_buffer & 1;
    unsigned short *obu0 = (unsigned short *)(&opacity_buffer[active_buffer_index][0]);
    unsigned int *bbu0 = (unsigned int *)(&bitmap_buffer[active_buffer_index][0]);
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
    unsigned short *obu1 = (unsigned short *)(&opacity_buffer[active_buffer_index^1][0]);
    unsigned int *bbu1 = (unsigned int *)(&bitmap_buffer[active_buffer_index^1][0]);
#endif
    if ((obu0[offset] == 0xfdfd) || ((op != 0) && (obu0[offset] == 0))) {
        bbu0[offset] = px;
        obu0[offset] = op;
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
        bbu1[offset] = px;
        obu1[offset] = op;
#endif
    }
}

#endif

//-------------------------------------------------------------------
// Edge overlay

void draw_or_erase_edge_pixel(coord px, coord py, color cl, int is_draw)
{
#ifdef CAM_DRAW_8BIT
    //  Skip top left pixel if screen erase detection is on to avoid triggering the detector.
    if ((px == 0) && (py == 0)) return;
#endif

    // Make sure pixel is on screen.
    if ((px < 0) || (py < 0) || (px >= CAMERA_SCREEN_BM_WIDTH) || (py >= CAMERA_SCREEN_BM_HEIGHT)) return;

    // bitmap buffer offset
    register unsigned int offset = py * CAMERA_SCREEN_BM_BUFFER_WIDTH + ASPECT_XCORRECTION(px);

#ifdef CAM_DRAW_RGBA    
    // Get YUV color value
    cl = yuv_colors[cl&31];
#endif

#ifdef CAM_DRAW_8BIT

    // See if we need to erase, do nothing if not drawing and current pixel is not edge pixel, otherwise set draw color to transparent.
    if (!is_draw)
    {
#if defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
        if (bitmap_buffer[active_bitmap_buffer][offset] != cl) return;
#else
        if (frame_buffer[0][offset] != cl) return;
#endif
        cl = 0;     // Transparent
    }

    // Draw pixel
    draw_pixel_proc_norm(offset,   cl);
#if CAM_USES_ASPECT_CORRECTION
    draw_pixel_proc_norm(offset+1, cl);  // Draw second pixel if screen scaling is needed
#endif

#else // CAM_DRAW_RGBA || CAM_DRAW_YUV

#if defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
    unsigned char *obu0 = (unsigned char *)(&opacity_buffer[active_bitmap_buffer&1][0]);
#else
    unsigned char *obu0 = (unsigned char *)(&opacity_buffer[0][0]);
#endif

    // See if we need to erase, do nothing if not drawing and current pixel is not edge pixel, otherwise set draw color to transparent.
    if (!is_draw)
    {
        if (obu0[offset] != 254) return;
        cl = 0;     // Transparent
    }

    unsigned int y;
    unsigned int o;
    CALC_YUV_LUMA_OPACITY_FOR_COLOR(cl,y,o);
    unsigned int u;
    unsigned int v;
    CALC_YUV_CHROMA_FOR_COLOR(cl,u,v);

    if (o == 255) o = 254;  // Adjust opacity so we can test later

    register unsigned int offs2 = (offset>>1)<<2;

#if defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
    unsigned char *bbu0 = (unsigned char *)(&bitmap_buffer[active_bitmap_buffer&1][0]);
#else
    unsigned char *bbu0 = (unsigned char *)(&bitmap_buffer[0][0]);
    unsigned char *obu1 = (unsigned char *)(&opacity_buffer[1][0]);
    unsigned char *bbu1 = (unsigned char *)(&bitmap_buffer[1][0]);
#endif

    obu0[offset] = o;
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
    obu1[offset] = o;
#endif
    if (offset&1) // x is odd
    {
        bbu0[offs2+3] = y; // Y
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
        bbu1[offs2+3] = y; // Y
#endif
    }
    else // x is even
    {
        bbu0[offs2+1] = y; // Y
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
        bbu1[offs2+1] = y; // Y
#endif
    }
    bbu0[offs2+0] = u; // U?
    bbu0[offs2+2] = v; // V?
#if !defined(DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY)
    bbu1[offs2+0] = u; // U?
    bbu1[offs2+2] = v; // V?
#endif

#endif // !CAM_DRAW_8BIT
}

//-------------------------------------------------------------------
// Character drawing functions for 8 bit and RGBA drawing - see gui_draw_yuv.c for YUV optimised versions

#if defined(CAM_DRAW_RGBA) || defined(CAM_DRAW_8BIT) 

// DIGIC II...5 and RGBA drawing for DIGIC 6 and later

// Draw single character at normal size
void draw_char(coord x, coord y, const char ch, twoColors cl)
{
    unsigned i, ii;

    twoColors clb = MAKE_COLOR(BG_COLOR(cl),BG_COLOR(cl));

    unsigned int offset, size;
    unsigned char *sym = get_cdata(&offset, &size, ch);

    // First draw blank lines at top
    if (offset > 0)
        draw_rectangle(x,y,x+FONT_WIDTH-1,y+offset-1,clb,RECT_BORDER0|DRAW_FILLED);

    // Now draw character data

    unsigned j;
    for (j=i=offset; i<size;)
    {
        unsigned int dsym;
        int rep;
#if defined(BUILTIN_FONT_RLE_COMPRESSED_V2)
        if (sym[j] > 0xF0)
        {
            rep = (sym[j] & 0x0F) + 1;
            j++;
        }
        else
        {
            rep = (sym[j] & 0x80) ? 2 : 1;
        }
        dsym = fontdata_lookup[sym[j] & 0x7f];
#elif defined(BUILTIN_FONT_RLE_COMPRESSED)
        rep = (sym[j] & 0x80) ? 2 : 1;
        dsym = fontdata_lookup[sym[j] & 0x7f];
#else
        rep = 1;
        dsym = sym[j];
#endif
        j++;
        for (; rep > 0; rep--)
        {
            for (ii=0; ii<FONT_WIDTH; ii++)
            {
                draw_pixel(x+ii, y+i, (dsym & (0x80>>ii))? FG_COLOR(cl) : BG_COLOR(cl));
            }
            i++;
        }
    }

    // Last draw blank lines at bottom
    if (i < FONT_HEIGHT)
        draw_rectangle(x,y+i,x+FONT_WIDTH-1,y+FONT_HEIGHT-1,clb,RECT_BORDER0|DRAW_FILLED);
}

// Draw char with scaling in X or Y dimension
void draw_char_scaled(coord x, coord y, const char ch, twoColors cl, int xsize, int ysize)
{
    unsigned i, ii;

    twoColors clf = MAKE_COLOR(FG_COLOR(cl),FG_COLOR(cl));
    twoColors clb = MAKE_COLOR(BG_COLOR(cl),BG_COLOR(cl));

    unsigned int offset, size;
    unsigned char *sym = get_cdata(&offset, &size, ch);

    // First draw blank lines at top
    if (offset > 0)
        draw_rectangle(x,y,x+FONT_WIDTH*xsize-1,y+offset*ysize-1,clb,RECT_BORDER0|DRAW_FILLED);

    // Now draw character data
    unsigned j;
    for (j=i=offset; i<size;)
    {
        unsigned int dsym;
        int rep;
        unsigned int last;
        int len;
#if defined(BUILTIN_FONT_RLE_COMPRESSED_V2)
        if (sym[j] > 0xF0)
        {
            rep = (sym[j] & 0x0F) + 1;
            j++;
        }
        else
        {
            rep = (sym[j] & 0x80) ? 2 : 1;
        }
        dsym = fontdata_lookup[sym[j] & 0x7f];
#elif defined(BUILTIN_FONT_RLE_COMPRESSED)
        rep = (sym[j] & 0x80) ? 2 : 1;
        dsym = fontdata_lookup[sym[j] & 0x7f];
#else
        rep = 1;
        dsym = sym[j];
#endif
        j++;
        last = dsym & 0x80;
        len = 1;
        for (ii=1; ii<FONT_WIDTH; ii++)
        {
            if (((dsym << ii) & 0x80) != last)
            {
                draw_rectangle(x+(ii-len)*xsize,y+i*ysize,x+ii*xsize-1,y+i*ysize+rep*ysize-1,(last)?clf:clb,RECT_BORDER0|DRAW_FILLED);
                last = (dsym << ii) & 0x80;
                len = 1;
            }
            else
            {
                len++;
            }
        }
        draw_rectangle(x+(ii-len)*xsize,y+i*ysize,x+ii*xsize-1,y+i*ysize+rep*ysize-1,(last)?clf:clb,RECT_BORDER0|DRAW_FILLED);
        i += rep;
    }

    // Last draw blank lines at bottom
    if (i < FONT_HEIGHT)
        draw_rectangle(x,y+i*ysize,x+FONT_WIDTH*xsize-1,y+FONT_HEIGHT*ysize-1,clb,RECT_BORDER0|DRAW_FILLED);
}

#endif

//-------------------------------------------------------------------
// Palette handling

extern unsigned char ply_colors[];

unsigned char *chdk_colors = ply_colors;

void set_palette()
{
#ifdef CAM_DRAW_8BIT
    extern unsigned char rec_colors[];
    if (camera_info.state.mode_rec)
        chdk_colors = rec_colors;
    else
        chdk_colors = ply_colors;
#endif // CAM_DRAW_8BIT
}

color get_script_color(int cl)
{
    // > 255, CHDK standard / script color
    if (cl & ~0xff) {
        cl &= 0xff;
        if(cl <= IDX_COLOR_MAX)
            return chdk_colors[cl];
        return chdk_colors[0];
    } else {
        return cl;
    }
}

// Convert user adjustable color (from conf struct) to Canon colors
color chdkColorToCanonColor(chdkColor col)
{
    if (col.type)
        return chdk_colors[col.col];
    return col.col;
}

twoColors user_color(confColor cc)
{
    color fg = chdkColorToCanonColor(cc.fg);
    color bg = chdkColorToCanonColor(cc.bg);

    return MAKE_COLOR(bg,fg);
}

//-------------------------------------------------------------------

#include "gui_draw_shapes.c"
#include "gui_draw_text.c"

//-------------------------------------------------------------------
