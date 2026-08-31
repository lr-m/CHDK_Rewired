#include "font_8x16_uni_packed.h"

//-------------------------------------------------------------------
// Currently selected codepage
static int current_cp = 0;

//-------------------------------------------------------------------
unsigned char *get_current_font_data(unsigned char ch)
{
    // Test for 'common' char?
    if (ch < 128)
        return font_data + cp_common[ch];
    // If not return value from current codepage
    return font_data + codepages[current_cp][ch-128];
}

//-------------------------------------------------------------------
void font_set(int codepage)
{
    // Save selected codepage
    current_cp = codepage;
}

//-------------------------------------------------------------------
