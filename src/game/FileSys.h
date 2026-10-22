#ifndef FILESYS_H
#define FILESYS_H

#include "common.h"

#include <libds.h>
/* <kernel.h> renames the BIOS's delete() to erase() for C, and the compiler driver takes a .cpp file for C */
#undef delete

#include "MemorySys.h"
/* only the declaration: the files that use FileSys need not compile <string> */
#include <stl_config.h>
#include <stl_string_fwd.h>

class PackedFiles;

/* A file in memory. */
struct FileData {
    u_char* data;
    u_long size;
    int allocated; /* read into memory of its own, rather than part of an archive */

    FileData() : data(NULL) {}
};

/*
 * Loads files: from the archive built into the executable, from an archive
 * loaded with LoadArchive(), or from the CD, in that order.
 *
 * A file can come in versions for different languages, named with the
 * languages they are for as a hex bit mask ("menu/04_title.tim"). The name
 * without a mask (the version for all languages) is looked for first, then
 * the version for the selected language alone, then the ones for it with
 * the languages of the masks 0xFE, 0x03 and 0xFB. A file not found is
 * looked for again and again.
 */
class FileSys {
public:
    /* `root` is the directory on the CD files are read from. */
    static void Init(const char* root);
    static void Quit();
    static void LoadArchive(const char* name);
    static FileData LoadFile(const char* name);
    static void DeleteFile(FileData file);
    /* `callback` is called while a file is read from the CD: at the start, then as more of it comes in. */
    static void SetReadCallback(void (*callback)());

private:
    static void MakeCdPath(string& path, const char* name);
    static void ReadyCallback(u_char intr, u_char* result, u_long* data);
    static void SyncCallback(u_char intr, u_char* result);
    static int ReadSectors(DslLOC* pos, int size, u_long* dest);
    static int ReadFile(const char* name, FileData* file);
    static int FindInArchive(PackedFiles* archive, string name, FileData* file);
    static void LocalizedName(string name, int languages, int digits, string& result);
    static int NextLanguages(int languages, int* tried, int wrap);

    static string s_path;
    static string s_name;
    static string s_root;
    static FileData s_archive;
    static int s_languageMasks[6]; /* by language number */

    /* The compiler lays these out in reverse order of declaration (.sdata). */
    static int s_ok;
    static int s_done;
    static int s_remaining; /* words */
    static u_long* s_dest;
    static void (*s_callback)();
    static bool s_failIfMissing; /* never set: with it, LoadFile() gives up after one round of names */
};

#endif
