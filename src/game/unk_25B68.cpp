#include "common.h"

#include "Movie.h"
#include "ModelMovie.h"
#include "AudioSys.h"
#include "Menu.h"
#include "VideoSys.h"

/*
 * The kiosk version (MAIN_K) is built from this file too, with KIOSK
 * defined. It makes the kiosk demo instead of the title, so it leaves out
 * the constructors of the title and of its logo, and the title's
 * IntroOver() and Leave(); the title executable leaves out the kiosk demo's
 * constructor.
 */

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/game/nonmatchings/unk_25B68", D_800197D8);

int ToGameFrames(int time)
{
    if (time == FRAMES_LOOP) {
        return FRAMES_LOOP;
    }
    if (time == FRAMES_STOP) {
        return FRAMES_STOP;
    }
    int result = (VideoSys::FrameRate() / 30 * time).Integer();
    result -= VideoSys::NumBuffers();
    if (result < 0) {
        result = 0;
    }
    return result;
}

/* A model movie of a scene, played from `time` (in 30ths of a second) on. */
struct MovieCue {
    const char* file; /* "" ends the list */
    int time;
    int model;        /* one of the element's models */
    int label;
    int frame;        /* `time` in frames of the game, worked out at the start */
};

/* how jittery a model's lines are over a while */
struct JitterCue {
    int model;
    int time;   /* in 30ths of a second; -1 ends the list */
    int length;
    int jitter; /* whole pixels */
    int type;
};

#define JITTER_FALLING -88  /* from `jitter` down to none */
#define JITTER_RISING -89   /* from none up to `jitter` */
#define JITTER_CONSTANT -90

/* lines jitter this much unless a cue says otherwise */
#define JITTER_DEFAULT 6.0

/* when the title shows, at the end of the intro (in 30ths of a second) */
#define TITLE_TIME 821

/*
 * Model movies played one after the other, each on one of the element's
 * models. When the scene is left, the lines scatter more and more.
 */
class ModelElement : public MovieElement {
public:
    ModelElement(MovieCue* cues, const char** models, JitterCue* jitters);
    ~ModelElement();

    void Update(int leaving);
    int IsFinished();
    void Seek(int label);
    void Reset();

private:
    void UpdateJitter(int frame, int leaving);

    ModelMovie* m_movies[10];
    TmdModel* m_models[10];
    MovieCue* m_cues;
    const char** m_modelFiles; /* "" ends the list */
    JitterCue* m_jitters;
    int m_scatter;     /* frames of scattering */
    int m_scatterStep; /* +1 going away, -1 coming back */
    int m_numCues;
};

ModelElement::ModelElement(MovieCue* cues, const char** models, JitterCue* jitters)
    : m_cues(cues), m_modelFiles(models), m_jitters(jitters)
{
    m_scatter = 20;
    m_scatterStep = -1;
    int i;
    for (i = 0; *m_modelFiles[i] != '\0'; i++) {
        m_models[i] = LoadTmdModel(m_modelFiles[i]);
        m_models[i]->SetJitter(JITTER_DEFAULT, 1);
    }
    int n;
    for (n = 0; *m_cues[n].file != '\0'; n++) {
        m_cues[n].frame = ToGameFrames(m_cues[n].time);
    }
    m_cues[n].frame = ToGameFrames(m_cues[n].time);
    for (n = 0; *m_cues[n].file != '\0'; n++) {
        m_movies[n] = new ModelMovie(m_models[m_cues[n].model], MemPtr<short>(m_cues[n].file), 0);
        if (m_cues[n + 1].frame == FRAMES_LOOP) {
            m_movies[n]->m_loop = 1;
        }
    }
    m_cues[n].frame = ToGameFrames(m_cues[n].time);
    m_numCues = n;
}

