#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#include "Format.h"
#include "TitleDraw.h"
#include "Title.h"
#include "LanguageSelect.h"
#include "Parameter.h"
#include "ModelMovie.h"
#include "ObjectSwap.h"
#include "ScoreMeter.h"
#include "VideoSys.h"
#include "Gameplay.h"

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/TitleDraw", D_8001A884);

/* the camera at (x, y, z), looking along z to depth `targetZ` (in unk_28A88.cpp) */
void LookAlongZ(Fixed x, Fixed y, Fixed z, Fixed targetZ, Fixed fov);

/* the models of the screens, in title/ */
enum {
    MODEL_VIBRI,
    MODEL_SPEED,
    MODEL_CD,
    MODEL_TRACK,
    MODEL_ALERT,
    MODEL_OPTION,
    MODEL_AGAIN,
    MODEL_HISCORE,
    MODEL_CLEAR_OVER,
    MODEL_MAIN,
    MODEL_MODE_PSJ,
    MODEL_MODE_UNKNOWN,
    MODEL_CLEAR_SONG,
    MODEL_SPEED_METER,
    MODEL_SPEED_SCRIBBLE /* a tangle of lines (spd_moja) */
};

/*
 * How Vibri is: lying (asleep), sitting or standing, each with a movie of
 * her speaking, played while a voice plays; then a pause, the speaking
 * movie stopped, before she is back in the pose it began from.
 */
enum {
    VIBRI_LYING,
    VIBRI_LYING_SPEAKING,
    VIBRI_SITTING,
    VIBRI_SITTING_SPEAKING,
    VIBRI_STANDING,
    VIBRI_STANDING_SPEAKING,
    VIBRI_PAUSED,
    VIBRI_HIDDEN
};

NumberParameter<int> TitleDraw::s_subline("Subline", 0, 1, -1, 2);
NumberParameter<int> TitleDraw::s_offset("Offset", 0, 1);

/* the menus: their items, a column of them, and the cursors */
static const int s_mainItems[] = { 1, 2, 3, MENU_BACK };
static const int s_psjModeItems[] = { 1, 2, 3, 4, MENU_BACK };
static const int s_unknownModeItems[] = { 1, 2, 3, 4, 5, MENU_BACK };
static const int s_againItems[] = { 1, MENU_BACK };
static const int s_cdChangeItems[] = { 1, 2, MENU_BACK };
static const int s_optionItems[] = { 5, 6, MENU_BACK };
static const int s_speedItems[] = { 1, 2, 3, MENU_BACK };
static const int s_backItems[] = { 14, MENU_BACK };

static MenuGrid s_mainGrid(1, 4, s_mainItems);
MenuCursor TitleDraw::s_mainCursor;
static MenuGrid s_psjModeGrid(1, 5, s_psjModeItems);
MenuCursor TitleDraw::s_psjModeCursor;
static MenuGrid s_unknownModeGrid(1, 6, s_unknownModeItems);
MenuCursor TitleDraw::s_unknownModeCursor;
static MenuGrid s_againGrid(1, 2, s_againItems);
MenuCursor TitleDraw::s_againCursor;
static MenuGrid s_cdChangeGrid(1, 3, s_cdChangeItems);
MenuCursor TitleDraw::s_cdChangeCursor;
static MenuGrid s_optionGrid(1, 3, s_optionItems);
MenuCursor TitleDraw::s_optionCursor;
static MenuGrid s_speedGrid(1, 4, s_speedItems);
MenuCursor TitleDraw::s_speedCursor;
static MenuGrid s_backGrid(1, 2, s_backItems);
MenuCursor TitleDraw::s_backCursor;

/* how fast the menus' movies play, and how much their models jitter */
static Fixed s_menuSpeed = 1.0;
static GteFixed s_menuJitter = 5.0;
Fixed TitleDraw::s_speed;
/* nearly black: hides the two "Z"s of Vibri asleep on the high score screens */
static CVECTOR s_hideColor = { 0, 0, 2, 0 };
static GteFixed s_textJitter = 11.0;

static const char* s_modelFiles[TitleDraw::NUM_MODELS] = {
    "vibri/vibri.tmd",
    "speed/md_spd.tmd",
    "cd/cd.tmd",
    "trk/trk.tmd",
    "al/al.tmd",
    "option/op.tmd",
    "again/ag.tmd",
    "hiscore/hi.tmd",
    "end/cl_ov.tmd",
    "ma/ma.tmd",
    "mode/mdv.tmd",
    "mode/mdu.tmd",
    "end/clr_song.tmd",
    "end/spd_mtr.tmd",
    "end/spd_moja.tmd",
};

static const char* s_movieFiles[TitleDraw::NUM_MOVIES] = {
    "vibri/mn_v01a.anm",
    "vibri/mn_v01b.anm",
    "vibri/mn_v02a.anm",
    "vibri/mn_v02b.anm",
    "vibri/mn_v03a.anm",
    "vibri/mn_v03b.anm",
    "v_cl/clr_v04.anm",
    "v_cl/clr_v05.anm",
    "v_cl/clr_v12.anm",
    "v_cl/clr_v14.anm",
    "v_cl/clr_v15.anm",
    "v_cl/clr_v16.anm",
    "v_ov/gov_v03a.anm",
    "v_ov/gov_v03b.anm",
    "v_ov/gov_v03c.anm",
    "v_ov/gov_v05.anm",
    "v_ov/gov_v06a.anm",
    "v_ov/gov_v06b.anm",
    "v_ov/gov_v07.anm",
    "v_ov/gov_v01.anm",
    "v_ov/spd_v02.anm",
    "v_ov/spd_v03.anm",
    "v_song/v01.anm",
    "v_song/v02.anm",
    "v_song/v03a.anm",
    "v_song/v03b.anm",
    "v_song/v04.anm",
    "v_song/v05a.anm",
    "v_song/v05b.anm",
    "v_song/v06.anm",
    "v_song/v07.anm",
    "v_song/v08.anm",
    "v_cl/s_v01.anm",
    "v_cl/s_v02.anm",
    "v_cl/s_v03.anm",
    "v_cl/s_v04.anm",
    "v_cl/s_v05.anm",
    "v_cl/s_v06.anm",
    "v_cl/s_v07.anm",
    "speed/spd01_02.anm",
    "speed/spd02_03.anm",
    "speed/spd03_04.anm",
    "speed/spd04_01.anm",
    "speed/open.anm",
    "speed/close.anm",
    "cd/cd-12.anm",
    "cd/cd-23.anm",
    "cd/cd-31.anm",
    "cd/open.anm",
    "cd/close.anm",
    "trk/trk_l.anm",
    "trk/trk_r.anm",
    "trk/trk_exit.anm",
    "al/al_rot.anm",
    "option/op01_02.anm",
    "option/op02_03.anm",
    "option/op03_01.anm",
    "option/open.anm",
    "option/close.anm",
    "again/ag_12.anm",
    "again/ag_21.anm",
    "again/close.anm",
    "hiscore/hi1_open.anm",
    "hiscore/hi1_loop.anm",
    "hiscore/hi2_l.anm",
    "hiscore/hi2_r.anm",
    "hiscore/hi2_ex.anm",
    "end/clr_01.anm",
    "end/g_ov_01.anm",
    "end/g_ov_02.anm",
    "end/g_ov_03.anm",
    "end/g_ov_04.anm",
    "end/spd_g02.anm",
    "end/s_ten02.anm",
    "ma/ma12.anm",
    "ma/ma23.anm",
    "ma/ma34.anm",
    "ma/ma41.anm",
    "mode/mdv12.anm",
    "mode/mdv23.anm",
    "mode/mdv34.anm",
    "mode/mdv45.anm",
    "mode/mdv51.anm",
    "mode/mdv_op.anm",
    "mode/mdv_cl.anm",
    "mode/mdu12.anm",
    "mode/mdu23.anm",
    "mode/mdu34.anm",
    "mode/mdu45.anm",
    "mode/mdu56.anm",
    "mode/mdu61.anm",
    "mode/mdu_op.anm",
    "mode/mdu_cl.anm",
    "end/song_01.anm",
    "end/song_ten.anm",
    "end/s_mtr01.anm",
    "end/s_kem01.anm",
    "end/s_kem02.anm",
    "end/spd_m01.anm",
    "end/spd_m02.anm",
    "v_cl/clr_v00.anm",
    "end/clr_00.anm",
};

/* the model each movie moves */
static const int s_movieModels[TitleDraw::NUM_MOVIES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* MODEL_VIBRI */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1,                                           /* MODEL_SPEED */
    2, 2, 2, 2, 2,                                              /* MODEL_CD */
    3, 3, 3,                                                    /* MODEL_TRACK */
    4,                                                          /* MODEL_ALERT */
    5, 5, 5, 5, 5,                                              /* MODEL_OPTION */
    6, 6, 6,                                                    /* MODEL_AGAIN */
    7, 7, 7, 7, 7,                                              /* MODEL_HISCORE */
    8, 8, 8, 8, 8, 8, 8,                                        /* MODEL_CLEAR_OVER */
    9, 9, 9, 9,                                                 /* MODEL_MAIN */
    10, 10, 10, 10, 10, 10, 10,                                 /* MODEL_MODE_PSJ */
    11, 11, 11, 11, 11, 11, 11, 11,                             /* MODEL_MODE_UNKNOWN */
    12, 12,                                                     /* MODEL_CLEAR_SONG */
    13, 13, 13,                                                 /* MODEL_SPEED_METER */
    14, 14,                                                     /* MODEL_SPEED_SCRIBBLE */
    0, 8,                                                       /* MODEL_VIBRI, MODEL_CLEAR_OVER */
};

static const char* s_cameraFiles[TitleDraw::NUM_CAMERAS] = {
    "cam/c_l.anc",
    "cam/r_c.anc",
    "cam/zoom_in.anc",
    "cam/song_01.anc",
    "cam/song_02.anc",
    "cam/song_03a.anc",
    "cam/song_03b.anc",
    "cam/song_04.anc",
    "cam/song_06.anc",
    "cam/song_07.anc",
    "cam/song_08.anc",
    "cam/song_05a.anc",
    "cam/song_05b.anc",
    "cam/ma_cam.anc",
};

SoundSequence TitleDraw::s_voices(1);
SoundSequence TitleDraw::s_music(0);

/* the voice saying a number */
VoiceLine TitleDraw::s_voiceLine = { { 0 } };

/*
 * Times each model is drawn, jittered anew: Subline + 1, read once at
 * start-up (the game keeps two copies).
 */
static int s_passes = TitleDraw::s_subline.m_value + 1;
static int s_passes2 = TitleDraw::s_subline.m_value + 1;

MenuInput TitleDraw::s_input;
TmdModel* TitleDraw::s_models[NUM_MODELS];
ModelMovie* TitleDraw::s_movies[NUM_MOVIES];
CameraMovie* TitleDraw::s_cameras[NUM_CAMERAS];
ObjectSwap* TitleDraw::s_marks[7];
ObjectSwap* TitleDraw::s_bonusDigits[NUM_DIGITS];
ObjectSwap* TitleDraw::s_scoreDigits[NUM_DIGITS];
ObjectSwap* TitleDraw::s_totalDigits[NUM_DIGITS];
ObjectSwap* TitleDraw::s_songTotalDigits[NUM_DIGITS];
ObjectSwap* TitleDraw::s_firstScoreDigits[NUM_DIGITS];
ObjectSwap* TitleDraw::s_secondScoreDigits[NUM_DIGITS];
ObjectSwap* TitleDraw::s_thirdScoreDigits[NUM_DIGITS];

/*
 * The state of the screens: globals of this file, not members (the game
 * has them after the file's static constants, where only file-scope
 * variables without an initializer are put).
 */
