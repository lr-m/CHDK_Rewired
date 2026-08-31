#ifndef DIRENT_H
#define DIRENT_H

// CHDK dirent

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

// CHDK structs for opendir/readdir/closedir
// Conversion to/from camera specific versions done in wrapper code
struct dirent   // Returned from readdir
{
    char            d_name[100];    // We only use the name value
};

// Returned from opendir
typedef struct
{
    void            *cam_DIR;   // Camera specific internal DIR structure
    int             fw_dir;     // 1 if firmware function is used, 0 if CHDK implementation
    struct dirent   dir;        // Last info returned from readdir
} DIR;

// opendir_chdk has non-standard second arg
#define OPENDIR_FL_NONE     0x0 
#define OPENDIR_FL_CHDK_LFN 0x1 // use CHDK long file name support, if available.
                                // No effect on VxWorks, which supports LFNs natively
                                // Non-CHDK LFN support on DryOS varies by OS version and filesystem
extern DIR*           opendir (const char* name); // wrapper of opendir_chdk(name,FL_NONE) for easy compatibility
extern DIR*           opendir_chdk (const char* name,unsigned flags);
extern struct dirent* readdir (DIR*);
extern int            closedir (DIR*);

//---------------------------------------------------------------

#endif
