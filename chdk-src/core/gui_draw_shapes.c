
// Geometric shapes and buttons
// File included into gui_draw.c

//-------------------------------------------------------------------

// Lines

void draw_line(coord x1, coord y1, coord x2, coord y2, color cl)
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
     int ystep = (y1 < y2)?1:-1;
     int x;
     for (x=x1; x<=x2; ++x)
     {
         if (steep) draw_pixel(y, x, cl);
         else draw_pixel(x, y, cl);
         error += deltay;
         if ((error<<1) >= deltax)
         {
             y += ystep;
             error -= deltax;
         }
     }
}

void draw_hline(coord x, coord y, int len, color cl)
{
    if (draw_grid_clip)
    {
        int i;
        for (i=0; i<len; i++) draw_pixel(x+i, y, cl);
        return;
    }
    if ((y < 0) || (x >= camera_screen.width) || (y >= camera_screen.height)) return;
    if (x < 0) { len += x; x = 0; }
    if ((x + len) > camera_screen.width) len = camera_screen.width - x;
#ifndef CAM_HAS_DISPLAY_REFRESH_FLAG
    if (conf.rotate_osd)
    {
        if ((y == camera_screen.height-1) && ((x+len) >= camera_screen.width-1)) { x--; len--; }   // Skip guard pixel
    }
    else
    {
        if ((y == 0) && (x == 0)) { x++; len--; }   // Skip guard pixel
    }
#endif
    register unsigned int offset = y * camera_screen.buffer_width + ASPECT_XCORRECTION(x);
    len = ASPECT_XCORRECTION(len);      // Scale the line length if needed
    for (; len>0; len--, offset++)
        draw_pixel_proc(offset, cl);
}

void draw_vline(coord x, coord y, int len, color cl)
{
    if (draw_grid_clip)
    {
        int i;
        for (i=0; i<len; i++) draw_pixel(x, y+i, cl);
        return;
    }
    if ((x < 0) || (x >= camera_screen.width) || (y >= camera_screen.height)) return;
    if (y < 0) { len += y; y = 0; }
    if ((y + len) > camera_screen.height) len = camera_screen.height - y;
    register unsigned int offset = y * camera_screen.buffer_width + ASPECT_XCORRECTION(x);
    for (; len>0; len--, offset+=camera_screen.buffer_width)
    {
        draw_pixel_proc(offset, cl);
#if CAM_USES_ASPECT_CORRECTION
        draw_pixel_proc(offset+1, cl);  // Draw second pixel if screen scaling is needed
#endif
    }
}

//-------------------------------------------------------------------

// Generic rectangle
// 'flags' defines type - filled, round corners, shadow and border thickness
void draw_rectangle(coord x1, coord y1, coord x2, coord y2, twoColors cl, int flags)
{
    // Normalise values
    if (x1 > x2)
        swap(x1, x2);
    if (y1 > y2)
        swap(y1, y2);

    // Check if completely off screen
    if ((x2 < 0) || (y2 < 0) || (x1 >= camera_screen.width) || (y1 >= camera_screen.height))
        return;

    int round = (flags & RECT_ROUND_CORNERS) ? 1 : 0;
    int thickness;
    int i;

    // Shadow (do this first, as edge draw shrinks rectangle for fill)
    if (flags & RECT_SHADOW_MASK)
    {
        thickness = ((flags & RECT_SHADOW_MASK) >> 4);
        for (i=1; i<=thickness; i++)
        {
            draw_vline(x2+i, y1+1, y2 - y1, COLOR_BLACK);
            draw_hline(x1+1, y2+i, x2 - x1 + thickness, COLOR_BLACK);
        }
    }

    // Edge
    thickness = flags & RECT_BORDER_MASK;
    for (i=0; i<thickness; i++)
    {
        // Clipping done in draw_hline and draw_vline
        draw_vline(x1, y1 + round * 2, y2 - y1 - round * 4 + 1, FG_COLOR(cl));
        draw_vline(x2, y1 + round * 2, y2 - y1 - round * 4 + 1, FG_COLOR(cl));
        draw_hline(x1 + 1 + round, y1, x2 - x1 - round * 2 - 1, FG_COLOR(cl));
        draw_hline(x1 + 1 + round, y2, x2 - x1 - round * 2 - 1, FG_COLOR(cl));

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
        if (x2 >= camera_screen.width)  x2 = camera_screen.width - 1;
        if (y2 >= camera_screen.height) y2 = camera_screen.height - 1;

        coord y;
        for (y = y1; y <= y2; ++y)
            draw_hline(x1, y, x2 - x1 + 1, BG_COLOR(cl));
    }
}

