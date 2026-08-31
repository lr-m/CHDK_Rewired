#include "fileutil.h"
#include "../generic/wrappers.c"

#ifdef CAM_CUSTOM_SOUNDS
//-------------------------------------------------------------------
// Canon's My Camera cache. Unlike the other A4xx ports, A430 entries are
// 12-byte {buffer, size, cached_theme} records. Firmware function ffe8be98
// maps asset kinds 0x2001..0x2004 directly to cache entries 1..4:
// startup, shutter, operation/button and self-timer respectively.
//
// The allocator is idempotent and has already run through the replacement
// StartupImage task before gui_init reaches this function. Calling it again
// documents and enforces that precondition without reloading the assets.
#define A430_MYCAM_TABLE ((unsigned *)0x000812d8)
#define A430_MYCAM_INIT  ((void (*)(void))0xffe8c0c0)

static unsigned a430_le16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned a430_le32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

// Canon's player expects the format used by the stock My Camera sounds.
static int a430_valid_sound(const unsigned char *p, int size)
{
    if (!p || size < 44 || size > 256*1024) return 0;
    if (p[0]!='R' || p[1]!='I' || p[2]!='F' || p[3]!='F' ||
        p[8]!='W' || p[9]!='A' || p[10]!='V' || p[11]!='E' ||
        p[12]!='f' || p[13]!='m' || p[14]!='t' || p[15]!=' ') return 0;
    if (a430_le32(p+16) != 16 || a430_le16(p+20) != 1 ||
        a430_le16(p+22) != 1 || a430_le32(p+24) != 11025 ||
        a430_le16(p+34) != 8) return 0;
    if (p[36]!='d' || p[37]!='a' || p[38]!='t' || p[39]!='a') return 0;
    return a430_le32(p+40) <= (unsigned)(size-44);
}

int platform_load_custom_sounds(void)
{
    static const char * const names[3] = {
        "A/CHDK/SOUNDS/shutter.wav",
        "A/CHDK/SOUNDS/button.wav",
        "A/CHDK/SOUNDS/selftimer.wav"
    };
    unsigned loaded = 0;
    int i;

    A430_MYCAM_INIT();

    for (i = 0; i < 3; i++)
    {
        // Sound files map to cache entries 2..4. Each entry is three words.
        unsigned *slot = &A430_MYCAM_TABLE[(i + 2) * 3];
        int size = 0;
        unsigned char *data = (unsigned char *)load_file_to_length(
            names[i], &size, 0, 256*1024);

        if (a430_valid_sound(data, size))
        {
            slot[0] = (unsigned)data;
            slot[1] = (unsigned)size;
            loaded |= 1u << (i + 1);
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
     _MFOn();
     return(1);
  }
  return(0);
}
 
int UnlockMF(void)
{
  if (!camera_info.state.mode_play) {
     _MFOff();
     return(1);
  }
  return(0);
}