void ModelElement::UpdateJitter(int frame, int leaving)
{
    int t = 0;
    Fixed jitter = JITTER_DEFAULT;
    for (int i = 0; m_jitters[i].time != -1; i++) {
        int start = ToGameFrames(m_jitters[i].time);
        int end = ToGameFrames(m_jitters[i].time + m_jitters[i].length);
        if (frame >= start && frame <= end) {
            if (m_jitters[i].type == JITTER_CONSTANT) {
                jitter = Fixed::Int(m_jitters[i].jitter);
            } else {
                int length = end - start;
                if (m_jitters[i].type == JITTER_FALLING) {
                    t = end - frame;
                } else if (m_jitters[i].type == JITTER_RISING) {
                    t = frame - start;
                }
                jitter = Fixed::Int(t * m_jitters[i].jitter / length);
            }
        }
        m_models[m_jitters[i].model]->SetJitter(jitter, 1);
    }
    if (leaving) {
        m_scatter += m_scatterStep;
        int scatter = PerFrames(60, m_scatter);
        Fixed scattered = jitter + Fixed::Int(scatter * scatter * 8);
        for (int k = 0; *m_modelFiles[k] != '\0'; k++) {
            m_models[k]->SetJitter(scattered, 1);
        }
    } else if (m_scatter != 0) {
        m_scatter += m_scatterStep;
        int scatter = PerFrames(60, m_scatter);
        Fixed scattered = jitter + Fixed::Int(scatter * scatter * 8);
        for (int k = 0; *m_modelFiles[k] != '\0'; k++) {
            m_models[k]->SetJitter(scattered, 1);
        }
        if (m_scatter == 0) {
            m_scatterStep = 1;
        }
    }
}

ModelElement::~ModelElement()
{
    int i;
    for (i = 0; *m_cues[i].file != '\0'; i++) {
        delete m_movies[i];
    }
    for (i = 0; *m_modelFiles[i] != '\0'; i++) {
        delete m_models[i];
    }
}

void ModelElement::Update(int leaving)
{
    int draw = 0;
    if (m_frame == 0) {
        m_scatter = 0;
        m_scatterStep = 1;
    }
    UpdateJitter(m_frame, leaving);
    if (m_cues[m_cue].frame == FRAMES_STOP) {
        return;
    }
    if (m_frame >= m_cues[m_cue].frame) {
        draw = 1;
    }
    if (m_cues[m_cue + 1].frame == m_frame) {
        m_cue++;
    }
    if (m_cues[m_cue + 1].frame == FRAMES_STOP && m_movies[m_cue]->m_finished) {
        return;
    }
    if (draw) {
        m_movies[m_cue]->Draw();
    }
    m_frame++;
}

int ModelElement::IsFinished()
{
    if (m_cues[m_numCues].frame == FRAMES_LOOP) {
        return 0;
    }
    int finished = 0;
    if (m_cue == m_numCues - 1) {
        finished = m_movies[m_cue]->m_finished != 0;
    }
    return finished;
}

void ModelElement::Seek(int label)
{
    for (int i = 0; *m_cues[i].file != '\0'; i++) {
        if (m_cues[i].label == label) {
            m_frame = m_cues[i].frame;
            m_cue = i;
            return;
        }
    }
}

void ModelElement::Reset()
{
    m_cue = 0;
    m_frame = 0;
    for (int i = 0; *m_cues[i].file != '\0'; i++) {
        m_movies[i]->Rewind();
    }
}

/* A camera movie of a scene, played from `time` (in 30ths of a second) on. */
struct CameraCue {
    const char* file; /* "" ends the list */
    int time;
    int label;
    int frame;        /* `time` in frames of the game, worked out at the start */
};

/* Camera movies played one after the other. */
class CameraElement : public MovieElement {
public:
    CameraElement(CameraCue* cues);
    ~CameraElement();

    void Update(int leaving);
    int IsFinished();
    void Seek(int label);
    void Reset();

private:
    CameraMovie* m_cameras[10];
    CameraCue* m_cues;
    int m_numCues;
};

