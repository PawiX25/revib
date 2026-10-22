#ifndef TITLEDRAW_H
#define TITLEDRAW_H

#include "common.h"

#include "Fixed.h"
#include "Font.h"
#include "Menu.h"
#include "SoundSequence.h"
#include "NumberVoice.h"

class TmdModel;
class ModelMovie;
class CameraMovie;
struct ObjectSwap;
template <class T> class NumberParameter;

/*
 * The screens of Title (Title.cpp), drawn: the line models of title/ with
 * their movies and camera movies, the menus' cursors, the numbers and the
 * text in the chosen language.
 */
class TitleDraw {
public:
    static void Load();
    static void Unload();
    /* the sounds and voices queued by the screens */
    static void UpdateSounds();
    static void StopMusic();

    /*
     * The screens: `jitter` (4096 is 1) grows as the screen fades in or
     * out, and `still` is when there is no fade. What was chosen is
     * returned (a Title::Result).
     */
    static int DrawMain(int jitter, int still);
    static int DrawPsjModeSelect(int jitter, int still);
    static int DrawUnknownModeSelect(int still);
    static int DrawModeSpeedSelect(int still);
    static int DrawCdChange(int jitter, int still);
    static int DrawTrackSelect(int jitter, int still);
    static int DrawAlert(int jitter);
    static int DrawOption(int still);
    static int DrawPsjHiscore(int jitter, int still, int frame);
    static int DrawUnknownHiscore(int jitter, int still);
    static int DrawClear(int jitter);
    static int DrawAgain(int jitter);
    static int DrawAgainSelect(int jitter, int still);

    /* settings of the debug menu */
    static NumberParameter<int> s_subline;
    static NumberParameter<int> s_offset;

    /* the screens' models, movies and cameras, and the digits of a number shown */
    enum {
        NUM_MODELS = 15,
        NUM_MOVIES = 102,
        NUM_CAMERAS = 14,
        NUM_DIGITS = 6
    };

private:
    /* a camera movie played forwards or backwards */
    struct CameraCue {
        int camera;
        int speed;
        int backwards;
    };

    static void DrawText(const char* text, int jitter, GteFixed x, GteFixed y, GteFixed scale);
    static ModelMovie* NewModelMovie(TmdModel* model, const char* file);
    static CameraMovie* NewCameraMovie(const char* file);
    static void SetModelJitter(int model, int jitter, int passes);
    static void DrawMovie(int movie, Fixed speed);
    static void RewindMovies();
    static void SetCamera(int cue);
    static void UpdateCamera(int cue);
    static void DrawVibri(int jitter, int passes);
    static void ResetSwaps(ObjectSwap** swaps, int count);
    static void ShowNumber(ObjectSwap** swaps, int value, int seconds);
    static void ShowMarks(ObjectSwap** swaps);
    static void PickComment();
    static void CallCallback(int track);
    static int Wait(int sixtieths);
    static void DrawMenuMovie(int movie);

    static MenuCursor s_mainCursor;
    static MenuCursor s_psjModeCursor;
    static MenuCursor s_unknownModeCursor;
    static MenuCursor s_againCursor;
    static MenuCursor s_cdChangeCursor;
    static MenuCursor s_optionCursor;
    static MenuCursor s_speedCursor;
    static MenuCursor s_backCursor;

    static VoiceLine s_voiceLine;
    static SoundSequence s_voices;
    static SoundSequence s_music;

    static Fixed s_speed;

    /*
     * The digits of the numbers shown: the score of CLEAR and AGAIN (CLEAR's
     * time on a speed course); CLEAR's bonus and its total, which
     * clr_song.tmd shows too; and HISCORE's three scores. (These and the
     * members below are laid out in memory in the reverse order.)
     */
    static ObjectSwap* s_thirdScoreDigits[NUM_DIGITS];
    static ObjectSwap* s_secondScoreDigits[NUM_DIGITS];
    static ObjectSwap* s_firstScoreDigits[NUM_DIGITS];
    static ObjectSwap* s_songTotalDigits[NUM_DIGITS];
    static ObjectSwap* s_totalDigits[NUM_DIGITS];
    static ObjectSwap* s_scoreDigits[NUM_DIGITS];
    static ObjectSwap* s_bonusDigits[NUM_DIGITS];
    static ObjectSwap* s_marks[7];
    static CameraMovie* s_cameras[NUM_CAMERAS];
    static ModelMovie* s_movies[NUM_MOVIES];
    static TmdModel* s_models[NUM_MODELS];
    static TmdModel* Model(int model) { return s_models[model]; }
    static MenuInput s_input;
};

#endif
