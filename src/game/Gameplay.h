#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "common.h"

#include "MemorySys.h"
#include <deque>

#include "Fixed.h"
#include "Random.h"
#include "ScriptEvent.h"

class AnimatedModel;
class CGameCamera;
class SpeedMeter;
class TimeMeter;
class RoundSign;
class SpeedLabels;

/* A stretch of the music's time, in samples, once set. */
struct Interval {
    int set;
    int from;
    int to;

    Interval() : set(0) {}
    Interval(int from, int to) : set(1), from(from), to(to) {}

    /* `time` is at or past the end */
    int IsOver(int time) const { return set && time - to >= 0; }
};

/* The ways to play. */
enum Course {
    COURSE_BRONZE,       /* the game's own music, two tracks each */
    COURSE_SILVER,
    COURSE_GOLD,
    COURSE_ONE_TRACK,    /* an audio CD's: a track the player chooses */
    COURSE_ALL_TRACKS,   /* every track, in an order of the disc's */
    COURSE_SPEED_1,      /* played to reach a speed: the best times are the shortest */
    COURSE_SPEED_2,
    COURSE_SPEED_3,
    COURSE_IN_ORDER,     /* every track, from the first, played by the game itself */
    NUM_COURSES
};

/* in a CourseInfo's track numbers */
#define TRACKS_ALL    -99 /* numTracks: all of the disc's; a first track: none, the course does not play that disc */
#define TRACK_CHOSEN  -98 /* the player's, from the track select */

#define NUM_HIGH_SCORES 3

struct CourseInfo {
    int numTracks;
    int autoPlay;        /* the game plays the parts itself */
    int firstHomeTrack;  /* with the game's own disc */
    int firstAudioTrack; /* with an audio CD */
    int shuffle;         /* the tracks in the order SignalSys::TrackOrder() gives them */
    float minSpeed;      /* of a speed course */
    float maxSpeed;
    int points;          /* of a speed course: the points that bring the highest speed */
    int again;           /* played again from the track that was lost */
    int highScores[NUM_HIGH_SCORES]; /* the first ones */
};

/*
 * g_courses[NUM_COURSES] (Gameplay.cpp): not declared here. Its definition
 * must be the first declaration the compiler sees, so that the table is put
 * out with the other constant tables at the end of Gameplay's .rodata; Glue
 * declares it itself.
 */

/* the four roads, then the double roads (two of them at once: 4 to 9) */
enum Road { ROAD_BLOCK, ROAD_PIT, ROAD_LOOP, ROAD_WAVE, NUM_SINGLE_ROADS = 4, NUM_ROADS = 10 };

/* a double road */
inline int IsDouble(int road)
{
    return (u_int)(road - NUM_SINGLE_ROADS) < NUM_ROADS - NUM_SINGLE_ROADS;
}

/* the first of the two roads a double road is made of */
inline int FirstOfTwo(int road)
{
    if (road >= 9) {
        return ROAD_LOOP;
    }
    if (road >= 7) {
        return ROAD_PIT;
    }
    return ROAD_BLOCK;
}

/* the second one */
inline int SecondOfTwo(int road)
{
    if (road >= 9) {
        return road - 6;
    }
    if (road >= 7) {
        return road - 5;
    }
    return road - 3;
}

/* A part of the road coming, waiting for its buttons. */
struct Part {
    int road;
    int time;       /* in samples, as the sound is heard */
    Fixed rate;     /* how fast it comes, per 64 samples */
    int effect;     /* the screw effect is on at its time */
    Fixed position; /* how far ahead of Vibri it is */
    int done;
    int missed;
    u_short pressed; /* the buttons pressed for it so far */
    int other;      /* a single road that is not one of its own, pressed when the demo misses it */

    Part(int road, int time, Fixed rate, int effect)
        : road(road), time(time), rate(rate), effect(effect), position(), done(0), missed(0), pressed(0)
    {
    }

    int operator<(const Part& part) const { return position < part.position; }
    /* the piece of road drawn for it, Vibri being at `offset` */
    struct RoadPiece Piece(const Fixed& offset) const;
};

/* A piece of the road drawn: what SceneDraw is told of a part. */
struct RoadPiece {
    int object;     /* of the road model: the part's road */
    Fixed position; /* how far ahead of Vibri it is */
    int missed;     /* the part was missed: the piece jitters hard */
    int leaning;    /* leans as the camera passes it: the screw effect was on */

    RoadPiece(int object, Fixed position, int missed, int leaning)
        : object(object), position(position), missed(missed), leaning(leaning)
    {
    }
};

inline RoadPiece Part::Piece(const Fixed& offset) const
{
    return RoadPiece(road, position + offset, missed, effect);
}

/* The road drawn: the parts from behind Vibri to as far as the road is seen. */
class RoadPieces : public deque<RoadPiece> {
public:
    void Add(RoadPiece piece);
};

/* The speed a speed course has reached: from its lowest up, by the points scored. */
class SpeedRange {
public:
    SpeedRange();
    SpeedRange(Fixed min, Fixed max, int points, int scored);

    /* `points` more scored */
    void Add(int points);

    Fixed m_min;
    Fixed m_max;
    int m_points;  /* that bring the highest speed */
    Fixed m_speed;
    int m_scored;
};

/* The points, and how much the parts hit are worth. */
class Score {
public:
    /* what Press() makes of a press, and the bonus it adds */
    enum PressResult {
        PRESS_NONE = 0,         /* not counted: two were already */
        PRESS_SAME = 1,         /* a road of the part coming, or one pressed already */
        PRESS_OTHER = 2,        /* another road than the single one coming */
        PRESS_OTHER_DOUBLE = 4  /* another road than the two of the double one coming */
    };

    Score() { Reset(0); }

