#ifndef MESSAGE_H
#define MESSAGE_H

#include "common.h"

#include "ModelMovie.h"
#include "TmdLineObj.h"

/* A model moved by a movie, seen through a camera movie. */
struct AnimatedModel {
    TmdModel* m_model;
    ModelMovie m_movie;
    CameraMovie m_camera;

    /* `jitter`: of the model's lines */
    AnimatedModel(const char* model, const char* movie, const char* camera, int loop, double jitter)
        : m_model(LoadTmdModel(model)),
          m_movie(m_model, MemPtr<short>(movie), 0),
          m_camera(MemPtr<short>(camera), 0)
    {
        m_model->SetJitter(Fixed(jitter), 1);
        m_movie.SetLoop(loop);
    }
    ~AnimatedModel() { delete m_model; }
};

/*
 * The animated "now loading" picture, kept moving while files are read.
 * There is one at a time.
 */
class Message : AnimatedModel {
public:
    Message();
    ~Message();

private:
    /* draws a frame; FileSys calls it while it waits for the CD */
    static void Show();

    static Message* s_instance;
};

#endif
