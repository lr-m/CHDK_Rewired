/*
 * CHDK code_gen2 (Digic6+ version)
 *      automate generation of boot.c, capt_seq.c, movie_rec.c and filewrite.c source files for CHDK
 *      Based on the code_gen.c application and modified to use 'capdis' instead of 'chdk_dasm'
 *
 *      NOTE: requires the tools/capdis application (which requires capstone tools)
*
*   usage :- code_gen2 ROMBASE instructions_file firmware_dump_file
*
*   where:
*       ROMBASE = start address of the firmware in the camera
*       instructions_file   = filename of the instructions file used to generate the code files
*       firmware_dump_file  = filename of the camera firmware dump
*
*   If present the progam will also read stubs_entry.S and stubs_entry_2.S to get function names and addresses
*/

/*
    Notes.
    ======

    The purpose of this program is to generate the source code files used to boot CHDK and override / patch
    the default firmware behaviour. This is done by mimicing the normal camera startup code but with key
    elements replaced or changed to insert the CHDK code.

    A key component of this is to disassemble the firmware code, copy it to the CHDK source file, and then
    patch the necessary instructions for CHDK. This can be tedious and it is easy to make mistakes.

    This program aims to automate the third step of applying the patches and generating final source files.

    Note: this program is not magic - it will still require some digging into the camera firmware disassembly
    and finding functions and function lengths. It may help with the overall porting process, especially for
    a new firmware version for an existing camera port.
    
    The program uses the tools/capdis program to perform tha disassembly and then processes the output strings from capdis
    to generate the final ouput.
*/

