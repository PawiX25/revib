#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <libetc.h>

#include "Format.h"
#include "MemorySys.h"
#include <string>

#include "Glue.h"
#include "Random.h"
#include "AudioStream.h"
#include "CdSys.h"
#include "Gameplay.h"
#include "SignalSys.h"
#include "Title.h"
#include <algorithm>

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Glue", D_8001DDD0);

/* the courses (Gameplay.cpp) */
extern const CourseInfo g_courses[NUM_COURSES];

/* the longest a CD goes */
#define MAX_DISC_SECONDS (75 * 60)

DiscInfo Glue::s_discInfo;
ShellOpen Glue::s_shellOpen;
ShellClose Glue::s_shellClose;
NoDisc Glue::s_noDisc;
Interval Glue::s_trackStart;
Glue::CourseResult Glue::s_courseResults[NUM_COURSES] = {
    { Title::RESULT_BRONZE, COURSE_BRONZE },
    { Title::RESULT_SILVER, COURSE_SILVER },
    { Title::RESULT_GOLD, COURSE_GOLD },
    { Title::RESULT_SPEED_1, COURSE_SPEED_1 },
    { Title::RESULT_SPEED_2, COURSE_SPEED_2 },
    { Title::RESULT_SPEED_3, COURSE_SPEED_3 },
    { Title::RESULT_ALL_TRACKS, COURSE_ALL_TRACKS },
    { Title::RESULT_ONE_TRACK, COURSE_ONE_TRACK },
    { Title::RESULT_IN_ORDER, COURSE_IN_ORDER },
};
int Glue::s_comments[4] = { 3, 1, 0, 2 };

int Glue::s_seeded;
int Glue::s_leaving;
int Glue::s_step;
Glue::HighScores* Glue::s_highScores;
u_int Glue::s_course;
int Glue::s_screenShown;
int Glue::s_numScreens;
int Glue::s_done;
int* Glue::s_vibration;
int* Glue::s_soundEffects;
int Glue::s_chosenTrack;
int Glue::s_demoTrack;
int Glue::s_demoStarted;
int Glue::s_demoLevel;
int Glue::s_demo;
int Glue::s_trackSeconds;
int Glue::s_checking;
int Glue::s_discSeconds;
int Glue::s_numTracks;
int Glue::s_audioDisc;
int Glue::s_homeDisc;
int Glue::s_tracksToBuffer;
int Glue::s_track;
int Glue::s_playingTrack;
int Glue::s_tracksToPlay;
int Glue::s_score;
int Glue::s_changedState;
int Glue::s_state;
signed char Glue::s_trackOrder[MAX_TRACKS];
Title::Screen Glue::s_screens[MAX_SCREENS];

void Glue::ChooseTrack(int track)
{
    s_chosenTrack = track;
}

void Glue::SetOptions(int soundEffects, int vibration)
{
    *s_soundEffects = soundEffects;
    *s_vibration = vibration;
}

void Glue::ClearScreens()
{
    s_numScreens = 0;
    s_screenShown = 0;
}

Title::Screen Glue::TopScreen()
{
#line 106 "C:/psj/dev/locale/game/Glue.cpp"
    assert(s_numScreens > 0);
#line 101 "src/game/Glue.cpp"
    return s_screens[s_numScreens - 1];
}

void Glue::PushScreen(Title::Screen screen, int show)
{
#line 124 "C:/psj/dev/locale/game/Glue.cpp"
    assert(s_numScreens < MAX_SCREENS);
#line 109 "src/game/Glue.cpp"
    s_screens[s_numScreens++] = screen;
    if (show) {
        ShowScreen();
    }
}

void Glue::PopScreen(int show)
{
    s_numScreens--;
#line 132 "C:/psj/dev/locale/game/Glue.cpp"
    assert(s_numScreens > 0);
#line 121 "src/game/Glue.cpp"
    if (show) {
        ShowScreen();
    }
}

int Glue::IsScreenShown()
{
    return s_screenShown;
}

void Glue::ClearTrackOrder()
{
    for (int i = 0; i < MAX_TRACKS; i++) {
        s_trackOrder[i] = 0;
    }
}

