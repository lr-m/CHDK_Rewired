#ifndef ERRNO_H
#define ERRNO_H

// CHDK errno

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

// on vxworks we could find the actual errno, but this is easier to automate sig
// doesn't exist on dryos, but we stub it
extern int errnoOfTaskGet(int tid);

#define errno (errnoOfTaskGet(0))

//---------------------------------------------------------------

#endif

