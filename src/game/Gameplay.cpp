/*
 * Effect::operator=(), Update() and AddEvent() are linked from the game's
 * asm: compiled from the C++ below, their stack frames are 8, 16 and 16 bytes
 * smaller than the game's (which never uses the extra bytes). That is all
 * that differs in Update(). In Effect::operator=() and AddEvent() a few
 * registers of the inlined Interval::IsOver() differ too, as in
 * Glue::Update(); AddEvent() also builds its Part in other registers, and
 * tests for a single road in both FirstRoad() and SecondRoad() where the game
 * tests once (one instruction more).
 * -DNON_MATCHING builds their C++ instead. The normal build still compiles
 * Update() and AddEvent(), as UpdateSource() and AddEventSource() in a
 * section the link drops, so that their strings and the template instances
 * they bring in land where the game has them.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Gameplay", D_8001D5C0);

#include "Format.h"

#include "MemorySys.h"

#include <string>
#include <deque>
#include <algorithm>

#include "Gameplay.h"
#include "AudioStream.h"
#include "AudioSys.h"
#include "GameCamera.h"
#include "Glue.h"
#include "InputSys.h"
#include "LanguageSelect.h"
#include "Message.h"
#include "Meters.h"
#include "ModelMovie.h"
#include "SceneDraw.h"
#include "ScoreMeter.h"
#include "SignalSys.h"
#include "TmdLineObj.h"
#include "VideoSys.h"

/* The speed shown in a speed course: three labels, one for each course. */
class SpeedLabels {
public:
    SpeedLabels()
    {
        m_model = LoadTmdModel("meters/spd_lbl.tmd");
        m_model->SetJitter(3.0, 1);
        for (int i = 0; i < 3; i++) {
            m_movies[i] = new ModelMovie(m_model, MemPtr<short>(s_files[i]), 0);
        }
        Select(COURSE_SPEED_1);
    }
    ~SpeedLabels()
    {
        for (int i = 0; i < 3; i++) {
            delete m_movies[i];
        }
        delete m_model;
    }

    void Select(int course)
    {
        m_current = course - COURSE_SPEED_1;
        m_movies[m_current]->Rewind();
    }
    void Draw() { m_movies[m_current]->Draw(); }

private:
    static const char* s_files[3];

    TmdModel* m_model;
    ModelMovie* m_movies[3];
    int m_current;
};

const char* SpeedLabels::s_files[3] = {
    "meters/spd_lbl1.anm",
    "meters/spd_lbl2.anm",
    "meters/spd_lbl3.anm",
};

/* "Round 1" or "Round 2", at the start of a track of the game's own courses. */
class RoundSign {
public:
    RoundSign() : m_camera(MemPtr<short>("message/round.anc"), 0)
    {
        m_model = LoadTmdModel("message/round.tmd");
        m_model->SetJitter(3.0, 1);
        for (int i = 0; i < 2; i++) {
            m_movies[i] = new ModelMovie(m_model, MemPtr<short>(s_files[i]), 0);
        }
        Select(0);
    }
    ~RoundSign()
    {
        for (int i = 0; i < 2; i++) {
            delete m_movies[i];
        }
        delete m_model;
    }

    void Select(int round)
    {
#line 117 "C:/psj/dev/locale/game/Gameplay.cpp"
        assert(round < 2);
#line 115 "src/game/Gameplay.cpp"
        m_round = round;
        m_movies[round]->Rewind();
        m_camera.Rewind();
    }
    void Draw()
    {
        m_camera.Update(0);
        m_movies[m_round]->Draw();
    }

private:
    static const char* s_files[2];

    TmdModel* m_model;
    ModelMovie* m_movies[2];
    int m_round;
    CameraMovie m_camera;
};

const char* RoundSign::s_files[2] = {
    "message/round01.anm",
    "message/round02.anm",
};

/* how far before or past Vibri a part can be pressed for (as Part::position) */
static Fixed s_window(0.2);
/* how far into its move Vibri takes the next buttons */
static Fixed s_moveDone(0.5);

