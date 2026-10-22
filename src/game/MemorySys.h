#ifndef MEMORYSYS_H
#define MEMORYSYS_H

#include "common.h"

#include <stddef.h>

/* A free block on the malloc3() free list. */
struct HeapBlock {
    HeapBlock* next;
    u_long size; /* in 8-byte units; 0 ends the list */
};

/*
 * The game's heap: a thin layer over the PSY-Q malloc3() allocator that
 * counts allocations, can report the memory map and the free list, and stops
 * the game with a dump of the free list when memory runs out.
 */
class MemorySys {
public:
    static void Init(void* heap, u_long size);
    static void Quit();
    static void Info();
    static void DumpUsage();
    static void DumpHeap();
    static void* malloc(size_t size);
    static void free(void* p);
    static u_long CountHeapFree();

private:
    static u_long FreeBytes()
    {
        u_long total = 0;
        for (HeapBlock* block = s_heap; block->size != 0; block = block->next) {
            total += block->size * 8;
        }
        return total;
    }
    static void ReportOutOfMemory(size_t size);

    /* The compiler lays these out in reverse order of declaration (.sdata). */
    static HeapBlock* s_heap;
    static u_long s_heapSize;
    static int s_numFrees;
    static int s_numAllocs;
};

/* The rest of the game allocates through MemorySys, the STL's allocator included. */
#ifndef MEMORYSYS_IMPLEMENTATION
#define malloc MemorySys::malloc
#define free MemorySys::free
#endif

#endif
