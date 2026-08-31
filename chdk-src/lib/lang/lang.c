#include "stdlib.h"
#include "stddef.h"
#include "string.h"
#include "lang.h"
#include "fileutil.h"

static char* preparsed_lang_default_start=0;
//static char* preparsed_lang_default_end=0;  // seems no longer needed

// This is threshold to determine is this id in .lng or just string
#define MAX_LANGID 0x1000

/*
 *  Warning: lang string references are stored as 15 bit offsets to save space
 *  This will break once any language resource exceeds approx. 32766 bytes in size
 *
 *  the language buffer is slightly larger than optimal, due to the reason
 *  that no effort is being made to identify escape sequences
 *  while calculating total string length in lang_parse_from_mem()
 *  for example, the English resource has ~50 bytes surplus when loaded (as of 03/2016)
 */

//-------------------------------------------------------------------

static short* strings = NULL;        // string offset list (allocated at heap or mapped from gui_lang.c);
static int count = 0;                // current maximal string id (init at lang_init with GUI_LANG_ITEMS)
static char* langbuf = NULL;         // buffer for loaded lang strings
static int langbuflen = 0;           // length of langbuf
static int lbpointer;                // used while loading lang strings

//-------------------------------------------------------------------
void lang_init(int num) {

    if (strings) {
        free(strings);
        count = 0;
    }

    ++num;
    strings = malloc(num*sizeof(short));
    if (strings) {
        memset(strings, 0, num*sizeof(short));
        count = num;
    }

}

// create place for string in langbuf
//-------------------------------------------------------------------
static char* placelstr(int size) {
    char *ret = langbuf + lbpointer;
    lbpointer += size;
    if (lbpointer <= langbuflen) {
        return ret;
    }
    return 0;
}

// add to string list string "str" with id "num"
//-------------------------------------------------------------------
static void lang_add_string(int num, const char *str) {
    int f=0;
    char *p;

    if (strings && num<count) {

       p = placelstr(strlen(str)+1);
       if (p) {
           strings[num] = -(1 + p - langbuf); // lang string offsets are encoded as negative
           for (; *str; ++str) {
                if (f) {
                    if (*str == '"' || *str == '\\') *(p-1)=*str;
                    else if (*str == 'n') *(p-1)='\n';
                    else *p++=*str;
                    f = 0;
                } else {
                    *p++=*str;
                    if (*str == '\\') {
                        f = 1;
                    }
                }
           }
           *p=0;
       }
    }
}

// Parsing of loaded .lng file
// buf - source array (content of file.lng )
//-------------------------------------------------------------------
int lang_parse_from_mem(char *buf, int size) {
    char *p, *s, *e, *b;
    int i, langbufneed;
    char** pstrtmp;

	if  ( size <= 0 )
	  return 0;

    langbufneed = 0;
    lbpointer = 0;
    // allocate temporary array for storing pointers to found strings
    pstrtmp = malloc(count*sizeof(char*));
    if (!pstrtmp) {
        return 0;
    }
    // needs to be zeroed
    memset(pstrtmp, 0, count*sizeof(char*));

    // initialize _final_ offset array with built-in strings
    char* load_builtin_lang_strings(int, short*);
    load_builtin_lang_strings(count-1, strings);

    e = buf-1;
    while(e) {
        p = e+1;
        while (*p && (*p=='\r' || *p=='\n')) ++p; //skip empty lines
        i = strtol(p, &e, 0/*autodetect base oct-dec-hex*/);    // convert "*p" to long "i" and return pointer beyond to e
        if (e!=p) {
            p = e;
            e = strpbrk(p, "\r\n");        //break string with zero on \r|\n
            if (e) *e=0;

            while (*p && *p!='\"') ++p;    // cut string from "" if it exists
            if (*p) ++p;
            s = p;
            while (*p && (*p!='\"' || *(p-1)=='\\')) ++p;
            *p=0;

            // store string address and add its length to total
            if ((i > 0) && (i<count)) {
                // ignore string IDs that have zero length built-in string
                // rationale: built-in strings are always complete (have English fallback),
                // except for IDs that are disabled for the port
                //
                // lang_str() only returns built-in strings at this point
                b = lang_str(i);
                if (b && b[0]!=0) {
                    langbufneed += strlen(s) + 1;
                    pstrtmp[i] = s;
                }
            }
        } else { //skip invalid line
            e = strpbrk(p, "\r\n");
            if (e) *e=0;
        }
    }

    if ((langbufneed>langbuflen)||(!langbuf)) { // existing buffer is too small or not allocated
        if (langbuf) {
            free(langbuf);
        }
        langbuf = malloc(langbufneed);
        if (langbuf) {
            langbuflen = langbufneed;
        }
        else {
            langbuflen = 0;
        }
    }
    // zeroing is not required (holes between strings will contain junk, but that doesn't hurt)
    // memset(langbuf, 0, langbuflen);

    for (i=1; i<count; i++) {
        if (pstrtmp[i]) {   // add string if it exists
            lang_add_string(i, pstrtmp[i]);
        }
    }

    free(pstrtmp);
	return 1;
}

// init string offset array to built-in defaults
//-------------------------------------------------------------------
char* load_builtin_lang_strings(int cnt, short* array) {
    int i;
    char *p = preparsed_lang_default_start;

    for ( i = 1; i<=cnt; i++ )
    {
        array[i] = 1 + p - preparsed_lang_default_start;
        while (*p) p++;
        p++;
    }
    return p;
}

// This function have to be called before any other string load
//-------------------------------------------------------------------
void lang_map_preparsed_from_mem( char* gui_lang_default, int num )
{

    preparsed_lang_default_start = gui_lang_default;
    lang_init( num );
    // preparsed_lang_default_end =  // variable not currently needed
    load_builtin_lang_strings(num, strings);

}

//-------------------------------------------------------------------
void lang_load_from_file(const char *filename) {
    process_file(filename, lang_parse_from_mem, 1);
}


//-------------------------------------------------------------------
char* lang_str(int str) {
    if (str && str<MAX_LANGID) {
        if (strings && str<count && strings[str]) {
            if (strings[str]>0) {
                // string from builtin resource
                return preparsed_lang_default_start + strings[str] - 1;
            }
            else if (langbuf) {
                // string from loaded lang file
                return langbuf - strings[str] - 1;
            }
        }
    } else { // not ID, just char*
        return (char*)str;
    }
    return "";
}

//-------------------------------------------------------------------
// make hash of string
unsigned int lang_strhash31(int langid)
{
    if ( langid<MAX_LANGID ) 
		return langid;

	unsigned char* str = (unsigned char*)langid;
	unsigned hash=0;
	for ( ; *str; str++ )
		hash = *str ^ (hash<<6) ^ (hash>>25);
	if ( hash<MAX_LANGID )
		hash |= (1<<31);
	return hash;
}
