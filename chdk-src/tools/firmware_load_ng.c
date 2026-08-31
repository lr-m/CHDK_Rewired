#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <capstone.h>

#include "stubs_load.h" // needed for sv in fw struct
#include "firmware_load_ng.h"


// Add a valid range to the list
static void addBufRange(firmware *fw, int o, int l)
{
    BufRange *n = malloc(sizeof(BufRange));
    n->p = fw->buf32 + o;
    n->off = o;
    n->len = l;
    n->next = 0;
    if (fw->br == 0)
    {
        fw->br = n;
    }
    else
    {
        fw->last->next = n;
    }
    fw->last = n;
}

// Find valid ranges for the firmware dump
static void findRanges(firmware *fw)
{
    int i, j, k;

    // Find all the valid ranges for checking (skips over large blocks of 0xFFFFFFFF)
    fw->br = 0; fw->last = 0;
    k = -1; j = 0;
    for (i = 0; i < fw->size32; i++)
    {
        if (fw->buf32[i] == 0xFFFFFFFF)   // Possible start of block to skip
        {
            if (k == -1)            // Mark start of possible skip block
            {
                k = i;
            }
        }
        else                        // Found end of block ?
        {
            if (k != -1)
            {
                if (i - k > 32)     // If block more than 32 words then we want to skip it
                {
                    if (k - j > 8)
                    {
                        // Add a range record for the previous valid range (ignore short ranges)
                        addBufRange(fw,j,k - j);
                    }
                    j = i;          // Reset valid range start to current position
                }
                k = -1;             // Reset marker for skip block
            }
        }
    }
    // Add range for last valid block
    if (k != -1)
    {
        if (k - j > 8)
        {
            addBufRange(fw,j,k - j);
        }
    }
    else
    {
        if (i - j > 8)
        {
            addBufRange(fw,j,i - j);
        }
    }
}

// return the buffrange for a given offset or null if not found
BufRange *getBufRangeForIndex(firmware *fw,int i)
{
    BufRange *br = fw->br;
    while (br) {
        if(i >= br->off && i < br->off + br->len) {
            return br;
        }
        br = br->next;
    }
    return NULL;
}

// Find the index of a string in the firmware
// Assumes the string starts on a 32bit boundary.
// String + terminating zero byte should be at least 4 bytes long
// Handles multiple string instances
int find_Nth_str(firmware *fw, char *str, int N)
{
    int nlen = strlen(str);
    uint32_t nm0 = *((uint32_t*)str);
    uint32_t *p;
    int j;

    BufRange *br = fw->br;
    while (br)
    {
        for (p = br->p, j = 0; j < br->len - nlen/4; j++, p++)
        {
            if ((nm0 == *p) && ((nlen<=4) || (memcmp(p+1,str+4,nlen-4) == 0)) )
            {
                if (--N == 0)
                    return j+br->off;
            }
        }
        br = br->next;
    }

    return -1;
}

int find_str(firmware *fw, char *str)
{
    return find_Nth_str(fw, str, 1);
}

// find sequence of bytes, starting from star_adr, up to max_adr, any alignment
// returns firmware address or 0
// use repeated calls to find multiple
// NOTE only handles ROM addresses
uint32_t find_next_bytes_range(firmware *fw, const void *bytes, size_t len, uint32_t start_adr, uint32_t max_adr)
{
    if(!start_adr) {
        start_adr = fw->base;
    }
    if(start_adr < fw->base || start_adr >= fw->base + fw->size8) {
        fprintf(stderr,"find_next_bytes_range invalid start_adr 0x%08x\n",start_adr);
        return 0;
    }
    if(!max_adr) {
        max_adr = fw->base + fw->size8-1;
    }
    if(max_adr < fw->base || max_adr >= fw->base + fw->size8) {
        fprintf(stderr,"find_next_bytes_range invalid max_adr 0x%08x\n",max_adr);
        return 0;
    }
    int end_k = (max_adr - fw->base);
    BufRange *p = getBufRangeForIndex(fw,(start_adr - fw->base)/4);
    if(!p) {
        return 0;
    }
    int k = start_adr - fw->base;

    while (k < end_k)
    {
        for (; k < (p->off + p->len)*4; k++)
        {
            if (memcmp(fw->buf8+k,bytes,len) == 0) {
                return fw->base+k;
            }
        }
        p = p->next;
        if(!p) {
            break;
        }
        k = p->off*4;
    }
    return 0;
}

// find up to max matching byte sequences, storing addresses in result
// returns count
int find_bytes_all(firmware *fw, const void *bytes, size_t len, uint32_t adr, uint32_t *result, int max)
{
    int i;
    for(i=0,adr=find_next_bytes_range(fw,bytes,len,0,0); adr && (i < max); adr=find_next_bytes_range(fw,bytes,len,adr+len,0),i++) {
        result[i] = adr;
    }
    return i;
}

uint32_t find_next_substr_bytes(firmware *fw, const char *str, uint32_t adr)
{
    //fprintf(stderr,"find_next_substr_bytes 0x%08x\n",adr);
    // strlen excludes null
    return find_next_bytes_range(fw,str,strlen(str),adr,0);
}

uint32_t find_next_str_bytes_range(firmware *fw, const char *str, uint32_t adr,uint32_t max_adr)
{
    // +1 to include the null in memcmp
    return find_next_bytes_range(fw,str,strlen(str)+1,adr,max_adr);
}

uint32_t find_next_str_bytes_main_fw(firmware *fw, const char *str, uint32_t adr)
{
    // max is end of fw code + 4096, assuming it fits in fw
    // while early code could technically load from base - 1k, unlikely
    uint32_t max_adr;
    if(fw->base + fw->size8 - 4096 > fw->rom_code_search_max_adr) {
        max_adr = fw->rom_code_search_max_adr + 4096;
    } else {
        max_adr = fw->base + fw->size8;
    }
    return find_next_bytes_range(fw,str,strlen(str)+1,adr,max_adr);
}

// find a string within range of LDR pc or ADR, starting from main fw
uint32_t find_str_bytes_main_fw(firmware *fw, const char *str)
{
    return find_next_str_bytes_main_fw(fw,str,fw->rom_code_search_min_adr);
}

uint32_t find_next_str_bytes(firmware *fw, const char *str, uint32_t adr)
{
    // +1 to include the null in memcmp
    return find_next_bytes_range(fw,str,strlen(str)+1,adr,0);
}

// Find the index of a string in the firmware, can start at any address
// returns firmware address
uint32_t find_str_bytes(firmware *fw, const char *str)
{
    return find_next_str_bytes(fw,str,fw->base);
}

// find a string within adr_range containing start adr
uint32_t find_next_str_bytes_adr_range(firmware *fw, const char *str, uint32_t adr)
{
    if(!adr) {
        fprintf(stderr,"find_next_str_bytes_adr_range: null adr\n");
        return 0;
    }
    adr_range_t *rng = adr_get_range(fw,adr);
    if(!rng) {
        fprintf(stderr,"find_next_str_bytes_adr_range: adr 0x%x out of range\n", adr);
        return 0;
    }
    // find bytes using ROM address mapping
    uint32_t start_adr = adr2romadr(fw, adr);
    uint32_t max_adr = adr2romadr(fw, rng->start + rng->bytes - 1);
    uint32_t r = find_next_bytes_range(fw,str,strlen(str)+1,start_adr,max_adr);
    if(r) {
        return romadr2adr(fw,r);
    }
    return r;
}

// find a string within the specified adr ranges, using ADR_RANGE_M_* defines
// if adr is 0, start with first matching range
uint32_t find_next_str_bytes_adr_ranges(firmware *fw, const char *str, uint32_t range_match, uint32_t adr)
{
    adr_range_t *rng = NULL;
    if(adr) {
        rng = adr_get_range(fw,adr);
        if(!rng) {
            fprintf(stderr,"find_next_str_bytes_adr_ranges: adr 0x%x out of range\n", adr);
            return 0;
        }
    }
    do {
        if(!adr) {
            rng = adr_range_find_next(fw,rng,range_match);
            // no more ranges matching criteria
            if(!rng) {
                return 0;
            }
            adr = rng->start;
            rng++;
        }
        // TODO would be nice to be able to restrict ROM to main_fw
        // searching whole ROM and copied regions doesn't make much sense
        // but if search ram and ROM, probably want ROM last
        adr = find_next_str_bytes_adr_range(fw,str,adr);
    } while(!adr);
    return adr;
}

// find a string in possibly code ranges defined by SEARCH_F_* bits in search_ranges
// if adr is 0, start with first matching range
uint32_t find_next_str_bytes_code(firmware *fw, const char *str, uint32_t search_ranges, uint32_t adr)
{
    // TODO ugly flag remapping
    // ROM handled seperately because we want to do it last, and respect main code region
    uint32_t range_match = 0;
    if(search_ranges & SEARCH_F_RAM_CODE) {
        range_match |= ADR_RANGE_M_RAM_CODE;
    }
    if(search_ranges & SEARCH_F_TCM_CODE) {
        range_match |= ADR_RANGE_M_TCM_CODE;
    }
    uint32_t str_adr = 0;
    if(range_match) {
        // if adr is in ROM, this will return 0 since not include in range_match
        str_adr = find_next_str_bytes_adr_ranges(fw, str, range_match, adr);
        if(str_adr) {
            return str_adr;
        }
    }
    if(!(search_ranges & SEARCH_F_ROM_CODE)) {
        return 0;
    }
    if(!adr) {
        adr = fw->rom_code_search_min_adr;
    }
    return find_next_str_bytes_main_fw(fw,str,adr);
}

int isASCIIstring(firmware *fw, uint32_t adr)
{
    unsigned char *p = (unsigned char*)adr2ptr_with_data(fw, adr);
    if(!p) {
        return 0;
    }
    // TODO should avoid running off end of dump
    int i;
    for (i = 0; (i < 100) && (p[i] != 0); i++)
    {
        if (!((p[i] == '\r') || (p[i] == '\n') || (p[i] == '\t') || ((p[i] >= 0x20) && (p[i] <= 0x7f))))
        {
            return 0;
        }
    }
    if ((i >= 2) && (p[i] == 0))
        return 1;
    return 0;
}

// return address range struct for adr, or NULL if not in known range
adr_range_t *adr_get_range(firmware *fw, uint32_t adr)
{
    int i;
    adr_range_t *r=fw->adr_ranges;
    for(i=0;i<fw->adr_range_count;i++) {
        if(adr >= r->start && adr < r->start + r->bytes) {
            return r;
        }
        r++;
    }
    return NULL;
}

// round adr to fall within adr_range with 4 byte alignment
uint32_t adr_range_clamp_adr_align4(adr_range_t *r, uint32_t adr)
{
    if(!r || r->type == ADR_RANGE_INVALID || r->bytes < 8) {
        return 0;
    }
    adr = ADR_ALIGN4(adr);
    if(adr < r->start) {
        adr = r->start;
        if((adr & ~3) != adr) {
            adr = ADR_ALIGN4(adr) + 4;
        }
    } else if(adr > r->start + r->bytes - 1) {
        adr = ADR_ALIGN4(r->start + r->bytes - 4);
    }
    return adr;
}

// return whether the range matches the give ADR_RANGE_M bits
int adr_range_match(adr_range_t *r, uint32_t range_match)
{
    if(!r) {
        return 0;
    }
    if(r->type == ADR_RANGE_ROM && (range_match & ADR_RANGE_M_ROM)) {
        return 1;
    }
    if(r->type == ADR_RANGE_INIT_DATA && (range_match & ADR_RANGE_M_RAM_DATA)) {
        return 1;
    }
    if(r->type == ADR_RANGE_RAM_CODE) {
        if((r->flags & ADR_RANGE_FL_EVEC) && (range_match & ADR_RANGE_M_EVEC)) {
            return 1;
        }
        if((r->flags & ADR_RANGE_FL_TCM) && (range_match & ADR_RANGE_M_TCM_CODE)) {
            return 1;
        }
        // ram code only
        if(range_match & ADR_RANGE_M_RAM_CODE) {
            if(r->flags & (ADR_RANGE_FL_EVEC | ADR_RANGE_FL_TCM)) {
                return 0;
            }
            return 1;
        }
    }
    return 0;
}

// return next address range matching critera, starting from rng, or first if null
adr_range_t *adr_range_find_next(firmware *fw, adr_range_t *start, uint32_t range_match)
{
    adr_range_t *r;
    if(!start) {
        start = &fw->adr_ranges[0];
    }
    for(r=start;r->type != ADR_RANGE_INVALID;r++) {
        if(adr_range_match(r,range_match)) {
            return r;
        }
    }
    return NULL;
}

// return what kind of range adr is in
int adr_get_range_type(firmware *fw, uint32_t adr)
{
    adr_range_t *r=adr_get_range(fw,adr);
    if(!r) {
        return ADR_RANGE_INVALID;
    }
    return r->type;
}

uint32_t ptr2adr(firmware *fw, uint8_t *ptr)
{
    // TODO handle copied, or maybe another func to convert?
    return (ptr-fw->buf8)+fw->base;
}

uint8_t* adr2ptr(firmware *fw, uint32_t adr)
{
    adr_range_t *r=adr_get_range(fw,adr);
    if(!r) {
        return NULL;
    }
    switch(r->type) {
        case ADR_RANGE_RAM_CODE:
        case ADR_RANGE_ROM:
            return (r->buf)+(adr - r->start);
        default:
            return NULL;
    }
}

