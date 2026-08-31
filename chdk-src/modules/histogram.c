#include "camera_info.h"
#include "conf.h"
#include "math.h"
#include "modes.h"
#include "viewport.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_osd.h"
#include "shooting.h"
#include "histogram.h"

//-------------------------------------------------------------------

// Indexes into the various arrays for calculating the histogram channels
#define HISTO_R                     0       // Red channel
#define HISTO_G                     1       // Green channel
#define HISTO_B                     2       // Blue channel
#define HISTO_RGB                   3       // Combined Red, Green and Blue
#define HISTO_Y                     4       // Luminance (Y) from viewport

// Display modes - must match gui_histo_view_modes in gui.c
#define OSD_HISTO_LAYOUT_A          0       // A - RGB
#define OSD_HISTO_LAYOUT_Y          1
#define OSD_HISTO_LAYOUT_A_Y        2
#define OSD_HISTO_LAYOUT_R_G_B      3
#define OSD_HISTO_LAYOUT_A_yrgb     4
#define OSD_HISTO_LAYOUT_Y_argb     5
#define OSD_HISTO_LAYOUT_BLEND      6
#define OSD_HISTO_LAYOUT_BLEND_Y    7
#define LAYOUT_TYPE_MASK            0x0F
#define LAYOUT_SMALL                32
#define LAYOUT_BLEND                64

#ifndef LARGE_HISTO
#define HISTO_DOT_SIZE  3
#else
// digic 6 CHDK screen is ~480 lines instead for 240
#define HISTO_DOT_SIZE  5
#endif
#define HISTO_DOT_PAD   (HISTO_DOT_SIZE + 2)

// Approximate number of pixels to sample, subject to minimums step sizes
// Actual number of samples may exceed target due to rounding/truncation in step calculations
// Actual total must be less than 64K to allow storing histogram counts in unsigned short
// 20k based on typical number in earlier implementation
// cameras typically process 250-600 px/ms uncached, 300-1200 cached
// NOTE target could be adjusted based on cam_info.cam_digic
#define HISTO_TARGET_SAMPLES 20000
// Minimum X step is 4, for alignment with YUV tuples (could technically be 2 on Digic >=6 cams with UYVY viewports)
#define HISTO_XSTEP_MIN 4
// Minimum Y step is 2, no need to sample more densely
#define HISTO_YSTEP_MIN 2

static unsigned char histogram[5][HISTO_WIDTH];             // RGBYG
static unsigned short *histogram_proc[5] = { 0,0,0,0,0 };   // RGBYG (logic in histogram_process stage 0 ensures unsigned short is large enough)
unsigned int histo_max[5], histo_max_center[5];             // RGBYG
static float histo_max_center_invw[5];                      // RGBYG

static long histo_magnification;
static int under_exposed;
static int over_exposed;

static int histogram_stage = 0;
static int histogram_drawn = 0;

// Flags for histogram_drawn variable. Low 12 bits is calculated as histogram height to erase.
#define HDRAW_READY         0x4000      // Calculations done - histogram ready to draw
#define HDRAW_MAG           0x2000      // Set if histogram magnification level was drawn
#define HDRAW_EXP           0x1000      // Set if 'EXP' message drawn

//-------------------------------------------------------------------
// Histogram calculation functions

// Clip value to byte range (for YUV -> RGB conversion)
static int clip(int v)
{
    if (v<0) v=0;
    else if (v>255) v=255;
    return v;
}

// Transform functions
static float identity(float x)      { return x; }
static float logarithmic(float x)   { return log(x); }

static void histogram_alloc()
{
    // Allocate arrays to store counts during processing
    // Each possible value is counted so the array sizes are set to 256, then these are summed to
    // convert down to desired width of 128.
    // This is faster than scaling each value as it is counted
    if (histogram_proc[0] == 0)
    {
        histogram_proc[0] = malloc(5 * 256 * sizeof(unsigned short));
        if (histogram_proc[0] != 0)
        {
            histogram_proc[1] = histogram_proc[0] + 256;
            histogram_proc[2] = histogram_proc[1] + 256;
            histogram_proc[3] = histogram_proc[2] + 256;
            histogram_proc[4] = histogram_proc[3] + 256;
        }
    }
}