CameraElement::CameraElement(CameraCue* cues) : m_cues(cues)
{
    int n = 0;
    for (int i = 0; *m_cues[i].file != '\0'; i++, n++) {
        m_cues[i].frame = ToGameFrames(m_cues[i].time);
        m_cameras[n] = new CameraMovie(MemPtr<short>(m_cues[i].file), 0);
        m_cameras[n]->Rewind();
        m_cameras[n]->SetSpeed(1.0);
    }
    m_numCues = n;
}

CameraElement::~CameraElement()
{
    for (int i = 0; *m_cues[i].file != '\0'; i++) {
        delete m_cameras[i];
    }
}

void CameraElement::Update(int leaving)
{
    if (m_cues[m_cue].frame == FRAMES_STOP) {
        return;
    }
    int show = m_frame >= m_cues[m_cue].frame;
    if (m_cues[m_cue + 1].frame == m_frame) {
        m_cue++;
    }
    if (show) {
        m_cameras[m_cue]->Update(0);
    }
    m_frame++;
}

int CameraElement::IsFinished()
{
    if (m_cues[m_numCues].frame == FRAMES_LOOP) {
        return 0;
    }
    int finished = 0;
    if (m_cue == m_numCues - 1) {
        finished = m_cameras[m_cue]->m_finished != 0;
    }
    return finished;
}

void CameraElement::Seek(int label)
{
    for (int i = 0; *m_cues[i].file != '\0'; i++) {
        if (m_cues[i].label == label) {
            m_frame = m_cues[i].frame;
            m_cue = i;
            return;
        }
    }
}

void CameraElement::Reset()
{
    m_cue = 0;
    m_frame = 0;
    for (int i = 0; *m_cues[i].file != '\0'; i++) {
        m_cameras[i]->Rewind();
    }
}

/* A sound of a scene, played at `time` (in 30ths of a second). */
struct SoundCue {
    int sound; /* program << 4 | tone, in the loaded sound bank; -1 ends the list */
    int time;
    int label;
    int frame; /* `time` in frames of the game, worked out at the start */
};

/* Sounds played one after the other. */
class SoundElement : public MovieElement {
public:
    SoundElement(SoundCue* cues);
    ~SoundElement() {}

    void Update(int leaving);
    int IsFinished();
    void Reset();
    void Seek(int label);

private:
    SoundCue* m_cues;
};

SoundElement::SoundElement(SoundCue* cues) : m_cues(cues)
{
    for (int i = 0; m_cues[i].sound != -1; i++) {
        m_cues[i].frame = ToGameFrames(m_cues[i].time);
    }
}

void SoundElement::Update(int leaving)
{
    if (m_cues[m_cue].frame == FRAMES_LOOP) {
        return;
    }
    if (m_cues[m_cue].frame == m_frame) {
        Sound sound(m_cues[m_cue].sound >> 4, m_cues[m_cue].sound & 0xF);
        m_cue++;
        AudioSys::PlaySound(&sound, 0, 64, -1);
    }
    m_frame++;
}

int SoundElement::IsFinished()
{
    if (m_cues[m_cue].frame == FRAMES_LOOP) {
        return 0;
    }
    return !AudioSys::IsPlaying(0);
}

void SoundElement::Reset()
{
    m_cue = 0;
    m_frame = 0;
    AudioSys::StopSound(0);
}

void SoundElement::Seek(int label)
{
    int i;
    for (i = 0; m_cues[i].sound > 0; i++) {
        if (m_cues[i].label == label) {
            m_frame = m_cues[i].frame;
            m_cue = i;
            Sound sound(m_cues[i].sound >> 4, m_cues[i].sound & 0xF);
            AudioSys::PlaySound(&sound, 0, 64, -1);
            return;
        }
    }
    m_cue = i;
}

/* two file names no code refers to */
INCLUDE_RODATA("asm/game/nonmatchings/unk_25B68", D_80019840);

/* The logo, shown once the intro is over, and gone while the scene is left. */
class LogoElement : public MovieElement {
public:
    LogoElement();
    ~LogoElement();

    void Update(int leaving);
    int IsFinished();
    void Seek(int label);
    void Reset();

private:
    void Scatter(int leaving);