uint8_t* adr2ptr_with_data(firmware *fw, uint32_t adr)
{
    adr_range_t *r=adr_get_range(fw,adr);
    if(!r) {
        return NULL;
    }
    switch(r->type) {
        case ADR_RANGE_RAM_CODE:
        case ADR_RANGE_INIT_DATA:
        case ADR_RANGE_ROM:
            return (r->buf)+(adr - r->start);
        default:
            return NULL;
    }
}

// return ROM address of address that may be in a remapped adr_range
uint32_t adr2romadr(firmware *fw, uint32_t adr)
{
    adr_range_t *r = adr_get_range(fw,adr);
    if(r) {
        return (adr - r->start) + r->src_start;
    }
    return 0;
}

// return the copied address of a ROM address in the source area, or 0 if out of range
// ROM addresses are returned as-is
uint32_t romadr2adr(firmware *fw, uint32_t adr)
{
    int i;
    adr_range_t *r=fw->adr_ranges;
    for(i=0;i<fw->adr_range_count;i++,r++) {
        // ROM contains all other source regions, handle after others
        if(r->type == ADR_RANGE_ROM) {
            continue;
        }
        if(adr >= r->src_start && adr < r->src_start + r->bytes) {
            return adr - r->src_start + r->start;
        }
    }
    // if adr is a valid ROM address, return as is
    if(adr_get_range_type(fw, adr) == ADR_RANGE_ROM) {
        return adr;
    }
    return 0;
}

// return constant string describing type
const char* adr_range_type_str(int type)
{
    switch(type) {
        case ADR_RANGE_INVALID:
            return "(invalid)";
        case ADR_RANGE_ROM:
            return "ROM";
        case ADR_RANGE_RAM_CODE:
            return "RAM code";
        case ADR_RANGE_INIT_DATA:
            return "RAM data";
        default:
            return "(unknown)";
    }
}

// return constant string describing type and flags
const char* adr_range_desc_str(adr_range_t *r)
{
    switch(r->type) {
        case ADR_RANGE_INVALID:
            return "(invalid)";
        case ADR_RANGE_ROM:
            return "ROM";
        case ADR_RANGE_RAM_CODE:
            if(r->flags & ADR_RANGE_FL_EVEC) {
                return "EVEC";
            } else if(r->flags & ADR_RANGE_FL_TCM) {
                return "TCM code";
            }
            return "RAM code";
        case ADR_RANGE_INIT_DATA:
            return "RAM data";
        default:
            return "(unknown)";
    }
}

// return true if adr is in firmware DATA or BSS
int adr_is_var(firmware *fw, uint32_t adr)
{
    return (adr > fw->data_start && adr < fw->memisostart);
}

// return true if adr is in the ROM search range, or one of the copied RAM code regions
int adr_is_main_fw_code(firmware *fw, uint32_t adr)
{
    int adr_type = adr_get_range_type(fw,adr);
    if(adr_type == ADR_RANGE_RAM_CODE) {
        return 1;
    }
    if(adr_type != ADR_RANGE_ROM) {
        return 0;
    }
    if(adr < fw->rom_code_search_min_adr  || adr > fw->rom_code_search_max_adr) {
        return 0;
    }
    return 1;
}

/*
return firmware address of 32 bit value, starting at address "start", up to max
*/
uint32_t find_u32_adr_range(firmware *fw, uint32_t val, uint32_t start,uint32_t maxadr)
{
    // TODO
    if(start == 0) {
        start=fw->base;
    }
    if(start & 3) {
        fprintf(stderr,"find_u32_adr unaligned start 0x%08x\n",start);
        return 0;
    }
    uint32_t *p=(uint32_t *)adr2ptr(fw,start);
    if(!p) {
        fprintf(stderr,"find_u32_adr bad start 0x%08x\n",start);
        return 0;
    }
    uint32_t *p_end;
    if(maxadr) {
        p_end = (uint32_t *)adr2ptr(fw,maxadr);
    } else {
        p_end = fw->buf32 + fw->size32 - 1;
    }
    // TODO should use buf ranges
    while(p<=p_end) {
        if(*p==val) {
            return ptr2adr(fw,(uint8_t *)p);
        }
        p++;
    }
    return 0;
}

// as above, full to end of fw
uint32_t find_u32_adr(firmware *fw, uint32_t val, uint32_t start)
{
    return find_u32_adr_range(fw,val,start, fw->base + (fw->size8 -4));
}

// return u32 value at adr
uint32_t fw_u32(firmware *fw, uint32_t adr)
{
    uint32_t *p=(uint32_t *)adr2ptr(fw,adr);
    if(!p) {
        fprintf(stderr,"fw_u32 bad adr 0x%08x\n",adr);
        return 0;
    }
    return *p;
}

// memcmp, but using a firmware address, returning 1 adr/size out of range
int fw_memcmp(firmware *fw, uint32_t adr,const void *cmp, size_t n)
{
    uint32_t *p=(uint32_t *)adr2ptr(fw,adr);
    if(!p) {
        return 1;
    }
    if(n >= fw->size8 - (adr - fw->base)) {
        return 1;
    }
    return memcmp(p,cmp,n);
}


// ****** address history functions ******
// reset address history to empty
void adr_hist_reset(adr_hist_t *ah)
{
    ah->cur=0;
    ah->count=0;
    // memset shouldn't be needed
    // memset(ah->adrs,0,ADR_HIST_SIZE*4);
}

// return the index of current entry + i. may be negative or positive, wraps. Does not check validity
int adr_hist_index(adr_hist_t *ah, int i)
{
    int r=(ah->cur+i)%ADR_HIST_SIZE;
    if(r < 0) {
        return ADR_HIST_SIZE + r;
    }
    return r;
}

// add an entry to address history
void adr_hist_add(adr_hist_t *ah, uint32_t adr)
{
    ah->cur=adr_hist_index(ah,1);
    ah->adrs[ah->cur]=adr;
    if(ah->count < ADR_HIST_SIZE)  {
        ah->count++;
    }
}

// return the i'th previous entry in this history, or 0 if not valid (maybe should be -1?)
// i= 0 = most recently disassembled instruction, if any
uint32_t adr_hist_get(adr_hist_t *ah, int i)
{
    if(!ah->count || i > ah->count) {
        return 0;
    }
    return ah->adrs[adr_hist_index(ah,-i)];
}

// ****** instruction analysis utilities ******
// is insn an ARM instruction?
// like cs_insn_group(cs_handle,insn,ARM_GRP_ARM) but doesn't require handle and doesn't check or report errors
int isARM(cs_insn *insn)
{
    int i;
    for(i=0;i<insn->detail->groups_count;i++) {
        if(insn->detail->groups[i] == ARM_GRP_ARM) {
            return 1;
        }
    }
    return 0;
}

/*
is insn a Rn relative load?
*/
int isLDR_Rn(cs_insn *insn, arm_reg r)
{
    return insn->id == ARM_INS_LDR
           && insn->detail->arm.op_count == 2
           && insn->detail->arm.operands[0].type == ARM_OP_REG
           && insn->detail->arm.operands[1].type == ARM_OP_MEM
           && insn->detail->arm.operands[1].mem.base == r;
}

/*
is insn a PC relative load?
*/
int isLDR_PC(cs_insn *insn)
{
    return isLDR_Rn(insn, ARM_REG_PC);
}

/*
is insn a PC relative load to PC?
*/
int isLDR_PC_PC(cs_insn *insn)
{
    if(!isLDR_PC(insn)) {
        return 0;
    }
    return (insn->detail->arm.operands[0].reg == ARM_REG_PC);
}

//  subw    rd, pc, #x?
int isSUBW_PC(cs_insn *insn)
{
    return(insn->id == ARM_INS_SUBW
       && insn->detail->arm.op_count == 3
       && insn->detail->arm.operands[0].type == ARM_OP_REG
       && insn->detail->arm.operands[0].reg != ARM_REG_PC
       && insn->detail->arm.operands[1].type == ARM_OP_REG
       && insn->detail->arm.operands[1].reg == ARM_REG_PC
       && insn->detail->arm.operands[2].type == ARM_OP_IMM);
}

//  addw    rd, pc, #x?
int isADDW_PC(cs_insn *insn)
{
    return(insn->id == ARM_INS_ADDW
       && insn->detail->arm.op_count == 3
       && insn->detail->arm.operands[0].type == ARM_OP_REG
       && insn->detail->arm.operands[0].reg != ARM_REG_PC
       && insn->detail->arm.operands[1].type == ARM_OP_REG
       && insn->detail->arm.operands[1].reg == ARM_REG_PC
       && insn->detail->arm.operands[2].type == ARM_OP_IMM);
}

// is insn ADD rd, pc, #x  (only generated for ARM in capstone)
int isADD_PC(cs_insn *insn)
{
    return (insn->id == ARM_INS_ADD
            && insn->detail->arm.op_count == 3
            && insn->detail->arm.operands[0].reg != ARM_REG_PC
            && insn->detail->arm.operands[1].type == ARM_OP_REG
            && insn->detail->arm.operands[1].reg == ARM_REG_PC
            && insn->detail->arm.operands[2].type == ARM_OP_IMM);
}

// is insn SUB rd, pc, #x  (only generated for ARM in capstone)
int isSUB_PC(cs_insn *insn)
{
    return (insn->id == ARM_INS_SUB
            && insn->detail->arm.op_count == 3
            && insn->detail->arm.operands[0].reg != ARM_REG_PC
            && insn->detail->arm.operands[1].type == ARM_OP_REG
            && insn->detail->arm.operands[1].reg == ARM_REG_PC
            && insn->detail->arm.operands[2].type == ARM_OP_IMM);
}

// does insn look like a function return?
int isRETx(cs_insn *insn)
{
    // BX LR
    if(insn->id == ARM_INS_BX
            && insn->detail->arm.op_count == 1
            && insn->detail->arm.operands[0].type == ARM_OP_REG
            && insn->detail->arm.operands[0].reg == ARM_REG_LR) {
        return 1;
    }

    // TODO LDR pc, [sp], imm is somewhat common, but could also be function pointer call

    // POP. capstone translates LDMFD   SP!,... in arm code to pop
    if(insn->id == ARM_INS_POP) {
        int i;
        for(i=0; i < insn->detail->arm.op_count; i++) {
            if(insn->detail->arm.operands[i].type == ARM_OP_REG
                && insn->detail->arm.operands[i].reg == ARM_REG_PC) {
                return 1;
            }
        }
    }
    // MOV PC, LR (some tools translate this to RET)
    if(insn->id == ARM_INS_MOV
            && insn->detail->arm.operands[0].type == ARM_OP_REG
            && insn->detail->arm.operands[0].reg == ARM_REG_PC
            && insn->detail->arm.operands[1].type == ARM_OP_REG
            && insn->detail->arm.operands[1].reg == ARM_REG_LR) {
        return 1;
    }
    return 0;
}

// does insn push LR (function start -ish)
int isPUSH_LR(cs_insn *insn)
{
    if(insn->id != ARM_INS_PUSH) {
        return 0;
    }
    int i;
    for(i=0; i < insn->detail->arm.op_count; i++) {
        if(insn->detail->arm.operands[i].type == ARM_OP_REG
            && insn->detail->arm.operands[i].reg == ARM_REG_LR) {
            return 1;
        }
    }
    return 0;
}

// does insn pop LR (func end before tail call)
int isPOP_LR(cs_insn *insn)
{
    if(insn->id != ARM_INS_POP) {
        return 0;
    }
    int i;
    for(i=0; i < insn->detail->arm.op_count; i++) {
        if(insn->detail->arm.operands[i].type == ARM_OP_REG
            && insn->detail->arm.operands[i].reg == ARM_REG_LR) {
            return 1;
        }
    }
    return 0;
}

// does insn pop PC
int isPOP_PC(cs_insn *insn)
{
    if(insn->id != ARM_INS_POP) {
        return 0;
    }
    int i;
    for(i=0; i < insn->detail->arm.op_count; i++) {
        if(insn->detail->arm.operands[i].type == ARM_OP_REG
            && insn->detail->arm.operands[i].reg == ARM_REG_PC) {
            return 1;
        }
    }
    return 0;
}

// is the instruction ADD* rx, imm
int isADDx_imm(cs_insn *insn)
{
    return ((insn->id == ARM_INS_ADD || insn->id == ARM_INS_ADDW) && insn->detail->arm.operands[1].type == ARM_OP_IMM);
}
// is the instruction SUB* rx, imm
int isSUBx_imm(cs_insn *insn)
{
    return (IS_INSN_ID_SUBx(insn->id) && insn->detail->arm.operands[1].type == ARM_OP_IMM);
}

// is the instruction an ADR or ADR-like instruction?
int isADRx(cs_insn *insn)
{
    return ((insn->id == ARM_INS_ADR)
        || isSUBW_PC(insn)
        || isADDW_PC(insn)
        || (isARM(insn) && (isADD_PC(insn) || isSUB_PC(insn))));
}

// if insn is LDR Rn, [pc,#x] return pointer to value, otherwise null
uint32_t* LDR_PC2valptr_thumb(firmware *fw, cs_insn *insn)
{
    if(!isLDR_PC(insn)) {
        return NULL;
    }
    uint32_t adr;
    // TODO NOTE doesn't do anything with scale (which can supposedly be neg?),
    // appears correct for examples seen so far
    adr=(insn->address&~3)+4+insn->detail->arm.operands[1].mem.disp;
    return (uint32_t *)adr2ptr(fw,adr);
}

