# OS environment config

ESED = sed -r

# keep sort order consistent
export LC_ALL=C

ifndef OSTYPE
  HOSTPLATFORM:=$(patsubst MINGW%,MINGW,$(shell uname -s))
  ISCYGWIN:=$(patsubst CYGWIN%,CYGWIN,$(shell uname -s))
  ifeq ($(HOSTPLATFORM),MINGW)
    OSTYPE = Windows
    EXE = .exe
    SH = sh
# avoid creating NUL files on some MSYS systems
    ifeq ($(shell test -e /dev/null && echo yes),yes)
        DEVNULL = /dev/null
    else
        DEVNULL = NUL
    endif
	OLDSEPARATOR = \\\\
	NEWSEPARATOR = /
	SORT := $(dir $(shell which uniq.exe | sed 's_$(OLDSEPARATOR)_$(NEWSEPARATOR)_g'))/sort.exe
  else
    ifeq ($(HOSTPLATFORM),Linux)
      OSTYPE = Linux
      EXE =
      SH =
      DEVNULL = /dev/null
      SORT = sort
    else
      ifeq ($(HOSTPLATFORM),Darwin)
        OSTYPE = Darwin
        EXE =
        SH =
        DEVNULL = /dev/null
        SORT = sort
        ESED = sed -E
      else
        ifeq ($(ISCYGWIN),CYGWIN)
          OSTYPE = Windows
          EXE = .exe
          SH = sh
          DEVNULL = /dev/null
          OLDSEPARATOR = \\\\
          NEWSEPARATOR = /
          SORT := $(dir $(shell which uniq.exe | sed 's_$(OLDSEPARATOR)_$(NEWSEPARATOR)_g'))/sort.exe
        else
          OSTYPE = Other
          EXE =
          SH =
          DEVNULL = /dev/null
          SORT = sort
        endif
      endif
    endif
  endif
else
  EXE =
  SH =
  DEVNULL = /dev/null
  SORT = sort
endif

# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
$(topdir)makefile_env.inc: ;
