#ifndef FIRMWARE_LOAD_NG_H
#define FIRMWARE_LOAD_NG_H

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Digic 2-5+ (ignoring S1)
#define FW_ARCH_ARMv5       1
// Digic 6, 7
#define FW_ARCH_ARMv7       2

// additional arch flags
#define FW_ARCH_FL_VMSA     1 // firmware implements Virtual Memory System Architecture, i.e. has MMU

// for clarity of thumb bit manipulations
#define ADR_SET_THUMB(x) ((x)|1)
#define ADR_CLEAR_THUMB(x) ((x)&~1)
#define ADR_IS_THUMB(x) ((x)&1)

#define ADR_ALIGN4(x) ((x)&~0x3)
#define ADR_ALIGN2(x) ((x)&~0x1)

#define ADR_IS_ALIGN4(x) (((x)&0x3)==0)

// address range (main or copied region) types
#define ADR_RANGE_INVALID   0
#define ADR_RANGE_ROM       1
#define ADR_RANGE_RAM_CODE  2 // any copied code
#define ADR_RANGE_INIT_DATA 3

// address range flags
#define ADR_RANGE_FL_NONE   0
#define ADR_RANGE_FL_TCM    1 // TCM
#define ADR_RANGE_FL_EVEC   2 // exception vector

// match bits for adr_range_find_next
#define ADR_RANGE_M_ROM         0x01 // whole ROM, including source of copied
#define ADR_RANGE_M_RAM_CODE    0x02 // only main RAM
#define ADR_RANGE_M_TCM_CODE    0x04
#define ADR_RANGE_M_EVEC        0x08
#define ADR_RANGE_M_RAM_DATA    0x10

// match bits find_next_str_bytes_code and possibly others
#define SEARCH_F_ROM_CODE   0x01
#define SEARCH_F_RAM_CODE   0x02
#define SEARCH_F_TCM_CODE   0x04

// Stores a range of valid data in the firmware dump (used to skip over empty blocks)
typedef struct bufrange {
    uint32_t *p;
    int off; // NOTE these are in 32 bit blocks, not bytes
    int len;
    struct bufrange* next;
} BufRange;


// TODO may want to make run-time adjustable
#define ADR_HIST_SIZE 64
typedef struct {
    // circular buffer of previous adr values, for backtracking
    // use addresses rather than insn, because you might want to scan with detail off and then backtrack with on
    // addresses have thumb bit set when disassembling in thumb mode
    int cur; // index of current address, after iter
    int count; // total number of valid entries
    uint32_t adrs[ADR_HIST_SIZE];
} adr_hist_t;

// state for disassembly iteration
typedef struct {
    const uint8_t *code; // pointer into buffer for code
    uint64_t adr; // firmware address - must be 64 bit for capstone iter, never has thumb bit set
                  // points to the next instruction to disassemble, insn->address gives the most
                  // recently disassembled address
    size_t size; // remaining code size
    cs_insn *insn; // cached instruction
    uint32_t thumb; // thumb state
    uint32_t insn_min_size; // 2 or 4, depending on thumb/arm state
    csh cs_handle; // capstone handle to use with this state, updated for arm/thumb transitions
    adr_hist_t ah; // history of previous instructions
}  iter_state_t;

// struct for regions of ROM that are copied elsewhere
typedef struct {
    uint8_t *buf;
    uint32_t start;     // copied / relocated firmware address
    uint32_t src_start; // source ROM firmware address
    int bytes; // size in bytes
    int type;   // ADR_RANGE_* define
    int flags;  // ADR_RANGE_FL_*
} adr_range_t;

#define FW_MAX_ADR_RANGES 10

#define FW_MAX_DRYOS_VERS 10

// scale factor for combined version + patch
#define FW_DRYOS_VER_MUL  100