uint32_t* LDR_PC2valptr_arm(firmware *fw, cs_insn *insn)
{
    if(!isLDR_PC(insn)) {
        return NULL;
    }
    uint32_t adr;
    // TODO NOTE doesn't do anything with scale (which can supposedly be neg?),
    // appears correct for examples seen so far
    adr=insn->address+8+insn->detail->arm.operands[1].mem.disp;
    return (uint32_t *)adr2ptr(fw,adr);
}

uint32_t* LDR_PC2valptr(firmware *fw, cs_insn *insn)
{
    if(isARM(insn)) {
       return LDR_PC2valptr_arm(fw,insn);
    } else {
       return LDR_PC2valptr_thumb(fw,insn);
    }
}

// return the address of value loaded by LDR rd, [pc, #x] or 0 if not LDR PC
uint32_t LDR_PC2adr(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(!isLDR_PC(insn)) {
        return 0;
    }
    if(isARM(insn)) {
       return insn->address+8+insn->detail->arm.operands[1].mem.disp;
    } else {
       return (insn->address&~3)+4+insn->detail->arm.operands[1].mem.disp;
    }
}

// return value generated by an ADR or ADR-like instruction, or 0 (which should be rarely generated by ADR)
uint32_t ADRx2adr(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_ADR) {
        return (insn->address&~3)+4+insn->detail->arm.operands[1].imm;
    }
    if(isSUBW_PC(insn)) {
        return (insn->address&~3)+4-insn->detail->arm.operands[2].imm;
    }
    if(isADDW_PC(insn)) {
        return (insn->address&~3)+4+insn->detail->arm.operands[2].imm;
    }
    if(isARM(insn)) {
        if(isADD_PC(insn)) {
            return insn->address+8+insn->detail->arm.operands[2].imm;
        }
        if(isSUB_PC(insn)) {
            return insn->address+8-insn->detail->arm.operands[2].imm;
        }
    }
    return 0;
}

// return the value generated by an ADR (ie, the location of the value as a firmware address)
// NOTE not checked if it is in dump
uint32_t ADR2adr(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id != ARM_INS_ADR) {
        return 0;
    }
    // TODO - capstone doesn't appear to generate ADR for ARM
    /*
    if(cs_insn_group(fw->cs_handle,insn,ARM_GRP_ARM)) {
       return 0;
    }
    */
    return (insn->address&~3)+4+insn->detail->arm.operands[1].imm;
}

// if insn is adr/ AKA ADD Rn, pc,#x return pointer to value, otherwise null
uint32_t* ADR2valptr(firmware *fw, cs_insn *insn)
{
    uint32_t adr=ADR2adr(fw,insn);
    return (uint32_t *)adr2ptr(fw,adr);
}

// return value loaded by PC relative LDR instruction, or 0 if out of range
uint32_t LDR_PC2val(firmware *fw, cs_insn *insn)
{
    uint32_t *p=LDR_PC2valptr(fw,insn);
    if(p) {
        return *p;
    }
    return 0;
}

// return value loaded by PC relative LDR pc..., or 0 if not matching or out of range
uint32_t LDR_PC_PC_target(firmware *fw, cs_insn *insn)
{
    if(!isLDR_PC_PC(insn)) {
        return 0;
    }
    return LDR_PC2val(fw,insn);
}

// return the target of B instruction, or 0 if current instruction isn't B
uint32_t B_target(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_B) {
        return insn->detail->arm.operands[0].imm;
    }
    return 0; // TODO could be valid
}


// return the target of CBZ / CBNZ instruction, or 0 if current instruction isn't CBx
uint32_t CBx_target(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_CBZ || insn->id == ARM_INS_CBNZ) {
        return insn->detail->arm.operands[1].imm;
    }
    return 0; // TODO could be valid
}

// return the target of BLX instruction, or 0 if current instruction isn't BLX imm
uint32_t BLXimm_target(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_BLX && insn->detail->arm.operands[0].type == ARM_OP_IMM) {
        return insn->detail->arm.operands[0].imm;
    }
    return 0; // TODO could be valid
}


// return the target of BL instruction, or 0 if current instruction isn't BL
uint32_t BL_target(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_BL) {
        return insn->detail->arm.operands[0].imm;
    }
    return 0; // TODO could be valid
}

// as above, but also including B for tail calls
uint32_t B_BL_target(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_B || insn->id == ARM_INS_BL) {
        return insn->detail->arm.operands[0].imm;
    }
    return 0; // TODO could be valid
}

//
// as above, but also including BLX imm
uint32_t B_BL_BLXimm_target(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_B
        || insn->id == ARM_INS_BL
        || (insn->id == ARM_INS_BLX && insn->detail->arm.operands[0].type == ARM_OP_IMM)) {
        return insn->detail->arm.operands[0].imm;
    }
    return 0; // TODO could be valid
}

// BX PC (mode change, small jump) Does NOT set thumb bit
uint32_t BX_PC_target(__attribute__ ((unused))firmware *fw, cs_insn *insn)
{
    if(insn->id == ARM_INS_BX
        && insn->detail->arm.operands[0].type == ARM_OP_REG
        && insn->detail->arm.operands[0].reg == ARM_REG_PC) {
        if(insn->size == 2) { // thumb
            // per arms docs, thumb bx pc from unaligned address is "undefined"
            // assume non-instruction
            if((insn->address & 2) == 2) {
                return 0;
            }
            return (uint32_t)(insn->address) + 4;
        } else {
            return (uint32_t)(insn->address) + 8;
        }
    }
    return 0;
}

// get the (likely) range of jumptable entries from a pc relative TBB or TBH instruction
// returns 0 on error or if instruction is not TBB/TBH
// returns 1 if instruction is TBB/TBH [PC,...]
int get_TBx_PC_info(firmware *fw,iter_state_t *is, tbx_info_t *ti)
{
    if(!(is->insn->id == ARM_INS_TBH || is->insn->id == ARM_INS_TBB) || is->insn->detail->arm.operands[0].mem.base != ARM_REG_PC) {
        return 0;
    }
    ti->start=(uint32_t)is->adr; // after current instruction
    ti->first_target=0;
    ti->bytes=(is->insn->id == ARM_INS_TBH)?2:1;

    uint32_t max_adr;
    // max possible (assuming jumptable is contiguous)
    if(ti->bytes==1) {
        max_adr=ti->start+(2*255);
    } else {
        max_adr=ti->start+(2*65535);
    }
    arm_reg i_reg=is->insn->detail->arm.operands[0].mem.index;
    // backtrack looking for
    // cmp index reg,#imm
    // ...
    // bhs ...
    int max_backtrack = 8;
    if(is->ah.count - 1 < max_backtrack) {
        max_backtrack = is->ah.count-1;
    }

    int max_count=0;
    int found_bhs=0;
    int i;
    for(i=1;i<=max_backtrack;i++) {
        fw_disasm_iter_single(fw,adr_hist_get(&is->ah,i)); // thumb state comes from hist
        if(fw->is->insn->id == ARM_INS_B && fw->is->insn->detail->arm.cc == ARM_CC_HS) {
            found_bhs=1;
            continue;
        }
        // TODO lots of other ways condition code or reg could be changed in between
        if(found_bhs && fw->is->insn->id == ARM_INS_CMP) {
            // cmp with correct operands, assume number of jumptable entries
            if((arm_reg)fw->is->insn->detail->arm.operands[0].reg == i_reg
                || fw->is->insn->detail->arm.operands[1].type == ARM_OP_IMM) {
                max_count = fw->is->insn->detail->arm.operands[1].imm;
            }
            // otherwise, give up
            break;
        }
    }
    if(max_count) {
        max_adr = ti->start+max_count*ti->bytes;
        //printf("get_TBx_PC_info: max_count %d start 0x%08x max_adr=0x%08x\n",max_count,ti->start,max_adr);
    }
    uint32_t adr=ti->start;
    while(adr < max_adr) {
        uint8_t *p=adr2ptr(fw,adr);
        if(!p) {
            fprintf(stderr,"get_TBx_PC_info: jumptable outside of valid address range at 0x%08x\n",adr);
            return 0;
        }
        uint16_t off;
        if(ti->bytes==1) {
            off=(uint16_t)*p;
        } else {
            off=*(uint16_t *)p;
        }

        // 0, probably padding at the end (could probably break here)
        // note shouldn't be padding on tbh, since aligned for thumb
        if(!off) {
            break;
        }
        uint32_t target = ti->start+2*off;
        // may indicate non-jumptable entry, if count not found, so don't increment adr
        if(target <= adr) {
            fprintf(stderr,"get_TBx_PC_info: jumptable target 0x%08x inside jumptable %d at 0x%08x\n",target,off,adr);
            break;
        }
        if(!ti->first_target || target < ti->first_target) {
            ti->first_target=target;
            if(target < max_adr) {
                max_adr=target; // assume jump table ends at/before first target
            }
        }
        adr+=ti->bytes;
    }
    // if found count, assume it's right
    if(max_count) {
        ti->count=max_count;
    } else {
        // otherwise, use final address
        ti->count=(adr-ti->start)/ti->bytes;
    }
    return 1;
}

// TODO should have variants of above including LDR pc, [pc, #x] for some of the above

// ****** disassembly iterator utilities ******
// allocate a new iterator state, optionally initializing at adr (0/invalid OK)
iter_state_t *disasm_iter_new(firmware *fw, uint32_t adr)
{
    iter_state_t *is=(iter_state_t *)malloc(sizeof(iter_state_t));
    // it doesn't currently appear to matter which handle is used to allocate
    // only used for overridable malloc functions and error reporting
    is->insn=cs_malloc(fw->cs_handle_arm);
    disasm_iter_init(fw,is,adr);
    return is;
}

// free iterator state and associated resources
void disasm_iter_free(iter_state_t *is)
{
    cs_free(is->insn,1);
    free(is);
    return;
}

// set iterator to adr, without clearing history (for branch following)
// thumb bit in adr sets mode
int disasm_iter_set(firmware *fw, iter_state_t *is, uint32_t adr)
{
    // set handle based on thumb bit to allow disassembly
    if(ADR_IS_THUMB(adr)) {
        is->cs_handle=fw->cs_handle_thumb;
        is->thumb=1;
        is->insn_min_size=2;
        adr=ADR_CLEAR_THUMB(adr);// ADR used for iteration must not contain thumb bit
    } else {
        is->cs_handle=fw->cs_handle_arm;
        is->thumb=0;
        is->insn_min_size=4;
        if(!ADR_IS_ALIGN4(adr)) {
            fprintf(stderr,"disasm_iter_set: unaligned ARM address 0x%08x\n",adr);
            is->code=NULL;
            is->size=0;
            is->adr=0;
            return 0;
        }
    }
    uint8_t *p=adr2ptr(fw,adr);
    if(!p) {
// TODO invalid currently allowed, for new
//        fprintf(stderr,"disasm_iter_set: bad address 0x%08x\n",adr);
        is->code=NULL; // make first iter fail
        is->size=0;
        is->adr=0;
        return 0;
    }
    // TODO should maybe mark is.insn invalid?
    is->code=p;
    is->size=fw->size8 - (p-fw->buf8);
    is->adr=adr;
    return 1;
}

// initialize iterator state at adr, clearing history
int disasm_iter_init(__attribute__ ((unused))firmware *fw, iter_state_t *is, uint32_t adr)
{
    adr_hist_reset(&is->ah);
    return disasm_iter_set(fw,is,adr);
}

// disassemble next instruction, recording address in history
// returns false if state invalid or disassembly fails
// if disassembly fails, is->adr is not incremented
int disasm_iter(__attribute__ ((unused))firmware *fw, iter_state_t *is)
{
    // iter_start not called or invalid
    if(!is->code) {
        return 0;
    }
    adr_hist_add(&is->ah,(uint32_t)is->adr | is->thumb); // record thumb state to allow backtracking through state changes
    return cs_disasm_iter(is->cs_handle, &is->code, &is->size, &is->adr, is->insn);
}

// re-disassemble the current instruction
// could be useful if turning detail off/on but doesn't seem to help perf much
// NOTE out of date
#if 0
int disasm_iter_redo(firmware *fw,iter_state_t *is) {
    if(!is->code || !is->ah.count) {
        return 0;
    }
    is->code -= is->insn->size;
    is->adr -= is->insn->size;
    is->size += is->insn->size;
    // call iter directly, to avoid touching history
    return cs_disasm_iter(is->cs_handle, &is->code, &is->size, &is->adr, is->insn);
}
#endif

// ***** disassembly utilities operating on the default iterator state *****
/*
initialize iter state to begin iterating at adr
history is cleared
*/
int fw_disasm_iter_start(firmware *fw, uint32_t adr)
{
    return disasm_iter_init(fw,fw->is,adr);
}

// disassemble the next instruction, updating cached state
int fw_disasm_iter(firmware *fw)
{
    return disasm_iter(fw,fw->is);
}

// disassemble single instruction at given adr, updating cached values
// history is cleared
int fw_disasm_iter_single(firmware *fw, uint32_t adr)
{
    fw_disasm_iter_start(fw,adr);
    return fw_disasm_iter(fw);
}


// ****** standalone disassembly without an iter_state ******
/*
disassemble up to count instructions starting at firmware address adr
allocates and returns insns in insn, can be freed with cs_free(insn, count)
*/
#if 0
size_t fw_disasm_adr(firmware *fw, uint32_t adr, unsigned count, cs_insn **insn)
{
    uint8_t *p=adr2ptr(fw,adr);
    if(!p) {
        *insn=NULL; // ?
        return 0;
    }
    return cs_disasm(fw->cs_handle, p, fw->size8 - (p-fw->buf8), adr, count, insn);
}
#endif

