#ifndef GAMEPARAMS_H
#define GAMEPARAMS_H

#include "common.h"

#include <stdio.h>

#include "CdSys.h"
#include "LanguageSelect.h"

/* the argc of an executable run by the loader; its argv is then the loader's LoaderData */
#define LOADER_ARGC ((int)0x9999A2F2)

/* the executables the loader runs */
enum Executable {
    EXECUTABLE_TITLE, /* main_t.exe */
    EXECUTABLE_GAME,  /* main_g.exe */
    EXECUTABLE_KIOSK  /* main_k.exe */
};

/* What the game executable keeps from one run to the next. */
struct GameRecord {
    int saved;              /* the rest has been set */
    int highScores[9][3];   /* of each course (NUM_COURSES), the best first (NUM_HIGH_SCORES) */
    int vibration;          /* the options: the controller's vibration, */
    int soundEffects;       /* and the game's sound effects */
};

/* What the loader keeps for the executables it runs. */
struct GameParams {
    GameRecord game;     /* used by the game executable */
    DiscId homeDisc;     /* the game's own disc, found on start-up */
    int firstRun;        /* set by the loader on start-up: logos, then the language menu */
    int returning;       /* set by the kiosk executable as it ends: the title goes straight to the mode menu */
    int demoCount;       /* the title's demos started so far, counted round its eight (TitleMain.cpp) */
    int demo;            /* the game is to play its demo of demoLevel */
    int demoLevel;       /* 1 to 6: bronze's first and second tracks, then silver's, then gold's */
    int seed;            /* the game's random numbers carry on from one game to the next */
    MenuChoices choices;
};

/* The loader's data, which the executables it runs get as argv. */
struct LoaderData {
    LoaderData(); /* (loader.cpp) */

    int next; /* the executable to run next, set by the one that ends */
    GameParams params;
};

#if defined(TITLE) || defined(KIOSK)
/*
 * Nothing in the executables calls it; only its format string was compiled
 * into the title's and the kiosk's main files (the game's and the loader's have none).
 */
inline int PrintLine(const char* line)
{
    return printf("%s\n", line);
}
#endif

#endif
