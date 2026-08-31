# This file is included from platform/*/Makefile
topdir=../../

include $(topdir)makefile_cam.inc

# Change CPU architecture to ARM V4 if building with GCC EABI
# Otherwise incorrect wrapper calls to firmware functions are generated
# not needed for DIGIC 6
ifndef THUMB_FW
    ARCHFLAGS+=-march=armv4t
endif

ifdef THUMB_FW
    PALETTE_FILES=../generic/palette.c
else
    PALETTE_FILES=platform_palette.c platform_palette.h
endif

OBJS=main.o lib.o wrappers.o kbd.o shooting.o platform_palette.o

libplatform.a: $(OBJS)

all: libplatform.a

kbd.o: kbd.c
lib.o: lib.c
main.o: main.c
shooting.o: shooting.c ../generic/shooting.c
wrappers.o: wrappers.c ../generic/wrappers.c

platform_palette.o: $(PALETTE_FILES)
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<


clean:
	rm -f *.o *.a

distclean: clean
	rm -f $(OBJS:.o=.d)

include $(topdir)bottom.inc


# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
../makefile_platform.inc: ;
../generic/*.c: ;
platform_palette.h: ;
