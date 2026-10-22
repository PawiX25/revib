#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "FastAllocator.h"

/* the blocks of each size, from 4 bytes to `maxSize` */
class MemoryPool {
public:
    MemoryPool(size_t maxSize);
    ~MemoryPool();

    void* Allocate(size_t size)
    {
#line 32 "C:/psj/dev/locale/game/FastAllocator.cpp"
        assert(size >= sizeof(Block));
#line 35 "C:/psj/dev/locale/game/FastAllocator.cpp"
        assert(size <= m_maxSize);
#line 22 "src/game/FastAllocator.cpp"
        Block* block = m_free[size];
        if (block != NULL) {
            m_free[size] = block->next;
        } else {
            m_newCount++;
            m_newCounts[size]++;
            block = (Block*)new char[size];
#line 46 "C:/psj/dev/locale/game/FastAllocator.cpp"
            assert(block != NULL);
#line 32 "src/game/FastAllocator.cpp"
        }
        m_allocCount++;
        return block;
    }

    void Free(void* p, size_t size)
    {
        Block* block = (Block*)p;
        m_freeCount++;
        block->next = m_free[size];
        m_free[size] = block;
    }

private:
    /* a block not in use */
    struct Block {
        Block* next;
    };

    int m_newCount;     /* blocks made */
    int m_allocCount;   /* blocks given out */
    int m_freeCount;    /* blocks given back */
    size_t m_maxSize;
    Block** m_free;     /* the blocks not in use, of each size */
    int* m_newCounts;   /* blocks made, of each size */
};

MemoryPool::MemoryPool(size_t maxSize)
    : m_newCount(0), m_allocCount(0), m_freeCount(0), m_maxSize(maxSize)
{
    m_free = new Block*[maxSize + 1];
    m_newCounts = new int[maxSize + 1];
    for (size_t size = 0; size <= maxSize; size++) {
        m_free[size] = NULL;
        m_newCounts[size] = 0;
    }
}

/* The blocks not in use are deleted, with a report of how the pool was used. */
MemoryPool::~MemoryPool()
{
    printf("MemoryPool::~MemoryPool()\n");
    printf("    allocation count = %d\n", m_allocCount);
    printf("    deallocation count = %d\n", m_freeCount);
    printf("    new count = %d\n", m_newCount);
    int deleteCount = 0;
    for (size_t size = 0; size <= m_maxSize; size++) {
        if (m_free[size] != NULL) {
            int count = 0;
            while (m_free[size] != NULL) {
                Block* block = m_free[size];
                count++;
                deleteCount++;
                m_free[size] = block->next;
                delete[] (char*)block;
            }
            printf("    size %d block(s): %d allocated, %d deallocated.\n", size, m_newCounts[size], count);
        }
    }
    printf("    delete count = %d\n", deleteCount);
#line 102 "C:/psj/dev/locale/game/FastAllocator.cpp"
    assert(m_allocCount == m_freeCount);
#line 95 "src/game/FastAllocator.cpp"
    delete[] m_free;
    delete[] m_newCounts;
}

MemoryPool FastAllocator::s_pool(1032);

void* FastAllocator::Allocate(size_t size)
{
    return s_pool.Allocate(size);
}

void FastAllocator::Free(void* p, size_t size)
{
    s_pool.Free(p, size);
}