/*
    Format of the instructions file used to generate the output code files.
    =======================================================================

    The input file is a series of instructions, one per line, that control the processing of this program.
    The instructions perform the following operations:
        - Open a new file for output
        - Close the output file
        - Copy lines from the instruction file directly to the output file unchanged
                e.g. to add header sections to .c files
        - Copy the contents of another file to the output file
                e.g. to store common code blocks in other source files
        - Begin disassembly of a new function
        - End function disassembly
        - Add the 'asm...' directive to the file to begin disassebled code
        - End the 'asm...' code block
        - Disassemble a block of code from the firmware into the output file
        - Skip over a block of firmware (e.g. DCD code)
        - Disassemble an instruction; but comment it out in the output file (including a comment for explanation)
        - Disassemble and patch an instruction that references another function
                e.g. convert BL sub_XXXXXXX to BL sub_XXXXXXXX_my
        - Disassemble and patch the immediate value in an instruction
                e.g. convert LDR R2, #0x800 to LDR R2, #0x2000
        - Generate a jump instruction that jumps back to the firmware at the current address (continue execution in the firmware)

    Each line of the input file looks like:
        operation arguments
    Arguments are of the form:
        name
        value
        name value
        name=value
    The first form is used when no value is required, the second when a single argument is needed, and the third and fourth are equivalent.

    Note there is not much error checking so lines not formatted correctly may cause invalid results.
    
    Lines with unrecognised operations or blank lines are ignores - so you can document the instruction file.
    It is recommended to use C style single line comments (//) to document the file, multi line comments are not implemented.

    Notes:
        Operation and argument names are case sensitive.
        Optional arguments below are shown in square brackets.
        Alternate versions of an argument are shown with '|' seperating them.
        Arguments with spaces (e.g. comments) must be enclosed in double quotes.
        Numeric arguments can be entered as decimal or hex, hex arguments must begin with 0x.
        If no FILE instruction is active output is written to stdout

    The available operations and arguments are:
        FILE filename                               - opens a new file for output
        ENDFILE                                     - closes the current file
        >>>                                         - copies all lines from the input file to the current output file until
                                                      a line beginning with '<<<' is found.
        >>> prelabel                                - as above, except the lines are copied before the 'loc_XXXXXXXX' label on the current instruction
                                                      if the current instruction address has a 'loc_XXXXXXXX' label (for branches, etc), the label is
                                                      normally written to the output file first.
                                                      the 'prelabel' argument causes the lines to be copied before the label.
        <<<                                         - ends the direct copy started above
        >>> file=filename                           - copies the contents of the named file to the output file (note no '<<<' is needed to end the copy)
        FUNC name=FuncName start=X length=N [jfw]   - starts a new function called 'FuncName' writing the prologue to the output file
                                                      
                                                      The 'jfw'  option will be passed to capdis to generate an instruction to jump back to the firmware at the end of the current function
                                                      Note: 'length' is strongly recommended; but can be calculated in limited cases
                                                      start = function start address in the firmware
                                                      length = number of instructions

                                                      If 'length' is not initially supplied then the code will try and calculate it
                                                      - If the FUNC instructions are all fixed length (FW n, PATCHSUB, PATCHVAL, REM & SKIP) then the
                                                        length of each is totalled to calculate 'length'

        FUNC ref=N length=N                         - starts a new function; with the name and start address from a PATCHSUB (see below), length rules as above
        FUNC sig=SigName [name=FuncName] length=N
                                                    - starts a new function. The address and name are looked up in the values
                                                      loaded from the stubs files. If supplied 'name' will override the function name from stubs.
                                                      length rules as above
        ENDFUNC                                     - ends the current function and writes the epiligue code
        ASM                                         - writes the 'asm...' stuff to begin assembly code in the .c source
        ENDASM                                      - ends the 'asm...' code block
        ENDASM prelabel                             - ends the 'asm...' code block
                                                      if there is a label for the next instruction output the label first
        FW                                          - disassemble a single instruction from the firmware to the output file at the current address
                                                      the initial start address is set by FUNC
                                                      the address is updated after each FW instruction by the number of instructions output
        FW n                                        - disassemble the next 'n' instructions to the output file. 
        FW $                                        - disassemble from the current address to the end of the function
        FW $-n                                      - disassemble from the current address to the end of the function - 'n' instructions
        REM ["comment"]                             - Disassemble a single instruction; but comment it out in the output file
                                                      the optional comment is added to the output file (to explain if necessary)
        SKIP                                        - Updates the disassembly address by one instruction (skips over the instruction)
        SKIP n                                      - Updates the disassembly address by 'n' instructions (skips over the instructions)
        PATCHVAL value=N [comment="S"]              - disassembles a single instruction, replacing any immediate value in the instruction with N
                                                      comments are written that show the original value
                                                      e.g. can be used to convert
                                                            LDR R2, #0x800
                                                      to    LDR R2, #0x2000
                                                      if supplied the 'S' comment value is also written to the output
        PATCHSUB [comment="S"]                      - disassembles the next instruction, patching a function reference.
                                                      this changes sub_XXXXXXXX to sub_XXXXXXXX_my
                                                      e.g. convert
                                                            BL sub_FF811234
                                                      to    BL sub_FF811234_my
                                                      or convert
                                                            LDR R1, =sub_FF811234
                                                      to    LDR R1, =sub_FF811234_my
                                                      if supplied the 'S' comment value is also written to the output
        PATCHSUB name=FuncName [comment="S"]        - disassembles the next instruction, patching a function reference.
                                                      this changes sub_XXXXXXXX to FuncName
                                                      e.g. convert
                                                            BL sub_FF811234
                                                      to    BL FuncName
                                                      or convert
                                                            LDR R1, =sub_FF811234
                                                      to    LDR R1, =FuncName
                                                      if supplied the 'S' comment value is also written to the output
        PATCHSUB ref=N [name=FN] [comment="s"]      - as above, this disassembles and patches a function reference instruction
                                                      in addition this saves the function address and patched name to reference array at index 'N'
                                                      this can then be used in the 'FUNC ref=N' operation to retrieve the saved name and address
                                                      to begin a new function. This saves the effort of entering the name and address on the FUNC
                                                      line and makes the instructions more portable across firmware versions.
                                                      Up to 20 patch references can be saved.
        BSUB                                        - Converts current branch/jump instruction destination from loc_XXXXXXXX to sub_XXXXXXXX
        LEADING "???"                               - Sets the string prefix added before each line output - default is 12 spaces. Can be adjusted to suit or match other code.
        DASM start=X length=N                       - starts a new disassembly in the curent FUNC at the specified start address and length
                                                      useful when functions contain data blocks within them that can confuse capdis.
        LBL                                         - output a 'loc_XXXXXXXX' label at the current address.

    Examples can be found in the G7X2 port
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "stubs_load.h"

typedef unsigned int t_address;
typedef unsigned int t_value;

//------------------------------------------------------------------------------------------------------------

// Globals used for instruction processing
FILE *outfile;
int direct_copy;
int in_func;

typedef struct {
    t_address ROM_start;
    const char* fw_name;
    char* leading;
} _options;

_options options;

int patch_ref_address[20];
char patch_ref_name[20][256];

typedef struct {
    t_address   addr;
    char        inst[256];
    int         is_inst;
} dis_line;

#define MAX_LINE 1000

dis_line dis_func[MAX_LINE];
int nxt_line = 0;
int cur_line = 0;
int func_len = 0;
int func_pos = 0;
t_address addr;

//------------------------------------------------------------------------------------------------------------

// Values loaded from stubs & other files
stub_values *sv;

//------------------------------------------------------------------------------------------------------------
// Parsing for instruction file
int lineno;
int largc;
char largs[20][256];
char *last_line;
char token[1024];

char* skip_space(char* c)
{
    while (*c && ((*c == ' ') || (*c == '\t') || (*c == '='))) c++;
    return c;
}

char* skip_token(char* c)
{
    while (*c && (*c != ' ') && (*c != '\t') && (*c != '=')) c++;
    return c;
}

char* skip_string(char* c)
{
    while (*c && (*c != '"')) c++;
    return c;
}

char* next_token(char *line)
{
    token[0] = 0;
    char *c = skip_space(line);
    char *n;
    if (*c)
    {
        if (*c == '"')
        {
            c++;
            n = skip_string(c);
            strncpy(token, c, n-c);
            token[n-c] = 0;
            return skip_space((*n)?n+1:n);
        }
        else
        {
            n = skip_token(c);
            strncpy(token, c, n-c);
            token[n-c] = 0;
            return skip_space(n);
        }
    }
    return c;
}

void parse_line(char *line)
{
    last_line = line;
    largc = 0;
    largs[largc][0] = 0;
    while (line && *line)
    {
        line = next_token(line);
        strcpy(largs[largc++], token);
    }
}

char* next_line(char *line)
{
    char *nxt = strchr(line,'\r');
    if (nxt != 0)
    {
        *nxt++ = 0;
        if (*nxt == '\n')
            nxt++;
    }
    else
    {
        nxt = strchr(line,'\n');
        if (nxt != 0)
            *nxt++ = 0;
    }
    lineno++;
    return nxt;
}

void print_args()
{
    int n;
    for (n=0; n<largc; n++) fprintf(stderr,"\t%d %s\n",n,largs[n]);
}

//------------------------------------------------------------------------------------------------------------

#define NULL_OP     0
#define FILE_OP     1
#define ENDFILE_OP  2
#define COPY_OP     3
#define COPY_LINE   4
#define FUNC_OP     5
#define ENDFUNC_OP  6
#define ASM_OP      7
#define ENDASM_OP   8
#define FW_OP       9
#define PATCHSUB_OP 10
#define PATCHVAL_OP 11
#define REM_OP      12
#define SKIP_OP     13
#define BSUB_OP     14
#define DASM_OP     15
#define LBL_OP      16

typedef struct _op
{
    struct _op  *next;

    int         operation;
    char        *source;
    int         lineno;

    char        *name;
    char        *comment;
    int         prelabel;
    t_address   func_start;
    int         func_len;
    int         patch_ref;
    int         fw_func_end_offset;
    int         fw_is_func_end_offset;
    int         op_len;
    t_address   patch_new_val;
    int         do_jump_fw;
} op;

op *op_head, *op_tail, *cur_func;

void set_op_name(op *p, char *nm)
{
    if (nm)
    {
        p->name = malloc(strlen(nm)+1);
        strcpy(p->name,nm);
    }
    else
    {
        p->name = 0;
    }
}

void set_op_comment(op *p, char *s)
{
    if (s)
    {
        p->comment = malloc(strlen(s)+1);
        strcpy(p->comment,s);
    }
    else
    {
        p->comment = 0;
    }
}

op *new_op(int type)
{
    op *p = malloc(sizeof(op));

    p->operation = type;

    p->source = malloc(strlen(last_line)+1);
    strcpy(p->source,last_line);
    p->lineno = lineno;

    p->name = 0;
    p->comment = 0;
    p->prelabel = 0;
    p->func_start = 0;
    p->func_len = 0;
    p->patch_ref = -1;
    p->fw_func_end_offset = 0;
    p->fw_is_func_end_offset = 0;
    p->op_len = -1;
    p->patch_new_val = 0;
    p->do_jump_fw = 0;

    if (op_tail)
    {
        op_tail->next = p;
    }
    else
    {
        op_head = p;
    }

    op_tail = p;
    p->next = 0;

    return p;
}

void chk_args(int count, char *msg, op *p)
{
    if (largc != count+1)
    {
        fprintf(stderr,"ERROR - %s\n",msg);
        fprintf(stderr,"LINE - %d, SOURCE - %s\n",p->lineno,p->source);
        exit(1);
    }
}

//------------------------------------------------------------------------------------------------------------

// Open an output file
void parse_FILE()
{
    op *p = new_op(FILE_OP);
    chk_args(1,"Missing FILE name",p);
    set_op_name(p,largs[1]);
}

// Close output file
void parse_ENDFILE()
{
    new_op(ENDFILE_OP);
}

// Direct copy input to output file
void parse_COPY()
{
    op *p = new_op(COPY_OP);
    int n;

    // Check if copy goes before or after the current instruction address label
    for (n=1; n<largc; n++)
    {
        if (strcmp(largs[n],"prelabel") == 0)
            p->prelabel = 1;
    }

    direct_copy = 1;

    // Check if the copy is from another source file - if so just copy that file
    for (n=1; n<largc; n++)
    {
        if (strcmp(largs[n],"file") == 0)
        {
            set_op_name(p,largs[++n]);
            direct_copy = 0;
        }
    }
}

// Start a new Function disassembly:
//  - sets up addresses & length
//  - does pass 1 & 2 of the disassembly to get the labels
//  - writes the function start to the source
void parse_FUNC()
{
    op *p = new_op(FUNC_OP);
    int n;

    for (n=1; n<largc; n++)
    {
        if (strcmp(largs[n], "name") == 0)
        {
            set_op_name(p,largs[++n]);
        }
        else if (strcmp(largs[n], "start") == 0)
        {
            p->func_start = strtoul(largs[++n],0,0);
        }
        else if (strcmp(largs[n], "length") == 0)
        {
            p->func_len = strtoul(largs[++n],0,0);
        }
        else if (strcmp(largs[n], "ref") == 0)
        {
            p->patch_ref = strtol(largs[++n],0,0);
        }
        else if (strcmp(largs[n], "sig") == 0)
        {
            osig *sig = find_sig(sv->stubs, largs[++n]);
            if (sig == 0) chk_args(-1,"Can't find firmware function for 'sig='",p);
            p->func_start = sig->val;
            p->name = sig->nm;
        }
        else if (strcmp(largs[n], "jfw") == 0)
        {
            p->do_jump_fw = 1;
        }
        else
        {
            fprintf(stderr,"????? '%s'",largs[n]);
            chk_args(-1,"Invalid FUNC argument",p);
        }
    }
}

// Continue disassembly at a new address and length
//  - sets up addresses & length
//  - does pass 1 & 2 of the disassembly to get the labels
//  - writes the function start to the source
void parse_DASM()
{
    op *p = new_op(DASM_OP);
    int n;

    for (n=1; n<largc; n++)
    {
        if (strcmp(largs[n], "start") == 0)
        {
            p->func_start = strtoul(largs[++n],0,0);
        }
        else if (strcmp(largs[n], "length") == 0)
        {
            p->func_len = strtoul(largs[++n],0,0);
        }
        else if (strcmp(largs[n], "jfw") == 0)
        {
            p->do_jump_fw = 1;
        }
        else
        {
            fprintf(stderr,"????? '%s'",largs[n]);
            chk_args(-1,"Invalid DASM argument",p);
        }
    }
}

// End function, writes the function end to the file
void parse_ENDFUNC()
{
    new_op(ENDFUNC_OP);
}

// Output a label at current address
void parse_LBL()
{
    new_op(LBL_OP);
}

// Writes the 'asm...' stuff to the file
void parse_ASM()
{
    new_op(ASM_OP);
}

// Ends the 'asm...' block
void parse_ENDASM()
{
    op *p = new_op(ENDASM_OP);
    int n;

    // Check if copy goes before or after the current instruction address label
    for (n=1; n<largc; n++)
    {
        if (strcmp(largs[n],"prelabel") == 0)
            p->prelabel = 1;
    }
}

// Disassemble a block of firmware code to the file
void parse_FW()
{
    op *p = new_op(FW_OP);
    int n;

    for (n=1; n<largc; n++)
    {
        switch (largs[n][0])
        {
        case '$':
            p->fw_func_end_offset = strtol(largs[n]+1,0,0);
            p->fw_is_func_end_offset = 1;
            break;
        default:
            if (strcmp(largs[n], "comment") == 0)
            {
                set_op_comment(p,largs[++n]);
            }
            else
            {
                p->op_len = strtoul(largs[n],0,0);
                if (p->op_len == 0) p->op_len++;
            }
            break;
        }
    }
    if (p->fw_is_func_end_offset == 0) {
        if (p->op_len <= 0) p->op_len = 1;
    }
}

// Disassemble and patch a single instruction that references another sub_XXXXXXXX
// The instruction can be B/BL/LDR etc.
void parse_PATCHSUB()
{
    op *p = new_op(PATCHSUB_OP);
    int n;

    for (n=1; n<largc; n++)
    {
        if (strcmp(largs[n], "name") == 0)
        {
            set_op_name(p,largs[++n]);
        }
        else if (strcmp(largs[n], "ref") == 0)
        {
            p->patch_ref = strtoul(largs[++n],0,0);
        }
        else if (strcmp(largs[n], "comment") == 0)
        {
            set_op_comment(p,largs[++n]);
        }
        else
        {
            chk_args(-1,"Invalid PATCHSUB argument",p);
        }
    }
}

// Disassemble and patch a single instruction, replacing an immediate value
void parse_PATCHVAL()
{
    op *p = new_op(PATCHVAL_OP);
    int n;

    for (n=1; n<largc; n++)
    {
        if (strcmp(largs[n], "value") == 0)
        {
            p->patch_new_val = strtoul(largs[++n],0,0);
        }
        else if (strcmp(largs[n], "comment") == 0)
        {
            set_op_comment(p,largs[++n]);
        }
        else
        {
            chk_args(-1,"Invalid PATCHVAL argument",p);
        }
    }
}

// Disassemble and comment out a single instruction
// An optional comment can be added to the line with the reason for removal
void parse_REM()
{
    op *p = new_op(REM_OP);

    if (largc > 1)
    {
        set_op_comment(p,largs[1]);
    }
}

// Skip N words in the firmware - used to skip over DCD in the middle of a function
void parse_SKIP()
{
    op *p = new_op(SKIP_OP);

    if (largc > 1)
        p->op_len = strtol(largs[1],0,0);
    else
        p->op_len = 1;
}

// Convert a 'b.w loc_xxxxxxxx' to 'b.w sub_xxxxxxxxx'
void parse_BSUB()
{
    new_op(BSUB_OP);
}

void parse_LEADING()
{
    if (largc > 1)
    {
        options.leading = malloc(strlen(largs[1])+1);
        strcpy(options.leading, largs[1]);
    }
}

//------------------------------------------------------------------------------------------------------------

void disassemble1(op* p)
{
    char c[256];
// popen on msys wants backslash (probably because it's invoking cmd), even though windows usually accepts forward
#ifdef WIN32 
#define DS "\\"
#else
#define DS "/"
#endif
    
    sprintf(c, ".."DS".."DS".."DS".."DS"tools"DS"capdis -d-addr -f=chdk -s=0x%08x -c=%d%s -stubs %s 0x%0x", p->func_start, p->func_len, p->do_jump_fw ? " -jfw" : "", options.fw_name, options.ROM_start);
//     printf("%s\n",c);
    
    char buf[1024];
    FILE *fp;

    if ((fp = popen(c, "r")) == NULL) {
        printf("Error opening pipe!\n");
        return;
    }

    nxt_line = 0;
    cur_line = 0;
    func_len = 0;
    func_pos = 0;
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf)-1] = 0;
        if (strncmp(buf, "// 0x", 5) == 0) {
            sscanf(buf, "// 0x%x", &addr);
        } else if (strncmp(buf, "//", 2) != 0) {
            if (strncmp(buf, "\"loc_", 5) == 0) {
                sscanf(buf, "\"loc_%x", &addr);
                dis_func[nxt_line].is_inst = 0;
            } else if ((strncmp(buf, "\"branchtable", 12) == 0) || (strncmp(buf, "\".align", 7) == 0) || (strncmp(buf, "\"    .byte", 10) == 0)) {
                dis_func[nxt_line].is_inst = 0;
            } else {
                dis_func[nxt_line].is_inst = 1;
                func_len += 1;
            }
            dis_func[nxt_line].addr = addr;
            strcpy(dis_func[nxt_line].inst, buf);
//             printf("%8x %s %d\n", dis_func[nxt_line].addr, dis_func[nxt_line].inst, dis_func[nxt_line].is_inst);
            nxt_line += 1;
            if (nxt_line >= MAX_LINE) {
                fprintf(stderr, "ERROR: function too long - start=%08x length=%d", p->func_start, p->func_len);
                exit(1);
            }
        }
    }

    if (pclose(fp)) {
        printf("Command not found or exited with error status\n");
    }
}

void disassemble_skip(int n) {
    for (; n > 0; n -= 1) {
        if (dis_func[cur_line].is_inst == 1) {
            func_pos += 1;
        }
        cur_line += 1;
    }
    addr = dis_func[cur_line].addr;
}

void disassemble_labels() {
    while ((dis_func[cur_line].is_inst == 0) && cur_line < nxt_line) {
        fprintf(outfile, "%s%s\n", options.leading, dis_func[cur_line].inst);
        disassemble_skip(1);
    }
}

void disassemble(op* p)
{
    t_value length = p->op_len;

    if (p->fw_is_func_end_offset)
        length = func_len - func_pos + p->fw_func_end_offset;

// fprintf(outfile,"//%08x %d %d %d %d\n", start, length, cur_line, nxt_line, func_len);
    t_value i;
    for (i = 0; i < length && cur_line < nxt_line;) {
        if (dis_func[cur_line].is_inst == 1) {
//             fprintf(outfile, "/*%08x*/%s\n", dis_func[cur_line].addr, dis_func[cur_line].inst);
            fprintf(outfile, "%s%s\n", options.leading, dis_func[cur_line].inst);
            i += 1;
        } else {
            fprintf(outfile, "%s%s\n", options.leading, dis_func[cur_line].inst);
        }
        disassemble_skip(1);
    }
}

