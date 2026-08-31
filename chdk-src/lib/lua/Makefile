topdir=../../

include $(topdir)makefile_modules.inc

OBJS = $Osetjmp.o \
       $Olapi.o $Olcode.o $Oldebug.o $Oldo.o $Oldump.o $Olfunc.o $Olgc.o $Ollex.o $Olmem.o \
       $Olobject.o $Olopcodes.o $Olparser.o $Olstate.o $Olstring.o $Oltable.o $Oltm.o \
       $Olundump.o $Olvm.o $Olzio.o \
       $Olauxlib.o $Olbaselib.o $Oldblib.o $Oltablib.o $Olmathlib.o $Olimathlib.o $Olfmathlib.o \
	   $Oliolib.o $Oloslib.o $Olstrlib.o $Oloadlib.o $Olinit.o

all: $Oliblua.a

$Oliblua.a: $(OBJS)

check:

clean:
	rm -f $O*.* *.o *.a

distclean: clean
	rm -f $(OBJS:.o=.d)

include $(topdir)bottom.inc
