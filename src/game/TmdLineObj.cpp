#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Random.h"
#include "TmdLineObj.h"
#include "VideoSys.h"

/* (in unk_22EE8.cpp) */
int ClipLine(SVECTOR* v0, SVECTOR* v1);

/* shared by all objects, so they never shake the same way */
Random g_jitterRandom;

#ifndef GAME /* the game's version does not have its format strings */
#include "Format.h"
#endif

/* the primitive modes of a TMD file that TmdLineObj understands */
enum {
    TMD_F3_NOLIGHT = 0x21, /* flat triangle, no lighting */
    TMD_F4_NOLIGHT = 0x29, /* flat quadrangle, no lighting */
    TMD_LINE_F2 = 0x40     /* flat line */
};

/* a primitive of a TMD file */
struct TmdPrimitive {
    u_char olen; /* words of the GPU packet */
    u_char ilen; /* words of the data after this header */
    u_char flag;
    u_char mode;
};

/* the start of a TMD file */
struct TmdHeader {
    u_long id;
    u_long flags;
    u_long numObjects;
    TmdObject objects[1]; /* numObjects of them */
};

/* the data of the primitives TmdLineObj reads, after their TmdPrimitive */
struct TmdLineF2 {
    CVECTOR color;
    short v0, v1;
};

struct TmdPolyF3 {
    CVECTOR color;
    short v0, v1, v2, pad;
};

struct TmdPolyF4 {
    CVECTOR color;
    short v0, v1, v2, v3;
};

/* the blue of a flat triangle with no red or green */
enum Marker {
    MARKER_CULL_BACK,  /* a marker: the object is drawn only while the triangle faces the camera */
    MARKER_CULL_FRONT, /* a marker setting m_cullFront, which nothing reads */
    MARKER_NONE        /* not a marker: like any other triangle */
};

TmdLineObj::TmdLineObj(TmdObject* object, int copyVertices)
    : m_ownsVertices(copyVertices), m_useColor(0), m_cullBack(0), m_cullFront(0)
{
    m_numVertices = object->numVertices;
    if (copyVertices) {
        m_vertices = new SVECTOR[m_numVertices];
        for (int i = 0; i < m_numVertices; i++) {
            m_vertices[i] = object->vertices[i];
        }
    } else {
        m_vertices = object->vertices;
    }

    m_lines = new Line[object->numPrimitives + 1];
    m_numLines = 0;
    u_long* prim = object->primitives;
    for (u_long i = 0; i < object->numPrimitives; i++) {
        int isFace = 0;
        TmdPrimitive* header = (TmdPrimitive*)prim;
        Line* line = NextLine();
        switch (header->mode) {
        case TMD_LINE_F2: {
            TmdLineF2* lineF2 = (TmdLineF2*)(header + 1);
            line->v0 = lineF2->v0;
            line->v1 = lineF2->v1;
            line->color = lineF2->color;
            break;
        }
        case TMD_F3_NOLIGHT: {
            /* any triangle but a marker counts as a line, its slot left unset */
            TmdPolyF3* poly = (TmdPolyF3*)(header + 1);
            if (poly->color.r == 0 && poly->color.g == 0) {
                switch ((Marker)poly->color.b) {
                case MARKER_CULL_BACK:
                    m_cullBack = 1;
                    m_faceVertices[0] = poly->v0;
                    m_faceVertices[1] = poly->v1;
                    m_faceVertices[2] = poly->v2;
                    isFace = 1;
                    break;
                case MARKER_CULL_FRONT:
                    m_cullFront = 1;
                    m_faceVertices[0] = poly->v0;
                    m_faceVertices[1] = poly->v1;
                    m_faceVertices[2] = poly->v2;
                    isFace = 1;
                    break;
                case MARKER_NONE:
                    break;
                }
            }
            break;
        }
        case TMD_F4_NOLIGHT: {
            TmdPolyF4* poly = (TmdPolyF4*)(header + 1);
            line->v0 = poly->v0;
            line->v1 = poly->v3;
            line->color = poly->color;
            break;
        }
        default:
            printf("code = 0x%X\n", header->mode);
#line 153 "C:/psj/dev/locale/game/TmdLineObj.cpp"
            assert(0);
#line 135 "src/game/TmdLineObj.cpp"
        }
        if (!isFace) {
            m_numLines++;
        }
        prim = prim + header->ilen + 1;
    }
    m_jitter = Fixed();
    m_passes = 1;
}

TmdLineObj::~TmdLineObj()
{
    delete[] m_lines;
    if (m_ownsVertices && m_vertices) {
        delete[] m_vertices;
    }
}

int TmdLineObj::IsFacingAway()
{
    long sxy0, sxy1, sxy2;
    long unused;
    RotTransPers3(&m_vertices[m_faceVertices[0]], &m_vertices[m_faceVertices[1]],
                  &m_vertices[m_faceVertices[2]], &sxy0, &sxy1, &sxy2, &unused, &unused);
    return (u_long)NormalClip(sxy0, sxy1, sxy2) >> 31;
}