void disassemble_rem(op* p)
{
    if (cur_line < nxt_line) {
        if (dis_func[cur_line].is_inst == 1) {
            fprintf(outfile, "//%s%s%s%s\n", options.leading, dis_func[cur_line].inst, p->comment ? " // " : "", p->comment ? p->comment : "");
        } else {
            fprintf(outfile, "%s%s\n", options.leading, dis_func[cur_line].inst);
        }
        disassemble_skip(1);
    }
}

void disassemble_patchsub(op* p)
{
    disassemble_labels();
    if (cur_line < nxt_line) {
        int sl = 4;
        int so = 8;
        char* s = strstr(dis_func[cur_line].inst, "sub_");
        if (s == 0) s = strstr(dis_func[cur_line].inst, "loc_");
        if (s == 0) { s = strstr(dis_func[cur_line].inst, "=0x"); if (s) { s += 1; sl = 2; } }
        if (s == 0) { s = strstr(dis_func[cur_line].inst, "_"); if (s) { sl = strlen(s)-3; so = 0; } }
        if (s) {
            t_address l;
            sscanf(s+sl, "%x", &l);
            l = l & 0xFFFFFFFE;
            char tmp[256];
            strncpy(tmp, dis_func[cur_line].inst, s - dis_func[cur_line].inst);
            tmp[s-dis_func[cur_line].inst] = 0;
            char nm[256];
            if (p->name)
                strcpy(nm, p->name);
            else
                sprintf(nm, "sub_%08x_my", l);
            char old[50];
            strncpy(old, s, sl+so);
            old[sl+so] = 0;
//             fprintf(outfile, "/*%08x*/%s%s%s // Patched\n", dis_func[cur_line].addr, tmp, nm, s+sl+8);
            fprintf(outfile, "%s%s%s%s // %s (was %s)\n", options.leading, tmp, nm, s+sl+so, p->comment ? p->comment : "Patched", old);
            if (p->patch_ref >= 0) {
                patch_ref_address[p->patch_ref] = l+1;
                strcpy(patch_ref_name[p->patch_ref], nm);
            }
        } else {
            fprintf(outfile, "%s%s\n", options.leading, dis_func[cur_line].inst);
        }
        disassemble_skip(1);
    }
}