Fixed Gameplay::s_beatPeriod;
Score Gameplay::s_score;
SpeedRange Gameplay::s_range;
Random Gameplay::s_random;
Fixed Gameplay::s_maxSpeed;
u_short Gameplay::s_roadButtonMask = BUTTON_L1 | BUTTON_UP | BUTTON_R1 | BUTTON_TRIANGLE;
deque<Part> Gameplay::s_parts;
Interval Gameplay::s_start;
Interval Gameplay::s_end;
Interval Gameplay::s_quiet;
Interval Gameplay::s_fade;
Fixed Gameplay::s_distance;
Fixed Gameplay::s_position;
Fixed Gameplay::s_speed;
Effect Gameplay::s_effect;
int Gameplay::s_formBonuses[4] = { 0, 1, 2, 3 };

float Gameplay::s_beatsPerSecond;
u_short Gameplay::s_quitButtons;
RoundSign* Gameplay::s_roundSign;
SpeedLabels* Gameplay::s_speedLabels;
AnimatedModel* Gameplay::s_demoModel;
AnimatedModel* Gameplay::s_autoModel;
CGameCamera* Gameplay::s_camera;
int Gameplay::s_courseSeconds;
int Gameplay::s_timeUp;
int Gameplay::s_seconds;
int Gameplay::s_cameraBreak;
int Gameplay::s_noSelect;
int Gameplay::s_videoDelay;
int Gameplay::s_streamDelay;
int Gameplay::s_lastPartTime;
int Gameplay::s_trackStart;
int Gameplay::s_autoPlay;
int Gameplay::s_demoHits;
int Gameplay::s_demoTracks;
int Gameplay::s_demo;
int Gameplay::s_title;
int Gameplay::s_speedCourse;
int Gameplay::s_homeCourse;
int Gameplay::s_auto;
TimeMeter* Gameplay::s_timeMeter;
SpeedMeter* Gameplay::s_speedMeter;
int Gameplay::s_gameOver;
int Gameplay::s_newPart;
int Gameplay::s_beatTime;
int Gameplay::s_combo;
int Gameplay::s_misses;
int Gameplay::s_level;
u_short Gameplay::s_roadButtons[NUM_ROADS];
int Gameplay::s_unused[16];

inline void RoadPieces::Add(RoadPiece piece)
{
    push_back(piece);
}

/*
 * The pad the game is played with. Unused: its call, turning "Play" into a
 * string, puts out the template instances of string and its allocator here,
 * ahead of the deque ones that Begin() brings in.
 */
static inline Pad* PlayPad()
{
    return InputSys::GetPad("Play");
}

/* the buttons of each road (a double road: both of its roads') */
static const struct {
    int road;
    u_short buttons;
} s_roadButtonTable[NUM_ROADS] = {
    { ROAD_BLOCK, BUTTON_L1 },
    { ROAD_PIT, BUTTON_UP },
    { ROAD_LOOP, BUTTON_R1 },
    { ROAD_WAVE, BUTTON_TRIANGLE },
    { 4, BUTTON_L1 | BUTTON_UP },
    { 5, BUTTON_L1 | BUTTON_R1 },
    { 6, BUTTON_L1 | BUTTON_TRIANGLE },
    { 7, BUTTON_UP | BUTTON_R1 },
    { 8, BUTTON_UP | BUTTON_TRIANGLE },
    { 9, BUTTON_R1 | BUTTON_TRIANGLE },
};

/* a road's button, and the buttons that press it: L2, R2, down and cross too */
struct OtherButtons {
    u_short button;
    u_short others;
};
static const OtherButtons s_otherButtons[4] = {
    { BUTTON_L1, BUTTON_L1 | BUTTON_L2 },
    { BUTTON_R1, BUTTON_R1 | BUTTON_R2 },
    { BUTTON_UP, BUTTON_UP | BUTTON_DOWN },
    { BUTTON_TRIANGLE, BUTTON_TRIANGLE | BUTTON_CROSS },
};

