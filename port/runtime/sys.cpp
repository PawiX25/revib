/*
 * The console's time and its kernel: vertical blanks, interrupts and
 * critical sections, the heap (malloc3), threads, and the symbols the
 * linker and the start-up code gave the game.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <emscripten.h>
#include <emscripten/fiber.h>

#include <sys/types.h>
extern "C" {
#include <libgte.h>
}

#include "psx.h"

extern "C" {

/* --- time --- */

volatile long Vcount = 0;

static void (*s_vsyncCallback)(void) = NULL;
static int s_critical = 0;
static double s_nextVblank = 0;

/* the PAL console's vertical blanks: 49.76 a second, while the SPU plays 44100 samples */
#define VBLANK_HZ 49.76
#define VBLANK_MS (1000.0 / VBLANK_HZ)
#define SAMPLES_PER_VBLANK 886 /* (and a quarter: see SamplesThisVblank()) */

static double s_sampleDebt = 0;

/* the samples of this vertical blank: 44100 / 49.76, on average */
static int SamplesThisVblank(void)
{
    s_sampleDebt += 44100.0 / VBLANK_HZ;
    int n = (int)s_sampleDebt;
    s_sampleDebt -= n;
    return n;
}

/* the interrupts held back by a critical section */
static void (*s_pending[16])(void);
static int s_numPending = 0;

void sys_interrupt(void (*handler)(void))
{
    if (handler == NULL) {
        return;
    }
    if (s_critical) {
        if (s_numPending < 16) {
            s_pending[s_numPending++] = handler;
        }
        return;
    }
    handler();
}

static void RunPending(void)
{
    while (s_numPending > 0 && !s_critical) {
        void (*handler)(void) = s_pending[0];
        memmove(s_pending, s_pending + 1, --s_numPending * sizeof(s_pending[0]));
        handler();
    }
}

static int s_frames = 0;

void sys_vblank(void)
{
    /* the frame on display, the sound of the frame */
    gpu_present();
    spu_run(SamplesThisVblank());
    Vcount++;
    cd_vsync();
    pad_vsync();
    sys_interrupt(s_vsyncCallback);

    s_frames++;
    if (js_fast()) {
        if (s_frames % 50 == 0) {
            emscripten_sleep(0);
        }
        return;
    }
    while (js_paused()) {
        emscripten_sleep(50);
        s_nextVblank = 0;
    }
    double now = js_now();
    if (s_nextVblank == 0 || now - s_nextVblank > 250) {
        s_nextVblank = now; /* (late by far: start again from now) */
    }
    s_nextVblank += VBLANK_MS;
    double wait = s_nextVblank - now;
    /* the sound sets the pace: between 4 and 8 frames of it queued */
    int queued = js_audio_queued();
    if (queued < 4 * SAMPLES_PER_VBLANK) {
        wait = 0;
        s_nextVblank = now;
    } else if (queued > 8 * SAMPLES_PER_VBLANK) {
        wait += 2;
        s_nextVblank += 2;
    }
    emscripten_sleep(wait > 0 ? (unsigned)wait : 0);
}

static int s_idleSamples = 0;

void sys_idle(void)
{
    /* busy loops wait for the SPU's interrupts: a little sound goes by */
    spu_run(16);
    s_idleSamples += 16;
    if (s_idleSamples >= SAMPLES_PER_VBLANK) {
        /* a whole frame spent waiting: the display and the vertical blank too */
        s_idleSamples = 0;
        gpu_present();
        Vcount++;
        cd_vsync();
        pad_vsync();
        sys_interrupt(s_vsyncCallback);
        emscripten_sleep(0);
    }
}

/* libetc */
int VSync(int mode)
{
    if (mode < 0) {
        return Vcount;
    }
    if (mode == 1) {
        return 0; /* (the time since the last one, in horizontal blanks) */
    }
    int count = mode == 0 ? 1 : mode;
    for (int i = 0; i < count; i++) {
        sys_vblank();
    }
    return 0;
}