/*
do a single stage of reading YUV data from framebuffer
Sampled area has margins of one step at top, right and left and bottom
Each stage samples every 3rd ystep row
*/
void histogram_sample_stage(unsigned char *img, int stage, int byte_width, int vis_byte_width, int height, int xstep_bytes, int ystep)
{
    // start at stage'th ystep (1-3), plus one step in x
    unsigned char *p_row = img + stage*ystep*byte_width + xstep_bytes;
    // end one step short of height
    unsigned char *p_max = img + byte_width*(height-ystep);
    // number of from start to end of sampled row
    // -2 xstep_bytes for margins
    int row_sample_len = vis_byte_width - 2*xstep_bytes;

    // every 3rd ystep for each stage
    int ystep_bytes = ystep*byte_width*3;

    for(;p_row < p_max; p_row += ystep_bytes) {
        // start sample 1 step after start
        unsigned char *p = p_row;
        unsigned char *p_row_end = p_row + row_sample_len;
        for(;p < p_row_end; p+= xstep_bytes) {
            int y, v, u, hi;
            y = p[1];
#ifndef THUMB_FW
            u = (signed char)p[0];
            v = (signed char)p[2];
#else
            u = (int)p[0] - 128;
            v = (int)p[2] - 128;
#endif
//                p[1] = p[3] = 255;    // Draw columns on screen for debugging

            ++histogram_proc[HISTO_Y][y];                       // Y
            hi = clip(((y<<12)          + v*5743 + 2048)>>12);  // R
            ++histogram_proc[HISTO_R][hi];
            hi = clip(((y<<12) - u*1411 - v*2925 + 2048)>>12);  // G
            ++histogram_proc[HISTO_G][hi];
            hi = clip(((y<<12) + u*7258          + 2048)>>12);  // B
            ++histogram_proc[HISTO_B][hi];
        }
    }
}