/* (Glue: extern const CourseInfo g_courses[NUM_COURSES];) */
const CourseInfo g_courses[NUM_COURSES] = {
    { 2, 0, 1, TRACKS_ALL, 0, -99.0, -99.0, -99, 1, { 6000, 4000, 2000 } },
    { 2, 0, 3, TRACKS_ALL, 0, -99.0, -99.0, -99, 1, { 6000, 4000, 2000 } },
    { 2, 0, 5, TRACKS_ALL, 0, -99.0, -99.0, -99, 1, { 5500, 3500, 1500 } },
    { 1, 0, TRACKS_ALL, TRACK_CHOSEN, 0, -99.0, -99.0, -99, 0, { 6500, 4500, 2500 } },
    { TRACKS_ALL, 0, TRACKS_ALL, 1, 1, -99.0, -99.0, -99, 0, { 50000, 35000, 20000 } },
    { 1, 0, TRACKS_ALL, TRACK_CHOSEN, 0, 1.5, 3.5, 210, 0, { 50, 60, 70 } },
    { 1, 0, TRACKS_ALL, TRACK_CHOSEN, 0, 1.5, 5.0, 300, 0, { 55, 65, 75 } },
    { 1, 0, TRACKS_ALL, TRACK_CHOSEN, 0, 1.5, 6.2, 372, 0, { 65, 75, 85 } },
    { TRACKS_ALL, 1, TRACKS_ALL, 1, 0, -99.0, -99.0, -99, 0, { 0, 0, 0 } },
};

/* the bonus at the end: a percentage of the points, by Vibri's form */
static const int s_rankBonuses[4] = { 30, 20, 15, 0 };

/* the first road of a part (its own, or the first of its two) */
static inline int FirstRoad(int road)
{
    if ((u_int)road < NUM_SINGLE_ROADS) {
        return road;
    }
    return FirstOfTwo(road);
}

/* the second road of a part (its own, or the second of its two) */
static inline int SecondRoad(int road)
{
    if ((u_int)road < NUM_SINGLE_ROADS) {
        return road;
    }
    return SecondOfTwo(road);
}

int Gameplay::Volume(int volume)
{
    if (Glue::GetSoundEffects()) {
        const Fixed quietest = 0.1;
        Fixed loudness = Glue::GetSoundLevel() * (Fixed(1.0) - quietest) + quietest;
        return (loudness * volume).Integer();
    }
    return 0;
}

void Gameplay::PlayRoadSound(int road, int pressed, int result)
{
    if (pressed) {
        int number;
        switch (result) {
        case Score::PRESS_SAME:
            number = 31;
            break;
        case Score::PRESS_OTHER:
            number = 28;
            break;
        case Score::PRESS_OTHER_DOUBLE:
            number = 29;
            break;
        case Score::PRESS_NONE:
        default:
            number = 27;
            break;
        }
        Sound sound(number);
        AudioSys::PlaySound(&sound, 3, Volume(100), -1);
    } else {
        Sound sound(road + 16);
        AudioSys::PlaySound(&sound, 3, Volume(127), -1);
    }
    SceneDraw::SetMove(road, pressed);
}

#ifdef NON_MATCHING
Effect& Effect::operator=(const Effect& effect)
{
    event = effect.event;
    if (!set) {
        set = effect.set;
        start = effect.start;
        end = effect.end;
    } else {
        set = effect.set;
        if (end.IsOver(effect.start.to)) {
            start = effect.start;
        }
        if (end.IsOver(effect.end.to)) {
            end = effect.end;
        }
    }
    return *this;
}
#else
INCLUDE_ASM("asm/main_g/nonmatchings/Gameplay", __as__6EffectRC6Effect);
#endif

void Gameplay::ResetEffect()
{
    s_effect = Effect();
}

