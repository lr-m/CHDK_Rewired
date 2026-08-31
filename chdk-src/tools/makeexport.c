#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

char* load_from_file(const char* filename);
void cut_export_token( char* sym );
char* find_last_token(char* sym );

#define MAX_SYM 2048

typedef struct
{
    unsigned int hash;
    char symbol[257];
    int count;
} hash_val;

hash_val hash_vals[MAX_SYM];
int hash_idx = 0;

void add_hash(unsigned int val, char *sym)
{
    int i;
    for (i=0; i<hash_idx; i++)
    {
        if (hash_vals[i].hash == val)
        {
            hash_vals[i].count++;
            fprintf(stderr,"Hash collision for 0x%08x (%s and %s)\n",val, sym, hash_vals[i].symbol);
            exit(-3);
        }
    }

    hash_vals[hash_idx].hash = val;
    hash_vals[hash_idx].count = 1;
    strcpy(hash_vals[hash_idx].symbol,sym);
    hash_idx++;
}

int cmp_hash(const void *a, const void *b)
{
    hash_val *ha = (hash_val*)a;
    hash_val *hb = (hash_val*)b;
    if (ha->hash < hb->hash) return -1;
    else if (ha->hash > hb->hash) return 1;
    else return 0;
}

void sort_hash()
{
    qsort(hash_vals,hash_idx,sizeof(hash_val),cmp_hash);
}

unsigned int hash(unsigned char *str)
{
    unsigned int hash = 5381;
    int c;

    // djb2 hash algorithm (Dan Bernstein - http://cr.yp.to/djb.html)
    while ((c = *str++) != 0)
        hash = ((hash << 5) + hash) ^ c; /* hash * 33 xor c */

    return hash;
}