// loaded firmware
typedef struct {
    union {
        uint8_t        *buf8;                // Firmware data
        uint32_t       *buf32;               // Firmware data
    };
    BufRange        *br, *last;         // Valid ranges

    int             arch;           // firmware CPU arch, set by caller at load time
    int             arch_flags;     // additional CPU arch information, inferred
    uint32_t        base;           // Base address of the firmware in the camera
    int             main_offs;      // Offset of main firmware from the start of the dump

    uint32_t        memisostart;        // Start address of the Canon heap memory (where CHDK is loaded)

    int             size8;          // Size of the firmware (as loaded from the dump) in bytes
    int             size32;         // Size of the firmware in 32 bit words

    int             dryos_ver;          // main firmware DryOS version number
    int             dryos_ver_patch;    // main firmware DryOS patch number (e.g 58p3 = 3)
    int             dryos_ver_full;     // main firmware DryOS version number * FW_DRYOS_VER_MUL + patch level
    const char      *dryos_ver_str;     // main firmware DryOS version string
    uint32_t        dryos_ver_adr;      // address of main firmware DryOS version string
    uint32_t        dryos_ver_ref_adr;  // address of pointer used to identify main fw string
    uint32_t        dryos_ver_list[FW_MAX_DRYOS_VERS]; // addresses of all found DryOS version strings
    uint32_t        dryos_ver_count;    // number of version strings found

    char            *firmware_ver_str;  // Camera firmware version string

    // TODO duplicated with adr_range stuff below
    uint32_t        data_start;         // Start address of DATA section in RAM
    uint32_t        data_init_start;    // Start address of initialisation section for DATA in ROM
    int             data_len;           // Length of data section in bytes

    // address ranges for ROM and copied data
    int             adr_range_count;
    adr_range_t     adr_ranges[FW_MAX_ADR_RANGES+1]; // +1 to for terminator with type == ADR_RANGE_INVALID

    // convenience values to optimize code searching
    uint32_t        rom_code_search_min_adr; // minimum ROM address for normal code searches (i.e. firmware start)
    uint32_t        rom_code_search_max_adr; // max ROM address for normal code searches, i.e. before copied data / code if known
    // Values loaded from stubs & other files
    stub_values     *sv;

    uint32_t thumb_default; // 1 if initial firmware code is expected to be thumb, 0 for arm.
    csh cs_handle_thumb; // capstone handle for thumb disassembly
    csh cs_handle_arm; // capstone handle for arm disassembly
    iter_state_t* is;
} firmware;

/*
convert firmware address to pointer, or NULL if not in valid range
*/
uint8_t* adr2ptr(firmware *fw, uint32_t adr);

// as above, but include initialized data area (NOTE may change on camera at runtime!)
uint8_t* adr2ptr_with_data(firmware *fw, uint32_t adr);

// return ROM address of address that may be in a remapped adr_range
uint32_t adr2romadr(firmware *fw, uint32_t adr);

// return the possibly copied address of a ROM address in the source area, or 0 if out of range
// ROM addresses are returned as-is
uint32_t romadr2adr(firmware *fw, uint32_t adr);

// return constant string describing type
const char* adr_range_type_str(int type);

// return whether the range matches the give ADR_RANGE_M bits
int adr_range_match(adr_range_t *r, uint32_t range_match);

// return next address range matching critera, starting from rng, or first if null
adr_range_t *adr_range_find_next(firmware *fw, adr_range_t *start, uint32_t range_match);

// return constant string describing type + flags
const char* adr_range_desc_str(adr_range_t *r);

// convert pointer into buf into firmware address
// current doesn't sanity check or adjust ranges
uint32_t ptr2adr(firmware *fw, uint8_t *ptr);

// return address range struct for adr, or NULL if not in known range
adr_range_t *adr_get_range(firmware *fw, uint32_t adr);

// round adr to fall within adr_range with 4 byte alignment
uint32_t adr_range_clamp_adr_align4(adr_range_t *r, uint32_t adr);

// return what kind of range adr is in
int adr_get_range_type(firmware *fw, uint32_t adr);

// return true if adr is in firmware DATA or BSS
int adr_is_var(firmware *fw, uint32_t adr);