int g_pickedComment;           /* which of Vibri's comments is said (PickComment()) */
int g_vibriState;              /* how Vibri is (VIBRI_*) */
int g_passScore;               /* CLEAR: the score the course needs */
int g_vibriFrame;              /* frames of VIBRI_PAUSED */
int g_entering;                /* the movie into the menu plays (CLEAR: the score is counted; CD CHECK: it is over) */
int g_vibriMovie;              /* the one Vibri plays */
int g_frame;                   /* of the results screens */
int g_waitFrame;               /* Wait()'s count, and a few screens' own */
int g_repeat;                  /* CLEAR: times the score was counted again */
int g_hiscoreFrame;            /* CLEAR, AGAIN: frame of the high scores shown after (0: not yet) */
int g_leaveFrame;              /* CLEAR, AGAIN: frames of the shaking out before the high scores (0: none) */
int g_cameraCue;               /* the SetCamera() cue whose camera UpdateCamera() plays (-1: none) */
int g_track;                   /* TRK SELECT: the track chosen */
int g_menuMovie;               /* of the menu's model (CLEAR: the step of the sequence) */
int g_leaving;                 /* the back item was chosen, the screen goes (CLEAR: the comment is shown) */
int g_medalShown;              /* CLEAR: the medal sequence plays */
int g_medal;                   /* CLEAR: 0 bronze, 1 silver, 2 gold */
int g_hiscoreCourse;           /* PSJ HISCORE: the course shown */
int g_unknownCourse;           /* UNKNOWN HISCORE: the course shown */
/* the signs of the models, shown by ObjectSwap */
ObjectSwap* g_againCourseSign; /* AGAIN SELECT: the course, */
ObjectSwap* g_againSpeedSign;  /* and a speed course's bronze, silver or gold */
ObjectSwap* g_soundEffectsSwap; /* OPTION: on or off */
ObjectSwap* g_vibrationSwap;
ObjectSwap* g_alertSwap;       /* which alert */
ObjectSwap* g_courseSign;      /* the high scores: the course, */
ObjectSwap* g_speedSign;       /* and a speed course's bronze, silver or gold */
ObjectSwap* g_commentSign;     /* CLEAR: the sign of the comment by the rank (Title::s_comment) */
ObjectSwap* g_speedMeterSign;  /* CLEAR, a speed course: its speed meter */

void TitleDraw::DrawText(const char* text, int jitter, GteFixed x, GteFixed y, GteFixed scale)
{
    LookAlongZ(0.0, 0.0, 0.0, 295.625, 3.75);
    Font::SetPosition(x, y);
    Font::SetScale(scale);
    Font::Draw(text, jitter);
}

ModelMovie* TitleDraw::NewModelMovie(TmdModel* model, const char* file)
{
    return new ModelMovie(model, MemPtr<short>(file), 0);
}

CameraMovie* TitleDraw::NewCameraMovie(const char* file)
{
    return new CameraMovie(MemPtr<short>(file), 0);
}

/* the frames of `sixtieths` of a second */
static int Frames(int sixtieths)
{
    return VideoSys::Fps() * sixtieths / 60;
}

void TitleDraw::SetModelJitter(int model, int jitter, int passes)
{
    for (int i = 0; i < Model(model)->m_numObjects; i++) {
        Model(model)->m_objects[i]->SetJitter(Fixed::FromRaw(jitter << 4), passes);
    }
}

void TitleDraw::DrawMovie(int movie, Fixed speed)
{
    s_movies[movie]->SetSpeed(speed);
    s_movies[movie]->Draw();
}

void TitleDraw::RewindMovies()
{
    for (int i = 0; i < NUM_MOVIES; i++) {
        s_movies[i]->Rewind();
    }
}

/* starts the camera movie of `cue` (-1: none), the others back at their start */
void TitleDraw::SetCamera(int cue)
{
    static const CameraCue cues[] = {
        { 0, 1, 0 }, { 0, -1, 1 }, { 1, 1, 0 }, { 1, -1, 1 }, { 2, 1, 0 }, { 2, -1, 1 },
    };
    if (cue < 0) {
        return;
    }
    for (int i = 0; i < NUM_CAMERAS; i++) {
        s_cameras[i]->Rewind();
        s_cameras[i]->SetSpeed(1.0);
    }
    s_cameras[cues[cue].camera]->Rewind(cues[cue].backwards);
    s_cameras[cues[cue].camera]->SetSpeed(Fixed::Int(cues[cue].speed));
}

void TitleDraw::UpdateCamera(int cue)
{
    static const int cameras[] = { 0, 0, 1, 1, 2, 2 };
    if (cue < 0) {
        return;
    }
    s_cameras[cameras[cue]]->Update(0);
}

void TitleDraw::DrawVibri(int jitter, int passes)
{
    static int initialized = 0;
    /* frames of VIBRI_PAUSED */
    static int wait;
    if (!initialized) {
        initialized = 1;
        wait = Frames(90);
    }
    if (g_vibriState == VIBRI_HIDDEN) {
        return;
    }
    SetModelJitter(MODEL_VIBRI, jitter + 0x4000, passes);
    if (Title::s_screen >= Title::SCREEN_PSJ_HISCORE && Title::s_screen <= Title::SCREEN_AGAIN) {
        s_models[MODEL_VIBRI]->m_objects[19]->SetColor(s_hideColor);
        s_models[MODEL_VIBRI]->m_objects[20]->SetColor(s_hideColor);
    } else {
        s_models[MODEL_VIBRI]->m_objects[19]->ClearColor();
        s_models[MODEL_VIBRI]->m_objects[20]->ClearColor();
    }
    if (g_vibriState == VIBRI_LYING_SPEAKING || g_vibriState == VIBRI_SITTING_SPEAKING ||
        g_vibriState == VIBRI_STANDING_SPEAKING) {
        s_movies[g_vibriMovie]->SetSpeed(1.0);
        if (!s_voices.IsPlaying()) {
            g_vibriState = VIBRI_PAUSED;
            g_vibriFrame = 0;
        }
    }
    if (g_vibriState == VIBRI_PAUSED) {
        s_movies[g_vibriMovie]->SetSpeed(0.0);
        if (++g_vibriFrame == wait) {
            /* from the speaking movie (1, 3 or 5) back to its pose */
            if (g_vibriMovie == 1) {
                g_vibriState = VIBRI_LYING;
            }
            if (g_vibriMovie == 3) {
                g_vibriState = VIBRI_SITTING;
            }
            if (g_vibriMovie == 5) {
                g_vibriState = VIBRI_STANDING;
            }
            g_vibriFrame = 0;
        }
    }
    LookAlongZ(39.1875, 0.0, -166.3125, 0.0, 2.9375);
    /* the movie of each state but VIBRI_PAUSED, which keeps the last one */
    static const int movies[] = { 0, 1, 2, 3, 4, 5 };
    int movie = g_vibriState == VIBRI_PAUSED ? g_vibriMovie : movies[g_vibriState];
    s_movies[movie]->Draw();
    g_vibriMovie = movie;
}

void TitleDraw::ResetSwaps(ObjectSwap** swaps, int count)
{
    for (int i = 0; i < count; i++) {
        swaps[i]->Show(0);
    }
}

/* `value` in the digits of `swaps`, without its leading zeros; with `seconds`, swaps[0] shows the unit ("sec.") */
void TitleDraw::ShowNumber(ObjectSwap** swaps, int value, int seconds)
{
    int count = seconds ? NUM_DIGITS - 1 : NUM_DIGITS;
    int first = seconds != 0;
    int digits[count];
    int divisor = 1;
    int shown = 0;
    for (int i = 0; i < count; i++) {
        digits[count - i - 1] = value / divisor % 10;
        divisor *= 10;
    }
    for (int i = 0; i < count; i++) {
        int place = i + 1;
        int index = count - place + first;
        int digit = digits[i];
        if (digit != 0 || shown) {
            shown = 1;
            swaps[index]->Show(digit + 1);
        } else {
            swaps[index]->Show(0);
        }
    }
    if (!shown) {
        swaps[first]->Show(1);
    }
    if (seconds) {
        swaps[0]->Show(11);
    }
}

void TitleDraw::ShowMarks(ObjectSwap** swaps)
{
    for (int i = 0; i < ScoreMeter::NUM_MARKS; i++) {
        swaps[ScoreMeter::NUM_MARKS - 1 - i]->Show(ScoreMeter::GetMark(i) + 1);
    }
}

/* which of the comments on a result is shown */
void TitleDraw::PickComment()
{
    int n = Title::s_random.Below(10);
    if (n < 4) {
        g_pickedComment = 0;
    } else if (n < 7) {
        g_pickedComment = 1;
    } else if (n < 9) {
        g_pickedComment = 2;
    } else {
        g_pickedComment = 3;
    }
}

void TitleDraw::CallCallback(int track)
{
    if (Title::s_callback != NULL) {
        Title::s_callback(track);
    }
}

/* 1 once every `sixtieths` of a second */
int TitleDraw::Wait(int sixtieths)
{
    g_waitFrame++;
    if (g_waitFrame == Frames(sixtieths)) {
        g_waitFrame = 0;
        return 1;
    }
    return 0;
}

void TitleDraw::DrawMenuMovie(int movie)
{
    if (Title::s_fade == Title::FADE_NONE) {
        s_speed = s_menuSpeed;
    }
    DrawMovie(movie, s_speed);
    if (s_movies[movie]->m_finished) {
        s_speed = 0.0;
        g_entering = 0;
    }
}

