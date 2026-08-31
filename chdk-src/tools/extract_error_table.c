/*
utility to extract canon error message list
see http://chdk.wikia.com/wiki/Canon_error_codes
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "dumputil.h"

void usage()
{
    fprintf(stderr,"extract_error_table [-vxworks] <dumpfile.bin> <base address>\n");
    exit(1);
}

void dump_error_table_dry(dump_t *dump,unsigned index)
{
    unsigned i;
    for(i=index; i < dump->words - 2 && dump->pw[i+1]; i+=2) {
        printf("E%02d %02d %s\n",(short)(dump->pw[i] >> 16),dump->pw[i]&0xFFFF,deref_string_ptr(dump,(i+1)*4));
    }
}

void dump_error_table_vx(dump_t *dump,unsigned index)
{
    unsigned i;
    for(i=index; i < dump->words - 2 && dump->pw[i+1]; i+=2) {
        printf("E%02d %s\n",dump->pw[i+1],deref_string_ptr(dump,i*4));
    }
}

int process_dump_dry(dump_t *dump)
{
    const char *sig_str="NoError";
    unsigned mark_offset=0;
    unsigned mark_addr=0;
    unsigned i;
    while(find_cstring(dump,&mark_offset,sig_str)) {
        mark_addr=offset_to_ptr(dump,mark_offset);
        i=0;
        while(find_word_aligned(dump,&i,mark_addr)) {
            if( dump->pw[i-1] == 0) { 
                printf("error_table 0x%X\n",(i-1)*4+dump->base);
                dump_error_table_dry(dump,i-1);
                return 1;
            }
            i++;
        }
        mark_offset += strlen(sig_str);
    }
    if(!mark_offset) {
        fprintf(stderr,"error: \"%s\" not found\n",sig_str);
    }
    else {
        fprintf(stderr,"error: error table not found\n");
    }
    return 0;
}

int process_dump_vx(dump_t *dump)
{
    const char *sig_str="BrtDrv.TimeOutPreBvInteg";
    unsigned mark_offset=0;
    unsigned mark_addr=0;
    unsigned i;
    while(find_cstring(dump,&mark_offset,sig_str)) {
        mark_addr=offset_to_ptr(dump,mark_offset);
        i=0;
        while(find_word_aligned(dump,&i,mark_addr)) {
            if( dump->pw[i+1] == 1) { 
                printf("error_table 0x%X\n",i*4+dump->base);
                dump_error_table_vx(dump,i);
                return 1;
            }
            i++;
        }
        mark_offset += strlen(sig_str);
    }
    if(!mark_offset) {
        fprintf(stderr,"error: \"%s\" not found\n",sig_str);
    }
    else {
        fprintf(stderr,"error: error table not found\n");
    }
    return 0;
}


int main(int argc, char **argv)
{

    dump_t dump;    
    int i;
    const char *dumpname=NULL;
    const char *base=NULL;
    int opt_vxworks = 0;

    for(i = 1; i < argc; i++) {
        if ( strcmp(argv[i],"-vxworks") == 0 ) {
            opt_vxworks = 1;
        } else if ( argv[i][0] == '-' ) {
            fprintf(stderr,"%s unknown option %s\n",argv[0],argv[i]);
            usage();
        }
        else {
            if (!dumpname) {
                dumpname=argv[i];
            } else if(!base) {
                base=argv[i];
            } else {
                fprintf(stderr,"%s unexpected %s\n",argv[0],argv[i]);
                usage();
            }
        }
    }
    if(!dumpname || !base) {
        usage();
    }

    if(!load_dump(dumpname,base,&dump))
        return 1;

    int r;
    if(opt_vxworks) {
        r=process_dump_vx(&dump);
    } else {
        r=process_dump_dry(&dump);
    }
    if(!r) {
        return 1;
    }
    return 0;
}
