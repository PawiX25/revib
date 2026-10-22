#include "common.h"

#include "AudioSys.h"
#include "SoundSequence.h"

SoundSequence::SoundSequence(int voice)
{
    m_voice = voice;
    m_current = voice;
    m_playing = 0;
    m_sounds = NULL;
}

void SoundSequence::Stop()
{
    if (m_playing) {
        AudioSys::StopSound(m_voice);
        AudioSys::StopSound(m_voice + 12);
        m_playing = 0;
    }
}

int SoundSequence::Play(const int* sounds, int volume)
{
    Stop();
    m_volume = volume;
    m_sounds = sounds;
    m_previous = -1;
    m_next = 0;
    m_playing = 1;
    return 1;
}

int SoundSequence::Play(int sound, int volume)
{
    m_single[0] = sound;
    m_single[1] = END;
    return Play(m_single, volume);
}

void SoundSequence::Update()
{
    while (m_playing) {
        if (m_sounds[m_next] != END) {
            if (AudioSys::IsPlaying(m_current)) {
                return;
            }
            int sound = m_sounds[m_next++];
            Sound s(sound >> 4, sound & 0xF);
            AudioSys::PlaySound(&s, m_current, m_volume, m_previous);
            m_previous = m_current;
            m_current = (m_current + 12) % 24;
        } else {
            /* playing until the last one ends */
            m_playing = AudioSys::IsPlaying(m_previous >= 0 ? m_previous : m_current);
            return;
        }
    }
}
