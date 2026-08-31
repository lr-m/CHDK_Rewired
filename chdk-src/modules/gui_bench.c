#include "camera_info.h"
#include "keyboard.h"
#include "viewport.h"
#include "clock.h"
#include "raw_buffer.h"
#include "lang.h"
#include "gui.h"
#include "gui_lang.h"
#include "gui_draw.h"
#include "cachebit.h"

#include "module_def.h"

// draw_txt_string no longer exported, replace it with a macro
#define draw_txt_string(x,y,s,c) draw_string((x)*FONT_WIDTH,(y)*FONT_HEIGHT,s,c)
#define draw_txt_stringnonalign(x,y,s,c) draw_string((x)*FONT_WIDTH+1,(y)*FONT_HEIGHT,s,c)

void gui_bench_draw();
void gui_module_menu_kbd_process();
int gui_bench_kbd_process();

gui_handler GUI_MODE_BENCH = 
    /*GUI_MODE_BENCH*/  { GUI_MODE_MODULE, gui_bench_draw, gui_bench_kbd_process, gui_module_menu_kbd_process, 0, 0 };

//-------------------------------------------------------------------
static struct {
    int screen_input_bps;
    unsigned int screen_input_size;
    int screen_output_bps;
    int memory_read_bps;
    int memory_read_uc_bps;
    int memory_write_bps;
    int memory_write_uc_bps;
    int disk_read_buf_kbps;
    int disk_write_buf_kbps;
    int disk_write_raw_kbps;
    int disk_write_mem_kbps;
    int cpu_ips;
    int text_cps;
    int textnonalign_cps;
} bench;

static char buf[48];

#define LOGBUFSIZE 1024
#define BENCHLOGFILE "A/BENCH.LOG"

static char txtbuf[LOGBUFSIZE];
static int txtbufptr = 0;
static int *benchbuf = NULL;
static char clearline[48];
static int bench_to_run = 0;
static int bench_to_draw = 0;
static int bench_mode = 0;
static int bench_log = 0;

#define BENCH_ALL 1
#define BENCH_NOCARD 2

#define BENCHTMP "A/BENCH.TMP"

#define BENCH_CTL_START 0
#define BENCH_CTL_LOG 1
#define BENCH_CTL_SD 2
#define BENCH_CTL_MAX BENCH_CTL_SD
static int bench_cur_ctl = BENCH_CTL_START;
static int bench_mode_next = BENCH_ALL;

//-------------------------------------------------------------------
void gui_bench_init() {
    bench.screen_input_bps=-1;
    bench.screen_input_size=0;
    bench.screen_output_bps=-1;
    bench.memory_read_bps=-1;
    bench.memory_read_uc_bps=-1;
    bench.memory_write_bps=-1;
    bench.memory_write_uc_bps=-1;
    bench.disk_read_buf_kbps=-1;
    bench.disk_write_buf_kbps=-1;
    bench.disk_write_raw_kbps=-1;
    bench.disk_write_mem_kbps=-1;
    bench.cpu_ips=-1;
    bench.text_cps=-1;
    bench.textnonalign_cps=-1;
    bench_to_run = 0;
    bench_mode = 0;
    bench_to_draw = 1;
}

