
// Low level routines that are highly dependant on hardware drawing method  - RGBA drawing system
// File selectively included into gui_draw.c

//-------------------------------------------------------------------

// Pixel drawing

extern unsigned int*    chdk_rgba;
extern unsigned int     rgba_colors[];
extern unsigned char    yuv_colors[];
int                     display_needs_canon_refresh;

// Draw pixel on screen
static void draw_pixel_std(unsigned int offset, color cl)
{
    // DIGIC 6, drawing on 32bpp RGBA overlay
    // Clamp color index
    cl = cl & 31;
    if (chdk_rgba[offset] != rgba_colors[cl]) {
        chdk_rgba[offset] = rgba_colors[cl];
        display_needs_canon_refresh = 1;
    }
}

//-------------------------------------------------------------------

// Handle guard pixel

void draw_set_guard() {}    // Not used for RGBA drawing

int draw_test_guard()
{
    extern int display_needs_refresh;
    if (display_needs_refresh)
    {
        display_needs_refresh = 0;
        return 0;
    }
    return 1;
}

//-------------------------------------------------------------------
// For CAM_DRAW_RGBA, first draw the icon on a temporary buffer, then copy to the screen
// Prevents triggering unnecessary calls to vid_bitmap_refresh from areas where the icon overdraws itself with different colours.

extern void (*draw_pixel_proc)(unsigned int offset, color cl);

// Current largest icon size
#define IW  32
#define IH  20

static int mw, mh;

static void idraw_hline(color* ibuf, coord x, coord y, int len, color cl)
{
    if ((y < 0) || (x >= IW) || (y >= IH)) return;
    if (x < 0) { len += x; x = 0; }
    if ((x + len) > IW) len = IW - x;
    if (y >= mh) mh = y + 1;
    if ((x + len) > mw) mw = x + len;
    x = y * IW + x;
    for (; len>0; len--, x++)
        ibuf[x] = cl;
}

static void idraw_vline(color* ibuf, coord x, coord y, int len, color cl)
{
    if ((x < 0) || (x >= IW) || (y >= IH)) return;
    if (y < 0) { len += y; y = 0; }
    if ((y + len) > IH) len = IH - y;
    if (x >= mw) mw = x + 1;
    if ((y + len) > mh) mh = y + len;
    y = y * IW + x;
    for (; len>0; len--, y+=IW)
        ibuf[y] = cl;
}

// Generic rectangle
// 'flags' defines type - filled, round corners, shadow and border thickness
static void idraw_rectangle(color* ibuf, coord x1, coord y1, coord x2, coord y2, twoColors cl, int flags)
{
    // Normalise values
    if (x1 > x2)
        swap(x1, x2);
    if (y1 > y2)
        swap(y1, y2);

    // Check if completely off screen
    if ((x2 < 0) || (y2 < 0) || (x1 >= IW) || (y1 >= IH))
        return;

    int round = (flags & RECT_ROUND_CORNERS) ? 1 : 0;
    int thickness;
    int i;

    // Edge
    thickness = flags & RECT_BORDER_MASK;
    for (i=0; i<thickness; i++)
    {
        // Clipping done in draw_hline and draw_vline
        idraw_vline(ibuf, x1, y1 + round * 2, y2 - y1 - round * 4 + 1, FG_COLOR(cl));
        idraw_vline(ibuf, x2, y1 + round * 2, y2 - y1 - round * 4 + 1, FG_COLOR(cl));
        idraw_hline(ibuf, x1 + 1 + round, y1, x2 - x1 - round * 2 - 1, FG_COLOR(cl));
        idraw_hline(ibuf, x1 + 1 + round, y2, x2 - x1 - round * 2 - 1, FG_COLOR(cl));

        x1++; x2--;
        y1++; y2--;

        round = 0;
    }

    // Fill
    if (flags & DRAW_FILLED)
    {
        // Clip values
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 >= IW) x2 = IW - 1;
        if (y2 >= IH) y2 = IH - 1;

        coord y;
        for (y = y1; y <= y2; ++y)
            idraw_hline(ibuf, x1, y, x2 - x1 + 1, BG_COLOR(cl));
    }
}