    void Reset(int points)
    {
        m_points = points;
        m_numPresses = 0;
        m_chain = 0;
        m_lastBase = 0;
        m_lastBonus = 0;
        m_lastChain = 0;
    }
    /* the part of `road` is hit; `times`: what its points are multiplied by */
    void Hit(int road, int times);
    /* `road` is pressed while the part of `partRoad` is coming: a PressResult */
    int Press(int road, int partRoad);

    int m_points;
    int m_numPresses; /* the presses counted before the part came (two at most) */
    int m_pressBonus; /* their PressResults added up, added to the part's points if it is hit */
    int m_chain;      /* the parts hit in a row (ten at most) */
    int m_presses[2]; /* the roads of those presses */
    int m_lastBase;   /* the points of the last part hit: its own, */
    int m_lastBonus;  /* the presses' bonus, */
    int m_lastChain;  /* and the chain's */
};

/* The effect going on (Gameplay keeps the screw ones): the last effect event and when it is on. */
struct Effect {
    int set;
    ScriptEvent event;
    Interval start;
    Interval end;

    Effect() : set(0) {}
    /* `from`: where the times are counted from */
    Effect(const ScriptEvent& event, int from) : set(1), event(event)
    {
        start = Interval(from, event.time);
        end = Interval(from, event.time + event.length);
    }
    Effect& operator=(const Effect& effect);

    /* the effect is on at `time` */
    int IsOn(const int& time) const { return set && start.IsOver(time) && !end.IsOver(time); }
};

class Gameplay {
public:
    static void Init(float distance, float position, float beatsPerSecond);
    static void Quit();
    /* no course, for the title's screens: Update() and AddEvent() do nothing */
    static void ShowTitle();
    /* `index`: of the track in the course */
    static void Start(u_int course, int seconds, int score, Interval start, int index, int demo);
    /* the music heard is at `position` (in samples) */
    static void Update(int position);
    static void AddEvent(ScriptEvent* event);
#ifndef NON_MATCHING
    /*
     * Update() and AddEvent() as written, under other names in a section the
     * link leaves out: the game's code of both is linked from asm, but these
     * still bring in their strings and template instances (see Gameplay.cpp).
     */
    static void UpdateSource(int position) __attribute__((section(".dropped")));
    static void AddEventSource(ScriptEvent* event) __attribute__((section(".dropped")));
#endif
    /* the music ends at `position` */
    static void EndAt(int position);
    /* the parts stop coming at `position` */
    static void Quiet(int position);
    static int GetScore();
    static int GetBonus();
    static int GetRank();
    static int GetSeconds();
    static int IsTimeUp();
    /* the seed for the next game */
    static u_long Seed(u_long seed);

private:
    /* `volume` for a sound of the game: the louder the music, the louder; 0 with the sound effects off */
    static int Volume(int volume);
    /*
     * `pressed` 0: the part of `road` was hit; otherwise `road` was pressed
     * without hitting a part, `result` being Score::Press()'s (0 when there
     * was none, and for the demo's misses).
     */
    static void PlayRoadSound(int road, int pressed, int result);
    static void ResetEffect();
    static void Begin(u_int course, int seconds, int score, int title, Interval start, int index, int demo);
    /* the parts that come to Vibri */
    static void Hit(Part& part);
    static void Miss(Part& part);

    struct PlacePart;
    struct PartAhead;
    struct Nearest;
    struct AutoPlay;
    friend struct PlacePart;
    friend struct Nearest;
    friend struct AutoPlay;

    /* which end bonus Vibri's form `form` gets (an index in s_rankBonuses, Gameplay.cpp) */
    static int FormRank(int form) { return s_formBonuses[form]; }

    static Fixed s_beatPeriod; /* how fast Vibri's shake after a miss dies down (per 64 samples) */
    static Score s_score;
    static SpeedRange s_range;
    static Random s_random;
    static Fixed s_maxSpeed;
    static u_short s_roadButtonMask;
    static deque<Part> s_parts;
    static Interval s_start;
    static Interval s_end;
    static Interval s_quiet;
    static Interval s_fade;
    static Effect s_effect;
    static int s_formBonuses[4];
    static Fixed s_distance; /* how far ahead the road is seen */
    static Fixed s_position; /* Vibri's place on the road */
    static Fixed s_speed;

    /* The compiler lays these out in reverse order of declaration (.sdata). */
    static float s_beatsPerSecond;
    static u_short s_quitButtons;
    static RoundSign* s_roundSign;
    static SpeedLabels* s_speedLabels;
    static AnimatedModel* s_demoModel;
    static AnimatedModel* s_autoModel;
    static CGameCamera* s_camera;
    static int s_courseSeconds;
    static int s_timeUp;
    static int s_seconds;
    static int s_cameraBreak;
    static int s_noSelect;
    static int s_videoDelay;   /* in samples */
    static int s_streamDelay;  /* in seconds */
    static int s_lastPartTime;
    static int s_trackStart;
    static int s_autoPlay;
    static int s_demoHits;     /* before the demo misses one */
    static int s_demoTracks;
    static int s_demo;
    static int s_title;
    static int s_speedCourse;
    static int s_homeCourse;
    static int s_auto;
    static TimeMeter* s_timeMeter;
    static SpeedMeter* s_speedMeter;
    static int s_gameOver;
    static int s_newPart;
    static int s_beatTime;     /* when Vibri's last shake began */
    static int s_combo;        /* parts hit in a row toward a better form (18) */
    static int s_misses;       /* since the level last changed: the third raises it */
    static int s_level;        /* Vibri's level: her form * 3, plus one for every three misses in it */

    /* (.data, reverse order too) */
    static u_short s_roadButtons[NUM_ROADS];
    static int s_unused[16];
};

#endif
