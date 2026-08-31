#include "fileutil.h"      // load_file_to_length, for the sound slots
#include "../generic/wrappers.c"

#ifdef CAM_CUSTOM_SOUNDS
//-------------------------------------------------------------------
// Canon's My Camera sound slots, replaced from files on the card.
//
// NOT the A480/A470 mechanism. Those two repoint the table entry at a buffer
// we malloc'd; on this body that would be wrong twice over, and the second
// way is a wild write. The getter is 0xffebc588, and it ends like this:
//
//     if (entry.cached_theme_id != selected_theme)
//         copy_from_flash(theme, idx, entry.buffer, flash_size);   // (a)
//     entry.size = flash_size;                                     // (b)
//     *out_ptr  = entry.buffer;
//     *out_size = flash_size;
//     entry.cached_theme_id = selected_theme;
//
// (b) means the size we write beside a repointed buffer is discarded on the
// next call and the player is told the *stock* asset's length regardless. (a)
// means that the first time the user changes My Camera theme, Canon memcpy's
// a flash-sized asset into whatever pointer is sitting in entry.buffer - and
// if that is our malloc'd WAV, sized for our file, it is a heap overflow.
//
// So the pointer is never touched. We overwrite the *contents* of the buffer
// Canon already allocated, which is the same discipline sub/100b/boot.c uses
// for the startup image on this body. A later theme change then simply copies
// the stock sound back over ours - no overflow, nothing to clean up, and the
// worst case is that the custom sound stops until the next boot.
//
// The table, from sub/100b/boot.c and re-checked here against the allocator
// at 0xffebc68c and its literals:
//
//   MYCAM_TABLE 0x000737e0   five entries, stride 0x0c
//     +0x00  void *buffer          malloc'd by 0xffebc68c, sized from flash
//     +0x04  u32   size            current theme's asset length
//     +0x08  u16   cached theme id
//   ids at 0xffebc44c: 0x2000 0x2001 0x2002 0x2003 0x2004, ascending, so on
//   this ROM the id and the entry index agree - unlike the A470, where they
//   are permuted. The index is what carries the meaning either way; that is
//   the A470's hardware-confirmed lesson (platform/a470/wrappers.c).
//
// Which slot is which is NOT inherited from that convention here - this body
// says so itself. The My Camera asset container in the flash sector at
// 0xfff70000 carries a name and a length per asset, in entry order:
//
//   idx  name       stock size  meaning
//    0   121_SI01        18426  Startup Image  (sub/100b/boot.c replaces this)
//    1   121_SS01        11111  Startup Sound  - left alone
//    2   121_RL01         3244  ReLease  -> shutter
//    3   121_OP01         3300  OPeration -> button
//    4   121_TM01        22092  TiMer    -> self timer
//
// The parse is self-checking: the five payloads are contiguous from +0xac and
// entry 0 lands on 0xfff700ac at 18426 bytes, which boot.c derived separately
// from the draw path. So the A470/A480 entry order is confirmed by name on
// this ROM rather than assumed.
//
// Those stock sizes are also the ceiling - see the cap note in the loop below.
// It is why cameras/a540/card/CHDK/SOUNDS holds its own shorter shutter.wav
// and button.wav rather than the A470's: 3244 bytes is 0.29s, and the A470's
// 10779-byte file would simply be refused. tools/mksound.py --max-bytes cuts
// a source sound to fit with a fade-out.
#define A540_MYCAM_TABLE ((volatile unsigned *)0x000737e0)
#define A540_MYCAM_DONE  ((volatile unsigned *)0x0000a9fc)

static unsigned a540_le16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned a540_le32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

