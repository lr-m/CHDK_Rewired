topdir=../../

include $(topdir)makefile_modules.inc

all: ubasic_text$(EXE)

# override on command line when building
ifndef PLATFORM
PLATFORM=g12
endif
ifndef PLATFORMSUB
PLATFORMSUB=100c
endif
ifndef ROMBASEADDR
ROMBASEADDR=0xFF810000
endif
ifndef MAXRAMADDR
MAXRAMADDR=0x07FFFFFF
endif
ifndef MEMISOSIZE
MEMISOSIZE=0x20000
endif
ifndef DIGIC
DIGIC=40
endif

UBASIC_TEST_CFLAGS=-Wall -DUBASIC_TEST -DCAMERA_$(PLATFORM) -DPLATFORMID=$(PLATFORMID) -I$(cam) -I$(include) -I$(modules) -I$(core) -I.
UBASIC_TEST_CFLAGS+=-DPLATFORM=\"$(PLATFORM)\" -DPLATFORMSUB=\"$(PLATFORMSUB)\" -DROMBASEADDR=$(ROMBASEADDR) -DMAXRAMADDR=$(MAXRAMADDR) -DMEMISOSIZE=$(MEMISOSIZE) -DCAM_DIGIC=$(DIGIC)
UBASIC_TEST_CFLAGS+=-DHDK_VERSION=\"$(VER)\" -DBUILD_NUMBER=\"$(BUILD_NUMBER)\" -DBUILD_SVNREV=\"$(BUILD_SVNREV)\"
UBASIC_TEST_CFLAGS+=-Wno-gnu-designator -lm
ifdef UBASIC_DEBUG
	UBASIC_TEST_CFLAGS+=-DDEBUG=1
endif

ubasic_text$(EXE):
	gcc $(UBASIC_TEST_CFLAGS) -o ubasic_test$(EXE) camera_functions.c $(topdir)lib/ubasic/tokenizer.c $(topdir)lib/ubasic/ubasic.c run-ubasic.c

clean:
	rm -f *.o ubasic_test$(EXE)

distclean: clean

include $(topdir)bottom.inc
