#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <libgte.h>

#include "Format.h"
#include "Title.h"
#include "TitleDraw.h"
#include "Parameter.h"
#include "InputSys.h"
#include "VideoSys.h"
#include "Gameplay.h"

/*
 * The ChoiceParameter destructor (emitted in Parameter.cpp) makes this egcs
 * instantiate vector<string>'s __destroy_aux() here as well; the game's
 * compiler did not, so it is declared extern.
 */
extern template void __destroy_aux(string*, string*, __false_type);

/* the "Info: status" and "Analog: Left" strings of a shared header, then "track = ..." of another */
INCLUDE_RODATA("asm/main_g/nonmatchings/Title", D_8001D20C);
INCLUDE_RODATA("asm/main_g/nonmatchings/Title", D_8001D258);

/* the frames of `sixtieths` of a second */
static int Frames(int sixtieths)
{
    return VideoSys::Fps() * sixtieths / 60;
}

/* Gives a controller a name when a parameter changes. */
class PlayerNameSetter : public ParameterListener {
public:
    PlayerNameSetter(const char* name) : m_name(name) {}

    void Changed(Parameter* parameter) { InputSys::SetPlayerName(m_name, 0); }

private:
    const char* m_name;
};

const char* Title::s_screenNames[NUM_SCREENS] = {
    "OFF",
    "MAIN",
    "PSJ MODE SELECT",
    "UNKNOWN MODE SELECT",
    "MODE SPEED SELECT",
    "CD CHANGE",
    "TRK SELECT",
    "ALERT VIB CD IN",
    "ALERT AUDIO CD IN",
    "CD CHECK",
    "OPTION",
    "PSJ HISCORE",
    "UNKNOWN HISCORE",
    "CLEAR",
    "AGAIN",
    "AGAIN SELECT",
    "DONE",
};

/* the music of each screen (0: none) */
static const int s_music[Title::NUM_SCREENS] = {
    0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0, 32, 0,
};

/* the debug menu: any screen, or back to the game ("exit" names the first controller "Play") */
ChoiceParameter Title::s_screenChoice("title", s_screenNames[0]);
ActionParameter Title::s_exit("exit");
PlayerNameSetter g_exitListener("Play");
Random Title::s_random;

int Title::s_numTracks;
Title::Callback Title::s_callback;
Title::OptionsCallback Title::s_optionsCallback;
int Title::s_soundEffects;
int Title::s_vibration;
int Title::s_score;
int Title::s_bonus;
int Title::s_seconds;
int Title::s_comment;
int Title::s_course;
int Title::s_speedCourse;
int* Title::s_highScores;
int Title::s_rank;
int Title::s_unused;
int Title::s_timeUp;
int Title::s_arrived;
int Title::s_fade;
int Title::s_screen;
int Title::s_previous;
int Title::s_next;
int Title::s_fadeFrame;
int Title::s_fadeStep;
int Title::s_frame;
ParameterMenu* Title::s_menu;
int Title::s_result;
int Title::s_lastChoice;

void Title::UpdateFade()
{
    int frames = Frames(20);
    s_fadeFrame += s_fadeStep;
    if (s_fadeFrame == frames && s_fadeStep == 1) {
        s_arrived = 1;
    } else {
        s_arrived = 0;
    }
    if (s_fadeFrame == frames) {
        s_frame = 0;
        s_screen = s_next;
        s_fadeStep--;
    } else if (s_fadeFrame == 0) {
        s_fadeStep = 0;
    }
    if (s_fadeStep > 0) {
        s_fade = FADE_OUT;
    } else if (s_fadeStep < 0 || s_fadeFrame == frames) {
        s_fade = FADE_IN;
    } else {
        s_fade = FADE_NONE;
    }
}

void Title::Init()
{
    TitleDraw::Load();
    s_exit.AddListener(&g_exitListener);
    for (int i = 1; i < NUM_SCREENS; i++) {
        s_screenChoice.AddChoice(s_screenNames[i]);
    }
    s_menu = new ParameterMenu("Title");
    s_menu->Add(&TitleDraw::s_subline);
    s_menu->Add(&TitleDraw::s_offset);
    s_menu->Add(&s_screenChoice);
    s_menu->Add(&s_exit);
    s_frame = 0;
    s_arrived = 1;
    s_fadeFrame = 0;
    s_fadeStep = 0;
    s_fade = FADE_NONE;
    s_result = 0;
    s_previous = 0;
    s_next = 0;
    s_screen = SCREEN_NONE;
    s_callback = NULL;
    s_optionsCallback = NULL;
    /* sound effects on, vibration off, until Glue::Init() sets the saved ones */
    SetOptions(1, 0);
    SetHighScores(NULL);
    s_speedCourse = 0;
    s_lastChoice = -1;
}

