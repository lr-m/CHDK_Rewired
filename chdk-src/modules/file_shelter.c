#include "camera_info.h"
#include "gui.h"
#include "gui_draw.h"
#include "meminfo.h"
#include "module_load.h"
#include "simple_module.h"
#include "clock.h"
#include "console.h"
#include "conf.h"
#include "sd_card.h"
#include "cachebit.h"
#include "exmem.h"
#include "time.h"
#include "dirent.h"
#include "ctype.h"

#define FSCRC_CALC 1 // 1 = file crc check immediately after writing, 0 = no check

#define MAX_PATH_LEN    100

// =========  MODULE INIT =================

static int running = 0;
static char osdbuf[128];

extern int basic_module_init();

/***************** BEGIN OF AUXILARY PART *********************
  ATTENTION: DO NOT REMOVE OR CHANGE SIGNATURES IN THIS SECTION
 **************************************************************/

int _run()
{
    basic_module_init();

    return 0;
}

int _module_can_unload()
{
    return (running==0);
}

int _module_exit_alt()
{
    //running = 0;
    return 0;
}

/******************** Module Information structure ******************/

libsimple_sym _librun =
{
    {
         0, 0, _module_can_unload, _module_exit_alt, _run
    }
};

ModuleInfo _module_info =
{
    MODULEINFO_V1_MAGICNUM,
    sizeof(ModuleInfo),
    {1,0},      // Module version

    ANY_CHDK_BRANCH, 0, OPT_ARCHITECTURE,           // Requirements of CHDK version
    ANY_PLATFORM_ALLOWED,       // Specify platform dependency

    (int32_t)"CHDK backup & restore",
    MTYPE_TOOL,                 // Temporary file shelter for CHDK files

    &_librun.base,

    CONF_VERSION,               // CONF version
    CAM_SCREEN_VERSION,         // CAM SCREEN version
    ANY_VERSION,                // CAM SENSOR version
    ANY_VERSION,                // CAM INFO version

    0,
};

/*************** END OF AUXILARY PART *******************/
//-------------------------------------------------------------------
// From public domain crc32 code
//-------------------------------------------------------------------

unsigned long crc32_for_byte(unsigned long r) {
    unsigned long j;
    for (j = 0; j < 8; j++) {
        r = (r & 1? 0: (unsigned long)0xEDB88320) ^ r >> 1;
    }
    return r ^ (unsigned long)0xFF000000;
}

void crc32(const void *data, unsigned long n_bytes, unsigned long* crc) {
    static unsigned long ct[0x100] = {0};
    unsigned long i;
    if (!ct[0]) {
        for (i = 0; i < 0x100; i++) {
            ct[i] = crc32_for_byte(i);
        }
    }
    for (i = 0; i < n_bytes; i++) {
        *crc = ct[(unsigned char)*crc ^ ((unsigned char*)data)[i]] ^ *crc >> 8;
    }
}

//-------------------------------------------------------------------
// file operation wrappers, allows switching between open/fopen
// if required on a camera
//-------------------------------------------------------------------
static int use_open_rather_than_fopen = 1;

int read_file_f(char *fn, void *buf, int size)
{
    int ret = -1;
    FILE *fd = fopen(fn, "r+b");
    if (fd) {
        ret = fread(buf, 1, size, fd);
        fclose(fd);
        if (ret == size) {
            ret = 0;
        }
        else {
            ret = 1;
        }
    }
    return ret;
}

int read_file_o(char *fn, void *buf, int size)
{
    int ret = -1;
    int fd = open(fn, O_RDONLY, 0777);
    if (fd >= 0) {
        ret = read(fd, buf, size);
        close(fd);
        if (ret == size) {
            ret = 0;
        }
        else {
            ret = 1;
        }
    }
    return ret;
}

int compute_file_crc_f(char *fn, void *buf, int size, int bufsize, unsigned long *crc)
{
    int ret = 0;
    if (!fn || !buf || !crc || !bufsize) {
        return -1;
    }
    FILE *fd = fopen(fn, "r+b");
    if (fd) {
        long chunk = bufsize<size?bufsize:size;
        while (1) {
            long r = fread(buf, 1, chunk, fd);
            if (r != chunk) {
                if (r != size) {
                    ret = 1;
                    break;
                }
            }
            crc32(buf, r, crc);
            size -= r;
            if (size <= 0) {
                break;
            }
        }
        fclose(fd);
    }
    return ret;
}

