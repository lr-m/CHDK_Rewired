#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>

#include <capstone.h>

#include "stubs_load.h"
#include "firmware_load_ng.h"

// borrowed from chdk_dasm.h, chdk_dasm.c
//------------------------------------------------------------------------------------------------------------
typedef unsigned int t_address;
typedef unsigned int t_value;

/* -----------------------------------------------------------------
 * Linked List Routines
 * ----------------------------------------------------------------- */
 
struct lnode {                    // linked list node - Storage memory address / memory data pairs
    struct lnode *next;
    t_address address;
    t_value data ;
}; 

struct llist {                    // Head of linked list
    struct lnode *head;
    int size;
};

/* -----------------------------------------------------------------
 * struct llist * new_list(void)
 * ----------------------------------------------------------------- */
struct llist * new_list()
{ 
    struct llist *lst;

    lst = (struct llist *) malloc(sizeof(struct llist));
    if (lst == NULL) {
        printf("\n **new_list() : malloc error");
        return NULL;
    }
    lst->head = 0;
    lst->size = 0;

    return lst;
}

void free_list(struct llist *lst)
{
    if (lst)
    {
        struct lnode *p, *n;
        p = lst->head;
        while (p)
        {
            n = p->next;
            free(p);
            p = n;
        }
        free(lst);
    }
}

/* -----------------------------------------------------------------
 * struct new_node * new_node(void * item, t_value data)
 * ----------------------------------------------------------------- */
struct lnode * new_node(t_address address, t_value data) {
    struct lnode *node;

    node = (struct lnode *) malloc (sizeof (struct lnode));
    if (node == NULL) {
        printf("\n **new_node() : malloc error");
        return NULL;
    }
    node->address = address;
    node->data = data;
    node->next = 0;

    return node;
}  

/* -----------------------------------------------------------------
 * int l_search(struct llist *ls, void *address) {
 * ----------------------------------------------------------------- */
struct lnode * l_search(struct llist *ls, t_address address) {
    struct lnode *node;

    node = ls->head;
    while ( node != NULL && node->address != address ) {
        node = node->next ;
    }
    if (node == NULL) {
        return 0; /* 'item' not found */
    }

    return node;
}

/* -----------------------------------------------------------------
 * int l_insert(struct llist *ls, void *item)
 * ----------------------------------------------------------------- */
int l_insert(struct llist *ls, t_address address, t_value data)
{
    struct lnode *node;

    if( l_search(ls, address)) return -1 ;
    node = new_node(address, data);
    if (node == NULL) return 0;
    node->next = ls->head;
    ls->head = node;  
    (ls->size)++;

    return 1;
}

void l_remove(struct llist *ls, t_address addr)
{
    if (ls)
    {
        struct lnode *p, *l;
        l = 0;
        p = ls->head;
        while (p)
        {
            if (p->address == addr)
            {
                if (l)
                    l->next = p->next;
                else
                    ls->head = p->next;
                (ls->size)--;
                return;
            }
            l = p;
            p = p->next;
        }
    }
}
 
#define ADDR_HASH_BITS 20
#define ADDR_HASH_BUCKETS   (1<<ADDR_HASH_BITS)
#define ADDR_HASH_MASK   (ADDR_HASH_BUCKETS-1)
#define ADDR_HASH(addr) (((unsigned)(addr) >> 2) & ADDR_HASH_MASK)
struct llist ** addr_hash_new() {
    struct llist **addr_hash = malloc(ADDR_HASH_BUCKETS*sizeof(struct llist *));
    if(!addr_hash) {
        fprintf(stderr,"addr_hash_now: malloc failed\n");
        exit(1);
    }
    memset(addr_hash,0,ADDR_HASH_BUCKETS*sizeof(struct llist *));
    return addr_hash;
}

void addr_hash_free(struct llist **addr_hash) {
    int i;
    for(i=0;i<ADDR_HASH_BUCKETS; i++) {
        if(addr_hash[i]) {
            free_list(addr_hash[i]);
        }
    }
    free(addr_hash);
}
struct lnode * addr_hash_get(struct llist **addr_hash,t_address addr) {
    unsigned key = ADDR_HASH(addr);
    if(!addr_hash[key]) {
        return NULL;
    }
    return l_search(addr_hash[key],addr);
}

void addr_hash_add(struct llist **addr_hash,t_address addr) {
    unsigned key = ADDR_HASH(addr);
    if(!addr_hash[key]) {
        addr_hash[key] = new_list();
    }
    l_insert(addr_hash[key],addr,0);
}

// function address for prop call annotation
// TODO should have a more generalized "recognize function arugments" system
static t_address set_prop; 
static t_address get_prop; 

void usage(void) {
    fprintf(stderr,"usage capdis [options] <file> <load address>\n"
                    "options:\n"
                    " -c=<count> disassemble at most <count> instructions\n"
                    " -s=<address|stub name> start disassembling at <address>. LSB controls ARM/thumb mode\n"
                    " -e=<address> stop disassembling at <address>\n"
                    " -o=<offset> start disassembling at <offset>. LSB controls ARM/thumb mode\n"
                    " -f=<chdk|objdump> format as CHDK inline ASM, or similar to objdump (default clean ASM)\n"
                    " -armv5 make firmware_load treat firmware as armv5\n"
                    " -stubs[=dir] load / use stubs from dir (default .) for names\n"
                    " -props=<n> load propset<n>.h and identify propcases in calls. Requires -stubs\n"
                    " -v increase verbosity\n"
                    " -d-const add details about pc relative constant LDRs\n"
                    " -d-bin print instruction hex dump\n"
                    " -d-addr print instruction addresses\n"
                    " -d-ops print instruction operand information\n"
                    " -d-groups print instruction group information\n"
                    " -d all of the -d-* above\n"
                    " -noloc don't generate loc_xxx: labels and B loc_xxx\n"
                    " -nosub don't generate BL sub_xxx\n"
                    " -noconst don't generate LDR Rd,=0xFOO\n"
                    " -nostr don't comments for string refs\n"
                    " -adrldr convert ADR Rd,#x and similar to LDR RD,=... (default with -f=chdk)\n"
                    " -noadrldr don't convert ADR Rd,#x and similar to LDR RD,=...\n"
                    " -nofwdata don't attempt to initialize firmware data ranges\n"
                    " -nosimplefunc don't comment calls / jumps that return immediately or imm constant\n"
                    " -jfw generate LDR PC,=0x... to return to firmware at end of disassembled range\n"
                    " -eret[=n] stop on Nth return-like instruction (if no count / end, sets count=500)\n"
              );
    exit(1);
}

