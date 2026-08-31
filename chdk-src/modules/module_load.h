#ifndef __MODULE_LOAD_H__
#define __MODULE_LOAD_H__

// Module definitions for inclusion into core CHDK files.

#include "module_def.h"

#define MAX_NUM_LOADED_MODULES  10

// Base typedefs
//-------------------

// Handler structure for module loading / unloading
// The 'default_lib' value points to the 'unloaded' interface functions
// These can then auto-load the module when called and redirect to the loaded
// interface functions.
typedef struct
{
    base_interface_t**  lib;
    base_interface_t*   default_lib;
    _version_t          version;        // Should match module_version in ModuleInfo
                                        // For simple modules like games set to {0,0} to skip version check
    char*               name;
} module_handler_t;

typedef struct
{
    flat_hdr*           hdr;            // memory allocated for module, 0 if not used
    unsigned int        hName;          // hash of the module path name, for finding loaded modules
    module_handler_t*   hMod;           // handler info
} module_entry;

// Common module functions
//-------------------------

flat_hdr* module_preload(const char *path, const char *name, _version_t ver);
int module_load(module_handler_t* hMod);
void module_unload(const char* name);

void get_module_info(const char *name, ModuleInfo *mi, char *modName, int modNameLen);

int module_run(char* name);

module_entry* module_get_adr(unsigned int idx);

// Asynchronous unloading
//-------------------------
void module_exit_alt();
void module_tick_unloader();

// Logging
void module_log_clear();

#endif /* __MODULE_LOAD_H__ */
