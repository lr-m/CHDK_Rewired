#ifndef EXMEM_H
#define EXMEM_H

// CHDK exmem

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

// Exmem

// returned information about an exmem type
typedef struct
{
    char            *addr; // note: address always includes uncached bit (for now)
    unsigned int     len;
} exmem_alloc_info;

extern unsigned int exmem_type_count;
extern char *get_exmem_type_name(unsigned int type);
extern int get_exmem_type_status(unsigned int type, exmem_alloc_info *allocinf);

// allocates UNCACHED exmem region for exmem type
// only a single allocation by type can exist at any time, assert is thrown otherwise
// allocation must be freed as soon as possible, otherwise normal camera functions might become affected
// allocation info is returned in allocinf, if not NULL
extern void *exmem_alloc_uncached(unsigned int type, unsigned int size, exmem_alloc_info *allocinf);
// frees UNCACHED exmem region
// in case allocation was accessed through CPU cache, caller must flush data cache before freeing
extern void exmem_free_uncached(unsigned int type);

#endif
