#ifndef __MODULE_HASH_H__
#define __MODULE_HASH_H__

// Struct for symbol hash table entries
typedef struct
{
    unsigned int    hash;
    const void      *address;
} sym_hash;

#endif /* __MODULE_HASH_H__ */
