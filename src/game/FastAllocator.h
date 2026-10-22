#ifndef FASTALLOCATOR_H
#define FASTALLOCATOR_H

#include "common.h"

#include <stddef.h>

class MemoryPool;

/*
 * Blocks of 4 to 1032 bytes for objects made and deleted often: a deleted
 * block is kept for the next one of its size rather than given back.
 */
class FastAllocator {
public:
    static void* Allocate(size_t size);
    static void Free(void* p, size_t size);

private:
    static MemoryPool s_pool;
};

#endif
