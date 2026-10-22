#include "common.h"

/* the allocator's "out of memory" string, of a shared header (first in this file's rodata) */
INCLUDE_RODATA("asm/main_g/nonmatchings/AudioStream", D_800196DC);

#include <libapi.h>
#undef delete /* kernel.h's */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "AudioStream.h"
#include "CdSys.h"
#include "FbStream.h"
#include "StaticQueue.h"
#include "Thread.h"

/* the blocks' levels are added up scaled down by this (a shift) */
#define LEVEL_SHIFT 14
/* how loud the sound is: the sum of the levels of about this many blocks around one */
#define LEVEL_BLOCKS 400

/* What follows the samples of a block. */
struct BlockLevel {
    int level;  /* of the block's samples */
    int window; /* the levels of about LEVEL_BLOCKS blocks around it */
};

/*
 * The sound of a stream, mixed to mono, given back `delay` milliseconds
 * later (rounded up to whole blocks), with how loud it is. A block holds
 * m_length samples, then its BlockLevel.
 */
class SoundDelay {
public:
    struct Blocks {
        short* delayed; /* given back, or silence while filling */
        short* current; /* the block just pushed */

        Blocks(short* delayed, short* current) : delayed(delayed), current(current) {}
    };

    SoundDelay(int rate, int samples, int delay, int step);
    ~SoundDelay();

    void Reset();
    /* the samples of both channels, every `step` of them */
    Blocks Push(short* left, short* right);

    short* Block(int index) const { return m_ring + index * m_size; }
    /* the block written `blocks` before the next one */
    int Ago(int blocks) const { return (m_write + (m_count - blocks)) % m_count; }

    const int m_rate;
    const int m_samples; /* per channel, in a block given to Push() */
    const int m_step;
    const int m_length;  /* samples in a block */
    const int m_size;    /* in shorts */
    const int m_delay;   /* in blocks */
    const int m_count;
    short* m_ring;
    short* m_silence;
    int m_write;
    int m_read;
    int m_filled;
    int m_level;

private:
    static int NumBlocks(int rate, int samples, int delay);
};

SoundDelay::SoundDelay(int rate, int samples, int delay, int step)
    : m_rate(rate), m_samples(samples), m_step(step), m_length(samples / step),
      m_size(m_length + sizeof(BlockLevel) / sizeof(short)),
      m_delay(NumBlocks(rate, samples, delay)), m_count(m_delay + 1)
{
    m_silence = new short[m_size];
    for (int i = 0; i < m_size; i++) {
        m_silence[i] = 0;
    }
    m_ring = new short[m_size * m_count];
#line 80 "C:/psj/dev/locale/game/AudioStream.cpp"
    assert(m_ring != NULL);
#line 85 "src/game/AudioStream.cpp"
    Reset();
}

SoundDelay::~SoundDelay()
{
    delete[] m_silence;
    delete[] m_ring;
}

/* blocks of `samples` lasting `delay` milliseconds, rounded up */
int SoundDelay::NumBlocks(int rate, int samples, int delay)
{
    return (delay * rate / 1000 + samples - 1) / samples;
}

void SoundDelay::Reset()
{
    m_filled = 0;
    m_write = 0;
    m_read = 0;
    m_level = 0;
}

/* Mixes `length` samples of two channels into `block`; their level. */
static inline int Mix(short* block, short* left, short* right, int length, int step)
{
    int level = 0;
    for (int i = 0; i < length; i++) {
        short sample = (*left + *right) / 2;
        block[i] = sample;
        if (sample >= 0) {
            level += sample;
        } else {
            level -= sample;
        }
        right += step;
        left += step;
    }
    return level;
}

SoundDelay::Blocks SoundDelay::Push(short* left, short* right)
{
    short* block = Block(m_write);
    m_write = (m_write + 1) % m_count;
    int level = Mix(block, left, right, m_length, m_step);
    ((BlockLevel*)&block[m_length])->level = level;
    m_level += level >> LEVEL_SHIFT;
    if (m_filled > LEVEL_BLOCKS) {
        m_level -= ((BlockLevel*)&Block(Ago(LEVEL_BLOCKS))[m_length])->level >> LEVEL_SHIFT;
        ((BlockLevel*)&Block(Ago(LEVEL_BLOCKS / 2))[m_length])->window = m_level;
    }
    short* delayed;
    if (m_filled == m_delay) {
        delayed = Block(m_read);
        m_read = (m_read + 1) % m_count;
    } else {
        delayed = m_silence;
        m_filled++;
    }
    return Blocks(delayed, block);
}

/* Mixes what the stream reads back, on each of its interrupts. */
class AudioThread : public Thread {
public:
    AudioThread(u_long stack) : Thread(stack), m_transfers(NULL) {}

