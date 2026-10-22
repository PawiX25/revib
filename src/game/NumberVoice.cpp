#include "common.h"

#include <string.h>

#include "NumberVoice.h"

/*
 * The clips being put together: s_clips[0..s_count), lasting s_beats.
 * Nothing in the game sets s_clips, so they go to the start of memory.
 */
int NumberVoice::s_count = 0;
int NumberVoice::s_beats = 0;
int* NumberVoice::s_clips = NULL;
int NumberVoice::s_unused = 0;
VoiceClip NumberVoice::s_man = { 88, 2 };

const VoiceClip NumberVoice::s_syllables[16] = {
    { 64, 1 }, { 65, 1 }, { 66, 1 }, { 67, 2 }, { 68, 2 }, { 69, 2 }, { 70, 1 }, { 71, 1 },
    { 72, 1 }, { 73, 2 }, { 74, 1 }, { 75, 1 }, { 76, 2 }, { 77, 1 }, { 78, 1 }, { 79, 1 },
};
const VoiceClip NumberVoice::s_others[3] = { { 90, 2 }, { 91, 1 }, { 0, 0 } };
const VoiceClip NumberVoice::s_tens[11] = {
    { 0, 0 }, { 81, 2 }, { 80, 2 }, { 80, 2 }, { 80, 2 }, { 81, 2 }, { 81, 2 }, { 80, 2 }, { 81, 2 }, { 80, 2 },
    { 89, 1 },
};
const VoiceClip NumberVoice::s_hundreds[11] = {
    { 0, 0 }, { 82, 2 }, { 82, 2 }, { 83, 2 }, { 84, 2 }, { 85, 2 }, { 85, 2 }, { 84, 2 }, { 85, 2 }, { 84, 2 },
    { 0, 0 },
};
const VoiceClip NumberVoice::s_thousand[2] = { { 86, 2 }, { 87, 2 } };

#define SILENCE NumberVoice::s_others[2]
#define SYLLABLE(i) NumberVoice::s_syllables[i]

/* A digit said, in one or two clips. */
struct SpokenDigit {
    VoiceClip clips[2];
};

static inline SpokenDigit Say(const VoiceClip& clip)
{
    SpokenDigit digit;
    memset(&digit, 0, sizeof(digit));
    digit.clips[0] = clip;
    return digit;
}

static inline SpokenDigit Say(const VoiceClip& first, const VoiceClip& second)
{
    SpokenDigit digit;
    digit.clips[0] = first;
    digit.clips[1] = second;
    return digit;
}

/* The digits in each form they are said in. */
#ifdef NON_MATCHING
static SpokenDigit s_digits[NumberVoice::NUM_FORMS * 10] = {
    Say(SILENCE),
    Say(SYLLABLE(0), SYLLABLE(13)),
    Say(SILENCE, SYLLABLE(2)),
    Say(SILENCE, SYLLABLE(3)),
    Say(SILENCE, SYLLABLE(5)),
    Say(SILENCE, SYLLABLE(6)),
    Say(SYLLABLE(7), SYLLABLE(8)),
    Say(SILENCE, SYLLABLE(9)),
    Say(SYLLABLE(10), SYLLABLE(15)),
    Say(SILENCE, SYLLABLE(12)),

    Say(SILENCE),
    Say(SYLLABLE(0), SYLLABLE(1)),
    Say(SILENCE, SYLLABLE(2)),
    Say(SILENCE, SYLLABLE(3)),
    Say(SILENCE, SYLLABLE(5)),
    Say(SILENCE, SYLLABLE(6)),
    Say(SYLLABLE(7), SYLLABLE(8)),
    Say(SILENCE, SYLLABLE(9)),
    Say(SYLLABLE(10), SYLLABLE(11)),
    Say(SILENCE, SYLLABLE(12)),

    Say(SILENCE),
    Say(SYLLABLE(0), SYLLABLE(1)),
    Say(SILENCE, SYLLABLE(2)),
    Say(SILENCE, SYLLABLE(4)),
    Say(SILENCE, SYLLABLE(5)),
    Say(SILENCE, SYLLABLE(6)),
    Say(SYLLABLE(7), SYLLABLE(8)),
    Say(SILENCE, SYLLABLE(9)),
    Say(SYLLABLE(10), SYLLABLE(11)),
    Say(SILENCE, SYLLABLE(12)),

    Say(SILENCE),
    Say(SILENCE, SILENCE),
    Say(SILENCE, SYLLABLE(2)),
    Say(SILENCE, SYLLABLE(3)),
    Say(SILENCE, SYLLABLE(5)),
    Say(SILENCE, SYLLABLE(6)),
    Say(SYLLABLE(7), SYLLABLE(14)),
    Say(SILENCE, SYLLABLE(9)),
    Say(SYLLABLE(10), SYLLABLE(15)),
    Say(SILENCE, SYLLABLE(12)),
};
#else
static SpokenDigit s_digits[NumberVoice::NUM_FORMS * 10];
#endif

