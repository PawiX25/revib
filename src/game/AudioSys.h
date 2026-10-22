#ifndef AUDIOSYS_H
#define AUDIOSYS_H

#include "common.h"

#include <libspu.h>
#include <libsnd.h>

template <class T> class MemPtr;

/* A sound in the sound bank: a program and one of its tones. */
struct Sound {
    int prog;
    int tone;

    Sound(int program, int toneNumber) : prog(program), tone(toneNumber) {}
    /* a sound number: (program << 4) | tone */
    explicit Sound(int number) : prog(number >> 4), tone(number & 0xF) {}
};

/*
 * The header (.VH) of a VAB sound bank, giving the programs, their tones and
 * where each waveform (VAG) is in sound memory.
 */
class VabHeader {
public:
    VabHeader() : m_vagOffsets(NULL) {}
    virtual ~VabHeader();

    void Set(u_char* vh, int spuAddr);
    /* The tone, with its waveform's address and the volume of the bank and program; NULL if there is none. */
    VagAtr* GetTone(u_char prog, u_char tone, int* addr, int* volume);

protected:
    VabHdr* m_header;
    ProgAtr* m_progs;
    VagAtr* m_tones;
    u_short* m_vagSizes;   /* in units of 8 bytes */
    int* m_vagOffsets;     /* in the body */
    int m_spuAddr;         /* of the body */
};

/* A VAB sound bank: its header, and the body (.VB) once in sound memory. */
class SoundBank : public VabHeader {
public:
    SoundBank(u_char* vh);
    virtual ~SoundBank();

    int Transfer(u_char* vb, int wait);

private:
    int Upload(int wait);
    static int IsTransferCompleted(int wait);

    u_char* m_body;
    u_long m_bodySize;
};

/*
 * Sound effects: up to ten sound banks in sound memory, played on the SPU's
 * 24 voices. A voice can be queued to start when another one ends.
 */
class AudioSys {
public:
    static void Init();
    static void Quit();
    static void SetVolume(float volume);   /* the master volume, 0..1 */
    /* the CD audio's volume and mixing, which only the kiosk sets */
    static void SetCdVolume(float volume); /* 0..1 */
    static void SetCdMix(int on);          /* mixes the CD's sound into the output */

    /* The game's sound bank, used by PlaySound(). */
    static void LoadSounds(const char* vh, const char* vb);
    static void FreeSounds();
    /* `after`: a voice to start after, or -1 to start now; returns the voice or -1 */
    static int PlaySound(const Sound* sound, int voice, int volume, int after);
    static void StopSound(int voice);
    static bool IsPlaying(int voice);

private:
    enum { NUM_BANKS = 10, NUM_VOICES = 24, NUM_SPU_BLOCKS = 15 };

    static void InitVoices();
    static void FreeAll();
    static short OpenBank(u_char* vh, short id);
    static int TransferBank(short id, u_char* vb, short wait);
    static void CloseBank(short id);
    static int KeyOn(short voice, short bank, short prog, short tone, short note, short fine,
                     short volumeLeft, short volumeRight, short after);
    static int KeyOff(short voice);
    static void KeyOffAll();
    static void Update();
    static bool IsVoiceOn(int voice);

    static MemPtr<u_char> s_soundsVh;
    static SoundBank* s_banks[NUM_BANKS];
    static char s_spuBlocks[SPU_MALLOC_RECSIZ * (NUM_SPU_BLOCKS + 1)];
    static int s_startAfter[NUM_VOICES]; /* voice to key on when this one ends, or -1 */

    static int s_starting;   /* keyed on or queued, not sounding yet */
    static int s_sounding;
    static int s_keyOff;     /* to key off at the next update */
    static int s_keyOn;      /* to key on at the next update */
    static int s_soundBank;
};

#endif
