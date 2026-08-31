// CHDK palette colors for cameras with yuv overlay

#include "palette.h"

#ifdef  CAM_DRAW_RGBA

// Colors for all camera modes
unsigned char ply_colors[] =
{
        0,                          // Transparent
        1,                          // Black
        2,                          // White
        3,                          // Red
        4,                          // Dark Red
        5,                          // Light Red
        6,                          // Green
        7,                          // Dark Green
        8,                          // Light Green
        9,                          // Blue
        10,                         // Dark Blue
        11,                         // Light Blue / Cyan
        12,                         // Grey
        13,                         // Dark Grey
        14,                         // Light Grey
        15,                         // Yellow
        16,                         // Dark Yellow
        17,                         // Light Yellow
        18,                         // Transparent Dark Grey
        19,                         // Magenta
};

// Colors for drawing on YUV bitmap
unsigned char yuv_colors[] =
{
        0x00,                       // Transparent
        0xff,                       // Black
        0x0f,                       // White
        0xbe,                       // Red
        0xd5,                       // Dark Red
        0x7a,                       // Light Red
        0x61,                       // Green
        0xc5,                       // Dark Green
        0x05,                       // Light Green
        0xa9,                       // Blue
        0xee,                       // Dark Blue
        0x45,                       // Light Blue / Cyan
        0x74,                       // Grey
        0xb4,                       // Dark Grey
        0x34,                       // Light Grey
        0x11,                       // Yellow
        0x5a,                       // Dark Yellow
        0x0e,                       // Light Yellow
        0xb0,                       // Transparent Dark Grey
        0xfe,                       // Magenta
        // Placeholders to expand to 32 values so color index can be clamped quickly with a bitwise and operation.
        // Could potentially be used for additional colours.
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black
        0xff,                       // Black (do not change)
};

// Colors for drawing on RGBA bitmap
unsigned int rgba_colors[] =
{
        0x00000000,                 // Transparent
        0xFF000000,                 // Black
        0xFFFFFFFF,                 // White
        0xFFFF0000,                 // Red
        0xFF8B0000,                 // Dark Red
        0xFFCD5C5C,                 // Light Red
        0xFF00FF00,                 // Green
        0xFF008000,                 // Dark Green
        0xFF90EE90,                 // Light Green
        0xFF0000FF,                 // Blue
        0xFF00008B,                 // Dark Blue
        0xFF00FFFF,                 // Light Blue / Cyan
        0xFF808080,                 // Grey
        0xFF404040,                 // Dark Grey
        0xFFC0C0C0,                 // Light Grey
        0xFFFFFF00,                 // Yellow
        0xFFFFA000,                 // Dark Yellow
        0xFFFFFF80,                 // Light Yellow
        0x80404040,                 // Transparent Dark Grey
        0xFFFF00FF,                 // Magenta
        // Placeholders to expand to 32 values so color index can be clamped quickly with a bitwise and operation.
        // Could potentially be used for additional colours.
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black
        0xFF000000,                 // Black (do not change)
};

#else // CAM_DRAW_RGBA

#undef  COLOR_TRANSPARENT
#define COLOR_TRANSPARENT           0x00
#undef  COLOR_BLACK
#define COLOR_BLACK                 0xff

// Colors for all camera modes
unsigned char ply_colors[] =
{
        COLOR_TRANSPARENT,         // Placeholder for script colors
        COLOR_BLACK,               // Placeholder for script colors
        0x0f,                       // White
        0xbe,                       // Red
        0xd5,                       // Dark Red
        0x7a,                       // Light Red
        0x61,                       // Green
        0xc5,                       // Dark Green
        0x05,                       // Light Green
        0xa9,                       // Blue
        0xee,                       // Dark Blue
        0x45,                       // Light Blue / Cyan
        0x74,                       // Grey
        0xb4,                       // Dark Grey
        0x34,                       // Light Grey
        0x11,                       // Yellow
        0x5a,                       // Dark Yellow
        0x0e,                       // Light Yellow
        0xb0,                       // Transparent Dark Grey
        0xfe,                       // Magenta
};

#endif // CAM_DRAW_RGBA