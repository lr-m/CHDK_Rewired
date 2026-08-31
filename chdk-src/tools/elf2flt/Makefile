topdir=../../

include $(topdir)makefile_tools.inc

HOSTCFLAGS+=-Wno-format

all: elf2flt$(EXE) fltdump$(EXE)

elf2flt$(EXE): myio.o elf-arm.o elfflt.o main.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

fltdump$(EXE): fltdump.o myio.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

clean:
	rm -f *.o elf2flt$(EXE) fltdump$(EXE)

distclean: clean

include $(topdir)bottom.inc

