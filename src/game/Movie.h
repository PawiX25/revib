#ifndef MOVIE_H
#define MOVIE_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Something played in a scene (models, the camera, sounds...), cue after
 * cue. Times are frames of the game since the scene started; a cue may
 * carry a label for MovieScene::Seek().
 */
class MovieElement {
public:
    MovieElement() : m_frame(0), m_cue(0) {}
    virtual ~MovieElement() {}

    /* `leaving`: the scene is being left */
    virtual void Update(int leaving) {}
    virtual int IsFinished() { return 1; }
    virtual void Reset() {}
    virtual void Seek(int label) {}

protected:
    int m_frame;
    int m_cue;
};

/* A scene: elements played together. */
class MovieScene {
public:
    MovieScene() : m_count(0), m_leaving(0) {}

    virtual void Add(MovieElement* element)
    {
#line 37 "C:/psj/dev/locale/game/Movie.h"
        assert(m_count < MAX_ELEMENTS);
#line 41 "src/game/Movie.h"
        m_elements[m_count++] = element;
    }
    virtual void Update()
    {
        for (int i = 0; i < m_count; i++) {
            m_elements[i]->Update(m_leaving);
        }
    }
    virtual int IsFinished()
    {
        int finished = 1;
        for (int i = 0; i < m_count; i++) {
            finished = finished && m_elements[i]->IsFinished();
        }
        return finished;
    }
    virtual void Reset()
    {
        for (int i = 0; i < m_count; i++) {
            m_elements[i]->Reset();
        }
    }
    /* goes to the cues with this label */
    virtual void Seek(int label)
    {
        Reset();
        for (int i = 0; i < m_count; i++) {
            m_elements[i]->Seek(label);
        }
    }
    virtual ~MovieScene()
    {
        for (int i = 0; i < m_count; i++) {
            delete m_elements[i];
        }
    }

protected:
    enum { MAX_ELEMENTS = 10 };

    int m_count;
    MovieElement* m_elements[MAX_ELEMENTS];
    int m_leaving;
};

class LogoElement;

/*
 * The title: the camera flying through the lines of the intro, then the
 * title and the logo. The intro can be skipped with the confirm or the
 * cancel button.
 */
class TitleScene : public MovieScene {
public:
    TitleScene();
    ~TitleScene() {}

    void Seek(int label);
    /* the intro has been played */
    int IntroOver();
    /* the elements go away */
    void Leave();
    void Update();

private:
    int m_skippable;
    int m_time;       /* frames since the start, up to 3000 */
    LogoElement* m_logo;
    int m_unused;
};

/*
 * The demo of the kiosk version, over when all its elements are. The
 * confirm or the cancel button ends it sooner: the confirm sound plays, the
 * lines scatter, and it is over once the sound has ended.
 */
class KioskScene : public MovieScene {
public:
    KioskScene();
    ~KioskScene() {}

    int IsFinished();

private:
    int m_unused;
};

#endif
