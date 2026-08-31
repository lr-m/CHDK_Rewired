#include "camera_info.h"
#include "keyboard.h"
#include "modes.h"
#include "sd_card.h"
#include "debug_led.h"
#include "lang.h"
#include "gui.h"
#include "gui_draw.h"
#include "gui_lang.h"
#include "gui_mbox.h"
#include "raw.h"
#include "conf.h"
#include "time.h"
#include "dirent.h"
#include "ctype.h"

#include "gui_fselect.h"
#include "raw_merge.h"
#include "dng.h"
#include "gui_mpopup.h"
#include "gui_tbox.h"
#include "gui_read.h"

#include "module_load.h"

/*
    HISTORY:    1.1 - added tbox usage [CHDK 1.1.1 required]
*/

int gui_fselect_kbd_process();
void gui_fselect_kbd_process_menu_btn();
void gui_fselect_draw(int enforce_redraw);

gui_handler GUI_MODE_FSELECT_MODULE =
    /*GUI_MODE_FSELECT*/    { GUI_MODE_FSELECT, gui_fselect_draw, gui_fselect_kbd_process, gui_fselect_kbd_process_menu_btn, 0, 0 };

//-------------------------------------------------------------------
#define BODY_LINES              12
#define BODY_FONT_LINES         BODY_LINES * FONT_HEIGHT

#define NAME_SIZE               camera_screen.fselect_name_size  // "FILENAME.123 "  (8.3 filenames)
#define SIZE_SIZE               camera_screen.fselect_size_size  // "1000 b|M|G"
#define TIME_SIZE               camera_screen.fselect_time_size  // "01.01'70 00:00"

#define NAME_FONT_SIZE          NAME_SIZE * FONT_WIDTH
#define EXTE_FONT_SIZE          EXTE_SIZE * FONT_WIDTH
#define SIZE_FONT_SIZE          SIZE_SIZE * FONT_WIDTH
#define TIME_FONT_SIZE          TIME_SIZE * FONT_WIDTH

#define SPACING                 4
#define TAB_DIVIDER             1
#define BORDER                  2
#define SCROLLBAR               4

#define MARKED_OP_NONE          0
#define MARKED_OP_CUT           1
#define MARKED_OP_COPY          2

//-------------------------------------------------------------------
// Module state
static int running = 0;
static gui_handler *gui_fselect_mode_old; // stored previous gui_mode

#define MAX_PATH_LEN    100

// Current selection, etc
static char selected_file[MAX_PATH_LEN];    // This full path to current file. So it is return value
static char buf[MAX_PATH_LEN];

// basic element of file list (struct fields ordered to minimize size)
typedef struct _fitem
{
    struct _fitem   *prev, *next;   // Linked list of items in directory
    unsigned long   size;
    unsigned long   mtime;
    unsigned short  n;
    unsigned char   marked;
    unsigned char   isdir;      // directory?
    unsigned char   isparent;   // '..'
    unsigned char   isvalid;    // stat() call succeeded on item
    char            name[1];    // this structure must dynamically allocated !!!
                                // when allocating add the required length of the name (malloc(sizeof(fitem) + strlen(name))
                                // only one buffer then needs to be allocated and released
} fitem;

// List container
typedef struct
{
    fitem           *head;
    fitem           *tail;
    unsigned short  count;
    char            dir[MAX_PATH_LEN];  // Current directory for list
} flist;

static flist    items;          // head of list<fitem>:  holder of current directory list
static flist    marked_items;   // head of list<fitem>:  holder of selected files list (keep independent filelist). made by Cut/Copy

static fitem    *top;           // ptr to first displayed file (top on the screen)
static fitem    *selected;      // ptr to current file (file on cursor)

static char marked_operation;   // info for paste: MARKED_OP_NONE, MARKED_OP_CUT, MARKED_OP_COPY

#define MAIN_H  (FONT_HEIGHT + TAB_DIVIDER + BODY_FONT_LINES + TAB_DIVIDER + FONT_HEIGHT)   // Constant

static coord main_x, main_y, main_w; //main browser window coord (without BORDERs)
static coord body_y; //main body window coord
static coord foot_y; //footer window coord

static int gui_fselect_redraw;  // flag request fselect redraw: 0-no, 1-only filelist, 2-whole_redraw(incl.border)
static int gui_fselect_readdir; // flag to force re-read of current directory
static char *fselect_title;     // Title of fselect window (could be different: Choose Text, Choose Script, etc)

static void (*fselect_on_select)(const char *fn);
static char raw_operation;      // info for process_raw_files() RAW_OPERATION_AVERAGE, RAW_OPERATION_SUM,

// FSelector POPUP
#define MPOPUP_CUT              0x0001
#define MPOPUP_COPY             0x0002
#define MPOPUP_PASTE            0x0004
#define MPOPUP_DELETE           0x0008
#define MPOPUP_SELINV           0x0010
#define MPOPUP_RAWOPS           0x0020
#define MPOPUP_PURGE_DCIM       0x0040
#define MPOPUP_PURGE_DIR        0x0080
#define MPOPUP_PURGE_FILE       0x0100
#define MPOPUP_RMDIR            0x0200
#define MPOPUP_MKDIR            0x0400
#define MPOPUP_RENAME           0x0800
#define MPOPUP_EDITOR           0x1000
#define MPOPUP_CHDK_REPLACE     0x2000

static struct mpopup_item popup[]= {
        { MPOPUP_CUT,           LANG_POPUP_CUT    },
        { MPOPUP_COPY,          LANG_POPUP_COPY   },
        { MPOPUP_PASTE,         LANG_POPUP_PASTE  },
        { MPOPUP_DELETE,        LANG_POPUP_DELETE },
        { MPOPUP_RMDIR,         LANG_POPUP_RMDIR  },
        { MPOPUP_PURGE_DCIM,    LANG_POPUP_PURGE  },
        { MPOPUP_PURGE_DIR,     LANG_POPUP_PURGE  },
        { MPOPUP_PURGE_FILE,    LANG_POPUP_PURGE  },
        { MPOPUP_MKDIR,         LANG_POPUP_MKDIR  },
        { MPOPUP_RENAME,        LANG_POPUP_RENAME },
        { MPOPUP_SELINV,        LANG_POPUP_SELINV },
        { MPOPUP_EDITOR,        (int)"Edit" },
        { MPOPUP_CHDK_REPLACE,  (int)"Set this CHDK" },
        { MPOPUP_RAWOPS,        (int)"Raw ops ->" },
        { 0,                    0 },
};

