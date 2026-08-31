
// Low level routines that are highly dependant on hardware drawing method  - 8 bit indexed drawing system
// File selectively included into gui_draw.c

//-------------------------------------------------------------------

// Pixel drawing

#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
extern char* bitmap_buffer[];
extern int active_bitmap_buffer;
#else
static char* frame_buffer[2];
#endif

// Draw pixel on screen
static void draw_pixel_std(unsigned int offset, color cl)
{
    // drawing on 8bpp paletted overlay
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    bitmap_buffer[active_bitmap_buffer][offset] = cl;
#else
    frame_buffer[0][offset] = frame_buffer[1][offset] = cl;
#endif
}

//-------------------------------------------------------------------

// Handle guard pixel

#define GUARD_VAL   COLOR_GREY_DK

void draw_set_guard()
{
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    *((unsigned char*)(bitmap_buffer[0])) = GUARD_VAL;
    *((unsigned char*)(bitmap_buffer[1])) = GUARD_VAL;
#else
    *((unsigned char*)(frame_buffer[0])) = GUARD_VAL;
    *((unsigned char*)(frame_buffer[1])) = GUARD_VAL;
#endif
}

int draw_test_guard()
{
#ifdef DRAW_ON_ACTIVE_BITMAP_BUFFER_ONLY
    if (*((unsigned char*)(bitmap_buffer[active_bitmap_buffer])) != GUARD_VAL) return 0;
#else
    if (*((unsigned char*)(frame_buffer[0])) != GUARD_VAL) return 0;
    if (*((unsigned char*)(frame_buffer[1])) != GUARD_VAL) return 0;
#endif
    return 1;
}

//-------------------------------------------------------------------

// Draw an OSD icon from an array of actions
void draw_icon_cmds(coord x, coord y, icon_cmd *cmds)
{
    int x1, y1, x2, y2;
    while (1)
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
            return;
        case IA_HLINE:
            draw_hline(x+x1, y+y1, x2, cb);
            break;
        case IA_VLINE:
            draw_vline(x+x1, y+y1, y2, cb);
            break;
        case IA_LINE:
            draw_line(x+x1, y+y1, x+x2, y+y2, cb);
            break;
        case IA_RECT:
            draw_rectangle(x+x1, y+y1, x+x2, y+y2, MAKE_COLOR(cb,cf), RECT_BORDER1);
            break;
        case IA_FILLED_RECT:
            draw_rectangle(x+x1, y+y1, x+x2, y+y2, MAKE_COLOR(cb,cf), RECT_BORDER1|DRAW_FILLED);
            break;
        case IA_ROUND_RECT:
            draw_rectangle(x+x1, y+y1, x+x2, y+y2, MAKE_COLOR(cb,cf), RECT_BORDER1|RECT_ROUND_CORNERS);
            break;
        case IA_FILLED_ROUND_RECT:
            draw_rectangle(x+x1, y+y1, x+x2, y+y2, MAKE_COLOR(cb,cf), RECT_BORDER1|DRAW_FILLED|RECT_ROUND_CORNERS);
            break;
        }
        cmds++;
    }
}

//-------------------------------------------------------------------
