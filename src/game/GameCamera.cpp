#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "MemorySys.h"

#include <string>

#include "Format.h"
#include "GameCamera.h"
#include "ModelMovie.h"

/* tracing of the camera's sequences, off */
#define TRACE 0

/* loops of a camera whose event gives none */
const int CGameCamera::DEFAULT_LOOPS = 2;
const float CGameCamera::SPEED = 1.0;
const float CGameCamera::FAST_SPEED = 1.3;

enum Group { BACK, SIDE, FRONT };

/* A camera: its movie is played a number of times in a row. */
struct Camera {
    int id; /* in camera events */
    const char* name;
    int group;
    CameraMovie* movie;
};

/* A move from a camera of one group to one of another, the other way round played backwards. */
struct Transition {
    const char* sameSideName;
    const char* switchSideName;
    int from;
    int to;
    CameraMovie* sameSide;   /* staying on the same side */
    CameraMovie* switchSide; /* going over to the other side */
};

/* the movies' files: road/<name>.anc */
static string s_directory("road/");
static string s_extension(".anc");
string g_cameraPath(32, ' ');

static Camera s_cameras[] = {
    { 1, "b01", BACK, NULL },
    { 2, "b02", BACK, NULL },
    { 3, "s01", SIDE, NULL },
    { 4, "s02", SIDE, NULL },
    { 5, "f01", FRONT, NULL },
    { 6, "f02", FRONT, NULL },
    { 7, "f03", FRONT, NULL },
};
const int CGameCamera::NUM_CAMERAS = sizeof(s_cameras) / sizeof(s_cameras[0]);

static Transition s_transitions[] = {
    { "tv_bs", "ta_bs", BACK, SIDE, NULL, NULL },
    { "tv_bf", "ta_bf", BACK, FRONT, NULL, NULL },
    { "tv_sf", "ta_sf", SIDE, FRONT, NULL, NULL },
    { "tv_bb", "ta_bb", BACK, BACK, NULL, NULL },
    { "tv_ss", "ta_ss", SIDE, SIDE, NULL, NULL },
    { "tv_ff", "ta_ff", FRONT, FRONT, NULL, NULL },
};
const int CGameCamera::NUM_TRANSITIONS = sizeof(s_transitions) / sizeof(s_transitions[0]);

void CGameCamera::SetNext(int camera, int loops)
{
    m_eventCamera = camera;
    m_eventLoops = loops;
}

void CGameCamera::TakeNext(int side)
{
    m_nextSide = side;
    m_nextCamera = m_eventCamera;
    m_nextLoops = m_eventLoops;
}

void CGameCamera::StartBase()
{
    m_camera = m_nextCamera;
    m_side = m_nextSide;
    m_loopsDone = 0;
    m_numLoops = m_nextLoops;
    m_movie = s_cameras[m_camera].movie;
    m_inTransition = 0;
    m_flip = m_side;
    if (TRACE) {
        printf("base=%d, flip=%d\n", m_camera, m_flip);
    }
    m_movie->Rewind();
    /* a camera's loops last as long whatever their number */
    Fixed speed = Fixed(m_fast ? FAST_SPEED : SPEED) * m_nextLoops / 2;
    m_movie->SetSpeed(speed);
}

void CGameCamera::StartTransition()
{
    int from = s_cameras[m_camera].group;
    int to = s_cameras[m_nextCamera].group;
    int transition = -1;
    int reverse = 0;
    for (int i = 0; i < NUM_TRANSITIONS; i++) {
        if (s_transitions[i].from == from && s_transitions[i].to == to) {
            transition = i;
        }
    }
    if (transition == -1) {
        reverse = 1;
        for (int i = 0; i < NUM_TRANSITIONS; i++) {
            if (s_transitions[i].from == to && s_transitions[i].to == from) {
                transition = i;
            }
        }
        if (transition == -1) {
            printf("transition not found\n");
        }
#line 117 "C:/psj/dev/locale/game/GameCamera.cpp"
        assert(transition != -1);
#line 124 "src/game/GameCamera.cpp"
    }
    int switchSide = m_side != m_nextSide;
    m_movie = switchSide ? s_transitions[transition].switchSide : s_transitions[transition].sameSide;
    m_inTransition = 1;
    m_movie->Rewind(reverse);
    m_movie->SetSpeed(Fixed::Int(reverse ? -1 : 1));
    m_flip = m_side != (reverse && switchSide);
    if (TRACE) {
        printf("transition=%d, reverse=%d, switchSide=%d, flip=%d\n", transition, reverse, switchSide, m_flip);
    }
}

