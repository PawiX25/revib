#ifndef METERS_H
#define METERS_H

#include "common.h"

#include "Fixed.h"
#include "ModelMovie.h"

/*
 * The speed meter: a dial (meters/speed) whose needle shows the speed and
 * whose mark shows the speed to reach, seen through its own camera.
 */
class SpeedMeter {
public:
    SpeedMeter(Fixed max, Fixed min);
    ~SpeedMeter();

    void SetTarget(Fixed speed);
    void Draw(Fixed speed);

private:
    /* shows `frame` of one of the dial's movies, kept within it */
    void SetFrame(ModelMovie* movie, Fixed frame);

    Fixed m_min;
    Fixed m_max;
    TmdModel* m_model;
    ModelMovie* m_dial;
    ModelMovie* m_needle;
    ModelMovie* m_target;
    CameraMovie m_camera;
    Fixed m_targetScale; /* frames of the movie per unit of speed */
    Fixed m_needleScale;
};

/* The time meter (meters/time), emptying as the music's time runs out. */
class TimeMeter {
public:
    TimeMeter();
    ~TimeMeter();

    /* `time`: the time left, from 1 (all) to 0 (none) */
    void Draw(Fixed time);

private:
    TmdModel* m_model;
    ModelMovie* m_movie;
    CameraMovie m_camera;
};

#endif
