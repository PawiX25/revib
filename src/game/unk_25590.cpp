#include "common.h"

#include "ModelMovie.h"
#include "VideoSys.h"

Movie::Movie(int numFrames, Fixed rate, int loop) : m_numFrames(numFrames), m_rate(rate)
{
    m_speed = 1.0;
    m_loop = loop;
    m_finished = 0;
    m_step = m_rate * m_speed;
    SetPosition(0.5);
}

void Movie::Advance()
{
    if (m_loop) {
        m_position = m_position + m_step;
        if (m_position < 0.0) {
            m_position = m_position + Fixed::Int(m_numFrames);
        }
        if (m_position >= Fixed::Int(m_numFrames)) {
            m_position = m_position - Fixed::Int(m_numFrames);
        }
    } else {
        Fixed position = m_position + m_step;
        if (position < 0.0) {
            position = 0.0;
        } else if (position >= Fixed::Int(m_numFrames)) {
            position = Fixed::Int(m_numFrames).Previous();
        }
        if (position == m_position) {
            m_finished = 1;
            return;
        }
        m_position = position;
    }
    m_finished = 0;
}

void Movie::SetPosition(Fixed position)
{
    m_position = position;
}

static inline MovieTrack ReadTrack(short* data)
{
    MovieTrack track;
    track.data = data;
    if (data[0] == MOVIE_HEADER) {
        track.rate = data[1];
        track.count = data[2];
        track.frames = data + 3;
    } else {
        track.rate = 60;
        track.count = data[0];
        track.frames = data + 1;
    }
    return track;
}

ModelMovie::ModelMovie(TmdModel* model, const MemPtr<short>& file, int loop)
    : Movie(ReadTrack(file.Data()).count, VideoSys::FramePeriod() * ReadTrack(file.Data()).rate, loop),
      m_model(model), m_file(file)
{
    m_track = new MovieTrack(ReadTrack(file.Data()));
    m_parent = &Coord::s_world;
}

ModelMovie::~ModelMovie()
{
    delete m_track;
}

/* what an object's movements in a frame are made of */
#define FRAME_ROTATE 1 /* 3 angles */
#define FRAME_SCALE 2  /* 3 scales */
#define FRAME_MOVE 4   /* 3 coordinates */

/* Draws the objects moved in the current frame, and goes on; returns how many there were. */
int ModelMovie::Draw()
{
    int frame = m_position.Integer();
    short* object = FrameData(frame);
    short* end = FrameData(frame + 1);
    int count = 0;
    while (object != end) {
        int flags = ((char*)object)[1];
        Coord coord;
        coord.SetParent(m_parent);
        short* values = object + 1;
        if (flags & FRAME_ROTATE) {
            coord.Rotate((SVECTOR*)values);
            values += 3;
        }
        if (flags & FRAME_SCALE) {
            coord.Scale(values);
            values += 3;
        }
        if (flags & FRAME_MOVE) {
            coord.SetTranslation(values[0], values[1], values[2]);
            values += 3;
        }
        MATRIX ls;
        GsGetLs(Coord::LibgsParent(&coord), &ls);
        GsSetLsMatrix(&ls);
        m_model->m_objects[*(u_char*)object]->Draw(0, NULL);
        object = values;
        count++;
    }
    Advance();
    return count;
}
