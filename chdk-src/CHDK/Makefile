topdir=../

include $(topdir)makefile_base.inc

ifndef OPT_DE_VERSION
LOGO=logo.dat
else
LOGO=logo_de.dat
endif

GENLUA=LUALIB/GEN/propset1.lua LUALIB/GEN/propset2.lua LUALIB/GEN/propset3.lua LUALIB/GEN/propset4.lua \
       LUALIB/GEN/propset5.lua LUALIB/GEN/propset6.lua LUALIB/GEN/propset7.lua LUALIB/GEN/propset8.lua \
       LUALIB/GEN/propset9.lua LUALIB/GEN/propset10.lua LUALIB/GEN/propset11.lua LUALIB/GEN/propset12.lua \
       LUALIB/GEN/propset13.lua \
       LUALIB/GEN/modelist.lua LUALIB/GEN/cnf_core.lua LUALIB/GEN/cnf_osd.lua LUALIB/GEN/cnf_user.lua \
       LUALIB/GEN/cnf_gps.lua
       
# generate some lua files from CHDK source
all: $(GENLUA)
	@echo "copy" $(LOGO)
	mkdir -p DATA
	cp $(LOGO) DATA/$(LOGO)
#	@echo "copy modules"
	mkdir -p MODULES
#	rm -f MODULES/*
#	cp $(modules)/$O*.flt MODULES

# Rule for creating Lua propset files
LUALIB/GEN/propset%.lua:  $(include)/propset%.h
	@echo "->" $@
	sed -n -f  $(tools)/gen_propset_lua.sed < $< > $@

LUALIB/GEN/modelist.lua:  $(include)/modelist.h
	@echo "->" $@
	sed -n -f  $(tools)/gen_modelist_lua.sed < $< > $@

LUALIB/GEN/cnf_core.lua: $(core)/conf.c $(tools)/gen_conf_lua$(EXE)
	@echo "->" $@
	$(tools)/gen_conf_lua$(EXE) $(core)/conf.c conf_info 1000 > $@

LUALIB/GEN/cnf_osd.lua: $(core)/conf.c $(tools)/gen_conf_lua$(EXE)
	@echo "->" $@
	$(tools)/gen_conf_lua$(EXE) $(core)/conf.c osd_conf_info 2000 > $@

LUALIB/GEN/cnf_user.lua: $(core)/conf.c $(tools)/gen_conf_lua$(EXE)
	@echo "->" $@
	$(tools)/gen_conf_lua$(EXE) $(core)/conf.c user_menu_conf_info 3000 > $@

LUALIB/GEN/cnf_gps.lua: $(core)/conf.c $(tools)/gen_conf_lua$(EXE)
	@echo "->" $@
	$(tools)/gen_conf_lua$(EXE) $(core)/conf.c gps_conf_info 4000 > $@

clean:
	rm -f $(GENLUA)
	rm -f DATA/$(LOGO)
	rm -f MODULES/*.flt

include $(topdir)bottom.inc


# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
$(core)/conf.c: ;
$(tools)/gen_conf_lua$(EXE): ;
$(include)/*.h: ;

