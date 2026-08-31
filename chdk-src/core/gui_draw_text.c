
// Text drawing
// File included into gui_draw.c

//-------------------------------------------------------------------
// String & text functions

// Draw a single line string up to a maximum pixel width
int draw_string_clipped(coord x, coord y, const char *s, twoColors cl, int max_width)
{
    while (*s && (*s != '\n') && (max_width >= FONT_WIDTH))
    {
        draw_char(x, y, *s, cl);
        s++;
        max_width -= FONT_WIDTH;
        x += FONT_WIDTH;
        if ((x>=camera_screen.width) && (*s))
        {
            draw_char(x-FONT_WIDTH,y, '>', cl);
            break;
        }
    }
    return x;
}

// Draw a single line string
int draw_string(coord x, coord y, const char *s, twoColors cl)
{
    return draw_string_clipped(x, y, s, cl, camera_screen.width);
}

// Draw a single line string:
//      - xo = left offset to start text (only applies to left justified text)
//      - max_width = maximum pixel width to use (staring from x)
//      - justification = left, center or right justified, also controls if unused area to be filled with background color
// Returns x position of first character drawn
int draw_string_justified(coord x, coord y, const char *s, twoColors cl, int xo, int max_width, int justification)
{
    // Get length in pixels
    const char *e = strchr(s, '\n');
    int l;
    if (e)
        l = (e - s) * FONT_WIDTH;
    else
        l = strlen(s) * FONT_WIDTH;
    if (l > max_width) l = max_width;

    // Calculate justification offset
    switch (justification & 0xF)
    {
    case TEXT_RIGHT:
        xo = (max_width - l);
        break;
    case TEXT_CENTER:
        xo = ((max_width - l) >> 1);
        break;
    }

    // Fill left side
    if ((justification & TEXT_FILL) && (xo > 0))
        draw_rectangle(x, y, x+xo-1, y+FONT_HEIGHT-1, cl, RECT_BORDER0|DRAW_FILLED);

    // Draw string (get length drawn in pixels)
    l = draw_string_clipped(x+xo, y, s, cl, max_width - xo) - x;

    // Fill right side
    if ((justification & TEXT_FILL) && (l < max_width))
        draw_rectangle(x+l, y, x+max_width-1, y+FONT_HEIGHT-1, cl, RECT_BORDER0|DRAW_FILLED);

    // Return start of first character
    return x+xo;
}

// Calculate the max line length and number of lines of a multi line string
// Lines are separated by newline '\n' characters
// Returns:
//      - max line length (return value)
//      - number of lines (in *max_lines)
int text_dimensions(const char *s, int width, int max_chars, int *max_lines)
{
    int l = 0, n;
    while (s && *s && (l < *max_lines))
    {
        const char *e = strchr(s, '\n');
        if (e)
        {
            n = e - s;
            e++;
        }
        else
        {
            n = strlen(s);
        }

        if (n > width) width = n;

        s = e;
        l++;
    }
    *max_lines = l;
    if (width > max_chars) width = max_chars;
    return width;
}

// Draw multi-line text string:
//      - max_chars = max # of chars to draw
//      - max_lines = max # of lines to draw
//      - justification = left, center or right justified, with optional fill of unused space
// Returns x position of first character on last line
int draw_text_justified(coord x, coord y, const char *s, twoColors cl, int max_chars, int max_lines, int justification)
{
    int rx = 0;
    while (s && *s && (max_lines > 0))
    {
        const char *e = strchr(s, '\n');
        if (e) e++;

        rx = draw_string_justified(x, y, s, cl, 0, max_chars*FONT_WIDTH, justification);

        s = e;
        y += FONT_HEIGHT;
        max_lines--;
    }
    return rx;
}

//-------------------------------------------------------------------

// Draw single line string at 'character' screen position (row, col)
// Pixel co-ordinate conversion --> x = col * FONT_WIDTH, y = row * FONT_HEIGHT
void draw_txt_string(coord col, coord row, const char *str, twoColors cl)
{
    draw_string(col*FONT_WIDTH, row*FONT_HEIGHT, str, cl);
}

//-------------------------------------------------------------------

// Draw text message and % progress indicator
void draw_progress_bar(const char* msg, const unsigned int perc)
{
    coord x=60, y=100;
    unsigned int w=240, h=40;

    draw_rectangle(x, y, x+w, y+h, MAKE_COLOR(COLOR_GREY, COLOR_WHITE), RECT_BORDER1|DRAW_FILLED|RECT_SHADOW3); // main box
    draw_string_justified(x, y+2, msg, MAKE_COLOR(COLOR_GREY, COLOR_WHITE), 0, w, TEXT_CENTER); //title text
    draw_rectangle(x+10, y+4+FONT_HEIGHT, x+w-10, y+h-10, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE), RECT_BORDER1|DRAW_FILLED); // progress rect
    draw_rectangle(x+11, y+5+FONT_HEIGHT, x+11+(w-22)*perc/100, y+h-11, MAKE_COLOR(COLOR_RED, COLOR_RED), RECT_BORDER0|DRAW_FILLED); // progress bar
}

//-------------------------------------------------------------------

// See gui_draw_yuv.c for the YUV versions of these functions which also support a 1/2 scale drawing option.

#if defined(CAM_DRAW_RGBA) || defined(CAM_DRAW_8BIT) 

// Draw single line string, with optional X and Y scaling
void draw_string_scaled(coord x, coord y, const char *s, twoColors cl, int xsize, int ysize)
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

// Draw CHDK OSD string at user defined position and scale
void draw_osd_string(OSD_pos pos, int xo, int yo, const char *s, twoColors c, OSD_scale scale)
{
    if ((scale.x == 0) || (scale.y == 0) || ((scale.x == 1) && (scale.y == 1)))
        draw_string(pos.x+xo, pos.y+yo, s, c);
    else
        draw_string_scaled(pos.x+(xo*scale.x), pos.y+(yo*scale.y), s, c, scale.x, scale.y);
}

#endif

//-------------------------------------------------------------------
