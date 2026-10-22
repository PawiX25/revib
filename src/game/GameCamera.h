#ifndef GAMECAMERA_H
#define GAMECAMERA_H

#include "common.h"

class CameraMovie;
class Coord;

#include "ScriptEvent.h"

/*
 * The camera during a course: one of the cameras' movies played a number of
 * times, until Break() starts a transition movie to the next one, which the
 * track's camera events choose (loops * 10 + camera id). A transition goes
 * from one group of cameras (back, side or front) to another, backwards
 * when only the opposite one exists, flipping left and right as it says.
 */
class CGameCamera {
public:
    CGameCamera();
    ~CGameCamera();

    /* `fast`: the movies run faster; `hold`: a camera's movie loops on past its number of times */
    void Reset(int fast, int hold);
    void Event(ScriptEvent* event);
    /* goes on to the next camera now */
    void Break();
    /* shows the camera, following `parent` */
    void Update(Coord* parent);
    /* the side the camera is on (0 or 1) */
    int Side();

private:
    static const int DEFAULT_LOOPS;
    static const float SPEED;
    static const float FAST_SPEED;
    static const int NUM_CAMERAS;
    static const int NUM_TRANSITIONS;

    void SetNext(int camera, int loops);
    void TakeNext(int side);
    void StartBase();
    void StartTransition();

    int m_fast;
    int m_hold;
    int m_waiting;          /* for the first camera event */
    CameraMovie* m_movie;
    int m_inTransition;
    int m_flip;
    int m_camera;
    int m_loopsDone;
    int m_numLoops;
    int m_side;
    int m_nextCamera;
    int m_nextLoops;
    int m_nextSide;
    int m_eventCamera;
    int m_eventLoops;
};

#endif
