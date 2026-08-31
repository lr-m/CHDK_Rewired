#include "stdlib.h"
#include "string.h"
#include "ctype.h"
#include "script_shoot_hook.h"
#include "script_api.h"

typedef struct {
    int count; // how many times the hook was called (since script start)
    int timeout; // timeout for blocking
    int active; // hook has been reached, set from hooked task, cleared from script to resume
} script_shoot_hook_t;

const char* shoot_hook_names[SCRIPT_NUM_SHOOT_HOOKS] = {
    "hook_preshoot",
    "hook_shoot",
    "hook_raw",
};

static script_shoot_hook_t hooks[SCRIPT_NUM_SHOOT_HOOKS];

// initialize, when script is loaded or unloaded
void script_shoot_hooks_reset(void)
{
    memset(hooks,0,sizeof(hooks));
}

// enable hooking with specified timeout, 0 to disable
void script_shoot_hook_set(int hook,int timeout) {
    hooks[hook].timeout = timeout;
}

void rawop_update_hook_status(int active);

// called from hooked task, returns when hook is released or times out
void script_shoot_hook_run(int hook)
{
    int timeleft = hooks[hook].timeout;
    hooks[hook].count++;
    // only mark hook active if it was set
    if(timeleft > 0) {
        // notify rawop when in raw hook, so it can update valid raw status
        // and values that might change between shots
        if(hook == SCRIPT_SHOOT_HOOK_RAW) {
            rawop_update_hook_status(1);
        }
        hooks[hook].active = 1;
        while(timeleft > 0 && hooks[hook].active) {
            msleep(10);
            timeleft -= 10;
        }
        if(hook == SCRIPT_SHOOT_HOOK_RAW) {
            rawop_update_hook_status(0);
        }
        hooks[hook].active = 0;
    }
    // TODO may want to record timeout
}

// returns true when hooked task in hook_run
int script_shoot_hook_ready(int hook)
{
    return hooks[hook].active;
}

// called from script to allow hook_run to return
void script_shoot_hook_continue(int hook)
{
    hooks[hook].active = 0;
}

// return number of times hook has been called (for this script)
int script_shoot_hook_count(int hook)
{
    return hooks[hook].count;
}