void disassemble_patchval(op* p)
{
    disassemble_labels();
    if (cur_line < nxt_line) {
        int sl = 1;
        char* s = strstr(dis_func[cur_line].inst, "#");
        if (s) {
            t_address l;
            sscanf(s+sl, "%x", &l);
            char tmp[256];
            strncpy(tmp, dis_func[cur_line].inst, s - dis_func[cur_line].inst);
            tmp[s-dis_func[cur_line].inst] = 0;
            fprintf(outfile, "%s%s#0x%x\\n\" // %s (was 0x%x)\n", options.leading, tmp, p->patch_new_val, p->comment ? p->comment : "Patched", l);
        } else {
            fprintf(outfile, "%s%s\n", options.leading, dis_func[cur_line].inst);
        }
        disassemble_skip(1);
    }
}

void disassemble_jumpfw()
{
    disassemble_labels();
    if (cur_line < nxt_line) {
        int sl = 4;
        char* s = strstr(dis_func[cur_line].inst, "loc_");
        if (s) {
            t_address l;
            sscanf(s+sl, "%x", &l);
            l = l & 0xFFFFFFFE;
            char tmp[256];
            strncpy(tmp, dis_func[cur_line].inst, s - dis_func[cur_line].inst);
            tmp[s-dis_func[cur_line].inst] = 0;
            fprintf(outfile, "%s%ssub_%08x%s // %s\n", options.leading, tmp, l, s+sl+8, "jump to FW");
        } else {
            fprintf(outfile, "%s%s\n", options.leading, dis_func[cur_line].inst);
        }
        disassemble_skip(1);
    }
}