int VSyncCallback(void (*f)(void))
{
    s_vsyncCallback = f;
    return 0;
}

int ResetCallback(void)
{
    s_vsyncCallback = NULL;
    return 0;
}

int StopCallback(void)
{
    return 0;
}

/* libapi */
long EnterCriticalSection(void)
{
    s_critical++;
    return 1;
}

void ExitCriticalSection(void)
{
    if (s_critical > 0) {
        s_critical--;
    }
    if (!s_critical) {
        RunPending();
        sys_idle();
    }
}

void SwEnterCriticalSection(void)
{
    s_critical++;
}

void SwExitCriticalSection(void)
{
    if (s_critical > 0) {
        s_critical--;
    }
    if (!s_critical) {
        RunPending();
    }
}

void FlushCache(void) {}

unsigned long GetGp(void)
{
    return 0;
}

/* the host's files (the debugger's PC file server): there are none */
int PCopen(char* name, int flags, int perms) { return -1; }
int PCcreat(char* name, int perms) { return -1; }
int PClseek(int fd, int offset, int mode) { return -1; }
int PCread(int fd, char* buff, int len) { return -1; }
int PCwrite(int fd, char* buff, int len) { return -1; }
int PCclose(int fd) { return -1; }

/* --- what the linker and the start-up code give the game (MemorySys::Info() prints them) --- */

int __textlen, __rdatalen, __datalen, __sdatalen, __bsslen, __sbsslen, __ctorslen, __dtorslen;
unsigned long __heapsize = 0;

/* --- malloc3(): first fit over a free list of HeapBlock (MemorySys.h), in 8-byte units --- */

struct HeapBlock {
    HeapBlock* next;
    unsigned long size; /* in 8-byte units, this header included; 0 ends the list */
};

static HeapBlock* s_heapHead;
static HeapBlock s_heapEnd = { NULL, 0 };

void InitHeap3(unsigned long* heap, unsigned long size)
{
    /* the list starts with a block of one unit that is never given out, then the free memory */
    HeapBlock* head = (HeapBlock*)heap;
    HeapBlock* rest = head + 1;
    rest->next = &s_heapEnd;
    rest->size = size / 8 - 1;
    head->next = rest;
    head->size = 1;
    s_heapHead = head;
    __heapsize = size;
}

void* malloc3(size_t bytes)
{
    unsigned long units = (bytes + sizeof(HeapBlock) + 7) / 8;
    HeapBlock* prev = s_heapHead;
    for (HeapBlock* block = prev->next; block->size != 0; prev = block, block = block->next) {
        if (block->size >= units) {
            if (block->size == units) {
                prev->next = block->next;
            } else {
                block->size -= units;
                block += block->size;
                block->size = units;
            }
            block->next = NULL;
            return block + 1;
        }
    }
    return NULL;
}

void free3(void* p)
{
    if (p == NULL) {
        return;
    }
    HeapBlock* block = (HeapBlock*)p - 1;
    /* kept in address order, joined to its neighbours */
    HeapBlock* prev = s_heapHead;
    while (prev->next->size != 0 && prev->next < block) {
        prev = prev->next;
    }
    HeapBlock* next = prev->next;
    if (next->size != 0 && block + block->size == next) {
        block->size += next->size;
        block->next = next->next;
    } else {
        block->next = next;
    }
    if (prev != s_heapHead && prev + prev->size == block) {
        prev->size += block->size;
        prev->next = block->next;
    } else {
        prev->next = block;
    }
}

/* --- threads: fibers (Thread.cpp's port) --- */

#define MAX_FIBERS 4
#define FIBER_STACK (64 * 1024)

struct Fiber {
    emscripten_fiber_t fiber;
    void (*entry)(void);
    int used;
    char* cStack;
    char* asyncifyStack;
};

static Fiber s_fibers[MAX_FIBERS + 1]; /* [0]: the main one */
static int s_current = 0;
static char s_mainAsyncifyStack[FIBER_STACK];

