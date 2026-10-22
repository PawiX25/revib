#include "common.h"

#include <string.h>

#include "PackedFiles.h"

static int Align4(int size)
{
    return (size + 3) & ~3;
}

static inline u_char ToLower(u_int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

/* Names are compared without regard to case. */
static int CompareNames(const char* a, const char* b)
{
    char ca = ToLower(*a);
    char cb = ToLower(*b);
    if (ca < cb) {
        return -1;
    }
    if (cb < ca) {
        return 1;
    }
    if (ca == '\0') {
        return 0;
    }
    return CompareNames(a + 1, b + 1);
}

/* The size and contents that follow an entry's name. */
static u_long* EntryFile(char* entry)
{
    return (u_long*)(entry + Align4(strlen(entry) + 1));
}

char* PackedFiles::FindEntry(const char* name)
{
    int first = 0;
    int last = m_numFiles;
    while (first < last) {
        int middle = (first + last) / 2;
        char* entry = (char*)this + m_offsets[middle];
        int order = CompareNames(name, entry);
        if (order < 0) {
            last = middle;
        } else if (order == 0) {
            return entry;
        } else {
            first = middle + 1;
        }
    }
    return NULL;
}

u_long* PackedFiles::Find(const char* name)
{
    char* entry = FindEntry(name);
    if (entry != NULL) {
        return EntryFile(entry);
    }
    return NULL;
}

/* "Assertion failed" and this file's name, from an assert whose code is not in the executable */
#ifndef GAME
INCLUDE_RODATA("asm/game/nonmatchings/PackedFiles", D_800194B8);
#else
INCLUDE_RODATA("asm/main_g/nonmatchings/PackedFiles", D_8001E0C8);
#endif