//------------------------------------------------------------------------------------------------------------

// Direct copy input to output file
void op_COPY(op *p)
{
    int x;

    disassemble_labels();

    // Check if the copy is from another source file - if so just copy that file
    if (p->name)
    {
        FILE *fp = fopen(p->name,"r");
        if (!fp)
        {
            fprintf(stderr,"Can't open file '%s'\n",p->name);
            exit(1);
        }

        char buf[1024];
        do
        {
            x = fread(buf,1,1024,fp);
            if (x > 0) fwrite(buf,1,x,outfile);
        } while (x > 0);

        fclose(fp);
        direct_copy = 0;
    }
}

// Start a new Function disassembly:
//  - sets up addresses & length
//  - calls capdis to generate the text disassembly
//  - writes the function start to the source
void op_FUNC(op *p)
{
    char func_name[256];

    *func_name = 0;

    cur_func = p;

    if (p->name)
        strcpy(func_name, p->name);

    if (p->patch_ref >= 0)
    {
        p->func_start = patch_ref_address[p->patch_ref];
        strcpy(func_name, patch_ref_name[p->patch_ref]);
    }

    in_func = 1;
    
    if (p->func_start == 0) chk_args(-1,"Missing FUNC start address",p);

    if (p->func_len == 0)
    {
        op *n = p->next;
        int cont = 1;
        while (n && cont)
        {
            switch (n->operation)
            {
            case FW_OP:
                if (n->fw_is_func_end_offset)
                {
                    cont = 0;
                    p->func_len = 0;
                }
                else
                {
                    p->func_len += n->op_len;
                }
                break;
            case PATCHSUB_OP:
            case PATCHVAL_OP:
            case REM_OP:
                p->func_len++;
                break;
            case SKIP_OP:
                p->func_len += n->op_len;
                break;
            case ENDFUNC_OP:
                cont = 0;
                break;
            }
            n = n->next;
        }

        if (p->func_len == 0)
        {
            chk_args(-1,"Missing FUNC end address or length",p);
        }
    }

    if (*func_name == 0)
        sprintf(func_name, "sub_%08X_my", p->func_start);

    disassemble1(p);

    fprintf(outfile,"\n/*************************************************************/");
    fprintf(outfile,"\n//** %s @ 0x%08X, length=%d %s\n", func_name, p->func_start, func_len, p->do_jump_fw ? "jfw" : "" ) ;
    fprintf(outfile,"void __attribute__((naked,noinline)) %s() {\n", func_name);

    addr = p->func_start;
}

