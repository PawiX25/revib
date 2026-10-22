#ifndef ANALYSISSCRIPT_H
#define ANALYSISSCRIPT_H

#include "common.h"

#include "Fixed.h"
#include "RoadSelect.h"
#include "FastAllocator.h"

template <class T> class MemPtr;
class ScriptMenu;

/* A stretch of road: how long its roads last and what chooses them. */
class RoadPart : public Shared {
public:
    void* operator new(size_t size) { return FastAllocator::Allocate(size); }
    void operator delete(void* p, size_t size) { FastAllocator::Free(p, size); }

    RoadPart(int period, int selectPeriod, RoadSelector* selector, RoadTempo* tempo)
        : m_period(period), m_selectPeriod(selectPeriod), m_selector(selector), m_tempo(tempo)
    {
    }
    ~RoadPart()
    {
        if (m_selector != NULL) {
            m_selector->Release();
        }
        if (m_tempo != NULL) {
            m_tempo->Release();
        }
    }

    int m_unused;
    int m_period;             /* in samples */
    int m_selectPeriod;       /* in samples */
    RoadSelector* m_selector; /* NULL: no parts come */
    RoadTempo* m_tempo;
};

/* What the analysis file starts with: how many of each thing it holds. */
struct ScriptHeader {
    int numSpeedLists;
    int numPartLists;
    int numEffectLists;
    int numTracks;
    int numPatterns;
    int numDistributions;
    int numRounds;  /* rounds with their own track, in the fixed order */
    int numSongs;   /* songs with their own track, in any order */
    int numMedleys; /* tracks of all the songs one after the other */
};

/* A list of the file: its count, then its entries in order of time. */
template <class T>
struct ScriptList {
    int count;
    T entries[1];
};

/* How long the roads last from `time` on, changing by the slopes (1/4096 a sample). */
struct ScriptSpeed {
    int time;
    int period;
    int selectPeriod;
    int periodSlope;
    int selectPeriodSlope;
};

/* What chooses the roads from `time` on. */
struct ScriptPart {
    enum Type { NONE, PATTERN, DISTRIBUTION };

    int time;
    int unused;
    Type type;
    int index; /* of the pattern or the distribution */
};

/* The effects starting at `time` (none when not positive), and a camera (-1: none). */
struct ScriptEffect {
    int time;
    int tempoLength;
    int tempoPattern; /* a RoadTempo::Pattern */
    int reverseLength;
    int screwLength;
    int camera;
};

/* The lists of a track. */
struct ScriptTrack {
    int speeds;
    int parts;
    int effects;
};

typedef ScriptList<ScriptSpeed> SpeedList;
typedef ScriptList<ScriptPart> PartList;
typedef ScriptList<ScriptEffect> EffectList;

/*
 * The tracks' script, worked out beforehand from the analysis of the
 * game's own songs: the road parts that come at each time of the song
 * played, and the effects and cameras sent to the game. A debug menu
 * ("Script" controller) can send them by hand instead.
 */
class AnalysisScript {
public:
    static void Load(const char* name);
    static void Free();
    static void UpdateMenu();
    static void DrawMenu();

    /* the song played in round `round` (`homeDisc` is not used) */
    static int SongOf(int homeDisc, int round);
    /* starts a game of `count` rounds (at most 100): in the fixed order, or shuffled by `seed` */
    static void Prepare(int fixedOrder, int count, u_long seed);
    /* round `round` starts at sound position `startTime` */
    static void Start(int round, int startTime);
    /* the road part at `time`, its periods scaled by `speed`; without `select`, it has no selector (no parts) */
    static RoadPart* NextPart(int time, int select, Fixed speed);
    /* the same with given periods */
    static RoadPart* NextPart(int time, int period, int selectPeriod, int select);

private:
    friend class ScriptMenu;

    static void FreeSelectors();
    static void FindCurrent();
    static void SendTempo(const ScriptEffect* effect);
    static void SendReverse(const ScriptEffect* effect);
    static void SendScrew(const ScriptEffect* effect);
    static void SendCamera(const ScriptEffect* effect);
    /* the effects still going on at the start of the round, and its last camera */
    static void CatchUp();
    static void SendEffects();
    /* of a CD track, in samples */
    static int TrackSamples(int track);
    static void SetTrackLists();

    static MemPtr<ScriptHeader>* s_file;
    static ScriptMenu* s_script;
    static u_long s_seed;
    static int s_round;

    /* The compiler lays these out in reverse order of declaration (.sdata). */
    static int s_speedIndex;
    static SpeedList* s_speeds;
    static int s_effectIndex;
    static EffectList* s_effects;
    static int s_partIndex;
    static PartList* s_parts;
    static int s_end;       /* of the track's script */
    static int s_now;       /* in the track */
    static int s_offset;    /* where the round starts in the track */
    static int s_startTime; /* the sound position at the start of the round */
    static int s_playing;
    static int s_track;
    static int s_fixedOrder;
    static RoadTempo* s_tempo;
    static RoadDraw** s_distributionSelectors;
    static RoadSequence** s_patternSelectors;
    static ScriptTrack* s_tracks;
    static EffectList** s_effectLists;
    static PartList** s_partLists;
    static SpeedList** s_speedLists;
    static RoadList** s_patterns;
    static RoadOdds* s_distributions;
    static int* s_medleyTracks;
    static int* s_songTracks;
    static int* s_roundTracks;
    static ScriptHeader* s_header;

    /* (.data, reverse order too) */
    static int s_starts[100]; /* of each song in a medley */
    static int s_order[100];  /* the songs of the rounds */
};

#endif
