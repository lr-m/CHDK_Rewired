/* 

SUBA.C

Based on suba - sub-allocate memory from larger chunk of memory
 * Copyright (c) 2003 Michael B. Allen <mba2000 ioplex.com>
 *
 * The MIT License
 * 
*/

// Rewritten for CHDK.

#include "stdlib.h"
#include "stddef.h"
#include "string.h"
#include "semaphore.h"

#define SUBA_SEMAPHORE_TIMEOUT  1000
#define SANITY_CHECKS           1

#define SUBA_MAGIC 0x53554241
#define CELL_MAGIC 0x5342434C

// Free list cell header
typedef struct {
    unsigned int    magic;      // cell check value
    size_t          size;
} cell_hdr;

// Free list cell
typedef struct _cell {
    cell_hdr        h;
    struct _cell*   next;       // link to next cell in free list
} cell;

// Memory allocator
typedef struct {
	unsigned int    magic;      // suba header identifier
	unsigned int*   magic2;     // guard value at the end of the block
    size_t          size;       // total size of memory area
	int             sem_id;     // Canon semaphore id
	cell            head;       // List header
	size_t          mincell;    // min cell size must be at least sizeof cell

    // CHDK added counters, modeled on DryOS values
    int free_block_count;       // Length of free list
    int free_size;              // Available memory in free list
    cell* largest_block;        // Largest block on free list
    size_t allocated_count;     // total number of current allocations
    size_t allocated_size;      // total amount of memory currently allocated, excluding overhead
    size_t allocated_peak;      // maximum value of allocated_size encountered
} allocator;

// Cell / Pointer manipulation macros
#define ALIGNMASK           (sizeof(cell*)-1)
#define ALIGN(s)            (((s) + ALIGNMASK) & ~ALIGNMASK)

#define ACTUALSIZE(size)    (size + sizeof(cell_hdr))
#define ALLOCSIZE(size)     (size - sizeof(cell_hdr))
#define CELL2ALLOC(c)       (void*)((void*)c + sizeof(cell_hdr))
#define ALLOC2CELL(p)       (cell*)((void*)p - sizeof(cell_hdr))

#define CEND(c)             ((cell*)((char*)c + c->h.size))
#define ISADJ(c1,c2)        (CEND(c1) == (c2))

#if SANITY_CHECKS
static cell*
check_cell(const allocator *suba, cell* c, char *fn, int ln)
{
    if (suba && (c > (cell*)suba) && (c < (cell*)((char*)suba + suba->size))) {
        if (c->h.magic != CELL_MAGIC)
            DebugAssert("Corrupt cell header", ln);
        return c;
    }
    char buf[200];
    sprintf(buf, "%s bad cell - %x %x", fn, c, suba);
    DebugAssert(buf, ln);
    return NULL;
}
#else
#define check_cell(s, c, f, l) (c)
#endif

#ifdef SUBA_HOST_DEBUG
// For testing on host (not in CHDK). Print free list to stdout.
void suba_dump(allocator *suba)
{
    cell *c = &suba->head;
    printf("%x %d => ", c, suba->mincell);
    while (c->h.size != 0)
    {
        printf("%x %d %x, ", c, c->h.size, c->next);
        c = c->next;
    }
    printf("%x %d %x\n", c, c->h.size, c->next);
}

int check_stats(allocator *suba)
{
    if (!TakeSemaphore(suba->sem_id, SUBA_SEMAPHORE_TIMEOUT))
    {
        int count = -1;
        int free_size = 0;
        cell* largest = &suba->head;
        cell* c = check_cell(suba, &suba->head, "suba_getmeminfo", __LINE__);
        while (c->h.size != 0)
        {
            count += 1;
            free_size += ALLOCSIZE(c->h.size);
            if (c->h.size > largest->h.size) largest = c;
            c = check_cell(suba, c->next, "suba_getmeminfo", __LINE__);
        }

        if (suba->largest_block == NULL)
            suba->largest_block = largest;

        int rv = 1;
        if (suba->largest_block->h.size != largest->h.size)
        {
            printf("Mismatch - largest %x %d %x %d\n", suba->largest_block, suba->largest_block->h.size, largest->h.size);
            rv = 0;
        }
        if (suba->free_size != free_size)
        {
            printf("Mismatch - free_size\n");
            rv = 0;
        }
        if (suba->free_block_count != count)
        {
            printf("Mismatch - free_block_count\n");
            rv = 0;
        }

        GiveSemaphore(suba->sem_id);

        return rv;
    }
    else
    {
        DebugAssert("check_stats TakeSemaphore fail", __LINE__);
    }

    return 0;
}
#endif
#ifdef SUBA_DEBUG
void suba_dump(allocator *suba)
{
    char buf[200];
    FILE *fp = fopen("A/suba_dump.log", "w");
    cell *c = &suba->head;
    sprintf(buf, "%x %d %x %d =>\n", c, suba->mincell, suba->largest_block, suba->largest_block->h.size);
    fwrite(buf, strlen(buf), 1, fp);
    while (c->h.size != 0)
    {
        sprintf(buf, "%x %d %x\n", c, c->h.size, c->next);
        fwrite(buf, strlen(buf), 1, fp);
        c = c->next;
    }
    sprintf(buf, "%x %d %x\n", c, c->h.size, c->next);
    fwrite(buf, strlen(buf), 1, fp);
    fclose(fp);
}