void TitleDraw::Load()
{
    /* the movies played round and round */
    static const int looping[] = {
        0, 1, 6, 13, 17, 2, 3, 4, 5, 53, 63, 93, 35
    };
    /* the objects of the ObjectSwaps: the one replaced, then the ones shown for 1, 2, ... */
    static const int againCourseObjects[] = { 3, 6, 7, 8, 9, 10, 14 };
    static const int againSpeedObjects[] = { 15, 11, 12, 13 };
    static const int soundEffectsObjects[] = { 2, 1 };
    static const int vibrationObjects[] = { 4, 3 };
    static const int alertObjects[] = { 1, 2, 4 };
    static const int courseObjects[] = { 6, 47, 44, 45, 43, 46 };
    static const int speedObjects[] = { 5, 40, 41, 42 };
    static const int marks[ScoreMeter::NUM_MARKS][16] = {
        { 21, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 },
        { 22, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 },
        { 23, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 },
        { 24, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 },
        { 25, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 },
        { 26, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 },
        { 27, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 },
    };
    static const int scoreDigits[NUM_DIGITS][12] = {
        { 31, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 63 },
        { 32, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 63 },
        { 33, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 63 },
        { 34, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 63 },
        { 35, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 63 },
        { 36, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 63 },
    };
    static const int bonusDigits[NUM_DIGITS][11] = {
        { 0, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 1, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 2, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 3, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 4, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 5, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
    };
    static const int totalDigits[NUM_DIGITS][11] = {
        { 11, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 12, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 13, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 14, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 15, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
        { 16, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61 },
    };
    static const int songTotalDigits[NUM_DIGITS][11] = {
        { 14, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 15, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 16, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 17, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 18, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 19, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
    };
    static const int firstScoreDigits[NUM_DIGITS][12] = {
        { 8, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 9, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 10, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 11, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 12, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 13, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
    };
    static const int secondScoreDigits[NUM_DIGITS][12] = {
        { 14, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 15, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 16, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 17, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 18, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 19, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
    };
    static const int thirdScoreDigits[NUM_DIGITS][12] = {
        { 20, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 21, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 22, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 23, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 24, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
        { 25, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39 },
    };
    static const int commentObjects[] = { 10, 7, 8, 9 };
    static const int speedMeterObjects[] = { 4, 5, 6 };
    char path[64];

    Font::Init();
    KeyRepeat::Init();
    for (int i = 0; i < NUM_MODELS; i++) {
        sprintf(path, "%s/%s", "title", s_modelFiles[i]);
        s_models[i] = LoadTmdModel(path);
    }
    for (int i = 0; i < NUM_MOVIES; i++) {
        sprintf(path, "%s/%s", "title", s_movieFiles[i]);
        s_movies[i] = NewModelMovie(s_models[s_movieModels[i]], path);
    }
    for (int i = 0; i < NUM_CAMERAS; i++) {
        sprintf(path, "%s/%s", "title", s_cameraFiles[i]);
        s_cameras[i] = NewCameraMovie(path);
    }
    s_cameras[13]->SetLoop(1);

    g_againCourseSign = new ObjectSwap(s_models[MODEL_AGAIN], againCourseObjects);
    g_againSpeedSign = new ObjectSwap(s_models[MODEL_AGAIN], againSpeedObjects);
    g_soundEffectsSwap = new ObjectSwap(s_models[MODEL_OPTION], soundEffectsObjects);
    g_vibrationSwap = new ObjectSwap(s_models[MODEL_OPTION], vibrationObjects);
    g_alertSwap = new ObjectSwap(s_models[MODEL_ALERT], alertObjects);
    g_courseSign = new ObjectSwap(s_models[MODEL_HISCORE], courseObjects);
    g_speedSign = new ObjectSwap(s_models[MODEL_HISCORE], speedObjects);
    for (int i = 0; i < NUM_DIGITS; i++) {
        s_bonusDigits[i] = new ObjectSwap(s_models[MODEL_CLEAR_OVER], bonusDigits[i]);
        s_scoreDigits[i] = new ObjectSwap(s_models[MODEL_CLEAR_OVER], scoreDigits[i]);
        s_totalDigits[i] = new ObjectSwap(s_models[MODEL_CLEAR_OVER], totalDigits[i]);
        s_songTotalDigits[i] = new ObjectSwap(s_models[MODEL_CLEAR_SONG], songTotalDigits[i]);
        s_firstScoreDigits[i] = new ObjectSwap(s_models[MODEL_HISCORE], firstScoreDigits[i]);
        s_secondScoreDigits[i] = new ObjectSwap(s_models[MODEL_HISCORE], secondScoreDigits[i]);
        s_thirdScoreDigits[i] = new ObjectSwap(s_models[MODEL_HISCORE], thirdScoreDigits[i]);
    }
    for (int i = 0; i < ScoreMeter::NUM_MARKS; i++) {
        s_marks[i] = new ObjectSwap(s_models[MODEL_CLEAR_OVER], marks[i]);
    }
    g_commentSign = new ObjectSwap(s_models[MODEL_CLEAR_OVER], commentObjects);
    g_speedMeterSign = new ObjectSwap(s_models[MODEL_SPEED_METER], speedMeterObjects);
    for (u_int j = 0; j < sizeof(looping) / sizeof(looping[0]); j++) {
        s_movies[looping[j]]->SetLoop(1);
    }

    s_input.confirm = s_input.up = s_input.down = s_input.changed = s_input.right = s_input.left = s_input.back = 0;
    s_backCursor = MenuCursor(&s_backGrid);
    s_mainCursor = MenuCursor(&s_mainGrid);
    s_psjModeCursor = MenuCursor(&s_psjModeGrid);
    s_unknownModeCursor = MenuCursor(&s_unknownModeGrid);
    s_againCursor = MenuCursor(&s_againGrid);
    s_optionCursor = MenuCursor(&s_optionGrid);
    s_speedCursor = MenuCursor(&s_speedGrid);
    s_cdChangeCursor = MenuCursor(&s_cdChangeGrid, 0, 1);

    g_vibriState = VIBRI_HIDDEN;
    g_passScore = 2500;
    g_medal = 2;
    g_hiscoreFrame = 0;
    g_leaveFrame = 0;
    g_vibriFrame = 0;
    g_entering = 0;
    g_vibriMovie = 0;
    g_frame = 0;
    g_waitFrame = 0;
    g_repeat = 0;
    g_track = 1;
    g_menuMovie = 0;
    g_leaving = 0;
    g_medalShown = 0;
    g_cameraCue = -1;
    g_hiscoreCourse = 0;
    g_unknownCourse = 0;
    s_speed = s_menuSpeed;
}

void TitleDraw::Unload()
{
    int i;

    Font::Free();
    delete g_againCourseSign;
    delete g_againSpeedSign;
    delete g_soundEffectsSwap;
    delete g_vibrationSwap;
    delete g_alertSwap;
    delete g_courseSign;
    delete g_speedSign;
    for (i = 0; i < NUM_DIGITS; i++) {
        delete s_bonusDigits[i];
        delete s_scoreDigits[i];
        delete s_totalDigits[i];
        delete s_firstScoreDigits[i];
        delete s_secondScoreDigits[i];
        delete s_thirdScoreDigits[i];
        delete s_songTotalDigits[i];
    }
    for (i = 0; i < ScoreMeter::NUM_MARKS; i++) {
        delete s_marks[i];
    }
    delete g_commentSign;
    delete g_speedMeterSign;
    for (i = 0; i < NUM_MOVIES; i++) {
        delete s_movies[i];
    }
    for (i = 0; i < NUM_MODELS; i++) {
        delete s_models[i];
    }
    for (i = 0; i < NUM_CAMERAS; i++) {
        delete s_cameras[i];
    }
    s_voices.Stop();
    s_music.Stop();
}

/*
 * CLEAR: the course was played to the end. On a speed course, the time
 * taken; on the others, the bonus and the score, then either Vibri's
 * comment or, from the score that passes, the medal and the course's score
 * said step by step. Then, if it is a high score, the high scores.
 */
int TitleDraw::DrawClear(int jitter)
{
    /* how the comment sign shows each comment, and Vibri's movie for each */
    static const int commentSigns[] = { 1, 0, 2, 3 };
    static const int commentMovies[] = { 8, 9, 11, 10 };
    /* the steps of the medal sequence: their camera, their second movie (-1: none) */
    static const int stepCameras[] = { 3, 4, 5, 6, 7, 11, 12, 8, 9, 10, -1 };
    static const int stepMovies2[] = { 93, 93, 93, 93, 94, 94, 94, -1, 93, 93, -1 };
    static const int medalSounds[][12] = {
        { 48, 48, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 48, 48, 51, 52, -1, 0, 0, 0, 0, 0, 0, 0 },
        { 48, 48, 49, 50, 48, 48, 51, 52, -1, 0, 0, 0 },
    };
    static const int courseNameSounds[][4] = {
        { 36, 53, 54, -1 },
        { 35, 53, 54, -1 },
        { 34, 53, 54, -1 },
        { 38, 39, 54, -1 },
        { 37, 39, 54, -1 },
    };
    /* the courses' names, in each language */
    static const char* courseNames[NUM_LANGUAGES][5] = {
        { "Bronze", "Silber", "Gold", "Einzelner Track", "Alle Tracks" },
        { "bronze", "silver", "gold", "Single track", "All tracks" },
        { "bronce", "plata", "oro", "Una canci~bn", "Todas las canciones" },
        { "Bronze", "Argent", "Or", "Sur un morceau", "Sur tous les morceaux" },
        { "bronzo", "argento", "oro", "Canzone singola", "Tutte le canzoni" },
        { "\xB3\xD2\xBA\xB0\xBD", "\xC0\xB9\xBA\xB0\xBD", "\xCF\xC2\xBA\xB0\xBD", "\xBC\xDD\xB8\xDE\xD9\xC4\xD7\xAF\xB8", "\xB5\xB0\xD9\xC4\xD7\xAF\xB8" },
    };
    /* the music played under the score said, by how long the line is (VoiceLine::tempo) */
    static const int scoreSounds[][8] = {
        { 62, 60, 61, -1, 0, 0, 0, 0 },
        { 62, 63, 62, 60, 61, -1, 0, 0 },
        { 62, 63, 62, 63, 62, 60, 61, -1 },
    };
    static const int stepMovies[] = { 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, -1 };
    /* the steps of each medal, bronze to gold */
    static const int medalSteps[][11] = {
        { 0, 4, 5, 6, 7, 9, 10, 0, 0, 0, 0 },
        { 0, 1, 4, 5, 6, 7, 8, 9, 10, 0, 0 },
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
    };
    static const int highScoreSounds[] = { 57, 58, -1 };
    static const int clearSounds[] = { 13, 14, 225, 226, 227, 192, -1 };
    static const int speedSounds[] = { 8, 8, 9, 13, 90, 218, -1 };
    static const int bonusSounds[] = { 193, 207, 194, -1 };
    static const int speedEndSounds[] = { 206, 220, -1 };
    static const int commentSounds[][11] = {
        { 196, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 197, 198, 199, -1, 0, 0, 0, 0, 0, 0, 0 },
        { 200, 201, 202, 203, 204, -1, 0, 0, 0, 0, 0 },
        { 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, -1 },
    };
    static const int models[] = { MODEL_CLEAR_OVER, MODEL_VIBRI, MODEL_CLEAR_SONG };
    /* the bonus, and the score, in each language */
    static const char* bonusTexts[NUM_LANGUAGES][3] = {
        { "Extra Bonus!", "Ja! Gut gemacht!", "Das gibt %d Punkte" },
        { "Extra bonus!", "Yes! Well done!", "That's %d points" },
        { "~mExtra bonus!", "~mS~T! ~mBien hecho!", "Eso son %d puntos" },
        { "Extra bonus!", "Oui! Bien jou~M!", "~Ia fait %d points" },
        { "Bonus extra!", "Si! Ben fatto!", "Questo vale %d punti" },
        { "\xCE\xDE\xB0\xC5\xBD\xA4\xCC\xDF\xD7\xBD!", "\xD4\xAF\xC0\xC8!", "%d \xC3\xDD \xC0\xDE\xD6" },
    };
    /* Vibri's comments when the score does not pass */
    static const char* comments[NUM_LANGUAGES][4] = {
        { "L~G~Ht zu w~jnschen ~jbrig!", "Ziemlich gut, oder?", "Vibri ist gl~jcklich.", "Yeah! Yeah! Yeah!" },
        { "Way to go!", "Pretty good, huh?", "Vibri's happy.", "Yeah! Yeah! Yeah!" },
        { "~mV~Ta libre!", "No est~F mal, ~neh?", "Vibri est~F contento.", "~mS~T! ~nS~T! ~mS~T!" },
        { "C'est le rythme ~D suivre!", "Super, hein?", "Vibri est ravi.", "Ouais! Ouais! Ouais!" },
        { "Pronti? Via!", "Molto bene, eh?", "Vibri ~N contento.", "Si! Si! Si!" },
        { "\xBD\xAF\xBA\xDE\xB2!", "\xC4\xDE\xB3\xC0\xDE\xA4\xB4\xAF\xCD\xDD!", "\xCB\xDE\xCC\xDE\xD8\xA4\xB3\xB7\xB3\xB7", "\xD4\xAF\xC0\xA4\xD4\xAF\xC0\xA4\xD4\xAF\xC0" },
    };
    /* the texts of the steps of the medal sequence */
    static const char* stepTexts[NUM_LANGUAGES][10] = {
        { "Gut gemacht. Weiter geht's!", "Gro~Hartig! Gl~jckwunsch.", "Gut gemacht. Weiter geht's!", "Gro~Hartig! Gl~jckwunsch.", "Die %s-Parcourse Bestleistung \nist", "%d Punkte", "%d Punkte", "Oh ja, es ist ein Rekord.", "Gro~Hartig! Gl~jckwunsch.", "Jippiiee!" },
        { "Well done. Way to go!", "Nice one! Congratulations.", "Well done. Way to go!", "Nice one! Congratulations.", "The %s course score is", "%d points", "%d points", "And yes! It's a high score.", "Nice one! Congratulations.", "Yippee!" },
        { "Bien hecho. ~mV~Ta libre!", "~mNo est~F mal! Enhorabuena.", "Bien hecho. ~mV~Ta libre!", "~mNo est~F mal! Enhorabuena.", "La puntuaci~bn del \ncircuito %s es", "%d puntos", "%d puntos", "~mS~T! Es una puntuacti~bn alta.", "~mNo est~F mal! Enhorabuena.", "~mYupiiii!" },
        { "Bien jou~M! C'est le bon rythme!", "H~M h~M super! F~Mlicitations!", "Bien jou~M! C'est le rythme ~D suivre!", "Bravo!! F~Mlicitations.", "Pour %s, le score est", "de %d points", "de %d points", "H~M! C'est un des meilleurs scores!", "Epatant! F~Mlicitations.", "Youpi!" },
        { "Ben fatto. Pronti? Via!", "Bella questa! Complimenti.", "Ben fatto. Pronti? Via!", "Bella questa! Complimenti.", "Il punteggio di rotta %s ~N", "%d punti", "%d punti", "E si! ~K un punteggio elevato.", "Bella questa! Complimenti.", "Yippee!" },
        { "\xD4\xAF\xC0\xC8\xA4\xD4\xAF\xC0\xC8", "\xD6\xB6\xAF\xC0\xC8\xA4\xB5\xD2\xC3\xDE\xC4\xB3", "\xD4\xAF\xC0\xC8\xA4\xD4\xAF\xC0\xC8", "\xD6\xB6\xAF\xC0\xC8\xA4\xB5\xD2\xC3\xDE\xC4\xB3", "%s \xC9\xC4\xB8\xC3\xDD\xCA", "%d \xC3\xDD\xC0\xDE\xAF\xC0\xD6", "%d \xC3\xDD\xC0\xDE\xAF\xC0\xD6", "\xBC\xB6\xD3\xCA\xB2\xBD\xBA\xB1!", "\xD6\xB6\xAF\xC0\xC8\xA4\xB5\xD2\xC3\xDE\xC4\xB3", "\xB2\xB4\xB2!" },
    };
    static const int speedMovies[] = { 35, 36, 37, 38 };
    /* the time taken to reach the target speed (Japanese: shown differently) */
    static const char* speedTexts[NUM_LANGUAGES][3] = {
        { "%d Sekunden ben~ctigt, um angepeiltes", "\nZiel zu erreichen.", NULL },
        { "It took %d seconds to reach", "\nthe target speed.", NULL },
        { "Te llevo %d segundos para alcanzar tu", "\nvelocidad objectivo.", NULL },
        { "Vitesse cible atteinte en %d secondes.", "\n", NULL },
        { "Ci sono volute %d secondi per arrivare", "\ndalla tua velocit~D ideale.", NULL },
        { "\xD3\xB8\xCB\xAE\xB3\xBD\xCB\xDF\xB0\xC4\xDE\xCF\xC3\xDE", "%d \xCB\xDE\xAE\xB3", "\xC0\xDE\xAF\xC0\xD6" },
    };
    /* the scores of each course that pass, and that win silver and gold */
    static const int medalScores[][3] = {
        { 2000, 4000, 6000 },
        { 2000, 4000, 6000 },
        { 1500, 3500, 5500 },
        { 2500, 4500, 6500 },
        { 20000, 35000, 50000 },
    };
    char text[32];
    int language = LanguageSelect::GetLanguage();
    int score;
    if (Title::s_speedCourse) {
        score = Title::s_seconds;
    } else {
        score = Title::s_score + Title::s_bonus;
    }

    if (Title::s_arrived) {
        NumberVoice::Say(&s_voiceLine, score, Title::s_speedCourse);
        for (int i = 0; i < NUM_CAMERAS; i++) {
            s_cameras[i]->Rewind();
        }
        ResetSwaps(s_scoreDigits, NUM_DIGITS);
        ResetSwaps(s_bonusDigits, NUM_DIGITS);
        ResetSwaps(s_totalDigits, NUM_DIGITS);
        ResetSwaps(s_songTotalDigits, NUM_DIGITS);
        ResetSwaps(s_marks, ScoreMeter::NUM_MARKS);
        PickComment();
        /* the sign shows the rank's comment; the movie, voice and text show the one just picked */
        g_commentSign->Show(commentSigns[Title::s_comment]);
        g_frame = 0;
        g_waitFrame = 0;
        g_repeat = 0;
        g_hiscoreFrame = 0;
        g_leaveFrame = 0;
        RewindMovies();
        s_music.Stop();
        if (Title::s_speedCourse) {
            g_speedMeterSign->Show(Title::s_course - COURSE_SPEED_1);
            s_voices.Play(speedSounds, 64);
            ShowNumber(s_scoreDigits, Title::s_seconds, 1);
        } else {
            s_voices.Play(clearSounds, 64);
            ShowNumber(s_scoreDigits, Title::s_score, 0);
            ShowMarks(s_marks);
            g_passScore = medalScores[Title::s_course][0];
            if (score >= medalScores[Title::s_course][2]) {
                g_medal = 2;
            } else if (score >= medalScores[Title::s_course][1]) {
                g_medal = 1;
            } else if (score >= g_passScore) {
                g_medal = 0;
            }
        }
        ShowNumber(s_bonusDigits, Title::s_bonus, 0);
        ShowNumber(s_totalDigits, score, 0);
        ShowNumber(s_songTotalDigits, score, 0);
        g_entering = 0;
        g_leaving = 0;
        g_menuMovie = 0;
        g_medalShown = 0;
    }
    int leaveJitter = g_leaveFrame * 300 * ONE;
    int modelJitter = s_menuJitter.Raw() + leaveJitter + (s_offset.m_value << 12);
    if (Title::s_fade == Title::FADE_OUT) {
        modelJitter += jitter;
        s_music.Stop();
    }
    for (int i = 0; i < 3; i++) {
        SetModelJitter(models[i], modelJitter, s_passes);
    }
    /* a button: on to the high scores, or away */
    if (ReadConfirm() && Title::s_fade == Title::FADE_NONE && !g_hiscoreFrame && !g_leaveFrame) {
        s_music.Stop();
        s_voices.Play(1, 90);
        if (Title::s_rank == 0) {
            return Title::RESULT_RESULTS_DONE;
        }
        g_leaveFrame = 1;
    }
    if (g_leaveFrame) {
        g_leaveFrame++;
        if (g_leaveFrame == Frames(20)) {
            g_hiscoreFrame = 1;
            g_leaveFrame = 0;
        }
    }
    if (!g_hiscoreFrame) {
        LookAlongZ(0.0, 0.0, -187.5, 0.0, 2.5);
        if (Title::s_speedCourse) {
            /* the time taken to reach the target speed */
            SetModelJitter(MODEL_SPEED_METER, modelJitter, s_passes);
            if (Frames(270) < g_frame && language != LANGUAGE_JAPANESE) {
                sprintf(text, speedTexts[language][0], Title::s_seconds);
                DrawText(text, leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                DrawText(speedTexts[language][1], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
            }
            LookAlongZ(0.0, 0.0, -187.5, 0.0, 2.5);
            s_movies[95]->Draw();
            if (g_frame >= Frames(85)) {
                s_movies[97]->Draw();
                s_movies[73]->Draw();
            }
            if (g_frame <= Frames(84)) {
                s_movies[32]->Draw();
                s_movies[96]->Draw();
            } else if (g_frame <= Frames(270)) {
                s_movies[33]->Draw();
            } else if (g_frame <= Frames(360)) {
                s_movies[34]->Draw();
                if (language == LANGUAGE_JAPANESE) {
                    DrawText(speedTexts[LANGUAGE_JAPANESE][0], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                }
            }
            if (g_frame == Frames(366)) {
                s_music.Play(s_voiceLine.clips, 64);
            }
            if (!g_menuMovie && Frames(372) < g_frame && !s_music.IsPlaying()) {
                s_music.Play(speedEndSounds, 64);
                g_menuMovie++;
            }
            if (g_frame >= Frames(361)) {
                s_movies[speedMovies[g_menuMovie]]->Draw();
                if (language == LANGUAGE_JAPANESE) {
                    if (g_menuMovie == 0 || g_menuMovie == 1) {
                        sprintf(text, speedTexts[LANGUAGE_JAPANESE][1], Title::s_seconds);
                        DrawText(text, leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                    }
                    if (g_menuMovie == 2) {
                        sprintf(text, speedTexts[LANGUAGE_JAPANESE][2], Title::s_seconds);
                        DrawText(text, leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                    }
                }
                if (s_movies[speedMovies[g_menuMovie]]->m_finished) {
                    if (g_menuMovie == 3) {
                        if (Title::s_rank != 0) {
                            if (!g_leaveFrame) {
                                g_leaveFrame = 1;
                            }
                            return 0;
                        }
                        return Title::RESULT_RESULTS_DONE;
                    }
                    g_menuMovie++;
                }
            }
        } else {
            if (g_frame == Frames(360)) {
                s_music.Play(s_voiceLine.clips, 64);
            }
            if (g_frame < Frames(112)) {
                s_movies[67]->Draw();
            } else if (!g_medalShown) {
                s_movies[101]->Draw();
            }
            if (g_frame < Frames(360)) {
                /* the bonus */
                s_movies[100]->Draw();
                if (g_frame >= Frames(320)) {
                    DrawText(bonusTexts[language][1], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                } else if (g_frame >= Frames(190)) {
                    DrawText(bonusTexts[language][0], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                }
            } else if (!g_leaving && !g_medalShown) {
                /* the score counted, then whether it passes */
                LookAlongZ(0.0, 0.0, -187.5, 0.0, 2.5);
                if (!g_entering) {
                    if (Frames(362) < g_frame && !s_music.IsPlaying()) {
                        g_entering = 1;
                        s_music.Play(bonusSounds, 64);
                    }
                }
                if (g_entering) {
                    s_movies[7]->Draw();
                    if (s_movies[7]->m_finished && Wait(40)) {
                        if (score < g_passScore) {
                            g_leaving = 1;
                            s_voices.Play(commentSounds[g_pickedComment], 64);
                        } else {
                            g_medalShown = 1;
                            s_voices.Play(medalSounds[g_medal], 64);
                        }
                        g_entering = 0;
                    }
                } else {
                    s_movies[6]->Draw();
                }
                sprintf(text, bonusTexts[language][2], Title::s_score + Title::s_bonus);
                DrawText(text, leaveJitter + jitter, -1800.0, 1050.0, 1.0);
            } else if (g_leaving) {
                /* the score did not pass: Vibri's comment */
                LookAlongZ(0.0, 0.0, -187.5, 0.0, 2.5);
                s_movies[commentMovies[g_pickedComment]]->Draw();
                DrawText(comments[language][g_pickedComment], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                if (s_movies[commentMovies[g_pickedComment]]->m_finished && Wait(30)) {
                    if (Title::s_rank != 0) {
                        g_leaveFrame = 1;
                    } else {
                        return Title::RESULT_RESULTS_DONE;
                    }
                }
            } else {
                /* the medal, and the score said */
                if (Frames(30) < g_waitFrame) {
                    g_waitFrame = 0;
                    if (Title::s_rank == 0) {
                        return Title::RESULT_RESULTS_DONE;
                    }
                    g_leaveFrame = 1;
                }
                int step = medalSteps[g_medal][g_menuMovie];
                int movie = stepMovies[step];
                int movie2 = stepMovies2[step];
                int camera = stepCameras[step];
                const char* stepText = stepTexts[language][step];
                s_cameras[camera]->Update(0);
                s_movies[movie]->Draw();
                if (movie2 > 0) {
                    s_movies[movie2]->Draw();
                }
                if (movie == 26) {
                    sprintf(text, stepText, courseNames[language][Title::s_course]);
                    DrawText(text, leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                } else if (movie == 27 || movie == 28) {
                    sprintf(text, stepText, score);
                    DrawText(text, leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                } else {
                    DrawText(stepText, leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                }
                if (s_movies[movie]->m_finished) {
                    if (movie == 26) {
                        s_voices.Play(s_voiceLine.timeline, 64);
                        s_music.Play(scoreSounds[s_voiceLine.tempo], 64);
                    }
                    if (movie == 27 && g_repeat < s_voiceLine.tempo - 1) {
                        /* the score is counted again while it is said */
                        s_movies[27]->Rewind();
                        g_repeat++;
                    } else if (movie == 26) {
                        if (s_voiceLine.tempo != 0) {
                            g_menuMovie++;
                            s_movies[26]->Rewind();
                        } else {
                            g_menuMovie += 2;
                        }
                    } else if (movie == 28) {
                        /* the high score's step only when there is one */
                        g_menuMovie += Title::s_rank != 0 ? 1 : 2;
                    } else {
                        int next = g_menuMovie + 1;
                        int nextMovie = stepMovies[medalSteps[g_medal][next]];
                        if (nextMovie != -1) {
                            g_menuMovie = next;
                            s_movies[nextMovie]->Rewind();
                        } else {
                            g_waitFrame++;
                        }
                    }
                    int newMovie = stepMovies[medalSteps[g_medal][g_menuMovie]];
                    if (newMovie == 26) {
                        s_voices.Play(courseNameSounds[Title::s_course], 64);
                    } else if (newMovie == 31 && !g_waitFrame) {
                        s_voices.Play(59, 90);
                    } else if (newMovie == 30) {
                        s_voices.Play(highScoreSounds, 64);
                    } else if (newMovie == 29) {
                        s_voices.Play(56, 90);
                    }
                }
            }
        }
    } else {
        s_music.Stop();
        return DrawPsjHiscore(jitter, 0, g_hiscoreFrame++);
    }
    g_frame++;
    return 0;
}

/*
 * AGAIN: the course was lost. Vibri's comment, said and shown, and the
 * score (when a speed course's time ran out, "time up" instead); then, if
 * it is a high score, the high scores.
 */
int TitleDraw::DrawAgain(int jitter)
{
    static const int commentSounds[][15] = {
        { 12, 176, 177, 179, 180, 181, 180, 181, -1, 0, 0, 0, 0, 0, 0 },
        { 12, 176, 177, 184, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 12, 176, 177, 185, 186, 187, 188, 189, 190, 190, 190, 190, 191, -1, 0 },
        { 12, 176, 177, 183, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    };
    static const int commentMovies[] = { 12, 16, 18, 15 };
    static const int timeUpSounds[] = { 9, 12, 176, 182, -1 };
    /* what Vibri says when a course is lost, in each language: the comments, and the third one's punch line */
    static const char* failComments[NUM_LANGUAGES][5] = {
        { "Waah! Waah!", "Wir brauchen mehr ~fbung!", "Vibri kann nicht mehr...", "Grrrrrrrggh!!", "nur ein Witz!" },
        { "Waah! Waah!", "We need more practice!", "Vibri can't go on...", "Grrrrrrrggh!!", "only joking!" },
        { "~mBuaah! ~mBuaah!", "~mNecesitamos practicar!", "Vibri ya no puede m~Fs...", "~m~mGrrrrrrrggh!!", "~mes una broma!" },
        { "Ouin! Ouin!", "Un peu plus d'entra~Rnement?", "Vibri n'en peut plus...", "Grrrrh!", "Poisson d'Avril!!!" },
        { "Waah! Waah!", "Ci servano un p~a pi~g di pratica!", "Vibri non pu~a proseguire...", "Grrrrrrrggh!!", "sto scherzando!" },
        { "\xCB\xDE\xB4\xB0\xDD\xA4\xB4\xB0\xDD\xA4\xB4\xB0\xDD", "\xB7\xC0\xB4\xC5\xB5\xBB\xC5\xB7\xAC", "\xCB\xDE\xCC\xDE\xD8\xA4\xD3\xB3\xC0\xDE\xD2", "\xD1\xB7\xB0!", "\xC5\xDD\xC1\xAC\xAF\xC3" },
    };
    static const int models[] = { MODEL_CLEAR_OVER, MODEL_VIBRI };
    /* "game over" and "time up" (a speed course), in each language */
    static const char* endTexts[NUM_LANGUAGES][2] = {
        { "Spiel zu Ende", "Zeit zu Ende" },
        { "Game Over", "time up" },
        { "Fin del juego", "se acab~b el tiempo" },
        { "Fin du jeu", "temps ~Mcoul~M." },
        { "Fine", "tempo esaurito" },
        { "\xB9\xDE\xB0\xD1\xB5\xB0\xCA\xDE\xB0", "\xBC\xDE\xB6\xDD\xB7\xDE\xDA" },
    };
    int language = LanguageSelect::GetLanguage();

    if (Title::s_arrived) {
        ResetSwaps(s_scoreDigits, NUM_DIGITS);
        ResetSwaps(s_marks, ScoreMeter::NUM_MARKS);
        PickComment();
        g_hiscoreFrame = 0;
        g_leaveFrame = 0;
        g_frame = 0;
        g_waitFrame = 0;
        RewindMovies();
        s_music.Stop();
        if (!Title::s_speedCourse || !Title::s_timeUp) {
            s_voices.Play(commentSounds[g_pickedComment], 64);
            ShowNumber(s_scoreDigits, Title::s_score, 0);
            ShowMarks(s_marks);
        } else {
            s_voices.Play(timeUpSounds, 64);
        }
    }
    int leaveJitter = g_leaveFrame * 300 * ONE;
    int modelJitter = s_menuJitter.Raw() + leaveJitter + (s_offset.m_value << 12);
    if (Title::s_fade == Title::FADE_OUT) {
        modelJitter += jitter;
    }
    for (int i = 0; i < 2; i++) {
        SetModelJitter(models[i], modelJitter, s_passes);
    }
    /* a button: on to the high scores, or away */
    if (ReadConfirm() && Title::s_fade == Title::FADE_NONE && !g_hiscoreFrame && !g_leaveFrame) {
        s_voices.Play(1, 90);
        if (Title::s_rank == 0) {
            return Title::RESULT_RESULTS_DONE;
        }
        g_leaveFrame = 1;
    }
    if (g_leaveFrame) {
        g_leaveFrame++;
        if (g_leaveFrame == Frames(20)) {
            g_hiscoreFrame = 1;
            g_leaveFrame = 0;
        }
    }
    if (!g_hiscoreFrame) {
        if (Title::s_speedCourse && Title::s_timeUp) {
            /* time is up: the scribble, then Vibri, shaking less and less, and "game over", then "time up" */
            if (g_frame >= Frames(193) && g_frame <= Frames(258)) {
                SetModelJitter(MODEL_VIBRI, (Frames(258) - g_frame) * 10 * ONE, s_passes);
            } else {
                SetModelJitter(MODEL_VIBRI, modelJitter, s_passes);
            }
            SetModelJitter(MODEL_SPEED_SCRIBBLE, modelJitter + 30 * ONE, s_passes);
            LookAlongZ(0.0, 0.0, -187.5, 0.0, 2.5);
            if (g_frame <= Frames(192)) {
                s_movies[98]->Draw();
            } else if (g_frame <= Frames(258)) {
                s_movies[99]->Draw();
                s_movies[20]->Draw();
                s_movies[72]->Draw();
                DrawText(endTexts[language][0], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
            } else {
                s_movies[72]->Draw();
                s_movies[21]->Draw();
                DrawText(endTexts[language][1], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
            }
            if (g_frame++ == Frames(390)) {
                if (Title::s_rank == 0) {
                    return Title::RESULT_RESULTS_DONE;
                }
                g_leaveFrame = 1;
            }
            return 0;
        }

        LookAlongZ(0.0, 0.0, -187.5, 0.0, 2.5);
        if (!Title::s_speedCourse) {
            if (g_frame < Frames(100)) {
                s_movies[68]->Draw();
            } else if (g_frame < Frames(112)) {
                s_movies[69]->Draw();
            } else {
                s_movies[70]->Draw();
            }
        }
        if (g_frame >= Frames(140)) {
            s_movies[71]->Draw();
        }
        if (g_frame < Frames(144)) {
            s_movies[19]->Draw();
        } else {
            if (g_pickedComment == 0) {
                if (g_frame < Frames(212)) {
                    s_movies[commentMovies[g_pickedComment]]->Draw();
                } else {
                    s_movies[13]->Draw();
                }
                if (g_frame >= Frames(188)) {
                    s_movies[14]->Draw();
                }
            } else if (g_pickedComment == 1) {
                if (g_frame < Frames(200)) {
                    s_movies[commentMovies[g_pickedComment]]->Draw();
                } else {
                    s_movies[17]->Draw();
                }
            } else {
                s_movies[commentMovies[g_pickedComment]]->Draw();
            }
            if (Frames(200) < g_frame) {
                if (g_pickedComment == 2) {
                    if (g_frame < Frames(360)) {
                        DrawText(failComments[language][g_pickedComment], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                    } else {
                        DrawText(failComments[language][4], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                    }
                } else {
                    DrawText(failComments[language][g_pickedComment], leaveJitter + jitter, -1800.0, 1050.0, 1.0);
                }
            }
        }
        if (!g_waitFrame) {
            g_frame++;
        }
        if (g_frame == Frames(480)) {
            if (Title::s_rank != 0) {
                g_leaveFrame = 1;
                return 0;
            }
            return Title::RESULT_RESULTS_DONE;
        }
    } else {
        s_music.Stop();
        return DrawPsjHiscore(jitter, 0, g_hiscoreFrame++);
    }
    return 0;
}

int TitleDraw::DrawAgainSelect(int jitter, int still)
{
    /* how the two signs show the course played */
    static const int courseSign[] = { 0, 1, 2, 3, 4, 6, 6, 6, 5 };
    static const int speedSign[] = { 0, 0, 0, 0, 0, 1, 2, 3, 0 };
    static const int upMovies[] = { 59, 60 };
    static const int downMovies[] = { 60, 59 };
    static const int sounds[] = { 99, 100 };

    if (Title::s_arrived) {
        g_againCourseSign->Show(courseSign[Title::s_course]);
        g_againSpeedSign->Show(speedSign[Title::s_course]);
        g_menuMovie = 59;
        s_againCursor.x = 0;
        s_againCursor.y = 0;
        s_speed = 0.0;
        RewindMovies();
        g_vibriState = VIBRI_SITTING;
        g_leaving = 0;
        g_cameraCue = 1;
        SetCamera(1);
    }
    s_againCursor.Read(&s_input, 1, 0, still);
    int modelJitter = (s_menuJitter + GteFixed::Int(s_offset.m_value)).Raw();
    if (s_input.up) {
        g_menuMovie = upMovies[s_againCursor.Index()];
        s_movies[g_menuMovie]->Rewind(1);
        s_speed = -s_menuSpeed;
    } else if (s_input.down) {
        g_menuMovie = downMovies[s_againCursor.Index()];
        s_movies[g_menuMovie]->Rewind();
        s_speed = s_menuSpeed;
    }
    /* (| rather than ||, here and in the screens below: the game's code tests both) */
    if ((s_input.up != 0) | (s_input.down != 0)) {
        g_vibriState = VIBRI_SITTING_SPEAKING;
        s_voices.Play(sounds[s_againCursor.Index()], 90);
    }
    if (s_input.back) {
        g_menuMovie = upMovies[1];
        g_vibriState = VIBRI_SITTING_SPEAKING;
        s_movies[g_menuMovie]->Rewind();
        s_voices.Play(sounds[s_againCursor.Index()], 90);
        s_speed = 0.0;
    }
    if (Title::s_next == Title::SCREEN_DONE) {
        modelJitter += jitter;
    }
    int passes = s_passes;
    SetModelJitter(MODEL_AGAIN, modelJitter, passes);
    DrawVibri(modelJitter, passes);
    if (!still && Title::GoesOn()) {
        UpdateCamera(g_cameraCue);
    } else {
        s_cameras[13]->Update(0);
    }
    DrawMovie(g_menuMovie, s_speed);
    if (g_leaving && s_movies[61]->m_finished) {
        g_leaving = 0;
        g_cameraCue = 3;
        SetCamera(3);
        return Title::RESULT_AGAIN_BACK;
    }
    if (s_input.confirm) {
        s_voices.Play(1, 90);
        if (s_againCursor.Index() != 1) {
            return Title::RESULT_AGAIN;
        }
        g_menuMovie = 61;
        g_leaving = 1;
        s_speed = s_menuSpeed;
    }
    return 0;
}

int TitleDraw::DrawMain(int jitter, int still)
{
    static const int results[] = {
        Title::RESULT_PSJ_MODES, Title::RESULT_UNKNOWN_MODES, Title::RESULT_OPTIONS, Title::RESULT_LEAVE,
    };
    static const int sounds[][4] = {
        { 240, 241, 126, SoundSequence::END },
        { 125, 243, 126, SoundSequence::END },
        { 119, SoundSequence::END },
        { 96, SoundSequence::END },
    };
    static const int upMovies[] = { 74, 75, 76, 77 };
    static const int downMovies[] = { 77, 74, 75, 76 };
    int result = 0;

    if (Title::s_arrived) {
        s_voices.Stop();
        if (s_mainCursor.Index() == 3) {
            s_mainCursor.x = 0;
            s_mainCursor.y = 0;
        }
        s_music.Play(32, 90);
        g_vibriState = VIBRI_LYING;
        g_menuMovie = upMovies[s_mainCursor.Index()];
        s_speed = 0.0;
        RewindMovies();
        SetCamera(1);
        g_leaving = 0;
        g_waitFrame = 0;
    }
    s_mainCursor.Read(&s_input, 1, 0, still);
    int modelJitter = s_menuJitter.Raw() + (s_offset.m_value << 12);
    if (Title::s_previous == Title::SCREEN_NONE) {
        modelJitter += jitter;
    }
    if (s_input.changed) {
        if (s_input.up) {
            g_menuMovie = upMovies[s_mainCursor.Index()];
            s_movies[g_menuMovie]->Rewind(1);
            s_speed = -s_menuSpeed;
        } else if (s_input.down) {
            g_menuMovie = downMovies[s_mainCursor.Index()];
            s_movies[g_menuMovie]->Rewind();
            s_speed = s_menuSpeed;
        }
        if ((s_input.up != 0) | (s_input.down != 0)) {
            g_vibriState = VIBRI_LYING_SPEAKING;
            s_voices.Play(sounds[s_mainCursor.Index()], 64);
        }
    }
    if (s_input.back) {
        g_menuMovie = upMovies[3];
        g_vibriState = VIBRI_LYING_SPEAKING;
        s_movies[g_menuMovie]->Rewind();
        s_voices.Play(sounds[3], 64);
        s_speed = 0.0;
    }
    if (g_leaving) {
        modelJitter = g_waitFrame * 300 * ONE;
        if (Wait(20)) {
            g_leaving = 0;
            return Title::RESULT_LEAVE;
        }
    }
    if (s_input.confirm && !g_leaving) {
        s_voices.Play(1, 90);
        int index = s_mainCursor.Index();
        if (index < 3) {
            result = results[index];
            SetCamera(0);
        } else {
            g_leaving = 1;
        }
    } else {
        result = 0;
    }
    int passes = s_passes;
    SetModelJitter(MODEL_MAIN, modelJitter, passes);
    if (Title::s_next != Title::SCREEN_ALERT_VIB_CD_IN && Title::s_next != Title::SCREEN_CD_CHECK) {
        DrawVibri(modelJitter, passes);
    }
    s_cameras[13]->Update(0);
    if (Title::s_fade == Title::FADE_OUT) {
        if (s_mainCursor.Index() != 3) {
            UpdateCamera(0);
        }
    }
    if (Title::s_fade == Title::FADE_IN) {
        if (Title::s_previous != Title::SCREEN_NONE) {
            UpdateCamera(1);
        }
    }
    if (Title::s_next != Title::SCREEN_ALERT_VIB_CD_IN && Title::s_next != Title::SCREEN_CD_CHECK) {
        DrawMovie(g_menuMovie, s_speed);
    }
    return result;
}

int TitleDraw::DrawPsjModeSelect(int jitter, int still)
{
    static const int results[] = {
        Title::RESULT_BRONZE, Title::RESULT_SILVER, Title::RESULT_GOLD,
        Title::RESULT_PSJ_HISCORES, Title::RESULT_PSJ_BACK,
    };
    static const int sounds[] = { 114, 113, 112, 128, 96 };
    static const int upMovies[] = { 78, 79, 80, 81, 82 };
    static const int downMovies[] = { 82, 78, 79, 80, 81 };

    if (Title::s_arrived) {
        g_vibriState = VIBRI_SITTING;
        g_leaving = 0;
        RewindMovies();
        if (Title::s_previous == Title::SCREEN_MAIN) {
            s_psjModeCursor.x = 0;
            s_psjModeCursor.y = 0;
            g_cameraCue = 2;
            g_entering = 1;
        } else {
            g_cameraCue = 1;
            g_entering = 0;
        }
        g_menuMovie = upMovies[s_psjModeCursor.Index()];
        s_speed = 0.0;
        SetCamera(g_cameraCue);
        s_music.Play(32, 90);
    }
    int active = still && !g_entering && !g_leaving;
    s_psjModeCursor.Read(&s_input, 1, 0, active);
    int modelJitter = s_menuJitter.Raw() + (s_offset.m_value << 12);
    if (Title::s_next == Title::SCREEN_DONE) {
        modelJitter += jitter;
    }
    SetModelJitter(MODEL_MODE_PSJ, modelJitter, s_passes);
    if (s_input.changed) {
        if ((s_input.up != 0) | (s_input.down != 0)) {
            g_vibriState = VIBRI_SITTING_SPEAKING;
            s_voices.Play(sounds[s_psjModeCursor.Index()], 90);
        }
        if (s_input.up) {
            g_menuMovie = upMovies[s_psjModeCursor.Index()];
            s_movies[g_menuMovie]->Rewind(1);
            s_speed = -s_menuSpeed;
        } else if (s_input.down) {
            g_menuMovie = downMovies[s_psjModeCursor.Index()];
            s_movies[g_menuMovie]->Rewind();
            s_speed = s_menuSpeed;
        }
    }
    if (s_input.back) {
        g_menuMovie = upMovies[4];
        g_vibriState = VIBRI_SITTING_SPEAKING;
        s_movies[g_menuMovie]->Rewind();
        s_voices.Play(sounds[s_psjModeCursor.Index()], 90);
        s_speed = 0.0;
    }
    DrawVibri(modelJitter, s_passes);
    s_cameras[13]->Update(0);
    int moving = Title::GoesOn() && !still;
    if (moving) {
        UpdateCamera(g_cameraCue);
    }
    if (g_entering) {
        DrawMenuMovie(83);
    } else {
        DrawMovie(g_menuMovie, s_speed);
    }
    if (g_leaving && s_movies[84]->m_finished) {
        g_leaving = 0;
        g_cameraCue = 3;
        SetCamera(3);
        return results[s_psjModeCursor.Index()];
    }
    if (s_input.confirm) {
        s_voices.Play(1, 90);
        if (s_psjModeCursor.Index() != 4) {
            g_cameraCue = 0;
            SetCamera(0);
            return results[s_psjModeCursor.Index()];
        }
        g_menuMovie = 84;
        g_leaving = 1;
        s_speed = s_menuSpeed;
    }
    return 0;
}

int TitleDraw::DrawUnknownModeSelect(int still)
{
    static const int results[] = {
        Title::RESULT_ONE_TRACK, Title::RESULT_ALL_TRACKS, Title::RESULT_SPEED_MODES,
        Title::RESULT_IN_ORDER, Title::RESULT_UNKNOWN_HISCORES, Title::RESULT_UNKNOWN_BACK,
    };
    static const int sounds[][3] = {
        { 115, 120, SoundSequence::END },
        { 117, 120, SoundSequence::END },
        { 116, SoundSequence::END },
        { 118, SoundSequence::END },
        { 128, SoundSequence::END },
        { 96, SoundSequence::END },
    };
    static const int upMovies[] = { 85, 86, 87, 88, 89, 90 };
    static const int downMovies[] = { 90, 85, 86, 87, 88, 89 };

    if (Title::s_arrived) {
        s_voices.Stop();
        s_music.Play(32, 90);
        g_vibriState = VIBRI_SITTING;
        g_leaving = 0;
        RewindMovies();
        if (Title::s_previous == Title::SCREEN_MAIN) {
            s_unknownModeCursor.x = 0;
            s_unknownModeCursor.y = 0;
            g_cameraCue = 2;
            g_entering = 1;
        } else {
            g_cameraCue = 1;
            g_entering = 0;
        }
        SetCamera(g_cameraCue);
        g_menuMovie = upMovies[s_unknownModeCursor.Index()];
        s_speed = 0.0;
    }
    int active = still && !g_entering && !g_leaving;
    s_unknownModeCursor.Read(&s_input, 1, 0, active);
    int modelJitter = s_menuJitter.Raw() + (s_offset.m_value << 12);
    SetModelJitter(MODEL_MODE_UNKNOWN, modelJitter, s_passes);
    if (s_input.changed) {
        if ((s_input.up != 0) | (s_input.down != 0)) {
            g_vibriState = VIBRI_SITTING_SPEAKING;
            s_voices.Play(sounds[s_unknownModeCursor.Index()], 64);
        }
        if (s_input.up) {
            g_menuMovie = upMovies[s_unknownModeCursor.Index()];
            s_movies[g_menuMovie]->Rewind(1);
            s_speed = -s_menuSpeed;
        } else if (s_input.down) {
            g_menuMovie = downMovies[s_unknownModeCursor.Index()];
            s_movies[g_menuMovie]->Rewind();
            s_speed = s_menuSpeed;
        }
    }
    if (s_input.back) {
        g_menuMovie = upMovies[5];
        g_vibriState = VIBRI_SITTING_SPEAKING;
        s_movies[g_menuMovie]->Rewind();
        s_voices.Play(sounds[s_unknownModeCursor.Index()], 64);
        s_speed = 0.0;
    }
    DrawVibri(modelJitter, s_passes);
    if (!still) {
        UpdateCamera(g_cameraCue);
    } else {
        s_cameras[13]->Update(0);
    }
    if (g_entering) {
        DrawMenuMovie(91);
    } else {
        DrawMovie(g_menuMovie, s_speed);
    }
    if (g_leaving && s_movies[92]->m_finished) {
        g_leaving = 0;
        g_cameraCue = 3;
        SetCamera(3);
        return results[s_unknownModeCursor.Index()];
    }
    if (s_input.confirm) {
        s_voices.Play(1, 90);
        if (s_unknownModeCursor.Index() != 5) {
            g_cameraCue = 0;
            SetCamera(0);
            return results[s_unknownModeCursor.Index()];
        }
        g_menuMovie = 92;
        g_leaving = 1;
        s_speed = s_menuSpeed;
    }
    return 0;
}

int TitleDraw::DrawTrackSelect(int jitter, int still)
{
    if (Title::s_arrived) {
        g_vibriState = VIBRI_STANDING;
        s_music.Play(32, 90);
        g_track = 1;
        CallCallback(1);
        s_backCursor.x = 0;
        s_backCursor.y = 0;
        g_menuMovie = 50;
        RewindMovies();
        g_cameraCue = 4;
        SetCamera(4);
        s_speed = 0.0;
        g_leaving = 0;
        g_frame = 0;
        g_waitFrame = 0;
        s_voices.Stop();
        NumberVoice::Say(&s_voiceLine, g_track, 1);
    }
    s_backCursor.Read(&s_input, 0, 0, still);
    int offset = s_offset.m_value << 12;
    int modelJitter = s_menuJitter.Raw() + offset;
    int textJitter = s_textJitter.Raw() + offset;
    if (Title::s_fade == Title::FADE_OUT && s_backCursor.Index() == 0) {
        modelJitter += jitter;
        textJitter += jitter;
    }
    int passes = s_passes;
    DrawVibri(modelJitter, passes);
    SetModelJitter(MODEL_TRACK, modelJitter, passes);
    if (s_backCursor.Index() == 0) {
        if (s_input.right) {
            if (Title::s_numTracks < ++g_track) {
                g_track = 1;
            }
            CallCallback(g_track);
            NumberVoice::Say(&s_voiceLine, g_track, 1);
            s_voices.Play(s_voiceLine.clips, 64);
            g_vibriState = VIBRI_STANDING_SPEAKING;
            TmdLineObj* arrow = s_models[MODEL_TRACK]->m_objects[1];
            arrow->SetJitter(Fixed::FromRaw(textJitter << 4), s_passes2);
            g_menuMovie = 51;
            s_movies[51]->Rewind();
            s_speed = s_menuSpeed;
        } else if (s_input.left) {
            if (--g_track == 0) {
                g_track = Title::s_numTracks;
            }
            CallCallback(g_track);
            NumberVoice::Say(&s_voiceLine, g_track, 1);
            s_voices.Play(s_voiceLine.clips, 64);
            g_vibriState = VIBRI_STANDING_SPEAKING;
            TmdLineObj* arrow = s_models[MODEL_TRACK]->m_objects[0];
            arrow->SetJitter(Fixed::FromRaw(textJitter << 4), s_passes2);
            g_menuMovie = 50;
            s_movies[50]->Rewind();
            s_speed = s_menuSpeed;
        }
    }
    if (s_input.back) {
        g_vibriState = VIBRI_STANDING_SPEAKING;
        g_menuMovie = 52;
        s_movies[52]->Rewind();
        s_speed = s_menuSpeed;
        s_voices.Play(96, 90);
    }
    if (s_input.changed) {
        if (s_input.up) {
            g_vibriState = VIBRI_STANDING_SPEAKING;
            g_menuMovie = 52;
            s_voices.Play(s_voiceLine.clips, 64);
            s_movies[g_menuMovie]->Rewind(1);
            s_speed = -s_menuSpeed;
        }
        if (s_input.down) {
            g_vibriState = VIBRI_STANDING_SPEAKING;
            g_menuMovie = 52;
            s_movies[52]->Rewind();
            s_speed = s_menuSpeed;
            s_voices.Play(96, 90);
        }
    }
    if (s_backCursor.Index() != 0) {
        TmdLineObj* back = s_models[MODEL_TRACK]->m_objects[2];
        back->SetJitter(Fixed::FromRaw(textJitter << 4), s_passes2);
    }
    UpdateCamera(g_cameraCue);
    DrawMovie(g_menuMovie, s_speed);

    /* the track's number, bigger and shaking while it is the item chosen */
    char text[32];
    GteFixed x = GteFixed::Int(g_track >= 10 ? 70 : 170);
    int numberJitter = GteFixed::Int(s_backCursor.Index() == 0 ? 5 : 0).Raw();
    if (Title::s_fade == Title::FADE_OUT && s_backCursor.Index() == 0) {
        numberJitter += jitter;
    }
    GteFixed scale(s_backCursor.Index() == 0 ? 2.8 : 1.8);
    sprintf(text, "%d", g_track);
    Font::SetPosition(x, GteFixed());
    Font::SetScale(scale);
    Font::Draw(text, numberJitter);

    if (g_leaving && Wait(10)) {
        g_cameraCue = 5;
        SetCamera(5);
        g_leaving = 0;
    }
    if (s_input.confirm) {
        s_voices.Play(1, 90);
        if (s_backCursor.Index() != 0) {
            g_leaving = 1;
            return Title::RESULT_TRACK_BACK;
        }
        return Title::RESULT_TRACK_CHOSEN;
    }
    return 0;
}

int TitleDraw::DrawModeSpeedSelect(int still)
{
    static const int sounds[] = { 114, 113, 112, 96 };
    static const int results[] = {
        Title::RESULT_SPEED_1, Title::RESULT_SPEED_2, Title::RESULT_SPEED_3, Title::RESULT_SPEED_BACK,
    };
    static const int upMovies[] = { 39, 40, 41, 42 };
    static const int downMovies[] = { 42, 39, 40, 41 };
    int modelJitter = s_menuJitter.Raw() + (s_offset.m_value << 12);

    if (Title::s_arrived) {
        SetModelJitter(MODEL_SPEED, modelJitter, s_passes);
        if (Title::s_previous == Title::SCREEN_UNKNOWN_MODE_SELECT) {
            s_speedCursor.x = 0;
            s_speedCursor.y = 0;
            g_cameraCue = 2;
            g_entering = 1;
        } else {
            g_cameraCue = 1;
            g_entering = 0;
        }
        g_vibriState = VIBRI_LYING;
        s_music.Play(32, 90);
        g_menuMovie = upMovies[s_speedCursor.Index()];
        RewindMovies();
        SetCamera(g_cameraCue);
        s_speed = 0.0;
        g_leaving = 0;
    }
    int active = still && !g_entering && !g_leaving;
    s_speedCursor.Read(&s_input, 1, 0, active);
    if (s_input.changed) {
        if ((s_input.up != 0) | (s_input.down != 0)) {
            s_voices.Play(sounds[s_speedCursor.Index()], 90);
            g_vibriState = VIBRI_LYING_SPEAKING;
        }
        if (s_input.up) {
            g_menuMovie = upMovies[s_speedCursor.Index()];
            s_movies[g_menuMovie]->Rewind(1);
            s_speed = -s_menuSpeed;
        } else if (s_input.down) {
            g_menuMovie = downMovies[s_speedCursor.Index()];
            s_movies[g_menuMovie]->Rewind();
            s_speed = s_menuSpeed;
        }
    }
    if (s_input.back) {
        g_vibriState = VIBRI_LYING_SPEAKING;
        g_menuMovie = upMovies[s_speedCursor.Index()];
        s_movies[g_menuMovie]->Rewind();
        s_voices.Play(sounds[s_speedCursor.Index()], 90);
        s_speed = 0.0;
    }
    DrawVibri(modelJitter, s_passes);
    if (!still) {
        UpdateCamera(g_cameraCue);
    } else {
        s_cameras[13]->Update(0);
    }
    if (g_entering) {
        DrawMenuMovie(43);
    } else {
        DrawMovie(g_menuMovie, s_speed);
    }
    if (g_leaving && s_movies[44]->m_finished) {
        g_leaving = 0;
        g_cameraCue = 3;
        SetCamera(3);
        return results[s_speedCursor.Index()];
    }
    if (s_input.confirm) {
        s_voices.Play(1, 90);
        if (s_speedCursor.Index() != 3) {
            g_cameraCue = 0;
            SetCamera(0);
            return results[s_speedCursor.Index()];
        }
        g_menuMovie = 44;
        g_leaving = 1;
        s_speed = s_menuSpeed;
    }
    return 0;
}

/* OPTION: "Sound effects" and "Vibration", each on or off, then "Back" */
int TitleDraw::DrawOption(int still)
{
    /* said on each item */
    static const int sounds[] = { 129, 130, 96 };
    static const int upMovies[] = { 54, 55, 56 };
    static const int downMovies[] = { 56, 54, 55 };
    int modelJitter = s_menuJitter.Raw() + (s_offset.m_value << 12);

    if (Title::s_arrived) {
        g_vibriState = VIBRI_SITTING;
        g_leaving = 0;
        /* ON is the model's own object, OFF the one shown in its place */
        g_soundEffectsSwap->Show(Title::s_soundEffects ^ 1);
        g_vibrationSwap->Show(Title::s_vibration ^ 1);
        s_music.Play(32, 90);
        g_entering = 1;
        g_menuMovie = 54;
        s_optionCursor.x = 0;
        s_optionCursor.y = 0;
        RewindMovies();
        g_cameraCue = 2;
        SetCamera(2);
        s_speed = 0.0;
    }
    int active = still && !g_entering && !g_leaving;
    s_optionCursor.Read(&s_input, 1, 0, active);
    int passes = s_passes;
    DrawVibri(modelJitter, passes);
    SetModelJitter(MODEL_OPTION, modelJitter, passes);
    if (s_input.changed) {
        if ((s_input.up != 0) | (s_input.down != 0)) {
            s_voices.Play(sounds[s_optionCursor.Index()], 90);
            g_vibriState = VIBRI_SITTING_SPEAKING;
        }
        if (s_input.up) {
            g_menuMovie = upMovies[s_optionCursor.Index()];
            s_movies[g_menuMovie]->Rewind(1);
            s_speed = -s_menuSpeed;
        } else if (s_input.down) {
            g_menuMovie = downMovies[s_optionCursor.Index()];
            s_movies[g_menuMovie]->Rewind();
            s_speed = s_menuSpeed;
        }
    }
    if (s_input.back) {
        g_vibriState = VIBRI_SITTING_SPEAKING;
        g_menuMovie = upMovies[s_optionCursor.Index()];
        s_movies[g_menuMovie]->Rewind();
        s_voices.Play(sounds[s_optionCursor.Index()], 90);
        s_speed = 0.0;
    }
    if (!still) {
        UpdateCamera(g_cameraCue);
    } else {
        s_cameras[13]->Update(0);
    }
    if (g_entering) {
        DrawMenuMovie(57);
    } else {
        DrawMovie(g_menuMovie, s_speed);
    }
    if (g_leaving && s_movies[58]->m_finished) {
        g_leaving = 0;
        g_cameraCue = 3;
        SetCamera(3);
        return Title::RESULT_OPTION_BACK;
    }
    if (s_input.confirm) {
        /* said for off and for on */
        static const int onOff[] = { 132, 131 };
        if (s_optionCursor.Index() == 0 || s_optionCursor.Index() == 1) {
            g_vibriState = VIBRI_SITTING_SPEAKING;
            if (s_optionCursor.Index() == 0) {
                Title::s_soundEffects = Title::s_soundEffects == 1 ? 0 : 1;
                s_voices.Play(onOff[Title::s_soundEffects], 90);
            }
            if (s_optionCursor.Index() == 1) {
                Title::s_vibration = Title::s_vibration == 1 ? 0 : 1;
                s_voices.Play(onOff[Title::s_vibration], 90);
            }
            g_soundEffectsSwap->Show(Title::s_soundEffects ^ 1);
            g_vibrationSwap->Show(Title::s_vibration ^ 1);
            if (Title::s_optionsCallback != NULL) {
                Title::s_optionsCallback(Title::s_soundEffects, Title::s_vibration);
            }
        } else {
            s_voices.Play(1, 90);
        }
        if (s_optionCursor.Index() == 2) {
            g_menuMovie = 58;
            g_leaving = 1;
            s_speed = s_menuSpeed;
        }
    }
    return 0;
}

int TitleDraw::DrawCdChange(int jitter, int still)
{
    static const int results[] = { Title::RESULT_CHANGE_CD, Title::RESULT_CD_CHANGED, Title::RESULT_CD_CHANGE_BACK };
    static const int upMovies[] = { 45, 46, 47 };
    static const int downMovies[] = { 47, 45, 46 };
    static const int sounds[][3] = {
        { 246, 247, SoundSequence::END },
        { 246, 248, SoundSequence::END },
        { 96, SoundSequence::END },
    };
    static const int cameraCues[] = { 0, 0, 3 };

    if (Title::s_arrived) {
        g_waitFrame = 0;
        g_vibriState = VIBRI_SITTING;
        s_music.Play(32, 90);
        s_speed = 0.0;
        g_leaving = 0;
        RewindMovies();
        if (Title::s_previous == Title::SCREEN_TRACK_SELECT || Title::s_previous == Title::SCREEN_ALERT_AUDIO_CD_IN) {
            /* back from the track or the alert: on the item chosen, not on the back item */
            g_cameraCue = 1;
            if (s_cdChangeCursor.Index() == 2) {
                s_cdChangeCursor.x = 0;
                s_cdChangeCursor.y = 1;
            }
            g_entering = 0;
        } else {
            s_cdChangeCursor.x = 0;
            s_cdChangeCursor.y = 1;
            g_cameraCue = 2;
            g_entering = 1;
        }
        g_menuMovie = upMovies[s_cdChangeCursor.Index()];
        SetCamera(g_cameraCue);
        s_voices.Stop();
    }
    int active = still && !g_entering && !g_leaving;
    s_cdChangeCursor.Read(&s_input, 1, 0, active);
    int modelJitter = s_menuJitter.Raw() + (s_offset.m_value << 12);
    if (Title::s_next == Title::SCREEN_DONE) {
        modelJitter += jitter;
    }
    SetModelJitter(MODEL_CD, modelJitter, s_passes);
    if (s_input.changed) {
        if ((s_input.up != 0) | (s_input.down != 0)) {
            s_voices.Play(sounds[s_cdChangeCursor.Index()], 64);
            g_vibriState = VIBRI_SITTING_SPEAKING;
            if (s_input.up) {
                g_menuMovie = upMovies[s_cdChangeCursor.Index()];
                s_movies[g_menuMovie]->Rewind(1);
                s_speed = -s_menuSpeed;
            } else if (s_input.down) {
                g_menuMovie = downMovies[s_cdChangeCursor.Index()];
                s_movies[g_menuMovie]->Rewind();
                s_speed = s_menuSpeed;
            }
        }
    }
    if (s_input.back) {
        g_vibriState = VIBRI_SITTING_SPEAKING;
        g_menuMovie = upMovies[s_cdChangeCursor.Index()];
        s_movies[g_menuMovie]->Rewind();
        s_voices.Play(sounds[s_cdChangeCursor.Index()], 64);
        s_speed = 0.0;
    }
    DrawVibri(modelJitter, s_passes);
    s_cameras[13]->Update(0);
    if (Title::s_fade == Title::FADE_OUT && !g_leaving) {
        int index = s_cdChangeCursor.Index();
        /* not on the way from RESULT_CD_CHANGED to a course without the track select */
        if (index != 1 || (Title::s_course != COURSE_ALL_TRACKS && Title::s_course != COURSE_IN_ORDER)) {
            UpdateCamera(cameraCues[index]);
        }
    }
    if (Title::s_fade == Title::FADE_IN) {
        UpdateCamera(g_cameraCue);
    }
    if (g_entering) {
        DrawMenuMovie(48);
    } else {
        DrawMovie(g_menuMovie, s_speed);
    }
    if (g_leaving && s_movies[49]->m_finished) {
        g_leaving = 0;
        SetCamera(cameraCues[s_cdChangeCursor.Index()]);
        return results[s_cdChangeCursor.Index()];
    }
    if (s_input.confirm) {
        s_voices.Play(1, 90);
        if (s_cdChangeCursor.Index() < 2) {
            SetCamera(cameraCues[s_cdChangeCursor.Index()]);
            return results[s_cdChangeCursor.Index()];
        }
        g_leaving = 1;
        g_menuMovie = 49;
        s_speed = s_menuSpeed;
        s_movies[49]->Rewind();
    }
    return 0;
}

int TitleDraw::DrawAlert(int jitter)
{
    static const int sounds[][6] = {
        { 242, 243, 245, 246, 244, SoundSequence::END },
        { 240, 241, 245, 246, 244, SoundSequence::END },
        { 101, SoundSequence::END },
    };
    /* nearly black: CD CHECK hides the "triangle = Back" sign */
    static CVECTOR hideColor = { 0, 0, 2, 0 };

    if (Title::s_arrived) {
        /* which alert: 0 the audio CD's, 1 the Vib-Ribbon CD's, 2 the check */
        int alert = 0;
        g_leaving = 0;
        g_entering = 0;
        g_waitFrame = 0;
        g_vibriState = VIBRI_STANDING;
        s_music.Play(32, 90);
        switch (Title::s_screen) {
        case Title::SCREEN_ALERT_AUDIO_CD_IN:
            SetCamera(4);
            break;
        case Title::SCREEN_ALERT_VIB_CD_IN:
            SetCamera(4);
            alert = 1;
            break;
        case Title::SCREEN_CD_CHECK:
            alert = 2;
            break;
        }
        RewindMovies();
        s_voices.Play(sounds[alert], 64);
        g_alertSwap->Show(alert);
    }
    int modelJitter = s_menuJitter.Raw() + (s_offset.m_value << 12);
    if (Title::s_fade == Title::FADE_OUT && Title::s_next == Title::SCREEN_DONE) {
        modelJitter += jitter;
    }
    /* the check is over when the screen goes to the track select */
    if (Title::s_screen == Title::SCREEN_CD_CHECK && !g_entering && Title::s_next == Title::SCREEN_TRACK_SELECT) {
        g_entering = 1;
        g_leaving = 1;
    }
    SetModelJitter(MODEL_ALERT, modelJitter, s_passes);
    if (Title::s_screen == Title::SCREEN_CD_CHECK) {
        s_models[MODEL_ALERT]->m_objects[0]->SetColor(hideColor);
    } else {
        s_models[MODEL_ALERT]->m_objects[0]->ClearColor();
    }
    DrawVibri(modelJitter, s_passes);
    UpdateCamera(4);
    s_movies[53]->Draw();
    if (g_leaving && g_waitFrame++ == Frames(10)) {
        SetCamera(5);
        g_leaving = 0;
    }
    int confirmed = Title::s_fade == Title::FADE_NONE && Title::AlertWaits() && ReadConfirm() == 2;
    if (confirmed) {
        g_leaving = 1;
        s_voices.Play(1, 90);
        return Title::RESULT_ALERT_CANCEL;
    }
    return 0;
}

/*
 * With `frame` 0, PSJ HISCORE: the high scores of the course chosen with
 * left and right; otherwise frame `frame` of the high scores shown after a
 * course (CLEAR, AGAIN), the new score's rank blinking.
 */
int TitleDraw::DrawPsjHiscore(int jitter, int still, int frame)
{
    /* the model's objects: the menu's arrows and back item, the ranks, the courses' names */
    static const int menuObjects[] = { 0, 1, 2, 7 };
    static const int rankObjects[] = { 27, 48, 49 };
    static const int courseNames[] = { 6, 47, 44, 45, 43, 40, 41, 42 };
    /* how the two signs show a course */
    static const int courseSign[] = { 0, 1, 2, 3, 4, 5, 5, 5 };
    static const int speedSign[] = { 0, 0, 0, 0, 0, 1, 2, 3 };
    /* said on each of the three courses, then on the back item */
    static const int sounds[] = { 114, 113, 112, 96 };
    /* the courses of PSJ HISCORE */
    static const int courses[] = { COURSE_BRONZE, COURSE_SILVER, COURSE_GOLD };
    /* nearly black, to hide an object */
    static CVECTOR hideColor = { 0, 0, 2, 0 };

    if (Title::s_arrived || frame == 2) {
        s_backCursor.x = 0;
        s_backCursor.y = 0;
        g_menuMovie = 64;
        g_frame = 0;
        s_speed = 0.0;
        if (Title::s_screen == Title::SCREEN_PSJ_HISCORE) {
            s_music.Play(32, 90);
        }
        RewindMovies();
        g_entering = 1;
        g_leaving = 0;
        g_vibriState = VIBRI_LYING;
        if (frame == 0) {
            g_courseSign->Show(g_hiscoreCourse);
            g_speedSign->Show(0);
            for (int i = 0; i < 4; i++) {
                s_models[MODEL_HISCORE]->m_objects[menuObjects[i]]->ClearColor();
            }
            for (int i = 0; i < 3; i++) {
                s_models[MODEL_HISCORE]->m_objects[rankObjects[i]]->ClearColor();
            }
        } else {
            /* after a course: no menu */
            g_courseSign->Show(courseSign[Title::s_course]);
            g_speedSign->Show(speedSign[Title::s_course]);
            for (int i = 0; i < 4; i++) {
                s_models[MODEL_HISCORE]->m_objects[menuObjects[i]]->SetColor(hideColor);
            }
        }
    }
#line 2220 "C:/psj/dev/locale/game/TitleDraw.cpp"
    assert(Title::s_highScores != NULL);
#line 2152 "src/game/TitleDraw.cpp"
    int course;
    if (Title::s_screen == Title::SCREEN_PSJ_HISCORE) {
        course = courses[g_hiscoreCourse];
    } else {
        course = Title::s_course;
    }
    /* the scores of the speed courses are times */
    int speed = course >= COURSE_SPEED_1;
    int seconds = speed && course < COURSE_IN_ORDER;
    ShowNumber(s_firstScoreDigits, Title::s_highScores[course * 3 + 0], seconds);
    ShowNumber(s_secondScoreDigits, Title::s_highScores[course * 3 + 1], seconds);
    ShowNumber(s_thirdScoreDigits, Title::s_highScores[course * 3 + 2], seconds);
    s_backCursor.Read(&s_input, 0, 0, still);
    LookAlongZ(39.1875, 0.0, -166.3125, 0.0, 2.9375);

    int modelJitter;
    if (frame != 0) {
        modelJitter = (s_menuJitter + GteFixed::Int(s_offset.m_value)).Raw();
        SetModelJitter(MODEL_HISCORE, modelJitter, s_passes);
        for (int i = 0; i < 3; i++) {
            s_models[MODEL_HISCORE]->m_objects[rankObjects[i]]->ClearColor();
        }
        /* the new score's rank blinks */
        if (frame % VideoSys::Fps() < Frames(30)) {
            s_models[MODEL_HISCORE]->m_objects[rankObjects[Title::s_rank - 1]]->SetColor(hideColor);
        } else {
            s_models[MODEL_HISCORE]->m_objects[rankObjects[Title::s_rank - 1]]->ClearColor();
        }
        DrawVibri(modelJitter, 1);
        if (Title::s_fade == Title::FADE_NONE && Frames(20) < frame) {
            s_movies[66]->SetSpeed(0.0);
            s_movies[66]->Seek(0.0);
            s_movies[66]->Draw();
            s_movies[63]->Draw();
        } else {
            if (Title::s_fade == Title::FADE_OUT) {
                s_movies[62]->SetSpeed(-s_menuSpeed);
            } else {
                s_movies[62]->SetSpeed(s_menuSpeed);
            }
            s_movies[62]->Draw();
        }
        if (frame == Frames(180)) {
            s_movies[62]->Rewind(1);
            return Title::RESULT_RESULTS_DONE;
        }
        return 0;
    }

    if (s_input.back) {
        g_vibriState = VIBRI_LYING_SPEAKING;
        s_voices.Play(sounds[3], 90);
    }
    if (s_backCursor.Index() == 0) {
        if (s_input.left) {
            g_menuMovie = 64;
            if (--g_hiscoreCourse < 0) {
                g_hiscoreCourse = 2;
            }
        } else if (s_input.right) {
            g_menuMovie = 65;
            if (++g_hiscoreCourse >= 3) {
                g_hiscoreCourse = 0;
            }
        }
        if ((s_input.left != 0) | (s_input.right != 0)) {
            s_voices.Play(sounds[g_hiscoreCourse], 90);
            g_vibriState = VIBRI_LYING_SPEAKING;
            s_movies[g_menuMovie]->Rewind();
            s_speed = s_menuSpeed;
            g_courseSign->Show(g_hiscoreCourse);
        }
    }
    if (s_input.changed && ((s_input.down != 0) | (s_input.up != 0))) {
        g_vibriState = VIBRI_LYING_SPEAKING;
        if (s_backCursor.Index() != 0) {
            s_voices.Play(sounds[3], 90);
        } else {
            s_voices.Play(sounds[g_hiscoreCourse], 90);
        }
    }
    int passes = s_passes;
    s_movies[66]->SetSpeed(0.0);
    s_movies[66]->Seek(Fixed::Int(s_backCursor.Index()));
    GteFixed offset = GteFixed::Int(s_offset.m_value);
    modelJitter = (s_menuJitter + offset).Raw();
    int textJitter = (s_textJitter + offset).Raw();
    DrawVibri(modelJitter, passes);
    SetModelJitter(MODEL_HISCORE, modelJitter, passes);
    if (s_backCursor.Index() == 0) {
        s_models[MODEL_HISCORE]->m_objects[courseNames[g_hiscoreCourse]]->SetJitter(Fixed::FromRaw(textJitter << 4), s_passes2);
    } else {
        s_models[MODEL_HISCORE]->m_objects[7]->SetJitter(Fixed::FromRaw(textJitter << 4), s_passes2);
    }
    if (Title::s_fade == Title::FADE_NONE) {
        if (s_backCursor.Index() != 0) {
            s_movies[66]->Draw();
        } else {
            DrawMovie(g_menuMovie, s_speed);
        }
        s_movies[63]->Draw();
    } else {
        if (Title::s_fade == Title::FADE_IN) {
            s_movies[62]->SetSpeed(s_menuSpeed);
        } else {
            s_movies[62]->SetSpeed(-s_menuSpeed);
        }
        s_movies[62]->Draw();
    }
    if (s_input.confirm && s_backCursor.Index() != 0) {
        s_voices.Play(1, 90);
        s_movies[62]->Rewind(1);
        return Title::RESULT_HISCORE_BACK;
    }
    return 0;
}

/* UNKNOWN HISCORE: the high scores of the audio CD's courses (but COURSE_IN_ORDER) */
int TitleDraw::DrawUnknownHiscore(int jitter, int still)
{
    /* the model's objects: the menu's arrows and back item and the ranks, in their own colours */
    static const int plainObjects[] = { 0, 1, 2, 7, 27, 48, 49 };
    /* how the two signs show each course, and the objects of its name */
    static const int courseSign[] = { 3, 4, 5, 5, 5 };
    static const int speedSign[] = { 0, 0, 1, 2, 3 };
    static const int courseNames[] = { 45, 43, 40, 41, 42 };
    /* said on each of the five courses, then on the back item */
    static const int sounds[][3] = {
        { 115, 120, SoundSequence::END },
        { 117, 120, SoundSequence::END },
        { 116, 114, SoundSequence::END },
        { 116, 113, SoundSequence::END },
        { 116, 112, SoundSequence::END },
        { 96, SoundSequence::END },
    };
    static const int courses[] = {
        COURSE_ONE_TRACK, COURSE_ALL_TRACKS, COURSE_SPEED_1, COURSE_SPEED_2, COURSE_SPEED_3,
    };

    if (Title::s_arrived) {
        for (int i = 0; i < 7; i++) {
            s_models[MODEL_HISCORE]->m_objects[plainObjects[i]]->ClearColor();
        }
        s_backCursor.x = 0;
        s_backCursor.y = 0;
        g_menuMovie = 64;
        s_speed = 0.0;
        s_music.Play(32, 90);
        RewindMovies();
        g_entering = 1;
        g_vibriState = VIBRI_LYING;
        g_courseSign->Show(courseSign[g_unknownCourse]);
        g_speedSign->Show(speedSign[g_unknownCourse]);
        s_voices.Stop();
    }
#line 2362 "C:/psj/dev/locale/game/TitleDraw.cpp"
    assert(Title::s_highScores != NULL);
#line 2310 "src/game/TitleDraw.cpp"
    int course = courses[g_unknownCourse];
    int speed = course >= COURSE_SPEED_1;
    int seconds = speed && course < COURSE_IN_ORDER;
    ShowNumber(s_firstScoreDigits, Title::s_highScores[course * 3 + 0], seconds);
    ShowNumber(s_secondScoreDigits, Title::s_highScores[course * 3 + 1], seconds);
    ShowNumber(s_thirdScoreDigits, Title::s_highScores[course * 3 + 2], seconds);
    s_backCursor.Read(&s_input, 0, 0, still);
    LookAlongZ(39.1875, 0.0, -166.3125, 0.0, 2.9375);
    if (s_input.back) {
        g_vibriState = VIBRI_LYING_SPEAKING;
        s_voices.Play(sounds[5], 64);
    }
    if (s_backCursor.Index() == 0) {
        if (s_input.left) {
            g_menuMovie = 64;
            if (--g_unknownCourse < 0) {
                g_unknownCourse = 4;
            }
        } else if (s_input.right) {
            g_menuMovie = 65;
            if (++g_unknownCourse >= 5) {
                g_unknownCourse = 0;
            }
        }
        if ((s_input.left != 0) | (s_input.right != 0)) {
            s_voices.Play(sounds[g_unknownCourse], 64);
            g_vibriState = VIBRI_LYING_SPEAKING;
            s_movies[g_menuMovie]->Rewind();
            s_speed = s_menuSpeed;
            g_courseSign->Show(courseSign[g_unknownCourse]);
            g_speedSign->Show(speedSign[g_unknownCourse]);
        }
    }
    if (s_input.changed && ((s_input.down != 0) | (s_input.up != 0))) {
        g_vibriState = VIBRI_LYING_SPEAKING;
        if (s_backCursor.Index() != 0) {
            s_voices.Play(sounds[5], 64);
        } else {
            s_voices.Play(sounds[g_unknownCourse], 64);
        }
    }
    s_movies[66]->SetSpeed(0.0);
    int passes = s_passes;
    s_movies[66]->Seek(Fixed::Int(s_backCursor.Index()));
    GteFixed offset = GteFixed::Int(s_offset.m_value);
    int modelJitter = (s_menuJitter + offset).Raw();
    int textJitter = (s_textJitter + offset).Raw();
    DrawVibri(modelJitter, passes);
    SetModelJitter(MODEL_HISCORE, modelJitter, passes);
    if (s_backCursor.Index() == 0) {
        Fixed nameJitter = Fixed::FromRaw(textJitter << 4);
        int namePasses = s_passes2;
        s_models[MODEL_HISCORE]->m_objects[courseNames[g_unknownCourse]]->SetJitter(nameJitter, namePasses);
        /* the speed courses' names have "Speed" too */
        if (g_unknownCourse >= 2) {
            s_models[MODEL_HISCORE]->m_objects[46]->SetJitter(nameJitter, namePasses);
        }
    } else {
        s_models[MODEL_HISCORE]->m_objects[7]->SetJitter(Fixed::FromRaw(textJitter << 4), s_passes2);
    }
    if (Title::s_fade == Title::FADE_IN) {
        DrawMovie(62, s_menuSpeed);
    } else if (Title::s_fade == Title::FADE_OUT) {
        DrawMovie(62, -s_menuSpeed);
    } else {
        if (s_backCursor.Index() != 0) {
            s_movies[66]->Draw();
        } else {
            DrawMovie(g_menuMovie, s_speed);
        }
        s_movies[63]->Draw();
    }
    if (s_input.confirm && s_backCursor.Index() != 0) {
        s_voices.Play(1, 90);
        s_movies[62]->Rewind(1);
        return Title::RESULT_HISCORE_BACK;
    }
    return 0;
}

void TitleDraw::UpdateSounds()
{
    s_voices.Update();
    s_music.Update();
}

void TitleDraw::StopMusic()
{
    s_music.Stop();
}
