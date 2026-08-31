
// Low level routines that are highly dependant on hardware drawing method  - YUV drawing system
// File selectively included into gui_draw.c

//-------------------------------------------------------------------

// Pixel drawing

static void draw_pixel_std(unsigned int offset, color cl)
{
    // DIGIC 6, drawing on 16bpp YUV overlay
    unsigned int y;
    unsigned int o;
    CALC_YUV_LUMA_OPACITY_FOR_COLOR(cl,y,o);
    unsigned int u;
    unsigned int v;
    CALC_YUV_CHROMA_FOR_COLOR(cl,u,v);

#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    int active_buffer_index =  active_bitmap_buffer & 1;
    unsigned char *obu = (unsigned char *)(&opacity_buffer[active_buffer_index][0]);
    unsigned char *bbu = (unsigned char *)(&bitmap_buffer[active_buffer_index][0]);
    obu[offset] = o;
    register unsigned int offs2 = (offset>>1)<<2;
    if (offset&1) // x is odd
    {
        bbu[offs2+3] = y; // Y
    }
    else // x is even
    {
        bbu[offs2+1] = y; // Y
    }
    bbu[offs2+0] = u; // U?
    bbu[offs2+2] = v; // V?
#else
    opacity_buffer[0][offset] = o;
    opacity_buffer[1][offset] = o;
    register unsigned int offs2 = (offset>>1)<<2;
    if (offset&1) // x is odd
    {
        (bitmap_buffer[0])[offs2+3] = y; // Y
        (bitmap_buffer[1])[offs2+3] = y; // Y
    }
    else // x is even
    {
        (bitmap_buffer[0])[offs2+1] = y; // Y
        (bitmap_buffer[1])[offs2+1] = y; // Y
    }
    (bitmap_buffer[0])[offs2+0] = u; // U?
    (bitmap_buffer[1])[offs2+0] = u; // U?
    (bitmap_buffer[0])[offs2+2] = v; // V?
    (bitmap_buffer[1])[offs2+2] = v; // V?
#endif
}

//-------------------------------------------------------------------

// Guard pixel

void draw_set_guard()
{
#ifndef CAM_HAS_DISPLAY_REFRESH_FLAG
    opacity_buffer[active_bitmap_buffer][0] = 0x42;
#endif
}

int draw_test_guard()
{
#ifdef CAM_HAS_DISPLAY_REFRESH_FLAG
    extern int display_needs_refresh;
    if (display_needs_refresh)
    {
        display_needs_refresh = 0;
        return 0;
    }
#else
    if (opacity_buffer[active_bitmap_buffer][0] != 0x42) return 0;
#endif
    return 1;
}

//-------------------------------------------------------------------

// pixel drawing functions for YUV, meant to be optimized for greater speed
// drawing is done directly, drawing function replacement is not supported
// OSD rotation is respected

#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
static unsigned char *current_opacity_buf;
static unsigned char *current_bitmap_buf;
#endif
static unsigned char yuvclr[8]; // order of bytes: background u,y,v,opacity; foreground u,y,v,opacity

// sets up decoded colors and buffer addresses for the following drawing operation
static void draw_pixel_simple_start(twoColors tc)
{
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    int active_buffer_index =  active_bitmap_buffer & 1;
    current_opacity_buf = (unsigned char *)(&opacity_buffer[active_buffer_index][0]);
    current_bitmap_buf = (unsigned char *)(&bitmap_buffer[active_buffer_index][0]);
#endif
    color cl;
    cl = BG_COLOR(tc);
    CALC_YUV_LUMA_OPACITY_FOR_COLOR(cl,yuvclr[1],yuvclr[3]);
    CALC_YUV_CHROMA_FOR_COLOR(cl,yuvclr[0],yuvclr[2]);
    cl = FG_COLOR(tc);
    CALC_YUV_LUMA_OPACITY_FOR_COLOR(cl,yuvclr[5],yuvclr[7]);
    CALC_YUV_CHROMA_FOR_COLOR(cl,yuvclr[4],yuvclr[6]);
}