// ***** utilities for searching disassembly over large ranges ******
/*
iterate over firmware disassembling, calling callback described above after each
successful disassembly iteration.  If disassembly fails, the iter state is advanced
minimum instruction size without calling the callback.
starts at address is taken from the iter_state, which should be initialized with
disasm_iter_new(), disasm_iter_init(), or a previous search or iter call.
end defaults to end of ram code or rom code (before init data, if known), based on start
v1 and udata are provided to the callback
*/
uint32_t fw_search_insn(firmware *fw, iter_state_t *is, search_insn_fn f, uint32_t v1, void *udata, uint32_t adr_end)
{
    uint32_t adr_start=is->adr;
    adr_range_t *r_start=adr_get_range(fw,adr_start);
    if(!r_start) {
        fprintf(stderr,"fw_search_insn: invalid start address 0x%08x\n",adr_start);
        return 0;
    }

    // default to end of start range
    if(!adr_end) {
        if(r_start->type == ADR_RANGE_ROM) {
            adr_end = fw->rom_code_search_max_adr;
        } else {
            adr_end=r_start->start + r_start->bytes - is->insn_min_size;
        }
    }
    adr_range_t *r_end=adr_get_range(fw,adr_end);

    if(!r_end) {
        fprintf(stderr,"fw_search_insn: invalid end address 0x%08x\n",adr_end);
        return 0;
    }
    // ignore thumb bit on end adr
    adr_end=ADR_CLEAR_THUMB(adr_end);

    if((r_start != r_end) || (adr_end < adr_start)) {
        fprintf(stderr,"fw_search_insn: invalid address range 0x%08x 0x%08x\n",adr_start,adr_end);
        return 0;
    }

    uint32_t adr=adr_start;
    // don't bother with buf ranges for RAM code
    if(r_start->type != ADR_RANGE_ROM) {
        while(adr < adr_end) {
            if(disasm_iter(fw,is)) {
                uint32_t r=f(fw,is,v1,udata);
                if(r) {
                    return r;
                }
                adr=(uint32_t)is->adr; // adr was updated by iter or called sub
            } else {
                // disassembly failed
                // increment by minimum instruction size and re-init
                adr=adr+is->insn_min_size;
                if(!disasm_iter_init(fw,is,adr|is->thumb)) {
                    fprintf(stderr,"fw_search_insn: disasm_iter_init failed\n");
                    return 0;
                }
             }
        }
        return 0;
    }
    BufRange *br=fw->br;
    // TODO might want to (optionally?) turn off details? For now, caller can set, doesn't seem to help perf much
    // TODO when searching ROM, could skip over RAM copied areas (currently just limit default range)
    while(br && adr < adr_end) {
        uint32_t *p_adr=(uint32_t *)adr2ptr(fw,(uint32_t)adr);
        uint32_t *br_end = br->p + br->len;
        uint32_t adr_chunk_end = ptr2adr(fw,(uint8_t*)br_end);
        if(adr_end < adr_chunk_end) {
            adr_chunk_end = adr_end;
        }
        // address is before start of current range, adjust
        if(p_adr < br->p) {
            adr=ptr2adr(fw,(uint8_t *)br->p);
            if(!disasm_iter_init(fw,is,(uint32_t)adr | is->thumb)) {
                return 0;
            }
            p_adr=(uint32_t *)adr2ptr(fw,(uint32_t)adr);
        }
        //printf("br:0x%08x-0x%08x\n",ptr2adr(fw,(uint8_t *)br->p),ptr2adr(fw,(uint8_t *)(br->p+br->len)));
        while(adr < adr_chunk_end) {
            if(disasm_iter(fw,is)) {
                uint32_t r=f(fw,is,v1,udata);
                if(r) {
                    return r;
                }
                adr=(uint32_t)is->adr; // adr was updated by iter or called sub
            } else {
                // disassembly failed. cs_disarm_iter does not update address
                // increment by half word and re-init
                adr=adr+is->insn_min_size;
                if(!disasm_iter_init(fw,is,adr|is->thumb)) {
                    fprintf(stderr,"fw_search_insn: disasm_iter_init failed\n");
                    return 0;
                }
             }
        }
        // next range
        br=br->next;
    }
    return 0;
}

// ****** callbacks for use with fw_search_insn ******

// search for constant references
uint32_t search_disasm_const_ref(firmware *fw, iter_state_t *is, uint32_t val, __attribute__ ((unused))void *unused)
{
//    printf("%"PRIx64" %s %s\n",is->insn->address,is->insn->mnemonic, is->insn->op_str);
    uint32_t av=ADRx2adr(fw,is->insn);
    if(av) {
//        printf("adr 0x%08x\n",av);
        if(av == val) {
            return (uint32_t)is->insn->address;
        }
        return 0;
    }
    uint32_t *pv=LDR_PC2valptr(fw,is->insn);
    if(pv) {
//        printf("ldr 0x%08x\n",*pv);
        if(*pv == val) {
            return (uint32_t)is->insn->address;
        }
    }
    return 0;
}

// search for string ref
uint32_t search_disasm_str_ref(firmware *fw, iter_state_t *is, __attribute__ ((unused))uint32_t val, void *udata)
{
    const char *str=(const char *)udata;
//    printf("%"PRIx64" %s %s\n",is->insn->address,is->insn->mnemonic, is->insn->op_str);
    uint32_t av=ADRx2adr(fw,is->insn);
    if(av) {
//        printf("adr 0x%08x\n",av);
        char *cmp=(char *)adr2ptr_with_data(fw,av);
        if(cmp && (strcmp(cmp,str) == 0)) {
            return (uint32_t)is->insn->address;
        }
        return 0;
    }
    uint32_t *pv=LDR_PC2valptr(fw,is->insn);
    if(pv) {
//        printf("ldr 0x%08x\n",*pv);
        char *cmp=(char *)adr2ptr_with_data(fw,*pv);
        if(cmp && (strcmp(cmp,str) == 0)) {
            return (uint32_t)is->insn->address;
        }
    }
    return 0;
}

// search for calls/jumps to immediate addresses
// thumb bit in address should be set appropriately
// returns 1 if found, address can be obtained from insn
uint32_t search_disasm_calls(firmware *fw, iter_state_t *is, uint32_t val, __attribute__ ((unused))void *unused)
{
    //printf("%"PRIx64" %s %s\n",is->insn->address,is->insn->mnemonic, is->insn->op_str);
    uint32_t sub=get_branch_call_insn_target(fw,is);
    if(sub) {
        if(sub == val) {
            return 1;
        }
    }
    return 0;
}

// a search_calls_multi_fn that just returns 1
int search_calls_multi_end(__attribute__ ((unused))firmware *fw, __attribute__ ((unused))iter_state_t *is, __attribute__ ((unused))uint32_t adr) {
    return 1;
}


// Search for calls to multiple functions (more efficient than multiple passes)
// if adr is found in null terminated search_calls_multi_data array, returns fn return value
// otherwise 0
uint32_t search_disasm_calls_multi(firmware *fw, iter_state_t *is, __attribute__ ((unused))uint32_t unused, void *userdata)
{
    search_calls_multi_data_t *data=(search_calls_multi_data_t *)userdata;
    uint32_t sub=get_branch_call_insn_target(fw,is);
    if(sub) {
        while(data->adr) {
            if(data->adr == sub) {
                return data->fn(fw,is,sub);
            }
            data++;
        }
    }
    return 0;
}

// as above, but check for single level of veneer
uint32_t search_disasm_calls_veneer_multi(firmware *fw, iter_state_t *is, __attribute__ ((unused))uint32_t unused, void *userdata)
{
    search_calls_multi_data_t *data=(search_calls_multi_data_t *)userdata;
    uint32_t sub=get_branch_call_insn_target(fw,is);
    if(sub) {
        while(data->adr) {
            if(data->adr == sub) {
                return data->fn(fw,is,sub);
            }
            data++;
        }
        uint32_t veneer=0;
        fw_disasm_iter_single(fw,sub);
        veneer=get_branch_call_insn_target(fw,fw->is);
        data=(search_calls_multi_data_t *)userdata;
        while(data->adr) {
            if(data->adr == veneer) {
                return data->fn(fw,is,sub);
            }
            data++;
        }
    }
    return 0;
}

// ****** utilities for extracting register values ******
/*
backtrack through is_init state history picking up constants loaded into r0-r3
return bitmask of regs with values found
affects fw->is, does not affect is_init

NOTE values may be inaccurate for many reasons, doesn't track all reg affecting ops,
doesn't account for branches landing in the middle of inspected code
doesn't account for many conditional cases
*/
int get_call_const_args(firmware *fw, iter_state_t *is_init, int max_backtrack, uint32_t *res)
{
    int i;
    /*
    static int dbg_count=0;
    if(is_init->insn->address==...) {
        dbg_count=1;
    } else {
        dbg_count=0;
    }
    */

    int rn_ldr[4];  // Track ldr Rn,(Rn,offset) instructions

    // init regs to zero (to support adds etc)
    for (i=0;i<4;i++) {
        res[i] = 0;
        rn_ldr[i] = 0;
    }

    // count includes current instruction (i.e. BL of call)
    if(is_init->ah.count <= 1) {
        return 0;
    }
    if(is_init->ah.count - 1 < max_backtrack) {
        /*
        if(dbg_count > 0) {
            printf("max_backtrack %d hist count %d\n",max_backtrack,is_init->ah.count);
        }
        */
        max_backtrack = is_init->ah.count-1;
    }
    uint32_t found_bits=0; // registers with known const values
    uint32_t known_bits=0; // registers with some value

    for(i=1;i<=max_backtrack && known_bits !=0xf;i++) {
        // TODO going backwards and calling start each time inefficient
        // forward could also find multi-instruction constants in some cases (e.g mov + add, movw + movt)
        fw_disasm_iter_single(fw,adr_hist_get(&is_init->ah,i)); // thumb state comes from hist
        /*
        if(dbg_count > 0) {
            printf("backtrack %d:%d  ",dbg_count,i);
            printf("%"PRIx64" %s %s\n",fw->is->insn->address,fw->is->insn->mnemonic, fw->is->insn->op_str);
        }
        */
        arm_insn insn_id = fw->is->insn->id;
        // BL, BLX etc will trash r0-r3
        // only break on unconditional - optimistic, could produce incorrect results
        if((insn_id == ARM_INS_BL || insn_id == ARM_INS_BLX
            // B/BX could mean execution goes somewhere totally different, but in practice it often just skipping over a word of data...
             /*|| insn_id == ARM_INS_B || insn_id == ARM_INS_BX*/)
             && fw->is->insn->detail->arm.cc == ARM_CC_AL) {
            break;
        }

        // if the first op isn't REG, continue
        // TODO lots of instructions could affect reg even if not first op
        if(fw->is->insn->detail->arm.operands[0].type != ARM_OP_REG) {
            continue;
        }
        arm_reg rd = fw->is->insn->detail->arm.operands[0].reg;
        // capstone arm.h regs enum R0-R12 are ordered
        // enum has entries before R0
        if(rd < ARM_REG_R0 || rd > ARM_REG_R3) {
            continue;
        }

        int rd_i = rd - ARM_REG_R0;
        uint32_t rd_bit = 1 << rd_i;
        // if we don't already have something for this reg
        if(!(known_bits & rd_bit)) {
            // know something has been done to this reg
            // note doesn't account for conditionals
            known_bits |=rd_bit;
            // is it an LDR
            uint32_t *pv=LDR_PC2valptr(fw,fw->is->insn);
            if(pv) {
                res[rd_i] += *pv;
                if (rn_ldr[rd_i]) {
                    // Later instruction is ldr Rn,(Rn,offset)
                    pv = (uint32_t *)adr2ptr(fw,res[rd_i]);
                    res[rd_i] = *pv;
                }
//                if(dbg_count) printf("found ldr r%d,=0x%08x\n",rd_i,res[rd_i]);
                found_bits |=rd_bit;
                continue;
            }
            uint32_t v=ADRx2adr(fw,fw->is->insn); // assumes ADR doesn't generate 0, probably safe
            if(v) {
                res[rd_i] += v;
//                 if(dbg_count) printf("found adrx r%d,0x%08x\n",rd_i,res[rd_i]);
                found_bits |=rd_bit;
                continue;
            }
            // immediate MOV note MOVT combinations, not accounted for, some handled ADDs below
            if( IS_INSN_ID_MOVx(insn_id)
                && fw->is->insn->detail->arm.operands[1].type == ARM_OP_IMM) {
                res[rd_i] += fw->is->insn->detail->arm.operands[1].imm;
//                if(dbg_count) printf("found move r%d,#0x%08x\n",rd_i,res[rd_i]);
                found_bits |=rd_bit;
            } else if(isADDx_imm(fw->is->insn)) {
                res[rd_i] += fw->is->insn->detail->arm.operands[1].imm;
//                if(dbg_count) printf("found add r%d,#0x%08x\n",rd_i,res[rd_i]);
                // pretend reg is not known
                known_bits ^=rd_bit;
                // do not set found bit here
            } else if(isSUBx_imm(fw->is->insn)) {
                res[rd_i] = (int)(res[rd_i]) - fw->is->insn->detail->arm.operands[1].imm;
//                if(dbg_count) printf("found add r%d,#0x%08x\n",rd_i,res[rd_i]);
                // pretend reg is not known
                known_bits ^=rd_bit;
                // do not set found bit here
            } else if (isLDR_Rn(fw->is->insn, rd)) {
                // Load register from memory pointed to by itself
                res[rd_i] = fw->is->insn->detail->arm.operands[1].mem.disp;
                rn_ldr[rd_i] = 1;
                // pretend reg is not known
                known_bits ^=rd_bit;
            } else if ((insn_id == ARM_INS_SUB) && (fw->is->insn->detail->arm.operands[1].reg == (int)rd)) {
                // subs Rn,Rn,#x
                res[rd_i] = res[rd_i] - fw->is->insn->detail->arm.operands[2].imm;
                // pretend reg is not known
                known_bits ^=rd_bit;
            }
        }
    }
//    if(dbg_count) printf("get_call_const_args found 0x%08x\n",found_bits);
    return found_bits;
}

