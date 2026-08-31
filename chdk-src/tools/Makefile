topdir=../

include $(topdir)makefile_tools.inc

SUBDIRS=packfi2 elf2flt

EXES=makeexport$(EXE) pakwif$(EXE) finsig_vxworks$(EXE) finsig_dryos$(EXE) gensig_vxworks$(EXE) gensig_dryos$(EXE) \
     dancingbits$(EXE) font_8x16_pack$(EXE) makelang$(EXE) code_gen$(EXE) gen_conf_lua$(EXE)

ifeq ($(OPT_CAPSTONE_TOOLS),1)
EXES+=finsig_thumb2$(EXE) code_gen2$(EXE) capdis$(EXE)
endif

all: $(EXES)

# not needed by batch builds, not built by default
EXTRA_EXES=rawconvert$(EXE) yuvconvert$(EXE) find_levent$(EXE) find_eventproc$(EXE) dumpchk$(EXE) extract_error_table$(EXE)

extras: $(EXTRA_EXES)

makeexport$(EXE): makeexport.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

pakwif$(EXE): pakwif.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

finsig_vxworks$(EXE): finsig_vxworks.o stubs_load.o firmware_load.o ptp_op_names.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

gensig_vxworks$(EXE): gensig_vxworks.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

finsig_dryos$(EXE): finsig_dryos.o stubs_load.o firmware_load.o ptp_op_names.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

gensig_dryos$(EXE): gensig_dryos.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

dumpchk$(EXE): dumpchk.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

dancingbits$(EXE): dancingbits.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

rawconvert$(EXE): rawconvert.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

yuvconvert$(EXE): yuvconvert.o
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

find_levent$(EXE): find_levent.o dumputil.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

extract_error_table$(EXE): extract_error_table.o dumputil.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

find_eventproc$(EXE): find_eventproc.o dumputil.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

font_8x16_pack$(EXE): font_8x16_pack.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

makelang$(EXE): makelang.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

code_gen$(EXE): code_gen.o stubs_load.o firmware_load.o chdk_dasm.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

code_gen2$(EXE): code_gen2.o stubs_load.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

gen_conf_lua$(EXE): gen_conf_lua.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

finsig_thumb2$(EXE): finsig_thumb2.o stubs_load.o firmware_load_ng.o ptp_op_names.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^ $(CAPSTONE_TOOLS_LINK)

capdis$(EXE): capdis.o stubs_load.o firmware_load_ng.o
	@echo $^ '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^ $(CAPSTONE_TOOLS_LINK)

clean:
	rm -f *.o $(EXES) $(EXTRA_EXES)

distclean: clean


signatures_vxworks.h: gensig_vxworks$(EXE) gensigs_vxworks.sh sig_ref_vxworks_1.txt sig_ref_vxworks_1.bin sig_ref_vxworks_2.txt sig_ref_vxworks_2.bin sig_ref_vxworks_3.txt sig_ref_vxworks_3.bin
	@echo '->' $@
ifdef OPT_GEN_SIGS
	if [ -s sig_ref_vxworks_1.bin ] && [ -s sig_ref_vxworks_2.bin ] && [ -s sig_ref_vxworks_3.bin ] ; then \
	  $(SH) ./gensigs_vxworks.sh $(SORT) vxworks "1 2 3" > signatures_vxworks.h ; \
	else \
	  echo CAUTION! \'signatures_vxworks.h\' is not updated due to \'sig_ref_vxworks_1.bin\` or \'sig_ref_vxworks_2.bin\` or \'sig_ref_vxworks_3.bin\` is empty! ; \
	  touch signatures_vxworks.h ; \
	fi
else
	echo CAUTION! Signature generation is turned off! ; \
	touch signatures_vxworks.h
endif

signatures_dryos.h: gensig_dryos$(EXE) gensigs_dryos.sh sig_ref_dryos_1.txt sig_ref_dryos_1.bin sig_ref_dryos_2.txt sig_ref_dryos_2.bin sig_ref_dryos_3.txt sig_ref_dryos_3.bin \
                    sig_ref_dryos_4.txt sig_ref_dryos_4.bin sig_ref_dryos_5.txt sig_ref_dryos_5.bin sig_ref_dryos_6.txt sig_ref_dryos_6.bin
	@echo '->' $@
ifdef OPT_GEN_SIGS
	if [ -s sig_ref_dryos_1.bin ] && [ -s sig_ref_dryos_2.bin ] && [ -s sig_ref_dryos_3.bin ] && [ -s sig_ref_dryos_4.bin ] && [ -s sig_ref_dryos_5.bin ] && \
	   [ -s sig_ref_dryos_6.bin ] ; then \
	  $(SH) ./gensigs_dryos.sh $(SORT) dryos "1 2 3 4 5 6" > signatures_dryos.h ; \
	else \
	  echo CAUTION! \'signatures_dryos.h\' is not updated due to \'sig_ref_dryos_?.bin\` is empty! ; \
	  touch signatures_dryos.h ; \
	fi
else
	echo CAUTION! Signature generation is turned off! ; \
	touch signatures_dryos.h
endif

sig_ref_vxworks_1.bin:
	touch sig_ref_vxworks_1.bin

sig_ref_vxworks_2.bin:
	touch sig_ref_vxworks_2.bin

sig_ref_vxworks_3.bin:
	touch sig_ref_vxworks_3.bin

sig_ref_dryos_1.bin:
	touch sig_ref_dryos_1.bin

sig_ref_dryos_2.bin:
	touch sig_ref_dryos_2.bin

sig_ref_dryos_3.bin:
	touch sig_ref_dryos_3.bin

sig_ref_dryos_4.bin:
	touch sig_ref_dryos_4.bin

sig_ref_dryos_5.bin:
	touch sig_ref_dryos_5.bin

sig_ref_dryos_6.bin:
	touch sig_ref_dryos_6.bin

finsig_vxworks.o: finsig_vxworks.c signatures_vxworks.h stubs_load.h firmware_load.h ptp_op_names.h
finsig_dryos.o: finsig_dryos.c signatures_dryos.h stubs_load.h firmware_load.h ptp_op_names.h
stubs_load.o: stubs_load.c stubs_load.h
firmware_load.o: firmware_load.c firmware_load.h stubs_load.h
code_gen.o: code_gen.c chdk_dasm.h stubs_load.h firmware_load.h
chdk_dasm.o: chdk_dasm.c chdk_dasm.h stubs_load.h firmware_load.h
ptp_op_names.o: ptp_op_names.c ptp_op_names.h

firmware_load_ng.o: firmware_load_ng.c firmware_load_ng.h stubs_load.h
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) $(CAPSTONE_TOOLS_INC) -c -o $@ $<

finsig_thumb2.o: finsig_thumb2.c stubs_load.h firmware_load_ng.h ptp_op_names.h
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) $(CAPSTONE_TOOLS_INC) -c -o $@ $< 

capdis.o: capdis.c stubs_load.h firmware_load_ng.h
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) $(CAPSTONE_TOOLS_INC) -c -o $@ $<

font_8x16_pack.o: font_8x16_pack.c ../lib/font/font_8x16_uni.h

include $(topdir)bottom.inc

# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
chdk_dasm.h: ;
firmware_load.h: ;
firmware_load_ng.h: ;
ptp_op_names.h: ;
stubs_load.h: ;
../lib/font/font_8x16_uni.h: ;