void Gameplay::Begin(u_int course, int seconds, int score, int title, Interval start, int index, int demo)
{
    s_start = start;
    s_demoTracks = 2;
    s_end = Interval();
    s_quiet = Interval();
    s_fade = Interval();
    s_level = 3;
    s_title = title;
    s_courseSeconds = seconds;
    s_demoHits = 0;
    s_combo = 0;
    s_misses = 0;
    s_beatTime = 0;
    s_trackStart = start.from;
    s_lastPartTime = start.from;
    s_demo = demo;
    const CourseInfo* info = &g_courses[course];
    s_beatPeriod = 1.0 / s_beatsPerSecond;
    s_newPart = 0;
    s_gameOver = 0;
    s_parts.clear();
    s_score.Reset(score);
    s_auto = info->autoPlay;
    s_autoPlay = info->autoPlay || demo;
    s_homeCourse = course < COURSE_ONE_TRACK;
    if (s_homeCourse) {
        s_roundSign->Select(index);
    }
    s_speedCourse = course - COURSE_SPEED_1 < 3;
    s_maxSpeed = info->maxSpeed;
    if (s_speedCourse) {
        s_range = SpeedRange(info->minSpeed, info->maxSpeed, info->points, 0);
        s_speedMeter->SetTarget(s_maxSpeed);
        s_speed = s_range.m_speed;
        s_speedLabels->Select(course);
    } else {
        s_speed = s_beatsPerSecond;
    }
    s_streamDelay = AudioStream::Delay() / AudioStream::SampleRate();
    s_videoDelay = ((VideoSys::FramePeriod() * VideoSys::NumBuffers()).Raw() >> 4) * AudioStream::SampleRate() >> 12;
    s_noSelect = 0;
    s_cameraBreak = 0;
    s_seconds = 0;
    s_timeUp = 0;
    s_camera->Reset(s_homeCourse, s_speedCourse);
    SceneDraw::Reset(s_speedCourse);
    SignalSys::ClearFixedCue();
    SignalSys::SetSelect();
    InputSys::GetPad("Play")->SetVibration(0);
    ResetEffect();
    if (demo) {
        s_quitButtons = LanguageSelect::ConfirmButton() | LanguageSelect::CancelButton();
    } else {
        s_quitButtons = BUTTON_START;
    }
}

void Gameplay::Init(float distance, float position, float beatsPerSecond)
{
    SceneDraw::Init(distance, position);
    s_distance = distance;
    s_position = position;
    s_beatsPerSecond = beatsPerSecond;
    for (int i = 0; i < NUM_ROADS; i++) {
        int road = s_roadButtonTable[i].road;
        s_roadButtons[road] = s_roadButtonTable[i].buttons;
    }
    s_speedMeter = new SpeedMeter(g_courses[COURSE_SPEED_3].maxSpeed, g_courses[COURSE_SPEED_3].minSpeed);
    s_timeMeter = new TimeMeter;
    s_autoModel = new AnimatedModel("message/auto.tmd", "message/auto.anm", "message/auto.anc", 1, 3.0);
    s_demoModel = new AnimatedModel("message/demo.tmd", "message/demo.anm", "message/demo.anc", 1, 3.0);
    s_speedLabels = new SpeedLabels;
    s_roundSign = new RoundSign;
    s_camera = new CGameCamera;
    ScoreMeter::Init();
    ShowTitle();
}

void Gameplay::Quit()
{
    ScoreMeter::Free();
    delete s_camera;
    delete s_roundSign;
    delete s_speedLabels;
    delete s_demoModel;
    delete s_autoModel;
    delete s_timeMeter;
    delete s_speedMeter;
    SceneDraw::Quit();
    s_parts.clear();
}

void Gameplay::ShowTitle()
{
    AudioSys::StopSound(4);
    Begin(COURSE_BRONZE, 1, 0, 1, Interval(), 0, 0);
}

void Gameplay::Start(u_int course, int seconds, int score, Interval start, int index, int demo)
{
    Begin(course, seconds, score, 0, start, index, demo);
    Sound sound(0, 11);
    AudioSys::PlaySound(&sound, 4, 63, -1);
}

/* Where each part is, from its time. */
struct Gameplay::PlacePart {
    int m_start;
    int m_now;
    Fixed m_speed; /* (per 64 samples) 0: each part's own */

    PlacePart(int start, int now, Fixed speed) : m_start(start), m_now(now), m_speed(speed) {}
    void operator()(Part& part) const
    {
        Fixed speed = m_speed;
        int now = m_now;
        int time = part.time - now;
        part.position = (speed.Raw() != 0 ? speed : part.rate) * (time >> 6);
    }
};

/* The parts from `m_limit` on. */
struct Gameplay::PartAhead {
    Fixed m_limit;

    PartAhead(Fixed limit) : m_limit(limit) {}
    int operator()(const Part& part) const { return part.position >= m_limit; }
};

/* The parts around Vibri. */
struct Gameplay::Nearest {
    Part* m_current;  /* at Vibri, to be pressed for */
    Part* m_passed;   /* just gone past */
    Part* m_next;     /* the first one still ahead */
    int m_nextTime;
    Part* m_pending;  /* the first one not hit or missed */
    int m_pendingTime;
    int m_start;

