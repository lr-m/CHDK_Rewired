// routines compiled as thumb
#include "platform.h"
#include "conf.h"
#include "gui_draw.h"
#include "keyboard.h"
#include "dirent.h"
#include "ctype.h"

#if defined(CAM_DRYOS)
#include "chdk-dir.c"
#endif

//---------------------------------------------------------------------

// CHDK dir functions

extern int   fw_closedir(void *d);
extern void *fw_opendir(const char* name);

DIR *opendir(const char* name)
{
    return opendir_chdk(name,OPENDIR_FL_NONE);
}

DIR *opendir_chdk(const char* name, unsigned flags)
{
    // Create CHDK DIR structure
    DIR *dir = malloc(sizeof(DIR));
    // If malloc failed return failure
    if (dir == 0) return NULL;

#if defined(CAM_DRYOS)
    extern int get_fstype(void);
    // try built-in routine first, but only on FAT
    if ((get_fstype() < 4) && (flags & OPENDIR_FL_CHDK_LFN))
    {
        dir->fw_dir = 0;
        dir->cam_DIR = CHDKOpenDir(name);
    }
    else
    {
        dir->cam_DIR = NULL;
    }
    if (!dir->cam_DIR)
    {
        // try firmware routine if built-in failed
        dir->fw_dir = 1;
        dir->cam_DIR = fw_opendir(name);
    }
#else
    (void)flags;
    dir->cam_DIR = fw_opendir(name);
#endif

    // Init readdir return value
    dir->dir.d_name[0] = 0;

    // If failed clean up and return failure
    if (!dir->cam_DIR)
    {
        free(dir);
        return NULL;
    }

    return dir;
}

#ifndef CAM_DRYOS

// Internal VxWorks dirent structure returned by readdir
struct __dirent
{
    char            d_name[100];
};

struct dirent* readdir(DIR *d)
{
    if (d && d->cam_DIR)
    {
        // Get next entry from firmware function
        extern void *fw_readdir(void *d);
        struct __dirent *de = fw_readdir(d->cam_DIR);
        // Return next directory name if present, else return 0 (end of list)
        if (de)
        {
            strcpy(d->dir.d_name,de->d_name);
            return &d->dir;
        }
        else
        {
            d->dir.d_name[0] = 0;
        }
    }
    return NULL;
}

#else // dryos

struct dirent * readdir(DIR *d)
{
    if (d && d->cam_DIR)
    {
        if (d->fw_dir == 0)
        {
            CHDKReadDir(d->cam_DIR, d->dir.d_name);
        }
        else
        {
            extern int fw_readdir(void *d, void* dd); // DRYOS
            fw_readdir(d->cam_DIR, d->dir.d_name);
        }
        return d->dir.d_name[0]? &d->dir : NULL;
    }
    return NULL;
}

#endif // dryos dir functions

int closedir(DIR *d)
{
    int rv = -1;
    if (d && d->cam_DIR)
    {
#if defined(CAM_DRYOS)
        if (d->fw_dir == 0)
        {
            rv = CHDKCloseDir(d->cam_DIR);
        }
        else
#endif
        rv = fw_closedir(d->cam_DIR);

        // Mark closed (just in case)
        d->cam_DIR = 0;
        // Free allocated memory
        free(d);    
    }
    return rv;
}

//----------------------------------------------------------------------------
// Char Wrappers (ARM stubs not required)

#if CAM_DRYOS

#define _U      0x01    /* upper */
#define _L      0x02    /* lower */
#define _D      0x04    /* digit */
#define _C      0x20    /* cntrl */
#define _P      0x10    /* punct */
#define _S      0x40    /* white space (space/lf/tab) */
#define _X      0x80    /* hex digit */
#define _SP     0x08    /* hard space (0x20) */
static int _ctype(int c,int t) {
    extern unsigned char ctypes[];  // Firmware ctypes table (in stubs_entry.S)
    return ctypes[c&0xFF] & t;
}

int isdigit(int c) { return _ctype(c,_D); }
int isspace(int c) { return _ctype(c,_S); }
int isalpha(int c) { return _ctype(c,(_U|_L)); }
int isupper(int c) { return _ctype(c,_U); }
int islower(int c) { return _ctype(c,_L); }
int ispunct(int c) { return _ctype(c,_P); }
int isxdigit(int c) { return _ctype(c,(_X|_D)); }
int iscntrl(int c) { return _ctype(c,_C); }

int tolower(int c) { return isupper(c) ? c | 0x20 : c; }
int toupper(int c) { return islower(c) ? c & ~0x20 : c; }

#else

// don't want to require the whole ctype table on vxworks just for this one
int iscntrl(int c) { return ((c >=0 && c <32) || c == 127); }

#endif

int isalnum(int c) { return (isdigit(c) || isalpha(c)); }

//----------------------------------------------------------------------------

#if CAM_DRYOS
char *strpbrk(const char *s, const char *accept)
{
    const char *sc1,*sc2;

    for (sc1 = s; *sc1 != '\0'; ++sc1)
    {
        for (sc2 = accept; *sc2 != '\0'; ++sc2)
        {
            if (*sc1 == *sc2)
                return (char*)sc1;
        }
    }
    return 0;
}
#endif

char *strstr(const char *s1, const char *s2)
{
    const char *p = s1;
    const int len = strlen(s2);

    for (; (p = strchr(p, *s2)) != 0; p++)
    {
        if (strncmp(p, s2, len) == 0)
            return (char*)p;
    }
    return (0);
}

