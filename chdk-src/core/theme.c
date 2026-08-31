//-------------------------------------------------------------------
// Themes - see include/theme.h for what a theme is and is not able to be.
//-------------------------------------------------------------------

#include "camera_info.h"
#include "conf.h"
#include "gui_draw.h"
#include "theme.h"

const char * const theme_names[THEME_COUNT] = {
    "Apple", "Vaporwave", "Frutiger Aero", "Terminal", "Gunmetal"
};

// One row per theme, one column per TC_* slot.
//
// The palette these pick from is twenty fixed entries and half of it is
// aliased - in record mode COLOR_MAGENTA and COLOR_YELLOW_LT are the same byte,
// and so are COLOR_CYAN and COLOR_BLUE_LT (see docs/OVERLAY_DESIGN.md).
//
// Orange and purple are not among the twenty, but they are in the hardware
// palette - PAL_ORANGE and PAL_PURPLE, read out of the ROM. See the note in
// include/theme.h for where they came from and how to check them.
//
// A theme is made by choosing *which* of the genuinely distinct entries carries
// each meaning, and by leaving the ones it does not want out entirely. That is
// most of what makes them look different from each other - Vaporwave never
// reaches for green, Aero never reaches for red, Gunmetal reaches for one hue
// at four brightnesses.
static const unsigned short theme_tab[THEME_COUNT][TC_N] = {
    // ACCENT          ACCENT2         TEXT          DIM
    // HILITE          HILITE_FG       EDIT          WARN
    // BAD             SPECIAL         LIGHT
    {   // Apple, and only the six stripes of the old logo:
        //
        //   Heavenly Green  #61bb46      Basic Red     #e03a3e
        //   My Sin          #fdb827      Dark Fuchsia  #963d97
        //   Orange Passion  #f5821f      German Blue   #009ddc
        //
        // No white and no grey, which is what this theme used for its text and
        // its disabled rows and which is what made it read as "the usual
        // colours plus a few stripes". Text is My Sin, disabled is Dark
        // Fuchsia - the darkest of the six and the one that recedes.
        //
        // My Sin is yellow + orange rather than the yellow base entry: the
        // base is a greenish yellow and the stripe is amber. Everything here
        // is a blend of the base entries - see TMIX() in include/theme.h.
        PAL_GREEN,          PAL_BLUE,        PAL_AMBER,      PAL_PURPLE,
        PAL_AMBER,          PAL_PURPLE,      PAL_ORANGE,     PAL_ORANGE,
        PAL_RED,            PAL_PURPLE,      PAL_AMBER,
    },
    {   // Vaporwave - hot pink, purple, yellow, turquoise, pink, and nothing
        // else. No green, no red, no blue on its own: blue only ever appears
        // mixed into a purple or a turquoise.
        PAL_PINK_HOT,       PAL_TURQUOISE,   PAL_WHITE,      PAL_PURPLE,
        PAL_PINK_HOT,       IDX_COLOR_BLACK, PAL_PURPLE,     PAL_YELLOW,
        PAL_PINK,           PAL_PURPLE,      PAL_TURQUOISE,
    },
    {   // Frutiger Aero - turquoise glass, deep blue, grass green, silver.
        // No warning colour: a note is not a fault, and silver says it without
        // putting a hot colour on a screen that is all water and grass.
        PAL_TURQUOISE,      IDX_COLOR_GREEN_LT, PAL_WHITE,   PAL_SILVER,
        PAL_TURQUOISE,      IDX_COLOR_BLACK, PAL_NAVY,       PAL_SILVER,
        IDX_COLOR_RED_LT,   PAL_NAVY,        PAL_TURQUOISE,
    },
    {   // Terminal - one hue at three brightnesses.
        IDX_COLOR_GREEN_LT, PAL_GREEN,       IDX_COLOR_GREEN_LT, PAL_MOSS,
        PAL_GREEN,          IDX_COLOR_BLACK, PAL_MOSS,       IDX_COLOR_YELLOW_DK,
        PAL_RED,            PAL_GREEN,       IDX_COLOR_GREEN_LT,
    },
    {   // Gunmetal - greys and silvers only. Destructive stays red: a delete
        // prompt matching the rest of the theme is one nobody reads, and that
        // is the one place looking like everything else is a bug. Use the
        // camera's solid 0x3f grey for both ordinary and dim text: its named
        // light/dark greys resolve translucently in the A470 record palette.
        PAL_WHITE,          PAL_SILVER,      IDX_COLOR_GREY,    IDX_COLOR_GREY,
        PAL_SILVER,         IDX_COLOR_BLACK, IDX_COLOR_GREY, PAL_WHITE,
        PAL_RED,            PAL_SILVER,      PAL_WHITE,
    },
};