// Continue disassembly at new address and length:
//  - sets up addresses & length
//  - calls capdis to generate the text disassembly
//  - writes the function start to the source
void op_DASM(op *p)
{
    in_func = 1;
    
    if (p->func_start == 0) chk_args(-1,"Missing DASM start address",p);

    if (p->func_len == 0)
    {
        op *n = p->next;
        int cont = 1;
        while (n && cont)
        {
            switch (n->operation)
            {
            case FW_OP:
                if (n->fw_is_func_end_offset)
                {
                    cont = 0;
                    p->func_len = 0;
                }
                else
                {
                    p->func_len += n->op_len;
                }
                break;
            case PATCHSUB_OP:
            case PATCHVAL_OP:
            case REM_OP:
                p->func_len++;
                break;
            case SKIP_OP:
                p->func_len += n->op_len;
                break;
            case ENDFUNC_OP:
                cont = 0;
                break;
            }
            n = n->next;
        }

        if (p->func_len == 0)
        {
            chk_args(-1,"Missing DASM end address or length",p);
        }
    }

    disassemble1(p);

    fprintf(outfile,"\n//** @ 0x%08X, length=%d %s\n", p->func_start, func_len, p->do_jump_fw ? "jfw" : "" ) ;

    addr = p->func_start;
}

