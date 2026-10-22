#include "common.h"

#include <libapi.h>
#undef delete /* kernel.h's */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Format.h"
#include "MemorySys.h"
#include <string>
#include "SignalSys.h"
#include "SignalAnalyzer.h"
#include "SignalModule.h"
#include "AnalysisScript.h"
#include "Glue.h"

StaticQueue<AudioEvent> SignalSys::s_queue(QUEUE_SIZE);
int SignalSys::s_fixedCue = 0;
int SignalSys::s_select = 1;
Fixed SignalSys::s_scale = Fixed::Int(1);
SignalAnalyzer* SignalSys::s_analyzer = NULL;
int SignalSys::s_fixedPeriod = 0;
int SignalSys::s_fixedSelectPeriod = 0;
int SignalSys::s_hadSelector = 0;

void SignalSys::Init(float distance, float speed, int time)
{
    AnalysisScript::Load("script/system3.fsl");
    AnalysisScript::Prepare(1, 1, 0);
    s_analyzer = new OnsetAnalyzer(AudioStream::SampleRate(), AudioStream::BlockSize(), distance, speed, time);
    AudioStream::SetReceiver(&s_queue);
    s_hadSelector = 0;
}

void SignalSys::Quit()
{
    AudioStream::ClearReceiver(&s_queue);
    delete s_analyzer;
    AnalysisScript::Free();
}

void SignalSys::Update(int frame)
{
    s_analyzer->BeginFrame();
    AnalysisScript::UpdateMenu();
    SwEnterCriticalSection();
    u_int count = s_queue.Count();
    SwExitCriticalSection();
    if (count != 0) {
        RoadPart* cue;
        if (s_fixedCue) {
            cue = AnalysisScript::NextPart(s_queue.Front().position, s_fixedPeriod, s_fixedSelectPeriod, s_select);
        } else {
            cue = AnalysisScript::NextPart(s_queue.Front().position, s_select, s_scale);
            int hasSelector = cue->m_selector != NULL;
            if (s_hadSelector && !hasSelector) {
                Glue::BufferQuiet(s_queue.Front().position);
            }
            s_hadSelector = hasSelector;
        }
        for (u_int i = 0; i < count; i++) {
            AudioEvent& event = s_queue.Front();
            if (event.trackStarted) {
                Glue::BufferTrackStart(event.track, event.position, event.trackLength);
                AnalysisScript::Start(event.track, event.position);
                cue->Release();
                if (s_fixedCue) {
                    cue = AnalysisScript::NextPart(s_queue.Front().position, s_fixedPeriod, s_fixedSelectPeriod, s_select);
                } else {
                    cue = AnalysisScript::NextPart(s_queue.Front().position, s_select, s_scale);
                    s_hadSelector = cue->m_selector != NULL;
                }
                s_analyzer->TrackStarted(event.position, event.track);
            }
            if (event.trackEnded) {
                Glue::BufferTrackEnd(event.position);
                s_analyzer->TrackEnded();
            }
            cue->AddRef();
            s_analyzer->Process(cue, event.position, event.samples);
            SwEnterCriticalSection();
            s_queue.Pop();
            SwExitCriticalSection();
        }
        cue->Release();
    }
    AnalysisScript::DrawMenu();
    s_analyzer->EndFrame(frame);
}

void SignalSys::SetFixedCue(int period, int selectPeriod)
{
    s_fixedCue = 1;
    s_fixedPeriod = period;
    s_fixedSelectPeriod = selectPeriod;
}

void SignalSys::ClearFixedCue()
{
    s_fixedCue = 0;
}

void SignalSys::ClearSelect()
{
    s_select = 0;
}

void SignalSys::SetSelect()
{
    s_select = 1;
}

int SignalSys::TrackOrder(int track, int homeDisc)
{
    return AnalysisScript::SongOf(homeDisc, track);
}

void SignalSys::SetDisc(int id, int numTracks, int homeDisc)
{
    /* the game's own disc needs no key */
    if (homeDisc != 0) {
        id = 0;
    } else {
        id ^= 0xCBF633B1;
    }
    AnalysisScript::Prepare(homeDisc, numTracks, id);
}

/* the allocator's "out of memory" string, of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/SignalSys", D_80019950);