void Title::Free()
{
    TitleDraw::Unload();
    delete s_menu;
}

void Title::Update()
{
    s_menu->Update();
    s_menu->Draw();
    UpdateFade();
    /* the screens shake more and more as they fade */
    int jitter = s_fadeFrame * (200 * ONE);
    int still = s_fadeFrame == 0;
    switch (s_screen) {
    case SCREEN_NONE:
    case SCREEN_DONE:
        break;
    case SCREEN_MAIN:
        s_result = TitleDraw::DrawMain(jitter, still);
        break;
    case SCREEN_PSJ_MODE_SELECT:
        s_result = TitleDraw::DrawPsjModeSelect(jitter, still);
        break;
    case SCREEN_UNKNOWN_MODE_SELECT:
        s_result = TitleDraw::DrawUnknownModeSelect(still);
        break;
    case SCREEN_MODE_SPEED_SELECT:
        s_result = TitleDraw::DrawModeSpeedSelect(still);
        break;
    case SCREEN_CD_CHANGE:
        s_result = TitleDraw::DrawCdChange(jitter, still);
        break;
    case SCREEN_TRACK_SELECT:
        s_result = TitleDraw::DrawTrackSelect(jitter, still);
        break;
    case SCREEN_ALERT_VIB_CD_IN:
    case SCREEN_ALERT_AUDIO_CD_IN:
    case SCREEN_CD_CHECK:
        s_result = TitleDraw::DrawAlert(jitter);
        break;
    case SCREEN_OPTION:
        s_result = TitleDraw::DrawOption(still);
        break;
    case SCREEN_PSJ_HISCORE:
        /* straight from the results, its menu takes no input */
        if (s_previous == SCREEN_CLEAR || s_previous == SCREEN_AGAIN) {
            still = 0;
        }
        s_result = TitleDraw::DrawPsjHiscore(jitter, still, 0);
        break;
    case SCREEN_UNKNOWN_HISCORE:
        s_result = TitleDraw::DrawUnknownHiscore(jitter, still);
        break;
    case SCREEN_CLEAR:
        s_result = TitleDraw::DrawClear(jitter);
        break;
    case SCREEN_AGAIN:
        s_result = TitleDraw::DrawAgain(jitter);
        break;
    case SCREEN_AGAIN_SELECT:
        s_result = TitleDraw::DrawAgainSelect(jitter, still);
        break;
    }
    TitleDraw::UpdateSounds();
    if (s_fade == FADE_OUT) {
        s_result = 0;
    }
    if (s_screenChoice.GetIndex() != s_lastChoice) {
        GoTo((Screen)s_screenChoice.GetIndex());
    }
    s_lastChoice = s_screenChoice.GetIndex();
}

void Title::GoTo(Screen screen)
{
    if (screen != SCREEN_NONE) {
        s_fadeFrame = 0;
        s_fadeStep = 1;
        s_next = screen;
        s_previous = s_screen;
    } else {
        s_screen = screen;
    }
    s_result = 0;
    if (s_music[s_screen] != 0 && s_music[s_next] != s_music[s_screen]) {
        TitleDraw::StopMusic();
    }
}

int Title::GetResult()
{
    return s_result;
}

void Title::SetNumTracks(int numTracks)
{
    s_numTracks = numTracks;
}

void Title::SetCallback(Callback callback)
{
    s_callback = callback;
}

void Title::SetScore(int score, int bonus)
{
    s_score = score;
    s_bonus = bonus;
}

void Title::SetSeconds(int seconds)
{
    s_seconds = seconds;
}

void Title::SetComment(int comment)
{
    s_comment = comment;
}

void Title::SetCourse(int course)
{
    s_course = course;
    s_speedCourse = course >= COURSE_SPEED_1 && course <= COURSE_SPEED_3;
}

void Title::SetOptions(int soundEffects, int vibration)
{
    s_soundEffects = soundEffects;
    s_vibration = vibration;
}

void Title::SetOptionsCallback(OptionsCallback callback)
{
    s_optionsCallback = callback;
}

void Title::SetHighScores(int* highScores)
{
    s_highScores = highScores;
}

void Title::SetRank(int rank)
{
    s_rank = rank;
}

void Title::SetTimeUp(int timeUp)
{
    s_timeUp = timeUp;
}

void Title::Seed(u_long seed)
{
    s_random.Seed(seed);
}
