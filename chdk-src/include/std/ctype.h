#ifndef CTYPE_H
#define CTYPE_H

// CHDK ctype

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

extern int isdigit(int c);
extern int isspace(int c);
extern int isalpha(int c);
extern int isupper(int c);
extern int islower(int c);
extern int ispunct(int c);
extern int isxdigit(int c);
extern int iscntrl(int c);
extern int isalnum(int c);

extern int tolower(int c);
extern int toupper(int c);

//---------------------------------------------------------------

#endif