/* a rest in the timeline */
#define REST 91

void NumberVoice::AddUnit(int unit, int digit)
{
    switch (unit) {
    case UNIT_TEN:
        s_clips[s_count] = s_tens[digit].sound;
        break;
    case UNIT_TEN_LAST:
        s_clips[s_count] = s_tens[10].sound;
        s_count++;
        s_clips[s_count] = s_others[1].sound;
        break;
    case UNIT_HUNDRED:
        s_clips[s_count] = s_hundreds[digit].sound;
        break;
    case UNIT_THOUSAND:
        if (digit == 3) {
            s_clips[s_count] = s_thousand[1].sound;
        } else {
            s_clips[s_count] = s_thousand[0].sound;
        }
        break;
    case UNIT_MAN:
        s_clips[s_count] = s_man.sound;
        break;
    }
    s_beats += 2;
    s_count++;
}

void NumberVoice::AddDigit(int digit, int form)
{
    int forms = form * 10;
    if (digit != 0 && digit < 1000000) {
        digit += forms;
        for (int j = 0; j < 2; j++) {
            if (s_digits[digit].clips[j].sound != 0) {
                s_clips[s_count] = s_digits[digit].clips[j].sound;
                s_beats += s_digits[digit].clips[j].beats;
                s_count++;
            }
        }
    }
}

/* 0..99 */
void NumberVoice::ReadTens(int n, int form)
{
    int tens = n / 10;
    int units = n - tens * 10;
    if (tens > 0) {
        if (tens >= 2) {
            AddDigit(tens, form != FORM_MAN ? form : 0);
        }
        AddUnit(units == 0 && form == 0 ? UNIT_TEN_LAST : UNIT_TEN, tens);
    }
    AddDigit(units, form);
}

/* 0..9999 */
void NumberVoice::ReadThousands(int n, int form)
{
    int high = n / 100;
    int rest = n - high * 100;
    int thousands = high / 10;
    int hundreds = high - thousands * 10;
    if (thousands >= 2) {
        AddDigit(thousands, form);
    }
    if (thousands > 0) {
        AddUnit(UNIT_THOUSAND, thousands);
    }
    if (hundreds >= 2) {
        AddDigit(hundreds, FORM_HUNDREDS);
    }
    if (hundreds > 0) {
        AddUnit(UNIT_HUNDRED, hundreds);
    }
    ReadTens(rest, form);
}

/* 0..999999 */
void NumberVoice::ReadNumber(int n, int form)
{
    if (n != 0) {
        int man = n / 10000;
        int rest = n - man * 10000;
        ReadTens(man, FORM_MAN);
        if (man > 0) {
            AddUnit(UNIT_MAN, 0);
        }
        ReadThousands(rest, form);
    }
}

void NumberVoice::Say(VoiceLine* line, int number, int form)
{
    s_count = 0;
    s_beats = 0;
    if (number == 0) {
        line->timeline[0] = -1;
        line->clips[0] = -1;
        line->tempo = 0;
        return;
    }
    ReadNumber(number, form);
    for (int i = 0; i < s_count; i++) {
        line->clips[i] = s_clips[i];
    }
    int beats;
    if (s_beats >= 15) {
        line->tempo = 2;
        beats = 22;
    } else if (s_beats >= 7) {
        line->tempo = 1;
        beats = 14;
    } else {
        line->tempo = 0;
        beats = 6;
    }
    /* the clips last, after rests */
    for (int i = 0; i < beats; i++) {
        int rests = beats - s_beats;
        if (i < rests) {
            line->timeline[i] = REST;
        } else if (s_count - (i - rests) > 0) {
            line->timeline[i] = line->clips[i - rests];
        } else {
            line->timeline[i] = -1;
        }
    }
    line->clips[s_count] = -1;
    line->timeline[beats] = -1;
}

#ifndef NON_MATCHING
/*
 * s_digits' static initializer. The C++ above gives the same code but for
 * three instructions: the game's keeps sp + 16, where each SpokenDigit is
 * built, in a register for the last three memsets instead of computing it
 * again each time.
 */
INCLUDE_ASM("asm/main_g/nonmatchings/NumberVoice", _GLOBAL_.I._11NumberVoice.s_count);
#endif