// map friendly names to arm_op_type_enum
static const char *arm_op_type_name(int op_type) {
    switch(op_type) {
        case ARM_OP_INVALID:
            return "invalid";
        case ARM_OP_REG:
            return "reg";
        case ARM_OP_IMM:
            return "imm";
        case ARM_OP_MEM:
            return "mem";
        case ARM_OP_FP:
            return "fp";
        case ARM_OP_CIMM:
            return "cimm";
        case ARM_OP_PIMM:
            return "pimm";
        case ARM_OP_SETEND:
            return "setend";
        case ARM_OP_SYSREG:
            return "sysreg";
    }
    return "unk";
}
// hack for inline C vs ASM style
const char *comment_start = ";";

static void describe_insn_ops(csh handle, cs_insn *insn) {
    printf("%s OPERANDS %d:\n",comment_start,insn->detail->arm.op_count);
    int i;
    for(i=0;i<insn->detail->arm.op_count;i++) {
        printf("%s  %d: %s",comment_start,i,arm_op_type_name(insn->detail->arm.operands[i].type));
        switch(insn->detail->arm.operands[i].type) {
            case ARM_OP_IMM:
            case ARM_OP_PIMM:
            case ARM_OP_CIMM:
                printf("=0x%x",insn->detail->arm.operands[i].imm);
                break;
            case ARM_OP_MEM: {
                const char *reg=cs_reg_name(handle,insn->detail->arm.operands[i].mem.base);
                if(reg) {
                    printf(" base=%s",reg);
                }
                reg=cs_reg_name(handle,insn->detail->arm.operands[i].mem.index);
                if(reg) {
                    printf(" index=%s",reg);
                }
                if(insn->detail->arm.operands[i].mem.disp) {
                    printf(" scale=%d disp=0x%08x",
                        insn->detail->arm.operands[i].mem.scale,
                        insn->detail->arm.operands[i].mem.disp);
                }
                break;
            }
            case ARM_OP_REG:
                printf(" %s",cs_reg_name(handle,insn->detail->arm.operands[i].reg));
                break;
            default:
                break;
        } 
        printf("\n");
    }
}
static void describe_insn_groups(csh handle, cs_insn *insn) {
    int i;
    printf("%s GROUPS %d:",comment_start,insn->detail->groups_count);
    for(i=0;i<insn->detail->groups_count;i++) {
        if(i>0) {
            printf(",");
        }
        printf("%s",cs_group_name(handle,insn->detail->groups[i]));
    }
    printf("\n");
}

#define COMMENT_LEN 255
// if adr is a string, append possibly truncated version to comment
// TODO assumes this is the last thing appended to comment
// TODO should have code to check if adr is known function
// TODO isASCIIstring currently rejects strings longer than 100
static void describe_str(firmware *fw, char *comment, uint32_t adr)
{
    int plevel=1;
    char *s=(char *)adr2ptr_with_data(fw,adr);
    if(!s) {
        //printf("s1 bad ptr 0x%08x\n",adr);
        return;
    }
    if(!isASCIIstring(fw,adr)) {
        //printf("s1 not ASCII 0x%08x\n",adr);
        // not a string, check for valid pointer
        uint32_t adr2=*(uint32_t *)s;
        // 
        s=(char *)adr2ptr_with_data(fw,adr2);
        if(!s) {
            //printf("s2 bad ptr 0x%08x\n",adr2);
            return;
        }
        if(!isASCIIstring(fw,adr2)) {
            //printf("s2 not ASCII 0x%08x\n",adr2);
            return;
        }
        plevel++;
    }
    int l=strlen(comment);
    // remaining space
    // TODO might want to limit max string to a modest number of chars
    int r=COMMENT_LEN - (l + 4 + plevel); // 4 for space, "" and possibly one inserted backslash
    if(r < 1) {
        return;
    }
    char *p=comment+l;
    *p++=' ';
    int i;
    for(i=0;i<plevel;i++) {
        *p++='*';
    }
    *p++='"';
    char *e;
    int trunc=0;
    if((int)strlen(s) < r) {
        e=p+strlen(s);
    } else {
        // not enough space for ellipsis, give up
        if(r <= 3) {
            *p++='"';
            *p++=0;
            return;
        }
        e=p+r - 3;
        trunc=1;
    }
    while(p < e) {
        if(*s == '\r') {
            *p++='\\'; 
            *p++='r';
        } else if(*s == '\n') {
            *p++='\\';
            *p++='n';
        } else {
            *p++=*s;
        }
        s++;
    }
    if(trunc) {
        strcpy(p,"...");
        p+=3;
    }
    *p++='"';
    *p++=0;
}

#define DIS_OPT_LABELS          0x00000001
#define DIS_OPT_SUBS            0x00000002
#define DIS_OPT_CONSTS          0x00000004
#define DIS_OPT_FMT_CHDK        0x00000008
#define DIS_OPT_FMT_OBJDUMP     0x00000010
#define DIS_OPT_ADR_LDR         0x00000020
#define DIS_OPT_STR             0x00000040
#define DIS_OPT_STUBS           0x00000080
#define DIS_OPT_STUBS_LABEL     0x00000100
#define DIS_OPT_PROPS           0x00000200
#define DIS_OPT_JMP_BACK        0x00000400
#define DIS_OPT_END_RET         0x00000800
#define DIS_OPT_SIMPLE_FUNCS    0x00001000