//-------------------------------------------------------------------

// *** Not used ***
//void draw_circle(coord x, coord y, const unsigned int r, color cl)
//{
//    int dx = 0;
//    int dy = r;
//    int p=(3-(r<<1));
//
//    do {
//        draw_pixel((x+dx),(y+dy),cl);
//        draw_pixel((x+dy),(y+dx),cl);
//        draw_pixel((x+dy),(y-dx),cl);
//        draw_pixel((x+dx),(y-dy),cl);
//        draw_pixel((x-dx),(y-dy),cl);
//        draw_pixel((x-dy),(y-dx),cl);
//        draw_pixel((x-dy),(y+dx),cl);
//        draw_pixel((x-dx),(y+dy),cl);
//
//        ++dx;
//
//        if (p<0)
//            p += ((dx<<2)+6);
//        else {
//            --dy;
//            p += (((dx-dy)<<2)+10);
//        }
//    } while (dx<=dy);
//}

//-------------------------------------------------------------------

void draw_ellipse(coord CX, coord CY, unsigned int XRadius, unsigned int YRadius, color cl, int flags)
{
    // Bresenham fast ellipse algorithm - http://homepage.smc.edu/kennedy_john/BELIPSE.PDF
    int X, Y;
    int XChange, YChange;
    int EllipseError;
    int TwoASquare, TwoBSquare;
    int StoppingX, StoppingY;
    TwoASquare = 2*XRadius*XRadius;
    TwoBSquare = 2*YRadius*YRadius;
    X = XRadius;
    Y = 0;
    XChange = YRadius*YRadius*(1-2*XRadius);
    YChange = XRadius*XRadius;
    EllipseError = 0;
    StoppingX = TwoBSquare*XRadius;
    StoppingY = 0;
    while ( StoppingX >= StoppingY ) 
    {
        if (flags & DRAW_FILLED)
        {
            draw_hline(CX-X,CY-Y,X*2+1,cl);
            draw_hline(CX-X,CY+Y,X*2+1,cl);
        }
        else
        {
            draw_pixel(CX-X,CY-Y,cl);
            draw_pixel(CX-X,CY+Y,cl);
            draw_pixel(CX+X,CY-Y,cl);
            draw_pixel(CX+X,CY+Y,cl);
        }
        Y++;
        StoppingY += TwoASquare;
        EllipseError += YChange;
        YChange += TwoASquare;
        if ((2*EllipseError + XChange) > 0 )
        {
            X--;
            StoppingX -= TwoBSquare;
            EllipseError += XChange;
            XChange += TwoBSquare;
        }
    }
    X = 0;
    Y = YRadius;
    XChange = YRadius*YRadius;
    YChange = XRadius*XRadius*(1-2*YRadius);
    EllipseError = 0;
    StoppingX = 0;
    StoppingY = TwoASquare*YRadius;
    int lastY = Y + 1;
    while ( StoppingX <= StoppingY )
    {
        if (flags & DRAW_FILLED)
        {
            // Only draw lines if Y has changed
            if (lastY != Y)
            {
                draw_hline(CX-X,CY-Y,X*2+1,cl);
                draw_hline(CX-X,CY+Y,X*2+1,cl);
                lastY = Y;
            }
        }
        else
        {
            draw_pixel(CX-X,CY-Y,cl);
            draw_pixel(CX-X,CY+Y,cl);
            draw_pixel(CX+X,CY-Y,cl);
            draw_pixel(CX+X,CY+Y,cl);
        }
        X++;
        StoppingX += TwoBSquare;
        EllipseError += XChange;
        XChange += TwoBSquare;
        if ((2*EllipseError + YChange) > 0 )
        {
            Y--;
            StoppingY -= TwoASquare;
            EllipseError += YChange;
            YChange += TwoASquare;
        }
    }
}

//-------------------------------------------------------------------

// Draw a button
void draw_button(int x, int y, int w, int str_id, int active)
{
    twoColors cl = MAKE_COLOR((active) ? COLOR_RED : COLOR_BLACK, COLOR_WHITE);
    w = w * FONT_WIDTH;

    draw_rectangle(x-2, y-2, x+w+2, y+FONT_HEIGHT+2, cl, RECT_BORDER1|DRAW_FILLED|RECT_SHADOW1);     // main box
    draw_string(x+((w-(strlen(lang_str(str_id))*FONT_WIDTH))>>1), y, lang_str(str_id), cl);
}

//-------------------------------------------------------------------
