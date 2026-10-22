#ifndef TITLE_H
#define TITLE_H

#include "common.h"

#include "Fixed.h"
#include "Random.h"

class ParameterMenu;
class ChoiceParameter;
class ActionParameter;

/*
 * The game's screens around the courses: the title, the mode, speed,
 * track and option menus, the CD changes and checks, and the results and
 * high scores, one at a time, with a fade between two. Title.cpp goes
 * from one to the other, TitleDraw.cpp shows them.
 */
class Title {
public:
    enum Screen {
        SCREEN_NONE,
        SCREEN_MAIN,
        SCREEN_PSJ_MODE_SELECT,
        SCREEN_UNKNOWN_MODE_SELECT,
        SCREEN_MODE_SPEED_SELECT,
        SCREEN_CD_CHANGE,
        SCREEN_TRACK_SELECT,
        SCREEN_ALERT_VIB_CD_IN,
        SCREEN_ALERT_AUDIO_CD_IN,
        SCREEN_CD_CHECK,
        SCREEN_OPTION,
        SCREEN_PSJ_HISCORE,
        SCREEN_UNKNOWN_HISCORE,
        SCREEN_CLEAR,
        SCREEN_AGAIN,
        SCREEN_AGAIN_SELECT,
        SCREEN_DONE,
        NUM_SCREENS
    };

    /* what was chosen on a screen (GetResult()) */
    enum Result {
        RESULT_NONE,
        RESULT_PSJ_MODES,        /* MAIN */
        RESULT_UNKNOWN_MODES,
        RESULT_OPTIONS,
        RESULT_LEAVE,            /* back to the title executable */
        RESULT_BRONZE,           /* PSJ MODE SELECT */
        RESULT_SILVER,
        RESULT_GOLD,
        RESULT_PSJ_HISCORES,
        RESULT_PSJ_BACK,
        RESULT_ONE_TRACK,        /* UNKNOWN MODE SELECT */
        RESULT_ALL_TRACKS,
        RESULT_SPEED_MODES,
        RESULT_IN_ORDER,
        RESULT_UNKNOWN_HISCORES,
        RESULT_UNKNOWN_BACK,
        RESULT_SPEED_1,          /* MODE SPEED SELECT */
        RESULT_SPEED_2,
        RESULT_SPEED_3,
        RESULT_SPEED_BACK,
        RESULT_20,               /* (no screen returns these) */
        RESULT_21,
        RESULT_22,
        RESULT_CHANGE_CD,        /* CD CHANGE */
        RESULT_CD_CHANGED,
        RESULT_CD_CHANGE_BACK,
        RESULT_TRACK_BACK,       /* TRK SELECT */
        RESULT_TRACK_CHOSEN,
        RESULT_ALERT_CANCEL,     /* the alerts */
        RESULT_29,               /* (no screen returns these) */
        RESULT_30,
        RESULT_31,
        RESULT_32,
        RESULT_33,
        RESULT_OPTION_BACK,      /* OPTION */
        RESULT_HISCORE_BACK,     /* PSJ HISCORE, UNKNOWN HISCORE */
        RESULT_AGAIN,            /* AGAIN SELECT */
        RESULT_AGAIN_BACK,
        RESULT_RESULTS_DONE      /* CLEAR, AGAIN: the results (and the high scores after them) are over */
    };

    /* the stages of the fade between two screens */
    enum Fade {
        FADE_NONE,
        FADE_IN,   /* the new screen comes */
        FADE_OUT   /* the old one goes */
    };

    /* told of the track chosen on TRK SELECT */
    typedef void (*Callback)(int track);
    /* told of the options chosen on OPTION */
    typedef void (*OptionsCallback)(int soundEffects, int vibration);

    static void Init();
    static void Free();
    /* shows the screen and goes on with the fade */
    static void Update();
    /* fades to `screen` (SCREEN_NONE: none, at once) */
    static void GoTo(Screen screen);
    /* what was chosen on the screen this frame (0: nothing) */
    static int GetResult();

    /* what the screens show */
    static void SetNumTracks(int numTracks);
    static void SetCallback(Callback callback);
    static void SetScore(int score, int bonus);
    static void SetSeconds(int seconds);
    static void SetComment(int comment);
    static void SetCourse(int course);
    static void SetOptions(int soundEffects, int vibration);
    static void SetOptionsCallback(OptionsCallback callback);
    static void SetHighScores(int* highScores);
    static void SetRank(int rank);
    static void SetTimeUp(int timeUp);
    static void Seed(u_long seed);

    /* the screen is an alert waiting for a button (CD CHECK goes on by itself) */
    static int AlertWaits() { return s_screen != SCREEN_CD_CHECK; }
    /* the screens go on (DONE: back to the game) */
    static int GoesOn() { return s_next != SCREEN_DONE; }

private:
    friend class TitleDraw;

    static void UpdateFade();

    static const char* s_screenNames[NUM_SCREENS];

    /* the debug menu, shown while a controller is named "Title" */
    static ChoiceParameter s_screenChoice;
    static ActionParameter s_exit;

    static Random s_random;

    /* (laid out in memory in the reverse order) */
    static int s_lastChoice;  /* of the debug menu */
    static int s_result;
    static ParameterMenu* s_menu;
    static int s_frame;
    static int s_fadeStep;    /* added to s_fadeFrame each frame: 1 to fade out, -1 to fade in */
    static int s_fadeFrame;
    static int s_next;
    static int s_previous;
    static int s_screen;
    static int s_fade;        /* a Fade */
    static int s_arrived;     /* the fade has just reached the new screen */

    static int s_timeUp;
    static int s_unused;
    static int s_rank;        /* of the score among the high scores, 1 to 3 (0: none) */
    static int* s_highScores;
    static int s_speedCourse; /* a speed course (COURSE_SPEED_1 to 3) */
    static int s_course;
    static int s_comment;     /* the comment whose sign CLEAR shows */
    static int s_seconds;     /* to reach the target speed */
    static int s_bonus;
    static int s_score;
    static int s_vibration;   /* the options */
    static int s_soundEffects;
    static OptionsCallback s_optionsCallback;
    static Callback s_callback;
    static int s_numTracks;
};

#endif