void check_stats(allocator *suba)
{
    int count = -1;
    int free_size = 0;
    cell* largest = &suba->head;
    cell* c = check_cell(suba, &suba->head, "check_stats", __LINE__);
    while (c->h.size != 0)
    {
        count += 1;
        free_size += ALLOCSIZE(c->h.size);
        if (c->h.size > largest->h.size) largest = c;
        c = check_cell(suba, c->next, "check_stats", __LINE__);
    }

    char buf[200];

    if (suba->largest_block->h.size != largest->h.size)
    {
        suba_dump(suba);
        sprintf(buf, "largest %x %d %x %d\n", suba->largest_block, suba->largest_block->h.size, largest, largest->h.size);
        DebugAssert(buf, __LINE__);
    }
    if (suba->free_size != free_size)
    {
        suba_dump(suba);
        sprintf(buf, "free_size %d %d\n", suba->free_size, free_size);
        DebugAssert(buf, __LINE__);
    }
    if (suba->free_block_count != count)
    {
        suba_dump(suba);
        sprintf(buf, "free_count %d %d\n", suba->free_block_count, count);
        DebugAssert(buf, __LINE__);
    }
}
#endif

void
suba_getmeminfo(allocator *suba, int *allocated_size, int *allocated_peak, int *allocated_count, int *free_size, int *largest_block, int *free_block_count)
{
    if (!TakeSemaphore(suba->sem_id, SUBA_SEMAPHORE_TIMEOUT))
    {
        if (suba->magic != SUBA_MAGIC || *suba->magic2 != SUBA_MAGIC)
            DebugAssert("suba_getmeminfo BAD MAGIC", __LINE__);

        // Find largest block if not currently known
        if (suba->largest_block == NULL)
        {
            cell* largest = &suba->head;
            cell* c = check_cell(suba, &suba->head, "suba_getmeminfo", __LINE__);
            while (c->h.size != 0)
            {
                if (c->h.size > largest->h.size) largest = c;
                c = check_cell(suba, c->next, "suba_getmeminfo", __LINE__);
            }
            suba->largest_block = largest;
        }

        *largest_block = ALLOCSIZE(suba->largest_block->h.size);
        *free_size = suba->free_size;
        *free_block_count = suba->free_block_count;

        *allocated_size = suba->allocated_size;
        *allocated_peak = suba->allocated_peak;
        *allocated_count = suba->allocated_count;

#ifdef SUBA_DEBUG
        check_stats(suba);
#endif

        GiveSemaphore(suba->sem_id);
    }
    else
    {
        DebugAssert("suba_getmeminfo TakeSemaphore fail", __LINE__);
    }
}

allocator *
suba_init(allocator *suba, size_t size, size_t mincell, char *name)
{
	size_t hdrsiz;

#if SANITY_CHECKS
	if (suba == NULL || size == 0) {
		DebugAssert("Invalid parameters to 'suba_init'", __LINE__);
		return NULL;
	}
#endif

	size = size & ~ALIGNMASK;
    hdrsiz = ALIGN(sizeof(allocator));
    mincell = ACTUALSIZE(ALIGN(mincell));

	// Initialise allocator header
    memset(suba, 0, hdrsiz);
    suba->magic = SUBA_MAGIC;
    // cell data must be large enough for next pointer
    suba->mincell = sizeof(cell);
    if (mincell > suba->mincell)
        suba->mincell = mincell;
    suba->size = size;
    suba->sem_id = CreateBinarySemaphore(name, 1);

    // Initialise free list
    cell *c1, *c2;

    // First cell - allocation block (size - header - empty terminator cell)
    c1 = (cell*)((void*)suba + hdrsiz);
    c1->h.size = size - hdrsiz - sizeof(cell);
    c1->h.magic = CELL_MAGIC;

    // Second cell - list terminator (size = 0, next = 0)
    c2 = CEND(c1);
    c2->h.size = 0;
    c2->h.magic = CELL_MAGIC;
    c2->next = 0;

    // Link first to second, reduce size of first so it will never merge with second terminator cell
    c1->next = c2;
    c1->h.size -= sizeof(cell*);

    // Guard value at the end of the memory block;
    suba->magic2 = (unsigned int*)CEND(c1);
    *suba->magic2 = SUBA_MAGIC;

    // Initial stats
    suba->free_block_count = 1;
    suba->free_size = ALLOCSIZE(c1->h.size);
    suba->largest_block = c1;

    // List head - dummy cell, separate from main block so will never be merged, too small to ever be allocated
    suba->head.h.size = sizeof(cell_hdr);
    suba->head.h.magic = CELL_MAGIC;
    suba->head.next = c1;

	return suba;
}

