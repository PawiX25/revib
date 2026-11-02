/*
 * The start of an executable in the port: what the PSY-Q start-up code and
 * the loader (loader.cpp) did. The page keeps the loader's LoaderData from
 * one executable to the next (port_loader_data()), and runs the one each
 * names next.
 *
 * Built with the game's sources (and their GAME or KIOSK), so that
 * LoaderData is the game's.
 */
#include "common.h"

#include <new>
#include <string.h>
#include <libds.h>

#include "GameParams.h"
#include "../runtime/psx.h"

extern "C" void InitHeap(unsigned long* heap, unsigned long size);

/* the game's main(), renamed by the build */
int port_game_main(int argc, char** argv);

/*
 * malloc3()'s heap, which only MemorySys::malloc() uses (new goes to the
 * C library's malloc in the port)
 */
static unsigned long s_heap[(1 << 20) / sizeof(unsigned long)];

static union {
    char bytes[sizeof(LoaderData)];
    double align;
} s_loader;
static int s_loaderRestored = 0;

/* before the game's static constructors (which may allocate from the heap) */
__attribute__((constructor(101))) static void PortEarly()
{
    gpu_init();
    spu_init();
    InitHeap(s_heap, sizeof(s_heap));
}

/* the loader's constructor (in loader.cpp, which the port does not build) */
LoaderData::LoaderData() {}

extern "C" {

void* port_loader_data(void)
{
    return s_loader.bytes;
}

int port_loader_data_size(void)
{
    return sizeof(LoaderData);
}

/* the page has put back the data of the executable that ran before */
void port_loader_restored(void)
{
    s_loaderRestored = 1;
}

/* the archive linked into the executable: in the executable's file on the disc */
void* port_builtin_archive(void)
{
    static u_char* s_archive = NULL;
    if (s_archive != NULL) {
        return s_archive;
    }
#if defined(GAME)
    const char* name = "\\MAIN_G.EXE;1";
    u_long address = 0x8001E408;
#elif defined(KIOSK)
    const char* name = "\\MAIN_K.EXE;1";
    u_long address = 0x8001A0C8;
#else
    const char* name = "\\MAIN_T.EXE;1";
    u_long address = 0x8001A2BC;
#endif
    /* the executables' text starts at 0x80019000, after their 2 KB header */
    u_long offset = 0x800 + (address - 0x80019000);
    DslFILE file;
    if (DsSearchFile(&file, (char*)name) == NULL) {
        printf("port: %s not found\n", name);
        static u_long s_empty[2] = { 0, 0 };
        return s_empty;
    }
    int lba = (btoi(file.pos.minute) * 60 + btoi(file.pos.second)) * 75 + btoi(file.pos.sector) - 150;
    u_long size = file.size - offset;
    s_archive = new u_char[(size + 2048 + 3) & ~3];
    u_char sector[2048];
    for (u_long done = 0; done < size;) {
        u_long at = offset + done;
        js_data_read(lba + at / 2048, sector);
        u_long in = 2048 - at % 2048;
        if (in > size - done) {
            in = size - done;
        }
        memcpy(s_archive + done, sector + at % 2048, in);
        done += in;
    }
    return s_archive;
}

} /* extern "C" */

int main()
{
    LoaderData* data = (LoaderData*)s_loader.bytes;
    if (!s_loaderRestored) {
        /* as the loader starts: the title first */
        new (data) LoaderData();
        data->next = EXECUTABLE_TITLE;
        data->params.game.saved = 0;
        data->params.firstRun = 1;
        data->params.demoCount = 0;
    }
    int status = port_game_main(LOADER_ARGC, (char**)data);
    /*
     * The page is told here, not by the runtime's exit: after the game's
     * audio thread (a fiber, Thread.cpp), the last return from main() goes
     * through the fibers' code, and the runtime never sees it.
     */
    js_main_done(status);
    return status;
}
