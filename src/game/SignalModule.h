#ifndef SIGNALMODULE_H
#define SIGNALMODULE_H

#include "common.h"

#include "Fixed.h"

class RoadPart;

/*
 * Looks at the sound, a block of samples at a time, for where to place the
 * cues it is given; it places them as events of the game
 * (Gameplay::AddEvent()).
 */
class SignalModule {
public:
    enum { BLOCK_SIZE = 128 };

    SignalModule(int blockSize);
    virtual ~SignalModule();

    /* `cue`: what is looked for now (the module takes over the reference); `time`: of the block's first sample */
    virtual void Process(RoadPart* cue, int time, short* samples) = 0;
    /* starts again, looking from `time` on */
    virtual void Reset(int time) {}

protected:
    int m_blockSize;
};

/*
 * Keeps the best place found in each window for the cue it looks for, and
 * places the cue there once the time, or the best place of a later window,
 * is more than twice the cue's length past it; before that, a later place
 * that stands out half as much again takes its place.
 */
class PeakPicker {
public:
    PeakPicker();
    virtual ~PeakPicker();

protected:
    /* places the cue (it is then let go of) */
    void Place();
    /* takes the candidate as the cue to place */
    void TakeCandidate();
    /* a window ends at `time` */
    void EndWindow(int time);
    void Clear();

    RoadPart* m_cue;          /* to be placed */
    int m_time;
    int m_level;              /* of the sound there, */
    int m_background;         /* and around it */
    Fixed m_scale;
    int m_length;             /* the cue's, scaled */
    RoadPart* m_candidate;    /* the best place found in the window so far */
    int m_candidateTime;
    int m_candidateLevel;
    int m_candidateBackground;
};

/*
 * Finds where the sound gets louder, a block at a time: the block whose
 * level over the last 4 blocks is highest compared with the level of the
 * last 32 blocks. (Not used by the game.)
 */
class BlockOnsetModule : public PeakPicker, public SignalModule {
public:
    enum { LONG_BLOCKS = 32, SHORT_BLOCKS = 4 };

    BlockOnsetModule(int time, int blockSize)
        : SignalModule(blockSize), m_windowStart(time), m_window(0), m_longIndex(0), m_shortIndex(0)
    {
        ResetHistory();
    }
    ~BlockOnsetModule();

    void Process(RoadPart* cue, int time, short* samples);
    void Reset(int time);

private:
    void ResetHistory();

    int m_windowStart;
    u_int m_window;
    int m_long[LONG_BLOCKS];
    int m_longIndex;
    int m_short[SHORT_BLOCKS];
    int m_shortIndex;
};

/*
 * Finds where the sound gets louder: the sample at which the level of the
 * last 3 blocks (384 samples) is highest compared with the level of the
 * last 31 blocks.
 */
class OnsetModule : public PeakPicker, public SignalModule {
public:
    /* the long and short sums cover one block less than these */
    enum { HISTORY = 32, RECENT = 4 };

    OnsetModule(int time, int blockSize, int rate);
    ~OnsetModule();

    void Process(RoadPart* cue, int time, short* samples);
    void Reset(int time);

private:
    void ResetHistory();

    int m_windowStart;
    u_int m_window;
    short m_silence[BLOCK_SIZE];      /* stands for the blocks not seen yet */
    short* m_history[HISTORY];        /* the last blocks */
    int m_historyIndex;
    int m_longLevel;                  /* the two running sums of the samples' squares, scaled down */
    int m_shortLevel;
};

#endif