/* the disc's tracks, in the order SignalSys::TrackOrder() gives them */
void Glue::ShuffleTracks(int numTracks)
{
    signed char keys[MAX_TRACKS];
#line 153 "C:/psj/dev/locale/game/Glue.cpp"
    assert(numTracks <= MAX_TRACKS);
#line 145 "src/game/Glue.cpp"
    for (int i = 0; i < numTracks; i++) {
        s_trackOrder[i] = i;
        keys[i] = SignalSys::TrackOrder(i + 1, s_homeDisc);
    }
    /* sorted by their keys */
    int sorted;
    do {
        sorted = 1;
        for (int i = 1; i < numTracks; i++) {
            if (keys[i - 1] > keys[i]) {
                swap(s_trackOrder[i - 1], s_trackOrder[i]);
                swap(keys[i - 1], keys[i]);
                sorted = 0;
            }
        }
    } while (!sorted);
}

int Glue::FirstTrack(const CourseInfo* course)
{
    return s_homeDisc ? course->firstHomeTrack : course->firstAudioTrack;
}

/* `track`: of the course */
void Glue::PlayTrack(int track)
{
    s_playingTrack = g_courses[s_course].shuffle == 1 ? s_trackOrder[track - 1] + 1 : track;
    CdSys::PlayTrack(s_playingTrack);
}

void Glue::StartTracks()
{
    const CourseInfo* course = &g_courses[s_course];
    int first = FirstTrack(course);
    int last = s_numTracks;
    switch (first) {
    case TRACKS_ALL:
#line 260 "C:/psj/dev/locale/game/Glue.cpp"
        assert(first != TRACKS_ALL);
#line 185 "src/game/Glue.cpp"
    case TRACK_CHOSEN:
        first = s_chosenTrack;
        break;
    default:
        if (s_demo) {
            first += s_demoTrack;
        }
        /* the game's own courses have two tracks */
        if (s_course < COURSE_ONE_TRACK) {
            first = min(first, last - 1);
        } else {
            first = min(first, last);
        }
        break;
    }
    int count = course->numTracks;
    if (count == TRACKS_ALL) {
        count = last;
    }
    s_tracksToBuffer = count;
    if (s_demo) {
        s_tracksToBuffer = count - s_demoTrack;
    }
    int left = last - first + 1;
    s_tracksToPlay = s_tracksToBuffer = min(s_tracksToBuffer, left);
    ShuffleTracks(last);
    s_track = first;
    PlayTrack(first);
}

void Glue::ToMainScreen()
{
    s_state = STATE_TITLE;
    s_demo = 0;
    int popped = 0;
    while (TopScreen() != Title::SCREEN_MAIN) {
        PopScreen(0);
        popped = 1;
    }
    if (popped || !IsScreenShown()) {
        ShowScreen();
    }
}

void Glue::SetHighScores(HighScores* highScores)
{
    s_highScores = highScores;
}

/*
 * The assert is always true and its call is dropped, but the space for the
 * call's arguments stays in the stack frame. Nothing of the call is left to
 * give its line in the game's file: 328 is a placeholder.
 */
void Glue::ResetHighScores()
{
    for (int course = 0; course < NUM_COURSES; course++) {
#line 328 "C:/psj/dev/locale/game/Glue.cpp"
        assert(course < NUM_COURSES);
#line 245 "src/game/Glue.cpp"
        for (int i = 0; i < NUM_HIGH_SCORES; i++) {
            s_highScores[course][i] = g_courses[course].highScores[i];
        }
    }
}

int Glue::RecordHighScore(int course, int score)
{
    int placed = 0;
    int pushed = 0; /* down by the new one */
    int rank = 0;
    for (int i = 0; i < NUM_HIGH_SCORES; i++) {
        if (placed) {
            swap(pushed, s_highScores[course][i]);
        } else if (IsSpeedCourse(course) ? score < s_highScores[course][i]
                                         : s_highScores[course][i] < score) {
            placed = 1;
            pushed = s_highScores[course][i];
            rank = i + 1;
            s_highScores[course][i] = score;
        }
    }
    return rank;
}

void Glue::Alert(Title::Screen screen)
{
    PushScreen(screen, 0);
    if (s_checking) {
        PushScreen(Title::SCREEN_CD_CHECK, 0);
    }
    ShowScreen();
    s_state = STATE_ALERT;
}

int Glue::HasDisc(Title::Screen screen)
{
    return (screen == Title::SCREEN_ALERT_VIB_CD_IN && s_homeDisc) ||
           (screen == Title::SCREEN_ALERT_AUDIO_CD_IN && s_audioDisc);
}

