/*
Arm cache control functions
TODO
Only valid on processors with cp15 (e.g. not s1)
*/
#ifdef THUMB_FW
// ARMv7 cache control (based on U-BOOT cache_v7.c, utils.h, armv7.h)

/* Invalidate entire I-cache and branch predictor array */
void __attribute__((naked,noinline)) icache_flush_all(void)
{
    /*
     * Invalidate all instruction caches to PoU.
     * Also flushes branch target cache.
     */
    asm volatile (
        "mov    r1, #0\n"
        "mcr    p15, 0, r1, c7, c5, 0\n"
        "mcr    p15, 0, r1, c7, c5, 6\n"
        "dsb    sy\n"
        "isb    sy\n"
        "bx     lr\n"
    );
}

/* Values for Ctype fields in CLIDR */
#define ARMV7_CLIDR_CTYPE_NO_CACHE      0
#define ARMV7_CLIDR_CTYPE_INSTRUCTION_ONLY  1
#define ARMV7_CLIDR_CTYPE_DATA_ONLY     2
#define ARMV7_CLIDR_CTYPE_INSTRUCTION_DATA  3
#define ARMV7_CLIDR_CTYPE_UNIFIED       4

#define ARMV7_DCACHE_INVAL_ALL      1
#define ARMV7_DCACHE_CLEAN_INVAL_ALL    2
#define ARMV7_DCACHE_INVAL_RANGE    3
#define ARMV7_DCACHE_CLEAN_INVAL_RANGE  4

#define ARMV7_CSSELR_IND_DATA_UNIFIED   0
#define ARMV7_CSSELR_IND_INSTRUCTION    1

#define CCSIDR_LINE_SIZE_OFFSET     0
#define CCSIDR_LINE_SIZE_MASK       0x7
#define CCSIDR_ASSOCIATIVITY_OFFSET 3
#define CCSIDR_ASSOCIATIVITY_MASK   (0x3FF << 3)
#define CCSIDR_NUM_SETS_OFFSET      13
#define CCSIDR_NUM_SETS_MASK        (0x7FFF << 13)

typedef unsigned int u32;
typedef int s32;

static inline s32 log_2_n_round_up(u32 n)
{
    s32 log2n = -1;
    u32 temp = n;
    
    while (temp) {
        log2n++;
        temp >>= 1;
    }
    
    if (n & (n - 1))
        return log2n + 1; /* not power of 2 - round up */
    else
        return log2n; /* power of 2 */
}

static u32 get_clidr(void)
{
    u32 clidr;

    /* Read current CP15 Cache Level ID Register */
    asm volatile ("mrc p15,1,%0,c0,c0,1" : "=r" (clidr));
    return clidr;
}

static u32 get_ccsidr(void)
{
    u32 ccsidr;

    /* Read current CP15 Cache Size ID Register */
    asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));
    return ccsidr;
}

static void set_csselr(u32 level, u32 type)
{   u32 csselr = level << 1 | type;

    /* Write to Cache Size Selection Register(CSSELR) */
    asm volatile ("mcr p15, 2, %0, c0, c0, 0" : : "r" (csselr));
}

static void v7_inval_dcache_level_setway(u32 level, u32 num_sets,
                     u32 num_ways, u32 way_shift,
                     u32 log2_line_len)
{
    int way, set, setway;

    /*
     * For optimal assembly code:
     *  a. count down
     *  b. have bigger loop inside
     */
    for (way = num_ways - 1; way >= 0 ; way--) {
        for (set = num_sets - 1; set >= 0; set--) {
            setway = (level << 1) | (set << log2_line_len) |
                 (way << way_shift);
            /* Invalidate data/unified cache line by set/way */
            asm volatile (" mcr p15, 0, %0, c7, c6, 2"
                    : : "r" (setway));
        }
    }
    /* DSB to make sure the operation is complete */
    asm volatile("dsb sy\n");
}

static void v7_clean_inval_dcache_level_setway(u32 level, u32 num_sets,
                           u32 num_ways, u32 way_shift,
                           u32 log2_line_len)
{
    int way, set, setway;

    /*
     * For optimal assembly code:
     *  a. count down
     *  b. have bigger loop inside
     */
    for (way = num_ways - 1; way >= 0 ; way--) {
        for (set = num_sets - 1; set >= 0; set--) {
            setway = (level << 1) | (set << log2_line_len) |
                 (way << way_shift);
            /*
             * Clean & Invalidate data/unified
             * cache line by set/way
             */
            asm volatile (" mcr p15, 0, %0, c7, c14, 2"
                    : : "r" (setway));
        }
    }
    /* DSB to make sure the operation is complete */
    asm volatile("dsb sy\n");
}

