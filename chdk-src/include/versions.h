#ifndef VERSIONS_H
#define VERSIONS_H

// Note: used in modules and platform independent code.
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//-------------------------------------------------------------------

// API / structure version for checking module compatibility against core CHDK build
typedef struct
{
    unsigned short  major;
    unsigned short  minor;
} _version_t;

// CHDK version info
typedef struct
{
    short   major;
    short   minor;
    short   maintenance;
    short   revision;       // SVN revision
} _chdk_version_t;

extern _chdk_version_t chdk_version, script_version;

//-------------------------------------------------------------------

extern void parse_version(_chdk_version_t *ver, const char *build, const char *rev);
extern int cmp_chdk_version(_chdk_version_t ver1, _chdk_version_t ver2);
extern int chk_api_version(_version_t api_ver, _version_t req_ver);

//-------------------------------------------------------------------

#define ANY_VERSION             {0,0}

//-------------------------------------------------------------------

#endif