// px: bit0 unset for background, set for foreground
static void draw_1pixel_simple(coord x, coord y, int px, int vrepeat)
{
    if ((x < 0) || (y < 0) || (x >= camera_screen.width) || (y+vrepeat >= camera_screen.height)/* || ((x == 0) && (y == 0))*/) return;
    unsigned int offset = y * camera_screen.buffer_width + x;
    int plus = camera_screen.buffer_width;
    if (conf.rotate_osd)
    {
        offset = rotate_base - offset;
        plus = -plus;
    }

#ifndef CAM_HAS_DISPLAY_REFRESH_FLAG
    if (!offset) return; // skip guard pixel
#endif

    int fg = px<<2;
    register unsigned int offs2 = (offset>>1)<<2;

#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    unsigned char *obu = current_opacity_buf;
    unsigned char *bbu = current_bitmap_buf;
#endif

    if (offset&1) // x is odd
    {
        while (1)
        {
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
            obu[offset] = yuvclr[fg+3];
            bbu[offs2+3] = yuvclr[fg+1]; // Y
#else
            opacity_buffer[0][offset] = yuvclr[fg+3];
            bitmap_buffer[0][offs2+3] = yuvclr[fg+1]; // Y
            opacity_buffer[1][offset] = yuvclr[fg+3];
            bitmap_buffer[1][offs2+3] = yuvclr[fg+1]; // Y
#endif
            if (!vrepeat) return;
            vrepeat--;
            offset += plus;
            offs2 += (plus<<1);
        }
    }
    else // x is even
    {
        while (1)
        {
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
            obu[offset] = yuvclr[fg+3];
            bbu[offs2+1] = yuvclr[fg+1]; // Y
            bbu[offs2+0] = yuvclr[fg]; // U
            bbu[offs2+2] = yuvclr[fg+2]; // V
#else
            opacity_buffer[0][offset] = yuvclr[fg+3];
            bitmap_buffer[0][offs2+1] = yuvclr[fg+1]; // Y
            bitmap_buffer[0][offs2+0] = yuvclr[fg]; // U
            bitmap_buffer[0][offs2+2] = yuvclr[fg+2]; // V
            opacity_buffer[1][offset] = yuvclr[fg+3];
            bitmap_buffer[1][offs2+1] = yuvclr[fg+1]; // Y
            bitmap_buffer[1][offs2+0] = yuvclr[fg]; // U
            bitmap_buffer[1][offs2+2] = yuvclr[fg+2]; // V
#endif
            if (!vrepeat) return;
            vrepeat--;
            offset += plus;
            offs2 += (plus<<1);
        }
    }
}

// px: 2 bits, describing 2 pixels (foreground when set, background when not), LSB is the second pixel
// first pixel specifies chroma
static void draw_2pixels_simple(coord x, coord y, int px, int vrepeat)
{
    if ((x < 0) || (y < 0) || (x+1 >= camera_screen.width) || (y+vrepeat >= camera_screen.height)/* || ((x == 0) && (y == 0))*/) return;
    unsigned int y1, y2;
    unsigned int offset = (y * camera_screen.buffer_width + x)>>1;
    int plus;
    unsigned short co;
    unsigned int yuv;
    if (conf.rotate_osd)
    {
        offset = (rotate_base>>1) - offset;
        plus = -(camera_screen.buffer_width>>1);
        y1 = px&1?4:0;
        y2 = px&2?4+1:0+1;
    }
    else
    {
        plus = camera_screen.buffer_width>>1;
        y1 = px&2?4:0;
        y2 = px&1?4+1:0+1;
    }

#ifndef CAM_HAS_DISPLAY_REFRESH_FLAG
    if (!offset) return; // skip guard pixel
#endif

    co = yuvclr[y1+3]+(yuvclr[y1+3]<<8);
    yuv = (*(unsigned int*)(&yuvclr[y1]) & 0xffffff) + (yuvclr[y2]<<24);
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    unsigned short *obu = (unsigned short *)current_opacity_buf;
    unsigned int *bbu = (unsigned int *)current_bitmap_buf;
#endif
    while (1)
    {
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
        obu[offset] = co;
        bbu[offset] = yuv;
#else
        ((unsigned short *)opacity_buffer[0])[offset] = co;
        ((unsigned int *)bitmap_buffer[0])[offset] = yuv;
        ((unsigned short *)opacity_buffer[1])[offset] = co;
        ((unsigned int *)bitmap_buffer[1])[offset] = yuv;
#endif
        if (!vrepeat) return;
        vrepeat--;
        offset += plus;
    }
}

