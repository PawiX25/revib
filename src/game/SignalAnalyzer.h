#ifndef SIGNALANALYZER_H
#define SIGNALANALYZER_H

#include "common.h"

class RoadPart;
class SignalModule;
class OnsetModule;

/*
 * Looks at the sound of the CD as it plays, for the cues of the track's
 * script, with a signal module; told when tracks start and end.
 */
class SignalAnalyzer {
public:
    /* `rate`: of the sound, samples per second; `blockSize`: of the blocks given to Process() */
    SignalAnalyzer(int rate, int blockSize);
    virtual ~SignalAnalyzer();

    /* the track `track` starts at `time` */
    virtual void TrackStarted(int time, int track) {}
    virtual void TrackEnded() {}
    /* before and after the blocks of a frame */
    virtual void BeginFrame() {}
    virtual void Process(RoadPart* cue, int time, short* samples);
    virtual void EndFrame(int frame) {}

protected:
    int m_rate;
    int m_blockSize;
    SignalModule* m_module;
};

/* Places the cues where the sound gets louder. */
class OnsetAnalyzer : public SignalAnalyzer {
public:
    /* looks from `time` on (`distance` and `speed` are not used) */
    OnsetAnalyzer(int rate, int blockSize, float distance, float speed, int time);
    ~OnsetAnalyzer();

    void TrackStarted(int time, int track);
    void TrackEnded() {}

private:
    OnsetModule* m_onsets;
};

#endif