#define MPOPUP_RAW_ADD          0x0020
#define MPOPUP_RAW_AVERAGE      0x0040
#define MPOPUP_SUBTRACT         0x0100
#define MPOPUP_RAW_DEVELOP      0x0200
#define MPOPUP_DNG_TO_CRW       0x0400

static struct mpopup_item popup_rawop[]= {
        { MPOPUP_RAW_ADD,       LANG_POPUP_RAW_SUM},
        { MPOPUP_RAW_AVERAGE,   LANG_POPUP_RAW_AVERAGE },
        { MPOPUP_RAW_DEVELOP,   LANG_MENU_RAW_DEVELOP },
        { MPOPUP_SUBTRACT,      LANG_POPUP_SUB_FROM_MARKED  },
        { MPOPUP_DNG_TO_CRW,    (int)"DNG -> CHDK RAW"},
        { 0,                    0 },
};

//-------------------------------------------------------------------
// List helper functions

static void free_list(flist *list)
{
    fitem *ptr = list->head;

    while (ptr)
    {
        fitem *prev = ptr;
        ptr = ptr->next;
        free(prev);
    }

    list->head = list->tail = 0;
    list->count = 0;
}

static void add_item(flist *list, const char *name,
        unsigned long size, unsigned long mtime,
        unsigned char marked, unsigned char isdir, unsigned char isparent, unsigned char isvalid)
{
    fitem *p = malloc(sizeof(fitem) + strlen(name));
    if (p)
    {
        p->n = list->count;
        strcpy(p->name, name);
        p->size = size;
        p->mtime = mtime;
        p->marked = marked;
        p->isdir = isdir;
        p->isparent = isparent;
        p->isvalid = isvalid;

        p->next = 0;
        p->prev = list->tail;
        if (list->tail)
            list->tail->next = p;
        list->tail = p;
        if (list->head == 0)
            list->head = p;

        list->count++;
    }
}

int fselect_sort(const void* v1, const void* v2)
{
    fitem *i1 = *((fitem **)v1);
    fitem *i2 = *((fitem **)v2);

    if (i1->isdir)
    {
        if (i2->isdir)
        {
            if (i1->isparent)
            {
                return -1;
            }
            else if (i2->isparent)
            {
                return 1;
            }
            else
            {
                return strcmp(i1->name, i2->name);
            }
        }
        else
        {
            return -1;
        }
    }
    else
    {
        if (i2->isdir)
        {
            return 1;
        }
        else
        {
            return strcmp(i1->name, i2->name);
        }
    }
}

static void sort_list(flist *list)
{
    if (list->count)
    {
        // sort
        fitem **sbuf = malloc(list->count*sizeof(fitem*));
        if (sbuf)
        {
            fitem *ptr = list->head;
            int i = 0;
            while (ptr)
            {
                sbuf[i++] = ptr;
                ptr = ptr->next;
            }

            extern int fselect_sort_nothumb(const void* v1, const void* v2);
            qsort(sbuf, list->count, sizeof(fitem*), fselect_sort_nothumb);

            list->head = sbuf[0];
            list->tail = sbuf[list->count-1];
            for (i=0; i<list->count-1; i++)
            {
                sbuf[i]->n = i;
                sbuf[i]->next = sbuf[i+1];
                sbuf[i+1]->prev = sbuf[i];
            }
            list->head->prev = 0;
            list->tail->next = 0;
            list->tail->n = list->count - 1;

            free(sbuf);
        }
    }
}

//-------------------------------------------------------------------
// Helper methods to check filenames (case insensitive)

// Match extension
static int chk_ext(const char *ext, const char *tst)
{
    if (ext && (*ext != '.'))    // if passed file name, find extension
        ext = strrchr(ext, '.');
    if (ext)
    {
        ext++;
        if (strlen(ext) == strlen(tst))
        {
            int i;
            for (i=0; i<strlen(tst); i++)
                if (toupper(ext[i]) != toupper(tst[i]))
                    return 0;
            return 1;
        }
    }
    return 0;
}

// Match start of filename
static int chk_prefix(const char *name, const char *prefix)
{
    if (name && (strlen(name) >= strlen(prefix)))
    {
        int i;
        for (i=0; i<strlen(prefix); i++)
            if (toupper(prefix[i]) != toupper(name[i]))
                return 0;
        return 1;
    }
    return 0;
}

// Match entire name
static int chk_name(const char *name, const char *tst)
{
    if (name && (strlen(name) == strlen(tst)))
    {
        int i;
        for (i=0; i<strlen(tst); i++)
            if (toupper(tst[i]) != toupper(name[i]))
                return 0;
        return 1;
    }
    return 0;
}

// Check for current or parent directory
static int is_parent(const char *name)  { return (strcmp(name, "..") == 0); }
static int is_current(const char *name)  { return (strcmp(name, ".") == 0); }

// Check if file is a RAW image
static int is_raw(const char *name)
{
    return ((chk_prefix(name,"crw_") || (chk_prefix(name,"img_")))) &&
           ((chk_ext(name,"cr2") || (chk_ext(name,"crw") || (chk_ext(name,"dng")))));
}

// Check if file is a JPG
static int is_jpg(const char *name)
{
    return (chk_prefix(name,"img_")) && (chk_ext(name,"jpg"));
}

//-------------------------------------------------------------------
// File / folder helper functions

static void delete_file(const char *path, const char *name)
{
    sprintf(selected_file, "%s/%s", path, name);
    remove(selected_file);
}

static void delete_dir(const char *path)
{
    remove(path);
}

// helper to handle gui lfn option
static DIR * opendir_fselect(const char *path) {
    return opendir_chdk(path,(conf.disable_lfn_parser_ui?OPENDIR_FL_NONE:OPENDIR_FL_CHDK_LFN));
}

// Uncached memory buffer for file copy
// Keep byffer after allocation (until module exits)
static unsigned char    *ubuf = 0;
#define COPY_BUF_SIZE   16384           // Copy buffer size