    CSpuFeedbackStream::Transfers* m_transfers;

protected:
    long Run()
    {
        for (;;) {
            if (m_transfers->blockKBytes == 0) {
                /* the first time: read the captured sound from now on */
                m_transfers->blockKBytes = 2;
                m_transfers->blockFirst = 0;
                m_transfers->block = AudioStream::s_capture;
            } else {
                SoundDelay::Blocks blocks = AudioStream::s_delay->Push(
                    (short*)m_transfers->block, (short*)(m_transfers->block + 0x400));
                m_transfers->workAreaData = (u_char*)blocks.delayed;
                SoundDelay* delay = AudioStream::s_delay;
                AudioStream::s_level = ((BlockLevel*)&blocks.delayed[delay->m_length])->window / LEVEL_BLOCKS;
                AudioEvent event;
                event.position = m_transfers->position;
                event.samples = blocks.current;
                if (AudioStream::s_trackStart.ready) {
                    event.trackStarted = 1;
                    AudioStream::s_trackStart.ready = 0;
                    event.track = AudioStream::s_trackStart.track;
                    event.trackLength = AudioStream::s_trackStart.lengthSeconds;
                } else {
                    event.trackStarted = 0;
                    event.track = 0;
                    event.trackLength = 0;
                }
                if (AudioStream::s_trackEnd.ready) {
                    event.trackEnded = 1;
                    AudioStream::s_trackEnd.ready = 0;
                } else {
                    event.trackEnded = 0;
                }
                SwEnterCriticalSection();
                /* lost if the receiver is full */
                StaticQueue<AudioEvent>* receiver = AudioStream::s_receiver;
                if (receiver != NULL && receiver->Count() < receiver->Capacity()) {
                    receiver->Put(event);
                }
                SwExitCriticalSection();
            }
            Return();
        }
    }
};

StaticQueue<AudioEvent>* AudioStream::s_receiver = NULL;
int AudioStream::s_initialized = 0;
TrackStartInfo AudioStream::s_trackStart;
TrackEnd AudioStream::s_trackEnd;
int AudioStream::s_rate = 0;
int AudioStream::s_blockSize = 0;
int AudioStream::s_blockShift = 0;
int AudioStream::s_delayBlocks = 0;
u_char* AudioStream::s_stack = NULL;
AudioThread* AudioStream::s_thread = NULL;
CSpuFeedbackStream* AudioStream::s_stream = NULL;
SoundDelay* AudioStream::s_delay = NULL;
int AudioStream::s_level = 0;
u_char AudioStream::s_capture[CAPTURE_SIZE];

/* the stream's callback: the thread answers it */
void AudioStream::OnTransfers(CSpuFeedbackStream::Transfers* transfers, void* data)
{
    s_thread->m_transfers = transfers;
    s_thread->Resume();
}

int AudioStream::Log2(int n)
{
    for (int i = 0;; i++) {
        if (1 << i == n) {
            return i;
        }
        if (i + 1 >= 32) {
#line 285 "C:/psj/dev/locale/game/AudioStream.cpp"
            assert(0);
#line 234 "src/game/AudioStream.cpp"
        }
    }
}

void AudioStream::Init(int delay)
{
#line 294 "C:/psj/dev/locale/game/AudioStream.cpp"
    assert(!s_initialized);
#line 243 "src/game/AudioStream.cpp"
    s_stack = new u_char[STACK_SIZE];
    s_thread = new AudioThread((u_long)(s_stack + STACK_SIZE));
    s_rate = 22050;
    s_blockSize = 128;
    s_blockShift = Log2(s_blockSize);
    int step = 44100 / s_rate; /* the stream's samples to each one kept */
    s_delay = new SoundDelay(44100, 256, delay, step);
    s_delayBlocks = s_delay->m_delay;
    s_stream = new CSpuFeedbackStream;
    s_stream->SetCallback(OnTransfers, NULL);
    s_initialized = 1;
    s_level = 0;
}

void AudioStream::Quit()
{
#line 319 "C:/psj/dev/locale/game/AudioStream.cpp"
    assert(s_initialized);
#line 262 "src/game/AudioStream.cpp"
    s_initialized = 0;
    delete s_stream;
    delete s_delay;
    delete s_thread;
    delete[] s_stack;
}

void AudioStream::Start()
{
    s_trackStart.ready = 0;
    s_trackEnd.ready = 0;
    CdSys::SetTrackStartReceiver(&s_trackStart);
    CdSys::SetTrackEndReceiver(&s_trackEnd.ready);
    s_stream->Start();
}

void AudioStream::Stop()
{
    s_stream->Stop();
    CdSys::ClearTrackStartReceiver(&s_trackStart);
    CdSys::ClearTrackEndReceiver(&s_trackEnd.ready);
}

void AudioStream::SetVolume(int left, int right)
{
    int depthLeft = 0x7FFF * left / 256;
    int depthRight = 0x7FFF * right / 256;
    s_stream->SetDepth(depthLeft, depthRight);
}

void AudioStream::SetReceiver(StaticQueue<AudioEvent>* queue)
{
#line 358 "C:/psj/dev/locale/game/AudioStream.cpp"
    assert(s_receiver == NULL);
#line 297 "src/game/AudioStream.cpp"
    s_receiver = queue;
}

void AudioStream::ClearReceiver(StaticQueue<AudioEvent>* queue)
{
#line 365 "C:/psj/dev/locale/game/AudioStream.cpp"
    assert(s_receiver == queue);
#line 305 "src/game/AudioStream.cpp"
    s_receiver = NULL;
}

int AudioStream::GetPosition()
{
    int delay = s_delayBlocks << s_blockShift;
    return s_stream->GetPosition() - delay;
}

int AudioStream::GetLevel()
{
    int level = s_level * 5 / 4;
    return level > 100 ? 100 : level;
}