void Glue::CancelAlert()
{
    if (s_demo) {
        ToMainScreen();
        return;
    }
    Title::Screen alert = TopScreen();
    PopScreen(0);
    /* the CD change screen was for the disc */
    if (!HasDisc(alert) && TopScreen() == Title::SCREEN_CD_CHANGE) {
        PopScreen(0);
    }
    s_state = STATE_TITLE;
    ShowScreen();
}

/* The steps go on until one waits for the player. */
void Glue::Start(int again)
{
    int step = s_step;
    int waiting = 0;
    const CourseInfo* course = &g_courses[s_course];
    do {
        switch (step) {
        case STEP_COURSE:
            step = STEP_DISC;
            Title::SetCourse(s_course);
            break;
        case STEP_DISC:
            if (s_leaving || s_course < COURSE_ONE_TRACK) {
                step = STEP_PLAY;
                if (!s_homeDisc) {
                    step = STEP_ASK_HOME_DISC;
                }
            } else {
                step = STEP_CD_CHANGE;
                if (!s_audioDisc) {
                    step = STEP_ASK_AUDIO_CD;
                }
            }
            break;
        case STEP_ASK_HOME_DISC:
            Alert(Title::SCREEN_ALERT_VIB_CD_IN);
            waiting = 1;
            step = STEP_PLAY;
            break;
        case STEP_ASK_AUDIO_CD:
            Alert(Title::SCREEN_ALERT_AUDIO_CD_IN);
            waiting = 1;
            step = STEP_CD_CHANGE_HIDDEN;
            break;
        case STEP_CD_CHANGE:
            PushScreen(Title::SCREEN_CD_CHANGE, 1);
            waiting = 1;
            step = STEP_TRACK_SELECT;
            break;
        case STEP_ASK_AUDIO_CD_AGAIN:
            Alert(Title::SCREEN_ALERT_AUDIO_CD_IN);
            waiting = 1;
            step = STEP_TRACK_SELECT;
            break;
        case STEP_CD_CHANGE_HIDDEN:
            PushScreen(Title::SCREEN_CD_CHANGE, 0);
            step = STEP_TRACK_SELECT;
            break;
        case STEP_TRACK_SELECT:
            if (FirstTrack(course) == TRACK_CHOSEN) {
                waiting = 1;
                Title::SetNumTracks(s_numTracks);
                PushScreen(Title::SCREEN_TRACK_SELECT, 1);
            }
            step = STEP_PLAY;
            break;
        case STEP_PLAY:
            if (s_leaving) {
                s_done = 1;
                waiting = 1;
            } else {
                /* off the screens on the way to the course */
                for (;;) {
                    Title::Screen top = TopScreen();
                    if (top == Title::SCREEN_CD_CHANGE || top == Title::SCREEN_TRACK_SELECT ||
                        top == Title::SCREEN_AGAIN_SELECT) {
                        PopScreen(0);
                    } else {
                        break;
                    }
                }
                Title::GoTo(Title::SCREEN_DONE);
                s_screenShown = 0;
                s_state = STATE_STARTING;
                waiting = 1;
                if (again) {
                    PlayTrack(s_track);
                } else {
                    s_score = 0;
                    StartTracks();
                }
            }
            break;
        }
    } while (!waiting);
    s_step = step;
}

void Glue::ChooseCourse(int result)
{
    if (result == Title::RESULT_LEAVE) {
        s_leaving = 1;
    } else {
        s_leaving = 0;
        int found = 0;
        for (int i = 0; i < NUM_COURSES; i++) {
            if (s_courseResults[i].result == result) {
                s_course = s_courseResults[i].course;
                found = 1;
                break;
            }
        }
#line 533 "C:/psj/dev/locale/game/Glue.cpp"
        assert(found);
#line 408 "src/game/Glue.cpp"
    }
    s_step = STEP_COURSE;
    Start(0);
}

/* from the CD change screen */
void Glue::ChangeCd()
{
#line 541 "C:/psj/dev/locale/game/Glue.cpp"
    assert(s_step == STEP_TRACK_SELECT);
#line 419 "src/game/Glue.cpp"
    s_step = STEP_ASK_AUDIO_CD_AGAIN;
    Start(0);
}