static int copy_file(const char *src_dir, const char *src_file, const char *dst_dir, const char *dst_file, int overwrite)
{
    int rv = 0;

    // If source and destination are the same abort (and fail) copy operation
    if (chk_name(src_dir, dst_dir) && chk_name(src_file, dst_file))
        return 0;

    // Get source path and open file
    sprintf(selected_file, "%s/%s", src_dir, src_file);
    int fsrc = open(selected_file, O_RDONLY, 0777);

    if (fsrc >= 0)
    {
        // Get destination path, check for overwrite, and open file
        sprintf(selected_file,"%s/%s", dst_dir, dst_file);
        if (!overwrite)
        {
            struct stat st;
            if (stat(selected_file, &st) == 0)
            {
                close(fsrc);
                // destination exists, and overwrite not selected, copy failed
                return 0;
            }
        }
        int fdst = open(selected_file, O_WRONLY|O_CREAT|O_TRUNC, 0777);

        if (fdst >= 0)
        {
            int ss, sd = 0;

            // Allocate buffer if not already allocated
            if (ubuf == 0)
            {
                ubuf = umalloc(COPY_BUF_SIZE);
                if (ubuf == 0)
                {
                    // umalloc failed - ToDo this should display an error popup?
                    close(fdst);
                    close(fsrc);
                    return 0;
                }
            }

            // Copy contents
            do
            {
                ss = read(fsrc, ubuf, COPY_BUF_SIZE);
                if (ss > 0)
                    sd = write(fdst, ubuf, ss);
            } while (ss > 0 && ss == sd);

            if (ss == 0)
                rv = 1;

            close(fdst);

            struct utimbuf t;
            t.actime = t.modtime = selected->mtime;
            utime(selected_file, &t);
        }

        close(fsrc);
    }

    return rv;
}

//-------------------------------------------------------------------
// Structure to hold info about a file or directory
typedef struct
{
    struct dirent   *de;
    unsigned long   size;
    unsigned long   mtime;
    unsigned char   deleted;        // deleted entry?
    unsigned char   isdir;          // directory?
    unsigned char   isparent;       // parent directory (..)?
    unsigned char   iscurrent;      // current directory (.)?
    unsigned char   isvalid;        // stat() call succeeded
    unsigned char   ishidden;       // hidden attribute?
} fs_dirent;

// Custom readdir - populates extra info about the file or directory
// Uses stat() after calling readdir() - we don't currently get this
// extra data from the firmware function.
static int fs_readdir(DIR *d, fs_dirent *de, const char* path)
{
    // DO NOT USE 'selected_file' global var.
    // This function is called from GUI task, 'selected_file' is used by 'KBD' task
    char pbuf[MAX_PATH_LEN];

    de->de = readdir(d);
    de->size = 0;
    de->mtime = 0;
    de->deleted  = 0;
    de->isparent = 0;
    de->iscurrent = 0;
    de->isdir = 0;
    de->isvalid = 0;
    de->ishidden = 0;

    if (de->de)
    {
        if (de->de->d_name[0] == 0xE5)
        {
            de->deleted = 1;
        }
        else
        {
            de->isparent  = is_parent(de->de->d_name);
            de->iscurrent = is_current(de->de->d_name);

            sprintf(pbuf, "%s/%s", path, de->de->d_name);
            struct stat st;
            if (de->isparent || de->iscurrent)
            {
                de->isdir = 1;
                de->isvalid = 1;
            }
            else if (stat(pbuf, &st) == 0)
            {
                de->size  = st.st_size;
                de->mtime = st.st_mtime;
                de->isvalid = 1;
                de->isdir = ((st.st_attrib & DOS_ATTR_DIRECTORY) != 0);
                de->ishidden = ((st.st_attrib & DOS_ATTR_HIDDEN) != 0);
            }
        }

        return 1;
    }

    return 0;
}

// Process contents of named directory
//  - for sub directories, process recursively if required (up to 'nested' levels deep)
//  - for files, call 'file_process' function
// Once file & directory processing done, call 'dir_process' function on input path
static void process_dir(const char *parent, const char *name, int nested, void (*file_process)(const char *path, const char *file), void (*dir_process)(const char *path))
{
    DIR         *d;
    fs_dirent   de;

    // Get full name
    char *path;
    if (name)
    {
        path = malloc(strlen(parent) + strlen(name) + 2);
        sprintf(path, "%s/%s", parent, name);
    }
    else
    {
        path = (char*)parent;
    }

    // Open directory
    d = opendir_fselect(path);

    if (d)
    {
        // Process contents
        while (fs_readdir(d, &de, path))
        {
            if (!de.deleted)
            {
                // Sub directory? Process recursively (but only 'nested' level deep)
                if (de.isdir)
                {
                    if (!de.isparent && !de.iscurrent && nested)
                        process_dir(path, de.de->d_name, nested-1, file_process, dir_process);
                }
                else if (file_process)
                {
                    file_process(path, de.de->d_name);
                }
            }
        }
        closedir(d);

        if (dir_process)
            dir_process(path);
    }

    if (name)
        free(path);
}

//-------------------------------------------------------------------
static void fselect_goto_prev(int step)
{
    int j;

    for (j=0; j<step; ++j)
    {
        if (selected->prev)
        {
            selected = selected->prev;      // previous line
            if (selected == top && top->prev)
                top = top->prev;
        }
        else if (step == 1)
        {
            // off top - jump to bottom
            selected = top = items.tail;
            while (((selected->n - top->n) < (BODY_LINES - 1)) && top->prev)
                top = top->prev;
        }
    }
}

//-------------------------------------------------------------------
static void fselect_goto_next(int step)
{
    int j;
    for (j=0; j<step; ++j)
    {
        if (selected->next)
        {
            selected = selected->next;      // next line
            if (((selected->n - top->n) == (BODY_LINES - 1)) && selected->next)
                top = top->next;
        }
        else if (step == 1)
        {
            selected = top = items.head;    // off bottom - jump to top
        }
    }
}

//-------------------------------------------------------------------
static void gui_fselect_free_data()
{
    free_list(&items);
    top = selected = NULL;
}

//-------------------------------------------------------------------
static void gui_fselect_read_dir()
{
    DIR         *d;
    fs_dirent   de;
    int         fndParent = 0;  // Set if parent ".." directory returned by fs_readdir (does not exist on exFat paritions)

    gui_fselect_free_data();

    if ((items.dir[0] == 'A') && (items.dir[1] == 0))
        d = opendir_fselect("A/");
    else
        d = opendir_fselect(items.dir);

    if (d)
    {
        while (fs_readdir(d, &de, items.dir))
        {
            if (!de.deleted && !de.iscurrent && (conf.show_hiddenfiles || !de.ishidden))
            {
                add_item(&items, de.de->d_name, de.size, de.mtime, 0, de.isdir, de.isparent, de.isvalid);
                if (de.isparent)
                    fndParent = 1;
            }
        }
        closedir(d);
    }

    // If not reading root directory, and ".." not found, then add it (for exFat partitions)
    if ((strlen(items.dir) > 2) && !fndParent)
    {
        add_item(&items, "..", 0, 0, 0, 1, 1, 1);
    }

    sort_list(&items);

    top = selected = items.head;
}

