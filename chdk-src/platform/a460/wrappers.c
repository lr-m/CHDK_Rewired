#include "fileutil.h"      // load_file_to_length, for the sound slots
#include "../generic/wrappers.c"

#ifdef CAM_CUSTOM_SOUNDS
//-------------------------------------------------------------------
// Canon's My Camera sound slots, repointed at files on the card. Same
// mechanism as the A470's and A480's.
//
// Both addresses are proven twice over here, which is better than either of
// those bodies managed:
//
//   the table is the one sub/100d/boot.c already reads to place the custom
//     boot screen, and that boot screen works on the body - so 0x0006e088
//     holding {buffer, size} at entry 0 is confirmed by hardware, not by
//     inference
//   disassembling the init at 0xffe68b0c shows it guarding on 0xcf0c, writing
//     five entry sizes at a 16 byte stride, allocating a buffer per entry, and
//     then registering three of them by ID
//
// This body registers its entries by ID the same way the A470 does:
//
//   register(3, entry2)      mov r0,#3 ; add r1,r5,#32 ; ldm r1,{r1,r2}
//   register(2, entry3)      mov r0,#2 ; add r1,r5,#48 ; ldm r1,{r1,r2}
//   register(4, entry4)      mov r0,#4 ; ldr r1,[r5,#64]
//
// The IDs are permuted against the entry numbers, and that permutation means
// nothing for which sound is which - see the note in platform/a470/wrappers.c
// for the version of this that shipped swapped and had to be corrected on the
// body. Entry 2 is the shutter and entry 3 is the button, as on every other
// camera here. Confirmed on hardware.
//
// On calling the init: sub/100d/boot.c records that calling it from the first
// task creation is what stopped this camera booting, because it writes through
// table pointers that are not valid that early. This runs from gui_init(),
// long after Canon has run its own init and set the guard - so the call finds
// the flag already set and returns immediately, and all this does is repoint
// two entries that Canon has already filled.
#define A460_MYCAM_TABLE ((unsigned *)0x0006e088)
#define A460_MYCAM_INIT  ((void (*)(void))0xffe68b0c)

static unsigned a460_le16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned a460_le32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

// 11025Hz 8 bit mono PCM, the format the stock slots hold. Anything else is
// refused rather than pointed at - the entry is handed to a firmware player
// that trusts it.
static int a460_valid_sound(const unsigned char *p, int size)
{
    if (!p || size < 44 || size > 256*1024) return 0;
    if (p[0]!='R' || p[1]!='I' || p[2]!='F' || p[3]!='F' ||
        p[8]!='W' || p[9]!='A' || p[10]!='V' || p[11]!='E' ||
        p[12]!='f' || p[13]!='m' || p[14]!='t' || p[15]!=' ') return 0;
    if (a460_le32(p+16) != 16 || a460_le16(p+20) != 1 ||
        a460_le16(p+22) != 1 || a460_le32(p+24) != 11025 ||
        a460_le16(p+34) != 8) return 0;
    if (p[36]!='d' || p[37]!='a' || p[38]!='t' || p[39]!='a') return 0;
    return a460_le32(p+40) <= (unsigned)(size-44);
}

int platform_load_custom_sounds(void)
{
    // Indexed by table entry, not by ID - see the slot note above. Entry 1 is
    // filled from ROM by the init and is left alone.
    static const char * const names[4] = {
        0,
        "A/CHDK/SOUNDS/shutter.wav",    // entry 2
        "A/CHDK/SOUNDS/button.wav",     // entry 3
        "A/CHDK/SOUNDS/selftimer.wav"   // entry 4 -> ID 4
    };
    unsigned loaded = 0;
    int i;

    A460_MYCAM_INIT();
    for (i = 1; i < 4; i++)
    {
        int size = 0;
        unsigned char *data = (unsigned char *)load_file_to_length(names[i], &size, 0,
                                                                   256*1024);
        if (a460_valid_sound(data, size))
        {
            A460_MYCAM_TABLE[(i+1)*4]     = (unsigned)data;
            A460_MYCAM_TABLE[(i+1)*4 + 1] = (unsigned)size;
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