    ModelMovie* m_movie;
    TmdModel* m_model;
    int m_start;       /* the frame it appears at */
    int m_scatter;     /* frames of scattering; the logo is hidden meanwhile */
    int m_scatterStep; /* +1 going away, -1 coming back */
    int m_unused;
};

#ifndef KIOSK
LogoElement::LogoElement() : m_scatter(20), m_scatterStep(-1), m_unused(0)
{
    m_start = ToGameFrames(TITLE_TIME);
    m_model = LoadTmdModel("movie/title/logo.tmd");
    m_model->SetJitter(0.0, 1);
    m_movie = new ModelMovie(m_model, MemPtr<short>("movie/title/logo.anm"), 0);
}
#else
/* the kiosk version keeps the logo's file names, without the code loading it */
INCLUDE_RODATA("asm/main_k/nonmatchings/unk_25B68", D_80019830);
#endif

void LogoElement::Scatter(int leaving)
{
    if (leaving) {
        m_scatter += m_scatterStep;
    } else if (m_scatter != 0) {
        m_scatter += m_scatterStep;
        if (m_scatter == 0) {
            m_scatterStep = 1;
        }
    }
}

LogoElement::~LogoElement()
{
    delete m_model;
    delete m_movie;
}

void LogoElement::Update(int leaving)
{
    if (m_frame == 0) {
        m_scatter = 0;
        m_scatterStep = 1;
    }
    if (++m_frame > m_start) {
        Scatter(leaving);
        m_movie->m_position = 0.0;
        if (m_scatter == 0) {
            m_movie->Draw();
        }
    }
}

int LogoElement::IsFinished()
{
    return 1;
}

void LogoElement::Seek(int label)
{
    m_frame = m_start - 1;
}

void LogoElement::Reset()
{
    m_frame = 0;
    m_movie->Rewind();
}

/* the lines' jitter in the title: the default all along */
JitterCue s_titleJitter[] = {
    { 0, -1 },
};

CameraCue s_titleCameras[] = {
    { "movie/title/t01.anc", 0 },
    { "movie/title/t02.anc", 446, 1 },
    { "", FRAMES_LOOP },
};

const char* s_titleModels[] = {
    "movie/title/t01.tmd",
    "movie/title/t02.tmd",
    "movie/title/t03.tmd",
    "",
};

MovieCue s_titleMovies[] = {
    { "movie/title/t011.anm", 0, 0 },
    { "movie/title/t012.anm", 115, 0 },
    { "movie/title/t013.anm", 230, 0 },
    { "movie/title/t014.anm", 335, 0 },
    { "movie/title/t021.anm", 446, 1 },
    { "movie/title/t022.anm", 497, 1 },
    { "movie/title/t03a.anm", TITLE_TIME, 2, 1 },
    { "", FRAMES_LOOP, -1 },
};

const char* s_vibriModels[] = {
    "movie/title/vibri.tmd",
    "",
};

MovieCue s_vibriMovies[] = {
    { "movie/title/t022v.anm", 497, 0 },
    { "movie/title/v_title.anm", TITLE_TIME, 0, 1 },
    { "", FRAMES_LOOP, -1 },
};

SoundCue s_titleSounds[] = {
    { 0x110, 0 },
    { 0x111, 446, 1 },
    { 0x112, 497 },
    { -1, FRAMES_LOOP },
};

/* when the kiosk demo's shots start, in 30ths of a second */
static int s_kiosk1Shot2 = 480;
static int s_kiosk2 = 938;
static int s_kiosk2Shot2 = 1496;
static int s_kiosk3 = 2104;
static int s_kiosk3Shot2 = 2515;
static int s_kioskEnd = 3025;

const char* s_kioskVibriModels[] = {
    "movie/kiosk2/model.tmd",
    "",
};

