#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/AnalysisScript", D_8001D3E0);

#include "Format.h"

#include "MemorySys.h"

#include <string>
#include <algorithm>

#include "Parameter.h"
#include "AnalysisScript.h"
#include "AudioStream.h"
#include "CdSys.h"
#include "Gameplay.h"
#include "InputSys.h"
#include "MemPtr.h"
#include "RandomPicks.h"
#include "VideoSys.h"

/* (in Format.cpp) */
string ToString(u_long value);

/*
 * The ChoiceParameter destructor (emitted in Parameter.cpp) makes this egcs
 * instantiate vector<string>'s __destroy_aux() here as well; the game's
 * compiler did not, so it is declared extern.
 */
extern template void __destroy_aux(string*, string*, __false_type);

/*
 * The script's debug menu: makes the road parts from its settings while
 * "Manual?" is set, and sends effects and cameras to the game by hand.
 */
class ScriptMenu : public ParameterListener {
public:
    ScriptMenu();
    ~ScriptMenu();

    void Changed(Parameter* parameter);
    /* prints the times of the round when the script is followed, and the menu */
    void Draw();
    void SetTime(int time);

    ParameterMenu m_menu;
    ChoiceParameter m_selector;
    NumberParameter<int> m_patternIndex;
    NumberParameter<int> m_distributionIndex;
    NumberParameter<float> m_roadPartPeriod; /* in seconds */
    NumberParameter<float> m_selectPeriod;   /* in seconds */
    NumberParameter<int> m_effectParameter;
    NumberParameter<float> m_effectDuration; /* in seconds */
    ActionParameter m_sendRandom;
    ActionParameter m_sendReverse;
    ActionParameter m_sendScrew;
    ActionParameter m_sendCamera;
    int m_time;
    RoadTempo* m_tempo;
    BoolParameter m_manual;
    RoadPart* m_part; /* the one made from the settings */

private:
    void Rebuild();
};

MemPtr<ScriptHeader>* AnalysisScript::s_file = NULL;
ScriptMenu* AnalysisScript::s_script = NULL;
u_long AnalysisScript::s_seed = 0;
int AnalysisScript::s_round = 1;

/* the controller the menu is on */
static const char* s_padName = "Script";

int AnalysisScript::s_speedIndex;
SpeedList* AnalysisScript::s_speeds;
int AnalysisScript::s_effectIndex;
EffectList* AnalysisScript::s_effects;
int AnalysisScript::s_partIndex;
PartList* AnalysisScript::s_parts;
int AnalysisScript::s_end;
int AnalysisScript::s_now;
int AnalysisScript::s_offset;
int AnalysisScript::s_startTime;
int AnalysisScript::s_playing;
int AnalysisScript::s_track;
int AnalysisScript::s_fixedOrder;
RoadTempo* AnalysisScript::s_tempo;
RoadDraw** AnalysisScript::s_distributionSelectors;
RoadSequence** AnalysisScript::s_patternSelectors;
ScriptTrack* AnalysisScript::s_tracks;
EffectList** AnalysisScript::s_effectLists;
PartList** AnalysisScript::s_partLists;
SpeedList** AnalysisScript::s_speedLists;
RoadList** AnalysisScript::s_patterns;
RoadOdds* AnalysisScript::s_distributions;
int* AnalysisScript::s_medleyTracks;
int* AnalysisScript::s_songTracks;
int* AnalysisScript::s_roundTracks;
ScriptHeader* AnalysisScript::s_header;
int AnalysisScript::s_starts[100];
int AnalysisScript::s_order[100];

