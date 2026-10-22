#ifndef SIGNALSYS_H
#define SIGNALSYS_H

#include "common.h"

#include "Fixed.h"
#include "StaticQueue.h"
#include "AudioStream.h"

class SignalAnalyzer;

/*
 * Places the script's cues in the music as it plays: the sound of
 * the CD (AudioStream's events) is looked at for each frame, and the
 * tracks starting and ending are passed on to Glue (and the starts to the
 * script).
 */
class SignalSys {
public:
    /*
     * `distance` and `speed`: Game's road distance and Vibri's position (not
     * used); `time`: the cues are looked for from then on
     */
    static void Init(float distance, float speed, int time);
    static void Quit();
    static void Update(int frame);

    /* the cues are given these periods rather than the script's (AnalysisScript::NextPart()) */
    static void SetFixedCue(int period, int selectPeriod);
    static void ClearFixedCue();
    /* without select, NextPart() gives the cues no selector (no parts) */
    static void ClearSelect();
    static void SetSelect();

    /*
     * The disc in the drive: the game's own disc keeps its tracks' order,
     * another disc gets an order picked at random from its id.
     */
    static void SetDisc(int id, int numTracks, int homeDisc);
    /* the place of the track `track` (from 1) in that order, to sort the tracks by (`homeDisc` is not used) */
    static int TrackOrder(int track, int homeDisc);

private:
    enum { QUEUE_SIZE = 32 };

    static StaticQueue<AudioEvent> s_queue;
    static int s_fixedCue;
    static int s_select; /* NextPart()'s select */
    static Fixed s_scale;
    static SignalAnalyzer* s_analyzer;
    static int s_fixedPeriod; /* the period of SetFixedCue() */
    static int s_fixedSelectPeriod; /* and the select period */
    static int s_hadSelector; /* the last cue had a selector: something to place */
};

#endif
