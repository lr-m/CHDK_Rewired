#ifndef CONF_MIGRATE_H
#define CONF_MIGRATE_H

// Load old (1.2) config file into current version.
// Provide access to conf variable for scripts using old ID's.

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

#include "flt.h"
#include "conf.h"

// Update version if changes are made to the module interface
#define CONF_MIGRATE_VERSION      {1,0}

// Config migration module interface
typedef struct
{
    base_interface_t    base;

    int (*load_old_config)(const confinfo_handler *confinfo_handlers);
    short (*map_old_id)(short);
} libconfmigrate_sym;

extern libconfmigrate_sym* libconfmigrate;

#endif