// return true if adr is in the ROM search range, or one of the copied RAM code regions
int adr_is_main_fw_code(firmware *fw, uint32_t adr);

//
// Find the index of a string in the firmware
// Assumes the string starts on a 32bit boundary.
// String + terminating zero byte should be at least 4 bytes long
// Handles multiple string instances
int find_Nth_str(firmware *fw, char *str, int N);

// above, N=1
int find_str(firmware *fw, char *str);

// find sequence of bytes, starting from star_adr, up to max_adr, any alignment
// returns firmware address or 0
// use repeated calls to find multiple
// NOTE only handles ROM addresses
uint32_t find_next_bytes_range(firmware *fw, const void *bytes, size_t len, uint32_t start_adr, uint32_t max_adr);

// find up to maxmatch matching byte sequences, storing addresses in result
// returns count
int find_bytes_all(firmware *fw, const void *bytes, size_t len, uint32_t adr, uint32_t *result, int maxmatch);

// find index of string, starting from address, str can start at any address
// returns firmware address or 0
// use repeated calls to find multiple
// NOTE only handles ROM addresses
uint32_t find_next_str_bytes(firmware *fw, const char *str, uint32_t adr);

// as above, but ending within range of LDR pc or ADR from main fw code
uint32_t find_next_str_bytes_main_fw(firmware *fw, const char *str, uint32_t adr);

// as find_next_str_bytes, but without terminating null
uint32_t find_next_substr_bytes(firmware *fw, const char *str, uint32_t adr);

// find a string within range of LDR pc or ADR, starting from main fw
// i.e., a string directly referenced from main code
uint32_t find_str_bytes_main_fw(firmware *fw, const char *str);

// as find_next_str_bytes, first match
uint32_t find_str_bytes(firmware *fw, const char *str);

// find a string within adr_range containing start adr
uint32_t find_next_str_bytes_adr_range(firmware *fw, const char *str, uint32_t adr);

// find a string within the specified adr ranges, using ADR_RANGE_M_* defines
// if adr is 0, start with first matching range
uint32_t find_next_str_bytes_adr_ranges(firmware *fw, const char *str, uint32_t range_match, uint32_t adr);

// find a string in possibly code ranges defined by SEARCH_F_* bits in search_ranges
// if adr is 0, start with first matching range
uint32_t find_next_str_bytes_code(firmware *fw, const char *str, uint32_t search_ranges, uint32_t adr);

int isASCIIstring(firmware *fw, uint32_t adr);

/*
return firmware address of 32 bit value, starting at address "start", up to "maxadr"
*/
uint32_t find_u32_adr_range(firmware *fw, uint32_t val, uint32_t start, uint32_t maxadr);

/*
return firmware address of 32 bit value, starting at address "start", to end of dump
*/
uint32_t find_u32_adr(firmware *fw, uint32_t val, uint32_t start);

// return u32 value at adr
uint32_t fw_u32(firmware *fw, uint32_t adr);

// memcmp, but using a firmware address, returning 1 adr/size out of range
int fw_memcmp(firmware *fw, uint32_t adr,const void *cmp, size_t n);

// ****** address history functions ******
// reset address history to empty
void adr_hist_reset(adr_hist_t *ah);

// return the index of current entry + i. may be negative or positive, wraps. Does not check validity
int adr_hist_index(adr_hist_t *ah, int i);

// add an entry to address history
void adr_hist_add(adr_hist_t *ah, uint32_t adr);

// return the i'th previous entry in this history, or 0 if not valid (maybe should be -1?)
// i= 0 = most recently disassembled instruction, if any
uint32_t adr_hist_get(adr_hist_t *ah, int i);

