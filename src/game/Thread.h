#ifndef THREAD_H
#define THREAD_H

#include "common.h"

/*
 * A kernel thread running Run(). The threads are switched to and from
 * explicitly: Resume() makes an interrupt return into the thread, which
 * gives the CPU back with Return(). At most MAX_THREADS exist at a time.
 */
class Thread {
public:
    enum { MAX_THREADS = 4 };

    /* `stack`: the top of the thread's stack */
    Thread(u_long stack);
    virtual ~Thread();

    /* The interrupt being handled returns into the thread. */
    void Resume();
    /* Back to the thread Resume() was called from. */
    void Return();

protected:
    virtual long Run() = 0;

private:
    static long Entry();
    void Exit();

    long m_handle;          /* -1 once the thread has ended */
    long m_resumedFrom;     /* the thread to return to */

    static Thread* s_threads[MAX_THREADS];
};

#endif
