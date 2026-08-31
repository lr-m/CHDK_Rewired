#include "camera_info.h"
#include "lang.h"
#include "conf.h"
#include "shooting.h"
#include "time.h"
#include "clock.h"
#include "task.h"

#include "gui.h"
#include "gui_menu.h"
#include "gui_lang.h"
#include "semaphore.h"

static int running = 0;

typedef struct {
    int handle;
    int (*send_data)(int handle, const char *buf, int part_size, int total_size, int, int, int); // total_size will be 0 except for the first call
} dummy_ptp_data;

extern int live_view_get_data(dummy_ptp_data *data, int flags);

int send_data_to_file(int handle, const char *buf, int part_size, int total_size, __attribute__ ((unused))int u1, __attribute__ ((unused))int u2, __attribute__ ((unused))int u3)
{
    // TODO write on some cams may require uncached, buffers are but structs + palette may not be
    // only the first call has total size set, write length of following record
    if(total_size) {
        write(handle, &total_size, sizeof(total_size));
    }
    write(handle, buf, part_size);
    return 0;
}
static dummy_ptp_data ptp_data = {
    -1,
    send_data_to_file,
};
static char fn[32];

static int scrdump_start(void)
{
    running = 1;
    int fd;
    static int lvdump_header[] = {
        0x766c6863, // magic chlv
        8, // size of version record
        1, // major ver
        0, // minor ver
    };
    // could be desirable but unlikely to work well. Better to use PTP
    if ( is_video_recording() ) {
        return -1;
    }
    // TODO might want to allow specifying name
    mkdir("A/DCIM");
    mkdir("A/DCIM/100CANON");
    fd = -1;
    int cnt = 0;
    do {
        sprintf(fn, "A/DCIM/100CANON/CRW_%04d.JPG", cnt++);
        if (stat(fn,0) != 0) {
            fd = open(fn, O_WRONLY|O_CREAT, 0777);
            break;
        }
    } while(cnt<9999);
    if (fd>=0) {
        ptp_data.handle = fd;
        // TODO write on some cams may require uncached
        write(fd, lvdump_header,sizeof(lvdump_header));
        return 0;
    }
    return -1;
}
static void scrdump_frame(int what)
{
    // TODO could limit frames, time, roll over files at specific size
    if(ptp_data.handle >= 0) {
        live_view_get_data(&ptp_data,what);
    }
}
static void scrdump_finish(void)
{
    if(ptp_data.handle >= 0) {
        close(ptp_data.handle);
        ptp_data.handle = -1;
        struct utimbuf t;
        t.actime = t.modtime = time(NULL);
        utime(fn, &t);
    }
}

// commands from the gui to the dumper task
enum SCRDUMP_CMD
{
    SCRDUMP_CMD_NONE = 0, // no command
    SCRDUMP_CMD_SCHEDULE, // schedule a dump
    SCRDUMP_CMD_EXIT,     // end task, ending any current dump
};

#define CMD_SEM_TIMEOUT 1000
static int cmd_sem;   // semaphore for command and settings
static int cmd_next;  // command for dumper task, cleared on receipt

typedef struct {
    int frame_buffers; // buffer selection, as used by live_view_get_data
    int frame_count;   // number of frames to grab
    int frame_time;    // ms between frame starts
    int start_delay;   // ms delay from dump request to dump start
} scrdump_settings_t;

scrdump_settings_t settings_next; // settings from menu on SCRDUMP_CMD_SCHEDULE

// states of the dumper task
enum SCRDUMP_STATE
{
    SCRDUMP_STATE_IDLE = 0, // nothing scheduled, just poll for commands
    SCRDUMP_STATE_PENDING,  // dump requested, not started
    SCRDUMP_STATE_RUN,      // dump frames
};


static int scrdump_task_running;
void scrdump_task(void)
{
    static scrdump_settings_t settings;

    int state = SCRDUMP_STATE_IDLE;
    int frames_left = 0;

    unsigned next_action_tick = get_tick_count();

    while(1) {
        if (!TakeSemaphore(cmd_sem, CMD_SEM_TIMEOUT)) {
            int cmd = cmd_next;
            cmd_next = SCRDUMP_CMD_NONE;
            if(cmd == SCRDUMP_CMD_SCHEDULE) {
                if(state == SCRDUMP_STATE_RUN) {
                    scrdump_finish();
                }
                memcpy(&settings,&settings_next,sizeof(settings));
                next_action_tick = (unsigned)get_tick_count() + settings.start_delay;
                frames_left = settings.frame_count;
                state = SCRDUMP_STATE_PENDING;
            } else if(cmd == SCRDUMP_CMD_EXIT) {
                GiveSemaphore(cmd_sem);
                if(state == SCRDUMP_STATE_RUN) {
                    scrdump_finish();
                }
                break;
            }
            GiveSemaphore(cmd_sem);
        }
        if(state == SCRDUMP_STATE_PENDING) {
            if(next_action_tick <= (unsigned)get_tick_count()) {
                // if starting dump now, schedule first frame immediately, next_action_tick unchanged
                if(scrdump_start() == 0) {
                    state = SCRDUMP_STATE_RUN;
                } else { // failed
                    state = SCRDUMP_STATE_IDLE;
                }
            }
        }
        if(state == SCRDUMP_STATE_RUN) {
            unsigned t = (unsigned)get_tick_count();
            if(next_action_tick <= t) {
                if(frames_left > 0) {
                    scrdump_frame(settings.frame_buffers);
                    frames_left--;

                    // next frame from the start of this frame
                    next_action_tick = t + settings.frame_time;
                }
                if(frames_left == 0) {
                    scrdump_finish();
                    state = SCRDUMP_STATE_IDLE;
                }
            }
        }
        if(state == SCRDUMP_STATE_IDLE) {
            next_action_tick =  (unsigned)get_tick_count() + 100;
        }

        unsigned now = (unsigned)get_tick_count();
        if(next_action_tick >= now) {
            unsigned sleep_time = next_action_tick - now;
            // sleep a maximum of 100ms to receive commands
            // minimum of 10 to avoid hogging cpu
            if(sleep_time > 100) {
                sleep_time = 100;
            } else if(sleep_time < 10) {
                sleep_time = 10;
            }
            msleep((long)sleep_time);
        } else {
            msleep(10);
        }
    }
    scrdump_task_running = 0;
    ExitTask();
}