void histogram_process()
{
    static unsigned char *img;

    static int viewport_byte_width, viewport_height;

    static int viewport_vis_byte_width;
    static int xstep_bytes, ystep;
    static int exposition_thresh;

    int i, c;
    float (*histogram_transform)(float);
    unsigned int histo_fill;
    int histo_main;

    // Select transform function
    histogram_transform = conf.histo_mode ? logarithmic : identity;

    histogram_alloc();
    // Check memory was allocated
    if (histogram_proc[0] == 0) return;

    // This function is called in the main spytask loop roughly every 20msec
    // To avoid hogging all the CPU it performs it's work in stages controlled by histogram-stage
    // Stage  Function
    //   0      Initialize global variables used in next stages
    //   1,2,3  Count number of values for a third of the viewport image at each stage
    //   4      Calculate max values, over and under exposure setting
    //   5      Calculate the histogram display values
    switch (histogram_stage)
    {
        case 0: {
            img = vid_get_viewport_active_buffer();
            if (!img) return;

            img += vid_get_viewport_image_offset();     // offset into viewport for when image size != viewport size (e.g. 16:9 image on 4:3 LCD)

            viewport_height = vid_get_viewport_height_proper();
            viewport_byte_width = vid_get_viewport_byte_width();
            int viewport_pix_width = vid_get_viewport_width_proper();
            int total_pixels = viewport_pix_width * viewport_height;
            int xstep;

            if (total_pixels <= HISTO_XSTEP_MIN*HISTO_YSTEP_MIN*HISTO_TARGET_SAMPLES) {
                xstep = HISTO_XSTEP_MIN;
                ystep = HISTO_YSTEP_MIN;
            } else {
                // initial y step based on min x step and total samples
                ystep = total_pixels/(HISTO_XSTEP_MIN*HISTO_TARGET_SAMPLES);
                // if Y step is large, redistribute some to X, keeping multiple of 4
                // in practice only hit for FHD HDMI
                if(ystep >= 5*HISTO_YSTEP_MIN) {
                    xstep = 2*HISTO_XSTEP_MIN;
                    ystep >>= 1;
                } else {
                    xstep = HISTO_XSTEP_MIN;
                }
            }

            // Original calculation was camera_screen.size / 500. Based on most cameras having 320x240 bitmap this = 172.
            // This does not work for high-res bitmap display (e.g. Digic6 YUV drawing) where camera_screen.size = 720x480 giving exposition_thresh 4 times larger.
            // Calculation now based on number of sampled pixels with default value still coming out at 172.
            exposition_thresh = total_pixels / (xstep * ystep * 125);

#ifndef THUMB_FW
            xstep_bytes = (xstep*3)/2; // 4 pixels = 6 bytes, step is multiple of 4
            viewport_vis_byte_width = (viewport_pix_width*3)/2;
#else
            xstep_bytes = xstep*2;
            viewport_vis_byte_width = viewport_pix_width*2;
#endif

            for (c=0; c<5; ++c) {
                memset(histogram_proc[c],0,256*sizeof(unsigned short));
                histo_max[c] = histo_max_center[c] = 0;
            }

            histogram_stage = 1;
            break;
        }

        case 1:
        case 2:
        case 3: {
            histogram_sample_stage(img, histogram_stage, viewport_byte_width, viewport_vis_byte_width, viewport_height, xstep_bytes, ystep);
            histogram_stage += 1;
            break;
        }

        case 4:
            for (i=0, c=0; i<HISTO_WIDTH; ++i, c+=2) { // G
                // Merge each pair of values into a single value (for width = 128)
                // Warning: this is optimised for HISTO_WIDTH = 128, don't change the width unless you re-write this code as well.
#ifndef LARGE_HISTO
                histogram_proc[HISTO_Y][i] = histogram_proc[HISTO_Y][c] + histogram_proc[HISTO_Y][c+1];
                histogram_proc[HISTO_R][i] = histogram_proc[HISTO_R][c] + histogram_proc[HISTO_R][c+1];
                histogram_proc[HISTO_G][i] = histogram_proc[HISTO_G][c] + histogram_proc[HISTO_G][c+1];
                histogram_proc[HISTO_B][i] = histogram_proc[HISTO_B][c] + histogram_proc[HISTO_B][c+1];
#endif
                // Calc combined RGB totals
                histogram_proc[HISTO_RGB][i] = histogram_proc[HISTO_R][i] + histogram_proc[HISTO_G][i] + histogram_proc[HISTO_B][i];
            }

            // calculate maximums
            for (c=0; c<5; ++c) {
                for (i=0; i<HISTO_WIDTH; ++i) {
                    if (histo_max[c]<histogram_proc[c][i])
                        histo_max[c]=histogram_proc[c][i];
                    if (histo_max_center[c]<histogram_proc[c][i] && i>=conf.histo_ignore_boundary && i<HISTO_WIDTH-conf.histo_ignore_boundary)
                        histo_max_center[c]=histogram_proc[c][i];
                }

                if (histo_max_center[c] > 0) {
                    histo_max_center_invw[c] = ((float)HISTO_HEIGHT)/histogram_transform((float)histo_max_center[c]);
                } else if (histo_max[c] > 0) {
                    histo_max_center_invw[c] = ((float)HISTO_HEIGHT)/histogram_transform((float)histo_max[c]);
                } else {
                    histo_max_center_invw[c] = 0.0f;
                }
            }

            if (histo_max[HISTO_RGB] > 0) { // over- / under- exposed
                under_exposed = (histogram_proc[HISTO_RGB][0]*8
                                +histogram_proc[HISTO_RGB][1]*4
                                +histogram_proc[HISTO_RGB][2]) > exposition_thresh;
                over_exposed  = (histogram_proc[HISTO_RGB][HISTO_WIDTH-3]
                                +histogram_proc[HISTO_RGB][HISTO_WIDTH-2]*4
                                +histogram_proc[HISTO_RGB][HISTO_WIDTH-1]*8) > exposition_thresh;
            } else {
                over_exposed = 0;
                under_exposed = 1;
            }

            histogram_stage = 5;
            break;

        case 5:
            // Select which calculated histogram channel determines magnification / scaling
            if (conf.histo_layout == OSD_HISTO_LAYOUT_Y || conf.histo_layout == OSD_HISTO_LAYOUT_Y_argb)
                histo_main = HISTO_Y;
            else
                histo_main = HISTO_RGB;

            histo_fill = 0;
            for (c=0; c<5; ++c) {
                for (i=0; i<HISTO_WIDTH; ++i) {
                    histogram[c][i] = (histogram_transform((float)histogram_proc[c][i]))*histo_max_center_invw[c];
                    if (histogram[c][i] > HISTO_HEIGHT)
                        histogram[c][i] = HISTO_HEIGHT;
                    if (c == histo_main)
                        histo_fill += histogram[c][i];
                }
            }

            histo_magnification = 0;
            if (conf.histo_auto_ajust) {
                if (histo_fill < (HISTO_HEIGHT*HISTO_WIDTH)/5) { // try to adjust if average level is less than 20%
                    histo_magnification = (20*HISTO_HEIGHT*HISTO_WIDTH) / histo_fill;
                    for (c=0; c<5; ++c) {
                        for (i=0;i<HISTO_WIDTH;i++) {
                            histogram[c][i] = histogram[c][i] * histo_magnification / 100;
                            if (histogram[c][i] > HISTO_HEIGHT)
                                histogram[c][i] = HISTO_HEIGHT;
                        }
                    }
                }
            }

            histogram_stage = 0;
            histogram_drawn |= HDRAW_READY;
            break;
    }
}

