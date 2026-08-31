#include "stdio.h"
#include "string.h"
#include "../lib/font/font_8x16_uni.h"
#include "../lib/font/codepages.h"

//
// Tool to generate run-time font and codepage data
//
// Creates compressed font_data array and run-time codepage tables
// Avoids generating static tables at startup of CHDK.
//

//-------------------------------------------------------------------
#define DEFAULT_SYMBOL          0xFFFD

//-------------------------------------------------------------------
// Packed font data
unsigned char font_data[16384];
unsigned short char_codes[4096];    // List of used charcodes
unsigned short font_offsets[4096];  // Offsets into font_data for each charcode
unsigned char fontdata_lookup[256]; // Lookup table (bytes actually used in font data)
int fontdata_ltlength = 0;

//-------------------------------------------------------------------
// Helper
unsigned char fdata_usage[256];

//-------------------------------------------------------------------
static int font_make_lookuptable()
{
    int i, j;
    unsigned char k;

    memset(fdata_usage,0,256);

    // determine if a byte is used
    for (i=0; orig_font_data[i].charcode != -1; i++)
    {
        if (orig_font_data[i].isUsed > 0)
        {
            for (j=0; j<16; j++)
            {
                k = orig_font_data[i].data[j];
                fdata_usage[k] = 1;
            }
        }
    }

    memset(fontdata_lookup, 0, 256);
    for (i=0, j=0; j<256; j++)
    {
        if (fdata_usage[j] != 0)
        {
            fontdata_lookup[i] = j;
            fdata_usage[j] = i; // store index
            i++;
        }
    }

    fontdata_ltlength = i;
    return i;
}

//-------------------------------------------------------------------
static unsigned short font_find_matching_glyph(int index)
{
    int i;
    int j;

    for (i=0; orig_font_data[i].charcode != -1; i++)
    {
        if (i >= index)
        {
            break;
        }
        j = memcmp(&orig_font_data[i].data[0], &orig_font_data[index].data[0], 16);
        if ((j == 0) && (orig_font_data[i].isUsed))
        {
            return i;
        }
    }
    return 0xffff;
}

//-------------------------------------------------------------------
static unsigned short font_find_offset(unsigned short charcode)
{
    int i=0;

    for (i=0; char_codes[i] != 0xFFFF; i++)
    {
        if (char_codes[i] == charcode)
            return font_offsets[i];
    }
    return 0xFFFF;
}

//-------------------------------------------------------------------
static void font_init_data(unsigned short *src, int st, int num)
{
    int i;
    unsigned short f;

    for (i=0; i<num; ++i)
    {
        f = font_find_offset(src[i]);
        if (f == 0xFFFF)
            f = font_find_offset(DEFAULT_SYMBOL);
        src[st+i] = f;
    }
}

//-------------------------------------------------------------------
static void check_used(unsigned short *cp)
{
    int i;
    for (i=0; i<128; i++)
    {
        unsigned short c = cp[i];
        int j;
        for (j=0; orig_font_data[j].charcode != -1; j++)
        {
            if (orig_font_data[j].charcode == c)
            {
                orig_font_data[j].isUsed++;
            }
        }
    }
}

//-------------------------------------------------------------------

