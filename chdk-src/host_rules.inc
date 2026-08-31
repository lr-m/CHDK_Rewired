# Host GCC rules for tools

HOSTCC=gcc
HOSTAR=ar
HOSTCFLAGS=-g -O2 -Wall -DBUILD_NUMBER=\"$(BUILD_NUMBER)\" -I$(modules)

ifdef OPT_WARNINGS
    HOSTCFLAGS+=-Wextra -Wunused -Wno-missing-field-initializers -Wno-strict-overflow
endif

%.o: %.c
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $<

%.o: $(modules)/%.c
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $<

%.o: $(llua)/%.c
	@echo $< '->' $@
	$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $<

.dep/%.d: %.c .dep
	$(HOSTCC) $(HOSTCFLAGS) -MM $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

.dep/%.d: $(llua)/%.c .dep
	$(HOSTCC) $(HOSTCFLAGS) -MM $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
$(topdir)host_rules.inc: ;
