#include "common.h"

#include <libapi.h>
#include <libspu.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "FbStream.h"

/* libspu's start of each reverb type's work area (in 8-byte units) */
extern "C" long _spu_rev_startaddr[];

/* the reverb work area: the last 512 bytes of SPU memory */
#define WORK_AREA 0x7FE00
/*
 * the branch with which SpuSetTransferStartAddr() rejects an address outside
 * 0x1010-0x7FFF8, such as the capture buffers'
 */
#define RANGE_CHECK 0x1440000B

/* Messages of a debugging build. */
static void Warn(const char* message)
{
}

static long* s_offStartAddr = &_spu_rev_startaddr[SPU_REV_MODE_OFF];
static u_long* s_rangeCheck = (u_long*)SpuSetTransferStartAddr + 6;
static CSpuFeedbackStream* s_instance = NULL;

/* the instruction read back after each change to it (as in the original; nothing uses it) */
static u_long s_patched;
/*
 * SpuSetTransferStartAddr() called uncached (KSEG1), so the patched
 * instruction is not taken from the instruction cache
 */
static u_long (*s_setTransferStartAddr)(u_long addr);

CSpuFeedbackStream::CSpuFeedbackStream()
{
    m_position = -30 * 44100;
    if (s_instance != NULL) {
        Warn("multiple CSpuFeedbackStream instances");
    }
    m_depthLeft = m_depthRight = 0x7FFF;
    m_callbackData = NULL;
    m_callback = NULL;
    m_muted = 1;
    m_silent = 0;
    m_running = 0;
    m_step = 0;
    m_workArea = WORK_AREA;
    s_instance = this;
    m_savedStartAddr = *s_offStartAddr;
    *s_offStartAddr = WORK_AREA / 8;
    if (*s_rangeCheck != RANGE_CHECK) {
        Warn("cannot modify SpuSetTransferStartAddr()");
    } else {
        *s_rangeCheck = 0; /* nop */
        s_patched = *s_rangeCheck;
    }
    s_setTransferStartAddr = (u_long (*)(u_long))((u_long)SpuSetTransferStartAddr | 0xA0000000);
#line 121 "C:/psj/dev/locale/game/FbStream.cpp"
    assert(s_setTransferStartAddr(0x100) == 0x100);
#line 66 "src/game/FbStream.cpp"
    SpuReserveReverbWorkArea(SPU_ON);
    SpuSetReverbModeDelayTime(0);
    SpuSetReverbModeFeedback(0);
    m_irqAddr = 0x400;
    SpuSetIRQCallback(IrqCallback);
}

CSpuFeedbackStream::~CSpuFeedbackStream()
{
    Stop();
    *s_offStartAddr = m_savedStartAddr;
    SpuReserveReverbWorkArea(SPU_OFF);
    if (*s_rangeCheck == 0) {
        *s_rangeCheck = RANGE_CHECK;
        s_patched = *s_rangeCheck;
    }
    s_instance = NULL;
}

void CSpuFeedbackStream::SetCallback(Callback callback, void* data)
{
    m_callbackData = data;
    m_callback = callback;
}

void CSpuFeedbackStream::ReverbTypeCallback()
{
    SpuSetReverbModeType(s_instance->m_reverbType);
    SpuSetIRQ(SPU_OFF);
    s_instance->m_typeSet = 1;
}

/* Sets the reverb type when the SPU reaches `irqAddr`. */
void CSpuFeedbackStream::SetReverbTypeAt(u_long irqAddr, long type)
{
    SpuIRQCallbackProc callback = SpuSetIRQCallback(ReverbTypeCallback);
    m_reverbType = type;
    m_typeSet = 0;
    SpuSetIRQAddr(irqAddr);
    SpuSetIRQ(SPU_ON);
    while (!m_typeSet) {
    }
    SpuSetIRQCallback(callback);
}