#if CAM_DRYOS
void *memchr(const void *s, int c, int n)
{
    while (n-- > 0)
    {
        if (*(char *)s == c)
            return (void *)s;
        s++;
    }
    return (void *)0;
}
#endif

//----------------------------------------------------------------------------

struct tm *get_localtime()
{
    time_t t = time(NULL);
    return localtime(&t);
}

//----------------------------------------------------------------------------

unsigned int GetJpgCount(void)
{
    return strtol(camera_jpeg_count_str(),((void*)0),0);
}

unsigned int GetRawCount(void)
{
    unsigned free_kb = GetFreeCardSpaceKb();
    unsigned raw_kb =  camera_sensor.raw_size/1024;
    unsigned jpgcount = GetJpgCount();
    unsigned avg_jpg_kb = (jpgcount>0)? free_kb/jpgcount : 0;

    // 0.25 raw margin
    unsigned margin_kb = raw_kb/4;
    if(free_kb <= raw_kb + margin_kb) {
        return 0;
    }
    free_kb -= margin_kb;
    return free_kb/(raw_kb+avg_jpg_kb);
}

//----------------------------------------------------------------------------

// viewport image offset - used when image size != viewport size (zebra, histogram, motion detect & edge overlay)
// returns the byte offset into the viewport buffer where the image pixels start (to skip any black borders)
// see G12 port for sample implementation
int vid_get_viewport_image_offset() {
    return (vid_get_viewport_yoffset() * vid_get_viewport_byte_width() * vid_get_viewport_yscale()) + (vid_get_viewport_xoffset() * 3);
}

// viewport image offset - used when image size != viewport size (zebra, histogram, motion detect & edge overlay)
// returns the byte offset to skip at the end of a viewport buffer row to get to the next row.
// see G12 port for sample implementation
int vid_get_viewport_row_offset() {
#ifndef THUMB_FW
    return (vid_get_viewport_byte_width() * vid_get_viewport_yscale()) - (vid_get_viewport_width_proper() * 6 / 4);
#else
    return (vid_get_viewport_byte_width() * vid_get_viewport_yscale()) - (vid_get_viewport_width_proper() * 4 / 2);
#endif
}

//----------------------------------------------------------------------------

#ifdef CAM_CLEAN_OVERLAY
/*
 * Making the Canon overlay invisible under selected conditions
 * Meant to be used on DIGIC 6 models that allow HDMI output in rec mode
 * On m3 and m10, same palette (0) is used in rec mode and during recording
 * Issues:
 * - regardless of setting, modes with color submode icon (such as hybrid auto) continue to display that (blue) icon
 * - when exposure controls are used, drop shadow of invisible osd elements may appear on screen
 */
extern char **palette_buffer_ptr[];
extern char active_palette_buffer;
extern char palette_control;
static char *palbackup = NULL;      // backup for the original palette
static int palette_is_clean = 0;
static long pending_screenerase = 0;
const unsigned int handled_palette = 0; // model dependent constant, 0 for m3 and m10
void pal_clean() {
    if (palette_is_clean) return;
    if (active_palette_buffer == handled_palette) {
        if (palbackup == NULL) {
            palbackup = malloc(256*4); // 256 entries, 4 bytes each
            if (palbackup) {
                memcpy(palbackup, *palette_buffer_ptr[handled_palette]+4, 256*4);
            }
        }
        if (palbackup) {
            memset(*palette_buffer_ptr[handled_palette]+4, 0, 256*4);
            pending_screenerase = get_tick_count() + 50; // arbitrary 50msec delay
            palette_is_clean = 1;
        }
    }
}
void pal_restore() {
    if (!palette_is_clean || (palbackup == NULL)) return;
    memcpy(*palette_buffer_ptr[handled_palette]+4, palbackup, 256*4);
    pending_screenerase = get_tick_count() + 10; // arbitrary 10msec delay
    palette_is_clean = 0;
}
void handle_clean_overlay() {
    if ((conf.clean_overlay == 1) && camera_info.state.mode_rec) { // clean during rec mode
        pal_clean();
    }
    else if ((conf.clean_overlay == 2) && is_video_recording()) { // clean during video recording only
        pal_clean();
    }
    else {
        pal_restore();
    }
    if (pending_screenerase && (get_tick_count()>=pending_screenerase) && !draw_is_suspended()) {
        pending_screenerase = 0;

#if 1
        // following seems to be effective for removing/redrawing Canon overlay
        palette_control |= 3; // magic constant, valid for m10 and m3
        vid_bitmap_refresh();
#else
        // following is for the case when palette_control + vid_bitmap_refresh is not effective
        extern void _displayblankscreen();
        extern void _undisplayblankscreen();
        _displayblankscreen();
        _undisplayblankscreen();
#endif
    }
}
#endif // CAM_CLEAN_OVERLAY

#ifdef CAM_UNLOCK_ANALOG_AV_IN_REC
void handle_analog_av_in_rec(void) {
    extern void SetVideoOutType(int);
    extern int GetVideoOutType(void);
    if (!camera_info.state.mode_rec) {
        return;
    }
    int vot = GetVideoOutType();
    if(conf.unlock_av_out_in_rec) {
        if(get_analog_av_physw_mod() && vot == 0) {
            SetVideoOutType(shooting_get_analog_video_standard());
        } else if(!get_analog_av_physw_mod() && vot != 0) {
            SetVideoOutType(0);
        }
    } else {
        if(vot != 0) {
            SetVideoOutType(0);
        }
    }
}
#endif  // CAM_UNLOCK_ANALOG_AV_IN_REC
//----------------------------------------------------------------------------