int process_file(const char *name, FILE *out_txt)
{
    char *cursym;

    char *cur = load_from_file(name);

    if (cur == 0)
    {
        printf("makeexport: file not found '%s'\n", name);
        return 0;
    }

    // Main cycle
    for(;*cur; )
    {
        for(; *cur==9 || *cur==' '; cur++)
            ;
        if (*cur=='(') {
            for(cur++; *cur && *cur!=')'; cur++);
            for(; *cur==9 || *cur==' ' || *cur==')'; cur++);
        }

        int is_address = 0;
        if (*cur=='&')
        {
            is_address = 1;
            for(cur++; *cur==9 || *cur==' '; cur++);
        }

        cursym=cur;
        for(; (*cur>='A' && *cur<='Z') ||
              (*cur>='a' && *cur<='z') ||
              (*cur>='0' && *cur<='9') ||
              *cur=='_';
            cur++);

        if (cursym!=cur) {
            char symbol[256], full_symbol[257];
            int size=cur-cursym;

            if ( size>255) {size=255;}
            memcpy(symbol,cursym,size);
            symbol[size]=0;
            full_symbol[0] = 0;
            if (is_address) strcpy(full_symbol,"&");
            strcat(full_symbol,symbol);
            cut_export_token(symbol);

            unsigned int hash_val = hash((unsigned char*)symbol);
            add_hash(hash_val,full_symbol);
            fprintf(out_txt,"%08x %s\n",hash_val,symbol);
            for(; size>=0; size--)
            {
                if ( symbol[size]>='a' && symbol[size]<='z')
                    symbol[size]-=0x20;
            }
        }

        for(; *cur && *cur!=10; cur++);
        for(; *cur==10; cur++);
    }

    return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// @tsv - Utility to convert export list to different required format
//
// USAGE:   makeexport module_exportlist.h exportlist.inc module_hashlist.c module_exportlist.c module_exportlist.inc
//
///////////////////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv )
{
	if ( argc < 5 )
	{
		printf("#error Not enough arguments for export list maker.\n");
		exit(-1);
	}

	FILE* out_h = fopen(argv[1],"wb");
	FILE* out_txt = fopen(argv[2],"wb");
    FILE* out_hash = fopen(argv[3],"wb");

	if (!out_h)
	{
		printf("#error Error creation exportlist.h.\n");
		exit(-1);
	}
	if (!out_txt)
	{
		printf("#error Error creation exportlist.txt.\n");
		exit(-1);
	}

	fprintf(out_h,"//Auto generated file. Do not edit the contents of this file.\n");
	fprintf(out_h,"//Update the modules/module_exportlist.c file\n\n");
	fprintf(out_h,"#ifndef MODULE_EXPORTLIST_H\n");
	fprintf(out_h,"#define MODULE_EXPORTLIST_H\n\n");

	// Separate CHDK build num
	char* build = BUILD_NUMBER;
	char* e;
    int build_num=0;
	int mult=10000;
    for ( ; *build; build++) {
		if ( *build<'0' || *build>'9') continue;
        build_num += mult*strtol(build, &e, 0/*autodetect base oct-dec-hex*/);
		if ( mult==1 ) break;
		build=e;
		mult/=100;
	}
	if ( *build )
		fprintf(out_h,"#define CHDK_BUILD_NUM %d\n\n",build_num);

	if (!process_file(argv[4], out_txt)) exit(-1);  // CHDK exports
    if (!process_file(argv[5], out_txt)) exit(-1);  // GCC library exports

    int n;
    sort_hash();
    fprintf(out_hash,"// This is an automatically generated file. DO NOT EDIT!\n");
    fprintf(out_hash, "\n#include \"module_hash.h\"\n\n");
    fprintf(out_hash, "// Address references so that symbol table will compile and link.\n// Don't need correct signatures here, just the name for linking.\n");
    for (n=0; n<hash_idx; n++)
    {
        if (hash_vals[n].symbol[0] == '&')  // variable
        {
            fprintf(out_hash,"extern int %s;\n",hash_vals[n].symbol+1);
        }
        else                                // function
        {
            fprintf(out_hash,"extern void %s(void);\n",hash_vals[n].symbol);
        }
    }
    fprintf(out_hash, "\n// Symbol hash table for resolving exported symbol references\nsym_hash symbol_hash_table[] =\n{\n");
    for (n=0; n<hash_idx; n++)
    {
        fprintf(out_hash,"{ 0x%08x, %s },\n",hash_vals[n].hash,hash_vals[n].symbol);
    }
    fprintf(out_hash, "};\n");

	if (hash_idx>=1)
		fprintf(out_h,"#define EXPORTLIST_COUNT %d\n\n",hash_idx);
	else {
		fprintf(out_h,"#error Malformed export list. Only %d valid records\n\n",hash_idx);
		exit(-2);
	}
	fprintf(out_h,"#endif\n");

	fclose(out_h);
	fclose(out_txt);
    fclose(out_hash);

    return 0;
}

//-------------------------------------------------------------------

char* load_from_file(const char *filename) 
{
    int f, size;
    static struct stat st;
    char *buf = 0;

    f = open(filename, O_RDONLY, 0777);
    if (f>=0) {
        size = (stat((char*)filename, &st)==0)?st.st_size:0;
        if (size) {
            buf = (char*)malloc(size+1);
            if (buf) {
                size = read(f, buf, size);
                buf[size]=0;
            }
        }
        close(f);
    }
    return buf;
}

void cut_export_token( char* sym )
{
	const char* token="_EXPORTEDSYM_";
	int sizetoken = strlen(token);
	char* src, *fin;

	fin=sym+strlen(sym)-sizetoken;
	for(;sym<=fin;sym++) {
		if (!memcmp(sym,token,sizetoken)) {
			for (src=sym+strlen(token); *src; src++,sym++)
				*sym=*src;
			*sym=0;
			return;
		}
	}
}

char* find_last_token(char* sym )
{
	char* token=sym;

	for (;*sym;sym++)
	{
		if ( *sym==' ' && sym[1]>' ')
		 token=sym+1;
	}
	return token;
}