CameraCue s_kioskCameras[] = {
    { "movie/kiosk1/k1_01.anc", 0, 0, 0 },
    { "movie/kiosk1/k1_02.anc", s_kiosk1Shot2 },
    { "movie/kiosk2/k2_01.anc", s_kiosk2, 1 },
    { "movie/kiosk2/k2_02.anc", s_kiosk2Shot2 },
    { "movie/kiosk3/k3_01.anc", s_kiosk3, 2 },
    { "movie/kiosk3/k3_02.anc", s_kiosk3Shot2 },
    { "", FRAMES_STOP, 3 },
};

JitterCue s_kioskVibriJitter[] = {
    { 0, 0, 20, 1600, JITTER_FALLING },
    { 0, 480, 120, 40, JITTER_CONSTANT },
    { 0, s_kiosk2 - 21, 20, 1600, JITTER_RISING },
    { 0, s_kiosk2, 20, 1600, JITTER_FALLING },
    { 0, s_kiosk3 - 21, 20, 600, JITTER_RISING },
    { 0, s_kiosk3, 20, 1600, JITTER_FALLING },
    { 0, s_kioskEnd - 21, 40, 1000, JITTER_RISING },
    { 0, -1, 0, 0, 0 },
};

MovieCue s_kioskVibriMovies[] = {
    { "movie/kiosk1/k1_v01.anm", 0, 0, 0, 0 },
    { "movie/kiosk1/k1_v02.anm", s_kiosk1Shot2 },
    { "movie/kiosk2/k2_v01.anm", s_kiosk2, 0, 1 },
    { "movie/kiosk2/k2_v02.anm", s_kiosk2Shot2 },
    { "movie/kiosk3/k3_v01.anm", s_kiosk3, 0, 2 },
    { "movie/kiosk3/k3_v02.anm", s_kiosk3Shot2 },
    { "", FRAMES_STOP, -1, 3 },
};

JitterCue s_kioskRoadJitter[] = {
    { 0, 0, 20, 1600, JITTER_FALLING },
    { 0, 480, 120, 40, JITTER_CONSTANT },
    { 0, 600, 30, 60, JITTER_FALLING },
    { 0, 630, 30, 60, JITTER_FALLING },
    { 0, s_kiosk2 - 21, 20, 1600, JITTER_RISING },
    { 1, s_kiosk2, 20, 1600, JITTER_FALLING },
    { 1, s_kiosk3 - 21, 20, 600, JITTER_RISING },
    { 2, s_kiosk3, 20, 1600, JITTER_FALLING },
    { 2, s_kioskEnd - 21, 40, 1000, JITTER_RISING },
    { 0, -1, 0, 0, 0 },
};

const char* s_kioskRoadModels[] = {
    "movie/kiosk1/k1_rd.tmd",
    "movie/kiosk2/k2_rd.tmd",
    "movie/kiosk3/k3_rd.tmd",
    "",
};

MovieCue s_kioskRoadMovies[] = {
    { "movie/kiosk1/k1_r01.anm", 0, 0, 0, 0 },
    { "movie/kiosk1/k1_r02.anm", s_kiosk1Shot2 },
    { "movie/kiosk2/k2_r01.anm", s_kiosk2, 1, 1 },
    { "movie/kiosk2/k2_r02.anm", s_kiosk2Shot2, 1 },
    { "movie/kiosk3/k3_r01.anm", s_kiosk3, 2, 2 },
    { "movie/kiosk3/k3_r02.anm", s_kiosk3Shot2, 2 },
    { "", FRAMES_STOP, -1, 3 },
};

const char* s_kioskPtModels[] = {
    "movie/kiosk1/k1_pt.tmd",
    "movie/kiosk2/k2_pt.tmd",
    "movie/kiosk3/k3_pt.tmd",
    "",
};

JitterCue s_kioskPtJitter[] = {
    { 0, s_kiosk2 - 21, 20, 1600, JITTER_RISING },
    { 1, s_kiosk2, 20, 1600, JITTER_FALLING },
    { 1, s_kiosk3 - 21, 20, 600, JITTER_RISING },
    { 2, s_kiosk3, 20, 1600, JITTER_FALLING },
    { 2, 3030, 20, 1000, JITTER_RISING },
    { 0, -1, 0, 0, 0 },
};