#define DIS_OPT_DETAIL_GROUP    0x00010000
#define DIS_OPT_DETAIL_OP       0x00020000
#define DIS_OPT_DETAIL_ADDR     0x00040000
#define DIS_OPT_DETAIL_BIN      0x00080000
#define DIS_OPT_DETAIL_CONST    0x00100000
#define DIS_OPT_DETAIL_ALL      (DIS_OPT_DETAIL_GROUP\
                                    |DIS_OPT_DETAIL_OP\
                                    |DIS_OPT_DETAIL_ADDR\
                                    |DIS_OPT_DETAIL_BIN\
                                    |DIS_OPT_DETAIL_CONST)

// add comments for adr if it is a ref known sub or string
void describe_const_op(firmware *fw, unsigned dis_opts, char *comment, uint32_t adr)
{
    osig* ostub=NULL;
    if(dis_opts & DIS_OPT_STUBS) {
        ostub = find_sig_val(fw->sv->stubs,adr);
        if(!ostub) {
            uint32_t *p=(uint32_t *)adr2ptr(fw,adr);
            if(p) {
                ostub = find_sig_val(fw->sv->stubs,*p);
            }
        }
        // found, and not null
        if(ostub && ostub->val) {
            // TODO overflow
            strcat(comment," ");
            strcat(comment,ostub->nm);
            // don't bother trying to comment as string
            return;
        }
    }
    if(dis_opts & DIS_OPT_STR) {
        describe_str(fw,comment,adr);
    }
}

void describe_prop_call(firmware *fw,iter_state_t *is, unsigned dis_opts, char *comment, uint32_t target)
{
    if(!(dis_opts & DIS_OPT_PROPS) || (target != get_prop && target != set_prop)) {
        return;
    }

    uint32_t regs[4];
    // backtrack up to 6 instructions, looking for r0 (propcase ID)
    if((get_call_const_args(fw,is,6,regs)&1)!=1) {
        return;
    }
    osig* ostub = find_sig_val(fw->sv->propcases,regs[0]);
    if(ostub && ostub->val) {
        sprintf(comment+strlen(comment)," %s (%d)",ostub->nm,ostub->val);
    } else {
        sprintf(comment+strlen(comment)," (%d)",regs[0]); // print number on propcase call line for easier searching
    }
}

void describe_simple_func(firmware *fw, unsigned dis_opts, char *comment, uint32_t target)
{
    if(!(dis_opts & DIS_OPT_SIMPLE_FUNCS)) {
        return;
    }
    simple_func_desc_t info;
    if(!check_simple_func(fw, target, MATCH_SIMPLE_FUNC_ANY, &info)) {
        return;
    }
    if(info.ftype == MATCH_SIMPLE_FUNC_NULLSUB) {
        strcat(comment," return");
    } else if (info.ftype == MATCH_SIMPLE_FUNC_IMM) {
        sprintf(comment+strlen(comment)," return %#x",info.retval);
    }
}

// if branch insn fill in / modify ops, comment as needed, return 1
// TODO code common with do_dis_call should be refactored
int do_dis_branch(firmware *fw, iter_state_t *is, unsigned dis_opts, char *ops, char *comment)
{
    uint32_t target = B_target(fw,is->insn);
    char op_pfx[8]="";
    if(!target) {
        target = CBx_target(fw,is->insn);
        if(!target) {
            return 0;
        }
        sprintf(op_pfx,"%s, ",cs_reg_name(is->cs_handle,is->insn->detail->arm.operands[0].reg));
    }
    osig* ostub=NULL;
    char *subname=NULL;
    uint32_t j_target=0;
    if(dis_opts & DIS_OPT_STUBS) {
       // search for current thumb state (there is no BX imm, don't currently handle ldr pc,...)
       ostub = find_sig_val(fw->sv->stubs,target|is->thumb);
       if(ostub) {
           if(ostub->is_comment) {
               strcat(comment,ostub->nm);
           } else {
               subname=ostub->nm;
           }
       } else {
           // check for veneer to known function
            if(fw_disasm_iter_single(fw,target|is->thumb)) {
                j_target=get_direct_jump_target(fw,fw->is);
                if(j_target) {
                    ostub = find_sig_val(fw->sv->stubs,j_target);
                    if(ostub) {
                        strcat(comment,"-> ");
                        strcat(comment,ostub->nm);
                    }
                }
            }
        }
    }
    if(dis_opts & DIS_OPT_LABELS) {
        if(subname) {
            if(dis_opts & DIS_OPT_FMT_CHDK) {
                sprintf(ops,"%s_%s",op_pfx,subname);
            } else {
                sprintf(ops,"%s%s",op_pfx,subname);
            }
        } else {
            sprintf(ops,"%sloc_%08x",op_pfx,target);
        }
    } else if (subname) {
       strcat(comment,subname);
    }
    uint32_t desc_adr = (j_target)?j_target:target;
    describe_prop_call(fw,is,dis_opts,comment,desc_adr | is->thumb);
    describe_simple_func(fw,dis_opts,comment,desc_adr | is->thumb);
    return 1;
}