void draw_hline_simple(coord x, coord y, int len, int px)
{
    if ((y < 0) || (x >= camera_screen.width) || (y >= camera_screen.height)) return;
    if (x < 0) { len += x; x = 0; }
    if ((x + len) > camera_screen.width) len = camera_screen.width - x;

    register unsigned int offset = y * camera_screen.buffer_width + (x);
    if (conf.rotate_osd)
    {
        offset = rotate_base - offset - len;
    }

#ifndef CAM_HAS_DISPLAY_REFRESH_FLAG
    // Skip guard pixel
    if (!offset)
    {
        offset++;
        len--;
    }
#endif

    int fg = px<<2;
    if (offset & 1)
    {
        register unsigned int offs2 = (offset>>1)<<2;
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
        unsigned char *obu = current_opacity_buf;
        unsigned char *bbu = current_bitmap_buf;
        obu[offset] = yuvclr[fg+3];
        bbu[offs2+3] = yuvclr[fg+1]; // Y
#else
        opacity_buffer[0][offset] = yuvclr[fg+3];
        bitmap_buffer[0][offs2+3] = yuvclr[fg+1]; // Y
        opacity_buffer[1][offset] = yuvclr[fg+3];
        bitmap_buffer[1][offs2+3] = yuvclr[fg+1]; // Y
#endif
        offset++;
        len--;
    }
    unsigned short co = yuvclr[fg+3]+(yuvclr[fg+3]<<8);
    unsigned int yuv = (*(unsigned int*)(&yuvclr[fg]) & 0xffffff) + (yuvclr[fg+1]<<24);
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    unsigned short *obud = (unsigned short *)current_opacity_buf;
    unsigned int *bbud = (unsigned int *)current_bitmap_buf;
    for (; len>0; len-=2, offset+=2)
    {
        obud[offset>>1] = co;
        bbud[offset>>1] = yuv;
    }
#else
    for (; len>0; len-=2, offset+=2)
    {
        ((unsigned short *)opacity_buffer[0])[offset>>1] = co;
        ((unsigned int *)bitmap_buffer[0])[offset>>1] = yuv;
        ((unsigned short *)opacity_buffer[1])[offset>>1] = co;
        ((unsigned int *)bitmap_buffer[1])[offset>>1] = yuv;
    }
#endif
    if (len == -1)
    {
        offset--;
        register unsigned int offs2 = (offset>>1)<<2;
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
        unsigned char *obu = current_opacity_buf;
        unsigned char *bbu = current_bitmap_buf;
        obu[offset] = yuvclr[fg+3];
        bbu[offs2+3] = yuvclr[fg+1]; // Y
#else
        opacity_buffer[0][offset] = yuvclr[fg+3];
        bitmap_buffer[0][offs2+3] = yuvclr[fg+1]; // Y
        opacity_buffer[1][offset] = yuvclr[fg+3];
        bitmap_buffer[1][offs2+3] = yuvclr[fg+1]; // Y
#endif
    }
}

// DIGIC 6: "optimizations" to improve speed
void draw_char(coord x, coord y, const char ch, twoColors cl)
{
    unsigned i, ii;

    unsigned int offset, size;
    unsigned char *sym = get_cdata(&offset, &size, ch);
    unsigned int fw = FONT_WIDTH;

    draw_pixel_simple_start(cl);

    // First draw blank lines at top
    for (i=0; i<offset; i++)
    {
        int j;
        j = i<<1;
        draw_hline_simple(x, y+j, fw, 0);
        draw_hline_simple(x, y+j+1, fw, 0);
    }

    // Now draw character data

    unsigned j;
    for (j=i; i<size;)
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

        {
            // upscaling 8 pixels to 14
            ii = 0;
            ii += (dsym&1)?3:0;
            ii += (dsym&2)?4:0;
            ii += (dsym&4)?0x18:0;
            ii += (dsym&8)?0x60:0;
            ii += (dsym&16)?0x180:0;
            ii += (dsym&32)?0x600:0;
            ii += (dsym&64)?0x1800:0;
            ii += (dsym&128)?0x2000:0;
            dsym = ii;
        }

        for (; rep > 0; rep--)
        {
            unsigned int px;
            unsigned int yt = y+(i<<1);
            ii = 0;
            if (x&1)
            {
                draw_1pixel_simple(x+ii, yt, dsym>>(fw-1), 1);
                ii++;
            }
            for (; ii<fw; ii+=2)
            {
                px = (dsym & ((3<<(fw-2))>>ii))>>(fw-2-ii);
                draw_2pixels_simple(x+ii, yt, px, 1);
            }
            if (x&1)
            {
                draw_1pixel_simple(x+ii-1, yt, dsym&1, 1);
            }
            i++;
        }
    }

    // Last draw blank lines at bottom
    for (; i<FONT_REAL_HEIGHT; i++)
    {
        int j;
        j = i<<1;
        draw_hline_simple(x, y+j, fw, 0);
        draw_hline_simple(x, y+j+1, fw, 0);
    }
}