MovieCue s_kioskPtMovies[] = {
    { "movie/kiosk1/k1_p01.anm", 0, 0, 0, 0 },
    { "movie/kiosk1/k1_p02.anm", s_kiosk1Shot2 },
    { "movie/kiosk2/k2_p01.anm", s_kiosk2, 1, 1 },
    { "movie/kiosk2/k2_p02.anm", s_kiosk2Shot2, 1 },
    { "movie/kiosk3/k3_p01.anm", s_kiosk3, 2, 2 },
    { "movie/kiosk3/k3_p02.anm", s_kiosk3Shot2, 2 },
    { "", FRAMES_STOP, -1, 3 },
};

JitterCue s_kioskFrogJitter[] = {
    { 0, 600, 30, 60, JITTER_FALLING },
    { 0, -1, 0, 0, 0 },
};

const char* s_kioskFrogModels[] = {
    "movie/kiosk1/frog.tmd",
    "",
};

MovieCue s_kioskFrogMovies[] = {
    { "movie/kiosk1/k1_f02.anm", s_kiosk1Shot2 },
    { "", FRAMES_STOP, -1 },
};

JitterCue s_kioskSnakeJitter[] = {
    { 0, 630, 30, 60, JITTER_FALLING },
    { 0, -1, 0, 0, 0 },
};

const char* s_kioskSnakeModels[] = {
    "movie/kiosk1/snake.tmd",
    "",
};

MovieCue s_kioskSnakeMovies[] = {
    { "movie/kiosk1/k1_s02.anm", s_kiosk1Shot2 },
    { "", FRAMES_STOP, -1 },
};

SoundCue s_kioskSounds[] = {
    { 0x100, 0 },
    { -1, FRAMES_STOP },
};

#ifndef KIOSK
TitleScene::TitleScene() : m_skippable(1), m_time(0), m_unused(0)
{
    Add(new CameraElement(s_titleCameras));
    Add(new ModelElement(s_titleMovies, s_titleModels, s_titleJitter));
    Add(new ModelElement(s_vibriMovies, s_vibriModels, s_titleJitter));
    m_logo = new LogoElement;
    Add(m_logo);
    Add(new SoundElement(s_titleSounds));
}
#endif

void TitleScene::Seek(int label)
{
    MovieScene::Seek(label);
    m_time = ToGameFrames(TITLE_TIME);
}

#ifndef KIOSK
int TitleScene::IntroOver()
{
    return m_time > ToGameFrames(TITLE_TIME);
}

void TitleScene::Leave()
{
    m_leaving = 1;
}
#endif

void TitleScene::Update()
{
    if (m_time >= ToGameFrames(TITLE_TIME)) {
        m_skippable = 0;
    }
    if (m_skippable && ReadConfirm()) {
        Seek(1);
    }
    MovieScene::Update();
    if (m_time < 3000) {
        m_time++;
    }
}

#ifdef KIOSK
KioskScene::KioskScene() : m_unused(0)
{
    Add(new CameraElement(s_kioskCameras));
    Add(new ModelElement(s_kioskVibriMovies, s_kioskVibriModels, s_kioskVibriJitter));
    Add(new ModelElement(s_kioskRoadMovies, s_kioskRoadModels, s_kioskRoadJitter));
    Add(new ModelElement(s_kioskPtMovies, s_kioskPtModels, s_kioskPtJitter));
    Add(new ModelElement(s_kioskFrogMovies, s_kioskFrogModels, s_kioskFrogJitter));
    Add(new ModelElement(s_kioskSnakeMovies, s_kioskSnakeModels, s_kioskSnakeJitter));
    Add(new SoundElement(s_kioskSounds));
}
#endif

int KioskScene::IsFinished()
{
    if (!m_leaving && ReadConfirm()) {
        m_leaving = 1;
        Sound confirm(0, 0);
        AudioSys::PlaySound(&confirm, 1, 64, -1);
    }
    if (m_leaving && !AudioSys::IsPlaying(1)) {
        return 1;
    }
    return MovieScene::IsFinished();
}
