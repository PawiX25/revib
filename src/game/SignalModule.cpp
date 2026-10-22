#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Format.h"
#include "MemorySys.h"
#include <string> /* nothing here uses it, but its empty string is in the file's small data */
#include "SignalModule.h"
#include "AnalysisScript.h"
#include "Gameplay.h"

/* a level only counts as sound above this */
#define MIN_LEVEL 999999

/* level / background is above otherLevel / otherBackground */
static inline int IsAbove(int level, int background, int otherLevel, int otherBackground)
{
    return (long long)otherLevel * background < (long long)level * otherBackground;
}

SignalModule::SignalModule(int blockSize) : m_blockSize(blockSize)
{
}

SignalModule::~SignalModule()
{
}

PeakPicker::PeakPicker()
    : m_cue(NULL), m_time(0), m_level(0), m_background(0), m_scale(Fixed::Int(1)), m_length(0),
      m_candidate(NULL), m_candidateTime(0), m_candidateLevel(0), m_candidateBackground(0)
{
}

PeakPicker::~PeakPicker()
{
    Clear();
}

void PeakPicker::Place()
{
    ScriptEvent event;
    event.type = ScriptEvent::PART;
    event.time = m_time;
    event.value = m_cue->m_selector->Next();
    event.length = m_cue->m_period;
    event.scale = m_scale.Raw();
    Gameplay::AddEvent(&event);
    m_cue->Release();
    m_cue = NULL;
}

void PeakPicker::TakeCandidate()
{
    if (m_cue != NULL) {
        m_cue->Release();
    }
    m_cue = m_candidate;
    m_time = m_candidateTime;
    m_level = m_candidateLevel;
    m_background = m_candidateBackground;
    m_candidate = NULL;
    if (m_cue->m_tempo != NULL) {
        m_scale = m_cue->m_tempo->Next(m_time);
    } else {
        m_scale = Fixed::Int(1);
    }
    m_length = (m_scale * m_cue->m_period).Raw() >> 16;
}

void PeakPicker::EndWindow(int time)
{
    if (m_candidate != NULL) {
        if (m_cue != NULL) {
            if (m_candidateTime - m_time > 2 * m_length) {
                /* far enough from the cue before: that one stays */
                Place();
                TakeCandidate();
            } else if (IsAbove(m_candidateLevel, m_candidateBackground, 3 * m_level / 2, m_background)) {
                /* too close, but much louder: it takes the place of the cue before */
                TakeCandidate();
            } else {
                m_candidate->Release();
                m_candidate = NULL;
            }
        } else {
            TakeCandidate();
        }
    }
    if (m_cue != NULL && time - m_time > 2 * m_length) {
        Place();
    }
}

void PeakPicker::Clear()
{
    if (m_candidate != NULL) {
        m_candidate->Release();
        m_candidate = NULL;
    }
    if (m_cue != NULL) {
        m_cue->Release();
        m_cue = NULL;
    }
}

BlockOnsetModule::~BlockOnsetModule()
{
}

void BlockOnsetModule::ResetHistory()
{
    int i;
    for (i = 0; i < LONG_BLOCKS; i++) {
        m_long[i] = 0;
    }
    for (i = 0; i < SHORT_BLOCKS; i++) {
        m_short[i] = 0;
    }
}

void BlockOnsetModule::Process(RoadPart* cue, int time, short* samples)
{
#line 207 "C:/psj/dev/locale/game/SignalModule.cpp"
    assert(m_blockSize == BLOCK_SIZE);
#line 129 "src/game/SignalModule.cpp"
    int center = time + BLOCK_SIZE / 2;
    int level = 0;
    int i;
    for (i = 0; i < BLOCK_SIZE; i++) {
        level += (*samples * *samples) >> 7;
        samples++;
    }
    m_long[m_longIndex] = level >> 3;
    if (++m_longIndex >= LONG_BLOCKS) {
        m_longIndex = 0;
    }
    m_short[m_shortIndex] = level >> 2;
    if (++m_shortIndex >= SHORT_BLOCKS) {
        m_shortIndex = 0;
    }
    int shortLevel = 0;
    for (i = 0; i < SHORT_BLOCKS; i++) {
        shortLevel += m_short[i];
    }
    int longLevel = 0;
    for (i = 0; i < LONG_BLOCKS; i++) {
        longLevel += m_long[i];
    }
    if (center - m_windowStart > m_window) {
        m_windowStart += m_window;
        if (m_candidate != NULL) {
            m_window = m_candidate->m_selectPeriod;
        } else {
            m_window = cue->m_selectPeriod;
        }
        EndWindow(m_windowStart);
        while (center - m_windowStart > m_window) {
            m_windowStart += m_window;
            EndWindow(m_windowStart);
        }
    }
    if (cue->m_selector != NULL) {
        if (shortLevel <= MIN_LEVEL) {
            /* too quiet */
        release:
            cue->Release();
            return;
        }
        if (m_candidate != NULL) {
            if (!IsAbove(shortLevel, longLevel, m_candidateLevel, m_candidateBackground)) {
                /* no louder than the candidate (a goto rather than a second Release() and return: the game's code shares them) */
                goto release;
            }
            m_candidate->Release();
        }
        m_candidate = cue;
        m_candidateTime = center;
        m_candidateLevel = shortLevel;
        m_candidateBackground = longLevel;
    } else {
        cue->Release();
    }
}