/*
starting from is_init, look for a direct jump, such as
 B <target>
 LDR PC, [pc, #x]
 BX PC
 movw ip, #x
 movt ip, #x
 bx ip
if found, return target address with thumb bit set appropriately
NOTE does not check for conditional
uses fw->is
does not check CBx, since it would generally be part of a function not a veneer
*/
uint32_t get_direct_jump_target(firmware *fw, iter_state_t *is_init)
{
    uint32_t adr=B_target(fw,is_init->insn);
    // B ... return with thumb set to current mode
    if(adr) {
        return (adr | is_init->thumb);
    }
    adr=LDR_PC_PC_target(fw,is_init->insn);
    // LDR pc #... thumb is set in the loaded address
    if(adr) {
        return adr;
    }
    // BX PC
    adr=BX_PC_target(fw,is_init->insn);
    if(adr) {
        // bx swaps mode
        if(is_init->thumb) {
            return ADR_CLEAR_THUMB(adr);
        } else {
            return ADR_SET_THUMB(adr);
        }
    }
    // an immediate move to ip (R12), candidate for multi-instruction veneer
    if((is_init->insn->id == ARM_INS_MOV || is_init->insn->id == ARM_INS_MOVW)
        && is_init->insn->detail->arm.operands[0].reg == ARM_REG_IP
        && is_init->insn->detail->arm.operands[1].type == ARM_OP_IMM) {
        adr = is_init->insn->detail->arm.operands[1].imm;
        // iter in default state, starting from is_init
        if(!fw_disasm_iter_single(fw,is_init->adr | is_init->thumb)) {
            fprintf(stderr,"get_direct_jump_target: disasm single failed at 0x%"PRIx64"\n",fw->is->insn->address);
            return 0;
        }
        // check for MOVT ip, #x
        if(!(fw->is->insn->id == ARM_INS_MOVT
            && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_IP
            && fw->is->insn->detail->arm.operands[1].type == ARM_OP_IMM)) {
// doesn't match second two insn veneer, not really an error
//            fprintf(stderr,"get_direct_jump_target: not 2 insn ip veneer 0x%"PRIx64"\n",fw->is->insn->address);
            return 0;
        }
        // thumb set in loaded adr
        adr = (fw->is->insn->detail->arm.operands[1].imm << 16) | (adr&0xFFFF);
        if(!fw_disasm_iter(fw)) {
            fprintf(stderr,"get_direct_jump_target: disasm 2 failed at 0x%"PRIx64"\n",fw->is->insn->address);
            return 0;
        }
        // BX ip ?
        if(fw->is->insn->id == ARM_INS_BX
            && fw->is->insn->detail->arm.operands[0].type == ARM_OP_REG
            && fw->is->insn->detail->arm.operands[0].reg == ARM_REG_IP) {
            return adr;
        }
    }
    return 0;
}

/*
return target of any single instruction branch or function call instruction,
with thumb bit set appropriately
returns 0 if current instruction not branch/call
*/
uint32_t get_branch_call_insn_target(firmware *fw, iter_state_t *is)
{
    uint32_t adr=B_BL_target(fw,is->insn);
    if(adr) {
        return (adr | is->thumb);
    }
    // CBx only exists in thumb
    if(is->thumb) {
        adr=CBx_target(fw,is->insn);
        if(adr) {
            return ADR_SET_THUMB(adr);
        }
    }

    adr=BLXimm_target(fw,is->insn);
    if(adr) {
        if(is->thumb) {
            return adr;
        } else {
            return adr | is->thumb;
        }
    }

    adr=LDR_PC_PC_target(fw,is->insn);
    if(adr) {
        return adr;
    }
    adr=BX_PC_target(fw,is->insn);
    if(adr) {
        // bx swaps mode
        if(is->thumb) {
            return ADR_CLEAR_THUMB(adr);
        } else {
            return ADR_SET_THUMB(adr);
        }
    }
    return 0;
}

/*
search up to max_search_ins for first LDR, =value
and then match up to max_seq_insns for a sequence like
LDR Rbase,=adr
... possible intervening ins
SUB Rbase,#adj // optional, may be any add/sub variant
... possible intervening ins
LDR Rval,[Rbase + #off]

returns 1 if found, 0 if not
stores registers and constants in *result if successful

NOTE bad values are possible with intervening ins, short sequences recommended

TODO similar code for STR would be useful, but in many cases would have to handle load or move into reg_val
*/
int find_and_get_var_ldr(firmware *fw,
                            iter_state_t *is,
                            int max_search_insns,
                            int max_seq_insns,
                            arm_reg match_val_reg, // ARM_REG_INVALID for any
                            var_ldr_desc_t *result)

{
    if(!insn_match_find_next(fw,is,max_search_insns,match_ldr_pc)) {
        // printf("find_and_get_var_ldr: LDR PC not found\n");
        return 0;
    }
    var_ldr_desc_t r;
    memset(&r,0,sizeof(r));
    r.reg_base=is->insn->detail->arm.operands[0].reg;
    r.adr_base=LDR_PC2val(fw,is->insn);
    int seq_count=1;

    while(seq_count < max_seq_insns) {
        // disassembly failed, no match (could ignore..)
        if(!disasm_iter(fw,is)) {
            return 0;
        }
        // assume first encountered LDR x,[pc] is the one to use
        // give up if we encounter another. Don't know beforehand which reg is base
        // NOTE: backward search would allow matching base that eventually ends up in desired reg
        if(isLDR_PC(is->insn)) {
            // printf("find_and_get_var_ldr: second ldr pc\n");
            return  0;
        }
        seq_count++;
        // firmware may use add/sub to get actual firmware base address
        if(isADDx_imm(is->insn) || isSUBx_imm(is->insn)) {
            if((arm_reg)is->insn->detail->arm.operands[0].reg != r.reg_base) {
                continue;
            }
            if(isADDx_imm(is->insn)) {
                r.adj=is->insn->detail->arm.operands[1].imm;
            } else {
                r.adj=-is->insn->detail->arm.operands[1].imm;
            }
            if(!disasm_iter(fw,is)) {
                return 0;
            }
            seq_count++;
        } else {
            r.adj=0;
        }
        // try to bail out if base reg trashed
        // BL, BLX etc will trash r0-r3, B, BX go somewhere else
        // only break on unconditional - optimistic, could produce incorrect results
        // can't account for branches into searched code
        if((r.reg_base >= ARM_REG_R0 && r.reg_base <= ARM_REG_R3)
                && (is->insn->id == ARM_INS_BL || is->insn->id == ARM_INS_BLX
                    || is->insn->id == ARM_INS_B || is->insn->id == ARM_INS_BX)
                && is->insn->detail->arm.cc == ARM_CC_AL) {
            // printf("find_and_get_var_ldr: bail B*\n");
            return 0;
        }
        if(is->insn->id != ARM_INS_LDR || (arm_reg)is->insn->detail->arm.operands[1].reg != r.reg_base) {
            // other operation on with base reg as first operand, give up
            // simplistic, many other things could affect reg
            if(is->insn->detail->arm.operands[0].type == ARM_OP_REG && (arm_reg)is->insn->detail->arm.operands[0].reg == r.reg_base) {
                // printf("find_and_get_var_ldr: bail mod base\n");
                return 0;
            }
            continue;
        }
        r.reg_val = is->insn->detail->arm.operands[0].reg;
        if(match_val_reg != ARM_REG_INVALID && (r.reg_val != match_val_reg)) {
            continue;
        }
        r.off = is->insn->detail->arm.operands[1].mem.disp;
        r.adr_adj = r.adr_base + r.adj;
        r.adr_final = r.adr_adj + r.off;
        memcpy(result,&r,sizeof(r));
        return 1;
    }
    return 0;
}

/*
find instruction or sequence that receives specified constant in specified r0-r3 reg
search starting from is to max_search_bytes
allow up to max_gap_insns between constant load and match, generally small (4-8 max)
returns address of match with thumb bit set according to mode, or 0 on failure
*/
int find_const_ref_match(firmware *fw,
                            iter_state_t *is,
                            int max_search_bytes,
                            int max_gap_insns,
                            arm_reg match_reg, // must be R0-R3
                            uint32_t val,
                            const insn_match_t *match,
                            int match_type)
{
    if(match_reg < ARM_REG_R0 || match_reg > ARM_REG_R3) {
        fprintf(stderr,"find_const_ref_match: invalid match_reg %d\n",match_reg);
        return 0;
    }
    if(max_gap_insns >= ADR_HIST_SIZE) {
        fprintf(stderr,"find_const_ref_match: invalid max_gap_insns %d\n",max_gap_insns);
        return 0;
    }
    int (*match_fn)(firmware *fw, iter_state_t *is, int max_insns, const insn_match_t *match);
    if(match_type == FIND_CONST_REF_MATCH_SEQ) {
        match_fn = insn_match_find_next_seq;
    } else if(match_type == FIND_CONST_REF_MATCH_ANY){
        match_fn = insn_match_find_next;
    } else {
        fprintf(stderr,"find_const_ref_match: invalid match_type %d\n",match_type);
        return 0;
    }
    // search for a ref to constant
    while(fw_search_insn(fw,is,search_disasm_const_ref,val,NULL,(uint32_t)(is->adr+max_search_bytes))) {
        //printf("find_const_ref_match: match str 0x%"PRIx64"\n",is->adr);
        uint32_t next_adr = (uint32_t)is->adr;
        // search for next bl / blx
        // could search include b for tail calls, but hard to distinguish
        if(match_fn(fw,is,max_gap_insns,match)) {
            uint32_t reg_num = match_reg - ARM_REG_R0;
            uint32_t reg_bit = 1 << reg_num;
            uint32_t regs[4];
            //printf("find_const_ref_match: match insn 0x%"PRIx64"\n",reg_num,is->adr);
            // backtrack to find out if const ref ends up in desired reg
            if((get_call_const_args(fw,is,max_gap_insns,regs)&reg_bit)==reg_bit) {
                //printf("find_const_ref_match: match reg r%d 0x%"PRIx64"\n",reg_num,is->adr);
                if(regs[reg_num] == val) {
                    return iter_state_adr(is);
                }
            }
        }
        // not matched, restore is and advance one instruction
        disasm_iter_init(fw,is,next_adr | is->thumb);
    }
    return 0;
}

/*
find call that receives specified constant in specified r0-r3 reg
search starting from is to max_search_bytes
allow up to max_gap_insns between constant load and call, generally small (4-8 max)
returns address of call with thumb bit set according to mode, or 0 on failure
*/
int find_const_ref_call(firmware *fw,
                            iter_state_t *is,
                            int max_search_bytes,
                            int max_gap_insns,
                            arm_reg match_reg, // must be R0-R3
                            uint32_t val)

{
    return find_const_ref_match(fw,is,max_search_bytes,max_gap_insns,match_reg,val,match_bl_blximm,FIND_CONST_REF_MATCH_ANY);
}

/*
check for, and optionally return information about
functions with return values that can be completely determined
from disassembly
uses fw->is
*/
// constants below may  as flags on input, and as return valaue
// no simple function found
#define MATCH_SIMPLE_FUNC_NONE    0x0
// immediately returns, with no value
#define MATCH_SIMPLE_FUNC_NULLSUB 0x1
// immediately returns with a MOV constant
#define MATCH_SIMPLE_FUNC_IMM     0x2
// TODO LDR pc, =const,  ADR
// TODO could also do pointer derefs and return pointer info without val
#define MATCH_SIMPLE_FUNC_ANY     0x3
int check_simple_func(firmware *fw, uint32_t adr, int match_ftype, simple_func_desc_t *info)
{
    const insn_match_t match_mov_r0_imm[]={
        {MATCH_INS(MOV,   2),  {MATCH_OP_REG(R0),  MATCH_OP_IMM_ANY}},
#if CS_API_MAJOR < 4
        {MATCH_INS(MOVS,  2),  {MATCH_OP_REG(R0),  MATCH_OP_IMM_ANY}},
#endif
        {ARM_INS_ENDING}
    };

    int found = 0;
    int found_val = 0;
    if(info) {
        info->ftype = MATCH_SIMPLE_FUNC_NONE;
        info->retval = 0;
    }
    if(!fw_disasm_iter_single(fw,adr)) {
        //fprintf(stderr,"check_simple_func: disasm_iter_single failed 0x%x\n",adr);
        return 0;
    }
    if(match_ftype & MATCH_SIMPLE_FUNC_IMM) {
        // check mov r0, #imm
        if(insn_match_any(fw->is->insn,match_mov_r0_imm)) {
            found_val = fw->is->insn->detail->arm.operands[1].imm;
            found = MATCH_SIMPLE_FUNC_IMM;
            // fprintf(stderr,"check_simple_func: found IMM\n");
            if(!fw_disasm_iter(fw)) {
                //fprintf(stderr,"check_simple_func: disasm_iter failed 0x%x\n",adr);
                return 0;
            }
        }
    }
    if(!isRETx(fw->is->insn)) {
        // fprintf(stderr,"check_simple_func: no ret\n");
        return 0;
    }
    // no previous found, check if ret alone
    if(!found && (match_ftype & MATCH_SIMPLE_FUNC_NULLSUB)) {
        found = MATCH_SIMPLE_FUNC_NULLSUB;
        // fprintf(stderr,"check_simple_func: found nullsub\n");
    }
    if(found) {
        if(info) {
            info->ftype = found;
            info->retval = found_val;
        }
    }
    return found;
}

