// Visual browser for loading bend recipes embedded in JPEG comments.
// Kept as a module because the JPEG thumbnail decoder is far too large for
// the camera's permanently resident 200K core image.

#include "camera_info.h"
#include "keyboard.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_mbox.h"
#include "lang.h"
#include "gui_lang.h"
#include "module_def.h"
#include "module_load.h"
#include "simple_module.h"
#include "bend_tag.h"
#include "bend_store.h"
#include "bend_shot.h"
#include "bend_seg.h"
#include "bendx.h"
#include "dirent.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "limits.h"
#include "callfunc.h"

// Modules deliberately cannot include camera.h. The native path is therefore
// selected from camera_info at runtime; every other model uses the fallback.
#define CAM_BEND_NATIVE_VIEWPORT 1

#ifndef INT_MIN
#define INT_MIN (-2147483647-1)
#endif
#ifndef SHRT_MIN
#define SHRT_MIN (-32767-1)
#endif

// There is no cap on how many pictures this browser will page through.
//
// It used to hold every path in one sorted array, which is why there was one:
// a fixed array needs a bound, and past it the oldest pictures silently fell
// off the end. Nothing needed the whole list - the grid shows four, and every
// use of the old `count` reduced to "is there another page this way?".
//
// So the page is selected straight out of the filesystem instead. Names sort
// descending (Canon's folder and file numbering makes that chronological), and
// one directory walk keeps the four greatest strictly below an anchor, or the
// four smallest strictly above it going back. Constant memory, any number of
// pictures, and it self-heals if a picture is deleted mid-browse because the
// next walk simply does not find it.
//
// The walk costs one readdir sweep per page turn. load_page() already opens
// four JPEGs and pulls an EXIF thumbnail out of each on that same keypress, so
// this is not where the time goes.
#define BP_PATH      40     // "A/DCIM/" + 8 + "/" + 12 + NUL is 29; Canon is 8.3
#define BP_SCRATCH   64     // scratch for building a path before it is stored
#define BP_PAGE      4
#define BP_TW        144
#define BP_TH        68
#define BP_HEAD      22
#define BP_FOOT      22

static int running, dirty, sel, detail, action;
// leave() is a request, not the act. keys() runs on the KBD task and redraw()
// on the GUI task, so unwinding the mode from the key handler raced the painter:
// leave() cleared `detail` on its way out, and a redraw() landing in that window
// took the !detail branch and ran draw_grid(), putting the header and the four
// thumbnail frames back over a screen that had already been handed on. What was
// left looked like residue - an empty album stranded over the gallery, gone at
// the next keypress because that forces the mode underneath to repaint.
//
// Erasing at the end of leave() does not fix it and was tried: the erase runs on
// the KBD task, so an in-flight draw_grid() on the GUI task simply finishes
// afterwards and paints over it.
//
// So the teardown moves to the GUI task, which is where gui_bend.c already puts
// its own mode switches for the same reason (bm_request, "done here, in the GUI
// task, because switching mode erases and repaints"). Being the task that owns
// the drawing, the erase there is strictly ordered after any paint that had
// already started, and there is no window left to lose.
static volatile int leave_req;      // set by keys(), acted on by redraw()
static volatile int leave_close_bend;   // ...and close bend mode behind us
// The key that asked to close. Swallowed on the way out so the mode underneath
// does not act on the same press - the menu reads MENUITEM_PROC with an
// auto-repeating SET, so without this the item that launched this browser fired
// again the instant the mode went back and the album reappeared, live and still
// answering the arrows. See kbd_swallow_until_release() in core/kbd_process.c.
static volatile long leave_key;
// Which route opened this browser, because the two want opposite endings.
//
//   gallery (playback)  - the errand is "what was this picture taken with".
//                         Answer it and get out: close the album and hand the
//                         gallery back, the way the prompt found it.
//   menu (shooting)     - the errand is "load me a bend", and the album is the
//                         list you are choosing from. Loading one is not a
//                         reason to throw the list away; come back to it so
//                         another picture can be picked.
//
// Recorded at _run() rather than tested at the time, because by the time LOAD
// is pressed the camera may have been switched between the two.
static int from_play;
static int have;                    // names on screen now, 0..BP_PAGE
static int more_older, more_newer;  // is there a page in that direction
static gui_handler *old_mode;
// Tentative definition; the initialiser is beside redraw(). do_leave() needs
// to compare old_mode against it, and that sits above both.
static gui_handler mode;
static char paths[BP_PAGE][BP_PATH];   // the current page, newest first
static unsigned short thumbs[BP_PAGE][BP_TW * BP_TH];
static unsigned char thumb_ok[BP_PAGE];
static bend_t picked_bend;
static bendx_chain_t picked_x;
static bend_segs_t picked_seg;
static int picked_bend_on, picked_x_on, picked_ok;
static char notice[80];

#ifdef CAM_BEND_NATIVE_VIEWPORT
// The paused A480 display surface is 720x240 UYVYYY (four pixels in six
// bytes). Canon's live-view ring allocations are taller, but only this first
// display-height portion is scanned out while LiveImageTool is paused.
// LiveImageTool.Pause leaves the image plane enabled but stops it replacing
// our frame with the sensor stream. Only the horizontal axis is doubled
// relative to CHDK's 360x240 UI.
#define BP_VH 240
typedef struct {
    void **ring;
    void *pause;
    void *resume;
    int   vw;       // viewport pixels per scan-out row - see below
    int   nbuf;     // how many buffers in the ring the producer cycles
} native_cfg_t;

// vw was a hardcoded 720 for every body, which is what all of these are. It is
// a per-body field anyway, because a wrong value here is the difference
// between a picture in a box and a picture all over the display: with too
// large a stride assumed, the paste walks off the end of each row and the
// thumbnails come out enormous and smeared across the screen. Row stride is
// vw*3/2 bytes (UYVYYY - four pixels in six) and the horizontal scale against
// CHDK's own bitmap width is vw/camera_screen.width.

static const native_cfg_t a430_100b_native = {
    (void **)0x5264, (void *)0xffc94ca0, (void *)0xffc94cb0, 720, 3 };
