#include "common.h"

#include <libspu.h>
#include <libsnd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "AudioSys.h"

int AudioSys::s_soundBank = 0;
int AudioSys::s_keyOn = 0;
int AudioSys::s_keyOff = 0;
int AudioSys::s_sounding = 0;
int AudioSys::s_starting = 0;

#ifndef GAME /* the game's version does not have its format strings */
#include "Format.h"
#endif
#include "MemPtr.h"
#include "VideoSys.h"

void AudioSys::Init()
{
    SpuInit();
    InitVoices();
    VideoSys::AddVSyncCallback(Update);
}

void AudioSys::Quit()
{
    VideoSys::RemoveVSyncCallback(Update);
    FreeAll();
    SpuQuit();
}

void AudioSys::SetVolume(float volume)
{
#line 61 "C:/psj/dev/locale/game/AudioSys.cpp"
    assert(volume >= 0.0f && volume <= 1.0);
#line 42 "src/game/AudioSys.cpp"
    short master = (short)(volume * 16383.0f);
    SpuSetCommonMasterVolume(master, master);
}

#ifdef KIOSK
void AudioSys::SetCdVolume(float volume)
{
#line 70 "C:/psj/dev/locale/game/AudioSys.cpp"
    assert(volume >= 0.0f && volume <= 1.0);
#line 52 "src/game/AudioSys.cpp"
    short cd = (short)(volume * 32767.0f);
    SpuSetCommonCDVolume(cd, cd);
}

void AudioSys::SetCdMix(int on)
{
    SpuSetCommonCDMix(on ? SPU_ON : SPU_OFF);
}
#endif

void AudioSys::LoadSounds(const char* vh, const char* vb)
{
    s_soundsVh = MemPtr<u_char>(vh);
    s_soundBank = OpenBank(s_soundsVh.Data(), -1);
    MemPtr<u_char> body(vb);
    TransferBank(s_soundBank, body.Data(), SPU_TRANSFER_WAIT);
}

void AudioSys::FreeSounds()
{
    CloseBank(s_soundBank);
    s_soundsVh = MemPtr<u_char>();
}

int AudioSys::PlaySound(const Sound* sound, int voice, int volume, int after)
{
    return KeyOn(voice, s_soundBank, sound->prog, sound->tone, sound->tone + 1, 0, volume, volume, after);
}

void AudioSys::StopSound(int voice)
{
    KeyOff(voice);
}

bool AudioSys::IsPlaying(int voice)
{
    return IsVoiceOn(voice);
}

void VabHeader::Set(u_char* vh, int spuAddr)
{
    m_header = (VabHdr*)vh;
    m_spuAddr = spuAddr;
    m_progs = (ProgAtr*)(vh + sizeof(VabHdr));
    m_tones = (VagAtr*)(vh + sizeof(VabHdr) + 128 * sizeof(ProgAtr));
    m_vagSizes = (u_short*)((u_char*)m_tones + m_header->ps * 16 * sizeof(VagAtr));
    delete m_vagOffsets; /* (delete, not delete[], as in the original) */
    m_vagOffsets = new int[m_header->vs + 1];
    int i = 1;
    int offset = 0;
    for (; i <= m_header->vs; i++) {
        m_vagOffsets[i] = offset;
        offset += m_vagSizes[i] * 8;
    }
}

VabHeader::~VabHeader()
{
    delete[] m_vagOffsets;
}

VagAtr* VabHeader::GetTone(u_char prog, u_char tone, int* addr, int* volume)
{
    if (prog >= m_header->ps) {
        return NULL;
    }
    ProgAtr* program = &m_progs[prog];
    if (tone >= program->tones) {
        return NULL;
    }
    VagAtr* attr = &m_tones[prog * 16 + tone];
    *addr = m_spuAddr + m_vagOffsets[attr->vag];
    *volume = (m_header->mvol * program->mvol) >> 7;
    return attr;
}

SoundBank::SoundBank(u_char* vh)
{
    Set(vh, -1);
    m_spuAddr = -1;
    /* the body follows the table of waveform sizes */
    u_char* body = (u_char*)&m_vagSizes[256];
    m_bodySize = m_header->fsize - (body - vh);
}

int SoundBank::Upload(int wait)
{
    m_spuAddr = SpuMalloc(m_bodySize);
#line 296 "C:/psj/dev/locale/game/AudioSys.cpp"
    assert(m_spuAddr >= 0);
#line 143 "src/game/AudioSys.cpp"
    SpuSetTransferStartAddr(m_spuAddr);
    SpuWrite(m_body, m_bodySize);
    return IsTransferCompleted(wait);
}

int SoundBank::Transfer(u_char* vb, int wait)
{
    m_body = vb;
    return Upload(wait);
}

int SoundBank::IsTransferCompleted(int wait)
{
    return SpuIsTransferCompleted(wait ? SPU_TRANSFER_WAIT : SPU_TRANSFER_PEEK) != 0;
}

SoundBank::~SoundBank()
{
    if (m_spuAddr >= 0) {
        SpuFree(m_spuAddr);
    }
}

void AudioSys::InitVoices()
{
    SpuInitMalloc(NUM_SPU_BLOCKS, s_spuBlocks);
    for (int i = 0; i < NUM_BANKS; i++) {
        s_banks[i] = NULL;
    }
    s_keyOn = 0;
    s_keyOff = 0;
    s_sounding = 0;
    s_starting = 0;
    for (int voice = 0; voice < NUM_VOICES; voice++) {
        s_startAfter[voice] = -1;
    }
}

