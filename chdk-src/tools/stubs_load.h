
// Functions for loading stubs and other source files, storing and searching for values contained within.

#define TYPE_NHSTUB 1
#define TYPE_DEF    2
#define TYPE_IGNORE 3
#define TYPE_CONST  4

// Structure for storing a value from a stubs (or other) file
typedef struct _osig
{
    char        nm[100];    // Name
    uint32_t    val;        // Value
	char		sval[100];  // String version of value
    int         type;       // Type of reference (NHSTUB, DEF)
    int         is_comment; // Used if loading values in comments as well as normal
    int         pct;        // Used for processing modemap entries
    struct _osig *nxt;      // next list entry
} osig;

// struct for storing an instance of the loaded values for a specific firmware
typedef struct _stubs
{
    osig    *stubs;
    osig    *stubs_min;
    osig    *modemap;
    osig    *makevals;
    osig    *propcases;

    uint32_t min_focus_len;
    uint32_t max_focus_len;
    uint32_t propset;
} stub_values;

stub_values* new_stub_values();

// Functions for accessing saved values
osig* find_sig(osig* p, const char *nm);
osig* find_sig_val(osig* p, uint32_t val);
osig* find_match(osig *p, const char *nm, uint32_t val);
osig* find_sig_val_by_type(osig* p, uint32_t val, int typ);

// Functions for loading files
void load_funcs(stub_values *sv, char *name);
void load_stubs(stub_values *sv, char *name, int exclude_comments);
void load_stubs_min(stub_values *sv);
void load_modemap(stub_values *sv);
void load_platform(stub_values *sv);
void load_propcases(stub_values *sv, char *name);
void load_makefile(stub_values *sv);
