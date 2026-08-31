#ifndef __FLT_H__
#define __FLT_H__

#if !defined(USE_INT32_FOR_PTRS)
#include "versions.h"
#endif

typedef unsigned short  uint16_t;
typedef short           int16_t;
typedef unsigned int    uint32_t;
typedef int             int32_t;

//================= Module information structure =====================

#define MODULEINFO_V1_MAGICNUM      0x023703e5

// CHDK versions
#define ANY_CHDK_BRANCH		        0
#define REQUIRE_CHDK_MAIN           1
#define REQUIRE_CHDK_DE		        2
#define REQUIRE_CHDK_SDM	        3
#define REQUIRE_CHDK_PRIVATEBUILD	4

// PlatformID check (can be used to make a module specific to a camera model)
#define ANY_PLATFORM_ALLOWED	    0

// Architecture of build (GCC ABI, thumb/thumb2, etc)
#define GCC_ELF_THUMB               1
#define GCC_EABI_THUMB              2
#define GCC_ELF_THUMB2           0x11   // Cortex R, unlikely variant
#define GCC_EABI_THUMB2          0x12   // Cortex R
#define GCC_ELF_THUMB2A          0x21   // Cortex A, unlikely variant
#define GCC_EABI_THUMB2A         0x22   // Cortex A

// Base module interface - once loaded into memory and any relocations done these
// functions provide the minimum interface to run the module code.
// Extend this interface for modules that need additional functions (e.g. libdng_sym in dng.h)
typedef struct
{
    int     (*loader)();        // loader function. Optional
                                // Should only perform initialisation that can be done when module is first loaded.
                                // Returns 0 for success, non-zero (error code) on failure
    int     (*unloader)();      // unloader function. Optional
                                // Does any necessary clean up just prior to module being removed
                                // Returns 0 for success, non-zero (error code) on failure [return value not currently used]
    int     (*can_unload)();    // ask module if it is safe to unload. Optional
                                // Called each keyboard task tick to see if it is safe to unload the module
    int     (*exit_alt)();      // alert module that CHDK is leaving <ALT> mode. Optional
                                // If the module should be unloaded when <ALT> mode exits then this function
                                // should tell the module to return 'true' on the next call to can_unload
                                // This function should not do any cleanup or shutdown of the module
    int     (*run)();           // run module (for simple modules like games). Optional
                                // If not supplied then an extended interface is required to call module code
} base_interface_t;

// Module types
#define MTYPE_UNKNOWN       0       // Undefined
#define MTYPE_EXTENSION     1       // System extension (e.g. dng, edge overlay, etc)
#define MTYPE_GAME          2       // Games (added to Games menu)
#define MTYPE_TOOL          3       // Custom tool (added to Tools menu)
#define MTYPE_SCRIPT_LANG   4       // Script language
#define MTYPE_MASK          0xFF    // Mask for type values above
#define MTYPE_SUBMENU_TOOL  0x100   // Flag to indicate tool module is a submenu

// Module information structure
// Contains everything used to communicate with and run code in the module
typedef struct
{
	uint32_t            magicnum;                   // MODULEINFO_V1_MAGICNUM - sanity check when loading
	uint32_t            sizeof_struct;              // sizeof this struct - sanity check when loading
	_version_t          module_version;             // version of module - compared to version in module_handler_t
	uint32_t            chdk_required_branch;       // CHDK version checks
	uint32_t            chdk_required_ver;
    uint32_t            chdk_required_architecture;
	uint32_t            chdk_required_platfid;

	int32_t             moduleName;			    // pointer to string with module name or -LANG_ID
	int32_t             moduleType; 		    // module type flags (see MTYPE_ definitions)

#if defined(USE_INT32_FOR_PTRS) // For elfflt.c on 64 bit Linux
    uint32_t            lib;
#else
    base_interface_t*   lib;                    // Pointer to interface library
#endif

    // Version checks (set to {0,0} to skip check)
    _version_t          conf_ver;               // CONF version (Conf structure in conf.h)
    _version_t          cam_screen_ver;         // CAM SCREEN version (camera_screen in camera_info.h)
    _version_t          cam_sensor_ver;         // CAM SENSOR version (camera_sensor in camera_info.h)
    _version_t          cam_info_ver;           // CAM INFO version (camera_info in camera_info.h)

    char                symbol;                 // Optional symbol to use in Games / Tools menu for module
} ModuleInfo;

//================= FLAT Header structure ==============

/*
    Structure of CFLAT v10 file:
        - flt_hdr           [see flat_hdr structure]
        - .text             [start from flt_hdr.entry]
        - .rodata + .data   [start from flt_hdr.data_start]
        - reloc_list        [start from flt_hdr.reloc_start]
            * this is just array of offsets in flat
        - import_list       [start from flt_hdr.import_start]
            * this is array of import_record_t

    Notes:
        .bss no longer stored in file, segment generated at runtime,
             - starts immediately after .data segment (@flt_hdr.reloc_start)
             - size specified in flt_hdr.bss_size
*/

#define FLAT_VERSION        10
#define FLAT_MAGIC_NUMBER   0x414c4643      //"CFLA"

// Module file header
// This is located at the start of the module file and is used to load the module
// into memory and perform any necessary relocations.
// Once loaded the _module_info pointer handles everything else.
typedef struct
{
    uint32_t magic;         // "CFLA"  (must match FLAT_MAGIC_NUMBER)
    uint32_t rev;           // version (must match FLAT_VERSION)
    uint32_t entry;         // Offset of start .text segment
    uint32_t data_start;    // Offset of .data segment from beginning of file
    uint32_t bss_size;      // Size of .bss segment (for run time allocation) [bss start == reloc_start]

    // Relocation info
    uint32_t reloc_start;   // Offset of relocation records from beginning of file (also start of bss segment)
    uint32_t import_start;  // Offset of import section
    uint32_t import_size;   // size of import section

    // Offset / Pointer to ModuleInfo structure
    union
    {
        uint32_t            _module_info_offset;    // Offset ModuleInfo from beginning of file
#if defined(USE_INT32_FOR_PTRS) // For elfflt.c on 64 bit Linux
        uint32_t            _module_info;           // Ptr to ModuleInfo after relocation
#else
        ModuleInfo*         _module_info;           // Ptr to ModuleInfo after relocation
#endif
    };
} flat_hdr;

#endif /* __FLT_H__ */
