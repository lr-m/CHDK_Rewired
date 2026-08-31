#!build time optional components - comment out the features you dont want to have in your build or copy this file to localbuildconf.inc and override settings there
OPT_CURVES=1
OPT_DEBUGGING=1

# Force config option on to allow Lua to call arbitrary ROM functions and eventprocs
# By default this is a user controlled setting in the CHDK UI.
# Setting this build option will permanently enable native calls
# WARNING: This is DANGEROUS. e.g. call_event_proc("EraseSectorOfRom",...)
#OPT_FORCE_LUA_CALL_NATIVE=1

# experimental - allocate memory from alternate heap. Default 2MB from EXMEM_RAMDISK
# See exmem_malloc_init in generic/wrappers.c
#OPT_EXMEM_MALLOC=1
# Set OPT_CHDK_IN_EXMEM to build and load CHDK into EXMEM memory 
# Need to define MEMISOSTART and EXMEM values in PLATFORM/PLATFORMSUB/makefile.inc (see G12 or SX30 for example)
# (if setting this must also set OPT_EXMEM_MALLOC !)
#OPT_CHDK_IN_EXMEM=1
# set this to enable exmem corruption testing (see wrappers.c). (Don't set OPT_CHDK_IN_EXMEM with this !)
#OPT_EXMEM_TESTING=1

# experimental PTP/USB interface
OPT_PTP=1

# record asserts in lua C code with dbg_dump_assert
#OPT_DBG_LUA_ASSERT=1

# Enable this to include debug code in the motion_detect module
#OPT_MD_DEBUG=1

# Enable this for an extra misc value display showing viewport history and FPS. Requires active_viewport_buffer FW variable.
#OPT_VIEWPORT_DEBUG=1

# needs proper fi2.inc in platform/ !!! see http://chdk.setepontos.com/index.php/topic,2995.0.html
#OPT_FI2=1

# if enabled, compiler will produce a lot of warnings, maybe not always correct ones, see http://chdk.setepontos.com/index.php/topic,2509.msg32191.html#msg32191
#OPT_WARNINGS=1

# If enabled (and reference binaries are present in /tools), compiler will generate function signatures
OPT_GEN_SIGS=1
# If enabled (and firmware binaries are present) compiler will update "stubs_entry.S"
OPT_GEN_STUBS=1

# Define this to add a 100K block of 0 bytes to the end of main.bin
#OPT_ZERO100K

# for CHDK-DE presets without OPT_DEFAULT_LANG
#OPT_DE_VERSION=1

# make the build show an "expired" notice after N days. To encourage users of test builds to provide feedback
#OPT_EXPIRE_TEST=

# Generate firmware calls and branches as PC24 relative offsets instead of long calls (saves 2.5 - 3K on core build size)
#  - requires a 32 bit version of ARM GCC (tested on Windows with GCC 4.4.0, 4.5.1 & 4.6.2), fails on 64 bit GCC (Linux & OSX)
#  - CHDK must be loaded in the lowest 16MB of RAM, firmware must be in upper 16MB
#  - will be disabled if OPT_CHDK_IN_EXMEM is enabled
#  - linker will generate errors if any calls/branches are out of range
#OPT_FIRMWARE_PC24_CALL=1

# Define this to enable camera to run with the battery door open
#  Note: not implemented for all cameras
#OPT_RUN_WITH_BATT_COVER_OPEN=1

# Define to enable capture of stats in file IO functions
#OPT_FILEIO_STATS=1

# Create module .elf and .flt dump files
#OPT_GEN_MODULE_DUMPS=1

# Define this to disable runtime compatibility check in loader
#OPT_DISABLE_COMPAT_CHECK=1

# Define this to enable building of tools using the capstone library (http://www.capstone-engine.org/)
# Required to rebuild stubs for thumb2 / digic 6 firmware
#OPT_CAPSTONE_TOOLS=1

# Set the following if capstone includes are not in the default search path
#CAPSTONE_TOOLS_INC=-I/path/to/capstone/include

# Set the following to the options required to link to the capstone library.
# At a minimum, -lcapstone or similar is required. If the library is not
# on the default search path, -L/path/to/capstone is also required
#CAPSTONE_TOOLS_LINK=-L/path/to/capstone -lcapstone

# Define this to disable E32 error (caused by broken image stabilizer)
# Only enable for newer cameras with optical image stabilizer, otherwise build will fail (due to missing fw function)
#OPT_DISABLE_CAM_ERROR=1

# ***please new entries above this line for protecting the chdk shell against errors***

# Override the value below to compile a different default language into
# the core CHDK code. This will save memory by not loading both english and the
# desired language.
# Set the value to the language file name without extension - e.g. OPT_DEFAULT_LANG=russian
#OPT_DEFAULT_LANG=

# Default svn revision. Used when svnversion fail.
DEF_SVN_REF=0