// Disassemble a block of firmware code to the file
void op_FW(op *p)
{
    disassemble(p);
}

// Disassemble and patch a single instruction that references another sub_XXXXXXXX
void op_PATCHSUB(op *p)
{
    disassemble_patchsub(p);
}

// Disassemble and patch a single instruction, replacing an immediate value
void op_PATCHVAL(op *p)
{
    disassemble_patchval(p);
}

// Disassemble and comment out a single instruction
// An optional comment can be added to the line with the reason for removal
void op_REM(op *p)
{
    disassemble_rem(p);
}

//------------------------------------------------------------------------------------------------------------

// Process operation on current line
void do_ops(op *p)
{
    switch (p->operation)
    {
    case FILE_OP:
        // Open an output file
        outfile = fopen(p->name, "w");
        fprintf(outfile, "/*\n * %s - auto-generated by CHDK code_gen2.\n */\n", p->name);
        break;
    case ENDFILE_OP:
        // Close output file
        if (outfile != stdout)
        {
            fclose(outfile);
            outfile = stdout;
        }
        break;
    case COPY_OP:
        op_COPY(p);
        break;
    case COPY_LINE:
        fprintf(outfile, "%s\n", p->comment);
        break;
    case FUNC_OP:
        op_FUNC(p);
        break;
    case DASM_OP:
        op_DASM(p);
        break;
    case ENDFUNC_OP:
        // End function, writes the function end to the file
        fprintf(outfile,"}\n");
        in_func = 0;
        break;
    case ASM_OP:
        // Writes the 'asm...' stuff to the file
        fprintf(outfile,"asm volatile (\n");
        break;
    case ENDASM_OP:
        // Ends the 'asm...' block
        disassemble_labels();
        fprintf(outfile,");\n");
        break;
    case FW_OP:
        op_FW(p);
        break;
    case PATCHSUB_OP:
        op_PATCHSUB(p);
        break;
    case PATCHVAL_OP:
        op_PATCHVAL(p);
        break;
    case REM_OP:
        op_REM(p);
        break;
    case SKIP_OP:
        // Skip N words in the firmware - used to skip over DCD in the middle of a function
        disassemble_skip(p->op_len);
        break;
    case BSUB_OP:
        // Generate a B instruction to jump back to the firmware code at the current address
        disassemble_jumpfw();
        break;
    case LBL_OP:
        fprintf(outfile, "%s\"loc_%08x:\\n\"\n", options.leading, addr);
        break;
    }
}