void BlockOnsetModule::Reset(int time)
{
    Clear();
    m_windowStart = time;
    m_window = 0;
    ResetHistory();
}

OnsetModule::OnsetModule(int time, int blockSize, int rate)
    : SignalModule(blockSize), m_windowStart(time), m_window(0), m_historyIndex(0), m_longLevel(0),
      m_shortLevel(0)
{
    ResetHistory();
    for (int i = 0; i < BLOCK_SIZE; i++) {
        m_silence[i] = 0;
    }
}

OnsetModule::~OnsetModule()
{
}

void OnsetModule::ResetHistory()
{
    for (int i = 0; i < HISTORY; i++) {
        m_history[i] = NULL;
    }
    m_longLevel = 0;
    m_shortLevel = 0;
}

void OnsetModule::Process(RoadPart* cue, int time, short* samples)
{
#line 331 "C:/psj/dev/locale/game/SignalModule.cpp"
    assert(m_blockSize == BLOCK_SIZE);
#line 224 "src/game/SignalModule.cpp"
    m_history[m_historyIndex] = samples;
    if (++m_historyIndex >= HISTORY) {
        m_historyIndex = 0;
    }
    int recentIndex = m_historyIndex - RECENT;
    if (recentIndex < 0) {
        recentIndex += HISTORY;
    }
    short* recent = m_history[recentIndex];
    if (recent == NULL) {
        recent = m_silence;
    }
    short* oldest = m_history[m_historyIndex];
    if (oldest == NULL) {
        oldest = m_silence;
    }
    int shortLevel = m_shortLevel;
    int longLevel = m_longLevel;
    for (int i = 0; i < BLOCK_SIZE;) {
        int now = time + i;
        if (now - m_windowStart >= m_window) {
            m_windowStart += m_window;
            if (m_candidate != NULL) {
                m_window = m_candidate->m_selectPeriod;
            } else {
                m_window = cue->m_selectPeriod;
            }
            EndWindow(m_windowStart);
            while (now - m_windowStart >= m_window) {
                m_windowStart += m_window;
                EndWindow(m_windowStart);
            }
        }
        int end = m_windowStart + m_window - time;
        if (end > BLOCK_SIZE) {
            end = BLOCK_SIZE;
        }
        int best = -1;
        int bestShort = 0;
        int bestLong = 1;
        for (; i < end; i++) {
            int power = samples[i] * samples[i];
            shortLevel -= (recent[i] * recent[i]) >> 9;
            shortLevel += power >> 9;
            longLevel -= (oldest[i] * oldest[i]) >> 12;
            longLevel += power >> 12;
            if (shortLevel > MIN_LEVEL && IsAbove(shortLevel, longLevel, bestShort, bestLong)) {
                best = i;
                bestShort = shortLevel;
                bestLong = longLevel;
            }
        }
        if (cue->m_selector != NULL && best >= 0) {
            if (m_candidate != NULL) {
                if (!IsAbove(bestShort, bestLong, m_candidateLevel, m_candidateBackground)) {
                    continue;
                }
                m_candidate->Release();
            }
            cue->AddRef();
            m_candidate = cue;
            m_candidateTime = time + best;
            m_candidateLevel = bestShort;
            m_candidateBackground = bestLong;
            /* (stored twice: the compiler drops this store, but it decides which register bestShort gets) */
            m_candidateLevel = bestShort;
        }
    }
    m_shortLevel = shortLevel;
    m_longLevel = longLevel;
    cue->Release();
}

void OnsetModule::Reset(int time)
{
    Clear();
    m_windowStart = time;
    m_window = 0;
    ResetHistory();
}

/* the allocator's "out of memory" string, of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/SignalModule", D_800197CC);
