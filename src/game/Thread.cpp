#include "common.h"

#include <libapi.h>
#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Thread.h"

#ifdef PORT
/*
 * The port: the runtime's fibers (port/runtime/sys.cpp). Resume() switches
 * to the thread at once, from the interrupt handler, and Return() back.
 */
extern "C" {
int port_thread_open(void (*entry)(void));
void port_thread_close(int handle);
int port_thread_current(void);
void port_thread_switch(int handle);
}

Thread* Thread::s_threads[MAX_THREADS];

static void PortEntry()
{
    for (;;) {
        Thread::PortRun();
    }
}

Thread::Thread(u_long stack)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        if (s_threads[i] == NULL) {
            s_threads[i] = this;
            break;
        }
    }
    m_handle = port_thread_open(PortEntry);
    m_resumedFrom = 0;
}

Thread::~Thread()
{
    port_thread_close(m_handle);
    for (int i = 0; i < MAX_THREADS; i++) {
        if (s_threads[i] == this) {
            s_threads[i] = NULL;
        }
    }
}

void Thread::Resume()
{
    if (m_handle == -1) {
        return;
    }
    m_resumedFrom = port_thread_current();
    port_thread_switch(m_handle);
}

void Thread::Return()
{
    port_thread_switch(m_resumedFrom);
}

/* the thread whose fiber this is: its Run() */
void Thread::PortRun()
{
    int current = port_thread_current();
    for (int i = 0; i < MAX_THREADS; i++) {
        Thread* thread = s_threads[i];
        if (thread != NULL && thread->m_handle == current) {
            thread->Run();
            thread->Exit();
            return;
        }
    }
}

void Thread::Exit()
{
    m_handle = -1;
    Return();
}

#else

/* the kernel's tables: [1] the process (its current thread), [2] the threads */
static struct ToT* s_kernelTables = (struct ToT*)0x100;
/* s_threads has not been cleared yet */
static int s_firstThread = 1;

#include "Format.h"

/* status register of a new thread: interrupts enabled once it runs */
#define THREAD_SR 0x404

static inline struct TCB* Tcb(long handle)
{
    return (struct TCB*)s_kernelTables[2].head + (handle & 0xFFFF);
}

Thread* Thread::s_threads[MAX_THREADS];

Thread::Thread(u_long stack)
{
    SwEnterCriticalSection();
    if (s_firstThread) {
        for (int i = 0; i < MAX_THREADS; i++) {
            s_threads[i] = NULL;
        }
        s_firstThread = 0;
    }
    for (int i = 0; i < MAX_THREADS; i++) {
        if (s_threads[i] == NULL) {
            s_threads[i] = this;
            break;
        }
    }
    SwExitCriticalSection();
    u_long gp = GetGp();
    SwEnterCriticalSection();
    m_handle = OpenTh(Entry, stack, gp);
    SwExitCriticalSection();
#line 67 "C:/psj/dev/locale/game/Thread.cpp"
    assert(m_handle != -1);
#line 51 "src/game/Thread.cpp"
    Tcb(m_handle)->reg[R_SR] = THREAD_SR;
}

Thread::~Thread()
{
    SwEnterCriticalSection();
    CloseTh(m_handle);
    for (int i = 0; i < MAX_THREADS; i++) {
        if (s_threads[i] == this) {
            s_threads[i] = NULL;
        }
    }
    SwExitCriticalSection();
}

void Thread::Resume()
{
    if (m_handle == -1) {
        return;
    }
    u_long* running = s_kernelTables[1].head;
    struct TCB* tcbs = (struct TCB*)s_kernelTables[2].head;
    u_long current = (*running - (u_long)tcbs) / sizeof(struct TCB);
    u_long mine = m_handle & 0xFFFF;
#line 99 "C:/psj/dev/locale/game/Thread.cpp"
    assert(current != mine);
#line 78 "src/game/Thread.cpp"
    m_resumedFrom = DescTH | current;
    *running = (u_long)(tcbs + mine);
}

void Thread::Return()
{
    ChangeTh(m_resumedFrom);
}

long Thread::Entry()
{
    SwEnterCriticalSection();
    u_long* running = s_kernelTables[1].head;
    struct TCB* tcbs = (struct TCB*)s_kernelTables[2].head;
    u_long current = (*running - (u_long)tcbs) / sizeof(struct TCB);
    Thread* thread = NULL;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (s_threads[i] != NULL && (s_threads[i]->m_handle & 0xFFFF) == current) {
            thread = s_threads[i];
        }
    }
    SwExitCriticalSection();
    if (thread != NULL) {
        thread->Run();
        thread->Exit();
        return 1;
    }
#line 142 "C:/psj/dev/locale/game/Thread.cpp"
    assert(thread != NULL);
#line 108 "src/game/Thread.cpp"
}

void Thread::Exit()
{
    SwEnterCriticalSection();
    m_handle = -1;
    SwExitCriticalSection();
    Return();
}

#endif