// The overlay's twelve readout fields per theme. Separate from the table above
// because these carry no meaning at all - the colour of the ISO field says
// nothing except "this is the ISO field" (docs/OVERLAY_DESIGN.md) - so a theme
// is free to assign them for looks, which is exactly what it cannot do with a
// warning.
//
// Order: BATT VOLT TEMP FREE SHOTS ISO TV MODE ZOOM RAM PRESET BEND.
static const unsigned short theme_osd[THEME_COUNT][THEME_OSD_FIELDS] = {
    {   PAL_GREEN,          PAL_BLUE,        PAL_RED,             PAL_PURPLE,
        PAL_GREEN,          PAL_AMBER,       PAL_ORANGE,          PAL_AMBER,
        PAL_BLUE,           PAL_PURPLE,      PAL_ORANGE,          PAL_RED },
    {   PAL_PINK_HOT,       PAL_TURQUOISE,   PAL_PURPLE,          PAL_TURQUOISE,
        PAL_PINK_HOT,       PAL_YELLOW,      PAL_WHITE,           PAL_PINK,
        PAL_PURPLE,         IDX_COLOR_GREY_LT, PAL_PINK,          PAL_TURQUOISE },
    {   IDX_COLOR_GREEN_LT, PAL_TURQUOISE,   PAL_WHITE,           PAL_NAVY,
        IDX_COLOR_GREEN,    PAL_TURQUOISE,   PAL_WHITE,           PAL_SILVER,
        IDX_COLOR_GREEN_LT, PAL_SILVER,      PAL_MOSS,            PAL_TURQUOISE },
    {   IDX_COLOR_GREEN_LT, PAL_GREEN,       PAL_MOSS,            PAL_GREEN,
        IDX_COLOR_GREEN_LT, PAL_GREEN,       IDX_COLOR_GREEN_LT,  IDX_COLOR_GREEN_LT,
        PAL_GREEN,          PAL_MOSS,        PAL_GREEN,           IDX_COLOR_GREEN_LT },
    {   PAL_WHITE,          PAL_SILVER,      IDX_COLOR_GREY,      PAL_SILVER,
        PAL_WHITE,          PAL_SILVER,      PAL_WHITE,           PAL_WHITE,
        IDX_COLOR_GREY,     IDX_COLOR_GREY,  PAL_SILVER,          PAL_WHITE },
};

// The histogram's gradient, bottom band first.
//
// Bottom to top rather than bar by bar: the colour comes from where on the box
// a pixel is, so the bars read as one field with a gradient across it instead
// of sixty-four independently coloured sticks. The bottom band is where almost
// every histogram has its mass, so it is never the dark end - a gradient that
// hides the data it is drawn from is decoration.
static const unsigned short theme_histo_tab[THEME_COUNT][THEME_HISTO_BANDS] = {
    { PAL_GREEN,          PAL_AMBER,      PAL_RED        },  // Apple
    { PAL_TURQUOISE,      PAL_PURPLE,     PAL_PINK_HOT   },  // Vaporwave
    { IDX_COLOR_GREEN_LT, PAL_TURQUOISE,  PAL_WHITE      },  // Aero
    { PAL_MOSS,           PAL_GREEN,      IDX_COLOR_GREEN_LT },  // Terminal
    { IDX_COLOR_GREY,     PAL_SILVER,     PAL_WHITE      },  // Gunmetal
};


// Out of range comes back as Apple rather than as anything clever. The value
// arrives from the config block, which can be zeroed, written by an older
// build, or carry a theme a later build removed - and every one of those wants
// the same answer: the colours this camera shipped with.
static int theme_id(void)
{
    int t = conf.theme;
    return (t >= 0 && t < THEME_COUNT) ? t : THEME_APPLE;
}

// A table entry is either one of CHDK's twenty names or a raw palette byte -
// see TRAW() and the note on the colours CHDK never named.
static color theme_resolve(unsigned short v)
{
    if (v & 0x100) return (color)(v & 0xff);
    return chdk_colors[v & 0xff];
}

color theme_color(int slot)
{
    if (slot < 0 || slot >= TC_N) slot = TC_TEXT;
    return theme_resolve(theme_tab[theme_id()][slot]);
}

color theme_osd_color(int field)
{
    if (field < 0 || field >= THEME_OSD_FIELDS) field = 0;
    return theme_resolve(theme_osd[theme_id()][field]);
}

color theme_histo(int band)
{
    if (band < 0) band = 0;
    if (band >= THEME_HISTO_BANDS) band = THEME_HISTO_BANDS - 1;
    return theme_resolve(theme_histo_tab[theme_id()][band]);
}
