#ifndef SOUNDSEQUENCE_H
#define SOUNDSEQUENCE_H

#include "common.h"

/*
 * Sounds played one after the other on two voices, `voice` and `voice + 12`
 * by turns, each one starting once the one before has been heard. A sound is
 * a program and a tone of the sound bank, (program << 4) | tone; -1 ends a
 * list.
 */
class SoundSequence {
public:
    enum { END = -1 };

    SoundSequence(int voice);

    void Stop();
    int Play(const int* sounds, int volume);
    int Play(int sound, int volume);
    /* once its voice is free, the next sound is set to start as the one before ends (the first at once) */
    void Update();
    /* from Play() until Stop(), or until an Update() finds the last sound over */
    int IsPlaying() { return m_playing; }

private:
    int m_voice;
    int m_current;  /* the voice for the next sound */
    int m_previous; /* the voice of the last one (-1: none yet) */
    int m_playing;
    const int* m_sounds;
    int m_next;     /* index of the next sound */
    int m_volume;
    int m_single[2]; /* the list Play(int, int) plays */
};

#endif
