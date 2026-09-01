#include "fileutil.h"
#include "../generic/wrappers.c"

#ifdef CAM_CUSTOM_SOUNDS
//-------------------------------------------------------------------
// Canon's My Camera sound slots, replaced from files on the card.
//
// NOT the A460/A470/A480 mechanism. Those three repoint the table entry at a
// buffer we malloc'd. This body's cache entry carries a third field and its
// getter acts on it, which makes repointing wrong twice over here - the second
// way is a wild write into the heap. This is the same layout, and the same
// conclusion, as the A540 (platform/a540/wrappers.c).
//
// The getter is FUN_ffe8bfbc, decompiled from PRIMARY_a430_100b.BIN. Stripped
// to what matters, with the table base DAT_ffe8c0bc = 0x000812d8 and entries of
// stride 0x0c:
//
//     size = flash_size(theme, kind);              // FUN_ffe8d074
//     if (entry.cached_theme_id != theme)
//         copy_from_flash(theme, kind, entry.buffer, size);   // (a) FUN_ffe8d0d8
//     entry.size = size;                                      // (b)
//     *out_ptr  = entry.buffer;
//     *out_size = size;
//     entry.cached_theme_id = theme;
//
// (b) rewrites entry.size from flash on *every* call, so the length we store
// beside a repointed buffer is discarded and the player is told the stock
// asset's length whatever we wrote. That alone is why the custom sound only
// ever played as far as the stock clip ran - the "short gated beep" recorded
// in TODO.md.
//
// (a) is the dangerous half. The previous version of this file wrote
// entry.buffer and entry.size but never entry.cached_theme_id, so the first
// getter call whose theme id did not match memcpy'd a *flash-sized* asset into
// our malloc'd WAV buffer, which was sized for our file. The shutter slot's
// stock asset is 3244 bytes and the shutter.wav on the card was 10779, but the
// self-timer's is 22092 - so the copy runs off the end of whatever heap block
// it lands in. The shutter sound is fetched as part of taking a picture, which
// is why the corruption showed up as the image processing after a shot being
// broken rather than as anything audible.
//
// So the pointer is never touched. We overwrite the *contents* of the buffer
// Canon already allocated - the same discipline sub/100b/boot.c uses for the
// startup image on this body. A later theme change simply copies the stock
// sound back over ours: no overflow, nothing to clean up, and the worst case is
// that the custom sound stops until the next boot.
//
// The table, re-checked here against the allocator at 0xffe8c0c0 and the
// literals it loads:
//
//   MYCAM_TABLE 0x000812d8   five entries, stride 0x0c   (DAT_ffe8c0bc,
//                            and DAT_ffe8c200 = 0x00081308 = table + 4*0x0c,
//                            which is where the allocator's backwards loop
//                            starts - so five entries, not six)
//     +0x00  void *buffer          malloc'd by the allocator
//     +0x04  u32   size            current theme's asset length
//     +0x08  u16   cached theme id
//   MYCAM_DONE  0x0000c22c   the allocator's own guard (DAT_ffe8c1f8), set to
//                            1 on the way out
//   kinds at 0xffe8be80: 0x2000 0x2001 0x2002 0x2003 0x2004, ascending, so on
//   this ROM the kind id and the entry index agree.
//
// Which slot is which is confirmed by NAME, not by convention - reading that
// wrong is what shipped the A470 and A460 with shutter and button swapped. The
// My Camera asset container in the flash sector at 0xfff70000 carries a name
// and a length per asset; records start at +0x26 with stride 0x1a, and the
// parse is self-checking because the payloads are contiguous from +0xac:
//
//   idx  name       stock size  meaning
//    0   121_SI01        18426  Startup Image  (sub/100b/boot.c replaces this)
//    1   121_SS01        11111  Startup Sound  - left alone, see TODO.md
//    2   121_RL01         3244  ReLease   -> shutter
//    3   121_OP01         3300  OPeration -> button
//    4   121_TM01        22092  TiMer     -> self timer
//
// Those five lengths are byte-identical to the A540's, which is a separate
// confirmation that this is the same Canon theme asset set.
//
// The stock sizes are also the ceiling, and it is a tight one: the getter
// reports the stock length whatever we write, so a longer file cannot be played
// and is refused here rather than half-played. tools/mksound.py --max-bytes
// cuts a source sound to fit with a fade-out, which is how
// cameras/a430/card/CHDK/SOUNDS gets its own shorter pair.
#define A430_MYCAM_TABLE ((volatile unsigned *)0x000812d8)
#define A430_MYCAM_DONE  ((volatile unsigned *)0x0000c22c)

static unsigned a430_le16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned a430_le32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

// 11025Hz 8 bit mono PCM, the format the stock slots hold. Anything else is
// refused rather than copied in: the buffer is handed straight to a firmware
// player that trusts it.
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

    // Deliberately does NOT call the allocator. It is idempotent by a guard, so
    // calling it early does not repeat Canon's work - it *replaces* it, and
    // Canon's own later call then falls straight through. On the A460 that was
    // actively dangerous this early (sub/100d/boot.c). The replacement
    // StartupImage task in sub/100b/boot.c has already run it by the time
    // gui_init() reaches here; the flag is the proof, not the assumption.
    if (*A430_MYCAM_DONE != 1)
        return 0;

    for (i = 2; i < 5; i++)
    {
        volatile unsigned *slot = &A430_MYCAM_TABLE[i * 3];
        unsigned char *dst = (unsigned char *)slot[0];
        unsigned cap = slot[1];
        int size = 0;
        unsigned char *data;

        // Canon's buffer for this slot, or nothing usable. The range test is
        // the same one this port's vid_get_viewport_live_fb() relies on: a RAM
        // address, not a ROM one and not a stray small integer.
        //
        // cap is the current theme's asset length, written beside the pointer
        // by the allocator's second loop. It is zero when the selected theme
        // has no asset for this slot, and the getter then reports length zero
        // whatever the buffer holds - so there is nothing to be gained by
        // writing into it.
        if ((unsigned)dst < 0x00100000 || (unsigned)dst > 0x20000000 || cap == 0)
            continue;

        data = (unsigned char *)load_file_to_length(names[i], &size, 0, 256*1024);
        if (a430_valid_sound(data, size) && (unsigned)size <= cap)
        {
            unsigned k;
            for (k = 0; k < (unsigned)size; k++)
                dst[k] = data[k];
            // The player is told the stock length by the getter, not ours, so
            // anything past our file is still played. Pad with 8 bit unsigned
            // PCM silence - 0x80, not 0x00, which would be a DC step and
            // audible as a click.
            for (k = (unsigned)size; k < cap; k++)
                dst[k] = 0x80;
            loaded |= 1u << i;
        }
        // Ours now lives in Canon's buffer, so unlike the A460/A470/A480 there
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