void draw_char_unscaled(coord x, coord y, const char ch, twoColors cl)
{
    unsigned i, ii;

    unsigned int offset, size;
    unsigned char *sym = get_cdata(&offset, &size, ch);
    unsigned int fw = FONT_REAL_WIDTH;

    draw_pixel_simple_start(cl);

    // First draw blank lines at top
    for (i=0; i<offset; i++)
    {
        draw_hline_simple(x, y+i, fw, 0);
    }

    // Now draw character data

    unsigned j;
    for (j=i; i<size;)
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
            unsigned int px;
            unsigned int yt = y+(i);
            ii = 0;
            if (x&1)
            {
                draw_1pixel_simple(x+ii, yt, dsym>>(fw-1), 0);
                ii++;
            }
            for (; ii<fw; ii+=2)
            {
                px = (dsym & ((3<<(fw-2))>>ii))>>(fw-2-ii);
                draw_2pixels_simple(x+ii, yt, px, 0);
            }
            if (x&1)
            {
                draw_1pixel_simple(x+ii-1, yt, dsym&1, 0);
            }
            i++;
        }
    }

    // Last draw blank lines at bottom
    for (; i<FONT_REAL_HEIGHT; i++)
    {
        draw_hline_simple(x, y+i, fw, 0);
    }
}

