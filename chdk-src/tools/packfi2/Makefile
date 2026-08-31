topdir=../../

include $(topdir)makefile_tools.inc

OBJS=fi2enc.o aes128.o compress.o deflate.o crc32.o zutil.o adler32.o trees.o

all: fi2enc$(EXE)

fi2enc$(EXE): $(OBJS)
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

clean:
	rm -f fi2enc$(EXE) $(OBJS)

distclean: clean

include $(topdir)bottom.inc

