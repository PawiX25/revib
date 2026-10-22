#ifndef MODELMOVIE_H
#define MODELMOVIE_H

#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#include "Coord.h"
#include "Fixed.h"
#include "MemPtr.h"
#include "TmdLineObj.h"

/* Animations played frame by frame: the movements of a model or of the camera. */
class Movie {
public:
    /* `rate`: frames per frame of the game */
    Movie(int numFrames, Fixed rate, int loop);

    /* goes on by a step; one that does not loop stops at its last frame */
    void Advance();
    void SetPosition(Fixed position);
    void SetLoop(int loop) { m_loop = loop; }
    void SetSpeed(Fixed speed)
    {
        m_speed = speed;
        m_step = m_speed * m_rate;
    }
    /* back to the first frame, or to the last one to play backwards */
    void Rewind(int backwards = 0)
    {
        m_finished = 0;
        Seek(backwards ? Fixed::Int(m_numFrames) - 0.5 : Fixed(0.5));
    }
    /* Defined after Rewind(), so Rewind() calls it instead of inlining it. */
    void Seek(Fixed position) { m_position = position; }

    int m_numFrames;
    Fixed m_step;     /* m_rate * m_speed */
    Fixed m_rate;
    Fixed m_speed;
    int m_finished;   /* the end was reached (or the start, going backwards) */
    int m_loop;
    Fixed m_position; /* in frames */
};

/* a movie file starts with this when it gives its rate; others are at 60 frames per second */
#define MOVIE_HEADER -0x8000

/* Where a movie's frames are, in its file. */
struct MovieTrack {
    short rate;   /* frames per second */
    short count;  /* frames */
    short* frames;
    short* data;  /* the start of the file */
};

/* Where a movie's frames are, in its file, without the file. */
struct MovieFrames {
    short rate;   /* frames per second */
    short count;  /* frames */
    short* frames;
};

/* The movements of the objects of a TMD model (an .anm file). */
class ModelMovie : public Movie {
public:
    ModelMovie(TmdModel* model, const MemPtr<short>& file, int loop);
    ~ModelMovie();

    int Draw();
    /* what the model follows */
    void SetParent(Coord* parent) { m_parent = parent; }

private:
    /* where the objects' movements in a frame are */
    short* FrameData(int frame) { return m_track->data + (u_short)m_track->frames[frame]; }

    TmdModel* m_model;
    MemPtr<short> m_file;
    Coord* m_parent;
    MovieTrack* m_track;
};

/* The movements of the camera (an .anc file). */
class CameraMovie : public Movie {
public:
    CameraMovie(const MemPtr<short>& file, int loop);
    ~CameraMovie();

    /* shows the current frame (mirrored left to right with `mirror`) and goes on */
    void Update(int mirror);
    /* what the camera follows */
    void SetParent(Coord* parent) { m_parent = parent; }

private:
    MemPtr<short> m_file;
    Coord* m_parent;
    MovieFrames* m_frames;
};

/* special times for the cue that ends a list (ToGameFrames() passes them on unchanged) */
#define FRAMES_LOOP -97 /* the element never finishes; a model element's last movie loops */
#define FRAMES_STOP -98 /* the element finishes with its last cue; a model element then stops drawing */

/* A time in 30ths of a second as frames of the game, less the ones already drawn ahead. */
int ToGameFrames(int time);

#endif