static const native_cfg_t a460_100d_native = {
    (void **)0x6028, (void *)0xffd68954, (void *)0xffd68964, 720, 3 };
static const native_cfg_t a470_102c_native = {
    (void **)0x6adc, (void *)0xffd8fa2c, (void *)0xffd8fa3c, 720, 3 };
// Same body, other firmware. All three came out of the 101b dump by the same
// tools/newport.py detectors that reproduce the 102c row above exactly - the
// LiveImageTool eventproc registration pair out of this image's own table, and
// the ring tied to Canon's producer indexing. vw is the camera's, not the
// firmware's, so it is 720 here as well.
static const native_cfg_t a470_101b_native = {
    (void **)0x6ab4, (void *)0xffd8efec, (void *)0xffd8effc, 720, 3 };
static const native_cfg_t a480_100b_native = {
    (void **)0x3e80, (void *)0xffd84984, (void *)0xffd84994, 720, 3 };
static const native_cfg_t a540_100b_native = {
    (void **)0x5288, (void *)0xffc9ce70, (void *)0xffc9ce80, 720, 3 };
static const native_cfg_t a640_100b_native = {
    (void **)0x5270, (void *)0xffca2c24, (void *)0xffca2c34, 720, 3 };
static int native_paused, native_ready;

// Viewport width actually in use, seeded from the table and adjustable on the
// camera with the zoom lever while the grid is up.
//
// It is adjustable because deriving it from firmware is not always possible:
// where a port never overrode vid_get_viewport_byte_width(), the table's value
// is an assumption, and reading the right one off the body in one sitting
// beats another round of inference. The value is shown in the header whenever
// it differs from the table's.
static const native_cfg_t *native_cfg(void);   // defined below the table
static int native_vw;
static const int native_vw_opts[] = { 360, 480, 540, 704, 720, 960, 1440 };
#define NATIVE_VW_N ((int)(sizeof(native_vw_opts)/sizeof(native_vw_opts[0])))

static int native_vw_cur(void)
{
    const native_cfg_t *c = native_cfg();
    if (!native_vw) native_vw = c ? c->vw : 720;
    return native_vw;
}

// step +1/-1 through the candidate list, landing on the nearest entry first.
static void native_vw_step(int d)
{
    int i, at = 0, cur = native_vw_cur();
    for (i = 0; i < NATIVE_VW_N; i++) if (native_vw_opts[i] == cur) at = i;
    at += d;
    if (at < 0) at = NATIVE_VW_N - 1;
    if (at >= NATIVE_VW_N) at = 0;
    native_vw = native_vw_opts[at];
}

static const native_cfg_t *native_cfg(void)
{
    if(!strcmp(camera_info.platform,"a430") &&
       !strcmp(camera_info.platformsub,"100b")) return &a430_100b_native;
    if(!strcmp(camera_info.platform,"a460") &&
       !strcmp(camera_info.platformsub,"100d")) return &a460_100d_native;
    if(!strcmp(camera_info.platform,"a470") &&
       !strcmp(camera_info.platformsub,"102c")) return &a470_102c_native;
    // 101a builds from the 101b source (sub/101a/makefile.inc overrides
    // PLATFORMSUB) but keeps TARGET_FW at 101a, and makefile_cam.inc passes
    // -DPLATFORMSUB="$(TARGET_FW)" - so a 101a build reports platformsub "101a"
    // while these addresses were recorded under "101b". Same ROM as far as the
    // whole tree is concerned, so both names select the same config.
    if(!strcmp(camera_info.platform,"a470") &&
       (!strcmp(camera_info.platformsub,"101b") ||
        !strcmp(camera_info.platformsub,"101a"))) return &a470_101b_native;
    if(!strcmp(camera_info.platform,"a480") &&
       !strcmp(camera_info.platformsub,"100b")) return &a480_100b_native;
    if(!strcmp(camera_info.platform,"a540") &&
       !strcmp(camera_info.platformsub,"100b")) return &a540_100b_native;
    if(!strcmp(camera_info.platform,"a640") &&
       !strcmp(camera_info.platformsub,"100b")) return &a640_100b_native;
    return 0;
}

static int native_supported(void) { return native_cfg()!=0; }