/* back from the screen of a step */
void Glue::StepBack()
{
    switch (s_step) {
    case STEP_CD_CHANGE:
        break;
    case STEP_TRACK_SELECT:
        s_step = STEP_CD_CHANGE;
        break;
    case STEP_PLAY:
        s_step = STEP_TRACK_SELECT;
        break;
    }
}

void Glue::HandleResult(int result)
{
    switch (result) {
    case Title::RESULT_AGAIN:
        Start(g_courses[s_course].again);
        break;
    case Title::RESULT_OPTIONS:
        PushScreen(Title::SCREEN_OPTION, 1);
        break;
    case Title::RESULT_PSJ_MODES:
        PushScreen(Title::SCREEN_PSJ_MODE_SELECT, 1);
        break;
    case Title::RESULT_UNKNOWN_MODES:
        PushScreen(Title::SCREEN_UNKNOWN_MODE_SELECT, 1);
        break;
    case Title::RESULT_SPEED_MODES:
        PushScreen(Title::SCREEN_MODE_SPEED_SELECT, 1);
        break;
    case Title::RESULT_PSJ_HISCORES:
        PushScreen(Title::SCREEN_PSJ_HISCORE, 1);
        break;
    case Title::RESULT_UNKNOWN_HISCORES:
        PushScreen(Title::SCREEN_UNKNOWN_HISCORE, 1);
        break;
    case Title::RESULT_PSJ_BACK:
    case Title::RESULT_UNKNOWN_BACK:
    case Title::RESULT_SPEED_BACK:
    case Title::RESULT_CD_CHANGE_BACK:
    case Title::RESULT_TRACK_BACK:
    case Title::RESULT_OPTION_BACK:
    case Title::RESULT_HISCORE_BACK:
    case Title::RESULT_AGAIN_BACK:
        PopScreen(1);
        StepBack();
        break;
    case Title::RESULT_RESULTS_DONE:
        PopScreen(1);
        break;
    case Title::RESULT_ALERT_CANCEL:
        CancelAlert();
        break;
    case Title::RESULT_LEAVE:
    case Title::RESULT_BRONZE:
    case Title::RESULT_SILVER:
    case Title::RESULT_GOLD:
    case Title::RESULT_ONE_TRACK:
    case Title::RESULT_ALL_TRACKS:
    case Title::RESULT_IN_ORDER:
    case Title::RESULT_SPEED_1:
    case Title::RESULT_SPEED_2:
    case Title::RESULT_SPEED_3:
    case Title::RESULT_20:
    case Title::RESULT_21:
    case Title::RESULT_22:
        ChooseCourse(result);
        break;
    case Title::RESULT_CHANGE_CD:
        ChangeCd();
        break;
    case Title::RESULT_CD_CHANGED:
    case Title::RESULT_TRACK_CHOSEN:
        Start(0);
        break;
    }
}

void Glue::OnStateChange()
{
    int state = s_state;
    if (state != s_changedState) {
        switch (state) {
        case STATE_TITLE:
            AudioStream::SetVolume(0, 0);
            Gameplay::ShowTitle();
            break;
        case STATE_ALERT:
            break;
        case STATE_BUFFERING: {
            int seconds;
            if (g_courses[s_course].numTracks == TRACKS_ALL) {
                seconds = s_discSeconds + s_numTracks;
            } else {
                seconds = s_trackSeconds;
            }
            Gameplay::Start(s_course, seconds, s_score, s_trackStart,
                            g_courses[s_course].numTracks - s_tracksToPlay, s_demo);
            break;
        }
        case STATE_PLAYING:
            AudioStream::SetVolume(256, 256);
            break;
        }
        s_changedState = s_state;
    }
}

void Glue::SeedTitle()
{
    if (!s_seeded) {
        s_seeded = 1;
        u_long seed = VSync(-1);
        seed = seed << 16 | seed;
        u_long next;
        for (int i = 0; i < 10; i++) {
            next = NextRandom(seed);
            seed = next;
        }
        Title::Seed(next);
    }
}

