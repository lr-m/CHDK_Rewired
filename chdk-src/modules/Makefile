topdir=../

include $(topdir)makefile_modules.inc

SUBDIRS=$(llua) $(lubasic) $(larmutil) $(llang) $(lfont) $(lcore) $(llibc)

CFLAGS+=-fno-reorder-functions -I$(llua)

LDOPTS=-nostdlib -Wl,--allow-shlib-undefined -Wl,-T,$(tools)/link-boot.ld -Wl,-N,-Ttext,0x24 -r -Wl,-d

# Only build modules files here. Platform independant code to be linked to core CHDK should be in libraries (above)
# to avoid 'long_call' overhead
FLTS=$Ofselect.flt $Oedgeovr.flt $Ocurves.flt $Ompopup.flt $Opalette.flt $Ogrids.flt $Omdetect.flt $Ozebra.flt \
     $O_dng.flt $Obenchm.flt $Ocalend.flt $O4wins.flt $Omastmind.flt $Oreversi.flt $Osokoban.flt $O_rawop.flt \
     $Otxtread.flt $Omemview.flt $Omodinsp.flt $Otetris.flt $Osnake.flt $Osudoku.flt $O_tbox.flt $O_osd_le.flt \
     $Oexmemins.flt $Ofshelter.flt $Ofwcrc.flt \
     $O_hexbox.flt $Oubasic.flt $Olua.flt $Ocpuinfo.flt $Ouseredit.flt $Oeyefi.flt $Ohisto.flt $Oshothist.flt \
     $Oscrdump.flt $O_confmig.flt $Orawevhst.flt $Obootimg.flt

# Include optional rules for user defined modules
# Can be used to add additional modules to FLTS and add the rules to build them
-include makefile_usermodules.inc

all: $(FLTS)

$O%.flt: $(llibc)/$Olibc.a
	@echo '->' $@
	mkdir -p $O
	$(CC) $(CFLAGS) -o $O$*.elf -Wl,--start-group $^ -Wl,--end-group $(LDOPTS)
    ifdef OPT_GEN_MODULE_DUMPS
		$(OBJDUMP) -j .text -j .data -j .bss -z -d -r -f -h $O$*.elf >$O$*.elf.dump
		$(tools)/elf2flt/elf2flt$(EXE) $O$*.elf $@ -e -f -h -r -s -v -S -iexportlist.inc -!$(tools)/elf2flt/stoplist.txt >$@.dump
    else
		$(tools)/elf2flt/elf2flt$(EXE) $O$*.elf $@ -iexportlist.inc -!$(tools)/elf2flt/stoplist.txt > $(DEVNULL)
    endif

$O%.o: games/%.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<

$Onothumb.o: nothumb.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) -marm -nostdinc -c -o $@ $<

$Oreversebytes.o: reversebytes.S
	@echo $< '->' $@
	$(CC) $(CFLAGS) -marm -nostdinc -c -o $@ $<

$Ocalend.flt: $Ogui_calendar.o
$O_tbox.flt: $Ogui_tbox.o
$O_hexbox.flt: $Ogui_hexbox.o
$Obenchm.flt: $Ogui_bench.o
$Omemview.flt: $Ogui_debug.o
$Opalette.flt: $Ogui_palette.o
$Ompopup.flt: $Ogui_mpopup.o
$Otxtread.flt: $Ogui_read.o
$Oedgeovr.flt: $Oedgeoverlay.o $Obitvector.o
$Ozebra.flt: $Ozebra.o
$Ohisto.flt: $Ohistogram.o
$Oshothist.flt: $Oshot_histogram.o
$Ocurves.flt: $Ocurves.o
$Omdetect.flt: $Omotion_detector.o
$Ogrids.flt: $Ogui_grid.o
$Omodinsp.flt: $Omodule_inspector.o
$O_dng.flt: $Odng.o $Oreversebytes.o
$O_rawop.flt: $Oraw_merge.o $Oreversebytes.o
$O_osd_le.flt: $Ogui_osd_edit.o
$Ofselect.flt: $Ogui_fselect.o $Onothumb.o
$Ouseredit.flt: $Ouser_menu_edit.o
$Ocpuinfo.flt: $Ocpuinfo.o
$Oexmemins.flt: $Oexmem_inspector.o
$Ofshelter.flt: $Ofile_shelter.o
$Oeyefi.flt: $Oeyefi.o $Omd5.o $Osha1.o
$Ofwcrc.flt: $Ofirmware_crc.o
$Oscrdump.flt: $Oscrdump.o
$O_confmig.flt: $Oconf_migrate.o
$Orawevhst.flt: $Oraw_ev_histo.o
$Obootimg.flt: $Oboot_image.o $(shell $(CC) -print-libgcc-file-name)

$Oubasic.flt: $Oubasic.o $Oscript_key_funcs.o $(lubasic)/$Olibubasic.a
$Olua.flt: $Oluascript.o $Oscript_key_funcs.o $Oscript_shoot_hook.o $Ocordic_math.o $Odbg_dump.o $Orawhookops.o $(llua)/$Oliblua.a

$O4wins.flt: $Ogui_4wins.o
$Omastmind.flt: $Ogui_mastermind.o
$Oreversi.flt: $Ogui_reversi.o
$Otetris.flt: $Ogui_tetris.o
$Osnake.flt: $Ogui_snake.o
$Osokoban.flt: $Ogui_sokoban.o
$Osudoku.flt: $Ogui_sudoku.o

$Ogui_calendar.o: gui_calendar.c simple_module.c
$Ogui_bench.o: gui_bench.c simple_module.c
$Oeyefi.o: eyefi.c simple_module.c

$Ogui_4wins.o: games/gui_4wins.c games/simple_game.c
$Ogui_mastermind.o: games/gui_mastermind.c games/simple_game.c
$Ogui_reversi.o: games/gui_reversi.c games/simple_game.c
$Ogui_tetris.o: games/gui_tetris.c games/simple_game.c
$Ogui_snake.o: games/gui_snake.c games/simple_game.c

$Ocpuinfo.o: simple_module.c cpuinfo_v5.c cpuinfo_v7.c

$Oshot_histogram.o: shot_histogram.c shot_histogram.h
$Oraw_ev_histo.o: raw_ev_histo.c raw_ev_histo.h

clean:
	rm -f $O*.* *.o *.flt *.dump
	rm -f $(core)/*.o $(core)/main.bin* $(core)/main.elf*

distclean: clean
#	rm -f *.flt

include $(topdir)bottom.inc


# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
games/*.c: ;
