#ifndef STDDEF_H
#define STDDEF_H

// CHDK stddef

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

typedef unsigned int size_t;

#define NULL ((void*)0)

// For building Lua
typedef int ptrdiff_t;

//---------------------------------------------------------------

#endif
