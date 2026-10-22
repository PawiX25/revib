#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
extern "C" {
#include <malloc.h>
}

#define MEMORYSYS_IMPLEMENTATION
#include "MemorySys.h"

extern "C" {
/* Memory layout for the PSY-Q start-up code: 2 MB of RAM, 16 KB of stack. */
unsigned long _ramsize = 0x00200000;
unsigned long _stacksize = 0x00004000;

/* Section sizes (the addresses of these symbols) and the heap size, provided by the linker / start-up code. */
extern int __textlen, __rdatalen, __datalen, __sdatalen, __bsslen, __sbsslen, __ctorslen, __dtorslen;
extern unsigned long __heapsize;
}

HeapBlock* MemorySys::s_heap;
u_long MemorySys::s_heapSize;
int MemorySys::s_numFrees;
int MemorySys::s_numAllocs;

void MemorySys::Init(void* heap, u_long size)
{
    size &= ~7;
    s_heapSize = size;
#line 134 "C:/psj/dev/locale/game/MemorySys.cpp"
    assert(((u_long)heap & 3) == 0);
#line 35 "src/game/MemorySys.cpp"
    s_heap = (HeapBlock*)heap;
    InitHeap3((u_long*)heap, size);
}

void MemorySys::Quit() {}

/* Unreferenced: "MemorySys: operation on block already freed\n" */
INCLUDE_RODATA("asm/game/nonmatchings/MemorySys", D_80019294);

inline void MemorySys::ReportOutOfMemory(size_t size)
{
    printf("MemorySys::malloc() failed\n");
    printf("\theap free:%d, requested:%d\n\r", FreeBytes(), size);
    DumpHeap();
#line 320 "C:/psj/dev/locale/game/MemorySys.cpp"
    assert(0);
#line 52 "src/game/MemorySys.cpp"
}

void MemorySys::Info()
{
    u_long rdata = (u_long)&__rdatalen;
    u_long sbss = (u_long)&__sbsslen;
    u_long sdata = (u_long)&__sdatalen;
    u_long dat = (u_long)&__datalen;
    u_long bss = (u_long)&__bsslen;
    u_long ctors = (u_long)&__ctorslen;
    u_long dtors = (u_long)&__dtorslen;
    u_long data = bss + sbss + dat + sdata + rdata + ctors + dtors;

    printf("MemorySys::Info()\n");
    printf("\tramsize: %7lu\n", _ramsize);
    printf("\t----------------\n");
    printf("\t kernel: %7lu\n", 0x10000);
    printf("\t  stack: %7lu\n", _stacksize);
    printf("\t   code: %7lu\n", (u_long)&__textlen);
    printf("\t   data: %7lu\n", data);
    printf("\t   heap: %7lu\n", __heapsize);
}

void MemorySys::DumpUsage()
{
    int used = s_heapSize - sizeof(HeapBlock);
    used -= CountHeapFree();
    printf("HEAP used: %d", used);
    printf(",  free: %d\n", FreeBytes());
}

void MemorySys::DumpHeap()
{
    printf("DumpHeap: ");
    for (HeapBlock* block = s_heap; block->size != 0; block = block->next) {
        int bytes = block->size * 8;
        if (bytes >= 1024) {
            printf("%dK  ", bytes / 1024);
        } else {
            printf("%d  ", bytes);
        }
    }
    printf("\n");
}

/* Called by the PSY-Q start-up code to set up the heap. */
extern "C" void InitHeap(unsigned long* heap, unsigned long size)
{
    MemorySys::Init(heap, size);
}

void* MemorySys::malloc(size_t size)
{
    void* p;
    if (size == 0) {
        p = NULL;
    } else {
        p = malloc3(size);
        if (p == NULL) {
            ReportOutOfMemory(size);
        }
        s_numAllocs++;
    }
    return p;
}

void MemorySys::free(void* p)
{
    if (p != NULL) {
        free3(p);
        s_numFrees++;
    }
}

u_long MemorySys::CountHeapFree()
{
    return FreeBytes();
}

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/game/nonmatchings/MemorySys", D_800193B4);