void Glue::Init(GameParams* params)
{
    s_state = STATE_TITLE;
    s_changedState = 0;
    s_trackStart = Interval();
    s_trackSeconds = 0;
    s_course = COURSE_BRONZE;
    s_chosenTrack = 1;
    s_track = 1;
    s_tracksToBuffer = 1;
    s_tracksToPlay = 1;
    s_audioDisc = 0;
    s_homeDisc = 0;
    s_checking = 0;
    s_numTracks = 0;
    s_discSeconds = 0;
    s_done = 0;
    s_seeded = 0;
    ClearScreens();
    PushScreen(Title::SCREEN_MAIN, 0);
    ClearTrackOrder();
    s_discInfo.ready = 0;
    s_shellOpen.ready = 0;
    s_shellClose.ready = 0;
    s_noDisc.ready = 0;
    CdSys::SetDiscInfoReceiver(&s_discInfo);
    CdSys::SetShellOpenReceiver(&s_shellOpen.ready);
    CdSys::SetShellCloseReceiver(&s_shellClose.ready);
    CdSys::SetNoDiscReceiver(&s_noDisc.ready);
    s_vibration = &params->game.vibration;
    s_soundEffects = &params->game.soundEffects;
    SetHighScores(params->game.highScores);
    Title::SetHighScores(&params->game.highScores[0][0]);
    if (!params->game.saved) {
        /* both options on */
        params->game.saved = 1;
        ResetHighScores();
        params->game.soundEffects = 1;
        params->game.vibration = 1;
    }
    s_demo = params->demo;
    s_demoLevel = params->demoLevel;
    s_demoStarted = 0;
    params->demo = 0;
    params->seed = Gameplay::Seed(params->seed);
    Title::SetOptions(*s_soundEffects, *s_vibration);
    Title::SetCallback(ChooseTrack);
    Title::SetOptionsCallback(SetOptions);
    if (!s_demo) {
        ShowScreen();
    }
}

void Glue::Quit()
{
    CdSys::ClearDiscInfoReceiver(&s_discInfo);
    CdSys::ClearShellOpenReceiver(&s_shellOpen.ready);
    CdSys::ClearShellCloseReceiver(&s_shellClose.ready);
    CdSys::ClearNoDiscReceiver(&s_noDisc.ready);
    Title::SetCallback(NULL);
    Title::SetOptionsCallback(NULL);
    Title::SetHighScores(NULL);
}

void Glue::BufferTrackStart(int track, int position, int seconds)
{
    if (s_state == STATE_STARTING && track == s_playingTrack) {
        s_trackStart = Interval(position - AudioStream::SampleRate() * MAX_DISC_SECONDS, position);
        s_trackSeconds = seconds;
        s_state = STATE_BUFFERING;
        OnStateChange();
    }
}

void Glue::BufferTrackEnd(int position)
{
    switch (s_state) {
    case STATE_BUFFERING:
    case STATE_PLAYING:
        /* the audio CD's tracks go on one after the other */
        if (--s_tracksToBuffer <= 0 || s_course < COURSE_ONE_TRACK) {
            Gameplay::EndAt(position);
        } else {
            PlayTrack(++s_track);
        }
        break;
    case STATE_STARTING:
        printf("BufferTrackEnd(): bad state\n");
        break;
    }
}

void Glue::BufferQuiet(int position)
{
    Gameplay::Quiet(position);
}

#ifdef NON_MATCHING
/*
 * The same code, but the game's stack frame has 8 more bytes it does not use
 * and a0/a1 are swapped in the IsOver() code (see Gameplay.cpp).
 */
void Glue::Update(int position)
{
    OnStateChange();
    if (s_trackStart.IsOver(position)) {
        s_trackStart = Interval();
        if (s_state == STATE_BUFFERING) {
            s_state = STATE_PLAYING;
        }
    }
    if (s_shellOpen.ready) {
        s_shellOpen.ready = 0;
        s_audioDisc = 0;
        s_homeDisc = 0;
        s_checking = 0;
        if (s_state == STATE_ALERT) {
            if (TopScreen() == Title::SCREEN_CD_CHECK) {
                PopScreen(1);
            }
        } else {
            ToMainScreen();
        }
    }
    if (s_shellClose.ready) {
        s_checking = 1;
        s_shellClose.ready = 0;
        if (s_state == STATE_ALERT) {
            PushScreen(Title::SCREEN_CD_CHECK, 1);
        }
    }
    if (s_noDisc.ready) {
        s_noDisc.ready = 0;
        s_checking = 0;
        if (s_state == STATE_ALERT && TopScreen() == Title::SCREEN_CD_CHECK) {
            PopScreen(1);
        }
    }
    if (s_discInfo.ready) {
        s_discInfo.ready = 0;
        s_checking = 0;
        if (s_state == STATE_ALERT && TopScreen() == Title::SCREEN_CD_CHECK) {
            PopScreen(0);
        }
        if (s_discInfo.numTracks != 0) {
            s_numTracks = s_discInfo.numTracks;
            s_homeDisc = s_discInfo.isHomeDisc;
            s_audioDisc = s_discInfo.isHomeDisc ^ 1;
            s_discSeconds = s_discInfo.lengthSeconds;
            SignalSys::SetDisc(s_discInfo.id, s_discInfo.numTracks, s_discInfo.isHomeDisc);
            if (s_state == STATE_ALERT && HasDisc(TopScreen())) {
                PopScreen(0);
                s_state = STATE_TITLE;
                Start(0);
            }
        }
        if (s_state == STATE_ALERT) {
            ShowScreen();
        }
    }
    int result;
    if (s_demo && !s_demoStarted) {
        /* the demo plays a track of the game's own courses */
        s_demoStarted = 1;
        int level = s_demoLevel - 1;
        s_demoTrack = level % 2;
        result = Title::RESULT_BRONZE + level / 2;
    } else {
        result = Title::GetResult();
    }
    HandleResult(result);
}
#else
INCLUDE_ASM("asm/main_g/nonmatchings/Glue", Update__4Gluei);
#endif