    Nearest(int start) : m_current(NULL), m_passed(NULL), m_next(NULL), m_pending(NULL), m_start(start) {}
    /* `time` comes before `other` */
    int Before(int time, int other) const { return (u_int)(time - m_start) < (u_int)(other - m_start); }
    void operator()(Part& part)
    {
        if (m_current == NULL) {
            if (part.position >= -s_window && part.position < s_window) {
                m_current = &part;
            }
        }
        if (m_passed == NULL) {
            if (Fixed::Int(-1) < part.position && Fixed() >= part.position) {
                m_passed = &part;
            }
        }
        if (!(part.position < Fixed::Int(-1))) {
            if (m_next == NULL || Before(part.time, m_nextTime)) {
                m_next = &part;
                m_nextTime = part.time;
            }
        }
        if (!part.done) {
            if (m_pending == NULL || Before(part.time, m_pendingTime)) {
                m_pending = &part;
                m_pendingTime = part.time;
            }
        }
    }
};

inline void Gameplay::Hit(Part& part)
{
    part.done = 1;
    PlayRoadSound(part.road, 0, 0);
    int form = s_level / 3;
    if (!s_auto && !s_speedCourse && form != 0) {
        if (++s_combo == 18) {
            Sound sound(1, 14);
            AudioSys::PlaySound(&sound, 4, Volume(127), -1);
            form--;
            s_combo = 0;
            s_level = form * 3;
            s_misses = 0;
            SceneDraw::PlayEvo();
        } else {
            SceneDraw::ShowEvo(s_combo);
        }
    }
    /* the best form scores double */
    int times = form == 0 ? 2 : 1;
    s_score.Hit(part.road, times);
    /* the part's points, without the chain or the doubling */
    s_range.Add(s_score.m_lastBase + s_score.m_lastBonus);
}

inline void Gameplay::Miss(Part& part)
{
    part.done = 1;
    part.missed = 1;
    Sound sound(1, 10);
    AudioSys::PlaySound(&sound, 2, Volume(127), -1);
    s_combo = 0;
    SceneDraw::ShowEvo(0);
    int form = s_level / 3;
    if (form == 0) {
        s_misses = 0;
        s_level = (form + 1) * 3;
    } else if (++s_misses == 3) {
        s_misses = 0;
        if (s_level < 11) {
            s_level++;
        } else {
            s_gameOver = 1;
        }
    }
    s_score.m_numPresses = 0;
    s_score.m_chain = 0;
}

/* The parts played by the game itself (auto courses and the demo), and the ones missed. */
struct Gameplay::AutoPlay {
    void operator()(Part& part) const
    {
        if (s_autoPlay && !part.done && !(Fixed() < part.position)) {
            if (s_demo) {
                if (s_demoHits == 0) {
                    s_demoHits = s_random.Below(14) + 5;
                }
                if (--s_demoHits != 0) {
                    Hit(part);
                } else {
                    if (!s_auto) {
                        Miss(part);
                    }
                    PlayRoadSound(part.other, 1, 0);
                }
            } else {
                Hit(part);
            }
        }
        if (part.position < -s_window && !part.done) {
            if (!s_auto) {
                Miss(part);
            }
        }
    }
};

