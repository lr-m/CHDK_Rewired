#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../core/gui_lang.h"

char **lang_conditions;
void load_conditions(char *file);
char* is_conditional(int id);
char* load_from_file(const char* filename);
void lang_load_from_mem(char *buf);

int num_lines=0;
char** langfile = 0;


////////////////////////////////////////////////////////////////////////////////////////////////
//
// @tsv - Utility to create preparsed gui_lang.c (from concatenation gui_lang.c with .lng file)
//
// USAGE:   makelang path_to_gui_lang.h path_to_english.lng [path_to_secondary.lng] > lang_str.h
//
///////////////////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv )
{
	int i;

	if ( argc < 3 )
	{
		printf("#error No arguments for language file parser. lang_str.h creation was failed\n");
		exit(-1);
	}

	num_lines=0;

	load_conditions(argv[1]);

	char* file1 = load_from_file(argv[2]);
	char* file2 = 0;
	if ( argc > 3 )
	  file2 = load_from_file(argv[3]);

	// Do calculation num of lang strings
	lang_load_from_mem ( file1 );
	lang_load_from_mem ( file2 );

	if ( num_lines <= 0 )
	{
		printf("#error No strings found by language file parser. lang_str.h creation was failed\n");
		exit(-1);
	}

	// Create array
	langfile = malloc( sizeof(char*)*(num_lines+1) );
	for ( i=0; i<=num_lines; i++ )
	  { langfile[i]=0; }

	// Load lang files
	lang_load_from_mem ( file1 );
	lang_load_from_mem ( file2 );


	char* lng_filename = (argc>2 && file2) ? argv[2] : argv[1];

	int num_empty_lines=0;
	char buf[500];

	printf("//Auto generated file. Do not edit the contents of this file.\n//Update the CHDK/LANG/*.lng files to make changes.\n//Generated from %s\n\n",lng_filename);

    // Extract the language name
    char *s = strrchr(lng_filename,'/');
    if (s == 0) s = lng_filename; else s++;
    strcpy(buf,s);
    s = strrchr(buf,'.');
    if (s) *s = 0;

	printf("static char* gui_lang_default = \n");
	
	for ( i=1; i<=num_lines; i++ )
	{
		if ( !langfile[i])
		{
			printf("/*%3d*/ \"\\0\"\n", i);
			num_empty_lines++;
		}
		else
		{
		  {
			// Do shielding of \ symbol
			const char* from = langfile[i];
			char* to = buf;
			
			while(*from)
			{
				*to=*from; to++;
				if (*from=='\\' && from[1]!='"' && from[1]!='n')
				 { *to=*from; to++;}
				from++;
			}
			*to=0;
		  }
		  char *iscond = is_conditional(i);
		  if (iscond)
		  {
		      if (iscond[0] == '!')
		          printf("#ifndef %s\n/*%3d*/ \"%s\"\n#endif\n        \"\\0\"\n", iscond+1, i, buf);
		      else
		          printf("#ifdef %s\n/*%3d*/ \"%s\"\n#endif\n        \"\\0\"\n", iscond, i, buf);
		  }
		  else
		  {
		      printf("/*%3d*/ \"%s\\0\"\n",i,buf);
		  }
		}
	}

    printf(";\n\n");
	
    if ( num_empty_lines > 50 )
	  printf("#warning Too many empty lines are detected. Please ensure that .lng files have serial numbers\n\n");

    printf("//Sanity check of GUI_LANG_ITEMS\n");
    printf("#if (GUI_LANG_ITEMS != %d)\n",num_lines);
    printf("#error GUI_LANG_ITEMS value have to be %d. Please fix at gui_lang.h\n", num_lines);
    printf("#endif\n");
	
    return 0;
}

// Parser language file
// Language strings are collected in langfile
//------------------------------------------------------
void lang_load_from_mem(char *buf) 
{
    char *p, *s, *e;
    int i;
    
    if ( !buf )
     return;

    e = buf-1;
    while(e) 
	{
        p = e+1;
        while (*p && (*p=='\r' || *p=='\n')) ++p; //skip empty lines
        i = strtol(p, &e, 0/*autodetect base oct-dec-hex*/);	// convert "*p" to long "i" and return pointer beyond to e

	if ( !langfile ) {
	   if ( i >= num_lines ) {
	      num_lines = i;
	   }
           e = strpbrk(p, "\r\n");		//find eol
	   continue;
	}
        
	if (e!=p) {
            p = e;
            e = strpbrk(p, "\r\n");		//break string with zero on \r|\n
            if (e) *e=0;
			
            while (*p && *p!='\"') ++p;	// cut string from "" if it exists
            if (*p) ++p;
            s = p; 
            while (*p && (*p!='\"' || *(p-1)=='\\')) ++p;
            *p=0;
		
            langfile[i]=s;		// add string
	}
	else {
	    //skip invalid line
            e = strpbrk(p, "\r\n");
            if (e) *e=0;
        }
    }
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
            buf = malloc(size+1);
            if (buf) {
                size = read(f, buf, size);
                buf[size]=0;
            }
        }
        close(f);
    }
    return buf;
}

//-------------------------------------------------------------------
// Conditional entries
char* skip_space(char *p)
{
    while (*p && ((*p == ' ') || (*p == '\t'))) p++;
    return p;
}

char* next_token(char *p, char* buf)
{
    *buf = 0;
    p = skip_space(p);
    if ((*p == '\r') || (*p == '\n')) return 0;
    while (*p && (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n')) *buf++ = *p++;
    *buf = 0;
    return p;
}

void load_conditions(char *file)
{
    lang_conditions = 0;

    char *buf = load_from_file(file);

    if (!buf)
        return;

    lang_conditions = malloc(sizeof(char*) * (GUI_LANG_ITEMS+1));
    memset(lang_conditions, 0, sizeof(char*) * (GUI_LANG_ITEMS+1));

    char *p, *e;
    char name[500];
    char index[20];
    char cond[200];
    int i;

    e = buf;
    while (e && *e)
    {
        p = e;
        if (strncmp(p, "#define", 7) == 0)
        {
            p = next_token(p+7, name);
            if (p)
            {
                p = next_token(p, index);
                if (p)
                {
                    p = next_token(p, cond);
                    if (p && (strcmp(cond, "//CONDITIONAL:") == 0))
                    {
                        p = next_token(p, cond);
                        if (p)
                        {
                            i = atoi(index);
                            lang_conditions[i] = malloc(strlen(cond)+1);
                            strcpy(lang_conditions[i], cond);
                        }
                    }
                }
            }
        }
        e = strpbrk(e, "\n");
        if (e) e++;
    }

    free(buf);
}

char* is_conditional(int id)
{
    if (lang_conditions && (id>=1) && (id<=GUI_LANG_ITEMS) && lang_conditions[id])
    {
        return lang_conditions[id];
    }
    return 0;
}