//-------------------------------------------------------------------
// Histogram colors
color cl_norm, cl_over, cl_bg;

// Histogram display functions

static void gui_osd_draw_single_histo(int hist, coord x, coord y, unsigned int w, unsigned int h, int small)
{
    register unsigned int i, threshold, y1, y2;
    register color cl;

    switch (hist)
    {
        case HISTO_R:
            cl=COLOR_RED;
            break;
        case HISTO_G:
            cl=COLOR_GREEN;
            break;
        case HISTO_B:
            cl=COLOR_BLUE;
            break;
        case HISTO_RGB:
        case HISTO_Y:
        default:
            cl=cl_norm;
            break;
    }

    for (i=0; i<w; ++i) {
        if (small)
            threshold = (histogram[hist][i<<1]+histogram[hist][(i<<1)+1])>>2;
        else
            threshold = histogram[hist][i];
        y2 = y1 = threshold;
        if (conf.show_overexp && (y1 == h)) { y1 = h - 3; y2 = h; }

        // Draw calculated histogram value
        if (y1 > 0)
            draw_vline(x+1+i, y+1+h-y1, y1, cl);

        // Draw over exposure (if required)
        if (y2 > y1)
            draw_vline(x+1+i, y+1+h-y2, y2-y1, cl_over);

        // Draw background to fill column unless already filled
        if (h > y2)
            draw_vline(x+1+i, y+1, h-y2, cl_bg);
    }
}

static void gui_osd_draw_blended_histo(coord x, coord y)
{
    register unsigned int i, v, red, grn, blu, sel, y1, y2, y3;
    static color cls[] = {
        0,
        IDX_COLOR_BLUE,
        IDX_COLOR_GREEN,
        IDX_COLOR_CYAN,
        IDX_COLOR_RED,
        IDX_COLOR_MAGENTA,
        IDX_COLOR_YELLOW,
        IDX_COLOR_WHITE
    };

    for (i=0; i<HISTO_WIDTH; ++i) {
        // Draw up to 4 lines - one for each color channel plus background to fill unused space.

        y1 = red = histogram[HISTO_R][i];
        y2 = grn = histogram[HISTO_G][i];
        y3 = blu = histogram[HISTO_B][i];

        // sort low to high
        if (y1 > y2) { v = y1; y1 = y2; y2 = v; }
        if (y1 > y3) { v = y1; y1 = y3; y3 = v; }
        if (y2 > y3) { v = y2; y2 = y3; y3 = v; }

        // First line - lowest value if not 0
        if (y1 > 0) {
            sel = 0;
            if (y1 <= red) sel = 4;
            if (y1 <= grn) sel |= 2;
            if (y1 <= blu) sel |= 1;
            draw_vline(x+1+i, y+1+HISTO_HEIGHT-y1, y1, chdk_colors[cls[sel]]);
        }

        // Second line - middle value if higher than lowest
        if (y2 > y1) {
            sel = 0;
            if (y2 <= red) sel = 4;
            if (y2 <= grn) sel |= 2;
            if (y2 <= blu) sel |= 1;
            draw_vline(x+1+i, y+1+HISTO_HEIGHT-y2, y2-y1, chdk_colors[cls[sel]]);
        }

        // Third line - highest value if higher than middle value
        if (y3 > y2) {
            sel = 0;
            if (y3 <= red) sel = 4;
            if (y3 <= grn) sel |= 2;
            if (y3 <= blu) sel |= 1;
            draw_vline(x+1+i, y+1+HISTO_HEIGHT-y3, y3-y2, chdk_colors[cls[sel]]);
        }

        // Draw background to fill column unless already filled
        if (HISTO_HEIGHT > y3)
            draw_vline(x+1+i, y+1, HISTO_HEIGHT-y3, cl_bg);
    }
}