static void v7_maint_dcache_level_setway(u32 level, u32 operation)
{
    u32 ccsidr;
    u32 num_sets, num_ways, log2_line_len, log2_num_ways;
    u32 way_shift;

    set_csselr(level, ARMV7_CSSELR_IND_DATA_UNIFIED);

    ccsidr = get_ccsidr();

    log2_line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >>
                CCSIDR_LINE_SIZE_OFFSET) + 2;
    /* Converting from words to bytes */
    log2_line_len += 2;

    num_ways  = ((ccsidr & CCSIDR_ASSOCIATIVITY_MASK) >>
            CCSIDR_ASSOCIATIVITY_OFFSET) + 1;
    num_sets  = ((ccsidr & CCSIDR_NUM_SETS_MASK) >>
            CCSIDR_NUM_SETS_OFFSET) + 1;
    /*
     * According to ARMv7 ARM number of sets and number of ways need
     * not be a power of 2
     */
    log2_num_ways = log_2_n_round_up(num_ways);

    way_shift = (32 - log2_num_ways);
    if (operation == ARMV7_DCACHE_INVAL_ALL) {
        v7_inval_dcache_level_setway(level, num_sets, num_ways,
                      way_shift, log2_line_len);
    } else if (operation == ARMV7_DCACHE_CLEAN_INVAL_ALL) {
        v7_clean_inval_dcache_level_setway(level, num_sets, num_ways,
                           way_shift, log2_line_len);
    }
}

static void v7_maint_dcache_all(u32 operation)
{
    u32 level, cache_type, level_start_bit = 0;

    u32 clidr = get_clidr();

    for (level = 0; level < 7; level++) {
        cache_type = (clidr >> level_start_bit) & 0x7;
        if ((cache_type == ARMV7_CLIDR_CTYPE_DATA_ONLY) ||
            (cache_type == ARMV7_CLIDR_CTYPE_INSTRUCTION_DATA) ||
            (cache_type == ARMV7_CLIDR_CTYPE_UNIFIED))
            v7_maint_dcache_level_setway(level, operation);
        level_start_bit += 3;
    }
}

void dcache_clean_all(void) {
    v7_maint_dcache_all(ARMV7_DCACHE_CLEAN_INVAL_ALL);
    /* anything else? */
}

#else // ARMv5 DIGIC

/*
get cache config word
*/
void __attribute__((naked,noinline)) cache_get_config(void) {
  asm volatile (
    "MRC    p15, 0, R0,c0,c0,1\n"
    "BX     LR\n"
  );
}

/*
get dcache size from word returned by get_config
*/
unsigned dcache_get_size(unsigned config) {
    unsigned sz = (config>>18) & 0xF;
    // shouldn't happen, s1 might not have cache but also doesn't have cp15
    if(sz == 0) {
        return 0;
    }
    // per ARM DDI 0201D Table 2-5 Cache size encoding
    // starts at 4kb = 3, each subsequent value is 2x
    return 0x200 << sz;
}

/*
flush (mark as invalid) entire instruction cache
*/
void __attribute__((naked,noinline)) icache_flush_all(void) {
  asm volatile (
    "MOV    r1, #0\n"
    "MCR    p15, 0, r1,c7,c5\n"
    "BX     LR\n"
  );
}

/*
clean (write all dirty) entire data cache
also drains write buffer (like canon code)
does *not* flush
*/
void __attribute__((naked,noinline)) dcache_clean_all(void) {
  asm volatile (
    "PUSH   {LR}\n"
    "BL     cache_get_config\n"
    "BL     dcache_get_size\n"
    "CMP    r0, #0\n"
    "BEQ    3f\n"
    // index limit (max index+1)
    // per ARM DDI 0201D 4kb = bits 9:5
    "LSR    r3, r0, #2\n" 
    "MOV    r1, #0\n"
"2:\n"
    "MOV    r0, #0\n"
"1:\n"
    "ORR    r2,r1,r0\n"
    "MCR    p15, 0, r2, c7, c10, 2\n" // clean line
    "ADD    r0, r0, #0x20\n" // next line
    "CMP    r0, r3\n" // segment complete ?
    "BNE    1b\n"
    "ADD    r1, r1,#0x40000000\n" // next segment
    "CMP    r1, #0\n"
    "BNE    2b\n"
    "MCR    p15, 0, r1, c7, c10, 4\n" // drain write buffer
"3:\n"
    "POP    {LR}\n"
    "BX     LR\n"
  );
}
#endif // THUMB_FW