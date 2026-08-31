#ifndef STDLIB_H
#define STDLIB_H

// CHDK stdlib

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

extern int rand(void);
extern void* srand(unsigned int seed);

extern void qsort(void *__base, int __nelem, int __size, int (*__cmp)(const void *__e1, const void *__e2));

extern long sprintf(char *s, const char *st, ...);

extern long strtol(const char *nptr, char **endptr, int base);
extern unsigned long strtoul(const char *nptr, char **endptr, int base);
#define atoi(n) strtol((n),NULL,0)

extern void *malloc(long size);
extern void free(void *p);

extern int open(const char *name, int flags, int mode );
extern int close(int fd);
extern int write(int fd, const void *buffer, long nbytes);
extern int read(int fd, void *buffer, long nbytes);
extern int lseek(int fd, long offset, int whence);
extern int rename(const char *oldname, const char *newname);
extern int chdir(char *pathname);
extern int remove(const char *name);

extern int abs( int v );

//-------------------------------------------------------------------------------------

// From sys/stat.h

// CHDK 'stat' structure - does not match VxWorks or DryOS internal structs
// Converted in stat function (generic/wrappers.c)
// This is the minimal set of values now available from the firmware
struct stat
{
    unsigned long   st_attrib;
    unsigned long   st_size;
    unsigned long   st_ctime;
    unsigned long   st_mtime;
};

extern long mkdir(const char *dirname);
extern int stat(const char *name, struct stat *pStat);

//-------------------------------------------------------------------------------------

// From fcntl.h

#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_APPEND        8   // DryOS only, wrapper code will removed this for VxWorks so file will be overwritten instead of appended

// CHDK defined values - note does not match VxWorks values
// Values are corrected in 'open' function to match OS requirements
#define O_TRUNC         0x200
#define O_CREAT         0x100

//-------------------------------------------------------------------------------------

// From VxWorks

#define DOS_ATTR_RDONLY         0x01            /* read-only file */
#define DOS_ATTR_HIDDEN         0x02            /* hidden file */
#define DOS_ATTR_SYSTEM         0x04            /* system file */
#define DOS_ATTR_VOL_LABEL      0x08            /* volume label (not a file) */
#define DOS_ATTR_DIRECTORY      0x10            /* entry is a sub-directory */
#define DOS_ATTR_ARCHIVE        0x20            /* file subject to archiving */

//-------------------------------------------------------------------------------------

// CHDK

extern void msleep(long msec);

extern unsigned char SetFileAttributes(const char* fn, unsigned char attr);

extern long mkdir_if_not_exist(const char *dirname);

extern void *umalloc(long size);
extern void ufree(void *p);

//---------------------------------------------------------------

#endif
