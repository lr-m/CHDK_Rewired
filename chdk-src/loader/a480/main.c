#include "../generic/check_compat.c"

// static void __attribute__((noreturn)) shutdown();
// static void __attribute__((noreturn)) panic(int cnt);

extern long *blob_chdk_core;
// extern long *blob_copy_and_reset;
extern long blob_chdk_core_size;

#define EARLY_BOOT_IMAGE_SIZE 18426
typedef struct {
    unsigned marker[4];
    unsigned size;
    unsigned char jpeg[EARLY_BOOT_IMAGE_SIZE];
} early_boot_slot_t;

// Uncompressed loader data: the running importer can locate and patch this
// marker directly in dancing-bits encoded DISKBOOT.BIN.
static volatile early_boot_slot_t early_boot_slot = {
    { 0x544f4f42, 0x544f4c53, 0x30383441, 0x31474d49 },
    0,
    { [0 ... EARLY_BOOT_IMAGE_SIZE-1] = 0xa5 }
};
// extern long blob_copy_and_reset_size;

void __attribute__((noreturn)) my_restart() {
    {
    // char *dst = dst_void;
    // const char *src = src_void;
    long *dst = (long*)MEMISOSTART;
    const long *src = blob_chdk_core;
    long length = (blob_chdk_core_size + 3) >> 2;

  core_copy(src, dst, length);

    // Preserve the patched slot beyond the loader lifetime. This address is
    // above the decompressed core and below the end of the A480 AgentRAM area;
    // the startup-image task consumes it before CHDK creates the remaining
    // AgentRAM heap.
    {
        const long *slot_src = (const long *)&early_boot_slot;
        long *slot_dst = (long *)0x002fb000;
        long slot_words = (sizeof(early_boot_slot) + 3) >> 2;
        core_copy(slot_src, slot_dst, slot_words);
    }

    }

    asm volatile (
        "MRS     R0, CPSR\n"
        "BIC     R0, R0, #0x3F\n"
        "ORR     R0, R0, #0xD3\n"
        "MSR     CPSR, R0\n"
        "LDR     R1, =0xC0200000\n"
        "MVN     R0, #0\n"
        "STR     R0, [R1,#0x10C]\n"
        "STR     R0, [R1,#0xC]\n"
        "STR     R0, [R1,#0x1C]\n"
        "STR     R0, [R1,#0x2C]\n"
        "STR     R0, [R1,#0x3C]\n"
        "STR     R0, [R1,#0x4C]\n"
        "STR     R0, [R1,#0x5C]\n"
        "STR     R0, [R1,#0x6C]\n"
        "STR     R0, [R1,#0x7C]\n"
        "STR     R0, [R1,#0x8C]\n"
        "STR     R0, [R1,#0x9C]\n"
        "STR     R0, [R1,#0xAC]\n"
        "STR     R0, [R1,#0xBC]\n"
        "STR     R0, [R1,#0xCC]\n"
        "STR     R0, [R1,#0xDC]\n"
        "STR     R0, [R1,#0xEC]\n"
        "CMP     R4, #7\n"
        "STR     R0, [R1,#0xFC]\n"
        "LDMEQFD SP!, {R4,PC}\n"
        "MOV     R0, #0x78\n"
        "MCR     p15, 0, R0,c1,c0\n"
        "MOV     R0, #0\n"
        "MCR     p15, 0, R0,c7,c10, 4\n"
        "MCR     p15, 0, R0,c7,c5\n"
        "MCR     p15, 0, R0,c7,c6\n"
        "MOV     R0, #0x40000006\n"
        "MCR     p15, 0, R0,c9,c1\n"
        "MCR     p15, 0, R0,c9,c1, 1\n"
        "MRC     p15, 0, R0,c1,c0\n"
        "ORR     R0, R0, #0x50000\n"
        "MCR     p15, 0, R0,c1,c0\n"
        "LDR     R0, =0x12345678\n"
        "MOV     R1, #0x40000000\n"
        "STR     R0, [R1,#0xFFC]\n"
        //   "LDR     R0, =loc_FFC00000\n"   // -
        "MOV     R0, %0\n"              // new jump-vector
        "LDMFD   SP!, {R4,LR}\n"
        "BX      R0\n"

        : : "r"(MEMISOSTART) : "memory","r0","r1","r2","r3","r4");

        while(1);
}

// #define LED_PR 0xFFFF0000
// 
// 
// static void __attribute__((noreturn)) shutdown()
// {
//     volatile long *p = (void*)LED_PR;       // turned off later, so assumed to be power
//         
//     asm(
//          "MRS     R1, CPSR\n"
//          "AND     R0, R1, #0x80\n"
//          "ORR     R1, R1, #0x80\n"
//          "MSR     CPSR_cf, R1\n"
//          :::"r1","r0");
//         
//     *p = 0x44;  // led off.
// 
//     while(1);
// }
// 
// 
// static void __attribute__((noreturn)) panic(int cnt)
// {
// 	volatile long *p=(void*)LED_PR;
// 	int i;
// 
// 	for(;cnt>0;cnt--){
// 		p[0]=0x46;
// 
// 		for(i=0;i<0x200000;i++){
// 			asm ("nop\n");
// 			asm ("nop\n");
// 		}
// 		p[0]=0x44;
// 		for(i=0;i<0x200000;i++){
// 			asm ("nop\n");
// 			asm ("nop\n");
// 		}
// 	}
// 	shutdown();
// }