//-------------------------------------------------------------------
// Attempt to set startup directory (and file) based on input 'dir'
// Note: 'dir' may be a directory name or a file name (including path)
// Returns 1 if valid directory/file found, 0 otherwise
int gui_fselect_find_start_dir(const char* dir)
{
    selected_file[0] = 0;
    strcpy(items.dir, dir);

    // Make sure there is something left to check
    while (strlen(items.dir) > 0)
    {
        // Find start of filename part (if present)
        char *p = strrchr(items.dir,'/');

        struct stat st;
        // check if input 'dir' exists
        if (stat(items.dir,&st) == 0)
        {
            // exists - check if it is a directory or file
            if ((st.st_attrib & DOS_ATTR_DIRECTORY) == 0)
            {
                // 'dir' is a file, copy filename to 'selected_file' and remove from 'items.dir'
                strcpy(selected_file, p+1);
                *p = 0;
            }
            return 1;
        }
        else
        {
            // could not find 'dir' - try one level up
            if (p)
                *p = 0;
            else
                return 0;
        }
    }

    return 0;
}

//-------------------------------------------------------------------
void gui_fselect_init(int title, const char* prev_dir, const char* default_dir, void (*on_select)(const char *fn))
{
    running = 1;

    main_w = SPACING + NAME_FONT_SIZE + FONT_WIDTH + SIZE_FONT_SIZE + FONT_WIDTH + TIME_FONT_SIZE + SPACING + SCROLLBAR;
    main_x = (camera_screen.width  - main_w) >> 1;
    main_y = (camera_screen.height - MAIN_H) >> 1;

    body_y = main_y + FONT_HEIGHT + TAB_DIVIDER;
    foot_y = body_y + BODY_FONT_LINES + TAB_DIVIDER;

    fselect_title = lang_str(title);

    // Try and set start directory, and optionally selected file, from inputs
    if (!gui_fselect_find_start_dir(prev_dir))
        if (!gui_fselect_find_start_dir(default_dir))
            gui_fselect_find_start_dir("A");

    gui_fselect_read_dir();

    // Find selected file if it exists in list
    if (selected_file[0])
    {
        fitem *p = items.head;
        while (p)
        {
            if (chk_name(p->name,selected_file))
            {
                break;
            }
            p = p->next;
            fselect_goto_next(1);
        }
        if (!p) selected_file[0] = 0;
    }

    fselect_on_select = on_select;
    marked_operation = MARKED_OP_NONE;
    gui_fselect_redraw = 2;
    gui_fselect_readdir = 0;
    gui_fselect_mode_old = gui_set_mode(&GUI_MODE_FSELECT_MODULE);
}

//-------------------------------------------------------------------
void gui_fselect_draw(int enforce_redraw)
{
    int i, j;
    twoColors cl_marked;

    if (gui_fselect_readdir)
    {
        gui_fselect_readdir = 0;
        gui_fselect_read_dir();
    }

    if (enforce_redraw)
        gui_fselect_redraw = 2;

    if (gui_fselect_redraw)
    {
        char dbuf[46];

        if (gui_fselect_redraw == 2)
        {
            // Title
            draw_string_justified(main_x, main_y, fselect_title, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE), 0, main_w, TEXT_CENTER|TEXT_FILL);

            draw_rectangle(main_x-BORDER, main_y-BORDER, main_x+main_w+BORDER-1, main_y+MAIN_H+BORDER-1, MAKE_COLOR(COLOR_WHITE, COLOR_WHITE), RECT_BORDER2); //border frame
            draw_line(main_x, body_y-1, main_x+main_w-1, body_y-1, COLOR_WHITE); //border head-body
            draw_line(main_x, foot_y-1, main_x+main_w-1, foot_y-1, COLOR_WHITE); //border body-foot
        }

        int off_body_y = body_y;

        fitem *ptr;
        unsigned long sum_size = 0;
        for (i=0, ptr=top; i<BODY_LINES && ptr; ++i, ptr=ptr->next, off_body_y += FONT_HEIGHT)
        {
            cl_marked = MAKE_COLOR((ptr==selected)?COLOR_RED:COLOR_GREY, (ptr->marked)?COLOR_YELLOW:COLOR_WHITE);

            // print name
            j = strlen(ptr->name);
            strncpy(dbuf, ptr->name, NAME_SIZE);
            if (j > NAME_SIZE)
                dbuf[NAME_SIZE-1] = '~'; // too long name

            if (ptr->isdir && ptr->isvalid)
            {
                if (j < NAME_SIZE)
                {
                    dbuf[j++] = '/';
                }
                else
                {
                    dbuf[NAME_SIZE-2] = '~';
                    dbuf[NAME_SIZE-1] = '/';
                }
            }
            for (; j < NAME_SIZE; j++)
                dbuf[j] = ' ';
            j = NAME_SIZE;
            dbuf[j++] = 0x06;   // Vertical line

            // print size or <Dir>
            if (ptr->isdir)
            {
                if (!ptr->isvalid)
                {
                    sprintf(dbuf+j, "  ??? ");
                }
                else if (ptr->isparent)
                {
                    sprintf(dbuf+j, " <Up> ");
                }
                else
                {
                    sprintf(dbuf+j, " <Dir>");
                }
            }
            else
            {
                unsigned long n = ptr->size;

                if (ptr->marked)
                  sum_size += n;

                if (n < 1024)
                    sprintf(dbuf+j, "%5db", n); // " 1023 b"
                else
                {
                    static char* suffixes = "kMG";
                    int sfx = 0;
                    if (n >= 4294967245ul)    // 4GB - 51 - avoid overflow
                    {
                        sfx = 2;    // 'G' suffix
                        n = 4096;   // 4G
                    }
                    else
                    {
                        // Round to 1 decimal place (51 = 1024 * 0.05)
                        n += 51;
                        // Reduce and round until < 1M, incrementing size suffix index
                        while (n >= 1024*1024)
                        {
                            n >>= 10;
                            n += 51;
                            sfx += 1;
                        }
                    }
                    unsigned long f = ((n & 0x3FF) * 10) >> 10;    // 1 digit of remainder % 1024
                    sprintf(dbuf+j, "%3d.%1d%c", n >> 10, f, suffixes[sfx]);
                }
            }
            j += SIZE_SIZE;
            dbuf[j++] = 0x06;   // Vertical line

            // print modification time
            if (ptr->mtime)
            {
                struct tm *time = localtime(&(ptr->mtime));
                sprintf(dbuf+j, "%02u.%02u'%02u %02u:%02u", time->tm_mday, time->tm_mon+1, (time->tm_year<100)?time->tm_year:time->tm_year-100, time->tm_hour, time->tm_min);
            }
            else
            {
                sprintf(dbuf+j, "%14s", "");
            }
            j += TIME_SIZE;
            dbuf[j] = 0;

            draw_string_justified(main_x, off_body_y, dbuf, cl_marked, SPACING, main_w-SCROLLBAR, TEXT_LEFT|TEXT_FILL);
        }

        //fill the rest of body
        if (i>0 && i<BODY_LINES)
        {
            draw_rectangle(main_x, off_body_y, main_x+main_w-SCROLLBAR-1, body_y+BODY_FONT_LINES-1, MAKE_COLOR(COLOR_GREY, COLOR_GREY), RECT_BORDER0|DRAW_FILLED);
        }

        // scrollbar
        int off_sbar_x = main_x + main_w - SCROLLBAR;
        draw_rectangle(off_sbar_x, body_y, off_sbar_x+SCROLLBAR-1, body_y+BODY_FONT_LINES-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
        if (items.count > BODY_LINES)
        {
            i = BODY_FONT_LINES - 1;
            j = (i * BODY_LINES) / items.count;
            if (j < 20) j = 20;
            i = ((i - j) * selected->n) / (items.count-1);
            draw_rectangle(off_sbar_x, body_y+i, off_sbar_x+SCROLLBAR-2, body_y+i+j, MAKE_COLOR(COLOR_WHITE, COLOR_WHITE), RECT_BORDER0|DRAW_FILLED);
        }

        //footer
        int max_footer_len = NAME_SIZE + SIZE_SIZE + SPACING;
        i = strlen(items.dir);
        if (i > max_footer_len)
        {
            strncpy(dbuf, items.dir+i-max_footer_len, max_footer_len);
            dbuf[0] = '.';
            dbuf[1] = '.';
        }
        else
        {
            strcpy(dbuf, items.dir);
        }
        draw_string_justified(main_x, foot_y, dbuf, MAKE_COLOR(COLOR_GREY, COLOR_WHITE), SPACING, main_w, TEXT_LEFT|TEXT_FILL);

        if (sum_size)
        {
            sprintf(dbuf, "%d b", sum_size); //selected size
        }
        else
        {
            unsigned int fr  = GetFreeCardSpaceKb();
            unsigned int tot = GetTotalCardSpaceKb();
            if (tot != 0)
                tot = (fr * 100) / tot;

            if (fr < 1024*1024)
                sprintf(dbuf, "%dM (%d%%)",    fr>>10, tot);
            else
                sprintf(dbuf, "%d.%dG (%d%%)", fr>>20, ((fr&0x000FFFFF)*100)>>20, tot);
        }
        draw_string(main_x+main_w-strlen(dbuf)*FONT_WIDTH-BORDER, foot_y, dbuf, MAKE_COLOR(COLOR_GREY, COLOR_WHITE)); // free space

        gui_fselect_redraw = 0;
    }
}