void AudioSys::FreeAll()
{
    KeyOffAll();
    Update();
    for (int i = 0; i < NUM_BANKS; i++) {
        if (s_banks[i] != NULL) {
            delete s_banks[i];
        }
    }
}

short AudioSys::OpenBank(u_char* vh, short id)
{
#line 350 "C:/psj/dev/locale/game/AudioSys.cpp"
    assert(id == -1);
#line 197 "src/game/AudioSys.cpp"
    int i;
    for (i = 0; i < NUM_BANKS; i++) {
        if (s_banks[i] == NULL) {
            break;
        }
    }
    if (i == NUM_BANKS) {
        return -1;
    }
    s_banks[i] = new SoundBank(vh);
    return i;
}

int AudioSys::TransferBank(short id, u_char* vb, short wait)
{
    return s_banks[id]->Transfer(vb, wait == SPU_TRANSFER_WAIT);
}

void AudioSys::CloseBank(short id)
{
#line 378 "C:/psj/dev/locale/game/AudioSys.cpp"
    assert(id < NUM_BANKS);
#line 220 "src/game/AudioSys.cpp"
    delete s_banks[id];
    s_banks[id] = NULL;
}

int AudioSys::KeyOn(short voice, short bank, short prog, short tone, short note, short fine,
                    short volumeLeft, short volumeRight, short after)
{
#line 390 "C:/psj/dev/locale/game/AudioSys.cpp"
    assert(bank < NUM_BANKS);
#line 392 "C:/psj/dev/locale/game/AudioSys.cpp"
    assert(voice != after);
#line 232 "src/game/AudioSys.cpp"
    if (s_banks[bank] == NULL) {
        return -1;
    }
    int addr;
    int volume;
    VagAtr* attr = s_banks[bank]->GetTone(prog, tone, &addr, &volume);
    if (attr == NULL) {
        return -1;
    }

    int bit = 1 << voice;
    SpuVoiceAttr voiceAttr;
    voiceAttr.voice = bit;
    voiceAttr.mask = SPU_VOICE_WDSA;
    voiceAttr.addr = addr;
    voiceAttr.mask |= SPU_VOICE_PITCH;
    voiceAttr.pitch = SsPitchFromNote(note, fine, attr->center, attr->shift);
    voiceAttr.mask |= SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    int level = volume * attr->vol;
    voiceAttr.volume.left = (level * volumeLeft) >> 7;
    voiceAttr.volume.right = (level * volumeRight) >> 7;
    voiceAttr.mask |= SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2;
    voiceAttr.adsr1 = attr->adsr1;
    voiceAttr.adsr2 = attr->adsr2;
    SpuSetVoiceAttr(&voiceAttr);

    s_starting |= bit;
    if (after >= 0) {
        s_startAfter[after] = voice;
    } else {
        /* `after` is negative here: this writes before the array, as the original does */
        s_startAfter[after] = -1;
        s_keyOn |= bit;
    }
    return voice;
}

int AudioSys::KeyOff(short voice)
{
    s_startAfter[voice] = -1;
    int bit = 1 << voice;
    s_keyOff |= bit;
    s_keyOn &= ~bit;
    s_starting &= ~bit;
    return 0;
}

void AudioSys::KeyOffAll()
{
    s_keyOff = (1 << NUM_VOICES) - 1;
    s_keyOn = 0;
    s_starting = 0;
    for (int voice = 0; voice < NUM_VOICES; voice++) {
        s_startAfter[voice] = -1;
    }
}

/* At every vertical blank: follow the voices, start queued ones and apply the key changes. */
void AudioSys::Update()
{
    int sounding = s_sounding;
    int starting = s_starting;
    for (int voice = 0; voice < NUM_VOICES; voice++) {
        int bit = 1 << voice;
        if (((starting | sounding) & bit) == 0) {
            continue;
        }
        switch (SpuGetKeyStatus(bit)) {
        case SPU_OFF:
        case SPU_ON_ENV_OFF:
            /* silent (its envelope at zero, keyed on or not): if it was sounding, start the voice queued after it */
            if (sounding & bit) {
                int next = s_startAfter[voice];
                if (next >= 0) {
                    s_startAfter[voice] = -1;
                    s_keyOn |= 1 << next;
                }
            }
            sounding &= ~bit;
            break;
        case SPU_ON:
        case SPU_OFF_ENV_ON:
            starting &= ~bit;
            sounding |= bit;
            break;
        }
    }
    s_sounding = sounding;
    s_starting = starting;
    if (s_keyOff != 0) {
        SpuSetKey(SPU_OFF, s_keyOff);
        s_keyOff = 0;
    }
    if (s_keyOn != 0) {
        SpuSetKey(SPU_ON, s_keyOn);
        s_keyOn = 0;
    }
}

bool AudioSys::IsVoiceOn(int voice)
{
    return ((s_starting | s_sounding) >> voice) & 1;
}

MemPtr<u_char> AudioSys::s_soundsVh;
SoundBank* AudioSys::s_banks[NUM_BANKS];
char AudioSys::s_spuBlocks[SPU_MALLOC_RECSIZ * (NUM_SPU_BLOCKS + 1)];
int AudioSys::s_startAfter[NUM_VOICES];
