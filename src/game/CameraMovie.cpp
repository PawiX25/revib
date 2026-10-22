#include "common.h"

#include "ModelMovie.h"
#include "VideoSys.h"

static inline MovieFrames ReadFrames(short* data)
{
    MovieFrames frames;
    if (data[0] == MOVIE_HEADER) {
        frames.rate = data[1];
        frames.count = data[2];
        frames.frames = data + 3;
    } else {
        frames.rate = 60;
        frames.count = data[0];
        frames.frames = data + 1;
    }
    return frames;
}

CameraMovie::CameraMovie(const MemPtr<short>& file, int loop)
    : Movie(ReadFrames(file.Data()).count, VideoSys::FramePeriod() * ReadFrames(file.Data()).rate, loop),
      m_file(file)
{
    m_frames = new MovieFrames(ReadFrames(file.Data()));
    m_parent = &Coord::s_world;
}

CameraMovie::~CameraMovie()
{
    delete m_frames;
}

/* (in unk_22EE8.cpp) */
void SetProjection(Fixed fov);

/* a frame of a camera movie */
struct CameraFrame {
    short x, y, z;          /* where the camera is */
    short targetX, targetY, targetZ;
    short roll;
    short fov;
};

void CameraMovie::Update(int mirror)
{
    static GsRVIEW2 view;
    CameraFrame* frame = (CameraFrame*)m_frames->frames + m_position.Integer();
    view.vpx = mirror ? -frame->x : frame->x;
    view.vpy = frame->y;
    view.vpz = frame->z;
    view.vrx = mirror ? -frame->targetX : frame->targetX;
    view.vry = frame->targetY;
    view.vrz = frame->targetZ;
    view.rz = frame->roll;
    view.super = Coord::LibgsParent(m_parent);
    SetProjection(Fixed::FromRaw(360 * frame->fov));
    GsSetRefView2(&view);
    Advance();
}