int compute_file_crc_o(char *fn, void *buf, int size, int bufsize, unsigned long *crc)
{
    int ret = 0;
    if (!fn || !buf || !crc || !bufsize) {
        return -1;
    }
    int fd = open(fn, O_RDONLY, 0777);
    if (fd >= 0) {
        long chunk = bufsize<size?bufsize:size;
        while (1) {
            long r = read(fd, buf, chunk);
            if (r != chunk) {
                if (r != size) {
                    ret = 1;
                    break;
                }
            }
            crc32(buf, r, crc);
            size -= r;
            if (size <= 0) {
                break;
            }
        }
        close(fd);
    }
    return ret;
}

int write_file_f(char *fn, void *buf, int size)
{
    int ret = -1;
    FILE *fd = fopen(fn, "w+b");
    if (fd) {
        ret = fwrite(buf, 1, size, fd);
        fclose(fd);
        if (ret == size) {
            ret = 0;
        }
        else {
            ret = 1;
        }
    }
    return ret;
}

int write_file_o(char *fn, void *buf, int size)
{
    int ret = -1;
    int fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0777);
    if (fd >= 0) {
        ret = write(fd, buf, size);
        close(fd);
        if (ret == size) {
            ret = 0;
        }
        else {
            ret = 1;
        }
    }
    return ret;
}

// reads size amount of bytes from file to buf
// returns nonzero in case of error
int read_file(char *fn, void *buf, int size)
{
    if (use_open_rather_than_fopen) {
        return read_file_o(fn, buf, size);
    }
    return read_file_f(fn, buf, size);
}

// writes size amount of bytes from buf to file
// returns nonzero in case of error
int write_file(char *fn, void *buf, int size)
{
    if (use_open_rather_than_fopen) {
        return write_file_o(fn, buf, size);
    }
    return write_file_f(fn, buf, size);
}

// computes crc of file
// returns nonzero in case of error
int compute_file_crc(char *fn, void *buf, int size, int bufsize, unsigned long *crc)
{
    if (use_open_rather_than_fopen) {
        return compute_file_crc_o(fn, buf, size, bufsize, crc);
    }
    return compute_file_crc_f(fn, buf, size, bufsize, crc);
}

//-------------------------------------------------------------------
// From glibc
//-------------------------------------------------------------------

int strcasecmp(const char *s1, const char *s2)
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  int result;
  if (p1 == p2)
    return 0;
  while ((result = tolower(*p1) - tolower(*p2++)) == 0)
    if (*p1++ == '\0')
      break;
  return result;
}

//-------------------------------------------------------------------
// Following is to avoid another export from CHDK core
//-------------------------------------------------------------------

long mkdir_if_not_exist(const char *dirname)
{
    // Check if directory exists and create it if it does not.
    if (stat(dirname,0) != 0) return mkdir(dirname);
    return 0;   // Success
}

//-------------------------------------------------------------------
// Modified routines from gui_fselect.c
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

static int is_parent(const char *name)  { return (strcmp(name, "..") == 0); }
static int is_current(const char *name)  { return (strcmp(name, ".") == 0); }

// Custom readdir - populates extra info about the file or directory
// Uses stat() after calling readdir() - we don't currently get this
// extra data from the firmware function.
static int fs_readdir(DIR *d, fs_dirent *de, const char* path)
{
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

            sprintf(pbuf, "%s%s%s", path, path[strlen(path)-1]=='/'?"":"/", de->de->d_name);
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
static void process_dir(const char *parent, const char *name, int nested, void (*file_process)(const char *path, const char *file, fs_dirent *de), void (*dir_process)(const char *path, fs_dirent *de))
{
    DIR         *d;
    fs_dirent   de;

    // Get full name
    char *path;
    if (name)
    {
        path = malloc(strlen(parent) + strlen(name) + 2);
        sprintf(path, "%s%s%s", parent, parent[strlen(parent)-1]=='/'?"":"/", name);
    }
    else
    {
        path = (char*)parent;
    }

    // Open directory
    d = opendir_chdk(path,OPENDIR_FL_CHDK_LFN);

    if (d)
    {
        // let caller get the dir name first
        if (dir_process)
            dir_process(path, &de);

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
                    file_process(path, de.de->d_name, &de);
                }
            }
        }
        closedir(d);

    }

    if (name)
        free(path);
}