static void idraw_line(color* ibuf, coord x1, coord y1, coord x2, coord y2, color cl)
{
    unsigned char steep = abs(y2 - y1) > abs(x2 - x1);
    if (steep)
    {
         swap(x1, y1);
         swap(x2, y2);
    }
    if (x1 > x2)
    {
         swap(x1, x2);
         swap(y1, y2);
    }
    if (x2 >= mw) { mw = x2 + 1; if (mw > IW) mw = IW; }
    if (y2 >= mh) { mh = y2 + 1; if (mh > IH) mh = IH; }
    int deltax = x2 - x1;
    int deltay = abs(y2 - y1);
    int error = 0;
    int y = y1;
    int ystep = (y1 < y2)?1:-1;
    int x;
    for (x=x1; x<=x2; ++x)
    {
        if ((x >= 0) && (x < IW) && (y >= 0) && (y < IH)) {
            if (steep)
                ibuf[x*IW+y] = cl;
            else
                ibuf[y*IW+x] = cl;
        }
        error += deltay;
        if ((error<<1) >= deltax)
        {
             y += ystep;
             error -= deltax;
        }
    }
}

// Draw an OSD icon from an array of actions
// For CAM_DRAW_RGBA draw on temp buffer first then copy to screen - prevents overdraw from triggering vid_bitmap_refresh unnecessarily.
void draw_icon_cmds(coord x, coord y, icon_cmd *cmds)
{
    if ((x >= camera_screen.width) || (y >= camera_screen.height)) return;

    color ibuf[IW*IH];

    memset(ibuf, 0, IW*IH);
    mw = 0; mh = 0;

    int x1, y1, x2, y2;
    int thickness = RECT_BORDER1;
    int done = 0;
    while (!done)
    {
        x1 = cmds->x1;
        y1 = cmds->y1;
        x2 = cmds->x2;
        y2 = cmds->y2;
        color cf = chdk_colors[cmds->cf];       // Convert color indexes to actual colors
        color cb = chdk_colors[cmds->cb];
        switch (cmds->action)
        {
        default:
        case IA_END:
            done = 1;
            break;
        case IA_HLINE:
            idraw_hline(ibuf, x1, y1, x2, cb);
            break;
        case IA_VLINE:
            idraw_vline(ibuf, x1, y1, y2, cb);
            break;
        case IA_LINE:
            idraw_line(ibuf, x1, y1, x2, y2, cb);
            break;
        case IA_RECT:
            idraw_rectangle(ibuf, x1, y1, x2, y2, MAKE_COLOR(cb,cf), thickness);
            break;
        case IA_FILLED_RECT:
            idraw_rectangle(ibuf, x1, y1, x2, y2, MAKE_COLOR(cb,cf), thickness|DRAW_FILLED);
            break;
        case IA_ROUND_RECT:
            idraw_rectangle(ibuf, x1, y1, x2, y2, MAKE_COLOR(cb,cf), thickness|RECT_ROUND_CORNERS);
            break;
        case IA_FILLED_ROUND_RECT:
            idraw_rectangle(ibuf, x1, y1, x2, y2, MAKE_COLOR(cb,cf), thickness|DRAW_FILLED|RECT_ROUND_CORNERS);
            break;
        }
        cmds++;
    }

    x1 = 0;
    y1 = 0;
    int w = mw;
    int h = mh;

    if (x < 0) {
        x1 = -x;
        w += x;
        x = 0;
        if (w <= 0) return;
    }

    if (y < 0) {
        y1 = -y;
        h += y;
        y = 0;
        if (h <= 0) return;
    }

    int src = y1 * IW + x1;
    int dst = y * camera_screen.buffer_width + x;
    for (; h > 0; h -= 1, src += IW, dst += camera_screen.buffer_width) {
        for (x = 0; x < w; x += 1) {
            draw_pixel_proc(dst+x, ibuf[src+x]);
        }
    }
}

//-------------------------------------------------------------------