// Process an operation
int run_op(char *name, void (*func)())
{
    if (strcmp(largs[0], name) == 0)
    {
        func();
        return 1;
    }
    return 0;
}

// Pre-Process operation on current line
void parse_ops()
{
    if (largc > 0)
    {
        if (run_op("FILE",      parse_FILE))       return;
        if (run_op("ENDFILE",   parse_ENDFILE))    return;
        if (run_op(">>>",       parse_COPY))       return;
        if (run_op("FUNC",      parse_FUNC))       return;
        if (run_op("ENDFUNC",   parse_ENDFUNC))    return;
        if (run_op("ASM",       parse_ASM))        return;
        if (run_op("ENDASM",    parse_ENDASM))     return;
        if (run_op("FW",        parse_FW))         return;
        if (run_op("PATCHSUB",  parse_PATCHSUB))   return;
        if (run_op("PATCHVAL",  parse_PATCHVAL))   return;
        if (run_op("REM",       parse_REM))        return;
        if (run_op("SKIP",      parse_SKIP))       return;
        if (run_op("BSUB",      parse_BSUB))       return;
        if (run_op("LEADING",   parse_LEADING))    return;
        if (run_op("DASM",      parse_DASM))       return;
        if (run_op("LBL",       parse_LBL))        return;
    }
}

//------------------------------------------------------------------------------------------------------------

void usage(char *err)
{
    fprintf(stderr,"code_gen2 <base> <primary> - Error = %s\n",err);
    exit(1);
}

op* last_op = 0;

void error(__attribute__ ((unused))char *fmt, __attribute__ ((unused))int n)
{
    if (last_op)
        fprintf(stderr,"Line - %d, Source --> %s\n",last_op->lineno,last_op->source);
    exit(1);
}

//------------------------------------------------------------------------------------------------------------

int main(int ac, const char * av[]) 
{
    // Check number of args
    if (ac < 4)
    {
        fprintf(stderr,"Usage: code_gen ROMBASE code_gen.txt PRIMARY.BIN [ALT_ROMBASE]\n");
        exit(1);
    }

    // Get ROMBASE value
    options.ROM_start = strtoul(av[1],0,0);
    if (options.ROM_start == 0)
    {
        fprintf(stderr,"Invalid ROMBASE.\n");
        exit(1);
    }

    // Get instruction file length
    struct stat st;
    if (stat(av[2],&st) != 0)
    {
        fprintf(stderr, "Failed to stat \"%s\".\n", av[2]);
        exit(1);
    }
    size_t len = st.st_size;

    // Open & read instruction file
    FILE *fp = fopen(av[2], "rb");
    if (!fp)
    {
        fprintf(stderr, "Failed to open \"%s\".\n", av[2]);
        exit(1);
    }

    // Allocate block for instruction file
    char *def = malloc(len+1);
    if (def == 0)
    {
        fprintf(stderr, "Not enough memory.\n");
        exit(1);
    }

    // Read instruction file
    size_t n = fread(def, 1, len, fp);
    def[n] = 0;
    fclose(fp);

    options.fw_name = (char*)av[3];
    options.leading = "            ";

    // Load function name/address values from stubs files
    sv = new_stub_values();
    load_funcs(sv, "funcs_by_name.csv");
    load_stubs(sv, "stubs_entry.S", 0);
    load_stubs(sv, "stubs_entry_2.S", 0);   // Load second so values override stubs_entry.S

    outfile = stdout;
    direct_copy = 0;
    in_func = 0;

    op_head = op_tail = 0;

    // Pre-Process
    lineno = 0;
    char *line = def;
    while ((line != 0) && (*line != 0))
    {
        char *nxt = next_line(line);

        // Check if just copying from input file to output file
        if (direct_copy)
        {
            // Check if end of copy
            if (strncmp(line,"<<<",3) == 0)
            {
                direct_copy = 0;
            }
            else
            {
                op *p = new_op(COPY_LINE);
                set_op_comment(p,line);
            }
        }
        else
        {
            parse_line(line);
            parse_ops();
        }

        line = nxt;
    }

    // Process
    op *p = op_head;
    while (p)
    {
        last_op = p;
        do_ops(p);
        p = p->next;
    }

    if (outfile != stdout)
        fclose(outfile);

    return 0;
}