//-------------------------------------------------------------------
/*
 * File storage in memory
 * fs_fheader struct
 * file name (padded up to word boundary)
 * file content (padded up to word boundary)
 *
 * above repeated for each directory and its files, invalid magic means end of chain
 * directory is stored first to ease restoration
 */

#define FMAGIC 0x004c4946 // FIL
#define DMAGIC 0x00524944 // DIR

enum rstatus {
    RS_OK = 0,
    RS_NAMELONG,
    RS_CRC,
    RS_FERR,
};

// restore status
const char *rsshumanL[] = {
    "OK",
    "Files not backed up",
    "Stored files corrupted",
    "Stored files corrupted",
    "Storage area corrupted",
    "A/CHDK exists, aborting",
};

// following struct needs to be an integer number of words
typedef struct
{
    unsigned long   magic;      // "FIL" for file, "DIR" for directory
    unsigned long   size;       // file size
    unsigned long   mtime;      // modification timestamp
    unsigned long   nlen;       // name length
    unsigned long   crc;        // crc32 of file data
    unsigned long   status;     // result code when restoring
} fs_fheader;

//-------------------------------------------------------------------

const char *wl1[] = {"DISKBOOT.BIN", "PS.FIR", "PS.FI2", 0};
static void *whitelist = 0;
static unsigned storageneed = 0;
static unsigned filecount = 0;
static unsigned failcount = 0;
static unsigned dircount = 0;
static unsigned tfilecount = 0;
static unsigned tdircount = 0;
static unsigned fileprocd = 0;
static unsigned dirprocd = 0;
static void *starea = 0; // start of allocated exmem area
static unsigned stmax = 0; // size of allocated exmem area
static void *tmpbuf = 0; // temporary buffer (for crc check of files written)
const unsigned tmpsiz = 32768; // temporary buffer size
static void *storptr = 0;
static unsigned long stcrc = 0; // crc of whole storage area

unsigned wordpad(unsigned u)
{
    return (u + 3) & 0xfffffffc;
}

// returns nonzero if pointer is inside the storage area
int is_valid_stptr(void *p)
{
    if (p>starea+stmax-sizeof(fs_fheader) || p<starea) {
        return 0;
    }
    return 1;
}

// checks whether name is allowed
// returns nonzero if it is on whitelist
// also returns nonzero if whitelist is NULL
int iswhitelisted(const char *name)
{
    if (whitelist == 0) {
        return 1;
    }
    char **wl = whitelist;
    char *s = *wl;
    while (s) {
        if (strcasecmp(name, s) == 0) {
            return 1;
        }
        s = *(++wl);
    }
    return 0;
}

// moves *currptr from current, valid stored file/dir to the next (existing or not)
// returns nonzero in case of success
// if magic is nonzero, the next object with requested magic is searched
int get_next_object(unsigned long magic, void **currptr)
{
    char *p = *currptr;
    if (!is_valid_stptr(p)) {
        return 0;
    }
    fs_fheader *fh = (void*)p;
    while (fh->magic==FMAGIC || fh->magic==DMAGIC) {
        p += sizeof(fs_fheader) + wordpad(fh->nlen) + wordpad(fh->size);
        if (!is_valid_stptr(p)) {
            break;
        }
        fh = (void*)p;
        if (!magic || fh->magic == magic) {
            *currptr = (void*)p;
            return 1;
        }
    }
    return 0;
}

// returns pointer to current object's name or NULL when invalid
char *get_object_name(void *p)
{
    if (!is_valid_stptr(p)) {
        return 0;
    }
    fs_fheader *fh = p;
    p += sizeof(fs_fheader);
    if (!is_valid_stptr(p) || !is_valid_stptr(p+fh->nlen)) {
        return 0;
    }
    return (char*)p;
}