static void FiberEntry(void* arg)
{
    Fiber* fiber = (Fiber*)arg;
    fiber->entry();
    /* (a thread's entry never returns: Thread::Entry() ends by switching away) */
    abort();
}

int port_thread_open(void (*entry)(void))
{
    if (!s_fibers[0].used) {
        s_fibers[0].used = 1;
        emscripten_fiber_init_from_current_context(&s_fibers[0].fiber, s_mainAsyncifyStack, sizeof(s_mainAsyncifyStack));
    }
    for (int i = 1; i <= MAX_FIBERS; i++) {
        Fiber* f = &s_fibers[i];
        if (!f->used) {
            f->used = 1;
            f->entry = entry;
            f->cStack = (char*)malloc(FIBER_STACK);
            f->asyncifyStack = (char*)malloc(FIBER_STACK);
            emscripten_fiber_init(&f->fiber, FiberEntry, f, f->cStack, FIBER_STACK, f->asyncifyStack, FIBER_STACK);
            return i;
        }
    }
    return -1;
}

void port_thread_close(int handle)
{
    if (handle <= 0 || handle > MAX_FIBERS || !s_fibers[handle].used) {
        return;
    }
    Fiber* f = &s_fibers[handle];
    free(f->cStack);
    free(f->asyncifyStack);
    memset(f, 0, sizeof(*f));
}

int port_thread_current(void)
{
    return s_current;
}

void port_thread_switch(int handle)
{
    if (handle == s_current || handle < 0 || handle > MAX_FIBERS || !s_fibers[handle].used) {
        return;
    }
    int from = s_current;
    s_current = handle;
    emscripten_fiber_swap(&s_fibers[from].fiber, &s_fibers[handle].fiber);
}

/* --- the menus, for the page's mouse and keyboard (Menu.h) --- */

int port_menu_screen = -1;

void port_menu_report(int rows, int columns, int x, int y, int active, int wrap)
{
    js_menu(port_menu_screen, rows, columns, x, y, active, wrap);
}

/* --- where the models' objects are drawn, for the mouse (TmdLineObj.h) --- */

#define MAX_MODELS 64
static const void* s_models[MAX_MODELS];
static int s_numModels;
static int s_object = -1, s_objectModel;
static int s_x0, s_y0, s_x1, s_y1;

static int ModelId(const void* model)
{
    for (int i = 0; i < s_numModels; i++) {
        if (s_models[i] == model) {
            return i;
        }
    }
    return -1;
}

void port_model_name(const void* model, const char* name)
{
    int id = ModelId(model);
    if (id < 0) {
        /* (a model freed and another loaded at its address takes the slot again) */
        id = s_numModels < MAX_MODELS ? s_numModels++ : MAX_MODELS - 1;
        s_models[id] = model;
    }
    js_model(id, name);
}

void port_object_begin(const void* model, int object)
{
    s_objectModel = ModelId(model);
    s_object = s_objectModel >= 0 ? object : -1;
    s_x0 = s_y0 = 0x7FFF;
    s_x1 = s_y1 = -0x8000;
}

void port_object_points(const DVECTOR* screen, const u_short* depth, int count)
{
    if (s_object < 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (depth[i] == 0) {
            continue;
        }
        if (screen[i].vx < s_x0) s_x0 = screen[i].vx;
        if (screen[i].vx > s_x1) s_x1 = screen[i].vx;
        if (screen[i].vy < s_y0) s_y0 = screen[i].vy;
        if (screen[i].vy > s_y1) s_y1 = screen[i].vy;
    }
}

void port_object_end(void)
{
    if (s_object >= 0 && s_x0 <= s_x1) {
        js_object(s_objectModel, s_object, s_x0, s_y0, s_x1, s_y1);
    }
    s_object = -1;
}

/* --- printf --- */

/* libmath's sprintf(), which prints floating point numbers too */
int sprintf2(char* s, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int n = vsprintf(s, format, args);
    va_end(args);
    return n;
}

void sys_printf(const char* format, ...)
{
    char text[512];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    js_log(text);
}

} /* extern "C" */
