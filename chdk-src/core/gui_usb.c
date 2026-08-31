#include "camera_info.h"
#include "conf.h"
#include "gui_draw.h"
#include "gui_usb.h"

//-------------------------------------------------------------------
static icon_cmd usb_icon[] =
{
        { IA_FILLED_ROUND_RECT, 3, 11, 29, 14, IDX_COLOR_GREY_DK, IDX_COLOR_GREY_DK },
        { IA_FILLED_ROUND_RECT, 0,  0, 31, 11, IDX_COLOR_GREY_DK, IDX_COLOR_GREY_DK },
        { IA_FILLED_RECT,       2,  1, 29,  2, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,       1,  2,  2,  8, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,      29,  2, 30,  8, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,       5,  5, 26,  8, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,       7,  7,  9,  8, IDX_COLOR_YELLOW,  IDX_COLOR_YELLOW  },
        { IA_FILLED_RECT,      12,  7, 14,  8, IDX_COLOR_YELLOW,  IDX_COLOR_YELLOW  },
        { IA_FILLED_RECT,      17,  7, 19,  8, IDX_COLOR_YELLOW,  IDX_COLOR_YELLOW  },
        { IA_FILLED_RECT,      22,  7, 24,  8, IDX_COLOR_YELLOW,  IDX_COLOR_YELLOW  },
        { IA_FILLED_RECT,       2,  9,  3, 10, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,      28,  9, 29, 10, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,       3, 11,  4, 12, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,      27, 11, 28, 12, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_FILLED_RECT,       5, 12, 26, 13, IDX_COLOR_GREY,    IDX_COLOR_GREY    },
        { IA_END }
};

static void gui_usb_draw_icon()
{
    draw_icon_cmds(conf.usb_info_pos.x, conf.usb_info_pos.y, usb_icon);
}
//-------------------------------------------------------------------
static void gui_usb_draw_text()
{
    twoColors cl = user_color(conf.osd_color);
    draw_string(conf.usb_info_pos.x, conf.usb_info_pos.y, "<USB>", cl);
}
//--------------------------------------------------------------------
void gui_usb_draw_osd(int is_osd_edit)
{
    if ((conf.usb_info_enable == 1 && conf.remote_enable) || is_osd_edit)
        gui_usb_draw_icon();
    else if ((conf.usb_info_enable == 2 && conf.remote_enable) || is_osd_edit)
        gui_usb_draw_text();
}