void CSpuFeedbackStream::Start()
{
    if (m_running) {
        return;
    }
    m_running = 1;
    m_transfers.blockFirst = 0;
    m_transfers.blockKBytes = 0;
    m_transfers.block = NULL;
    for (int i = 0; i < NUM_CAPTURES; i++) {
        m_transfers.captures[i] = NULL;
    }
    m_transfers.workAreaData = NULL;
    SetReverbTypeAt(m_irqAddr, SPU_REV_MODE_OFF);
    SpuSetTransferStartAddr(m_workArea);
    SpuWrite0(0x200);
    m_muted = 0;
    UpdateDepth();
    m_stopping = 0;
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
    m_savedTransferCallback = SpuSetTransferCallback(TransferCallback);
    SpuSetIRQAddr(m_irqAddr);
    SpuSetIRQ(SPU_ON);
}

void CSpuFeedbackStream::Stop()
{
    if (!m_running) {
        return;
    }
    m_muted = 1;
    UpdateDepth();
    EnterCriticalSection();
    m_stopping = 1;
    ExitCriticalSection();
    int stopped = 0;
    do {
        EnterCriticalSection();
        if (!m_stopping) {
            stopped = 1;
        }
        ExitCriticalSection();
    } while (!stopped);
    SpuSetTransferCallback(m_savedTransferCallback);
    m_running = 0;
}

void CSpuFeedbackStream::IrqCallback()
{
    SpuSetIRQ(SPU_OFF);
    if (s_instance->m_step != 0) {
        Warn("overrun");
    }
    s_instance->Step();
}

void CSpuFeedbackStream::TransferCallback()
{
    s_instance->Step();
}

/* The next transfer, until one has been started (its end comes back here). */
void CSpuFeedbackStream::Step()
{
    int started = 0;
#line 313 "C:/psj/dev/locale/game/FbStream.cpp"
    assert(m_running);
#line 179 "src/game/FbStream.cpp"
    do {
        m_step = (m_step + 1) % 8;
        switch (m_step) {
        case 1:
            m_half = SpuGetIRQAddr() == m_irqAddr;
            m_position += HALF_SIZE / 4;
            if (m_transfers.blockKBytes != 0 && m_transfers.block != NULL) {
                s_setTransferStartAddr((m_transfers.blockFirst * 2 + m_half) * HALF_SIZE);
                SpuRead(m_transfers.block, m_transfers.blockKBytes * 2 * HALF_SIZE - HALF_SIZE);
                started = 1;
                m_step += NUM_CAPTURES;
            }
            break;
        case 2:
        case 3:
        case 4:
        case 5: {
            int i = m_step - 2;
            if (m_transfers.captures[i] != NULL) {
                started = 1;
                s_setTransferStartAddr((i * 2 + m_half) * HALF_SIZE);
                SpuRead(m_transfers.captures[i], HALF_SIZE);
            }
            break;
        }
        case 6:
            if (m_transfers.workAreaData != NULL) {
                started = 1;
                s_setTransferStartAddr(m_workArea + m_half * (HALF_SIZE / 2));
                SpuWrite(m_transfers.workAreaData, HALF_SIZE / 2);
            }
            break;
        case 7:
            if (m_callback != NULL) {
                m_transfers.position = m_position;
                m_callback(&m_transfers, m_callbackData);
            }
            break;
        case 0:
            if (!m_stopping) {
                SpuSetIRQAddr(m_irqAddr + m_half * HALF_SIZE);
                SpuSetIRQ(SPU_ON);
            } else {
                m_stopping = 0;
            }
            started = 1;
            break;
        }
    } while (!started);
}

void CSpuFeedbackStream::SetDepth(short left, short right)
{
    m_depthLeft = left;
    m_depthRight = right;
    UpdateDepth();
}

void CSpuFeedbackStream::UpdateDepth()
{
    if (m_silent || m_muted) {
        SpuSetReverbModeDepth(0, 0);
    } else {
        SpuSetReverbModeDepth(m_depthLeft, m_depthRight);
    }
}

int CSpuFeedbackStream::GetPosition()
{
    SwEnterCriticalSection();
    int position = m_position - 2 * (HALF_SIZE / 4);
    SwExitCriticalSection();
    return position;
}