// if function call insn fill in / modify ops, comment as needed, return 1
int do_dis_call(firmware *fw, iter_state_t *is, unsigned dis_opts, char *ops, char *comment)
{
    if(!((is->insn->id == ARM_INS_BL || is->insn->id == ARM_INS_BLX) 
            && is->insn->detail->arm.operands[0].type == ARM_OP_IMM)) {
        return 0;
    }

    uint32_t target = get_branch_call_insn_target(fw,is); // target with thumb bit set appropriately
    osig* ostub=NULL;
    char *subname=NULL;
    uint32_t j_target=0;
    if(dis_opts & DIS_OPT_STUBS) {
       ostub = find_sig_val(fw->sv->stubs,target);
       if(ostub) {
           if(ostub->is_comment) {
               strcat(comment,ostub->nm);
           } else {
               subname=ostub->nm;
           }
       } else {
           // check for veneer to known function
            if(fw_disasm_iter_single(fw,target)) {
                j_target=get_direct_jump_target(fw,fw->is);
                if(j_target) {
                    ostub = find_sig_val(fw->sv->stubs,j_target);
                    if(ostub) {
                        // TODO could translate directly into sub call, but need to resolve BL vs BLX
                        strcat(comment,"-> ");
                        strcat(comment,ostub->nm);
                    }
                }
            }
       }
    }
    if(dis_opts & DIS_OPT_SUBS) {
        if(subname) {
            if(dis_opts & DIS_OPT_FMT_CHDK) {
                sprintf(ops,"_%s",subname);
            } else {
                strcpy(ops,subname);
            }
        } else {
            sprintf(ops,"sub_%08x",ADR_CLEAR_THUMB(target));
        }
    } else if (subname) {
       strcat(comment,subname);
    }
    uint32_t desc_adr = (j_target)?j_target:target;
    describe_prop_call(fw,is,dis_opts,comment,desc_adr);
    describe_simple_func(fw,dis_opts,comment,desc_adr);
    return 1;
}

static void do_dis_insn(
                    firmware *fw,
                    iter_state_t *is,
                    unsigned dis_opts,
                    char *mnem,
                    char *ops,
                    char *comment,
                    tbx_info_t *ti) {

    cs_insn *insn=is->insn;

    // default - use capstone disasm text as-is
    strcpy(mnem,insn->mnemonic);
    strcpy(ops,insn->op_str);
    comment[0]=0;
    // handle special cases, naming etc
    if(do_dis_branch(fw,is,dis_opts,ops,comment)) {
        return;
    }
    if(do_dis_call(fw,is,dis_opts,ops,comment)) {
        return;
    }
    if((dis_opts & (DIS_OPT_CONSTS|DIS_OPT_DETAIL_CONST)) && isLDR_PC(insn))  {
        // get address for display, rather than LDR_PC2valptr directly
        uint32_t ad=LDR_PC2adr(fw,insn);
        uint32_t *pv=(uint32_t *)adr2ptr(fw,ad);
        // don't handle neg scale for now, make sure not out of bounds
        if(pv) {
            if(dis_opts & DIS_OPT_CONSTS) {
                sprintf(ops,"%s, =0x%08x",
                        cs_reg_name(is->cs_handle,insn->detail->arm.operands[0].reg),
                        *pv);
                if(dis_opts & DIS_OPT_DETAIL_CONST) {
                    // show pc + and calculated addr
                    sprintf(comment,"[pc, #%d] (0x%08x)",insn->detail->arm.operands[1].mem.disp,ad);
                }
            } else if(dis_opts & DIS_OPT_DETAIL_CONST) {
                // thumb2dis.pl style
                sprintf(comment,"0x%08x: (%08x)",ad,*pv);
            }
            describe_const_op(fw,dis_opts,comment,ad);
        } else {
            sprintf(comment,"WARNING didn't convert PC rel to constant!");
        }
    } else if((dis_opts & (DIS_OPT_CONSTS|DIS_OPT_DETAIL_CONST)) && isADRx(insn))  {
        unsigned ad=ADRx2adr(fw,insn);
        uint32_t *pv=(uint32_t *)adr2ptr(fw,ad);
        if(pv) {
            if(dis_opts & DIS_OPT_ADR_LDR) {
                strcpy(mnem,"ldr");
                sprintf(ops,"%s, =0x%08x",
                        cs_reg_name(is->cs_handle,insn->detail->arm.operands[0].reg),
                        ad);
                if(dis_opts & DIS_OPT_DETAIL_CONST) {
                    // show original op
                    if(insn->id == ARM_INS_ADR) {
                        sprintf(comment,"adr %s, #%x (0x%08x)",
                                cs_reg_name(is->cs_handle,insn->detail->arm.operands[0].reg), 
                                insn->detail->arm.operands[1].imm,
                                *pv);
                    } else {
                        sprintf(comment,"%s %s, pc, #%x (0x%08x)",
                                insn->mnemonic,
                                cs_reg_name(is->cs_handle,insn->detail->arm.operands[0].reg), 
                                insn->detail->arm.operands[2].imm,
                                *pv);
                    }
                }
            } else {
                if(insn->id == ARM_INS_ADR) {
                    if(dis_opts & DIS_OPT_FMT_OBJDUMP) {
                        strcpy(mnem,"add");
                        sprintf(ops,"%s, pc, #%d",
                                cs_reg_name(is->cs_handle,insn->detail->arm.operands[0].reg), 
                                insn->detail->arm.operands[1].imm);
                    } // else keep as adr
                }
                if(dis_opts & DIS_OPT_DETAIL_CONST) {
                    // thumb2dis.pl style
                    sprintf(comment,"0x%08x: (%08x)",ad,*pv);
                }
            }
            describe_const_op(fw,dis_opts,comment,ad);
        } else {
            sprintf(comment,"WARNING didn't convert ADR/ADD/SUB Rd, PC, #x to constant!");
        }
    } else if(get_TBx_PC_info(fw,is,ti)) {
        sprintf(comment+strlen(comment),
                    "(jumptable r%d %d elements)",
                    insn->detail->arm.operands[0].mem.index - ARM_REG_R0,
                    ti->count);
    }
    // else ... default disassembly
}