//-------------------------------------------------------------------
signed char histo_layouts[8][5] = {
    { HISTO_RGB, -1 },                                                                                      // OSD_HISTO_LAYOUT_A
    { HISTO_Y, -1 },                                                                                        // OSD_HISTO_LAYOUT_Y
    { HISTO_RGB, HISTO_Y, -1 },                                                                             // OSD_HISTO_LAYOUT_A_Y
    { HISTO_R, HISTO_G, HISTO_B, -1 },                                                                      // OSD_HISTO_LAYOUT_R_G_B
    { HISTO_RGB, HISTO_Y|LAYOUT_SMALL, HISTO_R|LAYOUT_SMALL, HISTO_G|LAYOUT_SMALL, HISTO_B|LAYOUT_SMALL },  // OSD_HISTO_LAYOUT_A_yrgb
    { HISTO_Y, HISTO_RGB|LAYOUT_SMALL, HISTO_R|LAYOUT_SMALL, HISTO_G|LAYOUT_SMALL, HISTO_B|LAYOUT_SMALL },  // OSD_HISTO_LAYOUT_Y_argb
    { LAYOUT_BLEND, -1 },                                                                                   // OSD_HISTO_LAYOUT_BLEND
    { LAYOUT_BLEND, HISTO_Y, -1 },                                                                          // OSD_HISTO_LAYOUT_BLEND_Y
};

