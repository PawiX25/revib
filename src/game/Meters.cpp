#include "common.h"

#include <stdlib.h>

#include "MemorySys.h"
#include <string>

#include "Format.h"
#include "Fixed.h"
#include "Meters.h"
#include "TmdLineObj.h"

SpeedMeter::SpeedMeter(Fixed max, Fixed min)
    : m_min(min), m_max(max), m_camera(MemPtr<short>("meters/speed_01.anc"), 0)
{
    m_model = LoadTmdModel("meters/speed.tmd");
    m_dial = new ModelMovie(m_model, MemPtr<short>("meters/speed.anm"), 0);
    m_target = new ModelMovie(m_model, MemPtr<short>("meters/spd_trgt.anm"), 0);
    m_needle = new ModelMovie(m_model, MemPtr<short>("meters/spd_nedl.anm"), 0);
    m_dial->SetSpeed(0.0);
    m_target->SetSpeed(0.0);
    m_needle->SetSpeed(0.0);
    m_model->SetJitter(3.0, 1);
    Fixed range = m_max - m_min;
    m_targetScale = Fixed::Int(m_target->m_numFrames) / range;
    m_needleScale = Fixed::Int(m_needle->m_numFrames) / range;
}

SpeedMeter::~SpeedMeter()
{
    delete m_target;
    delete m_needle;
    delete m_dial;
    delete m_model;
}

void SpeedMeter::SetFrame(ModelMovie* movie, Fixed frame)
{
    Fixed end = Fixed::Int(movie->m_numFrames);
    if (frame < Fixed()) {
        frame = Fixed();
    }
    if (frame >= end) {
        frame = end - Fixed::Int(1);
    }
    movie->Seek(frame);
}

void SpeedMeter::SetTarget(Fixed speed)
{
    SetFrame(m_target, m_targetScale * (speed - m_min));
}

void SpeedMeter::Draw(Fixed speed)
{
    m_camera.Update(0);
    m_dial->Draw();
    m_target->Draw();
    SetFrame(m_needle, m_needleScale * (speed - m_min));
    m_needle->Draw();
}

TimeMeter::TimeMeter() : m_camera(MemPtr<short>("meters/time.anc"), 0)
{
    m_model = LoadTmdModel("meters/time.tmd");
    m_movie = new ModelMovie(m_model, MemPtr<short>("meters/time.anm"), 0);
    m_movie->SetSpeed(0.0);
    m_model->SetJitter(3.0, 1);
}

TimeMeter::~TimeMeter()
{
    delete m_movie;
    delete m_model;
}

void TimeMeter::Draw(Fixed time)
{
    m_camera.Update(0);
    /* the time gone */
    time = Fixed(1.0) - time;
    if (time < 0.0) {
        time = 0.0;
    } else if (Fixed(1.0) < time) {
        time = 1.0;
    }
    m_movie->Seek(time * (m_movie->m_numFrames - 1));
    m_movie->Draw();
}

/* the allocator's "out of memory" string, of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Meters", D_8001DD08);
