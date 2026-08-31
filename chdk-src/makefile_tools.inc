# Build rules for tools

include $(topdir)makefile_base.inc
include $(topdir)host_rules.inc

# Define empty recipes for source files (including the makefiles)
# to prevent make from trying implicit rules to create them. Speeds up build process
$(topdir)makefile_tools.inc: ;