void Glue::SignalGameOver()
{
    if (s_state == STATE_TITLE) {
        return;
    }
    if (s_state == STATE_PLAYING) {
        s_state = STATE_TITLE;
        PushScreen(Title::SCREEN_AGAIN_SELECT, 0);
        int rank = 0;
        if (!IsSpeedCourse(s_course)) {
            rank = RecordHighScore(s_course, Gameplay::GetScore());
        }
        Title::SetRank(rank);
        Title::SetScore(Gameplay::GetScore(), 0);
        Title::SetTimeUp(Gameplay::IsTimeUp());
        SeedTitle();
        PushScreen(Title::SCREEN_AGAIN, 1);
    } else {
        printf("SignalGameOver(): bad state\n");
    }
}

void Glue::SignalUserAbort()
{
    switch (s_state) {
    case STATE_STARTING:
    case STATE_BUFFERING:
    case STATE_PLAYING:
        s_state = STATE_TITLE;
        if (s_demo) {
            s_done = 1;
        } else {
            PushScreen(Title::SCREEN_AGAIN_SELECT, 1);
        }
        break;
    default:
        printf("SignalUserAbort(): bad state %d\n", s_state);
        break;
    }
}

void Glue::SignalGameClear()
{
    switch (s_state) {
    case STATE_TITLE:
    case STATE_STARTING:
        break;
    case STATE_PLAYING:
        if (s_demo) {
            s_state = STATE_TITLE;
            s_done = 1;
        } else if (s_course == COURSE_IN_ORDER) {
            s_state = STATE_TITLE;
            ShowScreen();
        } else if (Gameplay::GetScore() == 0) {
            SignalGameOver();
        } else {
            s_tracksToPlay--;
            if (s_course < COURSE_ONE_TRACK && s_tracksToPlay > 0) {
                /* the next track of the game's own course */
                s_state = STATE_STARTING;
                s_score += Gameplay::GetScore() + Gameplay::GetBonus();
                PlayTrack(++s_track);
            } else {
                s_state = STATE_TITLE;
                int score = IsSpeedCourse(s_course) ? Gameplay::GetSeconds()
                                                    : Gameplay::GetScore() + Gameplay::GetBonus();
                Title::SetRank(RecordHighScore(s_course, score));
                Title::SetScore(Gameplay::GetScore(), Gameplay::GetBonus());
                Title::SetSeconds(Gameplay::GetSeconds());
                Title::SetComment(s_comments[Gameplay::GetRank()]);
                SeedTitle();
                PushScreen(Title::SCREEN_CLEAR, 1);
            }
        }
        break;
    default:
        printf("SignalGameClear(): bad state %d\n", s_state);
        break;
    }
}

Fixed Glue::GetSoundLevel()
{
    return s_state == STATE_PLAYING ? Fixed::Int(AudioStream::GetLevel()) / 100 : Fixed();
}

int Glue::GetSoundEffects()
{
    return *s_soundEffects;
}

int Glue::GetVibration()
{
    return *s_vibration;
}

int Glue::IsDone()
{
    return s_done;
}

/* the allocator's "out of memory" string, of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Glue", D_8001DEEC);