static unsigned char clip8(int v)
{ return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

static unsigned char clips8(int v)
{ return (unsigned char)(signed char)(v < -128 ? -128 : (v > 127 ? 127 : v)); }

// Every buffer in the ring is present and non-NULL. The producer cycles all of
// them, so a buffer left untouched is one scan-out in nbuf showing something
// else - which is why nothing here writes a subset.
static int native_ring_ok(const native_cfg_t *cfg)
{
    int i;
    if(!cfg || !cfg->ring) return 0;
    for(i=0;i<cfg->nbuf;i++) if(!cfg->ring[i]) return 0;
    return 1;
}

static void native_clear(void)
{
    const native_cfg_t *cfg=native_cfg();
    void **fb;
    unsigned args[1]={0}; int i,j;
    native_ready=0;
    if(!native_ring_ok(cfg)) return;
    fb=cfg->ring;
    if(!native_paused) {
        call_func_ptr(cfg->pause,args,0);
        native_paused=1;
        // Pause is consumed by the next live-view boundary. Waiting here also
        // ensures none of the three buffers still belongs to the producer.
        msleep(50);
    }
    {
        int vsize=(native_vw_cur()*3/2)*BP_VH;
        for(j=0;j<cfg->nbuf;j++) {
            unsigned char *d=(unsigned char *)fb[j];
            for(i=0;i<vsize;i+=6) {
                d[i]=0; d[i+1]=0; d[i+2]=0;
                d[i+3]=0; d[i+4]=0; d[i+5]=0;
            }
        }
    }
}

// Paste one decoded RGB thumbnail into the native frame. Two CHDK pixels
// become one UYVYYY group (four physical pixels). Vertical coordinates map
// one-to-one. U and V are signed bytes on this generation of Canon hardware.
static void native_paste(int slot,const unsigned char *rgb,int w,int h)
{
    const native_cfg_t *cfg=native_cfg();
    void **fb=cfg ? cfg->ring : 0;
    int ox=12+(slot&1)*(camera_screen.width/2), oy=BP_HEAD+(slot/2)*(BP_TH+22);
    int stride,xs,gx,y,j,k;
    if(!native_paused || !native_ring_ok(cfg)) return;
    stride=native_vw_cur()*3/2;
    // Viewport pixels per CHDK bitmap pixel: 2 where the scan-out is 720 wide
    // against a 360 bitmap, 1 where the two match.
    xs=camera_screen.width ? native_vw_cur()/camera_screen.width : 2;
    if(xs<1) xs=1;
    // One 6-byte group is four viewport pixels sharing a chroma pair, so step
    // the destination in groups of four rather than stepping the source and
    // assuming what that lands on.
    for(y=0;y<BP_TH;y++)
    {
        int sy=y*h/BP_TH, py=oy+y;
        if(py<0 || py>=BP_VH) continue;
        for(gx=0;gx<BP_TW*xs;gx+=4)
        {
            int yy[4],rs=0,gs=0,bs=0,r,g,bl,u,v,px=xs*ox+gx;
            if(px<0 || (px/4)*6+6>stride) continue;
            for(k=0;k<4;k++)
            {
                int cx=(gx+k)/xs;
                const unsigned char *sp;
                if(cx>=BP_TW) cx=BP_TW-1;
                sp=rgb+(sy*w+(cx*w/BP_TW))*3;
                yy[k]=(77*sp[0]+150*sp[1]+29*sp[2])>>8;
                rs+=sp[0]; gs+=sp[1]; bs+=sp[2];
            }
            r=rs>>2; g=gs>>2; bl=bs>>2;
            u=(-43*r-85*g+128*bl)>>8;
            v=(128*r-107*g-21*bl)>>8;
            for(j=0;j<cfg->nbuf;j++) {
                unsigned char *d=(unsigned char *)fb[j]+py*stride+(px/4)*6;
                d[0]=clips8(u); d[1]=clip8(yy[0]); d[2]=clips8(v);
                d[3]=clip8(yy[1]); d[4]=clip8(yy[2]); d[5]=clip8(yy[3]);
            }
        }
    }
}

static void native_show(void)
{
    native_ready=native_paused && native_ring_ok(native_cfg());
}

static void native_stop(void)
{
    const native_cfg_t *cfg=native_cfg();
    unsigned args[1]={0};
    if(native_paused && cfg) call_func_ptr(cfg->resume,args,0);
    native_paused=native_ready=0;
}
#endif

// stb is only fed EXIF thumbnails, never the full multi-megapixel photograph.
// That bounds both its allocation and the time spent inside the GUI task.
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 512
#define STBI_ASSERT(x) ((void)0)
#define __SYMBIAN32__ 1
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

static int jpg_name(const char *s)
{
    int n = strlen(s);
    return n > 4 && s[n-4] == '.' &&
           (s[n-3] == 'J' || s[n-3] == 'j') &&
           (s[n-2] == 'P' || s[n-2] == 'p') &&
           (s[n-1] == 'G' || s[n-1] == 'g');
}

//-------------------------------------------------------------------
// The picture Canon is showing in playback, resolved to a path.
//
// core/bend_shot.c's playback_current_image_id() reads Canon's packed handle
// and hands back the directory and file numbers - the two halves of the
// "100-0042" ID it draws itself. That end has to be in the core image because
// it needs the four CAM_PB_IMAGE_* constants and modules cannot include
// camera.h. This end is the walk, and it is here because it is the expensive
// half: with the message box below it came to about a kilobyte, and the a480
// runs CHDK out of a 0x32000 ARAM region with a few hundred bytes spare. A .flt
// is read off the card, so here it costs the core nothing and the a480 gets the
// feature at all.
//
// The numbers are matched against the filesystem rather than formatted into a
// name: no assuming "%03dCANON" and "IMG_%04d.JPG". Prefixes and suffixes vary
// by body and by how the card was formatted - these ROMs carry a whole table of
// prefixes - and a wrong guess would silently load a different picture's bend.
// The digits are the part that is actually known.
static int pb_digits(const char *s, int n)
{
    int v = 0;
    while (n--)
    {
        if (*s < '0' || *s > '9') return -1;
        v = v * 10 + (*s++ - '0');
    }
    return v;
}

// Fills `out` (BP_PATH bytes) and returns 1, or returns 0 if the picture on
// screen cannot be named - which is not an error, just the cue to ask.
static int pb_resolve(char *out)
{
    DIR *root, *sub;
    struct dirent *de, *ie;
    char dirn[BP_SCRATCH];
    int dir = 0, file = 0, found = 0;

    if (!playback_current_image_id(&dir, &file)) return 0;

    root = opendir("A/DCIM");
    if (!root) return 0;
    while (!found && (de = readdir(root)) != 0)
    {
        if (de->d_name[0] == '.') continue;
        if (pb_digits(de->d_name, 3) != dir) continue;
        sprintf(dirn, "A/DCIM/%s", de->d_name);
        sub = opendir(dirn);
        if (!sub) continue;
        while ((ie = readdir(sub)) != 0)
        {
            // PREFIX + four digits + ".JPG". Only the digits and the extension
            // are checked; the four-character prefix is the camera's business.
            const char *nm = ie->d_name;
            int len = strlen(nm);
            if (len < 8 || !jpg_name(nm)) continue;
            if (pb_digits(nm + len - 8, 4) != file) continue;
            if ((int)(strlen(dirn) + 1 + len) >= BP_PATH) continue;
            sprintf(out, "%s/%s", dirn, nm);
            found = 1;
            break;
        }
        closedir(sub);
    }
    closedir(root);
    return found;
}

// Hold the BP_PAGE greatest (want_greatest) or smallest names seen so far.
// Descending for the greatest, ascending for the smallest, so in both cases
// the entry that falls off the end is the one at the back.
static void keep(char buf[][BP_PATH], int *n, const char *p, int want_greatest)
{
    int i, at;
    for (at = 0; at < *n; at++)
    {
        int c = strcmp(p, buf[at]);
        if (want_greatest ? (c > 0) : (c < 0)) break;
    }
    if (at >= BP_PAGE) return;
    if (*n < BP_PAGE) (*n)++;
    for (i = *n - 1; i > at; i--) strcpy(buf[i], buf[i-1]);
    strncpy(buf[at], p, BP_PATH-1); buf[at][BP_PATH-1] = 0;
}

// Fill paths[] with one page, newest first.
//
//   dir  0  the newest pictures on the card    (anchor ignored)
//        1  the page after `anchor`            (greatest names < anchor)
//       -1  the page before `anchor`           (smallest names > anchor)
//
// `matching` counts every name that passed the predicate, which is what says
// whether another page exists beyond this one - exactly, and for free, because
// more than BP_PAGE matches means at least one did not fit.
static void load_names(int dir, const char *anchor)
{
    DIR *root, *d;
    struct dirent *de, *ie;
    char dirn[BP_SCRATCH], path[BP_SCRATCH];
    char buf[BP_PAGE][BP_PATH];
    int matching = 0, n = 0, i;
    int want_greatest = (dir >= 0);

    have = 0; more_older = more_newer = 0;
    root = opendir("A/DCIM");
    if (!root) return;
    while ((de = readdir(root)) != 0)
    {
        if (de->d_name[0] == '.') continue;
        sprintf(dirn, "A/DCIM/%s", de->d_name);
        d = opendir(dirn);
        if (!d) continue;
        while ((ie = readdir(d)) != 0)
        {
            if (!jpg_name(ie->d_name)) continue;
            sprintf(path, "%s/%s", dirn, ie->d_name);
            // A name too long to store is one we could not reopen later
            // either, so it is not a picture this browser can offer.
            if ((int)strlen(path) >= BP_PATH) continue;
            if (dir > 0 && strcmp(path, anchor) >= 0) continue;
            if (dir < 0 && strcmp(path, anchor) <= 0) continue;
            matching++;
            keep(buf, &n, path, want_greatest);
        }
        closedir(d);
    }
    closedir(root);

    // Going back the buffer holds the smallest ascending; the screen wants
    // newest first either way.
    for (i = 0; i < n; i++)
        strcpy(paths[i], (dir < 0) ? buf[n-1-i] : buf[i]);
    have = n;

    if (dir > 0)       { more_older = matching > BP_PAGE; more_newer = 1; }
    else if (dir < 0)  { more_newer = matching > BP_PAGE; more_older = 1; }
    else               { more_older = matching > BP_PAGE; more_newer = 0; }
}

static unsigned rd16(const unsigned char *p, int le)
{ return le ? p[0] | ((unsigned)p[1]<<8) : ((unsigned)p[0]<<8) | p[1]; }
static unsigned rd32(const unsigned char *p, int le)
{ return le ? rd16(p,1) | (rd16(p+2,1)<<16) : (rd16(p,0)<<16) | rd16(p+2,0); }

// Locate the JPEG stored in EXIF IFD1. Canon writes this small preview near
// the start of every normal JPEG, so only the header is read from the card.
static int exif_thumb(const char *name, unsigned char **out, int *outn)
{
    unsigned char *b = 0, *j = 0;
    int fd, n, pos = 2, le, entries, i;
    unsigned tiff, ifd, next, off = 0, len = 0;
    b = malloc(128*1024);
    if (!b) return 0;
    fd = open(name, O_RDONLY, 0777);
    if (fd < 0) { free(b); return 0; }
    n = read(fd, b, 128*1024); close(fd);
    if (n < 16 || b[0] != 0xff || b[1] != 0xd8) goto bad;
    while (pos + 4 < n && b[pos] == 0xff)
    {
        int marker = b[pos+1], seg = ((int)b[pos+2]<<8) | b[pos+3];
        if (marker == 0xe1 && seg >= 16 && pos + 2 + seg <= n &&
            !memcmp(b+pos+4, "Exif\0\0", 6)) break;
        if (seg < 2) goto bad;
        pos += seg + 2;
    }
    if (pos + 16 >= n || b[pos+1] != 0xe1) goto bad;
    tiff = pos + 10;
    le = b[tiff] == 'I' && b[tiff+1] == 'I';
    if (!le && !(b[tiff] == 'M' && b[tiff+1] == 'M')) goto bad;
    ifd = tiff + rd32(b+tiff+4, le);
    if (ifd + 2 >= (unsigned)n) goto bad;
    entries = rd16(b+ifd, le);
    next = ifd + 2 + entries*12;
    if (next + 4 > (unsigned)n) goto bad;
    ifd = tiff + rd32(b+next, le);
    if (ifd + 2 >= (unsigned)n) goto bad;
    entries = rd16(b+ifd, le);
    for (i = 0; i < entries && ifd+2+(i+1)*12 <= (unsigned)n; i++)
    {
        unsigned char *e = b + ifd + 2 + i*12;
        unsigned tag = rd16(e, le);
        if (tag == 0x0201) off = rd32(e+8, le);
        if (tag == 0x0202) len = rd32(e+8, le);
    }
    if (!off || !len || len > 256*1024) goto bad;
    if (tiff + off + len <= (unsigned)n)
    {
        j = malloc(len);
        if (j) memcpy(j, b+tiff+off, len);
    }
    else
    {
        fd = open(name, O_RDONLY, 0777);
        j = malloc(len);
        if (fd >= 0 && j && lseek(fd, tiff+off, SEEK_SET) >= 0 && read(fd,j,len)==(int)len) {}
        else { if (j) free(j); j = 0; }
        if (fd >= 0) close(fd);
    }
    free(b);
    if (!j) return 0;
    *out = j; *outn = len; return 1;
bad:
    free(b); return 0;
}

static void load_thumb(int slot)
{
    unsigned char *jpg = 0, *rgb = 0;
    int jn, w, h, c, x, y;
    memset(thumbs[slot], 0, sizeof(thumbs[slot])); thumb_ok[slot] = 0;
    if (slot >= have || !exif_thumb(paths[slot], &jpg, &jn)) return;
    rgb = stbi_load_from_memory(jpg, jn, &w, &h, &c, 3); free(jpg);
    if (!rgb || w < 1 || h < 1) { if (rgb) stbi_image_free(rgb); return; }
#ifdef CAM_BEND_NATIVE_VIEWPORT
    native_paste(slot,rgb,w,h);
#endif
    for (y = 0; y < BP_TH; y++) for (x = 0; x < BP_TW; x++)
    {
        int sx = x*w/BP_TW, sy = y*h/BP_TH;
        unsigned char *p = rgb + (sy*w+sx)*3;
        static const unsigned char rgb[15][3] = {
            {0,0,0},{255,255,255},{220,35,35},{110,20,20},{255,110,90},
            {30,190,55},{15,90,35},{120,235,135},{35,75,210},{15,30,90},
            {70,210,225},{128,128,128},{55,55,55},{205,205,205},{235,210,35}
        };
        int k,best=0,bd=INT_MAX;
        for(k=0;k<15;k++) {
            int dr=(int)p[0]-rgb[k][0],dg=(int)p[1]-rgb[k][1],db=(int)p[2]-rgb[k][2];
            int d=dr*dr+dg*dg+db*db;if(d<bd){bd=d;best=k;}
        }
        switch(best) {
        case 0:thumbs[slot][y*BP_TW+x]=COLOR_BLACK;break;
        case 1:thumbs[slot][y*BP_TW+x]=COLOR_WHITE;break;
        case 2:thumbs[slot][y*BP_TW+x]=COLOR_RED;break;
        case 3:thumbs[slot][y*BP_TW+x]=COLOR_RED_DK;break;
        case 4:thumbs[slot][y*BP_TW+x]=COLOR_RED_LT;break;
        case 5:thumbs[slot][y*BP_TW+x]=COLOR_GREEN;break;
        case 6:thumbs[slot][y*BP_TW+x]=COLOR_GREEN_DK;break;
        case 7:thumbs[slot][y*BP_TW+x]=COLOR_GREEN_LT;break;
        case 8:thumbs[slot][y*BP_TW+x]=COLOR_BLUE;break;
        case 9:thumbs[slot][y*BP_TW+x]=COLOR_BLUE_DK;break;
        case 10:thumbs[slot][y*BP_TW+x]=COLOR_BLUE_LT;break;
        case 11:thumbs[slot][y*BP_TW+x]=COLOR_GREY;break;
        case 12:thumbs[slot][y*BP_TW+x]=COLOR_GREY_DK;break;
        case 13:thumbs[slot][y*BP_TW+x]=COLOR_GREY_LT;break;
        default:thumbs[slot][y*BP_TW+x]=COLOR_YELLOW;break;
        }
    }
    stbi_image_free(rgb); thumb_ok[slot] = 1;
}

static void load_page(void)
{
    int i;
#ifdef CAM_BEND_NATIVE_VIEWPORT
    if(native_supported()) native_clear();
#endif
    for(i=0;i<BP_PAGE;i++)load_thumb(i);
#ifdef CAM_BEND_NATIVE_VIEWPORT
    native_show();
#endif
    dirty=1;
}

static const char *base(const char *p)
{ const char *q=strrchr(p,'/'); return q?q+1:p; }

static void draw_thumb(int slot, int ox, int oy)
{
    int x,y;
    for(y=0;y<BP_TH;y++) for(x=0;x<BP_TW;x++)
        draw_pixel(ox+x,oy+y,(color)thumbs[slot][y*BP_TW+x]);
}

static void draw_grid(void)
{
    int i,x,y,idx;
#ifdef CAM_BEND_NATIVE_VIEWPORT
    if(native_ready)
        draw_rectangle(0,0,camera_screen.width-1,camera_screen.height-1,
                       MAKE_COLOR(COLOR_TRANSPARENT,COLOR_TRANSPARENT),RECT_BORDER0|DRAW_FILLED);
    else
#endif
    draw_rectangle(0,0,camera_screen.width-1,camera_screen.height-1,
                   MAKE_COLOR(COLOR_BLACK,COLOR_BLACK),RECT_BORDER0|DRAW_FILLED);
    draw_string(8,3,"LOAD BEND FROM IMAGE",MAKE_COLOR(COLOR_BLACK,COLOR_WHITE));
#ifdef CAM_BEND_NATIVE_VIEWPORT
    // Only while the native renderer is up, and only as long as it is being
    // tuned - it disappears again once the table carries the right value.
    if(native_ready)
    {
        const native_cfg_t *c=native_cfg();
        if(c && native_vw_cur()!=c->vw)
        {
            char vb[20];
            sprintf(vb,"vw %d",native_vw_cur());
            draw_string(camera_screen.width-8*FONT_WIDTH,3,vb,
                        MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
        }
    }
#endif
    if (more_newer) draw_string(camera_screen.width/2-4,3,"^",MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
    if (more_older) draw_string(camera_screen.width/2-4,camera_screen.height-18,"v",MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
    for(i=0;i<4;i++)
    {
        idx=i; x=12+(i&1)*(camera_screen.width/2); y=BP_HEAD+(i/2)*(BP_TH+22);
        if(idx>=have) continue;
        if(!thumb_ok[i])
            draw_string(x+34,y+28,"NO PREVIEW",MAKE_COLOR(COLOR_BLACK,COLOR_GREY));
#ifndef CAM_BEND_NATIVE_VIEWPORT
        else
            draw_thumb(i,x,y);
#else
        else if(!native_ready)
            draw_thumb(i,x,y);
#endif
        draw_rectangle(x-2,y-2,x+BP_TW+1,y+BP_TH+FONT_HEIGHT+1,
                       MAKE_COLOR(COLOR_BLACK,(sel==i)?COLOR_YELLOW:COLOR_GREY),RECT_BORDER1);
        draw_string(x,y+BP_TH+1,base(paths[idx]),MAKE_COLOR(COLOR_BLACK,COLOR_WHITE));
    }
    draw_string(8,camera_screen.height-FONT_HEIGHT-2,"MENU back   SET inspect",MAKE_COLOR(COLOR_BLACK,COLOR_GREY_LT));
}

static void read_picked(void)
{
    picked_ok = sel<have && bend_tag_read(paths[sel],&picked_bend,&picked_bend_on,
                                          &picked_x,&picked_x_on,&picked_seg);
}

static const bend_t *picked_matrix(int i)
{
    return (i == 0) ? &picked_bend : &picked_seg.b[i-1];
}

// The same pin-number/source header as bend mode, condensed into one shared
// header plus one row per recorded matrix. The leading cell takes the place of
// bend mode's SEG cell and identifies the matrix used by the legend below.
static int draw_matrices(int y, int n)
{
    int i, pin;
    int bits = camera_sensor.bits_per_pixel;
    int labelw = 28;
    int colw = (camera_screen.width-labelw)/bits;
    char s[8];

    draw_string(1,y,"BEND",MAKE_COLOR(COLOR_BLACK,COLOR_GREY));
    for (pin=bits-1;pin>=0;pin--)
    {
        int col=bits-1-pin;
        s[0]=(char)((pin<10)?'0'+pin:'a'+pin-10); s[1]=0;
        draw_string(labelw+col*colw+(colw-FONT_WIDTH)/2,y,s,
                    MAKE_COLOR(COLOR_BLACK,COLOR_GREY_LT));
    }
    y+=FONT_HEIGHT;
    for(i=0;i<n;i++)
    {
        const bend_t *m=picked_matrix(i);
        sprintf(s,"B%d",i+1);
        draw_string(3,y,s,MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
        for(pin=bits-1;pin>=0;pin--)
        {
            int col=bits-1-pin, len;
            bend_src_name(m->route[pin],s); len=strlen(s);
            draw_string(labelw+col*colw+(colw-len*FONT_WIDTH)/2,y,s,
                        MAKE_COLOR(COLOR_BLACK,
                          m->route[pin]==(unsigned char)BSRC_DATA(pin)?COLOR_GREY:COLOR_WHITE));
        }
        y+=FONT_HEIGHT;
    }
    return y;
}

static void draw_detail(void)
{
    char b[64], amt[20]; int y=2,n=0,mn=1,i;
    draw_rectangle(0,0,camera_screen.width-1,camera_screen.height-1,
                   MAKE_COLOR(COLOR_BLACK,COLOR_BLACK),RECT_BORDER0|DRAW_FILLED);
    draw_string(8,y,base(paths[sel]),MAKE_COLOR(COLOR_BLACK,COLOR_WHITE)); y+=FONT_HEIGHT+2;
    if(!picked_ok) draw_string(8,y,"No embedded bend recipe",MAKE_COLOR(COLOR_BLACK,COLOR_RED));
    else {
        n=bendx_chain_count(&picked_x);
        if(picked_seg.layout) mn=bend_seg_count(picked_seg.layout);
        if(!picked_bend_on) mn=0;
        if(mn) y=draw_matrices(y,mn);
        else { draw_string(8,y,"Bend matrix: off",MAKE_COLOR(COLOR_BLACK,COLOR_GREY)); y+=FONT_HEIGHT; }

        // Segment map and experimental chain share the remaining width. Even
        // at four entries each they use five rows, rather than ten stacked
        // rows, leaving the actions permanently visible at the foot.
        if(picked_seg.layout)
        {
            sprintf(b,"%s:",bend_seg_name(picked_seg.layout));
            draw_string(4,y,b,MAKE_COLOR(COLOR_BLACK,COLOR_CYAN));
            for(i=0;i<mn;i++)
            {
                sprintf(b,"%s: bend %d",bend_seg_label(picked_seg.layout,i),i+1);
                draw_string(4,y+(i+1)*FONT_HEIGHT,b,MAKE_COLOR(COLOR_BLACK,COLOR_WHITE));
            }
        }
        if(picked_x_on && n)
        {
            draw_string(184,y,"EXPERIMENTAL:",MAKE_COLOR(COLOR_BLACK,COLOR_MAGENTA));
            for(i=0;i<n;i++)
            {
                bendx_label(&picked_x.item[i],amt);
                if(amt[0]) sprintf(b,"%d %s %s",i+1,bendx_name(picked_x.item[i].kind),amt);
                else       sprintf(b,"%d %s",i+1,bendx_name(picked_x.item[i].kind));
                b[21]=0;
                draw_string(184,y+(i+1)*FONT_HEIGHT,b,MAKE_COLOR(COLOR_BLACK,COLOR_WHITE));
            }
        }
    }
    if(notice[0]) draw_string(8,camera_screen.height-58,notice,MAKE_COLOR(COLOR_BLACK,COLOR_GREEN));
    y=camera_screen.height-40;
    draw_string(42,y,"SAVE BEND",MAKE_COLOR(action==0?COLOR_YELLOW:COLOR_BLACK,action==0?COLOR_BLACK:COLOR_WHITE));
    draw_string(214,y,"LOAD BEND",MAKE_COLOR(action==1?COLOR_YELLOW:COLOR_BLACK,action==1?COLOR_BLACK:COLOR_WHITE));
    draw_string(8,camera_screen.height-18,"< > choose   SET confirm   MENU back",MAKE_COLOR(COLOR_BLACK,COLOR_GREY_LT));
}

static void leave(void)
{
    leave_req = 1;      // acted on by do_leave(), from redraw(), on the GUI task
}

// The actual teardown. GUI task only - see the note beside leave_req.
static void do_leave(void)
{

#ifdef CAM_BEND_NATIVE_VIEWPORT
    native_stop();
#endif

    // Take this browser's own pixels off the screen before handing the mode
    // back, the way bend mode's BM_REQ_LEAVE does with bm_erase_all().
    //
    // gui_set_need_restore() below is not enough on its own and never was. On
    // these bodies draw_restore() is vid_bitmap_refresh(), which asks *Canon* to
    // repaint its own OSD - it does not blank the bitmap CHDK drew into. Over
    // the gallery Canon repaints most of the screen but not the top strip, so
    // the header sat there and the thumbnail frames with it.
    //
    // The whole screen rather than the header alone because this is a
    // full-screen takeover: header, grid, notice line and footer are all ours
    // and none of them should outlive the mode.
    draw_rectangle(0,0,camera_screen.width-1,camera_screen.height-1,
                   MAKE_COLOR(COLOR_TRANSPARENT,COLOR_TRANSPARENT),
                   RECT_BORDER0|DRAW_FILLED);

    // Ask bend mode to close behind us, if the load path wanted that. Safe from
    // here: it only sets a flag, which gui_bend_activate() picks up on the next
    // pass, by which time this module may be unloaded either way.
    if (leave_close_bend)
    {
        leave_close_bend = 0;
        gui_bend_exit();
    }

    // Before handing the mode back, not after: the swallow has to be armed by
    // the time anything else is given a chance to read the keyboard.
    if (leave_key) { kbd_swallow_until_release(leave_key); leave_key = 0; }

    detail = 0;
    running = 0;        // gates _module_can_unload(), so it goes last

    // Whatever happens, do not hand the mode back to ourselves - that is the
    // trap described in _run(), and a browser that cannot be closed is worse
    // than one that closes to the wrong place. <ALT> is the fallback because
    // this is launched from the CHDK menu inside it, and because it is somewhere
    // the user can always get out of.
    {
        extern gui_handler defaultGuiHandler;
        gui_handler *back = old_mode;
        // Never hand the mode back to ourselves - see _run(). The fallback
        // follows the route: over the gallery the right place is the plain
        // playback handler, and from the menu it is <ALT>, which is where the
        // menu lives and somewhere the user can always get out of.
        if (!back || back == &mode)
            back = from_play ? &defaultGuiHandler : &altGuiHandler;
        old_mode = 0;
        gui_set_mode(back);
    }
    gui_set_need_restore();
    leave_req = 0;
}

static int keys(void)
{
    int k=kbd_get_autoclicked_key();
    if(!k) return 1;
    notice[0]=0;
    leave_key=k;    // whichever key this turns out to be, if it closes us
    if(detail)
    {
        if(k==KEY_MENU) { detail=0; dirty=1; }
        else if(k==KEY_LEFT || k==KEY_RIGHT) { action=!action; dirty=1; }
        else if(k==KEY_SET && !picked_ok)
        {
            // Silence here was indistinguishable from a successful load, and in
            // the menu route - where staying in the album is now correct - it
            // looked exactly like success. Say so instead.
            sprintf(notice,"No bend recorded in this picture");
            detail=0; dirty=1;
            return 1;
        }
        else if(k==KEY_SET && picked_ok)
        {
            if(action==0)
            {
                bend_store_recipe_t r;
                int s;
                memset(&r,0,sizeof(r));
                r.bend=picked_bend; r.bendx=picked_x; r.segs=picked_seg;
                r.bend_on=picked_bend_on; r.bendx_on=picked_x_on;
                s=bend_store_save_recipe(&r,0);
                sprintf(notice,s>=0?"Saved as BEND%02d.BND":"Could not save bend",s>=0?bend_store_slot_at(s):0);
            }
            else
            {
                // One shared function with core/gui.c's playback path - see
                // bend_shot_apply() in core/bend_shot.c. This used to do the
                // assignment itself and was missing the menu-shadow resync,
                // so bend_ui_push() put nine fields of the loaded bend back to
                // whatever the menu last held, on the next redraw.
                bend_shot_apply(&picked_bend, picked_bend_on,
                                &picked_x, picked_x_on, &picked_seg);

                if(!from_play)
                {
                    // Opened from the CHDK menu in shooting mode. The album is
                    // the list being chosen from, so loading one picture's bend
                    // comes back to it rather than tearing everything down -
                    // picking another is the obvious next thing to want, and
                    // the notice says the load happened.
                    //
                    // This is the opposite of what the code did before, which
                    // exited the menu entirely and left the gallery route with
                    // nothing to do at all. Saving already behaved this way.
                    { const char *nm = paths[sel];
                      const char *b = nm + strlen(nm);
                      while(b > nm && b[-1] != '/') b--;
                      sprintf(notice, "Loaded the bend from %s", b); }
                    detail = 0;
                    dirty  = 1;
                    return 1;
                }

                // Opened over the gallery. The question was about the picture
                // on screen, it has been answered, and the bend asked for is
                // now the live one - so close the album and give the gallery
                // back. Confirmation is not lost: the bend's name is on the
                // shooting screen the moment the overlay comes back.
                //
                // leave() unwinds this module back to whatever opened it.
                // gui_bend_exit() closes bend mode's browser panel behind it if
                // one was open; it only sets a flag, so do_leave() runs it once
                // this module has finished with the screen.
                leave_close_bend = 1;
                leave();
                return 1;
            }
            dirty=1;
        }
        return 1;
    }
    if(k==KEY_MENU) leave();
#ifdef CAM_BEND_NATIVE_VIEWPORT
    // Zoom adjusts the viewport width the native renderer writes against.
    // Only where that renderer is actually running - elsewhere the lever has
    // nothing to change and the keys stay free.
    else if((k==KEY_ZOOM_IN || k==KEY_ZOOM_OUT) && native_ready)
    { native_vw_step(k==KEY_ZOOM_IN?1:-1); load_page(); }
#endif
    else if(k==KEY_LEFT) { sel^=1; if(sel>=have) sel^=1; dirty=1; }
    else if(k==KEY_RIGHT) { sel^=1; if(sel>=have) sel^=1; dirty=1; }
    else if(k==KEY_UP)
    {
        if(sel>=2) { sel-=2; dirty=1; }
        else if(more_newer) { load_names(-1,paths[0]); sel&=1; if(sel>=have) sel=have?have-1:0; load_page(); }
    }
    else if(k==KEY_DOWN)
    {
        if(sel<2 && sel+2<have) { sel+=2; dirty=1; }
        else if(more_older) { load_names(1,paths[have-1]); sel&=1; if(sel>=have) sel=have?have-1:0; load_page(); }
    }
    else if(k==KEY_SET && sel<have) { read_picked(); detail=1; action=1; dirty=1; }
    return 1;
}

static void redraw(int force)
{
    // Before any painting, and instead of it: this is the GUI task, so the
    // teardown here is strictly ordered after whatever was last drawn.
    if(leave_req) { do_leave(); return; }
    if(detail) {
        if(dirty || force) draw_detail();
    } else {
        if(dirty || force) draw_grid();
    }
    dirty=0;
}

static gui_handler mode={GUI_MODE_MODULE,redraw,keys,0,0,GUI_MODE_FLAG_NORESTORE_ON_SWITCH};

// Load one picture's bend and say so, without ever showing the browser.
//
// This is the gallery gesture: hold the mode button over a picture, answer Yes,
// and the bend on screen becomes the live one. Used to live in core/gui.c as
// bend_shot_selected(); it is here now because the walk it needs is here, and
// because between them they did not fit in the a480's ARAM budget.
//
// Returns 1 if it handled the request - including "that frame had no bend",
// which is an answer and not a failure. 0 means the picture could not be named
// and the caller should fall back to asking.
static int load_from_playback(void)
{
    // Long enough for the longest message below with a path-less file name in
    // it - the layout line is the worst case.
    static char msg[128];
    char path[BP_PATH];
    bend_t        b;
    bendx_chain_t c;
    bend_segs_t   sg;
    int bend_on = 0, bendx_on = 0, n, nseg;
    const char *name, *lay;

    if (!pb_resolve(path)) return 0;

    // Report the name rather than the path - the path is most of a line on this
    // screen and the part that identifies the frame is the end of it.
    for (name = path + strlen(path); name > path && name[-1] != '/'; name--) ;

    if (!bend_tag_read(path, &b, &bend_on, &c, &bendx_on, &sg))
    {
        sprintf(msg, "No bend recorded for %s - it was not taken with one, or "
                     "the record has been stripped out of it", name);
        gui_mbox_init(LANG_INFORMATION, (int)msg,
                      MBOX_BTN_OK|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, NULL);
        return 1;
    }

    // The one correct order - see bend_shot_apply() in core/bend_shot.c. Doing
    // the assignment by hand here is what used to lose nine fields to
    // bend_ui_push() on the next redraw.
    bend_shot_apply(&b, bend_on, &c, bendx_on, &sg);

    n = bendx_chain_count(&conf.bendx);
    lay = (conf.bend_segs.layout != BSEG_OFF)
        ? bend_seg_name(conf.bend_segs.layout) : 0;
    nseg = lay ? bend_seg_count(conf.bend_segs.layout) : 0;

    // Named rather than counted, because "4 segments" and "Quarters" are the
    // same fact and only one of them tells you where to look.
    if (bend_on && lay)    sprintf(msg, "Loaded the bend from %s - %s, %d segments", name, lay, nseg);
    else if (bend_on && n) sprintf(msg, "Loaded the bend and %d profile%s from %s", n, (n == 1) ? "" : "s", name);
    else if (bend_on)      sprintf(msg, "Loaded the bend from %s", name);
    else if (n)            sprintf(msg, "Loaded %d profile%s from %s", n, (n == 1) ? "" : "s", name);
    else                   sprintf(msg, "%s was taken with nothing applied", name);

    // Quiet on success: the user asked for the bend, not for a report. Only the
    // two "nothing happened" cases speak up, because those must not be
    // indistinguishable from a load that worked.
    if (!bend_on && !n)
        gui_mbox_init(LANG_INFORMATION, (int)msg,
                      MBOX_BTN_OK|MBOX_TEXT_CENTER|MBOX_FUNC_RESTORE, NULL);
    return 1;
}

int _run(void)
{
    gui_handler *prev;

    // 2 means the gallery asked about the picture on screen - handle it without
    // ever becoming the GUI mode, so nothing is drawn and nothing has to be
    // torn down. Deliberately before gui_set_mode() below: entering the browser
    // and immediately leaving it is what the whole teardown saga was about.
    //
    // A body whose playback handle has not been reversed falls straight through
    // to the browser, which is what every camera did before any of it was.
    { extern int bend_pic_exit_on_load;
      if (bend_pic_exit_on_load == 2 && load_from_playback()) return 0; }

    // Re-entry is real and it used to be fatal.
    //
    // module_run() calls run() again on a module that is already loaded, and the
    // menu reads MENUITEM_PROC with an auto-repeating SET - so "Load bend from
    // image" can fire a second time while this browser is already the GUI mode.
    // The old code then did old_mode = gui_set_mode(&mode), and gui_set_mode()
    // returns the *current* handler, which by then was this module's own. From
    // that moment old_mode pointed at us: leave() handed the mode back to the
    // browser, every time, so the album could not be closed at all. It stayed on
    // screen still answering the arrow keys, which is what made it look like
    // leftover drawing rather than a mode that would not exit.
    //
    // So never capture ourselves, and treat a second entry as what it actually
    // is - a request to start the listing again, not to re-enter.
    prev = gui_set_mode(&mode);
    if (prev != &mode)
        old_mode = prev;

    // Told, not guessed. bend_pic_open() records which errand this is at the
    // moment the browser is asked for; deducing it here from camera state got it
    // wrong twice, and a wrong answer leaves the album on screen after a load.
    { extern int bend_pic_exit_on_load; from_play = (bend_pic_exit_on_load != 0); }
    running=1; leave_req=0; leave_close_bend=0; leave_key=0;
    sel=detail=0; notice[0]=0;
    load_names(0,0); load_page();
    dirty=1;
    return 0;
}
int _module_can_unload(void){return !running;}
// Called by the loader on its way out of <ALT>, not from the key handler, and
// there is no redraw() after it to service a request - so tear down here.
int _module_exit_alt(void){if(running) do_leave();return 0;}
libsimple_sym _libbendpic={{0,0,_module_can_unload,_module_exit_alt,_run}};
ModuleInfo _module_info={MODULEINFO_V1_MAGICNUM,sizeof(ModuleInfo),{1,0},
    ANY_CHDK_BRANCH,0,OPT_ARCHITECTURE,ANY_PLATFORM_ALLOWED,
    (int32_t)"Bend from image",MTYPE_TOOL,&_libbendpic.base,
    CONF_VERSION,CAM_SCREEN_VERSION,CAM_SENSOR_VERSION,CAM_INFO_VERSION,0};