#ifdef NON_MATCHING
void Gameplay::Update(int position)
#else
INCLUDE_ASM("asm/main_g/nonmatchings/Gameplay", Update__8Gameplayi);
void Gameplay::UpdateSource(int position)
#endif
{
    if (s_title) {
        return;
    }
    Pad* pad = InputSys::GetPad("Play");
    u_short pressed = Buttons(pad->m_buttons.pressed).Bits();
    if (s_quitButtons & pressed) {
        Glue::SignalUserAbort();
    }
    if (s_demo) {
        pressed = 0;
    }
    for (int i = 0; i < 4; i++) {
        const OtherButtons* other = &s_otherButtons[i];
        if (other->others & pressed) {
            pressed |= other->button;
        }
    }
    pressed &= s_roadButtonMask;
    /* in a speed course, all the parts come as fast */
    Fixed speed = s_speedCourse ? s_speed / (AudioStream::SampleRate() >> 6) : Fixed();
    for_each(s_parts.begin(), s_parts.end(), PlacePart(s_trackStart, position, speed));
    sort(s_parts.begin(), s_parts.end());
    deque<Part>::iterator behind = find_if(s_parts.begin(), s_parts.end(), PartAhead(Fixed::Int(-3) - s_position));
    while (s_parts.begin() != behind) {
        s_parts.pop_front();
    }
    Nearest nearest = for_each(s_parts.begin(), s_parts.end(), Nearest(s_trackStart));
    Part* passed = nearest.m_passed;
    if (pressed != 0 && SceneDraw::IsMoveDone(s_moveDone)) {
        Part* part = nearest.m_current;
        if (part != NULL) {
            if (!part->done) {
                int buttons = part->pressed | pressed;
                part->pressed = buttons;
                if (part->pressed == s_roadButtons[part->road]) {
                    Hit(*part);
                } else if ((part->pressed & ~s_roadButtons[part->road]) && !s_auto) {
                    Miss(*part);
                }
            }
        } else {
            for (int road = 0; road < NUM_ROADS; road++) {
                if (s_roadButtons[road] & pressed) {
                    int result = 0;
                    if (nearest.m_pending != NULL) {
                        result = s_score.Press(road, nearest.m_pending->road);
                    }
                    PlayRoadSound(road, 1, result);
                    break;
                }
            }
        }
    }
    for_each(s_parts.begin(), s_parts.end(), AutoPlay());
    if (passed == NULL && nearest.m_current == NULL) {
        if (s_end.IsOver(position)) {
            if (s_speedCourse) {
                s_timeUp = 1;
                Glue::SignalGameOver();
            } else {
                Glue::SignalGameClear();
            }
        } else if (s_gameOver) {
            Glue::SignalGameOver();
        }
    }
    int seconds = (position - s_start.to) / AudioStream::SampleRate();
    if (!s_speedCourse) {
        if (!s_homeCourse) {
            int noSelect = s_noSelect;
            int second = (seconds + s_streamDelay) % 60;
            s_noSelect = second >= 52;
            if (s_noSelect != noSelect) {
                if (s_noSelect) {
                    SignalSys::ClearSelect();
                } else {
                    SignalSys::SetSelect();
                }
            }
            int cameraBreak = s_cameraBreak;
            second = seconds % 60;
            s_cameraBreak = second >= 52;
            if (s_cameraBreak != cameraBreak && s_cameraBreak) {
                s_camera->Break();
            }
        } else if (s_quiet.IsOver(position)) {
            s_quiet = Interval();
            s_camera->Break();
        }
    }
    if (s_fade.set && s_end.set) {
        int length = s_end.to - s_fade.to;
        int time = position - s_fade.to;
        if (time > 0 && time <= length) {
            int volume = ((length - time) << 8) / length;
            AudioStream::SetVolume(volume, volume);
        }
    }
    if (s_speedCourse) {
        if (s_speed < s_range.m_speed) {
            s_speed += s_speed * Fixed(0.002);
        }
    } else if (nearest.m_next != NULL) {
        s_speed = nearest.m_next->rate * (AudioStream::SampleRate() >> 6);
    }
    if (s_speedCourse) {
        int period = (AudioStream::SampleRate() << 16) / s_range.m_speed.Raw();
        SignalSys::SetFixedCue(period, period * 3);
        if (passed == NULL && nearest.m_current == NULL && s_speed >= s_maxSpeed) {
            s_seconds = seconds;
            Glue::SignalGameClear();
        }
    }
    RoadPieces road;
    deque<Part>::iterator end = find_if(s_parts.begin(), s_parts.end(), PartAhead(s_distance - s_position + 3.0));
    for (deque<Part>::iterator it = s_parts.begin(); it < end; ++it) {
        RoadPiece piece = it->Piece(s_position);
        road.Add(piece);
    }
    if (nearest.m_current != NULL && nearest.m_current->time != s_lastPartTime) {
        s_newPart = 1;
        s_lastPartTime = nearest.m_current->time;
    }
    int passedDone = nearest.m_passed != NULL && passed->done;
    int near = nearest.m_current != NULL;
    int hit = passedDone && !passed->missed;
    int missed = passedDone && passed->missed;
    pad->SetVibration(Glue::GetVibration() && !s_autoPlay ? missed : 0);
    if (s_newPart && missed) {
        s_newPart = 0;
        s_beatTime = position;
        s_beatPeriod = passed->rate;
    }
    /* after a miss, Vibri shakes, less and less */
    Fixed shake = Fixed::Int(1) - s_beatPeriod * ((position - s_beatTime) >> 6);
    if (shake < Fixed() || Fixed::Int(1) < shake) {
        shake = Fixed();
    }
    int form = s_level / 3;
    int stage = s_level % 3;
    Fixed combo = Fixed::Int(s_combo) / 18;
    SceneDraw::Update();
    SceneDraw::Draw(s_speed, &road, near || hit, shake, form, stage, s_camera);
    if (s_auto) {
        AnimatedModel* message = s_autoModel;
        message->m_camera.Update(0);
        message->m_movie.Draw();
    }
    if (s_demo) {
        AnimatedModel* message = s_demoModel;
        message->m_camera.Update(0);
        message->m_movie.Draw();
    }
    VideoSys::Print("\n");
    if (s_speedCourse) {
        s_speedMeter->Draw(s_range.m_speed);
        s_speedLabels->Draw();
    } else if (!s_auto) {
        ScoreMeter::Draw(min(s_score.m_points, (int)ScoreMeter::MAX_SCORE), combo * combo * Fixed(9.0) + Fixed(1.0));
    }
    if (!s_auto) {
        s_timeMeter->Draw(Fixed::Int(1) - Fixed::Int(seconds) / s_courseSeconds);
    }
    if (s_homeCourse && !s_demo) {
        s_roundSign->Draw();
    }
}

