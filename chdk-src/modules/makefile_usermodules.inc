# User-defined modules. Picked up by the -include at the top of modules/Makefile,
# which exists precisely so additions like this do not have to edit that file.
#
# gbc.flt - Game Boy / Game Boy Color player, including the Game Boy Camera
# cartridge wired to the live viewport. See modules/gbc/ and docs/GBC_PORT.md.

FLTS+=$Ogbc.flt $Obendpic.flt

$Obendpic.flt: $Obend_picture.o $(shell $(CC) -print-libgcc-file-name)

# The vendored koenk/gbc core is written against a hosted C library. gbc_port.h
# supplies the handful of pieces it actually uses (fixed-width types, ucalloc,
# an assert that unwinds rather than aborts) and is force-included ahead of
# every translation unit, so the vendored sources keep their upstream shape and
# stay diffable against ee869fa.
#
# GBC_CHDK_PORT compiles out the save-state serialiser in core/state.c, which
# assumes a resident mem_ROM this port deliberately does not have.
GBC_DIR=gbc
GBC_CFLAGS=-I$(GBC_DIR) -I$(GBC_DIR)/core -include $(GBC_DIR)/gbc_port.h -DGBC_CHDK_PORT

# Speed flags, overriding the tree-wide ones from arm_rules.inc:
#
#   CFLAGS+=-fno-inline -Os -fno-strict-aliasing -fno-schedule-insns2
#
# Those exist because CHDK proper is fighting for room in a 0x32000 ARAM image
# and because parts of it depend on predictable codegen next to Canon's
# firmware. Neither applies to a vendored emulator that is loaded into RAM at
# runtime and never patched into a ROM, and for an instruction-decode loop
# -fno-inline plus optimise-for-size is close to the worst combination
# available.
#
# -fno-strict-aliasing is deliberately kept: gb_state type-puns its register
# file through a union, and dropping it would be inviting a miscompile that
# only shows up in one opcode.
#
# -O3 over -O2: this is an interpreter whose inner loop is a switch over 256
# opcodes and a rasteriser that is nested pixel loops, both of which are what
# inlining and scheduling are for. The extra code size lands in the module's
# runtime allocation, not the 0x32000 ARAM image.
#
# -funroll-loops stays, and the reasoning is worth recording because the
# obvious argument against it turns out to be wrong.
#
#     flags                core .text   x86 ns/insn   ARM ns/insn (qemu)
#     -O3 -funroll-loops   19804        15.6          168.8 min / 172.6 med
#     -O3                  18204        15.8          184.7 min / 187.0 med
#     -O2                  15396        16.5          183.1
#
# The ARM column is the emulator cross-built for armv5te/arm946e-s and run
# under qemu-arm, seven runs each; the gap is larger than either run-to-run
# spread, so the 8% is real and not noise.
#
# It was removed at one point on the theory that unrolling only bloats an
# interpreter - the inner loop is a 256-way switch with no trip count - and
# that 1600 extra bytes of the hottest code would cost more in instruction
# cache on an ARM946E-S than it saved. Plausible, and wrong: the rasteriser and
# the sensor pipeline are full of countable pixel loops, and unrolling those
# removes enough loop control to be worth 8% of total executed work.
#
# The one thing still not measured is the cache effect itself. qemu models
# instruction counts, not caches, so it scores exactly the axis that favours
# unrolling and is blind to the one that opposes it. But an unmeasured
# hypothesis does not outrank two measurements, so unrolling stays until
# somebody measures the body itself.
#
# These come after $(CFLAGS) on the compile line so they win.
GBC_SPEED=-O3 -finline-functions -funroll-loops -fschedule-insns2 -fno-strict-aliasing

# Port glue.
$Ogbc_%.o: $(GBC_DIR)/gbc_%.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) $(GBC_CFLAGS) $(GBC_SPEED) -nostdinc -c -o $@ $<

$Ogui_gbc.o: $(GBC_DIR)/gui_gbc.c
	@echo $< '->' $@
	$(CC) $(CFLAGS) $(GBC_CFLAGS) $(GBC_SPEED) -nostdinc -c -o $@ $<

# Vendored emulator core. Prefixed gbccore_ so these object files cannot
# collide with anything in modules/ proper - there is already a state.c-shaped
# hole in a few CHDK trees.
#
# Built in ARM mode, not Thumb. The core is written as very large switches -
# the instruction decoder, the MMU's address decode, the ROM header tables -
# and at the optimisation level CHDK builds with, gcc turns those into jump
# tables reached through the __gnu_thumb1_case_* helpers. Those live in libgcc,
# which modules do not link against, so the failure surfaces as
# "elf2flt unknown symbol: '__gnu_thumb1_case_shi'" with nothing pointing at
# the cause. ARM mode emits the tables inline.
#
# Worth knowing if this is ported to another body: the symptom only appears in
# an optimised build. Compiling these files by hand at -O0 to check them will
# not reproduce it.
#
# The cost is roughly a third more code size for the core, which lands in the
# module's runtime allocation rather than the 0x32000 ARAM image. The port glue
# in gbc/*.c stays in Thumb - it was checked at -Os and needs no helpers.
$Ogbccore_unity.o: $(GBC_DIR)/core/gbccore_unity.c
	@echo $< '->' $@ '(arm, unity)'
	$(CC) $(CFLAGS) $(GBC_CFLAGS) $(GBC_SPEED) -marm -nostdinc -c -o $@ $<

# core/emu.c, debugger.c and disassembler.c are deliberately absent: emu.c is
# savestates and hosted-stdio ROM loading (its three useful functions are
# restated in gbc_emu.c), and the other two are development tools that pull in
# readline.
# One object for the whole emulator core: core/gbccore_unity.c #includes
# state.c, mmu.c, lcd.c and cpu.c and carries the step loop, so gcc can inline
# mmu_read into the instruction decoder rather than reaching it through a
# -mlong-calls indirect. cpu.c/mmu.c/lcd.c/state.c are therefore NOT compiled
# separately - doing so would duplicate every symbol in them.
$Ogbc.flt: $Ogui_gbc.o $Ogbc_emu.o $Ogbc_rom.o $Ogbc_cam.o $Ogbc_feed.o \
           $Ogbc_save.o $Ogbccore_unity.o