// 11025Hz 8 bit mono PCM, the format the stock slots hold. Anything else is
// refused rather than copied in: the buffer is handed straight to a firmware
// player that trusts it.
static int a540_valid_sound(const unsigned char *p, int size)
{
    if (!p || size < 44 || size > 256*1024) return 0;
    if (p[0]!='R' || p[1]!='I' || p[2]!='F' || p[3]!='F' ||
        p[8]!='W' || p[9]!='A' || p[10]!='V' || p[11]!='E' ||
        p[12]!='f' || p[13]!='m' || p[14]!='t' || p[15]!=' ') return 0;
    if (a540_le32(p+16) != 16 || a540_le16(p+20) != 1 ||
        a540_le16(p+22) != 1 || a540_le32(p+24) != 11025 ||
        a540_le16(p+34) != 8) return 0;
    if (p[36]!='d' || p[37]!='a' || p[38]!='t' || p[39]!='a') return 0;
    return a540_le32(p+40) <= (unsigned)(size-44);
}

int platform_load_custom_sounds(void)
{
    // Indexed by table entry - see the slot note above.
    static const char * const names[5] = {
        0,                              // entry 0, startup image
        0,                              // entry 1, startup sound - left alone
        "A/CHDK/SOUNDS/shutter.wav",    // entry 2
        "A/CHDK/SOUNDS/button.wav",     // entry 3
        "A/CHDK/SOUNDS/selftimer.wav"   // entry 4
    };
    unsigned loaded = 0;
    int i;

    // Deliberately does NOT call the allocator at 0xffebc68c, for the reason
    // platform/a470/wrappers.c records: it is idempotent by a guard, so
    // calling it early does not repeat Canon's work, it *replaces* it, and
    // Canon's own later call then falls straight through. On the A460 that was
    // actively dangerous this early (sub/100d/boot.c). Wait for Canon instead.
    //
    // The done-flag is the allocator's own, set to 1 on the way out. If it is
    // clear, the table holds nothing yet and there is nothing to overwrite.
    if (*A540_MYCAM_DONE != 1)
        return 0;

    for (i = 2; i < 5; i++)
    {
        volatile unsigned *slot = &A540_MYCAM_TABLE[i * 3];
        unsigned char *dst = (unsigned char *)slot[0];
        unsigned cap = slot[1];
        int size = 0;
        unsigned char *data;

        // Canon's buffer for this slot, or nothing usable. The range test is
        // the same one the A470 port uses: a RAM address, not a ROM one and
        // not a stray small integer.
        //
        // cap is the *current theme's* asset length, which the allocator wrote
        // beside the pointer. It is <= the malloc'd size, and that is checked
        // rather than assumed - the two sizes come from different places:
        //
        //   alloc  0xffebd78c(idx)         reads a descriptor table at
        //                                  0x0000aa20, stride 0x10, field +4.
        //                                  No theme argument - it is the
        //                                  per-asset capacity, one number.
        //   get    0xffebd63c(theme, idx)  resolves the selected theme's data
        //                                  block and reads that theme's length
        //                                  for this asset out of it.
        //
        // The capacity has to cover every theme or Canon's own theme switch
        // would overflow its own buffer, so a single theme's length cannot
        // exceed it. That makes cap a conservative ceiling, not merely a
        // plausible one. It is zero when
        // the selected theme has no asset for this slot (the allocator skips
        // the copy and stores 0), and in that case the getter returns length
        // zero whatever the buffer holds, so there is nothing to be gained by
        // writing into it.
        if ((unsigned)dst < 0x00100000 || (unsigned)dst > 0x20000000 || cap == 0)
            continue;

        data = (unsigned char *)load_file_to_length(names[i], &size, 0, 256*1024);
        if (a540_valid_sound(data, size) && (unsigned)size <= cap)
        {
            int k;
            for (k = 0; k < size; k++)
                dst[k] = data[k];
            // The player is told the stock length by the getter, not ours, so
            // anything past our file is still played. Pad it with 8 bit
            // unsigned PCM silence - 0x80, not 0x00, which would be a DC step
            // and audible as a click.
            for (k = size; k < (int)cap; k++)
                dst[k] = 0x80;
            loaded |= 1u << i;
        }
        // Ours now lives in Canon's buffer, so unlike the A480 and A470 there
        // is nothing to keep alive for the rest of the boot.
        if (data)
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


/* void start_IS(void){
 _StartISDrive();
} */

/* void stop_IS(void){
 _StopISDrive();
} */
 
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
