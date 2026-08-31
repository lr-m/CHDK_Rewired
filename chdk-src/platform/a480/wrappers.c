#include "fileutil.h"
#include "gui_draw.h"

#include "../generic/wrappers.c"

#ifdef CAM_SUPPRESS_LOW_BATTERY_ICON
// Canon's LowBattery GUI object, reverse engineered from 100b LowBattery.c.
// Event 0x60 hides the widget. Setting its visibility state back to 1 makes a
// later 0x5f blink/show event a no-op. This touches only the display object;
// LowBat's voltage monitoring and shutdown state machine remain intact.
int camera_suppress_low_battery_icon(void)
{
    volatile int *state = (volatile int *)0x71b4;
    if (!state[0]) return 0; // GUI object has not been constructed yet
    ((void (*)(int, int))0xffd4027c)(0x60, 0);
    state[2] = 1;
    return 1;
}
#endif

#ifdef CAM_CUSTOM_SOUNDS
// Canon's My Camera table, populated by the idempotent init at 0xffc40124.
// Entry 0 is the startup JPEG; entries 1..4 are the sounds selected by IDs
// 0x2001..0x2004. Repointing an entry is safe after init and leaves the ROM and
// the stock buffers untouched. The loaded files stay allocated for the boot.
#define A480_MYCAM_TABLE ((unsigned *)0x000127c4)
#define A480_MYCAM_INIT  ((void (*)(void))0xffc40124)

static unsigned a480_le16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned a480_le32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static int a480_valid_sound(const unsigned char *p, int size)
{
    if (!p || size < 44 || size > 256*1024) return 0;
    if (p[0]!='R' || p[1]!='I' || p[2]!='F' || p[3]!='F' ||
        p[8]!='W' || p[9]!='A' || p[10]!='V' || p[11]!='E' ||
        p[12]!='f' || p[13]!='m' || p[14]!='t' || p[15]!=' ') return 0;
    if (a480_le32(p+16) != 16 || a480_le16(p+20) != 1 ||
        a480_le16(p+22) != 1 || a480_le32(p+24) != 11025 ||
        a480_le16(p+34) != 8) return 0;
    if (p[36]!='d' || p[37]!='a' || p[38]!='t' || p[39]!='a') return 0;
    return a480_le32(p+40) <= (unsigned)(size-44);
}

int platform_load_custom_sounds(void)
{
    static const char * const names[4] = {
        0,
        "A/CHDK/SOUNDS/shutter.wav",
        "A/CHDK/SOUNDS/button.wav",
        "A/CHDK/SOUNDS/selftimer.wav"
    };
    unsigned loaded = 0;
    int i;

    A480_MYCAM_INIT();
    for (i = 1; i < 4; i++)
    {
        int size = 0;
        unsigned char *data = (unsigned char *)load_file_to_length(names[i], &size, 0,
                                                                   256*1024);
        if (a480_valid_sound(data, size))
        {
            A480_MYCAM_TABLE[(i+1)*4] = (unsigned)data;
            A480_MYCAM_TABLE[(i+1)*4+1] = (unsigned)size;
            loaded |= 1u << i;
        }
        else if (data)
            free(data);
    }
    return (int)loaded;
}
#endif

long lens_get_focus_pos()
{
	return _GetFocusLensSubjectDistance();
}

long lens_get_focus_pos_from_lens()
{
	return _GetFocusLensSubjectDistanceFromLens(); 
}

long lens_get_target_distance()
{
	return _GetCurrentTargetDistance();
}
 
 //--------------------------------------------------
 // DoMFLock : use _MFOn/_MFOff  or  _PT_MFOn/_PT_MFOff  or _SS_MFOn/_SS_MFOff if defined in stubs_entry.S
 //            otherwise use PostLogicalEventForNotPowerType(levent_id_for_name(PressSW1andMF),0); (see sx500hs for an example)
 
int DoMFLock(void)
{
  if (!camera_info.state.mode_play) {
     _PT_MFOn();
     return(1);
  }
  return(0);
}
 
int UnlockMF(void)
{
  if (!camera_info.state.mode_play) {
     _PT_MFOff();
     return(1);
  }
  return(0);
}
