topdir=../../

include $(topdir)makefile_modules.inc

OBJS = $Otokenizer.o $Oubasic.o

all: $Olibubasic.a

$Olibubasic.a: $(OBJS)

clean:
	rm -f $O*.* *.o *.a

distclean: clean
	rm -f $(OBJS:.o=.d)

include $(topdir)bottom.inc