void do_adr_label(firmware *fw, struct llist **branch_list, iter_state_t *is, unsigned dis_opts)
{

    uint32_t adr=is->insn->address;
    osig* ostub = NULL;

    if(dis_opts & DIS_OPT_STUBS_LABEL) {
       // assume stub was recorded with same thumb state we are disassembling
       ostub = find_sig_val(fw->sv->stubs,adr|is->thumb);
       // TODO just comment for now
       if(ostub) {
           printf("%s %s 0x%08x\n",comment_start,ostub->nm,ostub->val);
       }
    }
    if(dis_opts & DIS_OPT_LABELS) {
        struct lnode *label = addr_hash_get(branch_list,adr);
        if(label) {
            if(dis_opts & DIS_OPT_FMT_CHDK) {
                printf("\"");
            }
            printf("loc_%08x:", label->address);
            if(dis_opts & DIS_OPT_FMT_CHDK) {
                printf("\\n\"");
            }
            printf("\n");
        }
    }
}
// TODO doesn't pay attention to various dis opts
static void do_tbb_data(firmware *fw, iter_state_t *is, unsigned dis_opts, tbx_info_t *ti)
{
    uint32_t adr=ti->start;
    if(dis_opts & DIS_OPT_FMT_CHDK) {
        printf("\"branchtable_%08x:\\n\"\n",adr);
    } else {
        printf("branchtable_%08x:\n",adr);
    }
    uint32_t i=0;
    while(i < ti->count) {
        uint8_t *p=adr2ptr(fw,adr);
        if(!p) {
            fprintf(stderr,"do_tbb_data: jumptable outside of valid address range at 0x%08x\n",adr);
            break;
        }
        uint32_t target = ti->start+2**p;
        // shouldn't happen in normal code? may indicate problem?
        if(target <= adr) {
            if(dis_opts & DIS_OPT_FMT_CHDK) {
                printf("\"    .byte 0x%02x\\n\" %s invalid juptable data?\n",*p,comment_start);
            } else {
                printf("    .byte 0x%02x ; invalid juptable data?\n",*p);
            }
            i++;
            adr++;
            continue;
        }
        if(dis_opts & DIS_OPT_FMT_CHDK) {
            printf("\"    .byte((loc_%08x - branchtable_%08x) / 2)\\n\" %s (case %d)\n",target,ti->start,comment_start,i);
        } else {
            printf("    .byte 0x%02x %s jump to loc_%08x (case %d)\n",*p,comment_start,target,i);
        }
        adr++;
        i++;
    }
    if(dis_opts & DIS_OPT_FMT_CHDK) {
// note, this is halfword aligned https://sourceware.org/binutils/docs/as/Align.html#Align
// "For other systems, including ppc, i386 using a.out format, arm and strongarm, 
// it is the number of low-order zero bits the location counter must have after advancement."
        printf("\".align 1\\n\"\n");
    }
    // ensure final adr is halfword aligned
    if(adr & 1) {
        uint8_t *p=adr2ptr(fw,adr);
        if(p) {
            if(!(dis_opts & DIS_OPT_FMT_CHDK)) {
                printf("    .byte 0x%02x\n",*p);
            }
        } else { 
            fprintf(stderr,"do_tbb_data: jumptable outside of valid address range at 0x%08x\n",adr);
        }
        adr++;
    }
    // skip over jumptable
    if(!disasm_iter_init(fw,is,adr | is->thumb)) {
        fprintf(stderr,"do_tbb_data: disasm_iter_init failed\n");
    }
}

// TODO
// mostly copy/pasted from tbb
// doesn't pay attention to various dis opts
static void do_tbh_data(firmware *fw, iter_state_t *is, unsigned dis_opts, tbx_info_t *ti)
{
    uint32_t adr=ti->start;
    if(dis_opts & DIS_OPT_FMT_CHDK) {
        printf("\"branchtable_%08x:\\n\"\n",adr);
    } else {
        printf("branchtable_%08x:\n",adr);
    }
    uint32_t i=0;
    while(i < ti->count) {
        uint16_t *p=(uint16_t *)adr2ptr(fw,adr);
        if(!p) {
            fprintf(stderr,"do_tbh_data: jumptable outside of valid address range at 0x%08x\n",adr);
            break;
        }
        uint32_t target = ti->start+2**p;
        // shouldn't happen in normal code? may indicate problem?
        if(target <= adr) {
            if(dis_opts & DIS_OPT_FMT_CHDK) {
                printf("\"    .short 0x%04x\\n\" %s invalid juptable data?\n",*p,comment_start);
            } else {
                printf("    .short 0x%04x ; invalid juptable data?\n",*p);
            }
            i++;
            adr+=ti->bytes;
            continue;
        }
        if(dis_opts & DIS_OPT_FMT_CHDK) {
            printf("\"    .short((loc_%08x - branchtable_%08x) / 2)\\n\" %s (case %d)\n",target,ti->start,comment_start,i);
        } else {
            printf("    .short 0x%04x %s jump to loc_%08x (case %d)\n",*p,comment_start,target,i);
        }
        adr+=ti->bytes;
        i++;
    }
    // tbh shouldn't need any alignment fixup
    // skip over jumptable
    if(!disasm_iter_init(fw,is,adr | is->thumb)) {
        fprintf(stderr,"do_tbh_data: disasm_iter_init failed\n");
    }
}

static void do_tbx_pass1(firmware *fw, iter_state_t *is, struct llist **branch_list, unsigned dis_opts, tbx_info_t *ti)
{
    uint32_t adr=ti->start;
    uint32_t i=0;
    while(i < ti->count) {
        uint8_t *p=adr2ptr(fw,adr);
        if(!p) {
            fprintf(stderr,"do_tbb_data: jumptable outside of valid address range at 0x%08x\n",adr);
            break;
        }
        uint16_t off;
        if(ti->bytes==1) {
            off=(uint16_t)*p;
        } else {
            off=*(uint16_t *)p;
        }
        uint32_t target = ti->start+2*off;
        // shouldn't happen in normal code? may indicate problem? don't add label
        if(target <= adr) {
            adr++;
            continue;
        }
        if(dis_opts & DIS_OPT_LABELS) {
            addr_hash_add(branch_list,target);
        }
        adr+=ti->bytes;
        i++;
    }
    // ensure final adr is halfword aligned (should only apply to tbb)
    if(adr & 1) {
        adr++;
    }
    // skip over jumptable data
    if(!disasm_iter_init(fw,is,adr | is->thumb)) {
        fprintf(stderr,"do_tbb_data: disasm_iter_init failed\n");
    }
}

