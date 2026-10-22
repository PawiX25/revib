#ifndef GLUE_H
#define GLUE_H

#include "common.h"

#include "CdSys.h"
#include "Fixed.h"
#include "GameParams.h"
#include "Gameplay.h"
#include "Title.h"

/* Set when the drive's lid closes (SetShellCloseReceiver()). */
struct ShellClose : CdReport {};

/* Set when the drive turns out to be empty (SetNoDiscReceiver()). */
struct NoDisc : CdReport {};

/*
 * Between the title's screens and the courses: goes from screen to screen
 * as the player chooses, asks for the discs a course needs, plays its tracks
 * one after the other and keeps the high scores.
 */
class Glue {
public:
    static void Init(GameParams* params);
    static void Quit();
    /* the music heard is at `position` (in samples) */
    static void Update(int position);
    /* back to the title executable */
    static int IsDone();

    /* What the music's buffer reaches, ahead of what is heard. */
    static void BufferTrackStart(int track, int position, int seconds);
    static void BufferTrackEnd(int position);
    static void BufferQuiet(int position);

    /* How the course goes. */
    static void SignalGameOver();
    static void SignalUserAbort();
    static void SignalGameClear();

    /* how loud the music heard is, 0 to 1 (0 unless playing) */
    static Fixed GetSoundLevel();
    /* the options */
    static int GetSoundEffects();
    static int GetVibration();

private:
    enum State {
        STATE_TITLE = 1, /* the title's screens */
        STATE_ALERT,     /* a screen asking for a disc */
        STATE_STARTING,  /* the track is asked of the drive */
        STATE_BUFFERING, /* the course goes, the music not yet heard */
        STATE_PLAYING    /* the music is heard */
    };

    /* The steps to the start of a course (Start()). */
    enum Step {
        STEP_COURSE,        /* the title is told the course */
        STEP_DISC,          /* which disc is wanted */
        STEP_ASK_HOME_DISC,
        STEP_ASK_AUDIO_CD,  /* then the CD change screen, under the alert */
        STEP_ASK_AUDIO_CD_AGAIN,
        STEP_CD_CHANGE,
        STEP_CD_CHANGE_HIDDEN,
        STEP_TRACK_SELECT,
        STEP_PLAY
    };

    enum { MAX_SCREENS = 10, MAX_TRACKS = 100 };

    /* Which course a result of the title chooses. */
    struct CourseResult {
        int result;
        Course course;
    };

    /* the callbacks of the title's track select and options screens */
    static void ChooseTrack(int track);
    static void SetOptions(int soundEffects, int vibration);

    /* The screens, one on top of the other: the top one is shown. */
    static void ClearScreens();
    static Title::Screen TopScreen();
    static void PushScreen(Title::Screen screen, int show);
    static void PopScreen(int show);
    static int IsScreenShown();
    static void ShowScreen()
    {
        Title::GoTo(TopScreen());
        s_screenShown = 1;
    }

    /* The tracks. */
    static void ClearTrackOrder();
    static void ShuffleTracks(int numTracks);
    static int FirstTrack(const CourseInfo* course);
    static void PlayTrack(int track);
    static void StartTracks();

    static void ToMainScreen();
    typedef int HighScores[NUM_HIGH_SCORES];
    static void SetHighScores(HighScores* highScores);
    static void ResetHighScores();
    /* the rank of `score` in the course's high scores (0: none), now in them */
    static int RecordHighScore(int course, int score);
    static int IsSpeedCourse(u_int course) { return course >= COURSE_SPEED_1 && course <= COURSE_SPEED_3; }

    /* The alerts that ask for a disc. */
    static void Alert(Title::Screen screen);
    /* the disc that the alert `screen` asks for is in */
    static int HasDisc(Title::Screen screen);
    static void CancelAlert();

    /* `again`: from the track that was lost */
    static void Start(int again);
    static void ChooseCourse(int result);
    static void ChangeCd();
    static void StepBack();
    static void HandleResult(int result);

    static void OnStateChange();
    static void SeedTitle();

    static DiscInfo s_discInfo;
    static ShellOpen s_shellOpen;
    static ShellClose s_shellClose;
    static NoDisc s_noDisc;
    /* the track's start in the buffer, until it is heard */
    static Interval s_trackStart;
    static CourseResult s_courseResults[NUM_COURSES];
    /* for each rank (Vibri's form at the end), the comment whose sign CLEAR shows */
    static int s_comments[4];

    /* The compiler lays these out in reverse order of declaration. */
    static int s_seeded;        /* the title's random numbers have been seeded */
    static int s_leaving;       /* to the title executable, once the game's disc is in */
    static int s_step;          /* the next Step */
    static HighScores* s_highScores; /* of each course */
    static u_int s_course;      /* a Course */
    static int s_screenShown;
    static int s_numScreens;
    static int s_done;
    static int* s_vibration;    /* the options, in the GameRecord */
    static int* s_soundEffects;
    static int s_chosenTrack;
    static int s_demoTrack;     /* of the demo's course */
    static int s_demoStarted;
    static int s_demoLevel;     /* GameParams::demoLevel */
    static int s_demo;
    static int s_trackSeconds;
    static int s_checking;      /* a disc is being read */
    static int s_discSeconds;
    static int s_numTracks;
    static int s_audioDisc;     /* an audio CD is in */
    static int s_homeDisc;      /* the game's own disc is in */
    static int s_tracksToBuffer;
    static int s_track;         /* of the course */
    static int s_playingTrack;  /* of the disc */
    static int s_tracksToPlay;
    static int s_score;         /* of the course's tracks before this one */
    static int s_changedState;  /* the state that OnStateChange() last saw */
    static int s_state;
    static signed char s_trackOrder[MAX_TRACKS];
    static Title::Screen s_screens[MAX_SCREENS];
};

#endif
