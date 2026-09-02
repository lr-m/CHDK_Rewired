#include "platform.h"
#include "raw_buffer.h"
#include "conf.h"
#include "raw.h"
#include "console.h"
#include "math.h"
#include "modules.h"
#include "shot_histogram.h"
#include "gui_lang.h"
#include "gui_mbox.h"
#include "cachebit.h"
#include "remotecap_core.h"
#include "ptp.h" // for remotecap constants
#include "script_api.h" // for script hook
#include "raw_ev_histo.h"
#include "viewport.h"
#include "bend.h"
#include "bendx.h"
#include "bend_seg.h"
#include "bend_shot.h"
#include "bend_tag.h"
#include "mexp.h"
#include "mexp_ghost.h"
#include "file_counter.h"

//-------------------------------------------------------------------
#ifdef CAM_DATE_FOLDER_NAMING
  #define RAW_TARGET_DIRECTORY    "A/DCIM/101___01"
#else
  #define RAW_TARGET_DIRECTORY    "A/DCIM/100CANON"
#endif

//#define RAW_TMP_FILENAME        "HDK_RAW.TMP"
#define RAW_TARGET_FILENAME     "%s%04d%s"
#define RAW_BRACKETING_FILENAME "%s%04d_%02d%s" 

//-------------------------------------------------------------------
#define RAW_DEVELOP_OFF     0
#define RAW_DEVELOP_RAW     1
#define RAW_DEVELOP_DNG     2

static char fn[64];
static int develop_raw = RAW_DEVELOP_OFF;

