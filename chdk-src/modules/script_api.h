#ifndef SCRIPT_API_H
#define SCRIPT_API_H

#include "flt.h"

// Return values from 'script_run'
#define SCRIPT_RUN_ENDED        0
#define SCRIPT_RUN_RUNNING      1
#define SCRIPT_RUN_ERROR        -1

enum {
    SCRIPT_SHOOT_HOOK_PRESHOOT=0,
    SCRIPT_SHOOT_HOOK_SHOOT,
    SCRIPT_SHOOT_HOOK_RAW,
    SCRIPT_NUM_SHOOT_HOOKS,
};

// Update version if changes are made to the module interface
#define SCRIPT_API_VERSION      {3,0}

// Module interface for script languages (Lua and uBasic)
typedef struct
{
    base_interface_t    base;

    int     (*script_start)( char const* script, int is_ptp ); // initialize and load script
    int     (*script_start_file)( char const* filename ); // initialize and load script from file
    int     (*script_run)( void );          // run script timeslice
    void    (*script_reset)(void);
    void    (*set_variable)(char *name, int value, int isBool, int isTable, int labelCount, const char **labels);
    void    (*set_as_ret)(int as_ret);      // save 'return' value from action stack code (e.g. motion detect, shoot)
    int     (*run_restore)( void );         // run the "restore" function at the end of a script
    void    (*shoot_hook)(int hook);        // run a hook in the shooting process, called from hooked task
    int     (*refresh_display)( void );     // alert script that screen needs to be refreshed
} libscriptapi_sym;

extern libscriptapi_sym* libscriptapi;
extern void module_set_script_lang( const char* script_file );

#endif