//-------------------------------------------------------------------
static void fselect_delete_file_cb(unsigned int btn)
{
    if (btn==MBOX_BTN_YES)
    {
        started();
        delete_file(items.dir, selected->name);
        finished();
        gui_fselect_readdir = 1;
    }
    gui_fselect_redraw = 2;
}

// Find a JPG matching a given RAW name. If 'nested' > 0 search recursively in
// child directories of 'folder.
// Returns 1 if found, 0 if not found (or match is not a RAW file)
static int find_jpg(const char *folder, const char *match, int nested)
{
    DIR         *d;
    fs_dirent   de;
    int         rv = 0;

    // Open directory
    d = opendir_fselect(folder);

    if (d)
    {
        // Process contents
        while (fs_readdir(d, &de, folder) && !rv)
        {
            if (!de.deleted)
            {
                // Sub directory? Process recursively (but only 'nested' levels deep)
                if (de.isdir)
                {
                    if (!de.isparent && !de.iscurrent && nested)
                    {
                        // Search sub-directory
                        char *path = malloc(strlen(folder) + strlen(de.de->d_name) + 2);
                        sprintf(path, "%s/%s", folder, de.de->d_name);
                        if (find_jpg(path, match, nested-1))
                            rv = 1;
                        free(path);
                    }
                }
                else
                {
                    //If the four digits of the Canon number are the same AND file is JPG
                    if (is_jpg(de.de->d_name) && (strncmp(match+4, de.de->d_name+4, 4) == 0))
                        rv = 1;
                }
            }
        }
        closedir(d);
    }

    return rv;
}

// If 'file' is a RAW file, scan its 'folder' for a JPG with the same
// image number, if not found delete the RAW image file.
static void purge_file(const char *folder, const char *file)
{
    //If no JPG found, delete RAW file
    if (is_raw(file))
        if (!find_jpg(folder, file, 0))
            delete_file(folder, file);
}

// If 'file' is a RAW file, scan its 'folder' and all sibling folders for a JPG with the same
// image number, if not found delete the RAW image file.
// Used when 'Purge RAW' run on the A/DCIM directory. CHDK can store RAW files in a different
// sub-directory of A/DCIM than the corresponding JPG.
static void purge_file_DCIM(const char *folder, const char *file)
{
    //If no JPG found, delete RAW file (search all sub-folders of A/DCIM for JPG)
    if (is_raw(file))
        if (!find_jpg("A/DCIM", file, 1))
            delete_file(folder, file);
}

static void fselect_purge_cb_DCIM(unsigned int btn)
{
    if (btn == MBOX_BTN_YES)
    {
        //If selected folder is A/DCIM or A/RAW (this is to purge all RAW files in any sub-folder)
        process_dir(items.dir, selected->name, 1, purge_file_DCIM, 0);
    }
}

static void fselect_purge_cb_dir(unsigned int btn)
{
    if (btn == MBOX_BTN_YES)
    {
        //If item is a Canon sub-folder of A/DCIM or A/RAW (this is to purge all RAW files inside a single Canon folder)
        process_dir(items.dir, selected->name, 0, purge_file, 0);
    }
}

static void fselect_purge_cb_file(unsigned int btn)
{
    if (btn == MBOX_BTN_YES)
    {
        //Inside a Canon folder (files list)
        fitem *ptr, *ptr2;

        //Loop to find all the RAW files in the list
        for (ptr=items.head; ptr; ptr=ptr->next)
        {
            //If file is RAW (Either CRW/CR2 prefix or file extension) and is not marked
            if (is_raw(ptr->name) && !ptr->marked)
            {
                // Flag for checking if matching JPG exists
                int found = 0;

                //Loop to find a corresponding JPG file in the list
                for (ptr2=items.head; ptr2; ptr2=ptr2->next)
                {
                    //If this is a JPG and the four digits of the Canon number are the same
                    if (is_jpg(ptr2->name) && (strncmp(ptr->name+4, ptr2->name+4, 4) == 0))
                    {
                        found=1;
                        break;
                    }
                }

                //If no JPG found, delete RAW file
                if (found == 0)
                    delete_file(items.dir, ptr->name);
            }
        }
        gui_fselect_readdir = 1;
    }
    gui_fselect_redraw = 2;
}