// returns pointer to current object's data or NULL when invalid
char *get_object_data(void *p)
{
    if (!is_valid_stptr(p)) {
        return 0;
    }
    fs_fheader *fh = p;
    p += sizeof(fs_fheader) + wordpad(fh->nlen);
    if (!is_valid_stptr(p) || !is_valid_stptr(p+fh->size)) {
        return 0;
    }
    return (char*)p;
}

// callback function for process_dir, counts files and their sizes
void fproc_totalsize(const char *path, const char *file, fs_dirent *de)
{
    if (whitelist && !iswhitelisted(file)) return;
    storageneed += wordpad(de->size);
    storageneed += wordpad(strlen(path) + 1 + strlen(file) + 1);
    storageneed += sizeof(fs_fheader);
    filecount += 1;
}

// callback function for process_dir, counts directories
void dproc_count(const char *path, __attribute__ ((unused))fs_dirent *de)
{
    if (whitelist && !iswhitelisted(path)) return;
    storageneed += wordpad(strlen(path) + 1);
    storageneed += sizeof(fs_fheader);
    dircount += 1;
}

// callback function for process_dir, stores directories
void dproc_store(const char *path, fs_dirent *de)
{
    if (whitelist && !iswhitelisted(path)) return;
    fs_fheader *fh = storptr;
    fh->magic = DMAGIC;
    fh->size = 0;
    fh->mtime = de->mtime;
    fh->nlen = strlen(path) + 1;
    fh->crc = 0;
    storptr += sizeof(fs_fheader);
    strcpy(storptr, path);
    storptr += wordpad(fh->nlen);
    dirprocd++;
}

// callback function for process_dir, stores files
void fproc_store(const char *path, const char *file, fs_dirent *de)
{
    if (whitelist && !iswhitelisted(file)) return;
    fs_fheader *fh = storptr;
    fh->magic = FMAGIC;
    fh->size = de->size;
    fh->mtime = de->mtime;
    fh->nlen = strlen(path) + 1 + strlen(file) + 1;
    fh->crc = 0;
    storptr += sizeof(fs_fheader);
    sprintf(storptr, "%s%s%s", path, path[strlen(path)-1]=='/'?"":"/", file);
    char *name = storptr;
    storptr += wordpad(fh->nlen);
    
    if ( read_file(name, storptr, fh->size) ) {
        failcount++;
        storptr -= sizeof(fs_fheader);
        storptr -= wordpad(fh->nlen);
        console_add_line(file);
    }
    else {
        crc32(storptr, fh->size, &fh->crc); // fh->crc is set to zero prior to this
        storptr += wordpad(fh->size);
        fileprocd++;
    }
}

// attempts to allocate an exmem area that is large enough to backup files
// uses exmem types that are unlikely to be used and are widely available
// uses globals
void alloc_storage()
{
    const char *exmemtypes[] = {"EXMEM_FIRMUP", "EXMEM_FAMODE", 0};
    const unsigned cushion = 128; // extra data to allocate (for eventual cached use)
    unsigned n, i;
    exmem_alloc_info ai;
    storageneed += sizeof(fs_fheader); // for the last entry
    storageneed += tmpsiz; // for temp buffer
    starea = 0;
    i = 0;
    while (exmemtypes[i]) {
        for (n=0; n<exmem_type_count; n++) {
            if (strncmp(exmemtypes[i],get_exmem_type_name(n),strlen(exmemtypes[i]))==0) {
                get_exmem_type_status(n, &ai);
                if (!ai.len) {
                    starea = exmem_alloc_uncached(n, storageneed+cushion, 0);
                    get_exmem_type_status(n, &ai);
                    if (ai.len-cushion < storageneed) {
                        starea = 0;
                        exmem_free_uncached(n);
                        char buf[64];
                        sprintf(buf,"%s need %x got %x",exmemtypes[i],storageneed,ai.len);
                        console_add_line(buf);
                    }
#if 1
                    else {
                        char buf[64];
                        sprintf(buf,"%s adr %x",exmemtypes[i],starea);
                        console_add_line(buf);
                    }
#endif
                }
                break;
            }
        }
        if (starea) {
            starea += cushion/2; // leave a safety border on both start and end of allocation
            stmax = storageneed - tmpsiz;
            tmpbuf = starea + stmax;
            storptr = starea;
            break;
        }
        i++;
    }
}

