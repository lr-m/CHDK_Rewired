#include "camera.h"
#include "lolevel.h"
#include "platform.h"
#include "core.h"
#include "keyboard.h"
#include "stdlib.h"

/* Ours stuff */
extern long link_bss_start;
extern long link_bss_end;
extern void boot();

static void core_hook_task_create(__attribute__ ((unused))void *tcb)
{
}

static int my_ncmp(const char *s1, const char *s2, long len);

static void core_hook_task_delete(void *tcb)
{
    char *name = (char*)(*(long*)((char*)tcb+0x34));
    // Prefix compare, not strcmp. The task's real name is longer than the
    // string being matched - on the a460 it is "tInitFileModules" - so an exact
    // comparison never fires and spytask never learns the file system is ready.
    // It then falls through its own 10 second timeout instead, which works by
    // accident rather than by design.
    //
    // createHook below has always used a 10 character prefix compare against
    // this same name, which is why the file-modules *replacement* hook fires on
    // cameras where this one silently does not. Matching it here makes the two
    // agree.
    if (my_ncmp(name,"tInitFileM",10)==0) core_spytask_can_start();
}


static int stop_hooking;

static void (*task_prev)(
    long p0,    long p1,    long p2,    long p3,    long p4,
    long p5,    long p6,    long p7,    long p8,    long p9);

static void (*init_file_modules_prev)(
    long p0,    long p1,    long p2,    long p3,    long p4,
    long p5,    long p6,    long p7,    long p8,    long p9);



void spytask(long ua, long ub, long uc, long ud, long ue, long uf)
{
    (void)ua; (void)ub; (void)uc; (void)ud; (void)ue; (void)uf;
    core_spytask();
}


static void task_start_hook(
    long p0,    long p1,    long p2,    long p3,    long p4,
    long p5,    long p6,    long p7,    long p8,    long p9)
{
    _CreateTask("SpyTask", 0x19, 0x2000, spytask, 0);

    task_prev(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9 );
}


#if !CAM_MULTIPART
static void remount_filesystem()
{
    _Unmount_FileSystem();
    _Mount_FileSystem();
}


static void init_file_modules_hook(
    long p0,    long p1,    long p2,    long p3,    long p4,
    long p5,    long p6,    long p7,    long p8,    long p9)
{
    remount_filesystem();
    init_file_modules_prev(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9 );
}
#endif


static void capt_seq_hook(
    long p0,    long p1,    long p2,    long p3,    long p4,
    long p5,    long p6,    long p7,    long p8,    long p9)
{
    (void)p0; (void)p1; (void)p2; (void)p3; (void)p4; (void)p5; (void)p6; (void)p7; (void)p8; (void)p9;
    capt_seq_task();
}


static void physw_hook(
    long p0,    long p1,    long p2,    long p3,    long p4,
    long p5,    long p6,    long p7,    long p8,    long p9)
{
    (void)p0; (void)p1; (void)p2; (void)p3; (void)p4; (void)p5; (void)p6; (void)p7; (void)p8; (void)p9;
    mykbd_task();
}


#if CAM_CHDK_HAS_EXT_VIDEO_MENU
static void movie_record_hook(
    long p0,    long p1,    long p2,    long p3,    long p4,
    long p5,    long p6,    long p7,    long p8,    long p9)
{
    (void)p0; (void)p1; (void)p2; (void)p3; (void)p4; (void)p5; (void)p6; (void)p7; (void)p8; (void)p9;
    movie_record_task();
}
#endif


static int my_ncmp(const char *s1, const char *s2, long len)
{
    int i;
    for (i=0;i<len;i++){
    if (s1[i] != s2[i])
        return 1;
    }
    return 0;
}