//-------------------------------------------------------------------
static void fselect_delete_folder_cb(unsigned int btn)
{
    if (btn==MBOX_BTN_YES)
    {
        process_dir(items.dir, selected->name, 999, delete_file, delete_dir);
        gui_fselect_readdir = 1;
    }
    gui_fselect_redraw = 2;
}

static void confirm_delete_directory()
{
    if (selected->isdir && !selected->isparent)
        gui_mbox_init(LANG_BROWSER_ERASE_DIR_TITLE, LANG_BROWSER_ERASE_DIR_TEXT,
                      MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_delete_folder_cb);
}

//-------------------------------------------------------------------
static void fselect_marked_toggle()
{
    if (selected && selected->isvalid && !selected->isdir)
    {
        selected->marked = !selected->marked;
    }
}

//-------------------------------------------------------------------
static void gui_fselect_marked_free_data()
{
    free_list(&marked_items);
    marked_operation = MARKED_OP_NONE;
}

//-------------------------------------------------------------------
static void fselect_marked_copy_list()
{
    gui_fselect_marked_free_data();

    fitem *ptr;

    for (ptr=items.head; ptr; ptr=ptr->next)
        if (ptr->marked)
            add_item(&marked_items, ptr->name, ptr->size, ptr->mtime, 1, ptr->isdir, ptr->isparent, ptr->isvalid);

    if (!marked_items.count)
        if (selected && selected->isvalid && !selected->isdir)
            add_item(&marked_items, selected->name, selected->size, selected->mtime, 1, selected->isdir, selected->isparent, selected->isvalid);

    strcpy(marked_items.dir, items.dir);
}

//-------------------------------------------------------------------
static void fselect_marked_paste_cb(unsigned int btn)
{
    fitem *ptr;
    int i = 0;

    if (btn != MBOX_BTN_YES) return;

    if (strcmp(marked_items.dir, items.dir) != 0)
    {
        for (ptr=marked_items.head; ptr; ptr=ptr->next)
        {
            if (ptr->isvalid && !ptr->isdir)
            {
                started();

                ++i;
                if (marked_items.count)
                    draw_progress_bar(lang_str(LANG_FSELECT_PROGRESS_TITLE),i*100/marked_items.count);

                int copied = copy_file(marked_items.dir, ptr->name, items.dir, ptr->name, 0);

                if (copied && (marked_operation == MARKED_OP_CUT))
                {
                    delete_file(marked_items.dir, ptr->name);
                }

                finished();
            }
        }
        if (marked_operation == MARKED_OP_CUT)
        {
            gui_fselect_marked_free_data();
        }
        gui_fselect_readdir = 1;
    }
    gui_fselect_redraw = 2;
}

//-------------------------------------------------------------------
static inline unsigned int fselect_real_marked_count()
{
    fitem  *ptr;
    register unsigned int cnt = 0;

    for (ptr=items.head; ptr; ptr=ptr->next)
    {
        if (ptr->isvalid && !ptr->isdir && ptr->marked)
            ++cnt;
    }
    return cnt;
}

//-------------------------------------------------------------------
static unsigned int fselect_marked_count()
{
    register unsigned int cnt = fselect_real_marked_count();

    if (!cnt)
    {
        if (selected && selected->isvalid && !selected->isdir)
            ++cnt;
    }

    return cnt;
}

//-------------------------------------------------------------------
static void fselect_marked_delete_cb(unsigned int btn)
{
    fitem  *ptr;
    unsigned int del_cnt=0, cnt;

    if (btn != MBOX_BTN_YES) return;

    cnt = fselect_marked_count();
    for (ptr=items.head; ptr; ptr=ptr->next)
        if (ptr->marked && ptr->isvalid && !ptr->isdir)
        {
            started();
            ++del_cnt;
            if (cnt)
                draw_progress_bar(lang_str(LANG_FSELECT_PROGRESS_TITLE),del_cnt*100/cnt);
            delete_file(items.dir, ptr->name);
            finished();
        }

    if (del_cnt == 0 && selected)
    {
        started();
        delete_file(items.dir, selected->name);
        finished();
    }
    gui_fselect_readdir = 1;
    gui_fselect_redraw = 2;
}

//-------------------------------------------------------------------
static void fselect_chdk_replace_cb(unsigned int btn)
{
    if (btn == MBOX_BTN_YES)
    {
        copy_file(items.dir, selected->name, "A", "DISKBOOT.BIN", 1);
        draw_progress_bar("Please reboot",100);
    }
}

//-------------------------------------------------------------------
static void fselect_marked_inverse_selection()
{
    fitem  *ptr;

    for (ptr=items.head; ptr; ptr=ptr->next)
        if (ptr->isvalid && !ptr->isdir)
            ptr->marked = !ptr->marked;

    gui_fselect_redraw = 2;
}

//-------------------------------------------------------------------
void process_raw_files(void)
{
    fitem *ptr;

    if ((fselect_marked_count()>1) && librawop->raw_merge_start(raw_operation))
    {
        for (ptr=items.head; ptr; ptr=ptr->next)
            if (ptr->marked && ptr->isvalid && !ptr->isdir)
            {
                sprintf(selected_file, "%s/%s", items.dir, ptr->name);
                librawop->raw_merge_add_file(selected_file);
            }
        librawop->raw_merge_end();
        gui_fselect_readdir = 1;
        gui_fselect_redraw = 2;
    }
}

static void fselect_subtract_cb(unsigned int btn)
{
    fitem *ptr;
    if (btn != MBOX_BTN_YES) return;

    for (ptr=items.head; ptr; ptr=ptr->next)
    {
        if (ptr->marked && ptr->isvalid && !ptr->isdir && chk_name(ptr->name,selected->name))
        {
            librawop->raw_subtract(ptr->name, items.dir, selected->name, items.dir);
        }
    }
    gui_fselect_readdir = 1;
    gui_fselect_redraw = 2;
}

