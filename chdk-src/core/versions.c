#include "stdlib.h"
#include "string.h"
#include "versions.h"

//==========================================================
_chdk_version_t chdk_version;

//==========================================================
static const char* get_val(const char *s, short *v)
{
    if (s && *s)
    {
        *v = strtol(s, (char**)&s, 0);
        if (s && (*s == '.')) s++;
    }
    return s;
}

// Convert version strings into _chdk_version_t structure
void parse_version(_chdk_version_t *ver, const char *build, const char *rev)
{
    memset(ver, 0, sizeof(_chdk_version_t));
    build = get_val(build, &ver->major);
    build = get_val(build, &ver->minor);
    build = get_val(build, &ver->maintenance);
    get_val(build, &ver->revision);
    get_val(rev, &ver->revision);
}

// Compare two chdk versions
// Returns 0 if equal, -1 if ver1 < ver2, 1 if ver1 > ver2
int cmp_chdk_version(_chdk_version_t ver1, _chdk_version_t ver2)
{
    if (ver1.major       < ver2.major)       return -1;
    if (ver1.major       > ver2.major)       return  1;
    if (ver1.minor       < ver2.minor)       return -1;
    if (ver1.minor       > ver2.minor)       return  1;
    if (ver1.maintenance < ver2.maintenance) return -1;
    if (ver1.maintenance > ver2.maintenance) return  1;
    if (ver1.revision    < ver2.revision)    return -1;
    if (ver1.revision    > ver2.revision)    return  1;
    return 0;
}

// Check requested api version against built api version
// Returns 1 if requested version is compatible, 0 otherwise
int chk_api_version(_version_t api_ver, _version_t req_ver)
{
    if (req_ver.major == 0)     // Request match against ANY API
        return 1;
    // Compatible only if built API major version is the same, and minor is same or later
    if ((api_ver.major == req_ver.major) && (api_ver.minor >= req_ver.minor))
        return 1;
    return 0;

}

//==========================================================
