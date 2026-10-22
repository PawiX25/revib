#ifndef PACKEDFILES_H
#define PACKEDFILES_H

#include "common.h"

/*
 * An archive of files, as one block in memory: the number of files, the
 * offset of each file's entry from the start of the archive (sorted by name),
 * then the entries. An entry is the file's name, padded to 4 bytes, its size
 * and its contents.
 */
class PackedFiles {
public:
    /* The size of the named file, followed by its contents; NULL if absent. */
    u_long* Find(const char* name);

private:
    char* FindEntry(const char* name);

    int m_numFiles;
    int m_offsets[1]; /* [m_numFiles] */
};

#endif
