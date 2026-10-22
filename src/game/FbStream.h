#ifndef FBSTREAM_H
#define FBSTREAM_H

#include "common.h"

/*
 * Streams what the SPU writes back into its memory, the capture buffers at
 * its start (the CD's left and right channels, voices 1 and 3, 1 KB each):
 * each time the SPU has filled half of them (an SPU interrupt), the halves
 * just filled are read as the callback asks, while the SPU fills the other
 * ones, and the callback's sound is written into the reverb work area, 256
 * bytes at a time, for the reverb to play. For this, reverb type 0's work
 * area is moved to the last 512 bytes of SPU memory, and
 * SpuSetTransferStartAddr() is patched to take the addresses of the capture
 * buffers. Only one should exist at a time: a second one only gets a warning
 * (in a debugging build).
 */
class CSpuFeedbackStream {
public:
    enum { NUM_CAPTURES = 4, HALF_SIZE = 0x200 };

    /* What is transferred after each interrupt; the callback sets it for the next one. */
    struct Transfers {
        u_char* captures[NUM_CAPTURES]; /* the half just filled of each capture buffer is read here (NULL: not read) */
        int blockFirst;                 /* or, in one read instead, from this capture buffer's half just filled on, */
        int blockKBytes;                /* this many KB less 512 bytes (0: no such read) */
        u_char* block;                  /* are read here (NULL: no such read) */
        u_char* workAreaData;           /* 256 bytes written into the same half of the reverb work area (NULL: none) */
        int position;                   /* samples streamed, up to the halves just filled (see GetPosition()) */
    };

    /* called after the transfers, with what has been read */
    typedef void (*Callback)(Transfers* transfers, void* data);

    CSpuFeedbackStream();
    ~CSpuFeedbackStream();

    void SetCallback(Callback callback, void* data);
    void Start();
    void Stop();
    void SetDepth(short left, short right);
    /*
     * The samples streamed, at 22050 a second (128 for each half buffer of
     * 256), counting from -60 s, less 256 (two halves) for the sound written
     * into the work area to be heard.
     */
    int GetPosition();

private:
    static void ReverbTypeCallback();
    static void IrqCallback();
    static void TransferCallback();

    void SetReverbTypeAt(u_long irqAddr, long type);
    void Step();
    void UpdateDepth();

    int m_half;             /* the half of the buffers the SPU has just filled */
    void* m_callbackData;
    Callback m_callback;
    int m_step;             /* of the transfers after each interrupt */
    volatile int m_typeSet;
    long m_reverbType;
    u_long m_workArea;      /* SPU address of the reverb work area */
    u_long m_irqAddr;       /* the SPU interrupts there (the right channel's capture buffer) and half a buffer on */
    short m_depthLeft;
    short m_depthRight;
    int m_muted;
    int m_silent;
    int m_running;
    long m_savedStartAddr;  /* reverb type 0's work area before ours */
    void (*m_savedTransferCallback)();
    Transfers m_transfers;
    volatile int m_stopping;
    int m_position;         /* samples streamed (see GetPosition()), up to the halves last filled */
};

#endif