void createHook (void *pNewTcb)
{
    char *name = (char*)(*(long*)((char*)pNewTcb+0x34));
    long *entry = (long*)((char*)pNewTcb+0x74);

#if defined(CAM_STARTUP_IMAGE) && !defined(CAM_STARTUP_IMAGE_TASK)
    // Every task creation is a chance to catch the startup image buffer after
    // Canon has filled it and before Canon has drawn it. Latches itself off
    // after the first successful write.
    //
    // Only for cameras WITHOUT a replaceable startup-image task. On the A430,
    // fill and draw are consecutive statements inside one function, so no
    // outside write can ever land between them - that camera defines
    // CAM_STARTUP_IMAGE_TASK and is served entirely by the replacement below.
    {
        extern void patch_startup_image(void);
        patch_startup_image();
    }
#endif

    // always hook first task creation
    // to create SpyProc
    if (!stop_hooking){
        task_prev = (void*)(*entry);
        *entry = (long)task_start_hook;
        stop_hooking = 1;
    } else {
        // hook/replace another tasks
        if (my_ncmp(name, "tPhySw", 6) == 0){
            *entry = (long)physw_hook;
        }

        if (my_ncmp(name, "tInitFileM", 10) == 0){
            init_file_modules_prev = (void*)(*entry);
#if CAM_MULTIPART
            *entry = (long)init_file_modules_task;
#else
            *entry = (long)init_file_modules_hook;
#endif
        }

        if (my_ncmp(name, "tCaptSeqTa", 10) == 0){
            *entry = (long)capt_seq_hook;
        }

#ifdef CAM_STARTUP_IMAGE_TASK
        // Matched on the entry address rather than the task name. The name
        // Canon's _CreateTask wrapper ends up registering for this one is not
        // obvious from the ROM, and the entry address is unambiguous.
        if (*entry == (long)CAM_STARTUP_IMAGE_TASK){
            extern void my_startup_image_task(void);
            *entry = (long)my_startup_image_task;
        }
#endif

#if CAM_CHDK_HAS_EXT_VIDEO_MENU
        if (my_ncmp(name, "tMovieReco", 10) == 0){
            *entry = (long)movie_record_hook;
        }
#endif

#if CAM_EXT_TV_RANGE
        if (my_ncmp(name, "tExpDrvTas", 10) == 0){
            *entry = (long)exp_drv_task;
        }
#endif

#if CAM_HAS_FILEWRITETASK_HOOK
        if (my_ncmp(name, "tFileWrite", 10) == 0){
            *entry = (long)filewritetask;
        }
#endif

// for cameras that have a "touch control dial" with 'TouchW' task.
// some cameras may use a different task name
#ifdef HOOK_TOUCHW
        if (my_ncmp(name, "tTouchW", 7) == 0){
            *entry = (long)my_touchw_task;
        }
#endif

        core_hook_task_create(pNewTcb);
    }
}

void deleteHook (void *pTcb)
{
    core_hook_task_delete(pTcb);
}

void startup()
{
    long *bss = &link_bss_start;

    // sanity check
    if ((long)&link_bss_end > (MEMISOSTART + MEMISOSIZE)){
        started();
        shutdown();
    }

    // initialize .bss senment
    while (bss<&link_bss_end)
        *bss++ = 0;

    // fill memory with this magic value so we could see what
    // parts of memory were or not used
#if 0
    long *ptr;
    for (ptr=(void*)MEMBASEADDR;((long)ptr)<MEMISOSTART;ptr+=4){
    ptr[0]=0x55555555;
    ptr[1]=0x55555555;
    ptr[2]=0x55555555;
    ptr[3]=0x55555555;
    }
#endif

    boot();
}

#if CAM_CONSOLE_LOG_ENABLED

#define DEV_HDR_WRITE_OFFSET (0x14C/4)

typedef int DEV_HDR;

int (*_tyWriteOrig)(DEV_HDR *hdr, char *buf, int len);


int hook_tyWriteOrig(DEV_HDR *hdr, char *buf, int len)
{
    // Slow, but stable writes
    FILE *fd = fopen("A/stdout.txt", "a");
    if (fd) {
        fwrite(buf, 1, len, fd);
        fclose(fd);
    }

    return _tyWriteOrig(hdr, buf, len);

}

void cam_console_init()
{
    DEV_HDR *DRV_struct;

    extern DEV_HDR* _iosDevFind(char*, int);
    DRV_struct = _iosDevFind("/tyCo/0", 0);

    _tyWriteOrig = (void*)DRV_struct[DEV_HDR_WRITE_OFFSET];

    FILE *fd = fopen("A/chdklog.txt", "a");
    if (fd) {
        // can't be used with "Fut" API
        //fprintf(fd, "DRV_struct: %x, _tyWriteOrig: %x\n", DRV_struct, _tyWriteOrig);
        char buf[256];
        int buflen = sprintf(buf, "DRV_struct: %x, _tyWriteOrig: %x\n", DRV_struct, _tyWriteOrig);
        fwrite(buf, 1, buflen, fd);
    }

    FILE *fdout = fopen("A/stdout.txt", "r");
    if (fdout)
    {
        DRV_struct[DEV_HDR_WRITE_OFFSET] = (int)hook_tyWriteOrig;
        fclose(fdout);
        // fprintf(fd, "tyWrite replaced, camera log enabled\n");
        fwrite("tyWrite replaced, camera log enabled\n", 1, sizeof("tyWrite replaced, camera log enabled\n"), fd);
    }

    if (fd)
    {
        fclose(fd);
    }

}

#endif