int main()
{
    int i, j, f = 0, cc = 0, r;
    unsigned char uc;
    unsigned short matches;

    int ncp = sizeof(codepages)/(sizeof(unsigned short*));

    int lte = 0; // encoding via lookup-table, if true

    // Check if chars in font are used in any codepages
    check_used(cp_common);
    for (j=0; j<ncp; j++)
    {
        check_used(codepages[j]);
    }

    // make lookup-table, return number of distinct bytes in font data
    lte = (font_make_lookuptable() < 128);

    printf("#ifndef GET_FONT_COMPRESSION_MODE\n\n");
    printf("// This is a compressed version of font_8x16_uni.h produced by the tools/font_8x16_pack program\n\n");
    printf("// Format of each character is 'FontData' structure, followed by FontData.size bytes of character data.\n\n");
    printf("static unsigned char font_data[] = {\n");

    for (i=0; orig_font_data[i].charcode != -1; i++)
    {
        if (orig_font_data[i].isUsed > 0)
        {
            int top = 0;
            int bottom = 0;
            for (j=0; j<16 && orig_font_data[i].data[j] == 0; j++) { top++; }
            for (j=15; j>=top && orig_font_data[i].data[j] == 0; j--) { bottom++; }
            if (top == 16)  // Blank character
            {
                // Fix values to fit into 4 bits each (sorted out in draw_char function)
                top--;
                bottom++;
            }

            char_codes[cc] = orig_font_data[i].charcode;
            font_offsets[cc] = f;
            cc++;
            matches = font_find_matching_glyph(i);
            if (matches != 65535)
            {
                printf("/*%04x == %04x*/", orig_font_data[i].charcode, orig_font_data[matches].charcode);
                font_offsets[cc-1] = font_find_offset(orig_font_data[matches].charcode);
            }
            else
            {
                font_data[f++] = (top << 4) | bottom;

                printf("/*%04x*/ 0x%02x,", orig_font_data[i].charcode, (top << 4) | bottom);
                r = f;
                for (j=top; j<16-bottom; j++)
                {
                    if (!lte)
                    {
                        font_data[f++] = orig_font_data[i].data[j];
                        printf(" 0x%02x,",orig_font_data[i].data[j]);
                    }
                    else
                    {
                        // lookup-table based encoding
                        uc = orig_font_data[i].data[j];
                        int cnt = 1;
                        int k;
                        for (k = j + 1; (k < 16 - bottom) && (uc == orig_font_data[i].data[k]); k++)
                        {
                            cnt += 1;
                        }
                        if (cnt == 1)
                        {
                            font_data[f] = fdata_usage[uc]; // byte's index in lookup table
                            f++;
                        }
                        else if ((cnt == 2) || (fontdata_ltlength >= 112))
                        {
                            font_data[f] = fdata_usage[uc] | 0x80; // byte's index in lookup table with bit 7 set
                            f++;
                            j++;
                        }
                        else
                        {
                            font_data[f] = (cnt-1) | 0xf0; // repeat count
                            f++;
                            font_data[f] = fdata_usage[uc]; // byte's index in lookup table
                            f++;
                            j += (cnt - 1);
                        }
                    }
                }
                if (lte)
                {
                    for (j=r; j<f; j++)
                    {
                        printf(" 0x%02x,",font_data[j]);
                    }
                }
            }
            printf("\n");
        }
    }

    char_codes[cc] = 0xFFFF;

    printf("};\n\n");
    printf("// font_data length: %d bytes\n", f);

    if (lte)
    {
        printf("unsigned char fontdata_lookup[] = {\n    ");
        for (i=0; i<fontdata_ltlength; i++)
        {
            printf("0x%02x,", fontdata_lookup[i]);
            if ( ((i+1) & 0xf) == 0 )
            {
                printf("\n    ");
            }
        }
        printf("};\n");
        printf("// lookup table length: %d bytes\n\n", fontdata_ltlength);
#if 0
        printf("    /* font data byte distribution\n    ");
        for (i=0; i<256; i++)
        {
            printf("0x%02x,", fdata_usage[i]);
            if ( ((i+1) & 0xf) == 0 )
            {
                printf("\n    ");
            }
        }
        printf("*/\n\n");
#endif
    }

    // Set up codepage entries, and save to file
    font_init_data(cp_common, 0, 128);

    printf("// Offsets to font character data stored in the font_data array.\n\n");
    printf("static unsigned short cp_common[] =\n{\n");
    for (i=0; i<128; i++)
    {
        if ((i & 15) == 0) printf("    ");
        printf("0x%04x,", cp_common[i]);
        if ((i & 15) == 15) printf("\n");
    }
    printf("};\n");

    for (j=0; j<ncp; j++)
    {
        font_init_data(codepages[j], 0, 128);
        printf("static unsigned short cp_win_%s[] =\n{\n", cp_names[j]);
        for (i=0; i<128; i++)
        {
            if ((i & 15) == 0) printf("    ");
            printf("0x%04x,", codepages[j][i]);
            if ((i & 15) == 15) printf("\n");
        }
        printf("};\n");
    }

    printf("\n// Array of pointers to codepage tables.\n\n");
    printf("static unsigned short* codepages[] =\n{\n");
    for (j=0; j<ncp; j++)
    {
        printf("    cp_win_%s,\n", cp_names[j]);
    }
    printf("};\n");

    printf("\n// Codepage names for menu UI in gui.c (gui_font_enum).\n\n");
    printf("int num_codepages = %d;\n",ncp);
    printf("char* codepage_names[] =\n{\n");
    for (j=0; j<ncp; j++)
    {
        printf("    \"Win%s\",\n", cp_names[j]);
    }
    printf("};\n\n");
    printf("#endif // !GET_FONT_COMPRESSION_MODE\n\n");

    if (lte)
    {
        if (fontdata_ltlength < 112)
            printf("#define BUILTIN_FONT_RLE_COMPRESSED_V2 1\n\n");
        else
            printf("#define BUILTIN_FONT_RLE_COMPRESSED 1\n\n");
    }

    return 0;
}
