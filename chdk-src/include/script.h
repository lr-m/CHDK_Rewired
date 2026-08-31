#ifndef SCRIPT_H
#define SCRIPT_H

// CHDK Script interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//-------------------------------------------------------------------
// Current stage of script processing

enum {	SCRIPT_STATE_INACTIVE=0,  // 0 - script is inactive now
		SCRIPT_STATE_RAN,	      // 1 - script works now
		SCRIPT_STATE_INTERRUPTED, // 2 - shutter button was pressed, cancel script
};

//-------------------------------------------------------------------
// Possible data types
#define DTYPE_INT       0           // Default
#define DTYPE_TABLE     1           // For parameters that select from a list of values, send to Lua as a table (Lua only)

// Structure used to hold a single script parameter
typedef struct _sc_param
{
    char            *name;          // Parameter name
    char            *desc;          // Title / description
    int             val;            // Current value
    int             def_val;        // Default value (from script, used to reset values)
    int             old_val;        // Previous value (to detect change)
    int             range;          // Min / Max values for validation
    short           range_type;     // Specifies if range values is signed (-9999-32767) or unsigned (0-65535)
                                    // Note: -9999 limit on negative values is due to current gui_menu code (and because menu only displays chars)
    short           data_type;      // See defines above
    int             option_count;   // Number of options for parameter
    char*           option_buf;     // Memory buffer to store option names for parameter
    const char**    options;        // Array of option names (points into option_buf memory)

    struct _sc_param* next;         // Next parameter in linked list
} sc_param;

//-------------------------------------------------------------------

// External references
extern char script_title[36];
extern sc_param* script_params;

//-------------------------------------------------------------------

extern void script_load(const char *fn);
extern void save_params_values(int enforce);

extern void script_console_add_line(long str_id);
extern void script_console_add_error(long str_id);
extern void script_print_screen_statement(int val);
//-------------------------------------------------------------------

extern void script_get_alt_text(char *buf);
extern void script_set_terminate_key(int key, const char *keyname);

extern int script_is_running();
extern long script_stack_start();
extern long script_start_gui( int autostart );
// for kbd_task kbd_process only, check if terminate has been requested by another task
extern void script_check_terminate(void);
// request and wait for script terminate from other task
extern void script_wait_terminate(void);
extern void script_end();
//-------------------------------------------------------------------

#endif