//-------------------------------------------------------------------
static void gui_bench_draw_results_screen(int pos, int value, int ss) {
    if (value!=-1) {
        if (value)
            sprintf(buf, "%7d Kb/s   %3d FPS", value/1024, (ss==0)?0:value/ss);
        else
            strcpy(buf, clearline);
        draw_txt_string(17, pos, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    }
}

//-------------------------------------------------------------------
static void gui_bench_draw_results_memory(int pos, int value, int value_uc) {
    if (value!=-1) {
        if (value)
            sprintf(buf, "%7d C,  %6d UC Kb/s ", value/1024, value_uc/1024);
        else
            strcpy(buf, clearline);
        draw_txt_string(17, pos, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    }
}

//-------------------------------------------------------------------
static void gui_bench_draw_results(int pos, int value) {
    if (value!=-1) {
        if (value)
            sprintf(buf, "%7d Kb/s      ", value);
        else
            strcpy(buf, clearline);
        draw_txt_string(17, pos, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    }
}

//-------------------------------------------------------------------

static void gui_bench_draw_results_cpu(int pos, int value) {
    if (value!=-1) {
        if (value)
            sprintf(buf, "%7d MIPS    ", value);
        else
            strcpy(buf, clearline);
        draw_txt_string(17, pos, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    }
}

//-------------------------------------------------------------------

static void gui_bench_draw_results_text(int pos, int value, int valuen, int ss) {
    if (value!=-1) {
        if (value) {
                int s=(ss==0)?0:FONT_WIDTH*FONT_HEIGHT*value/ss;
                int sn=(ss==0)?0:FONT_WIDTH*FONT_HEIGHT*valuen/ss;
                sprintf(buf, " %d, %d c/s %d, %d FPS", value, valuen, s, sn);
            }
        else
            strcpy(buf, clearline);
        draw_txt_string(17, pos, buf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    }
}

//-------------------------------------------------------------------

static void write_log(int really) {
    if (!really)
        return;
    int fd = open(BENCHLOGFILE, O_WRONLY|O_CREAT|O_APPEND, 0777);
    if (fd >= 0)
    {
        lseek(fd, 0, SEEK_END);
        write(fd, txtbuf, txtbufptr);
        close(fd);
    }
}

static void add_to_log(int really, char *title, char *txt) {
    if ((txtbufptr >= LOGBUFSIZE-48) || !really)
        return;
    txtbufptr+=sprintf(txtbuf+txtbufptr,"%s %s\n",title, txt);
}

static void add_log_head(int really) {
    if ((txtbufptr >= LOGBUFSIZE-200) || !really)
        return;
    txtbufptr+=sprintf(txtbuf+txtbufptr,"***\n%s %s r%s\n",camera_info.chdk_ver,camera_info.build_number,camera_info.build_svnrev);
    txtbufptr+=sprintf(txtbuf+txtbufptr,"Build date: %s %s\n",camera_info.build_date,camera_info.build_time);
    txtbufptr+=sprintf(txtbuf+txtbufptr,"Camera    : %s %s\n",camera_info.platform,camera_info.platformsub);
    txtbufptr+=sprintf(txtbuf+txtbufptr,"Mode      : 0x%x\n\n",camera_info.state.mode);
}

static int bench_draw_control_txt(int pos, const char *s, int active) {
    twoColors cl;
    if(active) {
        cl = user_color(conf.menu_cursor_color);
    } else {
        cl = user_color(conf.menu_color);
    }
    draw_txt_string(pos, 0,  s, cl);
    return pos+strlen(s);
}
static int bench_draw_checkbox(int pos, const char *title, int active, int checked) {
    pos = bench_draw_control_txt(pos,title,active);
    pos = bench_draw_control_txt(pos," [",active);
    pos = bench_draw_control_txt(pos,(checked)?"\x95":" ",active);
    pos = bench_draw_control_txt(pos,"]",active);
    return pos;
}
static void draw_controls(void) {
    int pos = bench_draw_control_txt(1,lang_str(LANG_BENCH_START),bench_cur_ctl == BENCH_CTL_START);
    pos = bench_draw_checkbox(pos+1,lang_str(LANG_BENCH_LOG),bench_cur_ctl == BENCH_CTL_LOG,bench_log);
    bench_draw_checkbox(pos+1,lang_str(LANG_BENCH_SDTEST),bench_cur_ctl == BENCH_CTL_SD,bench_mode_next == BENCH_ALL);
}

//-------------------------------------------------------------------

static void run_test(int num);

void gui_bench_draw() {
    int log_run = 0;
    switch (bench_to_run) {
        case 0:
            break;
        case 1:
            benchbuf = malloc(0x10000);
            txtbufptr = 0;
            // fall through
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
            run_test(bench_to_run);
            bench_to_run++;
            break;
        case 6:
            if (bench_mode!=BENCH_NOCARD) {
                run_test(bench_to_run);
                bench_to_run++;
                break;
            }
            // fall through
        case 10:
            run_test(bench_to_run);
            if (bench_mode!=BENCH_NOCARD) {
                remove(BENCHTMP);
            }
            bench_to_run = 0;
            bench_mode = 0;
            bench_to_draw = 1;
            log_run = 1;
            if (benchbuf)
                free(benchbuf);
            break;
    }
    switch (bench_to_draw) {
        case 1:
            draw_rectangle(camera_screen.disp_left, 0, camera_screen.disp_right, camera_screen.height-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_controls();

            draw_txt_string(1, 2,  lang_str(LANG_BENCH_CPU), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

            draw_txt_string(1, 3,  lang_str(LANG_BENCH_SCREEN),       MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 4,  lang_str(LANG_BENCH_WRITE), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 5,  lang_str(LANG_BENCH_READ), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

            draw_txt_string(1, 6,  lang_str(LANG_BENCH_MEMORY),  MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 7,  lang_str(LANG_BENCH_WRITE), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 8,  lang_str(LANG_BENCH_READ), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

            draw_txt_string(1, 9,  lang_str(LANG_BENCH_TEXTOUT), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

            draw_txt_string(1, 10, lang_str(LANG_BENCH_FLASH_CARD), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 11, lang_str(LANG_BENCH_WRITE_RAW), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 12, lang_str(LANG_BENCH_WRITE_MEM), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 13, lang_str(LANG_BENCH_WRITE_64K), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(3, 14, lang_str(LANG_BENCH_READ_64K), MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            
            /* no break here */
        case 2:
            add_log_head(log_run);
            gui_bench_draw_results_cpu(2, bench.cpu_ips);
            add_to_log(log_run,"CPU             :",buf);

            gui_bench_draw_results_screen(4, bench.screen_output_bps, camera_screen.buffer_size);
            add_to_log(log_run,"Screen write    :",buf);
            gui_bench_draw_results_screen(5, bench.screen_input_bps, bench.screen_input_size);
            add_to_log(log_run,"Viewport read   :",buf);

            gui_bench_draw_results_memory(7, bench.memory_write_bps, bench.memory_write_uc_bps);
            add_to_log(log_run,"Memory write    :",buf);
            gui_bench_draw_results_memory(8, bench.memory_read_bps, bench.memory_read_uc_bps);
            add_to_log(log_run,"Memory read     :",buf);

            gui_bench_draw_results_text(9, bench.text_cps, bench.textnonalign_cps, camera_screen.width * camera_screen.height);
            add_to_log(log_run,"Text drawing    :",buf);
            buf[0] = 0; // empty buffer before optional tests to avoid confusing output when those are not enabled

            gui_bench_draw_results(11, bench.disk_write_raw_kbps);
            add_to_log(log_run,"Card write (RAW):",buf);
            gui_bench_draw_results(12, bench.disk_write_mem_kbps);
            add_to_log(log_run,"Card write (MEM):",buf);
            gui_bench_draw_results(13, bench.disk_write_buf_kbps);
            add_to_log(log_run,"Card write (64k):",buf);
            gui_bench_draw_results(14, bench.disk_read_buf_kbps);
            add_to_log(log_run,"Card read  (64k):",buf);

            write_log(log_run);

            bench_to_draw = 0;
            break;
        default:
            bench_to_draw = 0;
            break;
    }
}

//-------------------------------------------------------------------

static unsigned int getcurrentmachinetime() {
    return *(unsigned int*)0xc0242014; // should be masked to the lower 20 bits (but high order bits are 0 in practice)
}

static unsigned int get_usec_diff(unsigned int t) {
    unsigned int v = *(unsigned int*)0xc0242014;
    if (v<t)
        return 0x100000+v-t;
    return v-t;
}

/*
 * write bytes uncached
 */
static void bench_screen_write() {
    long t;
    register unsigned int s;
    register char c;
    register char *scr;
    
    scr = vid_get_bitmap_fb();
    s = camera_screen.buffer_size;
    t = get_tick_count();

    for (c=0; c<64; ++c) {
        asm volatile (
            "mov    r0, %0\n"
            "mov    r1, %1\n"
        "bsw_loop:\n"
            "strb   r0, [r1]\n"
            "add    r1, r1, #1\n"
            "cmp    r1, %2\n"
            "bne    bsw_loop\n"
            :: "r" (c), "r" (scr), "r" (scr+s) : "r0", "r1"
        );
    };
    t = get_tick_count() - t;
    bench.screen_output_bps = (t==0)?0:s*64*100 / (t/10);
    msleep(10);
}

/*
 * read bytes uncached
 */
static void bench_screen_read() {
    long t;
    register unsigned int n, s;
    register char *scr;

    scr = vid_get_viewport_active_buffer();
    if (!scr) return;
    s = vid_get_viewport_byte_width() * vid_get_viewport_height_proper();
    // limit used mem area (due to low speed access)
    if ((s < 1) || (s > 360*240*3)) s = 360*240*3;
    bench.screen_input_size = s;
    t = get_tick_count();
    for (n=0; n<32; ++n) {
        asm volatile (
            "mov    r1, %0\n"
        "bsr_loop:\n"
            "ldrb   r0, [r1]\n"
            "add    r1, r1, #1\n"
            "cmp    r1, %1\n"
            "bne    bsr_loop\n"
            :: "r" (scr), "r" (scr+s) : "r0", "r1"
        );
    };
    t = get_tick_count() - t;
    bench.screen_input_bps = (t==0)?0:s*32*100 / (t/10);
    msleep(10);
}

/*
 * write words
 */
static void bench_mem_write(int *buff) {
    long t, m;
    register unsigned int n;
    register char *buf = (char *)buff;
    int *result[2];
    result[0]=&bench.memory_write_bps;
    result[1]=&bench.memory_write_uc_bps;
    for (m=0; m<2; m++) {
        if (m==1) {
            buf=ADR_TO_UNCACHED(buf);
        }
        t = get_tick_count();
        for (n=0; n<1024; ++n) {
            asm volatile (
                "mov    r0, %0\n"
                "mov    r1, %2\n"
            "bmw_loop:\n"
                "str    r1, [r0]\n"
                "add    r0, r0, #4\n"
                "cmp    r0, %1\n"
                "bne    bmw_loop\n"
                :: "r" (buf), "r" (buf+0x10000), "r" (n) : "r0", "r1"
            );
        };
        t = get_tick_count() - t;
        *result[m] = (t==0)?0:0x10000*100 / (t/10) * 1024;
        msleep(10);
    }
}

/*
 * read words
 */
static void bench_mem_read(int *buff) {
    long t, m;
    register unsigned int n;
    register char *buf = (char *)buff;
    int *result[2];
    result[0]=&bench.memory_read_bps;
    result[1]=&bench.memory_read_uc_bps;
    for (m=0; m<2; m++) {
        if (m==1) {
            buf=ADR_TO_UNCACHED(buf);
        }
        t = get_tick_count();
        for (n=0; n<1024; ++n) {
            asm volatile (
                "mov    r0, %0\n"
                "mov    r1, %2\n"
            "bmr_loop:\n"
                "ldr    r1, [r0]\n"
                "add    r0, r0, #4\n"
                "cmp    r0, %1\n"
                "bne    bmr_loop\n"
                :: "r" (buf), "r" (buf+0x10000), "r" (n) : "r0", "r1"
            );
        };
        t = get_tick_count() - t;
        *result[m] = (t==0)?0:0x10000*100 / (t/10) * 1024;
        msleep(10);
    }
}

/*
 * text writing speed (default font)
 */
static void bench_measure_text_write() {
    unsigned int t, best, d;
    register int c;

    // 4 test runs, return the best speed
    best = 0xffffffff;
    for (d=0; d<4; d++) {
        t = getcurrentmachinetime();
        for (c=0; c<16; c++) {
            // draw 80 chars
            draw_txt_string(1, 1,  "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_+", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_string(1, 1,  "abcdefghijklmnopqrstuvwxyz0123456789.-_+", MAKE_COLOR(COLOR_GREEN, COLOR_RED));
        };
        t = get_usec_diff(t);
        msleep(10);
        if (t < best) best = t;
    }
    bench.text_cps = (best==0)?0:16*80*1000000/best;
    best = 0xffffffff;
    for (d=0; d<4; d++) {
        t = getcurrentmachinetime();
        for (c=0; c<16; c++) {
            // draw 80 chars
            draw_txt_stringnonalign(1, 1,  "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_+", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            draw_txt_stringnonalign(1, 1,  "abcdefghijklmnopqrstuvwxyz0123456789.-_+", MAKE_COLOR(COLOR_GREEN, COLOR_RED));
        };
        t = get_usec_diff(t);
        msleep(10);
        if (t < best) best = t;
    }
    bench.textnonalign_cps = (best==0)?0:16*80*1000000/best;
}

//-------------------------------------------------------------------

/*
 * simple loop for "measuring" CPU speed
 * ARM mode is used due to the ambiguity of thumb mnemonics (sub vs. subs)
 */
void __attribute__((naked,noinline)) busyloop_thumb(int loops) {
    (void)loops;
    asm volatile (
#if defined(__thumb__)
    ".code 16\n"
    "bx      pc\n"
    ".code 32\n"
#endif
    ".align 4\n"
    "z1:\n"
    "subs    r0, r0, #1\n"
    "bhi     z1\n"
    "bx      lr\n"
    );
}

#define CYCLES 100000 // how many cycles to do
#define INSTPERCYCLE 2 // instructions / cycle in the countdown part of code

static void bench_measure_cpuspeed() {
    unsigned int t, best, c;

    // 4 test runs, return the best speed
    best = 0xffffffff;
    for (c=0; c<4; c++) {
        t = getcurrentmachinetime();
        busyloop_thumb(CYCLES);
        t = get_usec_diff(t);
        msleep(10);
        if (t < best) best = t;
    }
    bench.cpu_ips = (best==0)?0:INSTPERCYCLE*CYCLES/best; // MIPS
}

//-------------------------------------------------------------------

static void run_test(int num) {
    long t;
    int x;
    unsigned int n, s;
    switch (num) {
        case 1:
            bench.cpu_ips = 0;
            bench_measure_cpuspeed();
            bench.screen_output_bps = 0;
            bench_to_draw = 2;
            break;
        case 2:
            bench_screen_write();
            bench.screen_input_bps = 0;
            bench_to_draw = 1;
            break;
        case 3:
            bench_screen_read();
            bench.memory_write_bps = 0;
            bench_to_draw = 2;
            break;
        case 4:
            if (benchbuf) {
                bench_mem_write(benchbuf);
            }
            bench.memory_read_bps = 0;
            bench_to_draw = 2;
            break;
        case 5:
            if (benchbuf) {
                bench_mem_read(benchbuf);
            }
            bench.text_cps = 0;
            bench_to_draw = 2;
            break;
        case 6:
            bench_measure_text_write();
            if (bench_mode!=BENCH_NOCARD) {
                bench.disk_write_raw_kbps = 0;
            }
            bench_to_draw = 1;
            break;
        case 7:
            x = open(BENCHTMP, O_WRONLY|O_CREAT, 0777);
            if (x>=0) {
                t = get_tick_count();
                s=write(x, hook_raw_image_addr(), camera_sensor.raw_size)/1024;
                t = get_tick_count() - t;
                close(x);
                bench.disk_write_raw_kbps = (t==0)?0:s*100 / (t/10);
            }
            bench.disk_write_mem_kbps = 0;
            bench_to_draw = 2;
            break;
        case 8:
            x = open(BENCHTMP, O_WRONLY|O_CREAT, 0777);
            if (x>=0) {
                t = get_tick_count();
                s=write(x, (void*)0x10000, 0xC00000)/1024;
                t = get_tick_count() - t;
                close(x);
                bench.disk_write_mem_kbps = (t==0)?0:s*100 / (t/10);
            }
            bench.disk_write_buf_kbps = 0;
            bench_to_draw = 2;
            break;
        case 9:
            if (benchbuf) {
                x = open(BENCHTMP, O_WRONLY|O_CREAT, 0777);
                if (x>=0) {
                    s = 0;
                    t = get_tick_count();
                    for (n=0; n<256; ++n)
                        s+=write(x, benchbuf, 0x10000);
                    t = get_tick_count() - t;
                    s = s / 1024;
                    close(x);
                    bench.disk_write_buf_kbps = (t==0)?0:s*100 / (t/10);
                }
            }
            bench.disk_read_buf_kbps = 0;
            bench_to_draw = 2;
            break;
        case 10:
            if (benchbuf) {
                x = open(BENCHTMP, O_RDONLY, 0777);
                if (x>=0) {
                    s = 0;
                    t = get_tick_count();
                    for (n=0; n<256; ++n)
                        s+=read(x, benchbuf, 0x10000);
                    t = get_tick_count() - t;
                    s = s / 1024;
                    close(x);
                    bench.disk_read_buf_kbps = (t==0)?0:s*100 / (t/10);
                }
            }
            bench_to_draw = 2;
            break;
    }
}

//-------------------------------------------------------------------

int gui_bench_kbd_process()
{
    if (bench_to_run) return 0; // ignore keyboard during tests
    switch (kbd_get_autoclicked_key()) {
    case KEY_SET:
        switch(bench_cur_ctl) {
            case BENCH_CTL_START:
                gui_bench_init();
                bench_mode = bench_mode_next;
                bench_to_run = 1;
                break;
             case BENCH_CTL_LOG:
                bench_log = !bench_log;
                bench_to_draw = 1;
                break;
             case BENCH_CTL_SD:
                bench_mode_next = (bench_mode_next == BENCH_ALL)?BENCH_NOCARD:BENCH_ALL;
                bench_to_draw = 1;
                break;
        }
        break;
    // switch controls
    case KEY_LEFT:
        bench_cur_ctl = (bench_cur_ctl>0)?bench_cur_ctl-1:BENCH_CTL_MAX;
        bench_to_draw = 1;
        break;
    case KEY_RIGHT:
        bench_cur_ctl = (bench_cur_ctl<BENCH_CTL_MAX)?bench_cur_ctl+1:0;
        bench_to_draw = 1;
        break;
    }
    return 0;
}

//-------------------------------------------------------------------

int basic_module_init()
{
    int cl = strlen(lang_str(LANG_BENCH_CALCULATING));
    strncpy(clearline, lang_str(LANG_BENCH_CALCULATING), 27);
    if (cl < 27)
    {
        strncpy(clearline+cl, "                           ", 27-cl);
    }

    gui_bench_init();
    gui_set_mode(&GUI_MODE_BENCH);
    return 1;
}

#include "simple_module.c"

/******************** Module Information structure ******************/

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SIMPLE_MODULE_VERSION,		// Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,			// Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,		// Specify platform dependency

    -LANG_MENU_DEBUG_BENCHMARK,
    MTYPE_TOOL,             //Test camera low level performance

    &_librun.base,

    ANY_VERSION,                // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    CAM_SENSOR_VERSION,         // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0x2a,                       // Menu symbol
};