// menu values
static int scrdump_frames = 1;
static int scrdump_frame_delay = 10;
static int scrdump_buffers = 2; // both
static int scrdump_skip = 0;
static int scrdump_delay = 5;

void scrdump_schedule(void) {
    // if it times out, cmd is ignored
    if (!TakeSemaphore(cmd_sem, CMD_SEM_TIMEOUT)) {
        cmd_next = SCRDUMP_CMD_SCHEDULE;
        // 1 = bitmap 2 = both
        if(scrdump_buffers > 0) {
            settings_next.frame_buffers = 0x4;
            // skip only applies if bitmap selected
            // TODO could theoretically want alpha only on d6?
            if(!scrdump_skip) {
#ifdef THUMB_FW
                settings_next.frame_buffers |= 0x10;
#else
                settings_next.frame_buffers |= 0x8;
#endif
            }
        }
        // viewport setting
        // 0 = viewport 2 = both
        if(scrdump_buffers == 0 || scrdump_buffers == 2) {
            settings_next.frame_buffers |= 1;
        }
        settings_next.frame_count = scrdump_frames;
        settings_next.frame_time = scrdump_frame_delay*10;
        settings_next.start_delay = scrdump_delay*1000;
        GiveSemaphore(cmd_sem);
    }
}
void scrdump_menu_schedule(void)
{
    scrdump_schedule();
    gui_set_mode(&altGuiHandler);
}

void scrdump_menu_exit(void)
{
    // if it times out, cmd is ignored
    if (!TakeSemaphore(cmd_sem, CMD_SEM_TIMEOUT)) {
        cmd_next = SCRDUMP_CMD_EXIT;
        GiveSemaphore(cmd_sem);
        while(scrdump_task_running) {
            msleep(10);
        }
        gui_set_mode(&altGuiHandler);
        running = 0;
    }
}


static const char* scrdump_buffer_names[] =            { "Viewprt", "Bitmap", "Both"};
static CMenuItem scrdump_submenu_items[] = {
    MENU_ITEM   (0x5c,(int)"Schedule dump",                 MENUITEM_PROC,                  scrdump_menu_schedule,      0 ),
    MENU_ITEM   (0x2a,(int)"Frames",                        MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &scrdump_frames, MENU_MINMAX(1, 1000) ),
    MENU_ITEM   (0x2a,(int)"Frame Time (ms*10)",            MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &scrdump_frame_delay, MENU_MINMAX(4, 1000)),
    MENU_ENUM2  (0x5c,(int)"Framebuffers",                  &scrdump_buffers,               scrdump_buffer_names ),
#ifdef THUMB_FW
    MENU_ITEM   (0x5c,(int)"Skip Opacity",                  MENUITEM_BOOL,                  &scrdump_skip,         0 ),
#else
    MENU_ITEM   (0x5c,(int)"Skip Palette",                  MENUITEM_BOOL,                  &scrdump_skip,         0 ),
#endif
    MENU_ITEM   (0x2a,(int)"Dump Start Delay (s)",          MENUITEM_INT|MENUITEM_F_UNSIGNED|MENUITEM_F_MINMAX,   &scrdump_delay, MENU_MINMAX(0, 60)),
    MENU_ITEM   (0x5c,(int)"Exit module",                   MENUITEM_PROC,                  scrdump_menu_exit,      0 ),
    MENU_ITEM   (0x51,LANG_MENU_BACK,                       MENUITEM_UP,                    0,                                  0 ),
    {0}
};

static CMenu scrdump_submenu = {0x2a,(int)"Screen Dump Settings", scrdump_submenu_items };

int _run()
{
    running = 1;
    if(!cmd_sem) {
        cmd_sem = CreateBinarySemaphore("ScrDumpCmd", 1);
    }
    if(!scrdump_task_running) {
        // lower priority (higher number) than spytask, like dng reverser
        CreateTask("ScrDump",0x1A,0x800,scrdump_task);
        scrdump_task_running = 1;
    }

    gui_activate_sub_menu(&scrdump_submenu);
    return 0;
}

#include "simple_module.h"

// =========  MODULE INIT =================

/***************** BEGIN OF AUXILARY PART *********************
  ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
 **************************************************************/

//---------------------------------------------------------
// PURPOSE: Finalize module operations (close allocs, etc)
// RETURN VALUE: 0-ok, 1-fail
//---------------------------------------------------------
int _module_unloader()
{
    if(cmd_sem) {
        DeleteSemaphore(cmd_sem);
    }
    return 0;
}

int _module_can_unload()
{
    return (running == 0);
}

/******************** Module Information structure ******************/

libsimple_sym _libscrdump =
{
    {
        0, _module_unloader, _module_can_unload, 0, _run
    },
};

ModuleInfo _module_info = {
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SIMPLE_MODULE_VERSION,        // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,         // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int)"Screen Record",
    MTYPE_TOOL|MTYPE_SUBMENU_TOOL, // configure with menu

    &_libscrdump.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    CAM_INFO_VERSION,           // CAM INFO version
    0,
};

/*************** END OF AUXILARY PART *******************/
