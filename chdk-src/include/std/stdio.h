#ifndef STDIO_H
#define STDIO_H

// CHDK stdio

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

// reverse engineered file struct. Appears to be valid for both vxworks and dryos
// don't use this directly unless you absolutely need to
// don't EVER try to create one yourself, as this isn't the full structure.
typedef struct FILE_S {
    int fd;         // used by Read/Write
    unsigned len;   // +4 verfied in Fseek_FileStream
    int unk0;       // +8
    unsigned pos;   // +0xC verified in Fseek_FileStream
    // unk1;        // +0x10 
    // unk2;        // +0x14
    // io_buf;      // +0x18 32k uncached allocated in Fopen_FileStream
    // unk3;        // +0x20 related to StartFileAccess_Sem
    // ...name
} FILE;

extern FILE *fopen(const char *filename, const char *mode);
extern long fclose(FILE *f);
extern long fread(void *buf, long elsize, long count, FILE *f);
extern long fwrite(const void *buf, long elsize, long count, FILE *f);
extern long fseek(FILE *file, long offset, long whence);
extern long fflush(FILE *file);
extern long feof(FILE *file);
extern long ftell(FILE *file);
extern char *fgets(char *buf, int n, FILE *f);

#define ftell(f) (f ? (long)f->pos : -1)

//---------------------------------------------------------------

#endif
