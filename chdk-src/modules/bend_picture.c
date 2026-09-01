// Visual browser for loading bend recipes embedded in JPEG comments.
// Kept as a module because the JPEG thumbnail decoder is far too large for
// the camera's permanently resident 200K core image.

#include "camera_info.h"
#include "keyboard.h"
#include "conf.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_mbox.h"
#include "module_def.h"
#include "module_load.h"
#include "simple_module.h"
#include "bend_tag.h"
#include "bend_store.h"
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

#define BP_MAX       64
#define BP_PATH      64
#define BP_PAGE      4
#define BP_TW        144
#define BP_TH        68
#define BP_HEAD      22
#define BP_FOOT      22

static int running, dirty, count, page, sel, detail, action;
static gui_handler *old_mode;
static char paths[BP_MAX][BP_PATH];
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
#define BP_VW 720
#define BP_VH 240
#define BP_VSIZE (BP_VW*BP_VH*3/2)
typedef struct {
    void **ring;
    void *pause;
    void *resume;
} native_cfg_t;

static const native_cfg_t a410_100f_native = {
    (void **)0x4ff4, (void *)0xffc91c6c, (void *)0xffc91c7c };
static const native_cfg_t a430_100b_native = {
    (void **)0x5264, (void *)0xffc94ca0, (void *)0xffc94cb0 };
static const native_cfg_t a460_100d_native = {
    (void **)0x6028, (void *)0xffd68954, (void *)0xffd68964 };
static const native_cfg_t a470_102c_native = {
    (void **)0x6adc, (void *)0xffd8fa2c, (void *)0xffd8fa3c };
static const native_cfg_t a480_100b_native = {
    (void **)0x3e80, (void *)0xffd84984, (void *)0xffd84994 };
static const native_cfg_t a540_100b_native = {
    (void **)0x5288, (void *)0xffc9ce70, (void *)0xffc9ce80 };
static const native_cfg_t a640_100b_native = {
    (void **)0x5270, (void *)0xffca2c24, (void *)0xffca2c34 };
static int native_paused, native_ready;