void TmdLineObj::Draw(int zOffset, SVECTOR* join)
{
    if (m_cullBack && IsFacingAway()) {
        return;
    }

    int n = m_numVertices + 1;
    SVECTOR work[n];
    DVECTOR screen[n];
    u_short depth[n];
    u_short interpolation[n];
    u_short flags[n];
    m_work = work;
    m_screen = screen;
    m_depth = depth;
    m_interpolation = interpolation;
    m_flags = flags;

    if (join) {
        work[m_numVertices] = *join;
        int last = m_numLines;
        m_numLines++;
        m_lines[last].v0 = n - 1;
        m_lines[last].v1 = 0;
        m_lines[last].color = m_lines[0].color;
    }

    GsOT* ot = VideoSys::GetOT();
    int shift = 16 - ot->length;
    GsOT_TAG* tags = ot->org;
    for (int i = 0; i < m_passes; i++) {
        DrawPass(tags, shift, zOffset, join);
    }

    if (join) {
        m_numLines--;
    }
}

void TmdLineObj::DrawPass(GsOT_TAG* tags, int shift, int zOffset, SVECTOR* join)
{
    int range = m_jitter.Integer();
    int bits = 32 - Lzc(range * 2);
    int mask = (1 << bits) - 1;
    int high = 32 - bits;

    Random random = g_jitterRandom;
    SVECTOR* vertices = m_vertices;
    int n = m_numVertices;
    SVECTOR* work = m_work;
    for (int i = 0; i < n; i++) {
        int dx = ((random.Next() >> high) & mask) - range;
        if (dx >= range) {
            dx -= range * 2;
        }
        int dy = ((random.Next() >> high) & mask) - range;
        work[i].vx = vertices[i].vx + dx;
        if (dy >= range) {
            dy -= range * 2;
        }
        int dz = ((random.Next() >> high) & mask) - range;
        work[i].vy = vertices[i].vy + dy;
        if (dz >= range) {
            dz -= range * 2;
        }
        work[i].vz = vertices[i].vz + zOffset + dz;
    }
    g_jitterRandom = random;

    int count = m_numVertices;
    if (join) {
        *join = work[count++ - 1];
        int lastLine = m_numLines - 1;
        ClipLine(&work[m_lines[lastLine].v0], &work[m_lines[lastLine].v1]);
    }
    RotTransPersN(m_work, m_screen, m_depth, m_interpolation, m_flags, count);

    CVECTOR color = m_color;
    int useColor = m_useColor;
    DVECTOR* screen = m_screen;
    int numLines = m_numLines;
    u_short* depth = m_depth;
    for (int i = 0; i < numLines; i++) {
        Line* line = &m_lines[i];
        int v0 = line->v0;
        int v1 = line->v1;
        if (depth[v0] && depth[v1]) {
            LINE_F2* p = (LINE_F2*)GsOUT_PACKET_P;
            if (!useColor) {
                *(u_long*)&p->r0 = *(u_long*)&line->color;
            } else {
                *(u_long*)&p->r0 = *(u_long*)&color;
            }
            setLineF2(p);
            *(u_long*)&p->x0 = *(u_long*)&screen[v0];
            *(u_long*)&p->x1 = *(u_long*)&screen[v1];
            addPrim(tags + (depth[v0] >> shift), p);
            GsOUT_PACKET_P = (PACKET*)(p + 1);
        }
    }
}

/* Object `i` of a TMD file, with its pointers made absolute. */
static inline TmdObject GetObject(const TmdHeader* tmd, int i)
{
    TmdObject object = tmd->objects[i];
    object.vertices = (SVECTOR*)((u_long)tmd->objects + (u_long)object.vertices);
    object.normals = (SVECTOR*)((u_long)tmd->objects + (u_long)object.normals);
    object.primitives = (u_long*)((u_long)tmd->objects + (u_long)object.primitives);
    return object;
}

inline TmdModel::TmdModel(const MemPtr<u_long>& file) : m_file(file)
{
    m_numObjects = ((TmdHeader*)m_file.Data())->numObjects;
    m_objects = new TmdLineObj*[m_numObjects];
    for (int i = 0; i < m_numObjects; i++) {
        TmdHeader* tmd = (TmdHeader*)m_file.Data();
        TmdObject object = GetObject(tmd, i);
        m_objects[i] = new TmdLineObj(&object, 0);
    }
}

TmdModel* LoadTmdModel(const char* name)
{
    return new TmdModel(MemPtr<u_long>(name));
}

#ifdef GAME
/* The first object of a TMD file, with its own copy of the vertices (the file is let go). */
TmdLineObj* LoadTmdLineObj(const char* name)
{
    MemPtr<u_long> file(name);
    TmdObject object = GetObject((TmdHeader*)file.Data(), 0);
    return new TmdLineObj(&object, 1);
}
#endif
