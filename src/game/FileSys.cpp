#include "common.h"

#include <libds.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "FileSys.h"
#include <string>
#include "Format.h"
#include "PackedFiles.h"
#include "LanguageSelect.h"

#define SECTOR_WORDS (2048 / 4)

/*
 * The archive linked into the executable (the files of the loading screen,
 * and MAKEPAK.PIF).
 * The linker gives its address doubled, without the KSEG0 bit.
 */
extern "C" char __builtin_archive_x2[];

static inline PackedFiles* BuiltinArchive()
{
    return (PackedFiles*)(((u_long)__builtin_archive_x2 >> 1) | 0x80000000);
}

bool FileSys::s_failIfMissing = false;

/* "dir/name" -> "\ROOT\DIR\NAME;1" */
void FileSys::MakeCdPath(string& path, const char* name)
{
    path = "\\";
    path += s_root;
    path += name;
    path += ";1";
    for (u_int i = 0; i < path.size(); i++) {
        if (path[i] >= 'a' && path[i] <= 'z') {
            path[i] -= 'a' - 'A';
        } else if (path[i] == '/') {
            path[i] = '\\';
        }
    }
}

void FileSys::ReadyCallback(u_char intr, u_char* result, u_long* data)
{
    if (intr == DslDataReady) {
        int words = s_remaining;
        if (words > SECTOR_WORDS) {
            words = SECTOR_WORDS;
        }
        DsGetSector(s_dest, words);
        s_remaining -= words;
        s_dest += words;
        if (s_remaining == 0) {
            DsEndReadySystem();
            s_done = 1;
        }
    } else {
        printf("Sector read failed\n");
        s_ok = 0;
    }
}

void FileSys::SyncCallback(u_char intr, u_char* result)
{
    if (intr == DslComplete) {
        DsStartReadySystem(ReadyCallback, -1);
    } else {
        printf("Read failed\n");
        s_ok = 0;
    }
}

int FileSys::ReadSectors(DslLOC* pos, int size, u_long* dest)
{
    static u_char mode = DslModeSpeed | DslModeSize1;

    s_dest = dest;
    s_remaining = (size + 3) / 4;
    s_done = 0;
    s_ok = 1;
    DsCommand(DslSetmode, &mode, NULL, -1);
    DsCommand(DslReadN, (u_char*)pos, SyncCallback, -1);

    int reported = -1;
    while (s_ok && !s_done) {
        if (s_callback != NULL && s_remaining != reported) {
            reported = s_remaining;
            s_callback();
        }
    }
    return s_ok;
}

int FileSys::ReadFile(const char* name, FileData* file)
{
    MakeCdPath(s_path, name);
    DslFILE info;
    DslFILE* found = DsSearchFile(&info, (char*)s_path.c_str());
    if (found == NULL) {
        return 0;
    }
    if (found == (DslFILE*)-1) {
        printf("search error: %s\n", name);
        return 0;
    }
    file->allocated = 1;
    file->size = info.size;
    file->data = new u_char[(info.size + 3) & ~3];
    int ok = 0;
    while (!ok) {
        ok = ReadSectors(&info.pos, file->size, (u_long*)file->data);
        if (!ok) {
            printf("read error: %s\n", name);
        }
    }
    return 1;
}

int FileSys::FindInArchive(PackedFiles* archive, string name, FileData* file)
{
    int found;
    u_long* entry = archive->Find(name.c_str());
    if (entry != NULL) {
        file->allocated = 0;
        file->size = entry[0];
        file->data = (u_char*)(entry + 1);
        found = 1;
    } else {
        found = 0;
    }
    return found;
}

/* "dir/name" -> "dir/04_name"; no prefix for a file in all languages */
void FileSys::LocalizedName(string name, int languages, int digits, string& result)
{
    static char prefix[8];

    int all = (1 << (digits * 4)) - 1;
    result = name;
    if (languages != all) {
        sprintf(prefix, "%0*X_", digits, languages);
        string::size_type slash = result.rfind('/');
        result.insert(slash != string::npos ? slash + 1 : 0, prefix);
    }
}

/*
 * The masks of the versions of a file looked for in turn, each with the
 * selected language's bit added (0xFF: no mask).
 */
static const int s_languageGroups[] = { 0xFF, 0x00, 0xFE, 0x03, 0xFB };

int FileSys::NextLanguages(int languages, int* tried, int wrap)
{
    if (*tried >= 5) {
        if (!wrap) {
            return 0;
        }
        *tried = 0;
    }
    return languages | s_languageGroups[(*tried)++];
}

void FileSys::Init(const char* root)
{
    s_root = root;
    s_archive = FileData();
    s_callback = NULL;
    /* the bits files are tagged with, by language number */
    s_languageMasks[5] = 1 << 0;
    s_languageMasks[1] = 1 << 1;
    s_languageMasks[0] = 1 << 2;
    s_languageMasks[2] = 1 << 3;
    s_languageMasks[3] = 1 << 4;
    s_languageMasks[4] = 1 << 5;
}

void FileSys::Quit()
{
    DeleteFile(s_archive);
}

void FileSys::LoadArchive(const char* name)
{
    DeleteFile(s_archive);
    s_archive = LoadFile(name);
}

FileData FileSys::LoadFile(const char* name)
{
    FileData file;
    int tried = 0;
    int languages = s_languageMasks[LanguageSelect::GetLanguage()];
    for (;;) {
        string& path = s_name;
        int group = NextLanguages(languages, &tried, !s_failIfMissing);
        if (group == 0) {
            return file; /* not found: no data */
        }
        LocalizedName(string(name), group, 2, path);
        PackedFiles* builtin = BuiltinArchive();
        if (FindInArchive(builtin, string(path), &file)) {
            return file;
        }
        PackedFiles* loaded = (PackedFiles*)s_archive.data;
        if (loaded != NULL && FindInArchive(loaded, string(path), &file)) {
            return file;
        }
        if (ReadFile(path.c_str(), &file)) {
            return file;
        }
    }
}

void FileSys::DeleteFile(FileData file)
{
    if (file.allocated) {
        delete[] file.data;
    }
}

void FileSys::SetReadCallback(void (*callback)())
{
    if (callback != NULL) {
#line 392 "C:/psj/dev/locale/game/FileSys.cpp"
        assert(s_callback == NULL);
#line 232 "src/game/FileSys.cpp"
    }
    s_callback = callback;
}

/* allocated up front */
string FileSys::s_path(64, ' ');
string FileSys::s_name(64, ' ');
string FileSys::s_root(64, ' ');
FileData FileSys::s_archive;
int FileSys::s_languageMasks[6];

int FileSys::s_ok;
int FileSys::s_done;
int FileSys::s_remaining;
u_long* FileSys::s_dest;
void (*FileSys::s_callback)();