CGameCamera::CGameCamera()
{
    for (int i = 0; i < NUM_CAMERAS; i++) {
        g_cameraPath = s_directory;
        g_cameraPath += s_cameras[i].name;
        g_cameraPath += s_extension;
        s_cameras[i].movie = new CameraMovie(MemPtr<short>(g_cameraPath.c_str()), 0);
    }
    for (int i = 0; i < NUM_TRANSITIONS; i++) {
        g_cameraPath = s_directory;
        g_cameraPath += s_transitions[i].sameSideName;
        g_cameraPath += s_extension;
        s_transitions[i].sameSide = new CameraMovie(MemPtr<short>(g_cameraPath.c_str()), 0);
        g_cameraPath = s_directory;
        g_cameraPath += s_transitions[i].switchSideName;
        g_cameraPath += s_extension;
        s_transitions[i].switchSide = new CameraMovie(MemPtr<short>(g_cameraPath.c_str()), 0);
    }
    Reset(0, 0);
}

CGameCamera::~CGameCamera()
{
    for (int i = 0; i < NUM_CAMERAS; i++) {
        delete s_cameras[i].movie;
    }
    for (int i = 0; i < NUM_TRANSITIONS; i++) {
        delete s_transitions[i].sameSide;
        delete s_transitions[i].switchSide;
    }
}

void CGameCamera::Reset(int fast, int hold)
{
    m_fast = fast;
    m_hold = hold;
    m_waiting = 1;
    /* the first side camera, once */
    SetNext(2, 1);
    TakeNext(1);
    StartBase();
    Break();
}

void CGameCamera::Event(ScriptEvent* event)
{
    int value = event->value;
    int loops = value / 10;
    int id = value % 10;
    if (loops <= 0) {
        printf("camera event has bad loop count\n");
        loops = DEFAULT_LOOPS;
    }
    int camera = -1;
    for (int i = 0; i < NUM_CAMERAS; i++) {
        if (s_cameras[i].id == id) {
            camera = i;
        }
    }
    if (camera == -1) {
        printf("camera %d not found\n", value);
        camera = 0;
    }
    if (TRACE) {
        printf("camera %d event '%s'\n", value, ""); /* a script event has no name */
    }
    SetNext(camera, loops);
    if (m_waiting) {
        if (TRACE) {
            printf("\tfirst event\n");
        }
        m_waiting = 0;
        TakeNext(0);
        StartBase();
        TakeNext(0);
        StartTransition();
    }
}

void CGameCamera::Break()
{
    if (TRACE) {
        printf("break event\n");
    }
    TakeNext(m_fast ? 0 : m_side ^ 1);
    StartTransition();
}

void CGameCamera::Update(Coord* parent)
{
    if (m_waiting) {
        printf("warning: CGameCamera::Update() before first camera event\n");
    }
    m_movie->SetParent(parent);
    m_movie->Update(m_flip);
    if (m_movie->m_finished) {
        switch (m_inTransition) {
        case 0:
            if (m_hold || ++m_loopsDone < m_numLoops) {
                if (TRACE) {
                    printf("(base seq loop %d)\n", m_loopsDone);
                }
                m_movie->Rewind();
            }
            break;
        case 1:
            if (TRACE) {
                printf("trans seq ended\n");
            }
            StartBase();
            break;
        }
    }
}

int CGameCamera::Side()
{
    return m_inTransition == 0 ? m_side : m_nextSide;
}