/*
advance iter_state is trying to find the last function called by a function
function assumed to PUSH LR, POP LR or PC (many small functions don't!)
either the last BL/BLXimm before pop {... PC}
or B after POP {... LR}
MOV, ADR, ADD, SUB and LDR to R0-R3 are allowed between POP LR and the final B
If a POP occurs before min_insns, the match fails
Calls before min_insns are ignored
*/
uint32_t find_last_call_from_func(firmware *fw, iter_state_t *is,int min_insns, int max_insns)
{
    int push_found=0;
    uint32_t last_adr=0;
    int count;
    for(count=0; count < max_insns; count++) {
        if(!disasm_iter(fw,is)) {
            fprintf(stderr,"find_last_call_from_func: disasm failed 0x%"PRIx64"\n",is->adr);
            return 0;
        }
        // TODO could match push regs with pop
        if(isPUSH_LR(is->insn)) {
            // already found a PUSH LR, probably in new function
            if(push_found) {
                //printf("find_last_call_from_func: second push pc 0x%"PRIx64"\n",is->adr);
                return 0;
            }
            push_found=1;
            continue;
        }
        // ignore everything before push (could be some mov/ldr, shouldn't be any calls)
        // TODO may want to allow starting in the middle of a function
        if(!push_found) {
            continue;
        }
        // found a potential call, store
        if(insn_match_any(is->insn,match_bl_blximm) && count >= min_insns) {
            //printf("find_last_call_from_func: found call 0x%"PRIx64"\n",is->adr);
            last_adr=get_branch_call_insn_target(fw,is);
            continue;
        }
        // found pop PC, can only be stored call if present
        if(isPOP_PC(is->insn)) {
            // printf("find_last_call_from_func: found pop PC 0x%"PRIx64"\n",is->adr);
            if(last_adr) {
                return last_adr;
            }
            // no call found, or not found within min
            return 0;
        }
        // found pop LR, check if next is allowed tail sequence followed by unconditional B
        if(isPOP_LR(is->insn)) {
            // hit func end with less than min, no match
            if(count < min_insns) {
                // printf("find_last_call_from_func: pop before min 0x%"PRIx64"\n",is->adr);
                return 0;
            }
            if(!disasm_iter(fw,is)) {
                fprintf(stderr,"find_last_call_from_func: disasm failed 0x%"PRIx64"\n",is->adr);
                return 0;
            }
            // allow instructions likely to appear between pop and tail call
            // MOV, ADR, ADD/SUB or LDR to r0-r3
            // others are possible
            const insn_match_t match_tail[]={
                {MATCH_INS(MOV, MATCH_OPCOUNT_ANY), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_REST_ANY}},
// MOVS unlikely to be valid, though possible if followed by additional conditional instructions
// in any case, want to match capstone 4 behavior
#if CS_API_MAJOR < 4
                {MATCH_INS(MOV, MATCH_OPCOUNT_ANY), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_REST_ANY}},
#endif

                {MATCH_INS(LDR, 2), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_ANY}},
                {MATCH_INS(ADR, MATCH_OPCOUNT_ANY), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_REST_ANY}},
                {MATCH_INS(ADD, MATCH_OPCOUNT_ANY), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_REST_ANY}},
                {MATCH_INS(ADDW, MATCH_OPCOUNT_ANY), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_REST_ANY}},
                {MATCH_INS(SUB, MATCH_OPCOUNT_ANY), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_REST_ANY}},
                {MATCH_INS(SUBW, MATCH_OPCOUNT_ANY), {MATCH_OP_REG_RANGE(R0,R3), MATCH_OP_REST_ANY}},
                {ARM_INS_ENDING}
            };
            while(insn_match_any(is->insn,match_tail) && count < max_insns) {
                if(!disasm_iter(fw,is)) {
                    fprintf(stderr,"find_last_call_from_func: disasm failed 0x%"PRIx64"\n",is->adr);
                    return 0;
                }
                count++;
            }
            if(is->insn->id == ARM_INS_B && is->insn->detail->arm.cc == ARM_CC_AL) {
                return get_branch_call_insn_target(fw,is);
            }
            // don't go more than one insn after pop (could be more, but uncommon)
            // printf("find_last_call_from_func: more than one insn after pop 0x%"PRIx64"\n",is->adr);
            return 0;
        }
        // found another kind of ret, give up
        if(isRETx(is->insn)) {
            // printf("find_last_call_from_func: other ret 0x%"PRIx64"\n",is->adr);
            return 0;
        }
    }
    // printf("find_last_call_from_func: no match in range 0x%"PRIx64"\n",is->adr);
    return 0;
}

// ****** utilities for matching instructions and instruction sequences ******

