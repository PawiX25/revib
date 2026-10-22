#ifndef AUDIOSTREAM_H
#define AUDIOSTREAM_H

#include "common.h"

#include "CdSys.h"
#include "FbStream.h"

/* Set when the track playing ends (CdSys::SetTrackEndReceiver()). */
struct TrackEnd : CdReport {};

template <class T> class StaticQueue;
class AudioThread;
class SoundDelay;

/* What AudioStream tells its receiver about each block of the CD's sound. */
struct AudioEvent {
    int position;     /* the stream's position (as GetPosition() counts) when the block was captured */
    short* samples;   /* the block just captured, in mono, heard about Delay() later */
    int trackStarted; /* a track has started since the event before: */
    int track;        /* its number (of the audio tracks, from 1) */
    int trackLength;  /* and length, in seconds */
    int trackEnded;   /* the track playing has ended since the event before */
};

/*
 * The CD's sound, read back from the SPU's capture buffers as it plays and
 * played again, mixed to mono and about `delay` milliseconds late (8 s in
 * the game), through the reverb work area: the game sees each block of
 * sound before it is heard.
 */
class AudioStream {
public:
    /* `delay` in milliseconds */
    static void Init(int delay);
    static void Quit();
    static void Start();
    static void Stop();
    /* of the sound played again, 0..256 */
    static void SetVolume(int left, int right);

    static void SetReceiver(StaticQueue<AudioEvent>* queue);
    static void ClearReceiver(StaticQueue<AudioEvent>* queue);

    /* of the sound heard, in samples */
    static int GetPosition();
    /* how loud the sound heard is, 0..100 */
    static int GetLevel();
    /* samples in a second, of the blocks of the events */
    static int SampleRate() { return s_rate; }
    /* samples in a block of the events */
    static int BlockSize() { return s_blockSize; }
    /* how long the blocks are held back before they are played again, in samples */
    static int Delay() { return s_delayBlocks * s_blockSize; }

private:
    friend class AudioThread;

    static void OnTransfers(CSpuFeedbackStream::Transfers* transfers, void* data);
    static int Log2(int n);

    enum {
        STACK_SIZE = 1024,
        CAPTURE_SIZE = 0x600 /* bytes, from the left channel's half just filled to the end of the right one's */
    };

    static StaticQueue<AudioEvent>* s_receiver;
    static int s_initialized;
    static TrackStartInfo s_trackStart;
    static TrackEnd s_trackEnd;
    static int s_rate;
    static int s_blockSize;
    static int s_blockShift;
    static int s_delayBlocks;
    static u_char* s_stack;
    static AudioThread* s_thread;
    static CSpuFeedbackStream* s_stream;
    static SoundDelay* s_delay;
    static int s_level;
    static u_char s_capture[CAPTURE_SIZE];
};

#endif