// restores stored files to directory 'dest'
// test: 0 - write to card, 1 - check only
// returns 0 for OK
int restore(const char *dest, int test)
{
    char buf[256];
    int ret = 0;
    if (!starea) {
        return 1;
    }
    if (!test && strlen(dest)>2) mkdir_if_not_exist(dest);
    storptr = starea;
    if (!is_valid_stptr(storptr)) {
        return 2;
    }
    fs_fheader *fh = storptr;
    while (1) {
        if (fh->magic==DMAGIC) {
            char *lastdir = get_object_name(storptr);
            lastdir += 2; // chop off A/
            if (strlen(dest)+1+strlen(lastdir) > 255) {
                fh->status = RS_NAMELONG;
                ret |= 0x40000000;
                if (!get_next_object(0, &storptr)) {
                    break;
                }
                continue;
            }
            sprintf(buf,"%s/%s",dest,lastdir);
            if (!test) {
                ret |= mkdir_if_not_exist(buf)?0x20000000:0;
                if (stat(buf,0) == 0) {
                    tdircount--;
                }
            }
            else {
                msleep(10);
                tdircount--;
            }
        }
        else if (fh->magic==FMAGIC) {
            if (strlen(dest)+1+fh->nlen-2 > 255) {
                fh->status = RS_NAMELONG;
                ret |= 0x40000000;
                if (!get_next_object(0, &storptr)) {
                    break;
                }
                continue;
            }
            unsigned long crc = 0;
            char *fn = get_object_name(storptr);
            if (fn) {
                fn +=2; // chop off A/
            }
            else {
                return ret + 3;
            }
            char *fd = get_object_data(storptr);
            if (fd) {
                crc32(fd, fh->size, &crc); // crc is set to zero prior to this
                if (crc != fh->crc) {
                    fh->status = RS_CRC;
                    ret |= 0x10000000;
                    if (!get_next_object(0, &storptr)) {
                        break;
                    }
                    continue;
                }
            }
            sprintf(buf,"%s/%s",dest,fn);
            if (!test) {
                
                if ( write_file(buf, fd, fh->size) ) {
                    console_add_line(fn);
                    fh->status = RS_FERR;
                    ret |= 0x08000000;
                }
                else {
#if (FSCRC_CALC == 1)
                    crc = 0;
                    if ( compute_file_crc(buf, tmpbuf, fh->size, tmpsiz, &crc) ) {
                        console_add_line(fn);
                        fh->status = RS_FERR;
                        ret |= 0x04000000;
                    }
                    else {
                        if ( fh->crc != crc) {
                            console_add_line(fn);
                            fh->status = RS_FERR;
                            ret |= 0x02000000;
                        }
                        else {
                            struct utimbuf t;
                            t.actime = t.modtime = fh->mtime;
                            utime(buf, &t);
                            tfilecount--;
                        }
                    }
#else // (FSCRC_CALC != 1)
                    struct utimbuf t;
                    t.actime = t.modtime = fh->mtime;
                    utime(buf, &t);
                    tfilecount--;
#endif
                }
                
            }
            else {
                msleep(10);
                tfilecount--;
            }
        }
        else {
            break;
        }
        
        // advance pointer, error out if impossible
        if (!get_next_object(0, &storptr)) {
            ret += 4;
            break;
        }
        fh = storptr;
    }
    return ret;
}

//-------------------------------------------------------------------

int can_boot_from_fat32()
{
    if (conf.platformid > 0x3222) { // r47+
        return 1;
    }
    return 0;
}

//-------------------------------------------------------------------