// ****** instruction analysis utilities ******
// handle varying instruction ID enums in different capstone versions
// note these take an arm_insn enum, NOT a cs_insn *
#if CS_API_MAJOR < 4
// is insn_id a full reg MOV (does not include MOVT)
#define IS_INSN_ID_MOVx(insn_id) ((insn_id) == ARM_INS_MOV || (insn_id) == ARM_INS_MOVS || (insn_id) == ARM_INS_MOVW)
// is insn_id a a sub*
#define IS_INSN_ID_SUBx(insn_id) ((insn_id) == ARM_INS_SUB || (insn_id) == ARM_INS_SUBW || (insn_id) == ARM_INS_SUBS)
#else
#define IS_INSN_ID_MOVx(insn_id) ((insn_id) == ARM_INS_MOV || (insn_id) == ARM_INS_MOVW)
#define IS_INSN_ID_SUBx(insn_id) ((insn_id) == ARM_INS_SUB || (insn_id) == ARM_INS_SUBW)
#endif


// is insn an ARM instruction?
// like cs_insn_group(cs_handle,insn,ARM_GRP_ARM) but doesn't require handle and doesn't check or report errors
int isARM(cs_insn *insn);

/*
is insn a PC relative load?
*/
int isLDR_PC(cs_insn *insn);

/*
is insn a PC relative load to PC?
*/
int isLDR_PC_PC(cs_insn *insn);

// if insn is LDR Rn, [pc,#x] return pointer to value, otherwise null
uint32_t* LDR_PC2valptr_thumb(firmware *fw, cs_insn *insn);
uint32_t* LDR_PC2valptr_arm(firmware *fw, cs_insn *insn);
uint32_t* LDR_PC2valptr(firmware *fw, cs_insn *insn);

// return the address of value loaded by LDR rd, [pc, #x] or 0 if not LDR PC
uint32_t LDR_PC2adr(firmware *fw, cs_insn *insn);

// is insn address calculated with subw rd, pc, ...
int isSUBW_PC(cs_insn *insn);

// is insn address calculated with addw rd, pc, ...
int isADDW_PC(cs_insn *insn);

// is insn ADD rd, pc, #x  (only generated for ARM in capstone)
int isADD_PC(cs_insn *insn);

// is insn SUB rd, pc, #x  (only generated for ARM in capstone)
int isSUB_PC(cs_insn *insn);

// does insn look like a function return?
int isRETx(cs_insn *insn);

// does insn push LR (function start -ish)
int isPUSH_LR(cs_insn *insn);

// does insn pop LR (func end before tail call)
int isPOP_LR(cs_insn *insn);

// does insn pop PC
int isPOP_PC(cs_insn *insn);

// is the instruction ADD* rx, imm
int isADDx_imm(cs_insn *insn);

// is the instruction SUB* rx, imm
int isSUBx_imm(cs_insn *insn);

// is the instruction an ADR or ADR-like instruction?
int isADRx(cs_insn *insn);

// return value generated by an ADR or ADR-like instruction, or 0 (which should be rarely generated by ADR)
uint32_t ADRx2adr(firmware *fw, cs_insn *insn);

// return the value generated by an ADR (ie, the location of the value as a firmware address)
// NOTE not checked if it is in dump
uint32_t ADR2adr(firmware *fw, cs_insn *insn);

// if insn is adr/ AKA ADD Rn, pc,#x return pointer to value, otherwise null
uint32_t* ADR2valptr(firmware *fw, cs_insn *insn);

// return value loaded by PC relative LDR instruction, or 0 if out of range
uint32_t LDR_PC2val(firmware *fw, cs_insn *insn);

// return the target of B instruction, or 0 if current instruction isn't B
// both ARM and thumb instructions will NOT have the thumb bit set,
// thumbness must be determined from current state
uint32_t B_target(firmware *fw, cs_insn *insn);

// return the target of CBZ / CBNZ instruction, or 0 if current instruction isn't CBx
uint32_t CBx_target(firmware *fw, cs_insn *insn);

// return the target of BLX instruction, or 0 if current instruction isn't BLX imm
uint32_t BLXimm_target(firmware *fw, cs_insn *insn);

// return the target of BL instruction, or 0 if current instruction isn't BL
// both ARM and thumb instructions will NOT have the thumb bit set,
// thumbness must be determined from current state
uint32_t BL_target(firmware *fw, cs_insn *insn);