//-------------------------------------------------------------------
void raw_prepare_develop(const char* filename, int prompt)
{
    develop_raw = RAW_DEVELOP_OFF;
    if (filename)
    {
        struct stat st;
        if ((stat(filename,&st) != 0) || (st.st_size < camera_sensor.raw_size))
            return;
        if (prompt)
            gui_mbox_init((int)"", LANG_RAW_DEVELOP_MESSAGE, MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
        if (st.st_size == camera_sensor.raw_size)
            develop_raw = RAW_DEVELOP_RAW;
        else
            develop_raw = RAW_DEVELOP_DNG;
        strcpy(fn,filename);
    }
}

//-------------------------------------------------------------------
void patch_bad_pixels(void);
void raw_bitbend(void);
#ifdef CAM_BEND_EXPERIMENTAL
void raw_bendx(void);
#endif
void raw_bend_shot(void);
int raw_mexp(void);
//-------------------------------------------------------------------

char* get_raw_image_addr(void) {
    char *r=hook_raw_image_addr();
    if (!conf.raw_cache) return r;
    else return ADR_TO_CACHED(r);
}

char* get_alt_raw_image_addr(void) {    // return inactive buffer for cameras with multiple RAW buffers (otherwise return active buffer)
    char *r=hook_alt_raw_image_addr();
    if (!conf.raw_cache) return r;
    else return ADR_TO_CACHED(r);
}
//-------------------------------------------------------------------
// Get path to save raw files based on user settings
void raw_get_path(char *path)
{
    char rdir[32];

    switch ( conf.raw_in_dir )
    {
        case 2:
            strcpy(path,"A/RAW");
            mkdir_if_not_exist(path);
            get_target_dir_name(rdir);
            strcat(path, &rdir[6]) ;
            break ;
        case 1:
            get_target_dir_name(path);
            break ;
        default:
            strcpy(path, RAW_TARGET_DIRECTORY);
            break ;
    }
    mkdir_if_not_exist(path);
    strcat(path, "/");
}

/*
create a new raw file and return the file descriptor from open
name and 
create directories as need
should only be called in raw hook
modifies global fn
returns -1 if not enough space for one raw
*/
static int raw_create_time; // time raw file was opened, for time stamp
static int raw_br_counter;  // bracketing counter for raw suffix
int raw_createfile(void)
{
    int fd;

    // fail if less than one raw + jpeg space free
    if(GetRawCount() < 1) {
        return -1;
    }
    raw_create_time = time(NULL);
    
    raw_get_path(fn);

    if(raw_br_counter && conf.bracketing_add_raw_suffix && (shooting_get_drive_mode()==1)) {
        sprintf(fn+strlen(fn), 
                RAW_BRACKETING_FILENAME,
                img_prefixes[conf.raw_prefix],
                get_target_file_num(),
                raw_br_counter,
                conf.dng_raw&&conf.raw_dng_ext ? ".DNG" : img_exts[conf.raw_ext]);
    } else {
        sprintf(fn+strlen(fn),
                RAW_TARGET_FILENAME,
                img_prefixes[conf.raw_prefix],
                get_target_file_num(),
                conf.dng_raw&&conf.raw_dng_ext ? ".DNG" : img_exts[conf.raw_ext]);
    }
    fd = open(fn, O_WRONLY|O_CREAT, 0777);

    return fd;
}

/*
close filed opened by raw_createfile, set timestamp
*/
void raw_closefile(int fd)
{
    if(fd < 0) {
        return;
    }

    struct utimbuf t;
    t.actime = t.modtime = raw_create_time;
    close(fd);
    utime(fn, &t);

}

// Set in raw_process and used in get_raw_pixel & set_raw_pixel (for performance)
// Don't call set/get_raw_pixel until this value is initialised
// Which phase of raw_process() is running, for the on-screen gate readout.
// 0 idle, 1 bitbend, 2 bendx, 3 mexp, 4 bend_shot, 5 savefile, 6 past the bend
// engine and into Canon's own work.
//
// This exists because "where do the 14 seconds go" has now been guessed wrong
// three times - the redraw gate, the review flag, the experimental chain - and
// each guess cost a card write and a shot on the body. The whole question is
// answered by two numbers on a line that is already being drawn.
int raw_stage;

static char *rawadr;    // Pointer to current raw image buffer

// handle actual raw / dng saving to SD
// returns 1 on successful save, otherwise 0
static int raw_savefile(char *rawadr, char *altrawadr) {
    int ret = 0;
    started();
    int timer=get_tick_count();
    if (conf.dng_raw)
    {
        ret = libdng->write_dng(rawadr, altrawadr);
    }
    else 
    {
        int fd = raw_createfile();
        if(fd >= 0) {
            // Write active RAW buffer
            write(fd, ADR_TO_UNCACHED(rawadr), camera_sensor.raw_size);
            ret = 1;
            raw_closefile(fd);
        }
    }

    if (conf.raw_timer) {
        char txt[30];
        timer=get_tick_count()-timer;
        sprintf(txt, "saving time=%d", timer);
        console_add_line(txt);
    }

    finished();
    return ret;
}

// processing done when raw hook runs available
void raw_process(void)
{
    // Get pointers to RAW buffers (will be the same on cameras that don't have two or more buffers)
    rawadr = get_raw_image_addr();
    char *altrawadr = get_alt_raw_image_addr();

#if defined(CAM_CALC_BLACK_LEVEL)
    int v1 = get_raw_pixel(4, 4);
    int v2 = get_raw_pixel(4, 5);
    int v3 = get_raw_pixel(5, 4);
    int v4 = get_raw_pixel(5, 5);
    int raw_calc_black_level = (v1 + v2 + v3 + v4) / 4;
    if (raw_calc_black_level > CAM_BLACK_LEVEL * 2)
        camera_sensor.black_level = raw_calc_black_level;
    else
        camera_sensor.black_level = CAM_BLACK_LEVEL;
#endif

    if ((conf.save_raw && conf.dng_raw && is_raw_enabled()) 
        || (camera_info.remotecap.file_target & PTP_CHDK_CAPTURE_DNGHDR))
    {                             
        libdng->capture_data_for_exif();
	}

    if (camera_info.state.state_kbd_script_run)
        libshothisto->build_shot_histogram();

    if (is_raw_enabled())       // make sure it is safe to use raw buffers
        librawevhisto->build();

    libscriptapi->shoot_hook(SCRIPT_SHOOT_HOOK_RAW);

    // count/save badpixels if requested
    if (libdng->raw_init_badpixel_bin())
    {
        return;
    }

    if (develop_raw != RAW_DEVELOP_OFF)
    {
        started();
        if (develop_raw == RAW_DEVELOP_DNG)
        {
            libdng->load_dng_to_rawbuffer(fn, rawadr);
        }
        else
        {
            int fd = open(fn, O_RDONLY, 0777);
            if (fd >= 0) {
                read(fd, rawadr, camera_sensor.raw_size);
                close(fd);
            }
        }
#ifdef OPT_CURVES
        if (conf.curve_enable)
            libcurves->curve_apply();
#endif
        finished();
        develop_raw = RAW_DEVELOP_OFF;
        return;
    }

    if (conf.bad_pixel_removal) patch_bad_pixels();

    // Decide from the sequence's latched setting, not from an estimate of
    // whether this exposure ought to finish it. raw_mexp() reports completion
    // only after it has successfully produced the final composite.
    int bend_each = !conf.mexp_enable ||
        (mexp_count ? mexp_bend_each_used : conf.mexp_bend_each);
    int composite_done;

    if (bend_each)
        raw_bitbend();

#ifdef CAM_BEND_EXPERIMENTAL
    if (bend_each)
        raw_bendx();
#endif

    composite_done = raw_mexp();

    if (!bend_each && composite_done)
    {
        raw_bitbend();
#ifdef CAM_BEND_EXPERIMENTAL
        raw_bendx();
#endif
    }

    // And now write down what that was, beside the picture it made. After both
    // engines rather than before, so what is recorded is what was applied -
    // bend_sanitize() and bend_simplify() can both rewrite the matrix on the
    // way through, and a recipe that does not reproduce the frame is worse
    // than none.
    // In composite-only mode the intermediate JPEGs are deliberately clean,
    // so do not attach a bend recipe claiming an effect was applied to them.
    if (bend_each || composite_done)
    {
        raw_stage = 4;
        raw_bend_shot();
    }

    raw_stage = 6;              // out of the bend engine; Canon's work from here
    shooting_bracketing();

    if (conf.tv_bracket_value || conf.av_bracket_value || conf.iso_bracket_value || conf.subj_dist_bracket_value)
    {
        if (camera_info.state.state_shooting_progress != SHOOTING_PROGRESS_PROCESSING)
            raw_br_counter = 1;
        else
            raw_br_counter++;
    }
    else
        raw_br_counter=0;

    // if any remote cap targets, skip local raw
    if (camera_info.remotecap.file_target)
    {
        camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_PROCESSING;
        remotecap_raw_available(rawadr);
    }
    else if (!(conf.raw_save_first_only && camera_info.state.state_shooting_progress == SHOOTING_PROGRESS_PROCESSING))
    {
        camera_info.state.state_shooting_progress = SHOOTING_PROGRESS_PROCESSING;

        if (conf.save_raw && is_raw_enabled())
        {
            raw_stage = 5;
            raw_savefile(rawadr,altrawadr);
        }
    }

#ifdef OPT_CURVES
    if (conf.curve_enable)
        libcurves->curve_apply();
#endif
    raw_stage = 0;              // raw_process() done; spytask is free again
}

//-------------------------------------------------------------------

void set_raw_pixel(unsigned int x, unsigned int y, unsigned short value) {
#if CAM_SENSOR_BITS_PER_PIXEL==10
    unsigned char* addr=(unsigned char*)rawadr+y*camera_sensor.raw_rowlen+(x/8)*10;
    switch (x%8) {
        case 0: addr[0]=(addr[0]&0x3F)|(value<<6); addr[1]=value>>2;                  break;
        case 1: addr[0]=(addr[0]&0xC0)|(value>>4); addr[3]=(addr[3]&0x0F)|(value<<4); break;
        case 2: addr[2]=(addr[2]&0x03)|(value<<2); addr[3]=(addr[3]&0xF0)|(value>>6); break;
        case 3: addr[2]=(addr[2]&0xFC)|(value>>8); addr[5]=value;                     break;
        case 4: addr[4]=value>>2;                  addr[7]=(addr[7]&0x3F)|(value<<6); break;
        case 5: addr[6]=(addr[6]&0x0F)|(value<<4); addr[7]=(addr[7]&0xC0)|(value>>4); break;
        case 6: addr[6]=(addr[6]&0xF0)|(value>>6); addr[9]=(addr[9]&0x03)|(value<<2); break;
        case 7: addr[8]=value;                     addr[9]=(addr[9]&0xFC)|(value>>8); break;
    }
#elif CAM_SENSOR_BITS_PER_PIXEL==12
    unsigned char* addr=(unsigned char*)rawadr+y*camera_sensor.raw_rowlen+(x/4)*6;
    switch (x%4) {
        case 0: addr[0] = (addr[0]&0x0F) | (unsigned char)(value << 4);  addr[1] = (unsigned char)(value >> 4);  break;
        case 1: addr[0] = (addr[0]&0xF0) | (unsigned char)(value >> 8);  addr[3] = (unsigned char)value;         break;
        case 2: addr[2] = (unsigned char)(value >> 4);  addr[5] = (addr[5]&0x0F) | (unsigned char)(value << 4);  break;
        case 3: addr[4] = (unsigned char)value; addr[5] = (addr[5]&0xF0) | (unsigned char)(value >> 8);  break;
    }
#elif CAM_SENSOR_BITS_PER_PIXEL==14
    unsigned char* addr=(unsigned char*)rawadr+y*camera_sensor.raw_rowlen+(x/8)*14;
    switch (x%8) {
        case 0: addr[ 0]=(addr[0]&0x03)|(value<< 2); addr[ 1]=value>>6;                                                         break;
        case 1: addr[ 0]=(addr[0]&0xFC)|(value>>12); addr[ 2]=(addr[ 2]&0x0F)|(value<< 4); addr[ 3]=value>>4;                   break;
        case 2: addr[ 2]=(addr[2]&0xF0)|(value>>10); addr[ 4]=(addr[ 4]&0x3F)|(value<< 6); addr[ 5]=value>>2;                   break;
        case 3: addr[ 4]=(addr[4]&0xC0)|(value>> 8); addr[ 7]=value;                                                            break;
        case 4: addr[ 6]=value>>6;                   addr[ 9]=(addr[ 9]&0x03)|(value<< 2);                                      break;
        case 5: addr[ 8]=value>>4;                   addr[ 9]=(addr[ 9]&0xFC)|(value>>12); addr[11]=(addr[11]&0x0F)|(value<<4); break;
        case 6: addr[10]=value>>2;                   addr[11]=(addr[11]&0xF0)|(value>>10); addr[13]=(addr[13]&0x3F)|(value<<6); break;
        case 7: addr[12]=value;                      addr[13]=(addr[13]&0xC0)|(value>> 8);                                      break;
    }
#else 
    #error define set_raw_pixel for sensor bit depth
#endif
}

//-------------------------------------------------------------------
unsigned short get_raw_pixel(unsigned int x,unsigned  int y) {
#if CAM_SENSOR_BITS_PER_PIXEL==10
    unsigned char* addr=(unsigned char*)rawadr+y*camera_sensor.raw_rowlen+(x/8)*10;
    switch (x%8) {
        case 0: return ((0x3fc&(((unsigned short)addr[1])<<2)) | (addr[0] >> 6));
        case 1: return ((0x3f0&(((unsigned short)addr[0])<<4)) | (addr[3] >> 4));
        case 2: return ((0x3c0&(((unsigned short)addr[3])<<6)) | (addr[2] >> 2));
        case 3: return ((0x300&(((unsigned short)addr[2])<<8)) | (addr[5]));
        case 4: return ((0x3fc&(((unsigned short)addr[4])<<2)) | (addr[7] >> 6));
        case 5: return ((0x3f0&(((unsigned short)addr[7])<<4)) | (addr[6] >> 4));
        case 6: return ((0x3c0&(((unsigned short)addr[6])<<6)) | (addr[9] >> 2));
        case 7: return ((0x300&(((unsigned short)addr[9])<<8)) | (addr[8]));
    }
#elif CAM_SENSOR_BITS_PER_PIXEL==12
    unsigned char* addr=(unsigned char*)rawadr+y*camera_sensor.raw_rowlen+(x/4)*6;
    switch (x%4) {
        case 0: return ((unsigned short)(addr[1])        << 4) | (addr[0] >> 4);
        case 1: return ((unsigned short)(addr[0] & 0x0F) << 8) | (addr[3]);
        case 2: return ((unsigned short)(addr[2])        << 4) | (addr[5] >> 4);
        case 3: return ((unsigned short)(addr[5] & 0x0F) << 8) | (addr[4]);
    }
#elif CAM_SENSOR_BITS_PER_PIXEL==14
    unsigned char* addr=(unsigned char*)rawadr+y*camera_sensor.raw_rowlen+(x/8)*14;
    switch (x%8) {
        case 0: return ((unsigned short)(addr[ 1])        <<  6) | (addr[ 0] >> 2);
        case 1: return ((unsigned short)(addr[ 0] & 0x03) << 12) | (addr[ 3] << 4) | (addr[ 2] >> 4);
        case 2: return ((unsigned short)(addr[ 2] & 0x0F) << 10) | (addr[ 5] << 2) | (addr[ 4] >> 6);
        case 3: return ((unsigned short)(addr[ 4] & 0x3F) <<  8) | (addr[ 7]);
        case 4: return ((unsigned short)(addr[ 6])        <<  6) | (addr[ 9] >> 2);
        case 5: return ((unsigned short)(addr[ 9] & 0x03) << 12) | (addr[ 8] << 4) | (addr[11] >> 4);
        case 6: return ((unsigned short)(addr[11] & 0x0F) << 10) | (addr[10] << 2) | (addr[13] >> 6);
        case 7: return ((unsigned short)(addr[13] & 0x3F) <<  8) | (addr[12]);
    }
#else 
    #error define get_raw_pixel for sensor bit depth
#endif
    return 0;
}

//-------------------------------------------------------------------
// Bit bending
//
// A bend is a routing matrix - for every output bit, where its value comes
// from. See include/bend.h and BENDING_DESIGN.md. The maths lives in
// core/bend.c so tools/bend_selftest.c can check it on the host; this file
// holds the appliers, the code that walks a buffer and feeds pixels through.
//
// Two paths, chosen by whether the bend uses any non-data bus:
//
//   pure    every output bit comes from a data pin or a rail, so the bend is
//           a pure function of the pixel value, collapses to a lookup table,
//           and the packed 8-pixel fast path applies unchanged
//
//   bussed  at least one bit is fed by a clock, the black level, noise or a
//           held sample, so the result depends on position and on history and
//           has to be evaluated per pixel
//
// A bend that only rewires data pins therefore costs exactly what it did
// before the matrix landed. You pay for the buses only when you patch one in.

#define BB_NBITS        CAM_SENSOR_BITS_PER_PIXEL
#define BB_LUT_SIZE     (1 << BB_NBITS)
#define BB_MAX          (BB_LUT_SIZE - 1)

// The compiled data-pin mapping, one entry per possible sensor value.
//
// Allocated on first use rather than reserved statically. At 12 bits per pixel
// that is 4096 entries of unsigned short = 8192 bytes, and it used to sit in
// .bss permanently even with bending switched off. On the A480 that mattered:
// CHDK is loaded into AgentRAM there, the whole region is 204800 bytes, and the
// core had grown to within 480 bytes of it - which left no room for the custom
// boot screen and silently broke it (see include/boot_screen.h and
// loader/a480/main.c). Handing this table to the heap gives that space back.
//
// bb_run() allocates it before bend_compile() and skips bending entirely if the
// allocation fails, so the failure mode is an unbent frame rather than a frame
// bent through a null pointer. It is kept for the life of the boot once taken -
// this is not a per-shot allocation.
static unsigned short *bb_lut;
static bend_cc_t      bb_cc;

static int bb_lut_ready(void)
{
    if (!bb_lut)
        bb_lut = (unsigned short *)malloc(BB_LUT_SIZE * sizeof(*bb_lut));
    return bb_lut != 0;
}

// Optical black column for BUS_OB. This camera reads 3152 pixels per row but
// its active area starts at x=12, so columns 0..11 are physically masked
// sensor carrying the real per-row dark current and reset noise. Sampling it
// makes "short the black level clamp onto a data line" the actual bend rather
// than an imitation of one - it is the same number the clamp circuit uses.
#ifdef CAM_ACTIVE_AREA_X1
  #define BB_OB_X       ((CAM_ACTIVE_AREA_X1 >= 4) ? (CAM_ACTIVE_AREA_X1 - 4) : 0)
#else
  #define BB_OB_X       0
#endif

static unsigned bb_frame;       // BUS_FRAME - counts shots and live frames

// Whole-row fast path for the pure, unscoped case. A packed group holds
// several pixels, so unpacking a group at a time avoids the address arithmetic
// and the switch dispatch that get_raw_pixel/set_raw_pixel do for every single
// pixel. The bit layouts below are taken straight from those two functions -
// see tools/bend_selftest.c, which checks them against the real accessors for
// every phase at both depths.
//
// Worth having: the a470 is 3152x2346 and the a480 is 3720x2772, so this runs
// 7.4 and 10.3 million times a shot respectively.

#define BB_HAS_FAST_PATH \
    (CAM_SENSOR_BITS_PER_PIXEL == 10 || CAM_SENSOR_BITS_PER_PIXEL == 12)

#if CAM_SENSOR_BITS_PER_PIXEL == 12
// Six packed bytes hold four pixels.
static void bb_row_fast(unsigned char *p, unsigned int rowpix)
{
    unsigned int groups = rowpix >> 2;
    unsigned int i;

    for (i = 0; i < groups; i++, p += 6)
    {
        unsigned int p0 = ((unsigned int)p[1] << 4) | (p[0] >> 4);
        unsigned int p1 = ((unsigned int)(p[0] & 0x0f) << 8) | p[3];
        unsigned int p2 = ((unsigned int)p[2] << 4) | (p[5] >> 4);
        unsigned int p3 = ((unsigned int)(p[5] & 0x0f) << 8) | p[4];

        p0 = bb_lut[p0]; p1 = bb_lut[p1]; p2 = bb_lut[p2]; p3 = bb_lut[p3];

        p[0] = (unsigned char)(((p0 & 0x00f) << 4) | ((p1 >> 8) & 0x0f));
        p[1] = (unsigned char)((p0 >> 4) & 0xff);
        p[2] = (unsigned char)((p2 >> 4) & 0xff);
        p[3] = (unsigned char)(p1 & 0xff);
        p[4] = (unsigned char)(p3 & 0xff);
        p[5] = (unsigned char)(((p2 & 0x00f) << 4) | ((p3 >> 8) & 0x0f));
    }
}
#endif

#if CAM_SENSOR_BITS_PER_PIXEL == 10
// Ten packed bytes hold eight pixels.
static void bb_row_fast(unsigned char *p, unsigned int rowpix)
{
    unsigned int groups = rowpix >> 3;
    unsigned int i;

    for (i = 0; i < groups; i++, p += 10)
    {
        unsigned int p0 = ((0x3fc & (p[1] << 2)) | (p[0] >> 6));
        unsigned int p1 = ((0x3f0 & (p[0] << 4)) | (p[3] >> 4));
        unsigned int p2 = ((0x3c0 & (p[3] << 6)) | (p[2] >> 2));
        unsigned int p3 = ((0x300 & (p[2] << 8)) | (p[5]));
        unsigned int p4 = ((0x3fc & (p[4] << 2)) | (p[7] >> 6));
        unsigned int p5 = ((0x3f0 & (p[7] << 4)) | (p[6] >> 4));
        unsigned int p6 = ((0x3c0 & (p[6] << 6)) | (p[9] >> 2));
        unsigned int p7 = ((0x300 & (p[9] << 8)) | (p[8]));

        p0 = bb_lut[p0]; p1 = bb_lut[p1]; p2 = bb_lut[p2]; p3 = bb_lut[p3];
        p4 = bb_lut[p4]; p5 = bb_lut[p5]; p6 = bb_lut[p6]; p7 = bb_lut[p7];

        p[0] = (unsigned char)(((p0 & 0x003) << 6) | ((p1 >> 4) & 0x3f));
        p[1] = (unsigned char)((p0 >> 2) & 0xff);
        p[2] = (unsigned char)(((p2 & 0x03f) << 2) | ((p3 >> 8) & 0x03));
        p[3] = (unsigned char)(((p1 & 0x00f) << 4) | ((p2 >> 6) & 0x0f));
        p[4] = (unsigned char)((p4 >> 2) & 0xff);
        p[5] = (unsigned char)(p3 & 0xff);
        p[6] = (unsigned char)(((p5 & 0x00f) << 4) | ((p6 >> 6) & 0x0f));
        p[7] = (unsigned char)(((p4 & 0x003) << 6) | ((p5 >> 4) & 0x3f));
        p[8] = (unsigned char)(p7 & 0xff);
        p[9] = (unsigned char)(((p6 & 0x03f) << 2) | ((p7 >> 8) & 0x03));
    }
}
#endif

//-------------------------------------------------------------------
// Capture path. Called from raw_process() before the buffer is saved and
// before Canon develops the JPEG from it, so the damage lands in both.
//
// One pass per segment, each pass compiling its own matrix and walking only
// the spans of each row that belong to it. That is one lookup table alive at a
// time instead of four - two kilobytes of it, which is worth caring about on
// these bodies - and the total pixel work is unchanged, because no pixel is in
// two regions. What the extra passes cost is the row loop, four times over a
// couple of thousand rows, which is nothing beside the pixels.
//
// The fast path survives whole rows only. A horizontal split therefore costs
// what an unsegmented bend costs; a vertical one or a circle drops every row
// it cuts onto the unpack-and-repack loop, the same one a bus-fed bend uses.

// Repaint the persistent overlay from inside the long capture loops.
//
// raw_process() runs the whole bend engine, and it runs *in spytask*. Spytask's
// own call to gui_redraw() is what puts the overlay back after a shot - but the
// branch that calls raw_process() ends in `continue`, so that redraw is skipped
// for the entire duration of the work. The overlay therefore stayed off screen
// for exactly as long as bending took, which is why a heavier effect chain made
// the wait longer: the delay was the bend itself, not a timer and not Canon's
// review.
//
// This is the ordinary redraw, made from inside the work instead of after it.
// Same task and same context as normal, so nothing about the drawing changes -
// and every ownership gate still applies, so nothing is painted while Canon's
// review owns the screen. Throttled to ~10Hz, which is invisible against the
// per-pixel loops it sits in.
//
// mode_get() has to come with it, and leaving it out is what made this whole
// mechanism a no-op. gui_redraw() paints the persistent overlay only if
// posd_screen_active() agrees, and two of that function's terms - mode_rec and
// mode_play - are not read from Canon when they are tested. They are cached in
// camera_info.state, and the only thing that refreshes them is mode_get() at
// the *top of the spytask loop* - the one place raw_process() guarantees is not
// reached, because the branch that calls it ends in `continue`.
//
// So for the whole of a bend those two terms hold whatever they were when
// capture started, which is not the live record screen, so the overlay was
// gated off for exactly as long as the work took and came back the moment
// spytask got back to the loop head. Measured on the A470 at 14s for a 14s
// bend. Every previous attempt at this - the redraw gate in core/main.c, the
// review-flag level test, the timeout, the falling edge - was aimed at a gate
// that was not the one holding it shut, which is why none of them moved it.
//
// This is the same call the loop head makes, in the same task, at a tenth of
// the rate.
#define RAW_UI_SERVICE_MS 100

static int raw_ui_last;

static void raw_service_ui(void)
{
#ifdef CAM_BEND_YIELD_MS
    // Hand the CPU back, whatever this body does about the overlay.
    //
    // The bend runs in spytask and never blocks, so on a body where spytask is
    // not below Canon's imaging tasks it holds the processor for the length of
    // the pass - seconds - and the capture path times out behind it.
    //
    // Throttled by the clock rather than taken at every service point. The
    // points are every 64th row, but there is one pass per segment and another
    // per bendx profile, so a chain over a segmented frame reaches this a few
    // hundred times; sleeping at each one turned a bend into something you wait
    // through. A sleep is also not the length it asks for - it is the length it
    // asks for plus however long it takes spytask to be scheduled again - so
    // the only way to bound the cost is to bound how often it happens.
    // CAM_BEND_YIELD_MS per CAM_BEND_YIELD_EVERY_MS of work is the ceiling,
    // whatever the engines above do.
    {
        static int yield_last;
        int yt = get_tick_count();
        if (!yield_last || (unsigned)(yt - yield_last) >= CAM_BEND_YIELD_EVERY_MS)
        {
            yield_last = yt;
            msleep(CAM_BEND_YIELD_MS);
        }
    }
#endif
#ifndef CAM_POSD_SERVICE_UI_IN_CAPTURE
    // Off unless the port asks for it - see camera.h. Repainting during capture
    // puts the overlay back on a screen Canon is about to freeze for the review
    // on any body whose hold cannot track the real review.
    return;
#else
    extern void gui_redraw(void);
    extern int  mode_get(void);
    int t = get_tick_count();
    if (raw_ui_last && (unsigned)(t - raw_ui_last) < RAW_UI_SERVICE_MS) return;
    raw_ui_last = t;
    mode_get();
    gui_redraw();
#ifdef CAM_POSD_GATE_DEBUG
    // Drawn from here as well as from spytask. spytask does not reach its own
    // call to this for the whole of raw_process(), so without this the readout
    // is frozen during exactly the stall it is meant to explain - and a frozen
    // line is indistinguishable from a line reporting nothing has changed.
    { extern void posd_gate_debug_draw(void); posd_gate_debug_draw(); }
#endif
#endif // CAM_POSD_SERVICE_UI_IN_CAPTURE
}

static void bb_run(bend_t *b, int layout, int size, int seg)
{
    unsigned int x, y;
    unsigned int w = camera_sensor.raw_rowpix;
    unsigned int h = camera_sensor.raw_rows;
    int period, chan;

    // No table, no bend. Checked before anything is written to the frame.
    if (!bb_lut_ready()) return;

    bend_sanitize(b, BB_NBITS);
    // Held to the pure sources before compiling rather than filtered after,
    // so what the pin strip shows is what gets applied. See bend_simplify():
    // with a bus in the matrix this function drops off bb_row_fast() onto a
    // loop that unpacks and repacks every pixel of the sensor by hand.
    if (conf.bitbend_simple) bend_simplify(b);
    bend_compile(b, &bb_cc, bb_lut, BB_NBITS);
    if (bb_cc.is_identity) return;

    period = b->row_period;
    chan   = b->bayer;              // 0 = every pixel, 1..4 = Bayer position

    for (y = 0; y < h; y++)
    {
        unsigned char *rowp;
        bend_src_t src;
        unsigned noise_state;
        unsigned char  segs[BEND_SEG_SPANS];
        unsigned short xend[BEND_SEG_SPANS];
        int nsp, i, mine = 0;

        if (period > 1 && (y % period) != 0) continue;

        // Keep the UI alive through the bend - see raw_service_ui().
        if ((y & 0x3f) == 0) raw_service_ui();

        nsp = bend_seg_spans(layout, size, (int)y, (int)w, (int)h, segs, xend);
        for (i = 0; i < nsp; i++) if (segs[i] == seg) { mine = 1; break; }
        // Rows this segment has no part of are the majority of the frame for
        // every layout but Off, and skipping them here is what makes the pass
        // per segment cheap enough to be worth doing at all.
        if (!mine) continue;

        rowp = (unsigned char*)rawadr + y * camera_sensor.raw_rowlen;

        // Per-row bus sources. The optical black sample is taken before the
        // row is touched, so it is the sensor's value and not one we just bent.
        src.frame = bb_frame;
        src.ob    = (bb_cc.bus_used & (1u << BUS_OB)) ? (get_raw_pixel(BB_OB_X, y) & BB_MAX) : 0;
        src.prev  = 0;
        src.above = 0;
        src.tap   = 0;
        src.noise = 0;
        noise_state = bend_trash_seed(b, y, src.ob);

        for (i = 0; i < nsp; i++)
        {
            unsigned int x0 = (i > 0) ? xend[i - 1] : 0;
            unsigned int x1 = xend[i];

            if (segs[i] != (unsigned char)seg) continue;

#if BB_HAS_FAST_PATH
            if (bb_cc.is_pure && chan == 0)
            {
                // Pixels per packed group: 8 at 10bpp, 4 at 12bpp.
                const unsigned gp = (CAM_SENSOR_BITS_PER_PIXEL == 10) ? 8u : 4u;
                const unsigned gb = (CAM_SENSOR_BITS_PER_PIXEL == 10) ? 10u : 6u;
                unsigned int a0 = (x0 + gp - 1u) & ~(gp - 1u);
                unsigned int a1 = x1 & ~(gp - 1u);

                // Segment edges can cut through a packed group. Handle only
                // those few boundary pixels generically, then run the same
                // packed LUT path used by an unsegmented row over every whole
                // group contained by the span. Previously any vertical or
                // curved boundary made the entire span take the per-pixel
                // accessor path, which dominates capture time on the A460.
                if (a0 > x1) a0 = x1;
                for (x = x0; x < a0; x++)
                    set_raw_pixel(x, y, bb_lut[get_raw_pixel(x, y) & BB_MAX]);
                if (a1 > a0)
                    bb_row_fast(rowp + (a0 / gp) * gb, a1 - a0);
                for (x = (a1 > a0) ? a1 : a0; x < x1; x++)
                    set_raw_pixel(x, y, bb_lut[get_raw_pixel(x, y) & BB_MAX]);
                continue;
            }
#endif

            // The held sample entering a span is the pixel to its left, which
            // belongs to the segment next door and has already been bent by
            // its own pass. That is deliberate: on hardware the sample and
            // hold does not know where a region boundary is, and letting the
            // smear cross the seam is what stops a segmented bend looking like
            // four photographs stuck together.
            src.prev = (x0 > 0) ? (get_raw_pixel(x0 - 1, y) & BB_MAX) : 0;

            for (x = x0; x < x1; x++)
            {
                unsigned v;

                if (chan && (int)(((y & 1) << 1) | (x & 1)) != chan - 1) continue;

                v = get_raw_pixel(x, y) & BB_MAX;

                if (bb_cc.bus_used & (1u << BUS_NOISE))
                    src.noise = bend_trash_step(b, &noise_state, x);

                // The row above has already been bent by the time we read it,
                // and the tap usually points backwards too. That feedback is
                // wanted - it is what makes a held sample smear along the scan
                // instead of just tinting one pixel - and it cannot run away,
                // because every bus contributes single bits, not accumulating
                // magnitudes.
                if (bb_cc.bus_used & (1u << BUS_VHOLD))
                    src.above = (y > 0) ? (get_raw_pixel(x, y - 1) & BB_MAX) : 0;

                if (bb_cc.bus_used & (1u << BUS_TAP))
                {
                    int tx = (int)x + b->tap_dx;
                    int ty = (int)y + b->tap_dy;
                    if (tx < 0) tx = 0;
                    if (ty < 0) ty = 0;
                    if (tx >= (int)w) tx = (int)w - 1;
                    if (ty >= (int)h) ty = (int)h - 1;
                    src.tap = get_raw_pixel(tx, ty) & BB_MAX;
                }

                set_raw_pixel(x, y, bend_eval(&bb_cc, v, (int)x, (int)y, &src));
                src.prev = v;
            }
        }
    }
}

void raw_bitbend(void)
{
    int layout, size, i, n;

    if (!conf.bitbend_enable) return;
    raw_stage = 1;

    // The same gate the UI uses, applied here as well because a config block
    // restored from a card is not required to have been through the UI at all.
    bend_seg_sanitize(&conf.bend_segs, BB_NBITS);
    layout = conf.bend_segs.layout;
    size   = conf.bend_segs.size;
    n      = bend_seg_count(layout);

    bb_frame++;

    // Re-roll the pattern layouts, once per shot. The tick count is what makes
    // it differ between shots and bb_frame is what makes two shots inside the
    // same tick differ, which matters because these bodies count ticks in
    // milliseconds and a burst can land two captures in one.
    //
    // Deliberately not conf.bitbend.seed: that field exists so a saved bend
    // reproduces, and the point of this one is that it does not. The geometric
    // layouts ignore it entirely.
    bend_seg_set_seed((unsigned)get_tick_count() ^ (bb_frame * 2654435761u));

    started();
    // In order, and the order matters only where the buses read pixels the
    // pass before has already written - which is the seam behaviour described
    // in bb_run(), and is why this is a plain forward loop rather than
    // something that tries to be clever about which region is on top.
    for (i = 0; i < n; i++)
    {
        raw_service_ui();
        bb_run(bend_seg_conf(i), layout, size, i);
    }
    finished();
}

//-------------------------------------------------------------------
// Experimental profiles - see include/bendx.h and docs/EXPERIMENTAL_EFFECTS.md
//
#ifdef CAM_BEND_EXPERIMENTAL
//
// The engine addresses the buffer itself rather than being handed one pixel at
// a time, so everything it needs to stay inside comes from here: the geometry
// from camera_sensor, and the three foreign buffers from the accessors the
// port already has. bendx.c contains no address and no camera constant, which
// is the point - the only way a foreign read goes out of bounds is a length
// declared wrongly on this side.

// The ROM window the ROM bus effect reads from.
//
// One megabyte from the base, which every body this builds for has: the a480
// carries four (0xffc00000..0xffffffff, confirmed against its own stubs) and
// the DIGIC IV bodies eight. Reading further would buy nothing - a megabyte of
// ARM code and Canon's string tables is more texture than any one frame can
// show - and would need a per-camera size that nothing else in the tree has a
// reason to know.
#ifdef ROMBASEADDR
  #define BX_ROM_BASE   ((const unsigned char*)(ROMBASEADDR))
  #define BX_ROM_LEN    0x00100000
#else
  #define BX_ROM_BASE   ((const unsigned char*)0)
  #define BX_ROM_LEN    0
#endif

void raw_bendx(void)
{
    bendx_chain_t *c = &conf.bendx;
    bendx_buf_t buf;
    bendx_env_t env;
    unsigned char *scratch;
    int i, n;

    if (!conf.bendx_enable) return;
    raw_stage = 2;

    bendx_chain_sanitize(c);
    n = bendx_chain_count(c);
    if (n <= 0) return;

    buf.base   = (unsigned char*)rawadr;
    buf.rowlen = camera_sensor.raw_rowlen;
    buf.rowpix = camera_sensor.raw_rowpix;
    buf.rows   = camera_sensor.raw_rows;
    buf.ob_x   = BB_OB_X;
    buf.nbits  = CAM_SENSOR_BITS_PER_PIXEL;

    env.rom     = BX_ROM_BASE;
    env.rom_len = BX_ROM_LEN;

    // The JPEG encoder's buffer, on the ports that have reversed it. Null
    // elsewhere, which turns the JPEG bus into a dead list entry rather than
    // an address nobody checked - see hook_jpeg_buffer().
    {
        unsigned jl = 0;
        env.jpg     = (const unsigned char*)hook_jpeg_buffer(&jl);
        env.jpg_len = jl;
    }

    env.frame = bb_frame;
    env.black = camera_sensor.black_level;
    env.white = camera_sensor.white_level;

    // Whatever the largest member of the chain asks for - two rows for most of
    // them, more for the sort, which wants a counting histogram wider than two
    // rows are at 10bpp. From the heap, because the capture task's stack is
    // not the place for eleven kilobytes. Failing to get it means the shot
    // goes out unbent, which is the right failure - the alternative is
    // dropping the picture.
    scratch = malloc(bendx_chain_scratch_bytes(c, buf.rowlen));
    if (!scratch) return;

    started();

    // Service the overlay from inside each profile's row loop as well as
    // between profiles - one pass over the sensor is seconds of work on these
    // bodies and there was no service point anywhere inside it.
    bendx_set_service(raw_service_ui);

    // In order, each one working on what the last one left. That is what makes
    // a chain worth having rather than being N separate effects averaged: a
    // stuck address line under a decaying refresh is not either of them.
    //
    // The braces are load-bearing and were missing. Without them the loop body
    // was raw_service_ui() alone, bendx_apply() ran once after the loop with
    // i == n, and so:
    //   - a chain of N profiles applied exactly one profile, never the chain;
    //   - that one was item[n], the *live* slot being edited rather than any
    //     locked member - and at a full chain, n == BENDX_CHAIN_MAX, so it was
    //     item[4] of a [4] array, reading past the end of the struct;
    //   - the frame pass ran with no service call inside it at all.
    for (i = 0; i < n; i++)
    {
        raw_service_ui();
        bendx_apply(&c->item[i], &buf, &env, scratch);
    }

    bendx_set_service(0);
    finished();

    free(scratch);
}

#endif // CAM_BEND_EXPERIMENTAL

//-------------------------------------------------------------------
// Multiple exposure - see include/mexp.h and docs/MULTI_EXPOSURE.md.
//
// The applier, in the same position as the two above it: inside the raw hook,
// on the buffer Canon is about to develop the JPEG from. Combining here is
// what puts the composite in the JPEG and in the DNG both, and it is why this
// needs no support from the camera at all - no reversed address, no shooting
// mode, no cooperation from Canon's own bracketing.
//
// Where the exposures live between shots. The sensor is 10.3 million pixels on
// the a480 and 15MB packed, so the accumulator cannot be held in RAM on any of
// these bodies - the whole CHDK heap is a fraction of it. It goes to the card,
// in the sensor's own packed format so the file is exactly one raw and no
// conversion happens on the way in or out.
//
// The cost is real and worth stating plainly: one raw-sized read per exposure
// after the first, and one raw-sized write per exposure except the last. On an
// ordinary card in an ordinary body of this age that is seconds, not
// milliseconds, and it lands between the shots of the sequence. It is a slow
// feature. There is no faster place to put 15MB.
//
// Every exposure in the sequence is still saved by Canon as its own picture -
// nothing here can stop that, the JPEG writer is Canon's - so a two-shot
// sequence leaves two files: the first exposure, and the composite. The
// composite is always the last one.

#define MEXP_TMP_FILE   "A/CHDK/MEXP.TMP"

// Write the raw buffer out as the accumulator. Same shape as raw_savefile()'s
// non-DNG path, and uncached for the same reason: the buffer was written
// through the cache and the card DMA does not see it otherwise.
static int mexp_store(char *adr)
{
    int fd, ok;

    fd = open(MEXP_TMP_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0777);
    if (fd < 0) return 0;

    ok = (write(fd, ADR_TO_UNCACHED(adr), camera_sensor.raw_size)
            == (int)camera_sensor.raw_size);
    close(fd);

    if (!ok) remove(MEXP_TMP_FILE);
    return ok;
}

// Blend the accumulator into the raw buffer, row at a time. Returns 0 and
// leaves the buffer alone if the file is not there or not the right size - a
// card pulled between exposures, or a sequence left over from a body that was
// switched off part way through one.
static int mexp_combine(int mode, unsigned n)
{
    int fd, ok = 1;
    unsigned j;
    unsigned rowlen = camera_sensor.raw_rowlen;
    unsigned rowpix = camera_sensor.raw_rowpix;
    unsigned black  = camera_sensor.black_level;
    unsigned white  = camera_sensor.white_level;
    unsigned char *frow;
    unsigned short *av, *vv;
    struct stat st;

    if (stat(MEXP_TMP_FILE, &st) != 0 || st.st_size != (int)camera_sensor.raw_size)
        return 0;

    frow = malloc(rowlen);
    av   = malloc(rowpix * sizeof(unsigned short));
    vv   = malloc(rowpix * sizeof(unsigned short));
    if (!frow || !av || !vv)
    {
        // Same failure the bendx chain takes when it cannot get its scratch:
        // the exposure goes out on its own rather than the shot being lost.
        if (frow) free(frow);
        if (av)   free(av);
        if (vv)   free(vv);
        return 0;
    }

    fd = open(MEXP_TMP_FILE, O_RDONLY, 0777);
    if (fd < 0)
    {
        free(frow); free(av); free(vv);
        return 0;
    }

    for (j = 0; j < camera_sensor.raw_rows; j++)
    {
        unsigned char *live = (unsigned char*)rawadr + j * rowlen;

        // Reading the accumulator off the card a row at a time is the slowest
        // thing in the capture path, so service the UI here too.
        if ((j & 0x3f) == 0) raw_service_ui();

        if (read(fd, frow, rowlen) != (int)rowlen) { ok = 0; break; }

        mexp_row_unpack(CAM_SENSOR_BITS_PER_PIXEL, frow, av, rowpix);
        mexp_row_unpack(CAM_SENSOR_BITS_PER_PIXEL, live, vv, rowpix);

        // av is the accumulator and vv the exposure just taken, in that order,
        // because that is the order mexp_blend_px() documents and the average
        // mode is not symmetric in its arguments: n weights the first one.
        mexp_row_blend(mode, av, vv, rowpix, n, black, white);

        mexp_row_pack(CAM_SENSOR_BITS_PER_PIXEL, av, live, rowpix);

    }

    // No progress bar, deliberately, although this is the one operation in
    // CHDK slow enough to want one. raw_merge.c can draw one because it runs
    // from the file browser with the screen to itself; this runs inside the
    // raw hook, during the shot, which is exactly the window the persistent
    // OSD spends its time staying out of - posd_shot_hold() exists because
    // anything drawn there lands in the frame Canon is about to freeze for the
    // review. The busy LED from started() is the feedback that is safe here.

    close(fd);
    free(frow); free(av); free(vv);
    return ok;
}

// Give up on the sequence: the file, the counter and the ghost together, so
// there is no state left that a later shot could half-continue.
void raw_mexp_cancel(void)
{
    remove(MEXP_TMP_FILE);
    mexp_reset();
    mexp_ghost_clear();
}

// Returns non-zero only when this call successfully completed the composite.
int raw_mexp(void)
{
    int composite_done = 0;

    if (!conf.mexp_enable)
    {
        // Switched off part way through. Ending the sequence rather than
        // freezing it: the accumulator on the card describes a mode and a
        // frame count that the config no longer holds, and resuming it later
        // would combine exposures under rules that were never chosen.
        if (mexp_count) raw_mexp_cancel();
        return 0;
    }

    started();

    if (mexp_count == 0)
    {
        // First exposure of a sequence. The buffer is already the accumulator;
        // all that happens is that it is written down. The picture Canon
        // develops from it is untouched, which is correct - one exposure of a
        // multiple exposure looks exactly like an ordinary photograph.
        mexp_begin(conf.mexp_mode, conf.mexp_frames, conf.mexp_bend_each);
        if (mexp_store(rawadr))
            mexp_count = 1;
    }
    else if (mexp_combine(mexp_mode_used, (unsigned)mexp_count))
    {
        mexp_count++;

        // Not after the last one: nothing will read it, and it is 15MB of card
        // that the next sequence is going to overwrite anyway.
        if (mexp_count < mexp_target)
            mexp_store(rawadr);
    }
    else
    {
        // The accumulator could not be read. Rather than silently dropping
        // back to single frames for the rest of the sequence, end it - the
        // count on screen would otherwise be counting exposures that are not
        // in the picture.
        raw_mexp_cancel();
    }

    if (mexp_count >= mexp_target)
    {
        // Done. The buffer holds the composite and Canon is about to develop
        // it, so this is the frame the user gets. Everything else goes away,
        // and the next shot starts a new sequence.
        composite_done = 1;
        remove(MEXP_TMP_FILE);
        mexp_reset();
        mexp_ghost_clear();
    }
    else if (mexp_count > 0)
    {
        mexp_ghost_capture();
    }

    finished();
    return composite_done;
}

//-------------------------------------------------------------------
// Record what this frame was taken with, beside the frame - see
// include/bend_shot.h.
//
// Only when something was actually applied. A sidecar next to every ordinary
// photograph would double the file count on the card to say "no bend", which
// is what the absence of the file already says.

void raw_bend_shot(void)
{
    char dir[BEND_SHOT_PATHLEN];
    int bend_on, bendx_on;

    if (!conf.bend_shot_save) return;

    // Any region with a matrix in it, not just the camera's own - with the
    // layout off that is the same question it always was.
    bend_on  = conf.bitbend_enable && bend_seg_any();
#ifdef CAM_BEND_EXPERIMENTAL
    bendx_on = conf.bendx_enable && bendx_chain_count(&conf.bendx) > 0;
#else
    bendx_on = 0;
#endif
    if (!bend_on && !bendx_on) return;

    // The same two calls raw_createfile() uses to name a DNG, so the sidecar
    // lands in the directory this shot is going to and carries its number.
    dir[0] = 0;
    get_target_dir_name(dir);
    if (!dir[0]) return;

    // In the picture, or beside it. The comment inside the JPEG is the default
    // because it travels with the photograph - see include/bend_tag.h - and
    // the sidecar is still here for anyone who would rather not have their
    // JPEGs rewritten at all.
    //
    // The chain is passed even when it is off, and null on a build without
    // the second engine at all - the record layout does not change either way,
    // so one written here still loads on a camera that has one engine and not
    // the other.
    if (conf.bend_shot_save == BEND_SHOT_IN_JPEG)
    {
        bend_tag_queue(dir, get_target_file_num(), &conf.bitbend, bend_on,
#ifdef CAM_BEND_EXPERIMENTAL
                       &conf.bendx,
#else
                       0,
#endif
                       bendx_on, &conf.bend_segs);
        return;
    }

    // Per-shot recipes have one home: inside the JPEG. Numbered .BND files
    // remain presets explicitly saved by the user, never capture sidecars.
}

//-------------------------------------------------------------------
// Live view bending was here and has been removed.
//
// It applied the matrix to the 8 bit YUV viewport buffer, and it never worked:
// the imaging pipeline refills that buffer by DMA on its own schedule, across
// several channels whose target addresses are reprogrammed many times per
// frame, so anything written was overwritten in well under a frame time and
// the picture beat against the refill rate. See LIVEVIEW_NOTES.md for the
// addresses and for everything that was tried - a stable live bend needs the
// EVF DMA scheduler reversed, not another buffer address.
//
// It was kept for a while on the theory that a flickering bend is still a
// usable effect. It is not: it flickers hard enough to be unreadable, it cost
// a dedicated task at priority 0x19 and a second compiled table, and it made
// the capture bend harder to judge because the preview never matched it. The
// capture path is the one that is exact, and it is now the only one.

//-------------------------------------------------------------------
void patch_bad_pixel(unsigned int x,unsigned  int y) {
    int sum=0;
    int nzero=0;
    int i,j;
    int val;
    if ((x>=2) && (x<camera_sensor.raw_rowpix-2) && (y>=2) && (y<camera_sensor.raw_rows-2)) {
        if ((conf.bad_pixel_removal==1) || (conf.save_raw && conf.dng_raw)) {  // interpolation or DNG saving
            for (i=-2; i<=2; i+=2)
                for (j=-2; j<=2; j+=2)
                    if ((i!=0) && (j!=0)) {
                        val=get_raw_pixel(x+i, y+j);
                        if (val) {sum+=val; nzero++;}
                    }
            if (nzero) set_raw_pixel(x,y,sum/nzero);
        } else if (conf.bad_pixel_removal==2)  // or this makes RAW converter (internal/external)
            set_raw_pixel(x,y,0);
    }
}

struct point{
    int x;
    int y;
    struct point *next;
} *pixel_list=NULL;

void patch_bad_pixels(void) {
    struct point *pixel=pixel_list;
    while (pixel) {
        patch_bad_pixel((*pixel).x,(*pixel).y);
        pixel=(*pixel).next;
    }
}

int make_pixel_list(char * ptr, int size) {
    int x,y;
    struct point *pixel;
    char *endptr;

    if ( size <=0 ) return 0;

    while(*ptr) {
        while (*ptr==' ' || *ptr=='\t') ++ptr;    // whitespaces
        x=strtol(ptr, &endptr, 0);
        if (endptr != ptr) {
            ptr = endptr;
            if (*ptr++==',') {
                while (*ptr==' ' || *ptr=='\t') ++ptr;    // whitespaces
                if (*ptr!='\n' && *ptr!='\r') {
                    y=strtol(ptr, &endptr, 0);
                    if (endptr != ptr) {
                        ptr = endptr;
                        pixel=malloc(sizeof(struct point));
                        if (pixel) {
                            (*pixel).x=x;
                            (*pixel).y=y;
                            (*pixel).next=pixel_list;
                            pixel_list=pixel;
                        }
                    }
                }
            }
        }
        while (*ptr && *ptr!='\n') ++ptr;    // unless end of line
        if (*ptr) ++ptr;
    }
    return 0;
}