#ifdef NON_MATCHING
void Gameplay::AddEvent(ScriptEvent* event)
#else
INCLUDE_ASM("asm/main_g/nonmatchings/Gameplay", AddEvent__8GameplayP11ScriptEvent);
void Gameplay::AddEventSource(ScriptEvent* event)
#endif
{
    if (s_title) {
        return;
    }
    if (event->type == ScriptEvent::PART) {
        if (!s_start.IsOver(event->time)) {
            printf("Gameplay::AddEvent() : part event rejected (before start pos)\n");
            return;
        }
        if (s_end.IsOver(event->time)) {
            return;
        }
    }
    switch (event->type) {
    case ScriptEvent::PART: {
        int scale = event->scale;
        int length = s_speedCourse ? event->length : scale * event->length >> 16;
        Fixed rate = Fixed::Int(1) / (length >> 6);
        int time = event->time - s_videoDelay;
        Part part(event->value, time, rate, s_effect.IsOn(event->time));
        int first = FirstRoad(event->value);
        int second = SecondRoad(event->value);
        for (int i = 1; i < 4; i++) {
            int other = (first + i) % 4;
            if (other != first && other != second) {
                part.other = other;
                break;
            }
        }
        s_parts.push_back(part);
        break;
    }
    case ScriptEvent::EFFECT:
        if (!s_speedCourse && event->value == ScriptEvent::SCREW) {
            s_effect = Effect(*event, s_trackStart);
        }
        break;
    case ScriptEvent::CAMERA:
        s_camera->Event(event);
        break;
    }
}

void Gameplay::EndAt(int position)
{
    s_end = Interval(s_trackStart, position);
}

void Gameplay::Quiet(int position)
{
    s_quiet = Interval(s_trackStart, position);
    if (s_demo && --s_demoTracks == 0) {
        s_fade = s_quiet;
        s_end = Interval(s_trackStart, position + AudioStream::SampleRate() * 8);
    }
}

int Gameplay::GetScore()
{
    return min(s_score.m_points, (int)ScoreMeter::MAX_SCORE);
}

int Gameplay::GetBonus()
{
    int rank = FormRank(s_level / 3);
    return s_score.m_points * s_rankBonuses[rank] / 100;
}

int Gameplay::GetRank()
{
    return s_level / 3;
}

int Gameplay::GetSeconds()
{
    return s_seconds;
}

int Gameplay::IsTimeUp()
{
    return s_timeUp;
}

u_long Gameplay::Seed(u_long seed)
{
    s_random.Set(seed);
    u_long last = seed;
    u_long next;
    for (int i = 0; i < 32; i++) {
        next = NextRandom(last);
        last = next;
    }
    return next;
}