// as above, but also including B for tail calls
uint32_t B_BL_target(firmware *fw, cs_insn *insn);

// as above, but also including BLX imm
uint32_t B_BL_BLXimm_target(firmware *fw, cs_insn *insn);

// BX PC (mode change, small jump) Does NOT set thumb bit
uint32_t BX_PC_target(__attribute__ ((unused))firmware *fw, cs_insn *insn);

// results from get_TBx_PC_info
typedef struct {
    uint32_t start; // address of first jumptable entry
    uint32_t count; // number of entries, from preceding cmp, first_target or first invalid value
    uint32_t first_target; // lowest jumptable target address (presumably, >= end of jump table in normal code)
    int bytes; // size of jump table entries: 1 = tbb, 2=tbh
} tbx_info_t; // tbb/tbh info

// get the (likely) range of jumptable entries from a pc relative TBB or TBH instruction
// returns 0 on error or if instruction is not TBB/TBH, otherwise 1
int get_TBx_PC_info(firmware *fw,iter_state_t *is, tbx_info_t *ti);

// ****** disassembly iterator utilities ******
// allocate a new iterator state, optionally initializing at adr (0/invalid OK)
iter_state_t *disasm_iter_new(firmware *fw, uint32_t adr);

// free iterator state and associated resources
void disasm_iter_free(iter_state_t *is);

// set iterator to adr, without clearing history (for branch following)
// thumb bit in adr sets mode
int disasm_iter_set(firmware *fw, iter_state_t *is, uint32_t adr);

// initialize iterator state at adr, clearing history
// thumb bit in adr sets mode
int disasm_iter_init(firmware *fw, iter_state_t *is, uint32_t adr);

/*
disassemble next instruction, recording address in history
returns false if state invalid or disassembly fails
if disassembly fails, is->adr is not incremented
*/
int disasm_iter(firmware *fw, iter_state_t *is);

// ***** disassembly utilities operating on the default iterator state *****
// use the fw_disasm_iter functions to disassemble without setting up a new state,
// but beware some other functions might change them
/*
initialize iter state to begin iterating at adr
*/
int fw_disasm_iter_start(firmware *fw, uint32_t adr);

// disassemble the next instruction, updating cached state
int fw_disasm_iter(firmware *fw);

// disassemble single instruction at given adr, updating cached values
// history is cleared
int fw_disasm_iter_single(firmware *fw, uint32_t adr);

// ****** standalone disassembly without an iter_state ******
/*
disassemble up to count instructions starting at firmware address adr
allocates and returns insns in insn, can be freed with cs_free(insn, count)
returns actual number disassembled, less than count on error
*/
// UNUSED for now
//size_t fw_disasm_adr(firmware *fw, uint32_t adr, unsigned count, cs_insn **insn);


// ***** utilities for searching disassembly over large ranges ******
/*
callback used by fw_search_insn, called on valid instructions
_search continues iterating until callback returns non-zero.
is points to the most recently disassembled instruction
callback may advance is directly by calling disasm_iter
v1 and udata are passed through from the call to _search
*/
typedef uint32_t (*search_insn_fn)(firmware *fw, iter_state_t *is, uint32_t v1, void *udata);

/*
iterate over firmware disassembling, calling callback described above after each
successful disassembly iteration.  If disassembly fails, the iter state is advanced
2 bytes without calling the callback.
starts at address is taken from the iter_state, which should be initialized by disasm_iter_new()
disasm_iter_init(), or a previous search or iter call.
end defaults to end of ram code or rom code (before init data, if known), based on start
v1 and udata are provided to the callback
*/
uint32_t fw_search_insn(firmware *fw, iter_state_t *is, search_insn_fn f,uint32_t v1, void *udata, uint32_t adr_end);

// ****** callbacks for use with fw_search_insn ******
// search for constant references
uint32_t search_disasm_const_ref(firmware *fw, iter_state_t *is, uint32_t val, void *unused);

