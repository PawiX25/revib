#include "common.h"

#include <stdlib.h>

#include "MemorySys.h"
#include <string> /* nothing here uses it, but its empty string is first in the file's small data */
#include "Format.h"
#include "SignalAnalyzer.h"
#include "SignalModule.h"

/* the allocator's "out of memory" string, of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/SignalAnalyzer", D_8001986C);

SignalAnalyzer::SignalAnalyzer(int rate, int blockSize) : m_rate(rate), m_blockSize(blockSize), m_module(NULL)
{
}

SignalAnalyzer::~SignalAnalyzer()
{
}

void SignalAnalyzer::Process(RoadPart* cue, int time, short* samples)
{
    m_module->Process(cue, time, samples);
}

OnsetAnalyzer::OnsetAnalyzer(int rate, int blockSize, float distance, float speed, int time)
    : SignalAnalyzer(rate, blockSize)
{
    m_onsets = new OnsetModule(time, blockSize, m_rate);
    m_module = m_onsets;
}

OnsetAnalyzer::~OnsetAnalyzer()
{
    delete m_onsets;
}

void OnsetAnalyzer::TrackStarted(int time, int track)
{
    m_onsets->Reset(time);
}
