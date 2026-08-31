topdir=../../

include $(topdir)makefile_core.inc

OBJS = $Ofont_8x16.o $Orbf_font.o

all: $Olibfont.a

$Olibfont.a: $(OBJS)

$Ofont_8x16.o: font_8x16.c font_8x16_uni_packed.h

font_8x16_uni_packed.h: font_8x16_uni.h codepages.h $(tools)/font_8x16_pack$(EXE)
	$(tools)/font_8x16_pack$(EXE) > font_8x16_uni_packed.h

clean:
	rm -f $O*.* *.o *.a

distclean: clean
	rm -f $(OBJS:.o=.d)

include $(topdir)bottom.inc


# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
font_8x16_uni.h: ;
codepages.h: ;
$(tools)/font_8x16_pack$(EXE): ;