// TODO should have a tbb to tbh mode for CHDK, to relax range limits
static void do_tbx_data(firmware *fw, iter_state_t *is, unsigned dis_opts, tbx_info_t *ti)
{
    if(ti->bytes==1) {
        do_tbb_data(fw,is,dis_opts,ti);
    } else {
        do_tbh_data(fw,is,dis_opts,ti);
    }
}

static void do_dis_range(firmware *fw,
                    unsigned dis_start,
                    unsigned dis_count,
                    unsigned dis_end,
                    unsigned dis_end_ret_count,
                    unsigned dis_opts)
{
    iter_state_t *is=disasm_iter_new(fw,dis_start);
    size_t count=0;
    struct llist **branch_list = addr_hash_new();
    tbx_info_t ti;

    // pre-scan for branches
    if(dis_opts & DIS_OPT_LABELS) {
        // TODO should use fw_search_insn, but doesn't easily support count
        while(count < dis_count &&  is->adr < dis_end) {
            if(disasm_iter(fw,is)) {
                uint32_t b_tgt=get_branch_call_insn_target(fw,is);
                if(b_tgt) {
                    // currently ignore thumb bit
                    addr_hash_add(branch_list,ADR_CLEAR_THUMB(b_tgt));
                } else if(get_TBx_PC_info(fw,is,&ti)) { 
                    // handle tbx, so instruction counts will match,
                    // less chance of spurious stuff from disassembling jumptable data
                    do_tbx_pass1(fw,is,branch_list,dis_opts,&ti);
                }
            } else {
                if(!disasm_iter_init(fw,is,(is->adr+is->insn_min_size) | is->thumb)) {
                    fprintf(stderr,"do_dis_range: disasm_iter_init failed\n");
                    break;
                }
            }
            count++;
        }
    }
    count=0;
    disasm_iter_init(fw,is,dis_start);
    while(count < dis_count && is->adr < dis_end) {
        if(disasm_iter(fw,is)) {
            do_adr_label(fw,branch_list,is,dis_opts);
            if(!(dis_opts & DIS_OPT_FMT_OBJDUMP) // objdump format puts these on same line as instruction
                && (dis_opts & (DIS_OPT_DETAIL_ADDR | DIS_OPT_DETAIL_BIN))) {
                printf("%s",comment_start);
                if(dis_opts & DIS_OPT_DETAIL_ADDR) {
                    printf(" 0x%"PRIx64"",is->insn->address);
                }
                if(dis_opts & DIS_OPT_DETAIL_BIN) {
                    int k;
                    for(k=0;k<is->insn->size;k++) {
                        printf(" %02x",is->insn->bytes[k]);
                    }
                }
                printf("\n");
            }
            if(dis_opts & DIS_OPT_DETAIL_OP) {
                describe_insn_ops(is->cs_handle,is->insn);
            }
            if(dis_opts & DIS_OPT_DETAIL_GROUP) {
                describe_insn_groups(is->cs_handle,is->insn);
            }
            // mnemonic and op size from capstone.h
            char insn_mnemonic[32], insn_ops[160], comment[COMMENT_LEN+1];
            ti.start=0; // flag so we can do jump table dump below
            do_dis_insn(fw,is,dis_opts,insn_mnemonic,insn_ops,comment,&ti);
            if(dis_opts & DIS_OPT_FMT_CHDK) {
                printf("\"");
            }
/*
objdump / thumb2dis format examples
fc000016: \tf8df d004 \tldr.w\tsp, [pc, #4]\t; 0xfc00001c: (1ffffffc) 
fc000020: \tbf00      \tnop

TODO most constants are decimal, while capstone defaults to hex
*/

            if(dis_opts & DIS_OPT_FMT_OBJDUMP) {// objdump format puts these on same line as instruction
                if(dis_opts & DIS_OPT_DETAIL_ADDR) {
                    printf("%08"PRIx64": \t",is->insn->address);
                }
                if(dis_opts & DIS_OPT_DETAIL_BIN) {
                    if(is->insn->size == 2) {
                        printf("%02x%02x     ",is->insn->bytes[1],is->insn->bytes[0]);
                    } else if(is->insn->size == 4) {
                        printf("%02x%02x %02x%02x",is->insn->bytes[1],is->insn->bytes[0],is->insn->bytes[3],is->insn->bytes[2]);
                    }
                }
                printf(" \t%s", insn_mnemonic);
                if(strlen(insn_ops)) {
                    printf("\t%s",insn_ops);
                }
                if(strlen(comment)) {
                    printf("\t%s %s",comment_start,comment);
                }
            } else {
                printf("    %-7s", insn_mnemonic);
                if(strlen(insn_ops)) {
                    printf(" %s",insn_ops);
                }
                if(dis_opts & DIS_OPT_FMT_CHDK) {
                    printf("\\n\"");
                }
                if(strlen(comment)) {
                    printf(" %s %s",comment_start,comment);
                }
            }
            printf("\n");
            if(ti.start) {
                do_tbx_data(fw,is,dis_opts,&ti);
            }
            if((dis_opts & DIS_OPT_END_RET) && isRETx(is->insn)) { // end disassembly on return
                if(dis_end_ret_count > 1) {
                    dis_end_ret_count--;
                } else {
                    count++;
                    // could do this, but eret in code should work better pasting between ports
                    /*
                    if(dis_opts & DIS_OPT_FMT_CHDK) {
                        printf("// -f=chdk -s=0x%08x -c=%d\n",dis_start,(unsigned)count);
                    }
                    */
                    break;
                }
            }
        } else {
            uint16_t *pv=(uint16_t *)adr2ptr(fw,is->adr);
            // TODO optional data directives
            if(pv) {
                if(is->thumb) {
                    printf("%s %04x\n",comment_start,*pv);
                } else {
                    printf("%s %04x %04x\n",comment_start,*pv,*(pv+1));
                }
            } else {
                printf("%s invalid address %"PRIx64"\n",comment_start,is->adr);
            }
            if(!disasm_iter_init(fw,is,(is->adr+is->insn_min_size)|is->thumb)) {
                fprintf(stderr,"do_dis_range: disasm_iter_init failed\n");
                break;
            }
        }
        count++;
    }
    if(dis_opts & DIS_OPT_JMP_BACK) {
        if(dis_opts & DIS_OPT_FMT_CHDK) {
            printf("\"");
        }
        // note objdump format ignored, doesn't really make sense with jump back anyway
        printf("    ldr     pc, =0x%"PRIx64,is->adr|is->thumb);
        if(dis_opts & DIS_OPT_FMT_CHDK) {
            printf("\\n\"");
        }
        printf(" %s Continue in firmware\n",comment_start);
    }
    addr_hash_free(branch_list);
}

