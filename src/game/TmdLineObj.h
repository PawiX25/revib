#ifndef TMDLINEOBJ_H
#define TMDLINEOBJ_H

#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#include "Fixed.h"
#include "MemPtr.h"

/* An object of a TMD file, with its pointers made absolute. */
struct TmdObject {
    SVECTOR* vertices;
    u_long numVertices;
    SVECTOR* normals;
    u_long numNormals;
    u_long* primitives;
    u_long numPrimitives;
    long scale;
};

/*
 * A TMD object drawn as lines (the game's look): its lines, and of each flat
 * quadrangle the line from its first vertex to its last, with every vertex
 * jittered a little each time it is drawn. A flat triangle with no red or
 * green and a blue of 0 or 1 is a marker, not drawn: with blue 0, the object
 * is drawn only while that triangle faces the camera.
 */
class TmdLineObj {
public:
    struct Line {
        short v0, v1;
        CVECTOR color;
    };

    TmdLineObj(TmdObject* object, int copyVertices);
    ~TmdLineObj();

    /*
     * `zOffset` moves the object in depth. With `join`, a line from the
     * point it gives to the object's first vertex is drawn too, and the
     * object's last vertex, as drawn, is handed back in it for the next
     * object.
     */
    void Draw(int zOffset, SVECTOR* join);

    void SetJitter(Fixed jitter) { m_jitter = jitter; }
    /* times the object is drawn, each time jittered anew */
    void SetPasses(int passes) { m_passes = passes; }
    void SetJitter(Fixed jitter, int passes)
    {
        m_jitter = jitter;
        m_passes = passes;
    }
    /* the lines are drawn in `color` rather than in their own colours */
    void SetColor(const CVECTOR& color)
    {
        m_color = color;
        m_useColor = 1;
    }
    void ClearColor() { m_useColor = 0; }

private:
    /* the slot for the next line read */
    Line* NextLine() { return &m_lines[m_numLines]; }

    int IsFacingAway();
    void DrawPass(GsOT_TAG* tags, int shift, int zOffset, SVECTOR* join);

    int m_ownsVertices;
    int m_numVertices;
    int m_numLines;
    Line* m_lines;
    int m_useColor;
    CVECTOR m_color;
    int m_cullBack;        /* not drawn while the marker triangle faces away */
    int m_cullFront;       /* set by a marker of blue 1, never read */
    int m_faceVertices[3]; /* the marker triangle's */
    SVECTOR* m_vertices;
    /* work areas of a Draw() */
    SVECTOR* m_work;
    DVECTOR* m_screen;
    u_short* m_depth;
    u_short* m_interpolation;
    u_short* m_flags;
    int m_passes;
    Fixed m_jitter; /* how far vertices move */
};

/* A TMD model: one TmdLineObj per object. */
class TmdModel {
public:
    TmdModel(const MemPtr<u_long>& file);
    ~TmdModel()
    {
        for (int i = 0; i < m_numObjects; i++) {
            delete m_objects[i];
        }
        delete[] m_objects;
    }

    /* the jitter and passes of every object */
    void SetJitter(Fixed jitter, int passes)
    {
        for (int i = 0; i < m_numObjects; i++) {
            TmdLineObj* object = m_objects[i];
            object->SetJitter(jitter);
            object->SetPasses(passes);
        }
    }

    MemPtr<u_long> m_file;
    int m_numObjects;
    TmdLineObj** m_objects;
};

TmdModel* LoadTmdModel(const char* name);
TmdLineObj* LoadTmdLineObj(const char* name);

#endif