#define MAX_SUB_NAMES 6
static void setup_batch_subtract(void)
{
    fitem *ptr;
    int i;
    char *p = buf + sprintf(buf,"%s %s\n",selected->name,lang_str(LANG_FSELECT_SUB_FROM));
    for (ptr=items.head, i=0; ptr; ptr=ptr->next)
    {
        if (ptr->marked && ptr->isvalid && !ptr->isdir && (ptr->size >= camera_sensor.raw_size))
        {
            if ( i < MAX_SUB_NAMES )
            {
                sprintf(p, "%s\n",ptr->name);
                // keep a pointer to the one before the end, so we can stick ...and more on
                if (i < MAX_SUB_NAMES - 1)
                {
                    p += strlen(p);
                }
            }
            i++;
        }
    }
    if (i > MAX_SUB_NAMES)
    {
//      "...%d more files"
        sprintf(p,lang_str(LANG_FSELECT_SUB_AND_MORE),i - (MAX_SUB_NAMES - 1));
    }
    gui_mbox_init(LANG_FSELECT_SUBTRACT, (int)buf,
                  MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_subtract_cb);
}
//-------------------------------------------------------------------
void process_dng_to_raw_files(void)
{
    fitem *ptr;
    int i=0;
    started();
    msleep(100);
    finished();

    if (fselect_real_marked_count())
    {
        for (ptr=items.head; ptr; ptr=ptr->next)
            if (ptr->marked && ptr->isvalid && !ptr->isdir)
            {
                sprintf(selected_file, "%s/%s", items.dir, ptr->name);
                draw_progress_bar(selected_file, (i++)*100/fselect_real_marked_count()) ;
                libdng->convert_dng_to_chdk_raw(selected_file);
            }
    }
    else
    {
        sprintf(selected_file, "%s/%s", items.dir, selected->name);
        libdng->convert_dng_to_chdk_raw(selected_file);
    }
    gui_fselect_readdir = 1;
}

static void fselect_mpopup_rawop_cb(unsigned int actn)
{
    switch (actn) {
        case MPOPUP_RAW_AVERAGE:
            raw_operation=RAW_OPERATION_AVERAGE;
            process_raw_files();
            break;
        case MPOPUP_RAW_ADD:
            raw_operation=RAW_OPERATION_SUM;
            process_raw_files();
            break;
        case MPOPUP_RAW_DEVELOP:
            sprintf(buf, "%s/%s", items.dir, selected->name);
            raw_prepare_develop(buf, 1);
            break;
        case MPOPUP_SUBTRACT:
            setup_batch_subtract();
            break;
        case MPOPUP_DNG_TO_CRW:
            process_dng_to_raw_files();
            break;
    }
}

static void mkdir_cb(const char* name)
{
    if (name)
    {
        sprintf(selected_file,"%s/%s", items.dir, name);
        mkdir(selected_file);
        gui_fselect_readdir = 1;
        gui_fselect_redraw = 2;
    }
}

static void rename_cb(const char* name)
{
    if (name)
    {
        sprintf(selected_file, "%s/%s", items.dir, selected->name);
        sprintf(buf, "%s/%s", items.dir, name);
        rename(selected_file, buf);
        gui_fselect_readdir = 1;
        gui_fselect_redraw = 2;
    }
}

static int mpopup_rawop_flag;

//-------------------------------------------------------------------
// Return 1 if A/DCIM or A/RAW selected, 0 otherwise
static int isPurgeDCIM()
{
    return (chk_name(items.dir, "A") && (chk_name(selected->name, "DCIM") || chk_name(selected->name, "RAW")));
}

// Return 1 if sub-directory of A/DCIM or A/RAW selected
static int isPurgeDir()
{
    return (selected->isdir && !selected->isparent && ((chk_name(items.dir, "A/DCIM")) || (chk_name(items.dir, "A/RAW"))));
}

static void fselect_mpopup_cb(unsigned int actn)
{
    switch (actn)
    {
        case MPOPUP_CUT:
            fselect_marked_copy_list();
            marked_operation = MARKED_OP_CUT;
            break;
        case MPOPUP_COPY:
            fselect_marked_copy_list();
            marked_operation = MARKED_OP_COPY;
            break;
        case MPOPUP_PASTE:
            sprintf(buf, lang_str((marked_operation == MARKED_OP_CUT)?LANG_FSELECT_CUT_TEXT:LANG_FSELECT_COPY_TEXT), marked_items.count, marked_items.dir);
            gui_mbox_init((marked_operation == MARKED_OP_CUT)?LANG_FSELECT_CUT_TITLE:LANG_FSELECT_COPY_TITLE,
                          (int)buf, MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_marked_paste_cb);
            break;
        case MPOPUP_DELETE:
            sprintf(buf, lang_str(LANG_FSELECT_DELETE_TEXT), fselect_marked_count());
            gui_mbox_init(LANG_FSELECT_DELETE_TITLE, (int)buf,
                          MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_marked_delete_cb);
            break;
        case MPOPUP_RMDIR:
            confirm_delete_directory();
            break;
        case MPOPUP_MKDIR:
            libtextbox->textbox_init(LANG_POPUP_MKDIR, LANG_PROMPT_MKDIR, "", 15, mkdir_cb, 0);
            break;
        case MPOPUP_RENAME:
            libtextbox->textbox_init(LANG_POPUP_RENAME, LANG_PROMPT_RENAME, selected->name, 15, rename_cb, 0);
            break;
        case MPOPUP_PURGE_DCIM:
            //If selected folder is A/DCIM or A/RAW (this is to purge all RAW files in any sub-folder)
            gui_mbox_init(LANG_FSELECT_PURGE_TITLE, LANG_FSELECT_PURGE_DCIM_TEXT, MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_purge_cb_DCIM);
            break;
        case MPOPUP_PURGE_DIR:
            //If selected item is a Canon folder
            gui_mbox_init(LANG_FSELECT_PURGE_TITLE, LANG_FSELECT_PURGE_CANON_FOLDER_TEXT, MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_purge_cb_dir);
            break;
        case MPOPUP_PURGE_FILE:
            //If selected item is a file produced by the camera
            gui_mbox_init(LANG_FSELECT_PURGE_TITLE, LANG_FSELECT_PURGE_LIST_TEXT, MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_purge_cb_file);
            break;
        case MPOPUP_SELINV:
            fselect_marked_inverse_selection();
            break;
        case MPOPUP_CANCEL:
            break;

        case MPOPUP_RAWOPS:
            libmpopup->show_popup( popup_rawop, mpopup_rawop_flag, fselect_mpopup_rawop_cb);
            break;

        case MPOPUP_CHDK_REPLACE:
            gui_mbox_init((int)"Replacing CHDK", (int)"Do you want to replace current CHDK with this file",
                          MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_chdk_replace_cb);
            break;
        case MPOPUP_EDITOR:
            gui_mbox_init((int)"Editor", (int)"edit", MBOX_BTN_OK|MBOX_TEXT_CENTER, NULL);
            break;
    }
    gui_fselect_redraw = 2;
}