// search for string ref
uint32_t search_disasm_str_ref(firmware *fw, iter_state_t *is, uint32_t val, void *str);

// search for calls/jumps to immediate addresses
// thumb bit in address should be set appropriately
// returns 1 if found, address can be obtained from insn
uint32_t search_disasm_calls(firmware *fw, iter_state_t *is, uint32_t val, void *unused);

// callback for use with search_disasm_calls_multi
// adr is the matching address
typedef int (*search_calls_multi_fn)(firmware *fw, iter_state_t *is, uint32_t adr);

// structure used to define functions searched for, and functions to handle matches
// fn should be address with thumb bit set appropriately
typedef struct {
    uint32_t adr;
    search_calls_multi_fn fn;
} search_calls_multi_data_t;

// a search_calls_multi_fn that just returns 1
int search_calls_multi_end(firmware *fw, iter_state_t *is, uint32_t adr);

// Search for calls to multiple functions (more efficient than multiple passes)
// if adr is found in null terminated search_calls_multi_data array, returns fn return value
// otherwise 0
uint32_t search_disasm_calls_multi(firmware *fw, iter_state_t *is, uint32_t unused, void *userdata);

// as above, but check for single level of veneers
uint32_t search_disasm_calls_veneer_multi(firmware *fw, iter_state_t *is, uint32_t unused, void *userdata);

// ****** utilities for extracting register values ******
/*
backtrack through is_init state history picking up constants loaded into r0-r3
return bitmask of regs with values found
affects fw->is, does not affect is_init

NOTE values may be inaccurate for many reasons, doesn't track all reg affecting ops,
doesn't account for branches landing in the middle of inspected code
doesn't account for many conditional cases
*/
int get_call_const_args(firmware *fw, iter_state_t *is_init, int max_backtrack, uint32_t *res);

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
uint32_t get_direct_jump_target(firmware *fw, iter_state_t *is_init);

/*
return target of any single instruction branch or function call instruction,
with thumb bit set appropriately
returns 0 if current instruction not branch/call
*/
uint32_t get_branch_call_insn_target(firmware *fw, iter_state_t *is);

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
typedef struct {
    arm_reg reg_base;
    arm_reg reg_val;
    uint32_t adr_base; // address from original LDR
    uint32_t adr_adj; // address adjusted by adj if present, normally struct address useful for stubs comments
    uint32_t adr_final; // full address
    // offsets are guaranteed to be small
    int adj;
    int off;
} var_ldr_desc_t;
int find_and_get_var_ldr(firmware *fw,
                            iter_state_t *is,
                            int max_search_insns,
                            int max_seq_insns,
                            arm_reg match_val_reg, // ARM_REG_INVALID for any
                            var_ldr_desc_t *result);

/*
check for, and optionally return information about
functions with return values that can be completely determined
from disassembly
uses fw->is
*/
// constants below may  as flags on input, and as return value
// no simple function found
#define MATCH_SIMPLE_FUNC_NONE    0x0
// immediately returns, with no value
#define MATCH_SIMPLE_FUNC_NULLSUB 0x1
// immediately returns with a MOV constant
#define MATCH_SIMPLE_FUNC_IMM     0x2
// TODO LDR pc, =const,  ADR
// TODO could also do pointer derefs and return pointer info without val
#define MATCH_SIMPLE_FUNC_ANY     0x3
typedef struct {
    int ftype;
    uint32_t retval;
} simple_func_desc_t;
int check_simple_func(firmware *fw, uint32_t adr, int match_ftype, simple_func_desc_t *info);

/*
advance iter_state is trying to find the last function called by a function
function assumed to PUSH LR, POP LR or PC (many small functions don't!)
either the last BL/BLXimm before pop {... PC}
or B after POP {... LR}
MOV or LDR to R0-R3 are allowed between POP LR and the final B
If a POP occurs before min_insns, the match fails
Calls before min_insns are ignored
*/
uint32_t find_last_call_from_func(firmware *fw, iter_state_t *is,int min_insns, int max_insns);

