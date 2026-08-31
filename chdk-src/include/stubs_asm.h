// Common definitions for stubs_entry.S and stubs_entry_2.S macros

    .text

#define DEF(n,a) \
    .globl n; n = a

#ifndef THUMB_FW

#if defined(OPT_FIRMWARE_PC24_CALL)

#define NHSTUB(name, addr)\
    .globl _##name ;\
    .weak _##name ;\
    _##name = ## addr

#define STUB(addr)\
    .globl sub_ ## addr ;\
    sub_ ## addr = 0x ## addr

#else

#define NHSTUB(name, addr)\
    .globl _##name ;\
    .weak _##name ;\
    _##name: ;\
	ldr  pc, = ## addr

#define STUB(addr)\
    .globl sub_ ## addr ;\
    sub_ ## addr: ;\
	ldr  pc, =0x ## addr

#endif

#else // DIGIC 6

// macros for ARMv7 Thumb2

.syntax unified

#define NHSTUB(name, addr)\
    .globl _##name ;\
    .weak _##name ;\
    _##name: ;\
    .code 16 ;\
    ldr.w pc, [pc, #0] ;\
    .long addr

// for ExecuteEventProcedure...
#define NHSTUB2(name, addr)\
    .globl _##name ;\
    .weak _##name ;\
    _##name: ;\
    .code 32 ;\
    ldr pc, [pc, #-4] ;\
    .code 16 ;\
    .long addr

#define STUB(addr)\
    .globl sub_ ## addr ;\
    sub_ ## addr: ;\
    .code 16 ;\
    ldr.w pc, [pc, #0] ;\
    .long 0x ## addr | 1

// to be used with 'blx <label>' instructions found in thumb fw routines
#define STUB2(addr)\
    .globl sub_ ## addr ;\
    sub_ ## addr: ;\
    .code 32 ;\
    ldr   pc, [pc, #-4] ;\
    .code 16 ;\
    .long 0x ## addr

#endif // DIGIC 6

// allocate space for variables that are not found and CHDK may alter them
#define FAKEDEF(n, m) \
    .globl n; n: ;\
    .rept m ; \
    .long 0 ; \
    .endr

// Force finsig to ignore firmware version of a function - used when a custom
// version is provided that completely replaces firmware code.
//  e.g. IGNORE(MoveFocusLensToDistance) for a410 - alternate function supplied in focushack.c
#define IGNORE(name)

// Enable outputting constants from stubs_entry.S
#define DEF_CONST(n, m) \
    .globl n; n: ;\
    .long m