//-------------------------------------------------------------------
void finalize_fselect()
{
    gui_fselect_free_data();
    gui_fselect_marked_free_data();
}

static void exit_fselect(char* file)
{
    finalize_fselect();

    gui_set_mode(gui_fselect_mode_old);

    // call callback if set. file will be null if canceled (menu or leaving alt)
    if (fselect_on_select)
    {
        fselect_on_select(file);
        // if called mode will return control to filemanager - we need to redraw it
        gui_fselect_redraw = 2;
    }

    running = 0;
}

//-------------------------------------------------------------------
int gui_fselect_kbd_process()
{
    int i;
    int do_exit = 0;

    switch (kbd_get_autoclicked_key() | get_jogdial_direction())
    {
        case JOGDIAL_LEFT:
        case KEY_UP:
            if (selected)
            {
                if (camera_info.state.is_shutter_half_press) fselect_goto_prev(4);
                else fselect_goto_prev(1);
                gui_fselect_redraw = 1;
            }
            break;
        case KEY_DOWN:
        case JOGDIAL_RIGHT:
            if (selected)
            {
                if (camera_info.state.is_shutter_half_press) fselect_goto_next(4);
                else fselect_goto_next(1);
                gui_fselect_redraw = 1;
            }
            break;
        case KEY_ZOOM_OUT:
            if (selected)
            {
                fselect_goto_prev(BODY_LINES-1);
                gui_fselect_redraw = 1;
            }
            break;
        case KEY_ZOOM_IN:
            if (selected)
            {
                fselect_goto_next(BODY_LINES-1);
                gui_fselect_redraw = 1;
            }
            break;
        case KEY_RIGHT:
            if (selected)
            {
                fselect_marked_toggle();
                fselect_goto_next(1);
                gui_fselect_redraw = 1;
            }
            break;
        case KEY_LEFT:
            if (selected && selected->isvalid)
            {
                int marked_count = fselect_marked_count();

                i = MPOPUP_SELINV|MPOPUP_MKDIR;
                mpopup_rawop_flag = 0;

                if (marked_count > 0)
                {
                    i |= MPOPUP_CUT|MPOPUP_COPY|MPOPUP_DELETE;
                    if (marked_count > 1)
                        mpopup_rawop_flag |= MPOPUP_RAW_ADD|MPOPUP_RAW_AVERAGE;
                    // doesn't make sense to subtract from itself!
                    if (selected->marked == 0 && fselect_real_marked_count() > 0)
                        mpopup_rawop_flag |= MPOPUP_SUBTRACT;
                }

                if (marked_operation != MARKED_OP_NONE)
                    i |= MPOPUP_PASTE;

                //Check if 'Purge RAW' applies
                if (isPurgeDCIM())              // DCIM or RAW selected in A
                    i |= MPOPUP_PURGE_DCIM;
                if (isPurgeDir())               // sub-dir selected in A/DCIM or A/RAW
                    i |= MPOPUP_PURGE_DIR;
                if (is_raw(selected->name))     // raw file selected
                    i |= MPOPUP_PURGE_FILE;

                if (selected->isdir && !selected->isparent)
                    i |= MPOPUP_RMDIR;

                if (!selected->isparent) //If item is not UpDir
                    i |= MPOPUP_RENAME;

                if (selected->size >= camera_sensor.raw_size)
                    mpopup_rawop_flag |= MPOPUP_RAW_DEVELOP;

                if ((marked_count > 1) || (selected->size > camera_sensor.raw_size))
                    mpopup_rawop_flag |= MPOPUP_DNG_TO_CRW;

                if (chk_ext(selected->name, "bin")) //If item is *.bin file
                    i |= MPOPUP_CHDK_REPLACE;

                if (mpopup_rawop_flag)
                    i |= MPOPUP_RAWOPS;

                libmpopup->show_popup( popup, i, fselect_mpopup_cb);
            }
            break;
        case KEY_SET:
            if (selected && selected->isvalid && gui_fselect_redraw==0)
            {
                if (selected->isdir)
                {
                    if (selected->isparent)
                    {
                        char *s = strrchr(items.dir, '/');
                        if (s) *s = 0;
                    }
                    else
                    {
                        sprintf(items.dir+strlen(items.dir), "/%s", selected->name);
                    }
                    gui_fselect_readdir = 1;
                    gui_fselect_redraw = 1;
                }
                else
                {
                    sprintf(selected_file, "%s/%s", items.dir, selected->name);

                    char *ext = strrchr(selected->name,'.');
                    do_exit = 1;

                    if (!fselect_on_select)
                    {
                        if (chk_ext(ext,"txt") || chk_ext(ext,"log") || chk_ext(ext,"csv"))
                        {
                            fselect_on_select = (void (*)(const char*))libtxtread->read_file;
                        }
                        else if (chk_ext(ext,"flt"))
                        {
                            fselect_on_select = (void (*)(const char*))module_run;
                        }
                    }
                }
            }
            break;
        case KEY_ERASE:
        case KEY_DISPLAY:
            if (selected && selected->isvalid)
            {
                if (selected->isdir)
                {
                    confirm_delete_directory();
                } else
                {
                    gui_mbox_init(LANG_BROWSER_DELETE_FILE_TITLE, LANG_BROWSER_DELETE_FILE_TEXT,
                                  MBOX_TEXT_CENTER|MBOX_BTN_YES_NO|MBOX_DEF_BTN2, fselect_delete_file_cb);
                }
            }
            break;
    }

    if (do_exit)
        exit_fselect(selected_file);

    return 0;
}

void gui_fselect_kbd_process_menu_btn()
{
    // just free resource. callback called with NULL ptr
    exit_fselect(0);
}

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
    // Free file copy buffer if allocated
    if (ubuf)
    {
        ufree(ubuf);
        ubuf = 0;
    }
    finalize_fselect();
    return 0;
}

int _module_can_unload()
{
    return running == 0;
}

int _module_exit_alt()
{
    exit_fselect(0);
    return 0;
}

/******************** Module Information structure ******************/

libfselect_sym _libfselect =
{
    {
        0, _module_unloader, _module_can_unload, _module_exit_alt, 0
    },

    gui_fselect_init
};

ModuleInfo _module_info = {
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    GUI_FSELECT_VERSION,        // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,         // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    -LANG_MENU_MISC_FILE_BROWSER,
    MTYPE_EXTENSION,

    &_libfselect.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    CAM_SENSOR_VERSION,         // CAM SENSOR version
    CAM_INFO_VERSION,           // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