/* the entry of `list` at `time` (the last one starting at or before it) */
template <class T>
inline int Find(const ScriptList<T>* list, const int& time)
{
    int lo = 0;
    int hi = list->count;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (list->entries[mid].time <= time) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

void AnalysisScript::FreeSelectors()
{
    for (int i = 0; i < s_header->numPatterns; i++) {
        if (s_patternSelectors[i] != NULL) {
            s_patternSelectors[i]->Release();
            s_patternSelectors[i] = NULL;
        }
    }
    for (int j = 0; j < s_header->numDistributions; j++) {
        if (s_distributionSelectors[j] != NULL) {
            s_distributionSelectors[j]->Release();
            s_distributionSelectors[j] = NULL;
        }
    }
    if (s_tempo != NULL) {
        s_tempo->Release();
        s_tempo = NULL;
    }
}

ScriptMenu::ScriptMenu()
    : m_menu(s_padName),
      m_selector("Selector Type", "none"),
      m_patternIndex("Pattern Index", 0, 1, 0, AnalysisScript::s_header->numPatterns - 1),
      m_distributionIndex("Distribution Index", 0, 1, 0, AnalysisScript::s_header->numDistributions - 1),
      m_roadPartPeriod("Road Part Period", 0.5, 0.02, 0.02),
      m_selectPeriod("Select Period", 0.75, 0.02, 0.02),
      m_effectParameter("Effect Parameter", 30, 1, 0, 100),
      m_effectDuration("Effect Duration", 20.0, 1.0, 0.0),
      m_sendRandom("Send Random Effect"),
      m_sendReverse("Send Reverse Effect"),
      m_sendScrew("Send Screw Effect"),
      m_sendCamera("Send Camera Effect"),
      m_time(0),
      m_tempo(NULL),
      m_manual("Manual?", 0),
      m_part(NULL)
{
    if (AnalysisScript::s_header->numPatterns > 0) {
        m_selector.AddChoice("pattern");
    }
    if (AnalysisScript::s_header->numDistributions > 0) {
        m_selector.AddChoice("distribution");
    }
    m_menu.Add(&m_manual);
    m_menu.Add(&m_roadPartPeriod);
    m_menu.Add(&m_selectPeriod);
    m_menu.Add(&m_selector);
    m_menu.Add(&m_patternIndex);
    m_menu.Add(&m_distributionIndex);
    m_menu.Add(&m_effectParameter);
    m_menu.Add(&m_effectDuration);
    m_menu.Add(&m_sendRandom);
    m_menu.Add(&m_sendReverse);
    m_menu.Add(&m_sendScrew);
    m_menu.Add(&m_sendCamera);
    m_manual.AddListener(this);
    m_selector.AddListener(this);
    m_patternIndex.AddListener(this);
    m_distributionIndex.AddListener(this);
    m_roadPartPeriod.AddListener(this);
    m_selectPeriod.AddListener(this);
    m_effectParameter.AddListener(this);
    m_effectDuration.AddListener(this);
    m_sendRandom.AddListener(this);
    m_sendReverse.AddListener(this);
    m_sendScrew.AddListener(this);
    m_sendCamera.AddListener(this);
    Rebuild();
}

ScriptMenu::~ScriptMenu()
{
    if (m_part != NULL) {
        m_part->Release();
    }
    if (m_tempo != NULL) {
        m_tempo->Release();
    }
}

void ScriptMenu::Changed(Parameter* parameter)
{
    ScriptEvent event;
    if (parameter == &m_manual) {
        return;
    }
    if (parameter == &m_sendRandom) {
        if (m_tempo != NULL) {
            m_tempo->Release();
        }
        m_tempo = new RoadTempo(m_time, (int)(m_effectDuration.m_value * AudioStream::SampleRate()), m_effectParameter.m_value, m_time);
        Rebuild();
        event.type = ScriptEvent::EFFECT;
        event.value = ScriptEvent::TEMPO;
        event.time = m_time;
        event.length = (int)(m_effectDuration.m_value * AudioStream::SampleRate());
        Gameplay::AddEvent(&event);
    } else if (parameter == &m_sendReverse) {
        event.type = ScriptEvent::EFFECT;
        event.value = ScriptEvent::REVERSE;
        event.time = m_time;
        event.length = (int)(m_effectDuration.m_value * AudioStream::SampleRate());
        Gameplay::AddEvent(&event);
    } else if (parameter == &m_sendScrew) {
        event.type = ScriptEvent::EFFECT;
        event.value = ScriptEvent::SCREW;
        event.time = m_time;
        event.length = (int)(m_effectDuration.m_value * AudioStream::SampleRate());
        Gameplay::AddEvent(&event);
    } else if (parameter == &m_sendCamera) {
        event.type = ScriptEvent::CAMERA;
        event.time = m_time;
        event.value = m_effectParameter.m_value;
        Gameplay::AddEvent(&event);
    } else {
        Rebuild();
    }
}

void ScriptMenu::Draw()
{
    if (InputSys::IsPlayerName(s_padName)) {
        if (!m_manual.m_value && AnalysisScript::s_playing) {
            /* the seconds into the round, and into its track */
            string times = ToString((u_long)(AudioStream::GetPosition() - AnalysisScript::s_startTime) / AudioStream::SampleRate()) + ' '
                + ToString((u_long)(AudioStream::GetPosition() - AnalysisScript::s_startTime + AnalysisScript::s_offset) / AudioStream::SampleRate())
                + '\n';
            VideoSys::Print(times.c_str());
        }
    }
    m_menu.Draw();
}

void ScriptMenu::SetTime(int time)
{
    m_time = time;
    if (m_tempo != NULL && m_tempo->IsOver(time)) {
        m_tempo->Release();
        m_tempo = NULL;
        Rebuild();
    }
}

void ScriptMenu::Rebuild()
{
    if (m_part != NULL) {
        m_part->Release();
    }
    RoadSelector* selector = NULL;
    switch ((ScriptPart::Type)m_selector.GetIndex()) {
    case ScriptPart::NONE:
        break;
    case ScriptPart::PATTERN:
        if (AnalysisScript::s_header->numPatterns > 0) {
            selector = new RoadSequence(AnalysisScript::s_patterns[m_patternIndex.m_value]);
        } else {
            /* no patterns: a distribution instead */
            selector = new RoadDraw(&AnalysisScript::s_distributions[m_distributionIndex.m_value], AnalysisScript::s_round, AnalysisScript::s_seed);
        }
        break;
    case ScriptPart::DISTRIBUTION:
        selector = new RoadDraw(&AnalysisScript::s_distributions[m_distributionIndex.m_value], AnalysisScript::s_round, AnalysisScript::s_seed);
        break;
    }
    if (m_tempo != NULL) {
        m_tempo->AddRef();
    }
    m_part = new RoadPart((int)(m_roadPartPeriod.m_value * AudioStream::SampleRate()), (int)(m_selectPeriod.m_value * AudioStream::SampleRate()), selector, m_tempo);
}

void AnalysisScript::FindCurrent()
{
    s_partIndex = Find(s_parts, s_now);
    const ScriptPart* part = &s_parts->entries[s_partIndex];
    switch (part->type) {
    case ScriptPart::NONE:
        break;
    case ScriptPart::PATTERN:
        if (s_patternSelectors[part->index] == NULL) {
            s_patternSelectors[part->index] = new RoadSequence(s_patterns[part->index]);
        }
        break;
    case ScriptPart::DISTRIBUTION:
        if (s_distributionSelectors[part->index] == NULL) {
            s_distributionSelectors[part->index] = new RoadDraw(&s_distributions[part->index], s_round, s_seed);
        }
        break;
    }
    s_speedIndex = Find(s_speeds, s_now);
}

void AnalysisScript::SendTempo(const ScriptEffect* effect)
{
    if (effect->tempoLength > 0) {
        if (s_tempo != NULL) {
            s_tempo->Release();
        }
        s_tempo = new RoadTempo(s_startTime + effect->time - s_offset, effect->tempoLength, effect->tempoPattern, effect->time);
        ScriptEvent event;
        event.type = ScriptEvent::EFFECT;
        event.time = s_startTime + effect->time - s_offset;
        event.value = ScriptEvent::TEMPO;
        event.length = effect->tempoLength;
        Gameplay::AddEvent(&event);
    }
}

void AnalysisScript::SendReverse(const ScriptEffect* effect)
{
    if (effect->reverseLength > 0) {
        ScriptEvent event;
        event.type = ScriptEvent::EFFECT;
        event.time = s_startTime + effect->time - s_offset;
        event.value = ScriptEvent::REVERSE;
        event.length = effect->reverseLength;
        Gameplay::AddEvent(&event);
    }
}

void AnalysisScript::SendScrew(const ScriptEffect* effect)
{
    if (effect->screwLength > 0) {
        ScriptEvent event;
        event.type = ScriptEvent::EFFECT;
        event.time = s_startTime + effect->time - s_offset;
        event.value = ScriptEvent::SCREW;
        event.length = effect->screwLength;
        Gameplay::AddEvent(&event);
    }
}

void AnalysisScript::SendCamera(const ScriptEffect* effect)
{
    if (effect->camera >= 0) {
        ScriptEvent event;
        event.type = ScriptEvent::CAMERA;
        event.time = s_startTime + effect->time - s_offset;
        event.value = effect->camera;
        Gameplay::AddEvent(&event);
    }
}

void AnalysisScript::CatchUp()
{
    int camera = -1;
    for (; s_effectIndex < s_effects->count; s_effectIndex++) {
        const ScriptEffect* effect = &s_effects->entries[s_effectIndex];
        if (s_now < effect->time) {
            break;
        }
        if (s_now < effect->tempoLength + effect->time) {
            SendTempo(effect);
        }
        if (s_now < effect->reverseLength + effect->time) {
            SendReverse(effect);
        }
        if (s_now < effect->screwLength + effect->time) {
            SendScrew(effect);
        }
        if (effect->camera >= 0) {
            camera = s_effectIndex;
        }
    }
    if (camera >= 0) {
        SendCamera(&s_effects->entries[camera]);
    }
}

void AnalysisScript::SendEffects()
{
    while (s_effectIndex < s_effects->count) {
        const ScriptEffect* effect = &s_effects->entries[s_effectIndex];
        if (s_now < effect->time) {
            break;
        }
        SendTempo(effect);
        SendReverse(effect);
        SendScrew(effect);
        SendCamera(effect);
        s_effectIndex++;
    }
}

void AnalysisScript::Load(const char* name)
{
    MemPtr<ScriptHeader>* file = new MemPtr<ScriptHeader>(name);
    u_char* p = (u_char*)file->Data();
    s_header = (ScriptHeader*)p;
    s_file = file;
    p += sizeof(ScriptHeader);
    s_roundTracks = (int*)p;
    p += s_header->numRounds * sizeof(int);
    s_songTracks = (int*)p;
    p += s_header->numSongs * sizeof(int);
    s_medleyTracks = (int*)p;
    p += s_header->numMedleys * sizeof(int);
    s_patterns = new RoadList*[s_header->numPatterns];
    for (int i = 0; i < s_header->numPatterns; i++) {
        s_patterns[i] = (RoadList*)p;
        p += sizeof(int) + s_patterns[i]->count * sizeof(int);
    }
    s_distributions = (RoadOdds*)p;
    p += s_header->numDistributions * sizeof(RoadOdds);
    s_speedLists = new SpeedList*[s_header->numSpeedLists];
    for (int j = 0; j < s_header->numSpeedLists; j++) {
        s_speedLists[j] = (SpeedList*)p;
        p += sizeof(int) + s_speedLists[j]->count * sizeof(ScriptSpeed);
    }
    s_partLists = new PartList*[s_header->numPartLists];
    for (int k = 0; k < s_header->numPartLists; k++) {
        s_partLists[k] = (PartList*)p;
        p += sizeof(int) + s_partLists[k]->count * sizeof(ScriptPart);
    }
    s_effectLists = new EffectList*[s_header->numEffectLists];
    for (int l = 0; l < s_header->numEffectLists; l++) {
        s_effectLists[l] = (EffectList*)p;
        p += sizeof(int) + s_effectLists[l]->count * sizeof(ScriptEffect);
    }
    s_tracks = (ScriptTrack*)p;
    s_patternSelectors = new RoadSequence*[s_header->numPatterns];
    for (int m = 0; m < s_header->numPatterns; m++) {
        s_patternSelectors[m] = NULL;
    }
    s_distributionSelectors = new RoadDraw*[s_header->numDistributions];
    for (int n = 0; n < s_header->numDistributions; n++) {
        s_distributionSelectors[n] = NULL;
    }
    s_tempo = NULL;
    s_effectIndex = 0;
    s_script = new ScriptMenu;
}

void AnalysisScript::Free()
{
    delete s_script;
    delete[] s_patterns;
    delete[] s_speedLists;
    delete[] s_partLists;
    delete[] s_effectLists;
    FreeSelectors();
    delete[] s_patternSelectors;
    delete[] s_distributionSelectors;
    delete s_file;
}

void AnalysisScript::UpdateMenu()
{
    s_script->m_menu.Update();
}

void AnalysisScript::DrawMenu()
{
    s_script->Draw();
}

int AnalysisScript::SongOf(int homeDisc, int round)
{
    return s_order[round - 1];
}

int AnalysisScript::TrackSamples(int track)
{
    return CdSys::GetFirstAudioTrack()[track - 1].Length() / SECTORS_PER_SECOND * AudioStream::SampleRate();
}

/* the lists of the track `s_track`, and where its script ends */
inline void AnalysisScript::SetTrackLists()
{
    const ScriptTrack* track = &s_tracks[s_track];
    s_parts = s_partLists[track->parts];
    s_speeds = s_speedLists[track->speeds];
    s_effects = s_effectLists[track->effects];
    s_end = max(s_parts->entries[s_parts->count - 1].time, s_speeds->entries[s_speeds->count - 1].time);
    s_end = max(s_end, s_effects->entries[s_effects->count - 1].time);
}

void AnalysisScript::Prepare(int fixedOrder, int count, u_long seed)
{
    if (count > 100) {
        count = 100;
    }
    s_fixedOrder = fixedOrder;
    s_seed = seed;
    s_playing = 0;
    s_effectIndex = 0;
    if (fixedOrder) {
        for (int i = 0; i <= count; i++) {
            s_order[i] = i;
            s_starts[i] = 0;
        }
    } else if (s_header->numMedleys > 0) {
        RandomPicks(0, count - 1, count, seed, s_order);
        s_track = s_medleyTracks[0];
        SetTrackLists();
#line 648 "C:/psj/dev/locale/game/AnalysisScript.cpp"
        assert(s_end > 0);
#line 524 "src/game/AnalysisScript.cpp"
        for (int j = 1; j <= count; j++) {
            s_starts[s_order[j - 1]] = TrackSamples(j);
        }
        /* the time between the songs, shared out at random */
        int rest = s_end;
        for (int k = 0; k < count; k++) {
            rest -= s_starts[k];
        }
        if (rest < 0) {
            rest = 0;
        }
        int gaps[count];
        RandomPicks(0, rest, count, seed, gaps);
        sort(gaps, gaps + count);
        int done = s_starts[0];
        s_starts[0] = gaps[0];
        for (int l = 1; l < count; l++) {
            int start = done;
            done += s_starts[l];
            s_starts[l] = start + gaps[l];
        }
    } else if (s_header->numSongs > 0) {
        for (int m = 0; m < count; m++) {
            s_starts[m] = 0;
        }
        RandomPicks(0, s_header->numSongs - 1, count, seed, s_order);
    } else {
        /* no tracks at all */
#line 721 "C:/psj/dev/locale/game/AnalysisScript.cpp"
        assert(0);
#line 555 "src/game/AnalysisScript.cpp"
    }
}

void AnalysisScript::Start(int round, int startTime)
{
    if (round > 100) {
        round = 100;
    }
    FreeSelectors();
    s_round = round;
    s_startTime = startTime;
    s_playing = 1;
    s_effectIndex = 0;
    if (s_fixedOrder) {
        s_track = s_roundTracks[round - 1];
        s_offset = 0;
    } else if (s_header->numMedleys > 0) {
        s_track = s_medleyTracks[0];
        s_offset = s_starts[s_order[round - 1]];
    } else if (s_header->numSongs > 0) {
        s_track = s_songTracks[s_order[round - 1]];
        s_offset = 0;
    } else {
        /* no tracks at all */
#line 759 "C:/psj/dev/locale/game/AnalysisScript.cpp"
        assert(0);
#line 582 "src/game/AnalysisScript.cpp"
    }
    SetTrackLists();
    s_now = s_offset;
    CatchUp();
}

RoadPart* AnalysisScript::NextPart(int time, int select, Fixed speed)
{
    s_script->SetTime(time);
    if (s_script->m_manual.m_value) {
        s_script->m_part->AddRef();
        return s_script->m_part;
    }
    if (!s_playing) {
        int period = AudioStream::SampleRate();
        int selectPeriod = (int)(period * 0.1);
        return new RoadPart(period, selectPeriod, NULL, NULL);
    }
    s_now = min(max(0, time - s_startTime) + s_offset, s_end);
    SendEffects();
    FindCurrent();
    int period;
    int selectPeriod;
    if (s_speedIndex == s_speeds->count - 1) {
        const ScriptSpeed* last = &s_speeds->entries[s_speedIndex];
        period = last->period;
        selectPeriod = last->selectPeriod;
    } else {
        const ScriptSpeed* now = &s_speeds->entries[s_speedIndex];
        int since = s_now - now->time;
        period = now->period + (now->periodSlope * since >> 12);
        selectPeriod = now->selectPeriod + (now->selectPeriodSlope * since >> 12);
    }
    period = max(1, (int)((long long)period * speed.Raw() >> 16));
    selectPeriod = max(1, (int)((long long)selectPeriod * speed.Raw() >> 16));
    RoadSelector* selector = NULL;
    const ScriptPart* part = &s_parts->entries[s_partIndex];
    if (select) {
        switch (part->type) {
        case ScriptPart::NONE:
            break;
        case ScriptPart::PATTERN:
            selector = s_patternSelectors[part->index];
            selector->AddRef();
            break;
        case ScriptPart::DISTRIBUTION:
            selector = s_distributionSelectors[part->index];
            selector->AddRef();
            break;
        }
    }
    if (s_tempo != NULL) {
        s_tempo->AddRef();
    }
    return new RoadPart(period, selectPeriod, selector, s_tempo);
}

RoadPart* AnalysisScript::NextPart(int time, int period, int selectPeriod, int select)
{
    s_script->SetTime(time);
    if (s_script->m_manual.m_value) {
        s_script->m_part->AddRef();
        return s_script->m_part;
    }
    if (!s_playing) {
        return new RoadPart(period, selectPeriod, NULL, NULL);
    }
    s_now = min(max(0, time - s_startTime) + s_offset, s_end);
    SendEffects();
    RoadSelector* selector = NULL;
    FindCurrent();
    const ScriptPart* part = &s_parts->entries[s_partIndex];
    if (select) {
        switch (part->type) {
        case ScriptPart::NONE:
            break;
        case ScriptPart::PATTERN:
            selector = s_patternSelectors[part->index];
            selector->AddRef();
            break;
        case ScriptPart::DISTRIBUTION:
            selector = s_distributionSelectors[part->index];
            selector->AddRef();
            break;
        }
    }
    return new RoadPart(period, selectPeriod, selector, NULL);
}