void gui_osd_draw_histo(int is_osd_edit)
{
    if (is_osd_edit ||
        (camera_info.state.mode_rec && !camera_info.state.mode_video &&
         (
          ((conf.show_histo==SHOW_HISTO_HALF) && camera_info.state.is_shutter_half_press) ||
          ((conf.show_histo==SHOW_HISTO_REC) && camera_info.state.mode_rec && (recreview_hold==0)) ||
          ((conf.show_histo==SHOW_HISTO_ALWAYS) && (recreview_hold==0))
         )
        )
       )
    {
        // Check if histogram calculation has finished (and not in osd editor).
        if (!is_osd_edit && ((histogram_drawn & HDRAW_READY) == 0)) return;

        twoColors hc = user_color(conf.histo_color);
        twoColors hc2 = user_color(conf.histo_color2);

        cl_bg = BG_COLOR(hc);
        cl_norm = FG_COLOR(hc);
        cl_over = BG_COLOR(hc2);

        // keep flag bits for magnification and 'EXP' warning erase.
        // low 12 bits is pixel height of histogram rows drawn
        histogram_drawn &= (HDRAW_MAG | HDRAW_EXP);

        int i;
        int x = conf.histo_pos.x, y = conf.histo_pos.y, w, h;
        for (i = 0; i < 5 && histo_layouts[conf.histo_layout][i] >= 0; i += 1) {
            int l = histo_layouts[conf.histo_layout][i];
            // Size
            w = (l & LAYOUT_SMALL) ? HISTO_WIDTH / 2 : HISTO_WIDTH;
            h = (l & LAYOUT_SMALL) ? HISTO_HEIGHT / 2 : HISTO_HEIGHT;
            // Draw histogram type
            if (l & LAYOUT_BLEND)
                gui_osd_draw_blended_histo(x, y);
            else
                gui_osd_draw_single_histo(l & LAYOUT_TYPE_MASK, x, y, w, h, l & LAYOUT_SMALL);
            // Border
            draw_rectangle(x, y, x+1+w, y+1+h, hc2, RECT_BORDER1);
            // Vertical Lines
            if (conf.histo_show_ev_grid) {
                int j;
                for (j=1; j<=4; j+=1) draw_vline(x+(1+w)*j/5, y+1, h, FG_COLOR(hc2));
            }
            // Adjust position for next histogram
            if ((l & LAYOUT_SMALL) && (i & 1)) {
                x += (w + 1);
            } else {
                x = conf.histo_pos.x;
                y += (h + 1);
                histogram_drawn += (h + 1);
            }
        }

        if ((conf.histo_layout != OSD_HISTO_LAYOUT_R_G_B) && conf.show_overexp)
        {
            if (under_exposed)
            {
                draw_ellipse(conf.histo_pos.x+HISTO_DOT_PAD, conf.histo_pos.y+HISTO_DOT_PAD,
                             HISTO_DOT_SIZE, HISTO_DOT_SIZE, cl_over, DRAW_FILLED);
            }
            if (over_exposed)
            {
                draw_ellipse(conf.histo_pos.x+HISTO_WIDTH-HISTO_DOT_PAD, conf.histo_pos.y+HISTO_DOT_PAD,
                             HISTO_DOT_SIZE, HISTO_DOT_SIZE, cl_over, DRAW_FILLED);
            }
        }

        if ((conf.show_overexp ) && camera_info.state.is_shutter_half_press && (under_exposed || over_exposed)) {
            draw_string(conf.histo_pos.x+HISTO_WIDTH-FONT_WIDTH*3, conf.histo_pos.y-FONT_HEIGHT, "EXP", hc);
            histogram_drawn |= HDRAW_EXP;
        } else if (histogram_drawn & HDRAW_EXP) {
            draw_rectangle(conf.histo_pos.x+HISTO_WIDTH-FONT_WIDTH*3, conf.histo_pos.y-FONT_HEIGHT, 
                           conf.histo_pos.x+HISTO_WIDTH, conf.histo_pos.y-1, MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT), RECT_BORDER0|DRAW_FILLED);
            histogram_drawn ^= HDRAW_EXP;
        }

        if (conf.histo_auto_ajust) {
            if (is_osd_edit) histo_magnification = 999;
            if (histo_magnification) {
                char osd_buf[20];
                sprintf(osd_buf, " %d.%02dx ", histo_magnification/100, histo_magnification%100);
                draw_string(conf.histo_pos.x, conf.histo_pos.y-FONT_HEIGHT, osd_buf, hc);
                histogram_drawn |= HDRAW_MAG;
            } else if (histogram_drawn & HDRAW_MAG) {
                draw_rectangle(conf.histo_pos.x, conf.histo_pos.y-FONT_HEIGHT, 
                               conf.histo_pos.x+8*FONT_WIDTH, conf.histo_pos.y-1, MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT), RECT_BORDER0|DRAW_FILLED);
                histogram_drawn ^= HDRAW_MAG;
            }
        }
    }
    else if (histogram_drawn & ~HDRAW_READY)
    {
        // Erase all histogram elements including magnification and 'EXP' warning. Not needed in osd editor
        if (!is_osd_edit)
            draw_rectangle(conf.histo_pos.x, conf.histo_pos.y-FONT_HEIGHT, 
                           conf.histo_pos.x+HISTO_WIDTH+2, conf.histo_pos.y+(histogram_drawn&0xFFF), MAKE_COLOR(COLOR_TRANSPARENT, COLOR_TRANSPARENT), RECT_BORDER0|DRAW_FILLED);
        histogram_drawn = 0;
    }
}

// =========  MODULE INIT =================

/***************** BEGIN OF AUXILARY PART *********************
  ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
 **************************************************************/

//---------------------------------------------------------
// PURPOSE: Finalize module operations (close allocs, etc)
// RETURN VALUE: 0-ok, 1-fail
//---------------------------------------------------------
int _module_unloader()
{
    // Free allocated memory
    if (histogram_proc[0] != 0)
    {
        free(histogram_proc[0]);
        histogram_proc[0] = 0;
    }
    return 0;
}

int _module_can_unload()
{
    // Unload if turned off and histogram has been erased.
    return (conf.show_histo == 0) && ((histogram_drawn & ~HDRAW_READY) == 0);
}

/******************** Module Information structure ******************/

libhisto_sym _libhisto =
{
    {
         0, _module_unloader, _module_can_unload, 0, 0
    },

    histogram_process,
    gui_osd_draw_histo
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    HISTO_VERSION,              // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,           // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int32_t)"Histogram Overlay (dll)",
    MTYPE_EXTENSION,

    &_libhisto.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    CAM_INFO_VERSION,           // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