char *help1[] = {
    "Utility for temporarily storing CHDK",
    "system files in RAM, allowing to format",
    "card via Canon menu.",
    "Usage:",
    "1. Start this utility, press SET to backup",
    "2. Leave the utility, format card in Canon",
    "   menu. Do NOT switch off the camera.",
    "3. Start this utility again, press SET",
    "4. Make card bootable via CHDK menu",
    "5. Restart camera before starting to shoot",
    "IMPORTANT",
    "Do not format card if you get errors.",
    "Only the following files are stored:",
    "- Everything in CHDK directory.",
    "- DISKBOOT.BIN, PS.FIR, PS.FI2",
    "WARNING",
//    "Do NOT use this on partitioned cards!",
//    "Cards larger than 32GB will lose autoboot!",
//    "Cards larger than 2GB will lose autoboot",
//    "on cameras released prior to 2011 !",
    "Excessive amount of data in CHDK direcory",
    "may cause a crash while making backup!",
    "Do not use this on mission critical files!",
};

char *help_partwarn = "Do NOT format, card will lose partitions!";
char *help_bootwarn = "Card will lose autoboot if formatted!";

//-------------------------------------------------------------------


/*************** GUI MODULE *******************/

#include "gui_mbox.h"
#include "keyboard.h"

void gui_fileshelter_menu_kbd_process();
int gui_fileshelter_kbd_process();
void gui_fileshelter_draw();

gui_handler GUI_MODE_FILESHELTER = 
/*GUI_MODE_FILESHELTER*/   { GUI_MODE_MODULE, gui_fileshelter_draw, gui_fileshelter_kbd_process, gui_fileshelter_menu_kbd_process, 0, 0 };

static unsigned int cardsize;
static int canfat32boot;
static int partcount;
static int noformatyet;

static int fileshelter_mode = 0;
static unsigned disph = 0, dispw = 0, helph = 0;
static unsigned restore_status;
static unsigned scrolly;
static int fileshelter_redraw;
gui_handler *fileshelter_old_guimode;

int gui_fileshelter_kbd_process()
{
    switch (kbd_get_autoclicked_key())
    {
    case KEY_SET:
        if (starea) {
            if (stat("A/CHDK",0) != 0) {
                // CHDK dir not found, assuming formatted card
                unsigned long crc = 0;
                tfilecount = filecount;
                tdircount = dircount;
                fileshelter_redraw = 8;
                msleep(100); // give the display a chance to update
                crc32(starea, stmax, &crc);
                if (crc == stcrc) {
                    fileshelter_redraw = 5;
                    restore_status = restore("A",0);
                    fileshelter_redraw = 3;
                }
                else {
                    restore_status = 4;
                    fileshelter_redraw = 3;
                }
            }
            else {
                // CHDK dir present, doing a test run only
                unsigned long crc = 0;
                tfilecount = filecount;
                tdircount = dircount;
                fileshelter_redraw = 8;
                msleep(100); // give the display a chance to update
                crc32(starea, stmax, &crc);
                if (crc == stcrc) {
                    fileshelter_redraw = 11;
                    restore_status = restore("A",1);
                    fileshelter_redraw = 10;
                }
                else {
                    restore_status = 4;
                    fileshelter_redraw = 10;
                }
            }
        }
        else {
            fileshelter_redraw = 6;
            storageneed = 0;
            filecount = 0;
            dircount = 0;
            whitelist = wl1;
            process_dir("A/", 0, 0, fproc_totalsize, dproc_count);
            whitelist = 0;
            process_dir("A/CHDK", 0, 999, fproc_totalsize, dproc_count);
            alloc_storage();
            if (starea) {
                fileshelter_redraw = 7;
                failcount = 0;
                fileprocd = 0;
                dirprocd = 0;
                whitelist = wl1;
                process_dir("A/", 0, 0, fproc_store, dproc_store);
                whitelist = 0;
                process_dir("A/CHDK", 0, 999, fproc_store, dproc_store);
                fs_fheader *fh = storptr;
                fh->magic = 0;
                stcrc = 0;
                crc32(starea, stmax, &stcrc);
            }
            fileshelter_redraw = 1;
        }
        break;
    case KEY_DOWN:
        if (scrolly < sizeof(help1)/sizeof(char*)-1) {
            scrolly++;
        }
        else {
            scrolly = 0;
        }
        fileshelter_redraw = 4;
        break;
    case KEY_UP:
        if (scrolly > 0) {
            scrolly--;
        }
        else {
            scrolly = sizeof(help1)/sizeof(char*)-1;
        }
        fileshelter_redraw = 4;
        break;
    case KEY_SHOOT_HALF:
        fileshelter_mode = fileshelter_mode==0?1:0;
#if 0
        if (fileshelter_mode) {
            use_open_rather_than_fopen = 0;
        }
        else {
            use_open_rather_than_fopen = 1;
        }
#endif
        fileshelter_redraw = 2;
        break;
    }
    return 0;
}

