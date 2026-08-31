# This file is included from loader/*/Makefile
topdir=../../

include $(topdir)makefile_cam.inc

LDOPTS=-nostdlib -Wl,--allow-shlib-undefined -Wl,-EL,-T,$(tools)/link-boot.ld -Wl,-N,-Ttext,$(MEMBASEADDR)

CORE_FILE=$(camfw)/main.bin

CFLAGS+=-I. -I$(camfw) -DCORE_FILE=\"$(CORE_FILE)\"

ifdef OPT_DISABLE_COMPAT_CHECK
CFLAGS+=-DOPT_DISABLE_COMPAT_CHECK=$(OPT_DISABLE_COMPAT_CHECK)
endif

ifdef NEED_ENCODED_DISKBOOT
CFLAGS+=-DNEED_ENCODED_DISKBOOT=$(NEED_ENCODED_DISKBOOT)
endif

all: main.bin

main.bin: main.elf
main.elf: entry.o main.o blobs.o
blobs.o: $(CORE_FILE)

blobs.o: ../generic/blobs.S
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<

clean:
	rm -f *.o *.bin *.elf *.dump *.elf.syms

distclean: distclean-recursive
	rm -f *.o *.bin *.elf *.dump *.elf.syms

include $(topdir)bottom.inc


# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
../makefile_loader.inc: ;
entry.S: ;