int main(int argc, char** argv)
{
    char stubs_dir[256]="";
    char *dumpname=NULL;
    unsigned load_addr=0xFFFFFFFF;
    unsigned offset=0xFFFFFFFF;
    unsigned dis_start=0;
    const char *dis_start_fn=NULL;
    unsigned dis_end=0;
    unsigned dis_end_ret_count=0;
    unsigned dis_count=0;
    int do_fw_data_init=1;
    int verbose=0;
    unsigned dis_opts=(DIS_OPT_LABELS|DIS_OPT_SUBS|DIS_OPT_CONSTS|DIS_OPT_STR|DIS_OPT_SIMPLE_FUNCS);
    int dis_arch=FW_ARCH_ARMv7;
    int do_propset=0;
    if(argc < 2) {
        usage();
    }
    int i;
    for(i=1; i < argc; i++) {
        if ( strncmp(argv[i],"-c=",3) == 0 ) {
            dis_count=strtoul(argv[i]+3,NULL,0);
        }
        else if ( strncmp(argv[i],"-o=",3) == 0 ) {
            offset=strtoul(argv[i]+3,NULL,0);
        }
        else if ( strncmp(argv[i],"-s=",3) == 0 ) {
            if(!isdigit(argv[i][3])) {
                dis_start_fn = argv[i]+3;
            } else {
                dis_start=strtoul(argv[i]+3,NULL,0);
            }
        }
        else if ( strncmp(argv[i],"-e=",3) == 0 ) {
            dis_end=strtoul(argv[i]+3,NULL,0);
        }
        else if ( strncmp(argv[i],"-f=",3) == 0 ) {
            if ( strcmp(argv[i]+3,"chdk") == 0 ) {
                dis_opts |= DIS_OPT_FMT_CHDK;
                dis_opts |= DIS_OPT_ADR_LDR;
            } else if ( strcmp(argv[i]+3,"objdump") == 0 ) {
                dis_opts |= DIS_OPT_FMT_OBJDUMP;
            } else {
                fprintf(stderr,"unknown output format %s\n",argv[i]+3);
                usage();
            }
        }
        else if ( strcmp(argv[i],"-armv5") == 0 ) {
            dis_arch=FW_ARCH_ARMv5;
        }
        else if ( strcmp(argv[i],"-stubs") == 0 ) {
            strcpy(stubs_dir,".");
        }
        else if ( strncmp(argv[i],"-stubs=",7) == 0 ) {
            strcpy(stubs_dir,argv[i]+7);
        }
        else if ( strncmp(argv[i],"-props=",7) == 0 ) {
            do_propset=strtoul(argv[i]+7,NULL,0);
        }
        else if ( strcmp(argv[i],"-v") == 0 ) {
            verbose++;
        }
        else if ( strcmp(argv[i],"-d") == 0 ) {
            dis_opts |= DIS_OPT_DETAIL_ALL;
        }
        else if ( strcmp(argv[i],"-noloc") == 0 ) {
            dis_opts &= ~DIS_OPT_LABELS;
        }
        else if ( strcmp(argv[i],"-nosub") == 0 ) {
            dis_opts &= ~DIS_OPT_SUBS;
        }
        else if ( strcmp(argv[i],"-noconst") == 0 ) {
            dis_opts &= ~DIS_OPT_CONSTS;
        }
        else if ( strcmp(argv[i],"-nostr") == 0 ) {
            dis_opts &= ~DIS_OPT_STR;
        }
        else if ( strcmp(argv[i],"-noadrldr") == 0 ) {
            dis_opts &= ~DIS_OPT_ADR_LDR;
        }
        else if ( strcmp(argv[i],"-nofwdata") == 0 ) {
            do_fw_data_init = 0;
        }
        else if ( strcmp(argv[i],"-nosimplefunc") == 0 ) {
            dis_opts &= ~DIS_OPT_SIMPLE_FUNCS;
        }
        else if ( strcmp(argv[i],"-adrldr") == 0 ) {
            dis_opts |= DIS_OPT_ADR_LDR;
        }
        else if ( strcmp(argv[i],"-jfw") == 0 ) {
            dis_opts |= DIS_OPT_JMP_BACK;
        }
        else if ( strcmp(argv[i],"-eret") == 0 ) {
            dis_opts |= DIS_OPT_END_RET;
        }
        else if ( strncmp(argv[i],"-eret=",6) == 0 ) {
            dis_opts |= DIS_OPT_END_RET;
            dis_end_ret_count=strtoul(argv[i]+6,NULL,0);
        }
        else if ( strcmp(argv[i],"-d-const") == 0 ) {
            dis_opts |= DIS_OPT_DETAIL_CONST;
        }
        else if ( strcmp(argv[i],"-d-group") == 0 ) {
            dis_opts |= DIS_OPT_DETAIL_GROUP;
        }
        else if ( strcmp(argv[i],"-d-op") == 0 ) {
            dis_opts |= DIS_OPT_DETAIL_OP;
        }
        else if ( strcmp(argv[i],"-d-addr") == 0 ) {
            dis_opts |= DIS_OPT_DETAIL_ADDR;
        }
        else if ( strcmp(argv[i],"-d-bin") == 0 ) {
            dis_opts |= DIS_OPT_DETAIL_BIN;
        }
        else if ( argv[i][0]=='-' ) {
            fprintf(stderr,"unknown option %s\n",argv[i]);
            usage();
        } else {
            if(!dumpname) {
                dumpname=argv[i];
            } else if(load_addr == 0xFFFFFFFF) {
                load_addr=strtoul(argv[i],NULL,0);
            } else {
                fprintf(stderr,"unexpected %s\n",argv[i]);
                usage();
            }
        }
    }
    if(!dumpname) {
        fprintf(stderr,"missing input file\n");
        usage();
    }
    struct stat st;
    if(stat(dumpname,&st) != 0) {
        fprintf(stderr,"bad input file %s\n",dumpname);
        return 1;
    }
    int dumpsize = st.st_size;

    if(load_addr==0xFFFFFFFF) {
        fprintf(stderr,"missing load address\n");
        usage();
    }

    firmware fw;
    if(stubs_dir[0]) {
        dis_opts |= (DIS_OPT_STUBS|DIS_OPT_STUBS_LABEL); // TODO may want to split various places stubs names could be used
        fw.sv = new_stub_values();
        char stubs_path[300];
        sprintf(stubs_path,"%s/%s",stubs_dir,"funcs_by_name.csv");
        load_funcs(fw.sv, stubs_path);
        sprintf(stubs_path,"%s/%s",stubs_dir,"stubs_entry.S");
        load_stubs(fw.sv, stubs_path, 1);
        sprintf(stubs_path,"%s/%s",stubs_dir,"stubs_min.S");
        load_stubs(fw.sv, stubs_path, 1);
        sprintf(stubs_path,"%s/%s",stubs_dir,"stubs_entry_2.S");
        load_stubs(fw.sv, stubs_path, 1);   // Load second so values override stubs_entry.S
    }
    if(do_propset) {
        fw.sv->propcases = NULL;
        if(!(dis_opts & DIS_OPT_STUBS)) {
            fprintf(stderr,"-props requires -stubs\n");
            usage();
        }
        char props_path[300];
        sprintf(props_path,"%s/../../../../include/propset%d.h",stubs_dir,do_propset);
        // printf("load props %s %d\n",props_path,do_propset);
        load_propcases(fw.sv, props_path);
        if(!fw.sv->propcases) {
            fprintf(stderr,"propset %d load failed\n",do_propset);
            exit(1);
        }
        osig *ostub=find_sig(fw.sv->stubs,"SetPropertyCase");
        if(!ostub || !ostub->val) {
            fprintf(stderr,"SetPropertyCase not found, ignoring -props\n");
            set_prop = 0;
        } else {
            set_prop = ostub->val;
        }
        ostub=find_sig(fw.sv->stubs,"GetPropertyCase");
        if(!ostub || !ostub->val) {
            fprintf(stderr,"GetPropertyCase not found, ignoring -props\n");
            get_prop = 0;
        } else {
            get_prop = ostub->val;
        }
        if(get_prop && set_prop) {
            dis_opts |= DIS_OPT_PROPS;
        } else {
            do_propset=0;
        }
    }
    if(dis_start_fn) {
        if(!stubs_dir[0]) {
            fprintf(stderr,"-s with function name requires -stubs (did you forget an 0x?)\n");
            usage();
        }
        osig *ostub=find_sig(fw.sv->stubs,dis_start_fn);
        if(!ostub || !ostub->val) {
            fprintf(stderr,"start function %s not found (did you forget an 0x?)\n",dis_start_fn);
            return 1;
        }
        dis_start=ostub->val;
    }

    if(offset != 0xFFFFFFFF) {
        if(dis_start) {
            fprintf(stderr,"both start and and offset given\n");
            usage();
        }
        dis_start = offset+load_addr;
    }
    if(dis_end) {
        if(dis_count) {
            fprintf(stderr,"both end and count given\n");
            usage();
        }
        if(dis_end > load_addr+dumpsize) {
            fprintf(stderr,"end > load address + size\n");
            usage();
        }
        if(dis_end < dis_start) {
            fprintf(stderr,"end < start\n");
            usage();
        }
        dis_count=(dis_end-dis_start)/2; // need a count for do_dis_range, assume all 16 bit ins
    }
    // allow eret without count, enough for most cases
    if((dis_count==0) && (dis_opts & DIS_OPT_END_RET)) {
        dis_count=500;
    }
    if(dis_count==0) {
        fprintf(stderr,"missing instruction count\n");
        usage();
    }
    // count, no end
    if(dis_end==0) {
        dis_end=dis_start + dis_count * 4; // assume all 32 bit ins
    }
    if((dis_opts & (DIS_OPT_FMT_CHDK | DIS_OPT_FMT_OBJDUMP)) == (DIS_OPT_FMT_CHDK | DIS_OPT_FMT_OBJDUMP)) {
        fprintf(stderr,"multiple -f options not allowed\n");
        usage();
    }
    if(dis_opts & (DIS_OPT_FMT_CHDK)) {
        comment_start = "//";
    }
    
    firmware_load(&fw,dumpname,load_addr,dis_arch); 
    firmware_init_capstone(&fw);
    if(do_fw_data_init) {
        firmware_init_data_ranges(&fw);
    }

    // check for RAM code address
    if(dis_start < fw.base) {
        adr_range_t *ar=adr_get_range(&fw,dis_start);
        if(!ar || ar->type != ADR_RANGE_RAM_CODE) {
            fprintf(stderr,"invalid start address 0x%08x\n",dis_start);
            return 1;
        }
    }

    if(verbose) {
        printf("%s %s size:0x%x start:0x%x instructions:%d opts:0x%x\n",comment_start,dumpname,dumpsize,dis_start,dis_count,dis_opts);
    }


    do_dis_range(&fw, dis_start, dis_count, dis_end, dis_end_ret_count, dis_opts);

    firmware_unload(&fw);


    return 0;
}