void *
suba_alloc(allocator *suba, size_t size, int zero)
{
	cell *this_cell = NULL;

	// sanitise size & calc real size needed
	size = ALIGN(size);
    size_t cellsize = ACTUALSIZE(size);
    if (cellsize < suba->mincell) cellsize = suba->mincell;

	if (!TakeSemaphore(suba->sem_id, SUBA_SEMAPHORE_TIMEOUT))
	{
	    // Find first block big enough to satisfy request
	    cell *prev_cell = &suba->head;
        this_cell = check_cell(suba, prev_cell->next, "suba_alloc", __LINE__);
        while (this_cell->h.size > 0 && this_cell->h.size < cellsize) {
            prev_cell = this_cell;
            this_cell = check_cell(suba, this_cell->next, "suba_alloc", __LINE__);
        }

        // Check if block large enough was found
        if (this_cell->h.size > 0) {
            if (this_cell == suba->largest_block)
                suba->largest_block = NULL;     // Force recalculation of largest block in suba_getmeminfo

            if (this_cell->h.size > (cellsize + suba->mincell)) {
                // split new cell - return from end of larger block
                this_cell->h.size = this_cell->h.size - cellsize;
                this_cell = CEND(this_cell);
                this_cell->h.size = cellsize;
                this_cell->h.magic = CELL_MAGIC;
                suba->free_size -= cellsize;
            } else {
                // use the entire cell - unlink from free list
                prev_cell->next = this_cell->next;
                cellsize = this_cell->h.size;
                suba->free_block_count -= 1;
                suba->free_size -= ALLOCSIZE(cellsize);
            }

            // CHDK counters
            suba->allocated_size += ALLOCSIZE(cellsize);
            suba->allocated_count++;
            if(suba->allocated_size > suba->allocated_peak) {
                suba->allocated_peak = suba->allocated_size;
            }
        } else {
            this_cell = NULL;
        }

        GiveSemaphore(suba->sem_id);
    }
    else
    {
        DebugAssert("suba_alloc TakeSemaphore fail", __LINE__);
    }

	void *p = NULL;

	if (this_cell != NULL)
	{
	    p = CELL2ALLOC(this_cell);
	    if (zero) memset(p, 0, ALLOCSIZE(cellsize));
	}

	return p;
}

int
suba_free(allocator *suba, void *ptr)
{
    cell* this_cell = ALLOC2CELL(ptr);

#if SANITY_CHECKS
    // Sanity checks
    check_cell(suba, this_cell, "suba_free", __LINE__);
    if (this_cell->h.size > suba->size) {
        DebugAssert("Trying to free bad block", __LINE__);
        return -1;
    }
#endif

    if (!TakeSemaphore(suba->sem_id, SUBA_SEMAPHORE_TIMEOUT))
    {
        // CHDK counters
        suba->allocated_size -= ALLOCSIZE(this_cell->h.size);
        suba->allocated_count--;

        // List header
        cell* prev_cell = &suba->head;

        // find insertion point
        while (prev_cell->next < this_cell) {
            prev_cell = check_cell(suba, prev_cell->next, "suba_free", __LINE__);
        }
        cell* next_cell = check_cell(suba, prev_cell->next, "suba_free", __LINE__);

        // do prev cell and this cell need to be joined?
        if (ISADJ(prev_cell,this_cell)) {
            suba->free_size += this_cell->h.size;
            prev_cell->h.size += this_cell->h.size;
            this_cell = prev_cell;
        } else {
            this_cell->next = prev_cell->next;
            prev_cell->next = this_cell;
            suba->free_block_count += 1;
            suba->free_size += ALLOCSIZE(this_cell->h.size);
        }

        // do this cell and next cell need to be joined?
        if (ISADJ(this_cell,next_cell)) {
            this_cell->next = next_cell->next;
            this_cell->h.size += next_cell->h.size;
            suba->free_block_count -= 1;
            suba->free_size += sizeof(cell_hdr);
        }

        // Update largest_block if null or this one is now bigger
        if ((suba->largest_block != NULL) && (this_cell->h.size > suba->largest_block->h.size))
            suba->largest_block = this_cell;

        GiveSemaphore(suba->sem_id);
    }
    else
    {
        DebugAssert("suba_free TakeSemaphore fail", __LINE__);
        return -1;
    }

	return 0;
}