// ****** utilities for matching instructions and instruction sequences ******

// use XXX_INVALID (=0) for don't care
typedef struct {
    arm_op_type type; // ARM_OP_... REG, IMM, MEM support additional tests
    arm_reg reg1; // reg for register type operands, base for mem
    uint32_t flags; //
    int32_t imm;  // immediate value for imm, disp for mem
    arm_reg reg2; // index reg form mem, or second reg for reg range
} op_match_t;
#define MATCH_OP_FL_IMM     0x0001 // use IMM value
#define MATCH_OP_FL_LAST    0x0002 // ignore all following ops, only check count
// macros for initializing above
//                                  type            reg1                flags               imm     reg2
#define MATCH_OP_ANY                {ARM_OP_INVALID,ARM_REG_INVALID,    0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_REST_ANY           {ARM_OP_INVALID,ARM_REG_INVALID,    MATCH_OP_FL_LAST,   0,      ARM_REG_INVALID}
#define MATCH_OP_REG_ANY            {ARM_OP_REG,    ARM_REG_INVALID,    0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_REG(r)             {ARM_OP_REG,    ARM_REG_##r,        0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_REG_RANGE(r1,r2)   {ARM_OP_REG,    ARM_REG_##r1,       0,                  0,      ARM_REG_##r2}
#define MATCH_OP_IMM_ANY            {ARM_OP_IMM,    ARM_REG_INVALID,    0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_IMM(imm)           {ARM_OP_IMM,    ARM_REG_INVALID,    MATCH_OP_FL_IMM,    (imm),  ARM_REG_INVALID}
#define MATCH_OP_PIMM_ANY           {ARM_OP_PIMM,   ARM_REG_INVALID,    0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_PIMM(imm)          {ARM_OP_PIMM,   ARM_REG_INVALID,    MATCH_OP_FL_IMM,    (imm),  ARM_REG_INVALID}
#define MATCH_OP_CIMM_ANY           {ARM_OP_CIMM,   ARM_REG_INVALID,    0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_CIMM(imm)          {ARM_OP_CIMM,   ARM_REG_INVALID,    MATCH_OP_FL_IMM,    (imm),  ARM_REG_INVALID}
#define MATCH_OP_MEM_ANY            {ARM_OP_MEM,    ARM_REG_INVALID,    0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_MEM(rb,ri,imm)     {ARM_OP_MEM,    ARM_REG_##rb,       MATCH_OP_FL_IMM,    (imm),  ARM_REG_##ri}
#define MATCH_OP_MEM_BASE(r)        {ARM_OP_MEM,    ARM_REG_##r,        0,                  0,      ARM_REG_INVALID}
#define MATCH_OP_MEM_REGS(rb,ri)    {ARM_OP_MEM,    ARM_REG_##rb,       0,                  0,      ARM_REG_##ri}

#define MATCH_MAX_OPS 16

#define MATCH_OPCOUNT_ANY       -1 // match any operands specified in operands, without checking count
#define MATCH_OPCOUNT_IGNORE    -2 // don't check ops at all

#define MATCH_INS(ins,opcount)          ARM_INS_##ins,(opcount),ARM_CC_INVALID
#define MATCH_INS_CC(ins,cc,opcount)    ARM_INS_##ins,(opcount),ARM_CC_##cc

// use id ARM_INS_INVALID for don't care, ARM_INS_ENDING to end list of matches
typedef struct {
    arm_insn id;
    int op_count; // negative = special values above
    arm_cc cc; // match condition code _INVALID = don't care
    op_match_t operands[MATCH_MAX_OPS];
} insn_match_t;

// some common matches for insn_match_find_next
// match a B instruction
extern const insn_match_t match_b[];

// match a BL instruction
extern const insn_match_t match_bl[];

// match B and BL
extern const insn_match_t match_b_bl[];

// match B and BL, and BLX with an immediate
extern const insn_match_t match_b_bl_blximm[];

