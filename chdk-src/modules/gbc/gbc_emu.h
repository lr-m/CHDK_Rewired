#ifndef GBC_EMU_H
#define GBC_EMU_H

#include "gbc_port.h"
#include "gbc_rom.h"
#include "gbc_cam.h"

struct gb_state;

// How long one call may spend emulating before handing the GUI task back, and
// the hard ceiling on frames in that window.
//
// These were 40ms and 4 frames, and the frame cap was the binding constraint -
// which made it the single biggest limit on speed in the whole port, bigger
// than anything in the interpreter. CHDK calls the draw handler roughly every
// 200ms; four frames took about 140ms of that originally and around 78ms once
// the interpreter got faster, so more than half of every callback was spent
// idle. No amount of optimising the emulator can show up on screen while the
// frame cap ends the slice early.
//
// The camera ROM's startup is 165 emulated frames before it draws anything, so
// at four frames per callback it could not possibly start in less than about
// eight seconds.
//
// Raised, with real-time pacing added in gbc_emu_frame() so that a slice can
// never run *ahead* of the wall clock: the Game Boy is a 60Hz machine and
// emulating faster than that would make games run fast rather than smooth.
// Input is polled once per callback either way, so a longer slice costs no
// responsiveness - the slice length still bounds how long the GUI task is held.
#define GBC_SLICE_MS                100
#define GBC_MAX_FRAMES_PER_SLICE    12

// The Game Boy's frame rate, used for the pacing above.
#define GBC_TARGET_FPS              60

// Rasterise only every Nth displayed frame. The emulated machine still runs
// every frame at full speed; this only skips the pixel work, which is roughly
// half the cost of a frame. 2 halves it; 1 restores per-frame rasterising.
#define GBC_RENDER_EVERY            2

// How long battery RAM may sit dirty before it is written anyway. The commit
// signal is the ROM turning cartridge RAM off, which most games do promptly
// after a save and some never do at all.
#define GBC_SAV_MAX_MS              10000

typedef struct
{
    struct gb_state *state;
    gbc_rom_t *pager;
    gbc_cam_t *cart;                // camera cartridges only

    int open;
    int is_camera;
    int panicked;

    // Kept so gbc_emu_close() can name the save file. The path the caller
    // passed to gbc_emu_open() is a stack buffer in the picker and is long
    // gone by the time the album needs writing.
    char rom_path[128];

    // Ordinary cartridge battery RAM - see gbc_save.h. The camera cartridge
    // keeps its own path inside gbc_cam_t because its write is raised from
    // inside the mapper; this one is raised by the core's own flags and is
    // acted on out here, so the emulator can hold it.
    char sav_path[128];
    int  sav_len;                   // 0 when the cartridge has no battery RAM
    unsigned sav_at;                // tick of the last write, for the timer

    unsigned long frames;
    unsigned last_call;             // get_tick_count() at the previous frame
    int      render_phase;          // frames since the last rasterised one
    int      frame_budget;          // frames this slice may run, paced to 60Hz
} gbc_emu_t;

// Opens a ROM off the card and brings the machine up. Returns 0 on success.
int  gbc_emu_open(gbc_emu_t *e, const char *rom_path);
void gbc_emu_close(gbc_emu_t *e);

// Per-frame diagnostics. gbc_last_frame_hit_guard means the frame ran out of
// its instruction budget without reaching vblank.
extern unsigned gbc_last_frame_insns;
extern int      gbc_last_frame_hit_guard;
// Diagnostic: 0 bypasses the Game Boy's scanline rasteriser, leaving the
// emulated CPU untouched, so the change in gbc_ms_emu is what rendering costs.
extern int gbc_render_enabled;      // driven per frame by the slice loop
extern int gbc_render_user;         // what the player/diagnostic asked for

// Why the last gbc_emu_open() failed, if it did.
extern char gbc_open_error[64];

extern unsigned gbc_ms_gap, gbc_ms_emu, gbc_ms_blit;
extern int      gbc_frames_per_slice;

// Emulated frames per second against the wall clock, in tenths - so 87 is
// 8.7fps. Real hardware ran at 59.7. Zero until the first full second has
// been measured. See the definition for why it is not derived from the
// timings above.
extern int      gbc_fps_x10;
void            gbc_fps_reset(void);

// Forces a full repaint on the next frame. Call after anything else has drawn
// over the Game Boy screen area.
void gbc_invalidate_screen(void);

// Fills `buf` (needs >= 64 bytes) with the diagnostic line the module draws.
void gbc_hud_line(gbc_emu_t *e, char *buf, int len);

// Runs one frame and blits it. Returns 0 to continue, 1 if the player asked to
// leave, -1 on a fault (reason in gbc_panic_msg).
int  gbc_emu_frame(gbc_emu_t *e);

#endif