//-------------------------------------------------------------------

int basic_module_init()
{
    if (!running) {
        running = 1;
        disph = camera_screen.height / FONT_HEIGHT;
        dispw = camera_screen.width / FONT_WIDTH;
        helph = camera_screen.height / FONT_HEIGHT - 5;
        noformatyet = 1;
    }
    else {
        noformatyet = stat("A/CHDK",0)?0:1;
    }
    scrolly = 0;

    cardsize = GetTotalCardSpaceKb();
    partcount = get_part_count();
    canfat32boot = can_boot_from_fat32();

    fileshelter_redraw = 2;
    fileshelter_old_guimode = gui_set_mode(&GUI_MODE_FILESHELTER);
    return 1;
}

void gui_fileshelter_menu_kbd_process()
{
    if (!starea) {
        running = 0;
    }

    gui_set_mode(fileshelter_old_guimode);
}

void gui_fileshelter_draw()
{
    static int selfupdate = 0;
    static int warnshown = 0;
    
    int fredraw = fileshelter_redraw;
    fileshelter_redraw = 0;

    if (fredraw || selfupdate) {

        if (fredraw == 2)
        {
            draw_rectangle(camera_screen.disp_left, 0, camera_screen.disp_right, camera_screen.height-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(camera_screen.disp_left, 0, "CHDK backup & restore utility", MAKE_COLOR(COLOR_WHITE, COLOR_BLACK));
            draw_string(camera_screen.disp_left, FONT_HEIGHT, !starea?"SET-backup  MENU-exit \x18\x19-help":noformatyet?"SET-check   MENU-exit \x18\x19-help":"SET-restore MENU-exit \x18\x19-help", MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));

#if 0
            if (fileshelter_mode) { // meant to be temporary
                draw_char(camera_screen.disp_left+FONT_WIDTH*30, 0, '*', MAKE_COLOR(COLOR_BLACK, COLOR_BLUE));
            }
            else {
                draw_char(camera_screen.disp_left+FONT_WIDTH*30, 0, ' ', MAKE_COLOR(COLOR_BLACK, COLOR_BLUE));
            }
#endif

            fredraw = 4;
            selfupdate = 0;
        }
        if (fredraw == 3)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            if (tfilecount || tdircount) {
                sprintf(osdbuf, "Not restored: %u files, %u dirs", tfilecount, tdircount);
                if (sizeof(osdbuf)>dispw) osdbuf[dispw] = 0;
                draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, COLOR_YELLOW));
            }
            unsigned rssl = restore_status & 0xff;
            char *hrsm = "???";
            if (rssl < sizeof(rsshumanL)/sizeof(char*)) {
                hrsm = (char*)rsshumanL[rssl];
            }
            sprintf(osdbuf, restore_status?"Restore status: %08x":"Restore status:", restore_status);
            draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, restore_status?COLOR_RED:COLOR_GREEN));
            sprintf(osdbuf, "%s", hrsm);
            draw_string(camera_screen.disp_left, 4*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, restore_status?COLOR_RED:COLOR_GREEN));
            selfupdate = 0;
        }
        if (fredraw == 10)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            if (tfilecount || tdircount) {
                sprintf(osdbuf, "Failed: %u files, %u dirs", tfilecount, tdircount);
                if (sizeof(osdbuf)>dispw) osdbuf[dispw] = 0;
                draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, COLOR_YELLOW));
            }
            unsigned rssl = restore_status & 0xff;
            char *hrsm = "???";
            if (rssl < sizeof(rsshumanL)/sizeof(char*)) {
                hrsm = (char*)rsshumanL[rssl];
            }
            sprintf(osdbuf, restore_status?"Test status: %08x":"Test status:", restore_status);
            draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, restore_status?COLOR_RED:COLOR_GREEN));
            sprintf(osdbuf, "%s", hrsm);
            draw_string(camera_screen.disp_left, 4*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, restore_status?COLOR_RED:COLOR_GREEN));
            selfupdate = 0;
        }
        if (fredraw == 1)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            sprintf(osdbuf, "%u files, %u dirs, total %u bytes", filecount, dircount, storageneed);
            draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
            if (starea && !failcount) {
                draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, "Backup successful", MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            }
            else if (failcount) {
                sprintf(osdbuf, "Backup failed to store %u file(s)", failcount);
                draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, COLOR_YELLOW));
                draw_string(camera_screen.disp_left, 4*FONT_HEIGHT, "See CHDK console for filename(s)", MAKE_COLOR(COLOR_BLACK, COLOR_YELLOW));
            }
            else if (!starea) {
                draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, "Backup failed - insufficient memory", MAKE_COLOR(COLOR_BLACK, COLOR_RED));
            }
            else {
                draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, "Backup failed", MAKE_COLOR(COLOR_BLACK, COLOR_RED));
            }
            selfupdate = 0;
        }
        if (fredraw == 4)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*5, camera_screen.disp_right, camera_screen.height-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            unsigned y;
            for (y=0; y<helph; y++) {
                if (scrolly+y < sizeof(help1)/sizeof(char*)) {
                    draw_string(camera_screen.disp_left, (5+y)*FONT_HEIGHT, help1[scrolly+y], MAKE_COLOR(COLOR_BLACK, COLOR_GREY));
                }
            }
            if (!warnshown) {
                warnshown = 1;
                fredraw = 9;
            }
            selfupdate = 0;
        }
        if (fredraw == 5)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Restoring ...", MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            selfupdate = 5;
        }
        if (fredraw == 11)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Testing ...", MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            selfupdate = 5;
        }
        if (fredraw == 6)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Counting ...", MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            selfupdate = 6;
        }
        if (fredraw == 7)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Backing up ...", MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            selfupdate = 7;
        }
        if (fredraw == 8)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);
            draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, "Verifying integrity ...", MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            selfupdate = 6;
        }
        if (fredraw == 9)
        {
            draw_rectangle(camera_screen.disp_left, FONT_HEIGHT*2, camera_screen.disp_right, FONT_HEIGHT*5-1, MAKE_COLOR(COLOR_BLACK, COLOR_BLACK), RECT_BORDER0|DRAW_FILLED);

            // draw card related warnings if any
            if (canfat32boot && cardsize>32*1024*1024) {
                draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, help_bootwarn, MAKE_COLOR(COLOR_BLACK, COLOR_RED));
            }
            else if (!canfat32boot && cardsize>2*1024*1024) {
                draw_string(camera_screen.disp_left, 2*FONT_HEIGHT, help_bootwarn, MAKE_COLOR(COLOR_BLACK, COLOR_RED));
            }
            if (partcount > 1) {
                draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, help_partwarn, MAKE_COLOR(COLOR_BLACK, COLOR_RED));
            }

        }

        if (selfupdate == 5) {
            static unsigned fc=0,dc=0;
            if ((tfilecount!=fc) || (tdircount!=dc)) {
                sprintf(osdbuf, "Remaining: %u files, %u dirs       ", tfilecount, tdircount);
                if (sizeof(osdbuf)>dispw) osdbuf[dispw] = 0;
                draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            }
            fc = tfilecount;
            dc = tdircount;
        }
        else if (selfupdate == 6) {
            static int phase = 0;
            const char *pc = "/-\\|";
            draw_char(camera_screen.disp_left, 3*FONT_HEIGHT, pc[phase&3], MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            phase++;
        }
        else if (selfupdate == 7) {
            static unsigned fc=0,dc=0;
            if ((fileprocd!=fc) || (dirprocd!=dc)) {
                sprintf(osdbuf, "Remaining: %u files, %u dirs       ", filecount-fileprocd, dircount-dirprocd);
                if (sizeof(osdbuf)>dispw) osdbuf[dispw] = 0;
                draw_string(camera_screen.disp_left, 3*FONT_HEIGHT, osdbuf, MAKE_COLOR(COLOR_BLACK, COLOR_GREEN));
            }
            fc = fileprocd;
            dc = dirprocd;
        }

    }

}

