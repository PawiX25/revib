#ifndef MEMPTR_H
#define MEMPTR_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "FileSys.h"

/*
 * A file loaded into memory, shared through a reference count and freed
 * when the last MemPtr to it goes away. FileSys::LoadFile() looks for a
 * missing file until it is found; the game stops if it gives no data.
 */
template <class T>
class MemPtr {
public:
    MemPtr() : m_refs(NULL) {}

    MemPtr(const char* name) : m_refs(new int(1)), m_file(FileSys::LoadFile(name))
    {
        if (m_file.data == NULL) {
            printf("MemPtr: file %s not found.\n", name);
            exit(1);
        }
    }

    MemPtr(const MemPtr& other) : m_refs(other.m_refs), m_file(other.m_file)
    {
        if (m_refs != NULL) {
            ++*m_refs;
        }
    }

    ~MemPtr() { Release(); }

    MemPtr& operator=(const MemPtr& other)
    {
        if (m_file.data != other.m_file.data) {
            Release();
            m_file = other.m_file;
            m_refs = other.m_refs;
            if (m_refs != NULL) {
                ++*m_refs;
            }
        }
        return *this;
    }

    T* Data() const { return (T*)m_file.data; }
    T& operator[](int i) const { return Data()[i]; }
    /* in bytes; none without a file */
    u_long Size() const { return m_refs != NULL ? m_file.size : 0; }

private:
    void Release()
    {
        if (m_refs != NULL && --*m_refs == 0) {
            FileSys::DeleteFile(m_file);
            delete m_refs;
        }
    }

    int* m_refs;
    FileData m_file;
};

#endif