// some common matches for insn_match_find_next
const insn_match_t match_b[]={
    {MATCH_INS(B,   MATCH_OPCOUNT_IGNORE)},
    {ARM_INS_ENDING}
};
const insn_match_t match_bl[]={
    {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
    {ARM_INS_ENDING}
};
const insn_match_t match_b_bl[]={
    {MATCH_INS(B,   MATCH_OPCOUNT_IGNORE)},
    {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
    {ARM_INS_ENDING}
};

const insn_match_t match_b_bl_blximm[]={
    {MATCH_INS(B,   MATCH_OPCOUNT_IGNORE)},
    {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
    {MATCH_INS(BLX, 1), {MATCH_OP_IMM_ANY}},
    {ARM_INS_ENDING}
};

const insn_match_t match_bl_blximm[]={
    {MATCH_INS(BL,  MATCH_OPCOUNT_IGNORE)},
    {MATCH_INS(BLX, 1), {MATCH_OP_IMM_ANY}},
    {ARM_INS_ENDING}
};

const insn_match_t match_bxlr[]={
    {MATCH_INS(BX, 1), {MATCH_OP_REG(LR)}},
    {ARM_INS_ENDING}
};

const insn_match_t match_bxreg[]={
    {MATCH_INS(BX, 1), {MATCH_OP_REG_ANY}},
    {ARM_INS_ENDING}
};

const insn_match_t match_blxreg[]={
    {MATCH_INS(BLX, 1), {MATCH_OP_REG_ANY}},
    {ARM_INS_ENDING}
};

const insn_match_t match_ldr_pc[]={
    {MATCH_INS(LDR, 2), {MATCH_OP_REG_ANY,  MATCH_OP_MEM_BASE(PC)}},
    {ARM_INS_ENDING}
};

// iterate as long as sequence of instructions matches sequence defined in match
int insn_match_seq(firmware *fw, iter_state_t *is, const insn_match_t *match)
{
    //printf("%"PRIx64" insn_match_seq %s %s\n",is->insn->address,is->insn->mnemonic,is->insn->op_str);
    while(match->id != ARM_INS_ENDING && disasm_iter(fw,is) && insn_match(is->insn,match)) {
        //printf("%"PRIx64" insn_match_seq next %s %s\n",is->insn->address,is->insn->mnemonic,is->insn->op_str);
        match++;
    }
    return (match->id == ARM_INS_ENDING);
}

// capstone enum isn't in numeric order, (SP through PC in capstone 4, but probably shouldn't assume)
static const arm_reg reg_order[] = {
    ARM_REG_R0,
    ARM_REG_R1,
    ARM_REG_R2,
    ARM_REG_R3,
    ARM_REG_R4,
    ARM_REG_R5,
    ARM_REG_R6,
    ARM_REG_R7,
    ARM_REG_R8,
    ARM_REG_R9,
    ARM_REG_R10,
    ARM_REG_R11,
    ARM_REG_R12,
    ARM_REG_SP,
    ARM_REG_LR,
    ARM_REG_PC,
};

int reg_in_range(arm_reg r, arm_reg min_reg, arm_reg max_reg)
{
    int c = -1, c_min = -1, c_max = -1;
    int i;
    for(i=0; i<(int)(sizeof(reg_order)/sizeof(arm_reg)); i++) {
        if(reg_order[i] == r) {
            c = i;
        }
        if(reg_order[i] == min_reg) {
            c_min = i;
        }
        if(reg_order[i] == max_reg) {
            c_max = i;
        }
    }
    // any invalid / unlisted regs, false
    if( c < 0 || c_min < 0 || c_max < 0) {
        return 0;
    }
    return (c >= c_min && c <= c_max);
}

// check if single insn matches values defined by match
int insn_match(cs_insn *insn,const insn_match_t *match)
{
    // specific instruction ID requested, check
    if(match->id != ARM_INS_INVALID && insn->id != match->id) {
        return 0;
    }
    // condition code requested, check
    if(match->cc != ARM_CC_INVALID && insn->detail->arm.cc != match->cc) {
        return 0;
    }
    // no op checks, done
    if(match->op_count == MATCH_OPCOUNT_IGNORE) {
        return 1;
    }
    // operand count requested, check
    if(match->op_count >= 0 && insn->detail->arm.op_count != match->op_count) {
        return 0;
    }
    int i;
    // operands
    for(i=0; i<MATCH_MAX_OPS && i < insn->detail->arm.op_count; i++) {
        // specific type requested?
        if(match->operands[i].type != ARM_OP_INVALID && insn->detail->arm.operands[i].type != match->operands[i].type) {
            return 0;
        }
        // specific registers requested?
        if(match->operands[i].reg1 != ARM_REG_INVALID) {
            if(insn->detail->arm.operands[i].type == ARM_OP_REG) {
                // range requested
                if(match->operands[i].reg2 != ARM_REG_INVALID) {
                    if(!reg_in_range((arm_reg)insn->detail->arm.operands[i].reg,
                                        match->operands[i].reg1, match->operands[i].reg2)) {
                        return 0;
                    }
                } else if((arm_reg)insn->detail->arm.operands[i].reg != match->operands[i].reg1) {
                    return 0;
                }
            } else if(insn->detail->arm.operands[i].type == ARM_OP_MEM) {
                if(insn->detail->arm.operands[i].mem.base != match->operands[i].reg1) {
                    return 0;
                }
            } else {
                fprintf(stderr,"insn_match: reg1 match requested on operand not reg or mem %d\n",
                        insn->detail->arm.operands[i].type);
            }
        }
        if(match->operands[i].reg2 != ARM_REG_INVALID) {
            if(insn->detail->arm.operands[i].type == ARM_OP_MEM) {
                if(insn->detail->arm.operands[i].mem.index != match->operands[i].reg2) {
                    return 0;
                }
            } else if(insn->detail->arm.operands[i].type != ARM_OP_REG) { // reg handled above
                fprintf(stderr,"insn_match: reg2 match requested on operand not reg or mem %d\n",
                        insn->detail->arm.operands[i].type);
            }
        }
        if(match->operands[i].flags & MATCH_OP_FL_IMM) {
            if(insn->detail->arm.operands[i].type == ARM_OP_IMM
                    || insn->detail->arm.operands[i].type == ARM_OP_PIMM
                    || insn->detail->arm.operands[i].type == ARM_OP_CIMM) {
                if(insn->detail->arm.operands[i].imm != match->operands[i].imm) {
                    return  0;
                }
            } else if(insn->detail->arm.operands[i].type == ARM_OP_MEM) {
                if(insn->detail->arm.operands[i].mem.disp != match->operands[i].imm) {
                    return  0;
                }
            } else {
                fprintf(stderr,"insn_match: imm match requested on operand not imm or mem %d\n",
                        insn->detail->arm.operands[i].type);
            }
        }
        if(match->operands[i].flags & MATCH_OP_FL_LAST) {
            break;
        }
    }
    return 1;
}

// check if single insn matches any of the provided matches
int insn_match_any(cs_insn *insn,const insn_match_t *match)
{
    const insn_match_t *m;
    // check matches
    for(m=match;m->id != ARM_INS_ENDING;m++) {
        if(insn_match(insn,m)) {
            return 1;
        }
    }
    return 0;
}

// iterate is until current instruction matches any of the provided matches or until limit reached
int insn_match_find_next(firmware *fw, iter_state_t *is, int max_insns, const insn_match_t *match)
{
    int i=0;
    while(i < max_insns) {
        // disassembly failed, no match (could ignore..)
        if(!disasm_iter(fw,is)) {
            return 0;
        }
        // printf("%"PRIx64" insn_match_find_next %s %s\n",is->insn->address,is->insn->mnemonic,is->insn->op_str);
        if(insn_match_any(is->insn,match)) {
            return 1;
        }
        i++;
    }
    // limit hit
    return 0;
}

// iterate is until current instruction has matched any of the provided matches N times or max_insns reached
int insn_match_find_nth(firmware *fw, iter_state_t *is, int max_insns, int num_to_match, const insn_match_t *match)
{
    int i=0;
    int num_matched=0;
    while(i < max_insns) {
        // disassembly failed, no match (could ignore..)
        if(!disasm_iter(fw,is)) {
            return 0;
        }
        // printf("%"PRIx64" insn_match_find_next %s %s\n",is->insn->address,is->insn->mnemonic,is->insn->op_str);

        const insn_match_t *m;
        // check matches
        for(m=match;m->id != ARM_INS_ENDING;m++) {
            if(insn_match(is->insn,m)) {
                num_matched++;
                break;
            }
        }
        if(num_matched == num_to_match) {
            return 1;
        }
        i++;
    }
    // limit hit
    return 0;
}

// find next matching sequence starting within max_insns
int insn_match_find_next_seq(firmware *fw, iter_state_t *is, int max_insns, const insn_match_t *match)
{
    int count=0;
    while(count < max_insns) {
        const insn_match_t *m=match;
        //printf("%"PRIx64" insn_match_find_next_seq %s %s\n",is->insn->address,is->insn->mnemonic,is->insn->op_str);
        while(m->id != ARM_INS_ENDING && disasm_iter(fw,is) && insn_match(is->insn,m)) {
            m++;
            count++;
        }
        if(m->id == ARM_INS_ENDING) {
            return 1;
        }
        // non-matching
        count++;
    }
    return 0;
}

/*
backtrack through history finding the Nth instruction matching any of the provided matches
affects fw->is, does not affect is_init
max_insns excludes current instruction, so 1 means the previous instruction
*/
int insn_match_find_hist_nth(firmware *fw, iter_state_t *is_init, int max_insns, int num_to_match, const insn_match_t *match)
{
    int i=1;
    int num_matched=0;
    // requesting > hist size is a bug, always fail
    if(max_insns >= ADR_HIST_SIZE) {
        fprintf(stderr,"%"PRIx64" insn_match_find_hist_nth max_insns %d >= ADR_HIST_MAX %d\n",is_init->insn->address,max_insns, ADR_HIST_SIZE);
        return 0;
    }
    // if you want to check the current instruction, just use normal match functions
    if(max_insns < 1) {
        fprintf(stderr,"%"PRIx64" insn_match_find_hist_nth max_insns %d < 1\n",is_init->insn->address,max_insns);
        return 0;
    }
    // requesting 0 matches probably a mistake
    if(num_to_match < 1) {
        fprintf(stderr,"%"PRIx64" insn_match_find_hist_nth max_insns %d < 1\n",is_init->insn->address,num_to_match);
        return 0;
    }
    // count includes current instruction. Calling with empty history probably a mistake
    if(is_init->ah.count <= 1) {
        fprintf(stderr,"%"PRIx64" insn_match_find_hist_nth empty history\n",is_init->insn->address);
        return 0;
    }
    // history size is unpredictable, clamp if not available
    if(is_init->ah.count - 1 < max_insns) {
        // printf("%"PRIx64" insn_match_find_hist_nth clamp max_insns %d %d\n",is_init->insn->address,max_insns,is_init->ah.count-1);
        max_insns = is_init->ah.count-1;
    }
    // no match is possible within available history
    if(num_to_match > max_insns) {
        // printf("%"PRIx64" insn_match_find_hist_nth num_to_match %d > max_insns %d\n",is_init->insn->address,num_to_match,max_insns);
        return 0;
    }

    while(i <= max_insns) {
        // thumb state comes from hist
        if(!fw_disasm_iter_single(fw,adr_hist_get(&is_init->ah,i))) {
            // break on non-code
            // fprintf(stderr,"insn_match_find_hist_nth: history disasm failed 0x%"PRIx64"\n",fw->is->insn->address);
            return 0;
        }
        // printf("%"PRIx64" insn_match_find_hist_nth %d %s %s\n",fw->is->insn->address,i,fw->is->insn->mnemonic,fw->is->insn->op_str);

        const insn_match_t *m;
        // check matches
        for(m=match;m->id != ARM_INS_ENDING;m++) {
            if(insn_match(fw->is->insn,m)) {
                num_matched++;
                // printf("%"PRIx64" insn_match_find_hist_nth match\n",fw->is->insn->address);
                break;
            }
        }
        if(num_matched == num_to_match) {
            return 1;
        }
        i++;
    }
    // limit hit
    return 0;
}


// Search the firmware for something. The desired matching is performed using the supplied 'func' function.
// Continues searching until 'func' returns non-zero - then returns 1
// otherwise returns 0.
// Uses the BufRange structs to speed up searching
// Note: this version searches byte by byte in the firmware dump instead of by words
int fw_search_bytes(firmware *fw, search_bytes_fn func)
{
    BufRange *p = fw->br;
    while (p)
    {
        int k;
        for (k = p->off*4; k < (p->off + p->len)*4; k++)
        {
            if (func(fw,k))
                return 1;
        }
        p = p->next;
    }
    return 0;
}


// ****** firmware loading / initialization / de-allocation ******
// add given address range
void fw_add_adr_range(firmware *fw, uint32_t start, uint32_t end, uint32_t src_start, int type, int flags)
{
    if(fw->adr_range_count == FW_MAX_ADR_RANGES) {
        fprintf(stderr,"fw_add_adr_range: FW_MAX_ADR_RANGES hit\n");
        return;
    }
    if(src_start < fw->base) {
        fprintf(stderr,"fw_add_adr_range: src_start 0x%08x < base 0x%08x\n",src_start,fw->base);
        return;
    }
    if(src_start >= fw->base+fw->size8) {
        fprintf(stderr,"fw_add_adr_range: src_start 0x%08x outside dump end 0x%08x\n",src_start,fw->base+fw->size8);
        return;
    }
    if(end <= start) {
        fprintf(stderr,"fw_add_adr_range: end 0x%08x <= start 0x%08x\n",end,start);
        return;
    }
    uint32_t len=end-start;
    if(len > 0xFFFFFFFF - src_start) {
        fprintf(stderr,"fw_add_adr_range: range too long %d\n",len);
        return;
    }
    if(len > fw->size8 - (start - fw->base)) {
        fprintf(stderr,"fw_add_adr_range: range outside of dump %d\n",len);
        return;
    }
    adr_range_t *r=&fw->adr_ranges[fw->adr_range_count];
    // TODO some firmware copies (i.e. g5x code 2) may end on non-word aligned address even though copy is words
    r->start=start;
    r->src_start=src_start;
    r->bytes=len;
    r->type=type;
    r->flags=flags;
    r->buf=fw->buf8 + (r->src_start - fw->base);

    fw->adr_range_count++;
}

void find_dryos_vers(firmware *fw)
{
    const char *sig="DRYOS version 2.3, release #";
    fw->dryos_ver_count = find_bytes_all(fw,sig,strlen(sig),fw->base,fw->dryos_ver_list,FW_MAX_DRYOS_VERS);
    /*
    int i;
    for(i=0;i<fw->dryos_ver_count;i++) {
        fprintf(stderr,"found %s (%d) @0x%08x\n",
            (char *)adr2ptr(fw,fw->dryos_ver_list[i]),
            atoi((char *)adr2ptr(fw,fw->dryos_ver_list[i]+strlen(sig))),
            fw->dryos_ver_list[i]);
    }
    */
    if(fw->dryos_ver_count) {
        if(fw->dryos_ver_count == FW_MAX_DRYOS_VERS) {
            fprintf(stderr,"WARNING hit FW_MAX_DRYOS_VERS\n");
        }
        uint32_t i;
        int match_i;
        uint32_t min_adr = 0xFFFFFFFF;

        // ref should easily be in the first 8M (most near start but g7x2 at >0x500000)
        uint32_t maxadr = (fw->rom_code_search_max_adr - 0x800000 > fw->base)?fw->base + 0x800000:fw->rom_code_search_max_adr;
        // look for pointer to dryos version nearest to main ROM start, before the string itself
        // NOTE it's the *pointer* that must be nearest, the string may not be the first
        for(i=0; i<fw->dryos_ver_count; i++) {
            // TODO could limit range more, ctypes should be ref'd a lot
            // could sanity check not a random value that happens to match
            uint32_t adr = find_u32_adr_range(fw,fw->dryos_ver_list[i],fw->rom_code_search_min_adr,maxadr);
            if(adr && adr < min_adr) {
                min_adr = adr;
                match_i = i;
            }
        }
        if(min_adr == 0xFFFFFFFF) {
            fprintf(stderr,"WARNING dryos version pointer not found, defaulting to first\n");
            match_i = 0;
            min_adr = 0;
        }
        fw->dryos_ver_str = (const char *)adr2ptr(fw,fw->dryos_ver_list[match_i]);
        const char *s = (const char *)adr2ptr(fw,fw->dryos_ver_list[match_i]+strlen(sig));
        fw->dryos_ver = atoi(s);
        if(s[4] == '+' && s[5] == 'p') {
            fw->dryos_ver_patch = atoi(s+6);
            if(fw->dryos_ver_patch >= FW_DRYOS_VER_MUL) {
                fprintf(stderr,"WARNING unexpected patch revision %d\n",fw->dryos_ver_patch);
            }
        } else {
            fw->dryos_ver_patch = 0;
        }
        fw->dryos_ver_full = fw->dryos_ver * FW_DRYOS_VER_MUL + fw->dryos_ver_patch;
        fw->dryos_ver_adr = fw->dryos_ver_list[match_i];
        fw->dryos_ver_ref_adr = min_adr;
        // fprintf(stderr,"main firmware version %s @ 0x%08x ptr 0x%08x\n",fw->dryos_ver_str,fw->dryos_ver_adr,min_adr);
    } else {
        fw->dryos_ver = 0;
        fw->dryos_ver_patch = 0;
        fw->dryos_ver_full = 0;
        fw->dryos_ver_str = NULL;
        fw->dryos_ver_adr = 0;
    }
}

// load firmware and initialize stuff that doesn't require disassembly
void firmware_load(firmware *fw, const char *filename, uint32_t base_adr,int fw_arch)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
    {
        fprintf(stderr,"Error opening %s\n",filename);
        exit(1);
    }
    fseek(f,0,SEEK_END);
    fw->size8 = ftell(f);
    fseek(f,0,SEEK_SET);
    // dumps should be an integral number of 32 bit words
    // ensures accessing as 32 bit ints safe
    if(fw->size8&3) {
        fprintf(stderr,"WARNING: dump size %d is not divisible by 4, truncating\n",fw->size8);
        fw->size8 &= ~3;
    }

    // adjust to ensure base_adr + size doesn't overflow
    if((int)(0xFFFFFFFF - base_adr) < fw->size8) {
        fprintf(stderr,"adjusted dump size 0x%08x->",fw->size8);
        fw->size8 = 0xFFFFFFFC - base_adr;
        fprintf(stderr,"0x%08x\n",fw->size8);
    }

    fw->arch=fw_arch;
    fw->size32=fw->size8/4;

    fw->base = base_adr;

    fw->buf8 = malloc(fw->size8);
    if(!fw->buf8) {
        fprintf(stderr,"malloc %d failed\n",fw->size8);
        exit(1);
    }
    fread(fw->buf8, 1, fw->size8, f);
    fclose(f);
    findRanges(fw);

    fw->adr_range_count=0;
    // ensure all are initialized to invalid
    memset(&fw->adr_ranges,0,sizeof(fw->adr_ranges));
    // add ROM
    fw_add_adr_range(fw,fw->base, fw->base+fw->size8, fw->base, ADR_RANGE_ROM, ADR_RANGE_FL_NONE);

    fw->main_offs = 0;
    int k = find_str(fw, "gaonisoy");
    // assume firmware start is 32 bit jump over goanisoy
    if(k == -1) {
        // suppress warning on vxworks, main firmware start is always offset 0
        if(find_str(fw,"VxWorks") == -1) {
            fprintf(stderr,"WARNING gaonisoy string not found, assuming code start offset 0\n");
        }
    } else if (k != 1) {
        // check at 0x20004 - note doesn't just use offset of first gaonisoy, because could be ref'd in romstarter
        if(fw_memcmp(fw,fw->base+0x20004,"gaonisoy",8) == 0) {
            fw->main_offs = 0x20000;
        } else if (fw_memcmp(fw,fw->base+0x10004,"gaonisoy",8) == 0) { // newer armv5 firmwares base ff81000 start at ff820000
            fw->main_offs = 0x10000;
        } else {
            fprintf(stderr,"WARNING code start offset not found, assuming 0\n");
        }
    }

    fw->rom_code_search_min_adr = fw->base + fw->main_offs; // 0 if not found
    fw->rom_code_search_max_adr=fw->base+fw->size8 - 4; // default == end of fw, may be adjusted by firmware_init_data_ranges

    find_dryos_vers(fw);

    fw->firmware_ver_str = 0;
    k = find_str(fw, "Firmware Ver ");
    if (k != -1)
    {
        fw->firmware_ver_str = (char *)fw->buf8 + k*4;
    }
    // set expected instruction set
    if(fw->arch==FW_ARCH_ARMv5) {
        fw->thumb_default = 0;
    } else if(fw->arch==FW_ARCH_ARMv7) {
        fw->thumb_default = 1;
    } else {
        fprintf(stderr,"firmware_init_capstone: invalid arch\n");
    }
}

// test to verify thumb blx bug is patched in linked capstone
int do_blx_check(firmware *fw)
{
/*
test code blxbork.S
.syntax unified
.globl arm_code
.globl _start
_start:
.code 16
blx arm_code
movs r0, #1
blx arm_code
.align 4
.code 32
arm_code:
bx lr

arm-none-eabi-gcc -nostdlib blxbork.S -o blxbork.elf
*/

static const uint8_t code[]=
    "\x00\xf0\x06\xe8" // blx arm_code (start + 0x10)
    "\x01\x20" // movs r0,#1, to cause non-word align
    "\x00\xf0\x04\xe8" // blx arm_code
;
    cs_insn *insn;
    size_t count;
    count = cs_disasm(fw->cs_handle_thumb, code, sizeof(code), 0xFF000000, 3, &insn);

    if(!(count == 3 && insn[0].id == ARM_INS_BLX && insn[2].id == ARM_INS_BLX)) {
        fprintf(stderr,"do_blx_check: disassembly failed\n");
        return 0;
    }

    int r=(insn[0].detail->arm.operands[0].imm == insn[2].detail->arm.operands[0].imm);


    if(!r) {
        fprintf(stderr,"WARNING! Incorrect disassembly is likely\n");
    }
    cs_free(insn,count);
    return r;
}

// initialize capstone state for loaded fw
int firmware_init_capstone(firmware *fw)
{
    if (cs_open(CS_ARCH_ARM, CS_MODE_ARM, &fw->cs_handle_arm) != CS_ERR_OK) {
        fprintf(stderr,"cs_open ARM failed\n");
        return 0;
    }
    cs_option(fw->cs_handle_arm, CS_OPT_DETAIL, CS_OPT_ON);
    if (cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &fw->cs_handle_thumb) != CS_ERR_OK) {
        fprintf(stderr,"cs_open thumb failed\n");
        return 0;
    }
    cs_option(fw->cs_handle_thumb, CS_OPT_DETAIL, CS_OPT_ON);
    fw->is=disasm_iter_new(fw,0);
    do_blx_check(fw);
    return 1;
}

/*
look for
ldr rx, =ROM ADR
ldr ry, =non-rom adr
ldr rz, =non ROM adr > ry
leave is pointing at last LDR, or last checked instruction
*/

int find_startup_copy(firmware *fw,
                         iter_state_t *is,
                         int max_search,
                         uint32_t *src_start,
                         uint32_t *dst_start,
                         uint32_t *dst_end)
{
    int count=0;
    uint32_t *fptr = NULL;
    uint32_t *dptr = NULL;
    uint32_t *eptr = NULL;
    *src_start=0;
    *dst_start=0;
    *dst_end=0;

    while(disasm_iter(fw,is) && count < max_search) {
        uint32_t *pv=LDR_PC2valptr(fw,is->insn);
        // not an LDR pc, reset
        // TODO some firmwares might use other instructions
        if(!pv) {
            fptr=dptr=eptr=NULL;
        }else if(!fptr) {
            // only candidate if in ROM
            if(*pv > fw->base) {
                fptr=pv;
            }
        } else if(!dptr) {
            if(*pv < fw->base) {
                dptr=pv;
            } else {
                fptr=NULL; // dest address in ROM, reset
            }
        } else if(!eptr) {
            if(*pv < fw->base && *pv > *dptr) {
                eptr=pv;
            } else { // dest end address in ROM, or before source, reset
                    // TODO maybe should swap instead if < source
                fptr=dptr=NULL;
            }
        }
        if(fptr && dptr && eptr) {
            *src_start=*fptr;
            *dst_start=*dptr;
            *dst_end=*eptr;
            return 1;
        }
        count++;
    }
    return 0;
}

void find_exception_vec(firmware *fw, iter_state_t *is)
{
    // check for exception vector, d7 id
    // only on thumb2 for now
    if(fw->arch != FW_ARCH_ARMv7) {
        return;
    }

    const insn_match_t match_bl_mcr[]={
        {MATCH_INS(BL,  1), {MATCH_OP_IMM_ANY}},
        // Vector Base Address Register MCR p15, 0, <Rt>, c12, c0, 0 - not present on PMSA
        {MATCH_INS(MCR, 6), {MATCH_OP_PIMM(15),MATCH_OP_IMM(0),MATCH_OP_REG_ANY,MATCH_OP_CIMM(12),MATCH_OP_CIMM(0),MATCH_OP_IMM(0)}},
        {ARM_INS_ENDING}
    };

    // reset to main fw start
    disasm_iter_init(fw, is, fw->base + fw->main_offs + 12 + fw->thumb_default);
    if(!insn_match_find_next(fw,is,4,match_bl_mcr)) {
        // printf("no match!\n");
        return;
    }
    // check which instruction we matched
    uint32_t faddr = get_branch_call_insn_target(fw,is);
    if(faddr) {
        // bl = digic6, has function to set up exception vector
        disasm_iter_init(fw, is, faddr);
        disasm_iter(fw, is);
        int ra,rb;
        uint32_t va, vb;
        if(!IS_INSN_ID_MOVx(is->insn->id) || is->insn->detail->arm.operands[1].type != ARM_OP_IMM) {
            return;
        }
        ra = is->insn->detail->arm.operands[0].reg;
        va = is->insn->detail->arm.operands[1].imm;
        disasm_iter(fw, is);
        if(is->insn->id != ARM_INS_MOVT
            || is->insn->detail->arm.operands[0].reg != ra
            || is->insn->detail->arm.operands[1].type != ARM_OP_IMM) {
            return;
        }
        va = (is->insn->detail->arm.operands[1].imm << 16) | (va & 0xFFFF);
        // fw has BIC
        va = va & ~1;
        if(adr_get_range_type(fw,va) != ADR_RANGE_ROM) {
            return;
        }
        disasm_iter(fw, is);
        if(!IS_INSN_ID_MOVx(is->insn->id) || is->insn->detail->arm.operands[1].type != ARM_OP_IMM) {
            return;
        }
        rb = is->insn->detail->arm.operands[0].reg;
        vb = is->insn->detail->arm.operands[1].imm;
        disasm_iter(fw, is);
        if(is->insn->id != ARM_INS_MOVT
            || is->insn->detail->arm.operands[0].reg != rb
            || is->insn->detail->arm.operands[1].type != ARM_OP_IMM) {
            return;
        }
        vb = (is->insn->detail->arm.operands[1].imm << 16) | (vb & 0xFFFF);
        vb = vb & ~1;
        if(adr_get_range_type(fw,vb) != ADR_RANGE_ROM) {
            return;
        }
        if(va >= vb) {
            return;
        }
        fw_add_adr_range(fw,0,vb - va, va, ADR_RANGE_RAM_CODE, ADR_RANGE_FL_EVEC | ADR_RANGE_FL_TCM);
        // printf("ex vec 0x%08x-0x%08x\n",va,vb);

    } else if(is->insn->id == ARM_INS_MCR) {
        // digic 7 = mcr ...
        fw->arch_flags |= FW_ARCH_FL_VMSA;
        // rewind 1
        disasm_iter_init(fw, is, adr_hist_get(&is->ah,1));
        disasm_iter(fw, is);
        // uint32_t ex_vec = LDR_PC2val(fw,is->insn);
        //printf("found MCR @ 0x%"PRIx64" ex vec at 0x%08x\n",is->insn->address,ex_vec);
    }
}

// init basic copied RAM code / data ranges
void firmware_init_data_ranges(firmware *fw)
{
//TODO maybe should return status
    uint32_t src_start, dst_start, dst_end;
    uint32_t data_found_copy = 0;

    // start at fw start  + 12 (32 bit jump, gaonisoy)
    iter_state_t *is=disasm_iter_new(fw, fw->base + fw->main_offs + 12 + fw->thumb_default);

    fw->data_init_start=0;
    fw->data_start=0;
    fw->data_len=0;

    fw->memisostart=0;

    int base2_found=0;
    int base3_found=0;

    // TODO  pre-d6 ROMs have a lot more stuff before first copy
    int max_search=100;
    while(find_startup_copy(fw,is,max_search,&src_start,&dst_start,&dst_end)) {
        // all known copied code is 3f1000 or higher, guess data
        if(dst_start < 0x100000) {
            // fprintf(stderr, "data?  @0x%"PRIx64" 0x%08x-0x%08x from 0x%08x\n",is->adr,dst_start,dst_end,src_start);
            if(fw->data_init_start) {
                fprintf(stderr,"firmware_init_data_ranges: data already found, unexpected start 0x%08x src 0x%08x end 0x%08x\n",
                        dst_start,src_start,dst_end);
                continue;
            }

            // not a known value, warn
            if(dst_start != 0x1900 && dst_start != 0x8000) {
                fprintf(stderr,"firmware_init_data_ranges: guess unknown ROM data_start 0x%08x src 0x%08x end 0x%08x\n",
                        dst_start,src_start,dst_end);
            }
            fw->data_init_start=src_start;
            fw->data_start=dst_start;
            fw->data_len=dst_end-dst_start;
            fw_add_adr_range(fw,dst_start,dst_end,src_start, ADR_RANGE_INIT_DATA, ADR_RANGE_FL_NONE);
            data_found_copy=is->adr;
        } else if(dst_start < 0x08000000) { /// highest known first copied ram code 0x01900000
            // fprintf(stderr,"code1? @0x%"PRIx64" 0x%08x-0x%08x from 0x%08x\n",is->adr,dst_start,dst_end,src_start);
            if(base2_found) {
                fprintf(stderr,"firmware_init_data_ranges: base2 already found, unexpected start 0x%08x src 0x%08x end 0x%08x\n",
                        dst_start,src_start,dst_end);
                continue;
            }
            base2_found=1;
            // known values
            if( dst_start != 0x003f1000 &&
                dst_start != 0x00431000 &&
                dst_start != 0x00471000 &&
                dst_start != 0x00685000 &&
                dst_start != 0x00671000 &&
                dst_start != 0x006b1000 &&
                dst_start != 0x010c1000 &&
                dst_start != 0x010e1000 &&
                dst_start != 0x01900000) {
                fprintf(stderr,"firmware_init_data_ranges: guess unknown base2 0x%08x src 0x%08x end 0x%08x\n",
                        dst_start,src_start,dst_end);
            }
            fw_add_adr_range(fw,dst_start,dst_end,src_start,ADR_RANGE_RAM_CODE, ADR_RANGE_FL_NONE);
        } else { // know < ROM based on match, assume second copied code
            // fprintf(stderr, "code2? @0x%"PRIx64" 0x%08x-0x%08x from 0x%08x\n",is->adr,dst_start,dst_end,src_start);
            if(base3_found) {
                fprintf(stderr,"firmware_init_data_ranges: base3 already found, unexpected start 0x%08x src 0x%08x end 0x%08x\n",
                        dst_start,src_start,dst_end);
                continue;
            }
            base3_found=1;
            if(dst_start != 0xbfe10800 && // known digic 6 value (g5x)
               dst_start != 0xdffc4900) { // known digic 7 value (m5)
                fprintf(stderr,"firmware_init_data_ranges: guess unknown base3 0x%08x src 0x%08x end 0x%08x\n",
                        dst_start,src_start,dst_end);
            }
            fw_add_adr_range(fw,dst_start,dst_end,src_start,ADR_RANGE_RAM_CODE, ADR_RANGE_FL_TCM);
        }
        if(fw->data_start && base2_found && base3_found) {
            break;
        }
        // after first, shorter search range in between copies
        max_search=16;
    }

    // look for BSS init after last found copy
    if(data_found_copy) {
        int count=0;
        uint32_t *eptr=NULL;
        uint32_t *dptr=NULL;
        disasm_iter_init(fw,is,(data_found_copy-4) | fw->thumb_default);
        while(disasm_iter(fw,is) && count < 20) {
            uint32_t *pv=LDR_PC2valptr(fw,is->insn);
            // not an LDR pc, reset;
            if(!pv) {
                //dptr=eptr=NULL;
            } else if(!dptr) {
                // TODO older firmwares use reg with ending value from DATA copy
                // should be equal to end pointer of data
                if(*pv == fw->data_start + fw->data_len) {
                    dptr=pv;
                }
            } else if(!eptr) {
                if(*pv < fw->base) {
                    if(*pv != fw->data_start + fw->data_len) {
                        eptr=pv;
                    }
                } else { // dest end address in ROM, reset
                    eptr=dptr=NULL;
                }
            }
            if(dptr && eptr) {
                // fprintf(stderr, "bss?   @0x%"PRIx64" 0x%08x-0x%08x\n",is->adr,*dptr,*eptr);
                fw->memisostart=*eptr;
                break;
            }
            count++;
        }
    }

    find_exception_vec(fw,is);

    // if data found, adjust default code search range
    // TODO could use copied code regions too, but after data on known firmwares
    if(fw->data_start) {
        fw->rom_code_search_max_adr=fw->data_init_start;
    }
    // if dryos version string found, use as search limit
    if(fw->dryos_ver_adr) {
        if(fw->dryos_ver_adr < fw->rom_code_search_max_adr) {
            fw->rom_code_search_max_adr = fw->dryos_ver_adr;
        }
    }
    disasm_iter_free(is);
}

// free resources associated with fw
void firmware_unload(firmware *fw)
{
    if(!fw) {
        return;
    }
    if(fw->is) {
        disasm_iter_free(fw->is);
    }
    if(fw->cs_handle_arm) {
        cs_close(&fw->cs_handle_arm);
    }
    if(fw->cs_handle_thumb) {
        cs_close(&fw->cs_handle_thumb);
    }
    free(fw->buf8);
    memset(fw,0,sizeof(firmware));
}
