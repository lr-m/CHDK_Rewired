topdir=../../

include $(topdir)makefile_arm.inc

OBJS = $Ocallfunc.o $Oreboot.o $Ocache.o

OBJS_ALL = $(OBJS)

all: $Olibarmutil.a

$Olibarmutil.a: $(OBJS)

clean:
	rm -f $O*.* *.o *.a

distclean: clean
	rm -f $(OBJS_ALL:.o=.d)

include $(topdir)bottom.inc
