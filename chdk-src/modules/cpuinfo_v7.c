// cpuinfo, ARMv7 specific parts

static char linebuf[256]; // fixed buffer for outputting dynamic strings from interpreter functions

static const char *two_nth_str[] = {
"1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1K", "2K", "4K", "8K", "16K", "32K"
};

static const char *two_on_nth(unsigned val) {
    if (val < 16) {
        return two_nth_str[val];
    }
    return "invalid";
}

static const char *two_on_nth_granule(unsigned val) {
    if (val == 0) {
        return "no info";
    }
    else if (val > 9) {
        return "reserved";
    }
    else {
        return two_nth_str[val];
    }
    return "invalid";
}

const struct cpuinfo_bitfield_desc_s cpuinf_feat0[] = {
    {4,"ARM inst set",0},
    {4,"Thumb inst set",0},
    {4,"Jazelle inst set",0},
    {4,"ThumbEE inst set",0},
    {16,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_feat1[] = {
    {4,"Programmers' model",0},
    {4,"Security extensions",0},
    {4,"Microcontr. prog model",0},
    {4,"Virt. extensions",0},
    {4,"Generic timer ext.",0},
    {12,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_dbgfeat[] = {
    {4,"Coproc. dbg model",0},
    {4,"Coproc. secure dbg model",0},
    {4,"Memory-mapped dbg model",0},
    {4,"Coproc. trace model",0},
    {4,"Memory-mapped trace model",0},
    {4,"Debug model M",0},
    {4,"Perf. monitors",0},
    {4,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr0[] = {
    {4,"VMSA support",0},
    {4,"PMSA support",0},
    {4,"Cache coherence",0},
    {4,"Outer shareable",0},
    {4,"TCM support",0},
    {4,"Auxiliary registers",0},
    {4,"FCSE support",0},
    {4,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr1[] = {
    {4,"L1 Harvard cache VA",0},
    {4,"L1 unified cache VA",0},
    {4,"L1 Harvard cache s/w",0},
    {4,"L1 unified cache s/w",0},
    {4,"L1 Harvard cache",0},
    {4,"L1 unified cache",0},
    {4,"L1 cache test & clean",0},
    {4,"Branch predictor",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr2[] = {
    {4,"L1 Harvard fg prefetch",0},
    {4,"L1 Harvard bg prefetch",0},
    {4,"L1 Harvard range",0},
    {4,"Harvard TLB",0},
    {4,"Unified TLB",0},
    {4,"Mem barrier",0},
    {4,"WFI stall",0},
    {4,"HW access flag",0},
    {0}
};

static const char *mmfr3_cache(unsigned val) {
    switch (val) {
        case 0: return "Not supported";
        case 1: return "Supported";
    }
    return "(invalid)";
}

static const char *mmfr3_bp(unsigned val) {
    switch (val) {
        case 0: return "Not supported";
        case 1: return "Invalidate all";
        case 2: return "Invalidate all, invalidate by MVA";
    }
    return "(invalid)";
}

static const char *mmfr3_cms(unsigned val) {
    switch (val) {
        case 0: return "4 GByte";
        case 1: return "64 GByte";
        case 2: return "1 TByte";
    }
    return "(invalid)";
}

static const char *mmfr3_ss(unsigned val) {
    switch (val) {
        case 0: return "Supported";
        case 15: return "Not supported";
    }
    return "(invalid)";
}

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr3[] = {
    {4,"Cache maintain MVA", mmfr3_cache},
    {4,"Cache maintain set/way", mmfr3_cache},
    {4,"Branch predictor maintenance", mmfr3_bp},
    {4,"Maintenance broadcast",0},
    {4,"-",0},
    {4,"Transl. table coherent walk",0},
    {4,"Cached memory size", mmfr3_cms},
    {4,"Supersection support", mmfr3_ss},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar0[] = {
    {4,"Swap instrs",0},
    {4,"Bitcount instrs",0},
    {4,"Bitfield instrs",0},
    {4,"CmpBranch instrs",0},
    {4,"Coproc instrs",0},
    {4,"Debug instrs",0},
    {4,"Divide instrs",0},
    {4,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar1[] = {
    {4,"Endian instrs",0},
    {4,"Exception instrs",0},
    {4,"Exception AR instrs",0},
    {4,"Extend instrs",0},
    {4,"IfThen instrs",0},
    {4,"Immediate instrs",0},
    {4,"Interwork instrs",0},
    {4,"Jazelle instrs",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar2[] = {
    {4,"LoadStore instrs",0},
    {4,"Memhint instrs",0},
    {4,"MultiAccess Interruptible instructions",0},
    {4,"Mult instrs",0},
    {4,"MultS instrs",0},
    {4,"MultU instrs",0},
    {4,"PSR AR instrs",0},
    {4,"Reversal instrs",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar3[] = {
    {4,"Saturate instrs",0},
    {4,"SIMD instrs",0},
    {4,"SVC instrs",0},
    {4,"SynchPrim instrs",0},
    {4,"TabBranch instrs",0},
    {4,"ThumbCopy instrs",0},
    {4,"TrueNOP instrs",0},
    {4,"T2 Exec Env instrs",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar4[] = {
    {4,"Unprivileged instrs",0},
    {4,"WithShifts instrs",0},
    {4,"Writeback instrs",0},
    {4,"SMC instrs",0},
    {4,"Barrier instrs",0},
    {4,"SynchPrim_instrs_frac",0},
    {4,"PSR_M instrs",0},
    {4,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar5[] = {
    {32,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_ctr[] = {
    {4,"Icache min words/line", two_on_nth},
    {10,"(zero)",0},
    {2,"L1 Icache policy",0},
    {4,"Dcache min words/line", two_on_nth},
    {4,"Exclusives Reservation Granule", two_on_nth_granule},
    {4,"Cache Writeback Granule", two_on_nth_granule},
    {1,"(zero)",0},
    {3,"(register format)",0},
    {0}
};

static const char *ctype_str(unsigned val) {
    switch (val) {
        case 0: return "no cache";
        case 1: return "Icache only";
        case 2: return "Dcache only";
        case 3: return "Separate Icache, Dcache";
        case 4: return "Unified cache";
    }
    return "-";
}

const struct cpuinfo_bitfield_desc_s cpuinf_clidr[] = {
    {3,"Cache type, level1", ctype_str},
    {3,"Cache type, level2", ctype_str},
    {3,"Cache type, level3", ctype_str},
    {3,"Cache type, level4", ctype_str},
    {3,"Cache type, level5", ctype_str},
    {3,"Cache type, level6", ctype_str},
    {3,"Cache type, level7", ctype_str},
    {3,"Cache type, level8", ctype_str}, // L8 mentioned only in Cortex R4 TRM
    {3,"Level of coherency",0},
    {3,"Level of unification",0},
    {2,"(zero)",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_csselr[] = {
    {1,"Instruction, not data",0},
    {3,"Level",0},
    {28,"(unknown)",0},
    {0}
};

static const char *ccsidr_linesize(unsigned val) {
    return two_nth_str[val+2];
}

static const char *ccsidr_plusone(unsigned val) {
    sprintf(linebuf,"%i",val+1);
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_ccsidr[] = {
    {3,"Line size in words", ccsidr_linesize},
    {10,"Associativity", ccsidr_plusone},
    {15,"Number of sets", ccsidr_plusone},
    {1,"Write allocation",0},
    {1,"Read allocation",0},
    {1,"Write back",0},
    {1,"Write through",0},
    {0}
};

static const char *cache_tcm_size_str(unsigned val) {
    if (val == 0) 
        return "0";
    if (val < 3 || val > 14)
        return "invalid";
    return reg_sizes[val-3];
}

static const char *cache_tcm_addr_str(unsigned val) {
    sprintf(linebuf,"0x%08x",val<<12);
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_tcmreg[] = {
    {1,"Enabled",0},
    {1,"-",0},
    {5,"Size", cache_tcm_size_str},
    {5,"-",0},
    {20,"Base address", cache_tcm_addr_str},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mputype[] = {
    {1,"S",0},
    {7,"-",0},
    {8,"Num of MPU regions",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mpubase[] = {
    {32,"Base address",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_sctlr[] = {
    {1,"MPU Enable",0},
    {1,"Strict Align",0},
    {1,"L1 DCache Enable",0},
    {4,"- (SBO)",0},
    {4,"- (SBZ)",0},
    {1,"Branch Pred Enable",0},
    {1,"L1 ICache Enable",0},
    {1,"High Vector",0},
    {1,"Round Robin",0},
    {1,"- (SBZ)",0},
    {1,"- (SBO)",0},
    {1,"MPU background reg",0},
    {1,"- (SBO)",0},
    {1,"Div0 exception",0},
    {1,"- (SBZ)",0},
    {1,"FIQ Enable",0},
    {2,"- (SBO)",0},
    {1,"VIC",0},
    {1,"CPSR E bit",0},
    {1,"- (SBZ)",0},
    {1,"NMFI",0},
    {1,"TRE",0},
    {1,"AFE",0},
    {1,"Thumb exceptions",0},
    {1,"Endian",0},
    {0}
};

static const char *mpu_region_size_str(unsigned val) {
    if (val < 4 || val > 31)
        return "invalid";
    if (val < 11)
        return two_nth_str[val+1];
    return reg_sizes[val-11];
}

static const char *bitfield8(unsigned val) {
    linebuf[8] = 0;
    int n;
    for (n=0; n<8; n++) {
        linebuf[7-n] = (val & (1<<n))?'1':'0';
    }
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_mpusizeen[] = {
    {1,"Enabled",0},
    {5,"Size", mpu_region_size_str},
    {2,"-",0},
    {8,"Sub-regions disabled", bitfield8},
    {0}
};

static const char *mpu_rattr(unsigned val) {
    char *s="";
    char *s2="";
    char *t;
    t = (val&4)?"Shared":"Non-shared";
    if (val&0x20) {
        switch (val&3) {
            case 0: s = "Inner Non-cacheable"; break;
            case 1: s = "Inner Write-back, write-allocate"; break;
            case 2: s = "Inner Write-through, no write-allocate"; break;
            case 3: s = "Inner Write-back, no write-allocate"; break;
        }
        switch ((val&0x18)>>3) {
            case 0: s2 = "Outer Non-cacheable"; break;
            case 1: s2 = "Outer Write-back, write-allocate"; break;
            case 2: s2 = "Outer Write-through, no write-allocate"; break;
            case 3: s2 = "Outer Write-back, no write-allocate"; break;
        }
        sprintf(linebuf,"%s; %s; %s",s, s2, t);
    }
    else {
        switch (val&0x1B) {
            case 0: s = "Strongly ordered, shareable"; t=""; break;
            case 1: s = "Shareable device"; t="Shareable"; break;
            case 2: s = "Outer and Inner write-through, no write-allocate"; break;
            case 3: s = "Outer and Inner write-back, no write-allocate"; break;
            case 8: s = "Outer and Inner Non-cacheable"; break;
            case 11: s = "Outer and Inner write-back, write-allocate"; break;
            case 16: s = "Non-shareable Device"; t=""; break;
            default: s = "(reserved)"; t="";
        }
        sprintf(linebuf,"%s; %s",s, t);
    }
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_accesscontrol[] = {
    {6,"Region attributes", mpu_rattr},
    {2,"-",0},
    {3,"Access permission", regperm_str},
    {1,"-",0},
    {1,"Execute never",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_generic[] = {
    {32,"(raw value)",0},
    {0}
};

static const char * dbg_version(unsigned val) {
    switch(val) {
        case 0b0001: return "v6";
        case 0b0010: return "v6.1";
        case 0b0011: return "v7 full";
        case 0b0100: return "v7 basic";
        case 0b0101: return "v7.1";
        case 0b0110: return "v8";
        case 0b0111: return "v8.1";
        case 0b1000: return "v8.2";
    }
    return "???";
}

static const struct cpuinfo_bitfield_desc_s cpuinf_dbgdidr[] = {
    {4,"Revision",0},
    {4,"Variant",0},
    {8,"- (RAZ)",0},
    {4,"Version",dbg_version},
    {4,"Context",ccsidr_plusone},
    {4,"BRP",ccsidr_plusone},
    {4,"WRP",ccsidr_plusone},
    {0}
};

static const struct cpuinfo_bitfield_desc_s cpuinf_dbgd_address[] = {
    {2,"Valid",0},
    {10,"- (UNK)",0},
    {20,"Address",cache_tcm_addr_str},
    {0}
};

static const struct cpuinfo_bitfield_desc_s cpuinf_dbgdscr[] = {
    {1,"HALTED",0},
    {1,"RESTARTED",0},
    {4,"MOE",0},
    {1,"SDABORT_l",0},
    {1,"ADABORT_l",0},
    {1,"UND_l",0},
    {1,"FS",0},
    {1,"DBGack",0},
    {1,"INTdis",0},
    {1,"UDCCdis",0},
    {1,"ITRen",0},
    {1,"HDBGen",0},
    {1,"MDBGen",0},
    {1,"SPIDdis",0},
    {1,"SPNIDdis",0},
    {1,"NS",0},
    {1,"ADAdiscard",0},
    {2,"ExtDCCmode",0},
    {2,"- (SBZ)",0},
    {1,"InstrCompl_l",0},
    {1,"PipeAdv",0},
    {1,"TXfull_l",0},
    {1,"RXfull_l",0},
    {1,"- (SBZ)",0},
    {1,"TXfull",0},
    {1,"RXfull",0},
    {1,"- (SBZ)",0},
    {0}
};

const struct cpuinfo_word_desc_s cpuinfo_desc_pmsa[]={
    {"ID", cpuinf_id },
    {"Cache type", cpuinf_ctr },
    {"TCM type", cpuinf_generic },
    {"MPU type", cpuinf_mputype },
    {"Multiprocessor ID", cpuinf_generic },
    {"Processor feature 0", cpuinf_feat0 },
    {"Processor feature 1", cpuinf_feat1 },
    {"Debug feature", cpuinf_dbgfeat },
    {"Aux feature", cpuinf_generic },
    {"Mem model feature 0", cpuinf_mmfr0 },
    {"Mem model feature 1", cpuinf_mmfr1 },
    {"Mem model feature 2", cpuinf_mmfr2 },
    {"Mem model feature 3", cpuinf_mmfr3 },
    {"ISA feature 0", cpuinf_isar0 },
    {"ISA feature 1", cpuinf_isar1 },
    {"ISA feature 2", cpuinf_isar2 },
    {"ISA feature 3", cpuinf_isar3 },
    {"ISA feature 4", cpuinf_isar4 },
    {"ISA feature 5", cpuinf_isar5 },
    {"Cache level ID", cpuinf_clidr },
    {"Cache size ID reg (data, level0)", cpuinf_ccsidr },
    {"Cache size ID reg (inst, level0)", cpuinf_ccsidr },
    {"SCTLR", cpuinf_sctlr },
    {"ACTLR", cpuinf_generic },
    {"ACTLR2", cpuinf_generic },
    {"CPACR", cpuinf_generic },
    {"Build options 1", cpuinf_generic },
    {"Build options 2", cpuinf_generic },
    {"ATCM region reg", cpuinf_tcmreg },
    {"BTCM region reg", cpuinf_tcmreg },
    {"MPU region 0 base", cpuinf_mpubase },
    {"MPU region 0 size & enable", cpuinf_mpusizeen },
    {"MPU region 0 access control", cpuinf_accesscontrol },
    {"MPU region 1 base", cpuinf_mpubase },
    {"MPU region 1 size & enable", cpuinf_mpusizeen },
    {"MPU region 1 access control", cpuinf_accesscontrol },
    {"MPU region 2 base", cpuinf_mpubase },
    {"MPU region 2 size & enable", cpuinf_mpusizeen },
    {"MPU region 2 access control", cpuinf_accesscontrol },
    {"MPU region 3 base", cpuinf_mpubase },
    {"MPU region 3 size & enable", cpuinf_mpusizeen },
    {"MPU region 3 access control", cpuinf_accesscontrol },
    {"MPU region 4 base", cpuinf_mpubase },
    {"MPU region 4 size & enable", cpuinf_mpusizeen },
    {"MPU region 4 access control", cpuinf_accesscontrol },
    {"MPU region 5 base", cpuinf_mpubase },
    {"MPU region 5 size & enable", cpuinf_mpusizeen },
    {"MPU region 5 access control", cpuinf_accesscontrol },
    {"MPU region 6 base", cpuinf_mpubase },
    {"MPU region 6 size & enable", cpuinf_mpusizeen },
    {"MPU region 6 access control", cpuinf_accesscontrol },
    {"MPU region 7 base", cpuinf_mpubase },
    {"MPU region 7 size & enable", cpuinf_mpusizeen },
    {"MPU region 7 access control", cpuinf_accesscontrol },
    {"DBGDIDR", cpuinf_dbgdidr },
    {"DBGDRAR", cpuinf_dbgd_address },
    {"DBGDSAR", cpuinf_dbgd_address },
    {"DBGDSCR", cpuinf_dbgdscr },
    //{"Floating Point System ID register", cpuinf_generic },
    //{"Media and VFP Feature Register 0", cpuinf_generic },
    //{"Media and VFP Feature Register 1", cpuinf_generic },
    {0}
};


void __attribute__((naked,noinline)) cpuinfo_get_info_pmsa(__attribute__ ((unused))unsigned *results) {
    asm (
        ".syntax unified\n"
        ".code 16\n"
        ".align 2\n"
        "BX      PC\n"                  // switch to ARM mode
        ".code 32\n"

        "MRC    p15, 0, R1,c0,c0\n" // ident
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,1\n" // cache
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,2\n" // TCM
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,4\n" // MPU
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,5\n" // MPIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,0\n" // ID_PFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,1\n" // ID_PFR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,2\n" // ID_DFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,3\n" // ID_AFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,4\n" // ID_MMFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,5\n" // ID_MMFR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,6\n" // ID_MMFR2
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,7\n" // ID_MMFR3
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,0\n" // ID_ISAR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,1\n" // ID_ISAR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,2\n" // ID_ISAR2
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,3\n" // ID_ISAR3
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,4\n" // ID_ISAR4
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,5\n" // ID_ISAR5
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 1, R1,c0,c0,1\n" // CLIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #0\n"
        "MCR    p15, 2, R1,c0,c0,0\n" // CSSELR (data cache, level0)

        "MRC    p15, 1, R1,c0,c0,0\n" // CCSIDR (the currently selected one)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #1\n"
        "MCR    p15, 2, R1,c0,c0,0\n" // CSSELR (inst cache, level0)

        "MRC    p15, 1, R1,c0,c0,0\n" // CCSIDR (the currently selected one)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c1,c0,0\n" // SCTLR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c1,c0,1\n" // ACTLR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

//#ifndef CONFIG_QEMU
        "MRC    p15, 0, R1,c15,c0,0\n" // ACTLR2
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
//#endif

        "MRC    p15, 0, R1,c1,c0,2\n" // CPACR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c15,c2,0\n" // Build options 1 reg (from Cortex R4 TRM)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c15,c2,1\n" // Build options 2 reg (from Cortex R4 TRM)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c9,c1,1\n" // ATCM region reg
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c9,c1,0\n" // BTCM region reg
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #0\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // MPU Memory Region Number Register, region 0

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #1\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 1

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #2\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 2

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #3\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 3

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #4\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 4

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #5\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 5

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #6\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 6

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #7\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 7

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c0,c0,0\n" // DBGDIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c1,c0,0\n" // DBGDRAR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c2,c0,0\n" // DBGDSAR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c0,c1,0\n" // DBGDSCR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        //".word  0xEEF01A10\n" //"VMRS   R1, FPSID\n" // Floating Point System ID register
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        //".word  0xEEF71A10\n" //"VMRS   R1, MVFR0\n" // Media and VFP Feature Register 0
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        //".word  0xEEF61A10\n" //"VMRS   R1, MVFR1\n" // Media and VFP Feature Register 1
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        "BX     LR\n"

        :::"r0","r1"
    );
}

static const char * tlb_unified(unsigned val) {
    switch(val) {
        case 0: return "Unified TLB";
        case 1: return "Separate data and instruction TLB";
    }
    return "???";
}

static const char * tlb_entries(unsigned val) {
    switch(val) {
        case 0: return "64";
        case 1: return "128";
        case 2: return "256";
        case 3: return "512";
    }
    return "???";
}

const struct cpuinfo_bitfield_desc_s cpuinf_tlbtype[] = {
    {1,"TLB", tlb_unified},
    {2,"TLB entries", tlb_entries},
    {5,"-",0},
    {8,"Lockable unified or data entries",0},
    {8,"Lockable instruction entries",0},
    {8,"(zero)",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_sctlr_vmsa[] = {
    {1,"MMU Enable",0},
    {1,"Strict Align",0},
    {1,"Data or Unified Cache Enable",0},
    {1,"CP15 Barrier Enable",0},
    {3,"- (SBO)",0},
    {3,"- (SBZ)",0},
    {1,"SWP/SWPB Enable",0},
    {1,"Branch Pred Enable",0},
    {1,"ICache Enable",0},
    {1,"High Vector",0},
    {1,"Round Robin",0},
    {1,"- (SBZ)",0},
    {1,"- (SBO)",0},
    {1,"HA flag",0},
    {1,"- (SBO)",0},
    {1,"WXN (virt. ext. only)",0},
    {1,"UWXN (virt. ext. only)",0},
    {1,"FIQ Enable",0},
    {2,"- (SBO)",0},
    {1,"VE",0},
    {1,"CPSR E",0},
    {1,"- (SBZ)",0},
    {1,"NMFI",0},
    {1,"TRE",0},
    {1,"AFE",0},
    {1,"Thumb exceptions",0},
    {1,"- (SBZ)",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_clidr_vmsa[] = {
    {3,"Cache type, level1", ctype_str},
    {3,"Cache type, level2", ctype_str},
    {3,"Cache type, level3", ctype_str},
    {3,"Cache type, level4", ctype_str},
    {3,"Cache type, level5", ctype_str},
    {3,"Cache type, level6", ctype_str},
    {3,"Cache type, level7", ctype_str},
    {3,"Level of unification Inner Shareable",0},
    {3,"Level of coherency",0},
    {3,"Level of unification",0},
    {2,"(zero)",0},
    {0}
};

static const char *ttbraddr0(unsigned val) {
    val <<= 7;
    sprintf(linebuf,"0x%08x",val);
    return linebuf;
}

static const char *ttbraddr1(unsigned val) {
    val <<= 7;
    sprintf(linebuf,"0x%08x",val);
    return linebuf;
}

static const char *ttbcr_n(unsigned val) {
    val = 128 << (7-val);
    sprintf(linebuf,"TTBR0 table size %u bytes",val);
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_ttbcr[] = {
    {3,"N", ttbcr_n },
    {1,"(zero)" ,0},
    {1,"TTBR0 walks disabled" ,0},
    {1,"TTBR1 walks disabled" ,0},
    {25,"(zero)" ,0},
    {1,"Long descriptors" ,0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_ttbr0[] = {
    {1,"IRGN[1]",0},
    {1,"Shareable" ,0},
    {1,"(impl. defined)" ,0},
    {2,"RGN (Outer cacheability)" ,0},
    {1,"NOS (Inner shareable)" ,0},
    {1,"IRGN[0]" ,0},
    {25,"Table address", ttbraddr0 },
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_ttbr1[] = {
    {1,"IRGN[1]",0},
    {1,"Shareable" ,0},
    {1,"(impl. defined)" ,0},
    {2,"RGN (Outer cacheability)" ,0},
    {1,"NOS (Inner shareable)" ,0},
    {1,"IRGN[0]" ,0},
    {25,"Table address", ttbraddr1 },
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_actlr_vmsa[] = {
    {1,"Cache & TLB maint. broadcast" ,0},
    {1,"L2 prefetch enable" ,0},
    {1,"L1 prefetch enable" ,0},
    {1,"Write full line of zeroes" ,0},
    {2,"(zero)",0},
    {1,"SMP" ,0},
    {1,"Exclusive cache" ,0},
    {1,"Alloc in one way" ,0},
    {1,"Parity on" ,0},
    {22,"-",0},
    {0}
};

const struct cpuinfo_bitfield_desc_s cpuinf_cpacr_vmsa[] = {
    {20,"(zero)",0},
    {2,"CP10 access permission",0},
    {2,"CP11 access permission",0},
    {6,"(zero)",0},
    {1,"D32DIS",0},
    {1,"ASEDIS",0},
    {0}
};

const struct cpuinfo_word_desc_s cpuinfo_desc_vmsa[]={
    {"ID", cpuinf_id },
    {"Cache type", cpuinf_ctr },
    {"TCM type", cpuinf_generic },
    {"TLB type", cpuinf_tlbtype },
    {"TTBCR", cpuinf_ttbcr },
    {"TTBR0", cpuinf_ttbr0 },
    {"TTBR1", cpuinf_ttbr1 },
    {"Multiprocessor ID", cpuinf_generic },
    {"Revision ID", cpuinf_generic },
    {"Processor feature 0", cpuinf_feat0 },
    {"Processor feature 1", cpuinf_feat1 },
    {"Debug feature", cpuinf_dbgfeat },
    {"Aux feature", cpuinf_generic },
    {"Mem model feature 0", cpuinf_mmfr0 },
    {"Mem model feature 1", cpuinf_mmfr1 },
    {"Mem model feature 2", cpuinf_mmfr2 },
    {"Mem model feature 3", cpuinf_mmfr3 },
    {"ISA feature 0", cpuinf_isar0 },
    {"ISA feature 1", cpuinf_isar1 },
    {"ISA feature 2", cpuinf_isar2 },
    {"ISA feature 3", cpuinf_isar3 },
    {"ISA feature 4", cpuinf_isar4 },
    {"ISA feature 5", cpuinf_isar5 },
    {"Cache level ID", cpuinf_clidr_vmsa },
    {"Cache size ID reg (data, level0)", cpuinf_ccsidr },
    {"Cache size ID reg (inst, level0)", cpuinf_ccsidr },
    {"SCTLR", cpuinf_sctlr_vmsa },
    {"ACTLR", cpuinf_actlr_vmsa },
    {"ACTLR2", cpuinf_generic },
    {"CPACR", cpuinf_cpacr_vmsa },
    {"DACR", cpuinf_generic },
    //{"ATCM region reg", cpuinf_tcmreg }, // not specified in Cortex A literature
    //{"BTCM region reg", cpuinf_tcmreg }, // not specified in Cortex A literature
    {"NSACR (sec. ext. only)", cpuinf_generic },
    {"DBGDIDR", cpuinf_dbgdidr },
    {"DBGDRAR", cpuinf_dbgd_address },
    {"DBGDSAR", cpuinf_dbgd_address },
    {"DBGDSCR", cpuinf_dbgdscr },
    //{"Floating Point System ID register", cpuinf_generic },
    //{"Media and VFP Feature Register 0", cpuinf_generic },
    //{"Media and VFP Feature Register 1", cpuinf_generic },
    {"Config base addr reg", cpuinf_generic },
    {"PLEIDR", cpuinf_generic },
    {"TLB lockdown reg", cpuinf_generic },
    {"PRRR", cpuinf_generic },
    {"NMRR", cpuinf_generic },
    {0}
};

void __attribute__((naked,noinline)) cpuinfo_get_info_vmsa(__attribute__ ((unused))unsigned *results) {
    asm (
        ".syntax unified\n"
        ".code 16\n"
        ".align 2\n"
        "BX      PC\n"                  // switch to ARM mode
        ".code 32\n"

        "MRC    p15, 0, R1,c0,c0\n" // ident
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,1\n" // CTR, cache
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,2\n" // TCMTR, TCM
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,3\n" // TLBTR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c2,c0,2\n" // TTBCR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c2,c0,0\n" // TTBR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c2,c0,1\n" // TTBR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,5\n" // MPIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,6\n" // REVIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,0\n" // ID_PFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,1\n" // ID_PFR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,2\n" // ID_DFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,3\n" // ID_AFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,4\n" // ID_MMFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,5\n" // ID_MMFR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,6\n" // ID_MMFR2
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,7\n" // ID_MMFR3
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,0\n" // ID_ISAR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,1\n" // ID_ISAR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,2\n" // ID_ISAR2
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,3\n" // ID_ISAR3
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,4\n" // ID_ISAR4
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,5\n" // ID_ISAR5
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 1, R1,c0,c0,1\n" // CLIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #0\n"
        "MCR    p15, 2, R1,c0,c0,0\n" // CSSELR (data cache, level0)

        "MRC    p15, 1, R1,c0,c0,0\n" // CCSIDR (the currently selected one)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #1\n"
        "MCR    p15, 2, R1,c0,c0,0\n" // CSSELR (inst cache, level0)

        "MRC    p15, 1, R1,c0,c0,0\n" // CCSIDR (the currently selected one)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c1,c0,0\n" // SCTLR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c1,c0,1\n" // ACTLR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

//#ifndef CONFIG_QEMU
        "MRC    p15, 0, R1,c15,c0,0\n" // ACTLR2
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
//#endif

        "MRC    p15, 0, R1,c1,c0,2\n" // CPACR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        //"MRC    p15, 0, R1,c9,c1,1\n" // ATCM region reg, Cortex R4
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        //"MRC    p15, 0, R1,c9,c1,0\n" // BTCM region reg, Cortex R4
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c1,c1,2\n" // NSACR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c3,c0,0\n" // DACR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c0,c0,0\n" // DBGDIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c1,c0,0\n" // DBGDRAR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c2,c0,0\n" // DBGDSAR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p14, 0, R1,c0,c1,0\n" // DBGDSCR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        //".word  0xEEF01A10\n" //"VMRS   R1, FPSID\n" // Floating Point System ID register
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        //".word  0xEEF71A10\n" //"VMRS   R1, MVFR0\n" // Media and VFP Feature Register 0
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        //".word  0xEEF61A10\n" //"VMRS   R1, MVFR1\n" // Media and VFP Feature Register 1
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        "MRC    p15, 4, R1,c15,c0,0\n" // config base addr reg (Cortex A9)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c11,c0,0\n" // PLEIDR (Cortex A9)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c10,c0,0\n" // TLB lockdown reg (Cortex A9)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c10,c2,0\n" // PRRR (Cortex A9)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c10,c2,1\n" // NMRR (Cortex A9)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "BX     LR\n"

        :::"r0","r1"
    );
}

int __attribute__((naked,noinline)) cpu_is_vmsa() {
    asm (
        ".syntax unified\n"
        "MRC    p15, 0, R0,c0,c1,4\n" // ID_MMFR0
        "AND    R0, #15\n"
        "BX     LR\n"
        :::"r0"
        );
    return 0; // unreachable
}

void __attribute__((naked,noinline)) get_mmuregs_vmsa(__attribute__ ((unused))void *p) {
    asm (
        ".syntax unified\n"
        "MRC    p15, 0, R1,c2,c0,0\n" // TTBR0
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c2,c0,1\n" // TTBR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c2,c0,2\n" // TTBCR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "BX     LR\n"
        :::"r0","r1"
        );
}

struct mmuregs_s {
    unsigned    ttbr0;
    unsigned    ttbr1;
    unsigned    ttbcr;
};

struct l1tblentry_s {
    char *typ;
    char *pbit;
    unsigned domain;
    char physaddr[12];
    char l2addr[12];
    char *ngbit;
    char *sbit;
    char *accperm;
    char *caching;
    char *memtype;
    char *xnbit;
};

char *cpolicies[] = {
    "Cached OUTER 0 INNER 0",
    "Cached OUTER 0 INNER 1",
    "Cached OUTER 0 INNER 2",
    "Cached OUTER 0 INNER 3",
    "Cached OUTER 1 INNER 0",
    "Cached OUTER 1 INNER 1",
    "Cached OUTER 1 INNER 2",
    "Cached OUTER 1 INNER 3",
    "Cached OUTER 2 INNER 0",
    "Cached OUTER 2 INNER 1",
    "Cached OUTER 2 INNER 2",
    "Cached OUTER 2 INNER 3",
    "Cached OUTER 3 INNER 0",
    "Cached OUTER 3 INNER 1",
    "Cached OUTER 3 INNER 2",
    "Cached OUTER 3 INNER 3",
};

unsigned interpret_l1_table_entry(unsigned e, char *buf) {
    unsigned ret = 0, l2a = 0;
    struct l1tblentry_s col;
    col.typ = "";
    col.pbit = "";
    col.ngbit = "";
    col.domain = 0;
    col.physaddr[0] = 0;
    col.l2addr[0] = 0;
    col.sbit = "";
    col.accperm = "";
    col.caching = "";
    col.memtype = "";
    col.xnbit = "";
    int typ = -1;
    switch (e & 3) {
        case 1: typ = 2;
                break;
        case 2: typ = (e & 0x40000)?1:0;
                break;
    }
    if (typ < 0) {
        sprintf(buf,"Fault,");
        return 2;
    }
    if (e & 0x200) {
        col.pbit = "P";
    }
    col.domain = (e >> 5) & 15;
    if (typ == 2) {
        col.typ = "L2 ref";
        l2a = e & 0xfffffc00;
        sprintf(col.l2addr,"0x%08x",l2a);
    }
    else {
        col.typ = typ==1?"Supersection":"Section";
        sprintf(col.physaddr,"0x%08x",e & 0xfff00000);
        col.ngbit = e&0x20000?"Nonglobal":"Global";
        col.sbit = e&0x10000?"Shareable":"";
        col.xnbit = e&0x10?"No exec":"";
        switch (e & 0x8C00) {
            case 0: col.accperm = "--/--"; break;
            case 0x400: col.accperm = "RW/--"; break;
            case 0x800: col.accperm = "RW/R-"; break;
            case 0xC00: col.accperm = "RW/RW"; break;
            case 0x8000: col.accperm = "rsrvd"; break;
            case 0x8400: col.accperm = "R-/--"; break;
            case 0x8800: col.accperm = "R-/R-"; break;
            case 0x8C00: col.accperm = "rsrvd"; break;
        }
        switch (e & 0x700c) {
          case 0: col.caching = "STR ORD"; col.memtype = "Strongly-ordered"; break;
          case 4: col.caching = "SHR DEV"; col.memtype = "Device"; break;
          case 8: col.caching = "WRTHR, NAW"; col.memtype = "Normal"; break;
          case 0xc: col.caching = "WRBCK, NAW"; col.memtype = "Normal"; break;
          case 0x1000: col.caching = "NON CACH"; col.memtype = "Normal"; break;
          case 0x2000: col.caching = "NONSHR DEV"; col.memtype = "Device"; break;
          default:
              if (e & 0x4000) {
                  unsigned i = ((e&0x3000)>>10)|((e&0xc)>>2);
                  col.caching = cpolicies[i];
                  col.memtype = "Normal";
              }
        }
    }
    sprintf(buf,"%s,%s,%s,%u,%s,%s,%s,%s,%s,%s,%s,",col.typ,col.pbit,
            col.ngbit,col.domain,col.physaddr,col.l2addr,col.sbit,col.accperm,
            col.caching,col.memtype,col.xnbit);
    if (typ==1) {
        ret = 1;
    }
    if (l2a!=0) {
        ret = l2a;
    }
    return ret;
}

unsigned interpret_l2_table_entry(unsigned e, char *buf) {
    unsigned ret = 0, f;
    struct l1tblentry_s col;
    col.typ = "";
    col.pbit = "";
    col.ngbit = "";
    col.domain = 0;
    col.physaddr[0] = 0;
    col.l2addr[0] = 0;
    col.sbit = "";
    col.accperm = "";
    col.caching = "";
    col.memtype = "";
    col.xnbit = "";
    int typ = -1;
    switch (e & 3) {
        case 1: typ = 1;
                col.typ = "Large page";
                sprintf(col.physaddr,"0x%08x",e & 0xffff0000);
                col.xnbit = e&0x8000?"No exec":"";
                f = e;
                break;
        case 2:
        case 3: typ = 0;
                col.typ = "Small page";
                sprintf(col.physaddr,"0x%08x",e & 0xfffff000);
                col.xnbit = e&1?"No exec":"";
                f = ((e<<6) & 0x7000) | (e & 0xc); // rearrange bits for evaluation below
                break;
    }
    if (typ < 0) {
        sprintf(buf,"Fault,");
        return 2;
    }
    col.ngbit = e&0x800?"Nonglobal":"Global";
    col.sbit = e&0x400?"Shareable":"";
    switch ((e<<6) & 0x8C00) {
        case 0: col.accperm = "--/--"; break;
        case 0x400: col.accperm = "RW/--"; break;
        case 0x800: col.accperm = "RW/R-"; break;
        case 0xC00: col.accperm = "RW/RW"; break;
        case 0x8000: col.accperm = "rsrvd"; break;
        case 0x8400: col.accperm = "R-/--"; break;
        case 0x8800: col.accperm = "R-/R-"; break;
        case 0x8C00: col.accperm = "rsrvd"; break;
    }
    switch (f & 0x700c) {
      case 0: col.caching = "STR ORD"; col.memtype = "Strongly-ordered"; break;
      case 4: col.caching = "SHR DEV"; col.memtype = "Device"; break;
      case 8: col.caching = "WRTHR, NAW"; col.memtype = "Normal"; break;
      case 0xc: col.caching = "WRBCK, NAW"; col.memtype = "Normal"; break;
      case 0x1000: col.caching = "NON CACH"; col.memtype = "Normal"; break;
      case 0x2000: col.caching = "NONSHR DEV"; col.memtype = "Device"; break;
      default:
          if (f & 0x4000) {
              unsigned i = ((f&0x3000)>>10)|((f&0xc)>>2);
              col.caching = cpolicies[i];
              col.memtype = "Normal";
          }
    }
    sprintf(buf,"%s,%s,%s,,%s,%s,%s,%s,%s,%s,%s,",col.typ,col.pbit,
            col.ngbit,col.physaddr,col.l2addr,col.sbit,col.accperm,
            col.caching,col.memtype,col.xnbit);
    if (typ==1) {
        ret = 1;
    }
    return ret;
}

const char *csvhead = "Virt.addr,Table,Type,P bit,NG bit,Domain,Phys.addr,L2 ref,S bit,Privileged/Nonpriv.,Caching,Memtype,XN bit\n";

void memmapping_vmsa(FILE *f) {
    struct mmuregs_s mmuregs;
    get_mmuregs_vmsa(&mmuregs);
    unsigned tt0len = 128 << (7 - (mmuregs.ttbcr & 7));
    unsigned tt0adr = mmuregs.ttbr0 & 0xffffff80;
    unsigned tt1adr = mmuregs.ttbr1 & 0xffffff80;
    unsigned n, *e, r, pr=42, cycl, l1a;
    char *conclude;
    e = (unsigned*)tt0adr;
    cycl = tt0len/4;
    l1a = 0;
    unsigned remain = 2;
    fwrite(csvhead,1,strlen(csvhead),f);
    while (remain) {
        for (n=0; n<cycl; n++) {
            sprintf(linebuf,"0x%08X,L1,",l1a); // virtual address to be described by L1 entry
            fwrite(linebuf,1,strlen(linebuf),f);
            conclude = "\n";
            r = interpret_l1_table_entry(*e, linebuf);
            fwrite(linebuf,1,strlen(linebuf),f);
            if (r==1 && pr!=1) { // supersection begins
                if ((unsigned)e & 0x3f) {
                    conclude = "ERR: Unaligned supersection\n";
                }
                else {
                    unsigned m, v;
                    v = *e;
                    for (m=1; m<16; m++) {
                        if (v != *(e+m)) {
                            conclude = "ERR: Inconsistent supersection\n";
                            break;
                        }
                    }
                }
            }
            else if (r==2) {
                conclude = "\n";
            }
            fwrite(conclude,1,strlen(conclude),f);
            if (r>42) { // interpret L2 table
                unsigned nn, l2a, *ee, rr, prr=42;
                l2a = l1a;
                ee = (unsigned*) r;
                for (nn=0; nn<256; nn++) {
                    sprintf(linebuf,"0x%08X,L2,",l2a); // virtual address to be described by L2 entry
                    fwrite(linebuf,1,strlen(linebuf),f);
                    conclude = "\n";
                    rr = interpret_l2_table_entry(*ee, linebuf);
                    fwrite(linebuf,1,strlen(linebuf),f);
                    
                    if (rr==1 && prr!=1) { // large page begins
                        if ((unsigned)ee & 0x3f) {
                            conclude = "ERR: Unaligned large page\n";
                        }
                        else {
                            unsigned mm, vv;
                            vv = *ee;
                            for (mm=1; mm<16; mm++) {
                                if (vv != *(ee+mm)) {
                                    conclude = "ERR: Inconsistent large page\n";
                                    break;
                                }
                            }
                        }
                    }
                    else if (rr==2) {
                        conclude = "\n";
                    }
                    fwrite(conclude,1,strlen(conclude),f);
                    
                    ee++;
                    prr = rr;
                    l2a += 0x1000;
                }
            }
            e++;
            pr = r;
            l1a += 0x100000;
        }
        e = (unsigned*)(tt1adr+tt0len);
        cycl = (0x4000 - tt0len) / 4;
        remain--;
    }
    fclose(f);
}