// match only calls: BL or BLX imm
extern const insn_match_t match_bl_blximm[];

// match BX LR
extern const insn_match_t match_bxlr[];

// match BX <any reg>
extern const insn_match_t match_bxreg[];

// match BLX <any reg>
extern const insn_match_t match_blxreg[];

// match LDR rx [pc, ...]
extern const insn_match_t match_ldr_pc[];

// check if single insn matches values defined by match
int insn_match(cs_insn *insn, const insn_match_t *match);

// check if single insn matches any of the provided matches
int insn_match_any(cs_insn *insn,const insn_match_t *match);

// iterate is until current instruction matches any of the provided matches or until limit reached
int insn_match_find_next(firmware *fw, iter_state_t *is, int max_insns, const insn_match_t *match);

// iterate is until current instruction has matched any of the provided matches N times or max_insns reached
int insn_match_find_nth(firmware *fw, iter_state_t *is, int max_insns, int num_to_match, const insn_match_t *match);

// iterate as long as sequence of instructions matches sequence defined in match
int insn_match_seq(firmware *fw, iter_state_t *is, const insn_match_t *match);

// find next matching sequence starting within max_insns
int insn_match_find_next_seq(firmware *fw, iter_state_t *is, int max_insns, const insn_match_t *match);

/*
backtrack through history finding the Nth instruction matching any of the provided matches
affects fw->is, does not affect is_init
max_insns excludes current instruction, so 1 means the previous instruction
*/
int insn_match_find_hist_nth(firmware *fw, iter_state_t *is_init, int max_insns, int num_to_match, const insn_match_t *match);

#define FIND_CONST_REF_MATCH_ANY 0
#define FIND_CONST_REF_MATCH_SEQ 1
/*
find instruction or sequence that receives specified constant in specified r0-r3 reg
search starting from is to max_search_bytes
allow up to max_gap_insns between constant load and match, generally small (4-8 max)
returns address of match with thumb bit set according to mode, or 0 on failure
modifies is and fw->is
*/
int find_const_ref_match(firmware *fw,
                            iter_state_t *is,
                            int max_search_bytes,
                            int max_gap_insns,
                            arm_reg match_reg, // must be R0-R3
                            uint32_t val,
                            const insn_match_t *match,
                            int match_type);

/*
find call that receives specified constant in specified r0-r3 reg
search starting from is to max_search_bytes
allow up to max_gap_insns between constant load and call, generally small (4-8 max)
returns address of call with thumb bit set according to mode, or 0 on failure
modifies is and fw->is
*/
int find_const_ref_call(firmware *fw,
                            iter_state_t *is,
                            int max_search_bytes,
                            int max_gap_insns, // insns between ref and call
                            arm_reg match_reg, // must be R0-R3
                            uint32_t val);


// ****** utilities for non-disasm searching ******
// Search the firmware for something. The desired matching is performed using the supplied 'func' function.
// Continues searching until 'func' returns non-zero - then returns 1
// otherwise returns 0.
// Uses the BufRange structs to speed up searching
// Note: this version searches byte by byte in the firmware dump instead of by words
// borrowed from finsig_dryos
typedef int (*search_bytes_fn)(firmware*, int k);
int fw_search_bytes(firmware *fw, search_bytes_fn func);

// ****** firmware loading / initialization / de-allocation ******
// add given address range
void fw_add_adr_range(firmware *fw, uint32_t start, uint32_t end, uint32_t src_start, int type, int flags);

// load firmware and initialize stuff that doesn't require disassembly
void firmware_load(firmware *fw, const char *filename, uint32_t base_adr,int fw_arch);

// initialize capstone state for loaded fw
int firmware_init_capstone(firmware *fw);

// init basic copied RAM code / data ranges - init_capstone must be called first
void firmware_init_data_ranges(firmware *fw);

// free resources associated with fw
void firmware_unload(firmware *fw);

// get iter_state instruction address with optional thumb bit
#define iter_state_adr(is) ((uint32_t)is->insn->address | is->thumb)
#endif