static const native_cfg_t *native_cfg(void)
{
    // The A410 answers to two sub names for one firmware. platform/a410/sub/100e
    // builds from the 100f source ("identical firmware") but keeps TARGET_FW at
    // 100e, and makefile_cam.inc passes -DPLATFORMSUB="$(TARGET_FW)" - so the
    // shipped 100e build reports platformsub "100e" while the addresses below
    // were recorded under "100f". Matching only "100f" meant the A410 - the one
    // body this browser was first run on - silently took the fallback renderer.
    // Both names select the same config because they are the same ROM.
    if(!strcmp(camera_info.platform,"a410") &&
       (!strcmp(camera_info.platformsub,"100f") ||
        !strcmp(camera_info.platformsub,"100e"))) return &a410_100f_native;
    if(!strcmp(camera_info.platform,"a430") &&
       !strcmp(camera_info.platformsub,"100b")) return &a430_100b_native;
    if(!strcmp(camera_info.platform,"a460") &&
       !strcmp(camera_info.platformsub,"100d")) return &a460_100d_native;
    if(!strcmp(camera_info.platform,"a470") &&
       !strcmp(camera_info.platformsub,"102c")) return &a470_102c_native;
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

static void native_clear(void)
{
    const native_cfg_t *cfg=native_cfg();
    void **fb;
    unsigned args[1]={0}; int i,j;
    native_ready=0;
    if(!cfg) return;
    fb=cfg->ring;
    for(i=0;i<3;i++) if(!fb[i]) return;
    if(!native_paused) {
        call_func_ptr(cfg->pause,args,0);
        native_paused=1;
        // Pause is consumed by the next live-view boundary. Waiting here also
        // ensures none of the three buffers still belongs to the producer.
        msleep(50);
    }
    for(j=0;j<3;j++) {
        unsigned char *d=(unsigned char *)fb[j];
        for(i=0;i<BP_VSIZE;i+=6) {
            d[i]=0; d[i+1]=0; d[i+2]=0;
            d[i+3]=0; d[i+4]=0; d[i+5]=0;
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
    int x,y,j;
    if(!native_paused || !fb[0] || !fb[1] || !fb[2]) return;
    for(y=0;y<BP_TH;y++) for(x=0;x<BP_TW;x+=2)
    {
        const unsigned char *a=rgb+((y*h/BP_TH)*w+(x*w/BP_TW))*3;
        const unsigned char *b=rgb+((y*h/BP_TH)*w+((x+1)*w/BP_TW))*3;
        int ya=(77*a[0]+150*a[1]+29*a[2])>>8;
        int yb=(77*b[0]+150*b[1]+29*b[2])>>8;
        int r=(a[0]+b[0])>>1,g=(a[1]+b[1])>>1,bl=(a[2]+b[2])>>1;
        int u=(-43*r-85*g+128*bl)>>8;
        int v=(128*r-107*g-21*bl)>>8;
        int py=oy+y, px=2*(ox+x);
        for(j=0;j<3;j++) {
            unsigned char *d=(unsigned char *)fb[j]+py*(BP_VW*3/2)+(px/4)*6;
            d[0]=clips8(u); d[1]=clip8(ya); d[2]=clips8(v);
            d[3]=clip8(ya); d[4]=clip8(yb); d[5]=clip8(yb);
        }
    }
}

static void native_show(void)
{
    const native_cfg_t *cfg=native_cfg();
    void **fb=cfg ? cfg->ring : 0;
    native_ready=native_paused && fb[0] && fb[1] && fb[2];
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

// Keep the newest names in descending lexical order. Canon's folder and file
// numbering makes that chronological without relying on timestamps whose
// layout differs between old DryOS and VxWorks ports.
static void add_path(const char *p)
{
    int i, at;
    for (at = 0; at < count && strcmp(paths[at], p) > 0; at++);
    if (at >= BP_MAX) return;
    if (count < BP_MAX) count++;
    for (i = count - 1; i > at; i--) strcpy(paths[i], paths[i-1]);
    strncpy(paths[at], p, BP_PATH-1); paths[at][BP_PATH-1] = 0;
}

static void scan(void)
{
    DIR *root, *d;
    struct dirent *de, *ie;
    char dir[BP_PATH], path[BP_PATH];
    count = 0;
    root = opendir("A/DCIM");
    if (!root) return;
    while ((de = readdir(root)) != 0)
    {
        if (de->d_name[0] == '.') continue;
        sprintf(dir, "A/DCIM/%s", de->d_name);
        d = opendir(dir);
        if (!d) continue;
        while ((ie = readdir(d)) != 0)
        {
            if (!jpg_name(ie->d_name)) continue;
            sprintf(path, "%s/%s", dir, ie->d_name);
            add_path(path);
        }
        closedir(d);
    }
    closedir(root);
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

static void load_thumb(int slot, int idx)
{
    unsigned char *jpg = 0, *rgb = 0;
    int jn, w, h, c, x, y;
    memset(thumbs[slot], 0, sizeof(thumbs[slot])); thumb_ok[slot] = 0;
    if (idx >= count || !exif_thumb(paths[idx], &jpg, &jn)) return;
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
    for(i=0;i<BP_PAGE;i++)load_thumb(i,page+i);
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
    if (page>0) draw_string(camera_screen.width/2-4,3,"^",MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
    if (page+4<count) draw_string(camera_screen.width/2-4,camera_screen.height-18,"v",MAKE_COLOR(COLOR_BLACK,COLOR_YELLOW));
    for(i=0;i<4;i++)
    {
        idx=page+i; x=12+(i&1)*(camera_screen.width/2); y=BP_HEAD+(i/2)*(BP_TH+22);
        if(idx>=count) continue;
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
    int idx=page+sel;
    picked_ok = idx<count && bend_tag_read(paths[idx],&picked_bend,&picked_bend_on,
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
    draw_string(8,y,base(paths[page+sel]),MAKE_COLOR(COLOR_BLACK,COLOR_WHITE)); y+=FONT_HEIGHT+2;
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
#ifdef CAM_BEND_NATIVE_VIEWPORT
    native_stop();
#endif
    running=0; detail=0; gui_set_mode(old_mode); gui_set_need_restore();
}

static int keys(void)
{
    int k=kbd_get_autoclicked_key();
    if(!k) return 1;
    notice[0]=0;
    if(detail)
    {
        if(k==KEY_MENU) { detail=0; dirty=1; }
        else if(k==KEY_LEFT || k==KEY_RIGHT) { action=!action; dirty=1; }
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
                conf.bitbend=picked_bend; conf.bitbend_enable=picked_bend_on;
                conf.bend_segs=picked_seg; conf.bendx=picked_x; conf.bendx_enable=picked_x_on;
                // The JPEG contains every matrix. Preset slots are only names
                // from the source card and may have since been deleted or mean
                // something else on this card, so never restore them.
                { int i; for(i=0;i<BEND_SEG_MAX;i++) conf.bend_segs.slot[i]=0; }
                bend_seg_prep(camera_sensor.bits_per_pixel); bendx_chain_sanitize(&conf.bendx);
                bend_store_detach(); strcpy(notice,"Bend loaded");
            }
            dirty=1;
        }
        return 1;
    }
    if(k==KEY_MENU) leave();
    else if(k==KEY_LEFT) { sel^=1; if(page+sel>=count) sel^=1; dirty=1; }
    else if(k==KEY_RIGHT) { sel^=1; if(page+sel>=count) sel^=1; dirty=1; }
    else if(k==KEY_UP)
    {
        if(sel>=2) { sel-=2; dirty=1; }
        else if(page>0) { page-=4; load_page(); }
    }
    else if(k==KEY_DOWN)
    {
        if(sel<2 && page+sel+2<count) { sel+=2; dirty=1; }
        else if(page+4<count) { page+=4; sel&=1; load_page(); }
    }
    else if(k==KEY_SET && page+sel<count) { read_picked(); detail=1; action=1; dirty=1; }
    return 1;
}

static void redraw(int force)
{
    if(detail) {
        if(dirty || force) draw_detail();
    } else {
        if(dirty || force) draw_grid();
    }
    dirty=0;
}

static gui_handler mode={GUI_MODE_MODULE,redraw,keys,0,0,GUI_MODE_FLAG_NORESTORE_ON_SWITCH};

int _run(void)
{
    running=1; page=sel=detail=0; notice[0]=0; scan(); load_page();
    old_mode=gui_set_mode(&mode); dirty=1; return 0;
}
int _module_can_unload(void){return !running;}
int _module_exit_alt(void){leave();return 0;}
libsimple_sym _libbendpic={{0,0,_module_can_unload,_module_exit_alt,_run}};
ModuleInfo _module_info={MODULEINFO_V1_MAGICNUM,sizeof(ModuleInfo),{1,0},
    ANY_CHDK_BRANCH,0,OPT_ARCHITECTURE,ANY_PLATFORM_ALLOWED,
    (int32_t)"Bend from image",MTYPE_TOOL,&_libbendpic.base,
    CONF_VERSION,CAM_SCREEN_VERSION,CAM_SENSOR_VERSION,CAM_INFO_VERSION,0};
