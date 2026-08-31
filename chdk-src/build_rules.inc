# Build Rules for CHDK code (except tools)

# rule for building .o from .c in the same directory
%.o: %.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) $(ARCHFLAGS) -nostdinc -c -o $@ $<

# rule for building .o in $O sub-directory from .c in the current directory
$O%.o: %.c
	mkdir -p $O
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<

# rule for building .o from .c in the same directory - forces THUMB build
%.thm.o: %.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) -mthumb -nostdinc -c -o $@ $<

# rule for building .o from .c in the 'core' directory
$O%.o: $(core)/%.c
	mkdir -p $O
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<

# rule for building .o from .c in the 'core' directory - forces THUMB build
%.thm.o: $(core)/%.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) -mthumb -nostdinc -c -o $@ $<

# rule for building .o from .c in the 'modules' directory
$O%.o: $(modules)/%.c
	mkdir -p $O
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<

# rule for building .o from .S in the same directory
%.o: %.S
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<

# rule for building .o in $O sub-directory from .S in the current directory
$O%.o: %.S
	mkdir -p $O
	@echo $< '->' $@
	$(CC) $(CFLAGS) -nostdinc -c -o $@ $<
	
# rule for building .a
%.a:
	@echo $^ '->' $@
	$(AR) rcs $@ $^

%.bin: %.elf
	@echo $< '->' $@
	$(OBJDUMP) -j .text -j .data -j .bss -z -d -r -f -h $< > $@.dump
	$(OBJCOPY) -O binary $< $@

%.elf:
	@echo '->' $@
	$(CC) $(CFLAGS) -o $@ $^ $(LDOPTS)
	( $(NM) $@ | grep ' U ' > $@.syms ) && exit 1 || exit 0

.dep/%.d: %.c .dep
	$(CC) $(CFLAGS) -M $< > $@.$$$$; \
	    sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	    rm -f $@.$$$$

.dep/%.d: %.S .dep
	$(CC) $(CFLAGS) -M $< > $@.$$$$; \
	    sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	    rm -f $@.$$$$

# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
$(topdir)build_rules.inc: ;
