topdir=../../

# Makefile for platform independent 'core' library
# builds code from core and modules that only needs to be compiled once across all camera/firmware versions

include $(topdir)makefile_core.inc

OBJS = $Ogui_batt.o $Ogui_space.o $Ogui_usb.o $Ogui_mbox.o $Ogps.o $Ogui_script.o $Ogui_menu.o $Ogui_user_menu.o
OBJS+= $Osuba.o $Olevent.o $Oconsole.o $Ogps_math.o $Olive_view.o $Oaction_stack.o $Oscript.o
OBJS+= $Ousb_input.o $Ousb_module.o $Ousb_remote.o $Oautoiso.o
OBJS+= $Omodule_hashlist.o $Omodule_load.o $Omodules.o $Oversions.o

all: $Olibcore.a

$Olibcore.a: $(OBJS)

$Omodule_load.o: $(modules)/module_load.c $(modules)/module_load.h $(modules)/module_exportlist.h
$Omodule_hashlist.o: $(modules)/module_hashlist.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -fno-builtin -c -o $@ $<

$(modules)/module_hashlist.c: $(modules)/module_exportlist.c $(modules)/module_exportlist.inc $(tools)/makeexport$(EXE)
	@echo $< '->' $@
	$(tools)/makeexport$(EXE) $(modules)/module_exportlist.h $(modules)/exportlist.inc $(modules)/module_hashlist.c $(modules)/module_exportlist.c $(modules)/module_exportlist.inc

clean:
	rm -f $O*.* *.o *.a

distclean: clean
	rm -f $(OBJS:.o=.d)

include $(topdir)bottom.inc


# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
$(core)/*.c: ;
$(core)/*.h: ;
$(modules)/module_load.c: ;
$(modules)/module_exportlist.c: ;
$(modules)/*.h: ;
$(modules)/*.inc: ;
$(tools)/makeexport$(EXE): ;
