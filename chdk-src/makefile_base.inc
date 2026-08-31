# Base makefile rules

# Define default target (place this first so it is always selected if no target on command line)
.PHONY: default
default: all

MEMISOSIZE="(&_end-&_start)"
# override this on the command line or in buildconf to use PRIMARY.BIN from a different tree
# should be an absolute path
PRIMARY_ROOT=$(topdir)platform
include $(topdir)buildconf.inc
# optional local version of buildconf.inc, not in SVN so it can be used in autobuilds
# and to avoid getting accidentally included in svn diffs
# you may also set your default camera here
-include $(topdir)localbuildconf.inc
include $(topdir)version.inc
-include $(topdir)revision.inc

ifndef OPT_DE_VERSION
    VER=CHDK
    ifndef OPT_DEFAULT_LANG
        OPT_DEFAULT_LANG=english
    endif
else
    VER=CHDK_DE
    ifndef OPT_DEFAULT_LANG
        OPT_DEFAULT_LANG=german
    endif
endif

# Define some shorthand file & directory locations
bin=$(topdir)bin
chdk=$(topdir)CHDK
core=$(topdir)core
doc=$(topdir)doc
include=$(topdir)include
loader=$(topdir)loader/$(PLATFORM)
lib=$(topdir)lib
larmutil=$(lib)/armutil
lcore=$(lib)/core
lfont=$(lib)/font
llang=$(lib)/lang
llibc=$(lib)/libc
llua=$(lib)/lua
lubasic=$(lib)/ubasic
modules=$(topdir)modules
platform=$(topdir)platform
tools=$(topdir)tools
cam=$(platform)/$(PLATFORM)
camfw=$(cam)/sub/$(PLATFORMSUB)
TARGET_PRIMARY=$(PRIMARY_ROOT)/$(PLATFORM)/sub/$(PLATFORMSUB)/PRIMARY.BIN
PAKWIF=$(tools)/pakwif$(EXE)
PAKFI2=$(tools)/packfi2/fi2enc$(EXE)
ENCODE_DISKBOOT=$(tools)/dancingbits$(EXE)

# Load camera settings (optional)
ifdef PLATFORM
  -include $(cam)/makefile.inc
  ifndef THUMB_FW
    ifeq ($(DIGIC),$(filter $(DIGIC),60 70))
      THUMB_FW=1
    endif
  endif
  # update build rules for Digic 7 ARM changes
  ifndef ARMV7A
    ifeq ($(DIGIC),$(filter $(DIGIC),70))
      ARMV7A=1
    endif
  endif
endif

# Sub-directory for object and dump files
# used by modules and libraries build
ifndef THUMB_FW
    O=.o/
else
    ifndef ARMV7A
        ifdef CAM_DRAW_YUV
            O=.o2/
        else
            O=.o4/
        endif
    else
        ifdef CAM_DRAW_YUV
            O=.o3/
        else
            O=.o5/
        endif
    endif
endif

SILENT=SILENT

##########################################################################

include $(topdir)makefile_env.inc

##########################################################################

.PHONY: all
all: all-recursive

.PHONY: clean
clean: clean-recursive

.PHONY: distclean
distclean: distclean-recursive

.dep:
	mkdir .dep

clean-recursive:
	@for i in $(SUBDIRS); do \
	    echo \>\> Cleaning in $(FOLDER)$$i; \
	    $(MAKE) -C $$i FOLDER="$(FOLDER)$$i/" clean || exit 1; \
	done

distclean-recursive:
	@for i in $(SUBDIRS); do \
	    echo \>\> Distcleaning in $(FOLDER)$$i; \
	    $(MAKE) -C $$i FOLDER="$(FOLDER)$$i/" distclean || exit 1; \
	done

all-recursive:
	@for i in $(SUBDIRS); do \
	    echo \>\> Entering to $(FOLDER)$$i; \
	    $(MAKE) -C $$i FOLDER="$(FOLDER)$$i/" || exit 1; \
	    echo \<\< Leaving $(FOLDER)$$i; \
	done

depend-recursive:
	@for i in $(SUBDIRS); do \
	    echo \>\> Entering to $(FOLDER)$$i; \
	    $(MAKE) -C $$i FOLDER="$(FOLDER)$$i/" depend || exit 1; \
	    echo \<\< Leaving $(FOLDER)$$i; \
	done

ifdef SILENT
.SILENT:
endif


# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
Makefile: ;
makefile: ;
$(topdir)makefile_base.inc: ;
$(topdir)buildconf.inc: ;
$(topdir)localbuildconf.inc: ;
$(topdir)version.inc: ;
$(topdir)revision.inc: ;
$(cam)/makefile.inc: ;
*.c: ;
*.txt: ;
*.sh: ;
reversebytes.S: ;
callfunc.S: ;
setjmp.S: ;