void draw_char_scaled(coord x, coord y, const char ch, twoColors cl, int xsize, int ysize)
{
    unsigned i, ii;

    twoColors clf = MAKE_COLOR(FG_COLOR(cl),FG_COLOR(cl));
    twoColors clb = MAKE_COLOR(BG_COLOR(cl),BG_COLOR(cl));

    unsigned int offset, size;
    unsigned char *sym = get_cdata(&offset, &size, ch);

    ysize <<= 1;

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
        unsigned int lastmask = 1 << (FONT_WIDTH-1);

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

        {
            // upscaling 8 pixels to 14
            ii = 0;
            ii += (dsym&1)?3:0;
            ii += (dsym&2)?4:0;
            ii += (dsym&4)?0x18:0;
            ii += (dsym&8)?0x60:0;
            ii += (dsym&16)?0x180:0;
            ii += (dsym&32)?0x600:0;
            ii += (dsym&64)?0x1800:0;
            ii += (dsym&128)?0x2000:0;
            dsym = ii;
        }

        last = dsym & lastmask;
        len = 1;
        for (ii=1; ii<FONT_WIDTH; ii++)
        {
            if (((dsym << ii) & lastmask) != last)
            {
                draw_rectangle(x+(ii-len)*xsize,y+i*ysize,x+ii*xsize-1,y+i*ysize+rep*ysize-1,(last)?clf:clb,RECT_BORDER0|DRAW_FILLED);
                last = (dsym << ii) & lastmask;
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
    if (i < FONT_REAL_HEIGHT)
        draw_rectangle(x,y+i*ysize,x+FONT_WIDTH*xsize-1,y+FONT_REAL_HEIGHT*ysize-1,clb,RECT_BORDER0|DRAW_FILLED);
}

//-------------------------------------------------------------------

// Draw single line string, with optional X and Y scaling
void draw_string_scaled(coord x, coord y, const char *s, twoColors cl, int xsize, int ysize)
{
    if ((xsize==0) || (ysize==0))
    {
        while (*s && (*s != '\n'))
        {
            draw_char_unscaled(x, y, *s, cl);
            s++;
            x+=FONT_REAL_WIDTH;
            if ((x>=camera_screen.width) && (*s))
            {
                draw_char_unscaled(x-FONT_REAL_WIDTH,y, '>', cl);
                break;
            }
        }
    }
    else
    {
        while (*s && (*s != '\n'))
        {
            draw_char_scaled(x, y, *s, cl, xsize, ysize);
            s++;
            x+=FONT_WIDTH*xsize;
            if ((x>=camera_screen.width) && (*s))
            {
                draw_char_scaled(x-FONT_WIDTH*xsize,y, '>', cl, xsize, ysize);
                break;
            }
        }
    }
}

// Draw CHDK OSD string at user defined position and scale
void draw_osd_string(OSD_pos pos, int xo, int yo, char *s, twoColors c, OSD_scale scale)
{
    if ((scale.x == 1) && (scale.y == 1))
    {
        draw_string(pos.x+xo, pos.y+yo, s, c);
    }
    else if ((scale.x == 0) || (scale.y == 0))
    {
        draw_string_scaled(pos.x+(xo>>1), pos.y+(yo>>1), s, c, scale.x, scale.y);
    }
    else
    {
        draw_string_scaled(pos.x+(xo*scale.x), pos.y+(yo*scale.y), s, c, scale.x, scale.y);
    }
}

//-------------------------------------------------------------------

// Draw line scaled x2 in both X and Y co-ords. Used for drawing icons on high res screens
void draw_line_x2(coord x1, coord y1, coord x2, coord y2, color cl)
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
    int deltax = x2 - x1;
    int deltay = abs(y2 - y1);
    int error = 0;
    int y = y1;
    int ystep = (y1 < y2)?2:-2;
    int x;
    for (x=x1; x<=x2; x+=2)
    {
        if (steep) {
            draw_pixel(y, x, cl);
            draw_pixel(y+1, x, cl);
            draw_pixel(y, x+1, cl);
            draw_pixel(y+1, x+1, cl);
        } else {
            draw_pixel(x, y, cl);
            draw_pixel(x+1, y, cl);
            draw_pixel(x, y+1, cl);
            draw_pixel(x+1, y+1, cl);
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
// For CAM_DRAW_YUV scale up by 2 times and draw double thickness
void draw_icon_cmds(coord x, coord y, icon_cmd *cmds)
{
    int x1, y1, x2, y2;
    while (1)
    {
        x1 = cmds->x1<<1;
        y1 = cmds->y1<<1;
        x2 = cmds->x2<<1;
        y2 = cmds->y2<<1;
        color cf = chdk_colors[cmds->cf];       // Convert color indexes to actual colors
        color cb = chdk_colors[cmds->cb];
        switch (cmds->action)
        {
        default:
        case IA_END:
            return;
        case IA_HLINE:
            draw_hline(x+x1, y+y1, x2, cb);
            draw_hline(x+x1, y+y1+1, x2, cb);
            break;
        case IA_VLINE:
            draw_vline(x+x1, y+y1, y2, cb);
            draw_vline(x+x1+1, y+y1, y2, cb);
            break;
        case IA_LINE:
            draw_line_x2(x+x1, y+y1, x+x2, y+y2, cb);
            break;
        case IA_RECT:
            draw_rectangle(x+x1, y+y1, x+x2+1, y+y2+1, MAKE_COLOR(cb,cf), RECT_BORDER2);
            break;
        case IA_FILLED_RECT:
            draw_rectangle(x+x1, y+y1, x+x2+1, y+y2+1, MAKE_COLOR(cb,cf), RECT_BORDER2|DRAW_FILLED);
            break;
        case IA_ROUND_RECT:
            draw_rectangle(x+x1, y+y1, x+x2+1, y+y2+1, MAKE_COLOR(cb,cf), RECT_BORDER2|RECT_ROUND_CORNERS);
            break;
        case IA_FILLED_ROUND_RECT:
            draw_rectangle(x+x1, y+y1, x+x2+1, y+y2+1, MAKE_COLOR(cb,cf), RECT_BORDER2|DRAW_FILLED|RECT_ROUND_CORNERS);
            break;
        }
        cmds++;
    }
}

//-------------------------------------------------------------------
