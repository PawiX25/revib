#ifndef GAME_H
#define GAME_H

#include "common.h"

/*
 * The game, for the whole run of the executable (the title's screens
 * included): the music streaming, the script's signals and the gameplay,
 * all kept to where the music is.
 */
class Game {
public:
    Game();
    ~Game();

    /* the music is at `position` (in samples) */
    void Update(int position);

private:
    int m_position;        /* where the music is, in samples */
    float m_distance;      /* how far ahead the road is seen (the signals take it too, unused) */
    float m_vibriPosition; /* Vibri's place on the road (the signals take it too, as an unused speed) */
    int m_beatLength;      /* half a second of the music, in samples */
    float m_beatRate;      /* beats per sample */
};

/* The game, from Init() to Quit(). */
class GameSys {
public:
    static void Init();
    static void Draw();
    static void Update(int position);
    static void Quit();

private:
    static Game* s_game;
};

#endif
