#include "common.h"

#include <libapi.h>
#undef delete /* kernel.h's */
#include <libcd.h>
#include <libetc.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/loader/nonmatchings/loader", D_80010000);

#include "GameParams.h"

/*
 * The loader, SCES_028.73, which the CD boots: it runs the title, the game
 * and the kiosk executables, one after the other, giving each its
 * LoaderData as argv; the one that ends says which comes next.
 */

/* the loader's memory, for the startup code: up to where the executables are loaded */
extern "C" {
unsigned long _ramsize = 0x19000;
unsigned long _stacksize = 0x100;
}

/* where the loader is loaded */
static u_long s_loaderAddress = 0x80010000;
/* for CdlSetmode: double speed */
static u_char s_cdMode = CdlModeSpeed;
/* the first sector of the executable being loaded */
static int s_exeSector = 0;
/* where the sectors are read from */
static CdlLOC s_readPos;

/* the executables, by LoaderData::next */
struct ExeFile {
    const char* file;
    int unused; /* (the loader does not read it) */
};
static const ExeFile s_executables[] = {
    { "main_t.exe", 1500 },
    { "main_g.exe", 500 },
    { "main_k.exe", 1000 },
};

/* the executable's first sector: its header */
union ExeSector {
    XF_HDR header;
    u_long data[2048 / sizeof(u_long)];
};

LoaderData g_loaderData;
/* the CD file name of the executable */
static char s_path[32] = "\\";
ExeSector g_exe;

/* the drive at double speed, if it is not */
static void SetDoubleSpeed()
{
    if (!(CdMode() & CdlModeSpeed)) {
        CdControl(CdlSetmode, &s_cdMode, 0);
        int start = VSync(-1);
        while (VSync(-1) < start + 4) {
        }
    }
}

/* `name` as a file name of the CD: "\NAME;1" */
static void MakePath(char* path, const char* name)
{
    *path = '\\';
    for (path++; *name != 0; path++) {
        char c = *name++;
        if (c >= 'a' && c <= 'z') {
            c -= 'a' - 'A';
        } else if (c == '/') {
            c = '\\';
        }
        *path = c;
    }
    *path = ';';
    path++;
    *path = '1';
    path[1] = 0;
}

static int FindFile(const char* name)
{
    CdlFILE file;
    MakePath(s_path, name);
    CdlFILE* found = CdSearchFile(&file, s_path);
    if (found == NULL) {
        return 0;
    }
    if (found == (CdlFILE*)-1) {
        return 0;
    }
    s_exeSector = CdPosToInt(&found->pos);
    return 1;
}

/* `count` sectors from sector `offset` of the executable into `buffer` */
static bool ReadSectors(void* buffer, int offset, int count)
{
    CdIntToPos(s_exeSector + offset, &s_readPos);
    if (!CdControl(CdlSetloc, (u_char*)&s_readPos, 0)) {
        return 0;
    }
    if (!CdRead(count, (u_long*)buffer, CdlModeSpeed)) {
        return 0;
    }
    return CdReadSync(0, 0) == 0;
}

/* empty in the loader */
static void StopDevices()
{
}

static void InitSystem()
{
    ResetCallback();
    CdInit();
    SetDoubleSpeed();
}

static void ResetSystem()
{
    ResetCallback();
    CdInit();
    SetDoubleSpeed();
}

static int LoadExe(const char* name)
{
    bool ok = FindFile(name);
    if (!ok) {
        printf("exe open failed\n");
    } else {
        ok &= ReadSectors(&g_exe, 0, 1);
        if (!ok) {
            printf("exe header load failed\n");
        } else {
            /* the executables are loaded right after the loader's memory */
            u_long address = s_loaderAddress + _ramsize - 0x10000;
#line 283 "C:/psj/dev/locale/game/loader.cpp"
            assert(address == g_exe.header.exec.t_addr);
#line 151 "src/game/loader.cpp"
            ok &= ReadSectors((void*)address, 1, g_exe.header.exec.t_size >> 11);
            if (!ok) {
                printf("exe body load failed\n");
            } else {
                StopDevices();
                StopCallback();
                EnterCriticalSection();
                FlushCache();
                ok &= Exec(&g_exe.header.exec, LOADER_ARGC, (char**)&g_loaderData);
                if (ok) {
                    return ok;
                }
                printf("Exec call failed\n");
            }
        }
    }
    return ok;
}

int main()
{
    printf("Loader main()\n");
    InitSystem();
    g_loaderData.next = EXECUTABLE_TITLE;
    g_loaderData.params.game.saved = 0;
    g_loaderData.params.firstRun = 1;
    g_loaderData.params.demoCount = 0;
    while (1) {
        LoadExe(s_executables[g_loaderData.next].file);
        ResetSystem();
    }
}

LoaderData::LoaderData()
{
}
