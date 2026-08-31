#include "camera_info.h"
#include "gui.h"
#include "fileutil.h"
#include "conf.h"

#include "script_api.h"
#include "module_def.h"
#include "gui_lang.h"
#include "keyboard.h"

/***************** BEGIN OF AUXILARY PART *********************
  ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
 **************************************************************/

int ubasic_init(const char *program, int is_ptp);
int ubasic_run(void);
void ubasic_end(void);
void ubasic_set_variable(int varum, int value);
void ubasic_set_as_ret(int md_ret);
int jump_label(char * label);

static int ubasic_run_restore(void)             { return jump_label("restore"); }
static int ubasic_refresh_display(void)         { return 0; }

static void _set_variable(char *name, int value,
    __attribute__ ((unused))int isBool, __attribute__ ((unused))int isTable, __attribute__ ((unused))int labelCount, __attribute__ ((unused))const char **labels)
{
    ubasic_set_variable(name[0] - (name[0]>='a'?'a':('A'-26)), value);
}

static char *script_source = 0;
static char last_script[100] = "";

static int ubasic_init_file(char const* filename)
{
    // before 1.7, dial events are not seen by script or blocked form Canon FW
    if ((script_version.major < 1) || (script_version.major == 1 && script_version.minor < 7))
        camera_info.state.script_dial_control = 0;
    else
        camera_info.state.script_dial_control = DIAL_SCRIPT_KEYCLICK;

    if (strcmp(last_script, filename) != 0)
    {
        strcpy(last_script, filename);
        if (script_source)
            free(script_source);
        script_source = load_file(filename, 0, 1);
    }
    if (script_source)
    {
        return ubasic_init(script_source, 0);
    }
    return 0;
}

// shoot hooks not supported in ubasic
static void ubasic_script_shoot_hook_run(__attribute__ ((unused))int hook) { return; }
/******************** Module Information structure ******************/

static int module_unloader()
{
    if (script_source)
    {
        free(script_source);
        script_source = 0;
    }
    return 0;
}

libscriptapi_sym _libubasic =
{
    {
         0, module_unloader, 0, 0, 0
    },

    ubasic_init,
    ubasic_init_file,
    ubasic_run,
    ubasic_end,
    _set_variable,
    ubasic_set_as_ret,
    ubasic_run_restore,
    ubasic_script_shoot_hook_run,
    ubasic_refresh_display,
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    SCRIPT_API_VERSION,			// Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,			// Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,		// Specify platform dependency

    -LANG_MODULE_UBASIC,        // Module name
    MTYPE_SCRIPT_LANG,          //Run uBasic Scripts

    &_libubasic.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    CAM_SENSOR_VERSION,         // CAM SENSOR version
    CAM_INFO_VERSION,           // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
