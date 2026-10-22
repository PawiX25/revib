#ifndef NUMBERVOICE_H
#define NUMBERVOICE_H

#include "common.h"

/* A sound of the voice, and how many beats it lasts. */
struct VoiceClip {
    int sound;
    int beats;
};

/* The voice saying a number (a score, a time or a track's), clip after clip. */
struct VoiceLine {
    enum { MAX_CLIPS = 23 };

    int clips[MAX_CLIPS];    /* the clips said, -1 after the last */
    int timeline[MAX_CLIPS]; /* the clips beat by beat, rests first; -1 at the end */
    int tempo;               /* 0, 1 or 2: the line lasts 6, 14 or 22 beats */
};

/* Puts together the clips that say a number (in Japanese), up to 999999. */
class NumberVoice {
public:
    /* Fills `line` with the voice saying `number`; `form` is how its last digits are said. */
    static void Say(VoiceLine* line, int number, int form);

    /* the forms digits are said in */
    enum {
        FORM_MAN = 2,      /* before "ten thousand" */
        FORM_HUNDREDS = 3, /* before "hundred" */
        NUM_FORMS = 4
    };

    /* syllables of the digits */
    static const VoiceClip s_syllables[16];
    /* 90 (not said), a rest and silence */
    static const VoiceClip s_others[3];

private:
    enum Unit {
        UNIT_TEN = -99,
        UNIT_TEN_LAST = -98, /* the last thing said */
        UNIT_HUNDRED = -97,
        UNIT_THOUSAND = -96,
        UNIT_MAN = -95
    };

    static void AddUnit(int unit, int digit);
    static void AddDigit(int digit, int form);
    static void ReadTens(int n, int form);
    static void ReadThousands(int n, int form);
    static void ReadNumber(int n, int form);

    static int s_count;
    static int s_beats;
    static int* s_clips;
    static int s_unused;
    static VoiceClip s_man; /* ten thousand */
    static const VoiceClip s_tens[11];     /* "ten" after each tens digit; [10] alone */
    static const VoiceClip s_hundreds[11]; /* "hundred" after each hundreds digit */
    static const VoiceClip s_thousand[2];  /* "thousand", and after three */
};

#endif
