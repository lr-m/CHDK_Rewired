#include "fileutil.h"      // load_file_to_length, for the sound slots
#include "../generic/wrappers.c"

#ifdef CAM_CUSTOM_SOUNDS
//-------------------------------------------------------------------
// Canon's My Camera sound slots, repointed at files on the card.
//
// Same mechanism as the A480's (platform/a480/wrappers.c): the table is filled
// by an idempotent init, and repointing an entry after that init is safe -
// the ROM and the stock buffers are left alone, and the loaded files stay
// allocated for the rest of the boot.
//
// Both addresses are from tools/newport.py and then checked three ways
// against cameras/a470/ghidra/, because a wrong table base here is a pointer
// written into arbitrary RAM:
//
//   FUN_ffc45d04 asserts through s_MyCamFunc_c_ffc45ee0, so it is MyCamFunc.c
//   it guards on a flag and fills six entries of {buffer, size} at a 16 byte
//     stride, which is the layout the A480 documents
//   its table base literal, DAT_ffc45ef8, reads 0x00019434 out of the ROM
//     image - the address newport reported - and the size it writes into
//     entry 0 is 0x47fa, which is the startup JPEG size newport reported
//     separately
//
// On slot order, and on getting it wrong once. The init registers three
// entries by ID, and the IDs are not the entry numbers:
//
//   FUN_ffc506d8(3, entry2)     entry 2 -> ID 3
//   FUN_ffc506d8(2, entry3)     entry 3 -> ID 2
//   FUN_ffc506d8(4, entry4)     entry 4 -> ID 4
//
// The first version of this file read that permutation as meaning the sounds
// were swapped relative to the A480, and swapped the two names to compensate.
// On the body that came out backwards: the button played the shutter and the
// shutter played the button.
//
// So the entry index carries the meaning and the ID does not - entry 2 is the
// shutter and entry 3 is the button here exactly as on the A480, whatever ID
// each is registered under. The permutation is real, it is just not about
// this. Confirmed on hardware, which is the only reason it is stated flatly.
#define A470_MYCAM_TABLE ((unsigned *)0x00019434)
#define A470_MYCAM_INIT  ((void (*)(void))0xffc45d04)

static unsigned a470_le16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned a470_le32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

// 11025Hz 8 bit mono PCM, the format the stock slots hold. Anything else is
// refused rather than pointed at: the table entry is handed straight to a
// firmware player that trusts it.
static int a470_valid_sound(const unsigned char *p, int size)
{
    if (!p || size < 44 || size > 256*1024) return 0;
    if (p[0]!='R' || p[1]!='I' || p[2]!='F' || p[3]!='F' ||
        p[8]!='W' || p[9]!='A' || p[10]!='V' || p[11]!='E' ||
        p[12]!='f' || p[13]!='m' || p[14]!='t' || p[15]!=' ') return 0;
    if (a470_le32(p+16) != 16 || a470_le16(p+20) != 1 ||
        a470_le16(p+22) != 1 || a470_le32(p+24) != 11025 ||
        a470_le16(p+34) != 8) return 0;
    if (p[36]!='d' || p[37]!='a' || p[38]!='t' || p[39]!='a') return 0;
    return a470_le32(p+40) <= (unsigned)(size-44);
}

int platform_load_custom_sounds(void)
{
    // Indexed by table entry, not by ID - see the slot note above. Entry 1 is
    // replaced before Canon's startup worker in sub/102c/boot.c.
    static const char * const names[4] = {
        0,
        "A/CHDK/SOUNDS/shutter.wav",    // entry 2
        "A/CHDK/SOUNDS/button.wav",     // entry 3
        "A/CHDK/SOUNDS/selftimer.wav"   // entry 4 -> ID 4
    };
    unsigned loaded = 0;
    unsigned char *startup_data;
    int startup_size = 0;
    int i;

    startup_data = (unsigned char *)load_file_to_length(
        "A/CHDK/SOUNDS/startup.wav", &startup_size, 0, 256*1024);
    if (!a470_valid_sound(startup_data, startup_size))
    {
        if (startup_data) free(startup_data);
        startup_data = 0;
    }

    // Deliberately does NOT call A470_MYCAM_INIT().
    //
    // The A480 calls it here and its Canon startup jingle still plays; this
    // body's did not, and the init is the one thing this path does that could
    // reach that far back. It is idempotent by a guard, so calling it early
    // does not repeat Canon's work - it *replaces* it, and Canon's own later
    // call then returns having done nothing. If the startup sound is played
    // off the back of that call rather than off the table, ours running first
    // at gui_init() time would lose it. platform/a460/sub/100d/boot.c records
    // the same init being actively dangerous when called too early.
    //
    // So wait for Canon instead, and only repoint entries it has already
    // filled. An entry that is not populated yet is left alone rather than
    // pointed at our file, because the size beside it would be Canon's and
    // the player trusts both.
    //
    // The cost if Canon has not run by the time this does: no custom sounds
    // for that boot. That is the trade being tested - it was the A470's
    // shutter and button working against its startup jingle not.
    for (i = 1; i < 4; i++)
    {
        unsigned *slot = &A470_MYCAM_TABLE[(i + 1) * 4];
        int size = 0;
        unsigned char *data;

        // Canon's buffer pointer for this slot, or nothing yet. The range test
        // is the same one vid_get_viewport_live_fb() uses on this port: a RAM
        // address, not a ROM one and not a stray small integer.
        if (slot[0] < 0x00100000 || slot[0] > 0x20000000 || slot[1] == 0)
            continue;

        data = (unsigned char *)load_file_to_length(names[i], &size, 0, 256*1024);
        if (a470_valid_sound(data, size))
        {
            slot[0] = (unsigned)data;
            slot[1] = (unsigned)size;
            loaded |= 1u << i;
        }
        else if (data)
            free(data);
    }

    // 0x2001 is ignored on this body at both early and late startup. Play the
    // intro through entry 2 / 0x2002 instead: this is the exact route the body
    // demonstrably uses for the working custom shutter sound. PT_PlaySound has
    // synchronously parsed and queued the buffer before it returns, so restoring
    // the shutter slot here does not change the clip already in flight.
    if (startup_data)
    {
        unsigned *slot = &A470_MYCAM_TABLE[8];
        unsigned save_ptr = slot[0], save_size = slot[1];
        slot[0] = (unsigned)startup_data;
        slot[1] = (unsigned)startup_size;
        _PT_PlaySound(0x2002, 0, 0);
        slot[0] = save_ptr;
        slot[1] = save_size;
        loaded |= 1;
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

void camera_set_led(int led, int state, __attribute__ ((unused))int bright)
{
        // 0 green
        // 1 orange 
        // 8 blue
        // 9 af  

  int leds[] = {0,1,8,9};
  _LEDDrive(leds[led%4], state<=1 ? !state : state);
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
