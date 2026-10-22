#ifndef COORD_H
#define COORD_H

#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

/*
 * A libgs coordinate system, positioned in its parent's. A Coord whose
 * parent is the world has no parent as far as libgs is concerned.
 */
class Coord : public GsCOORDINATE2 {
public:
    /* with the matrix to be filled in */
    Coord(Coord* parent = &s_world) : m_matrix(&coord)
    {
        SetParent(parent);
        Invalidate();
    }
    /* scaled by `scale`, at the origin of the world */
    Coord(short scale) : m_matrix(&coord)
    {
        SetScale(scale, scale, scale);
        SetTranslation(0, 0, 0);
        SetParent(&s_world);
        Invalidate();
    }

    void SetScale(short x, short y, short z)
    {
        MATRIX* m = m_matrix;
        m->m[0][0] = x;
        m->m[0][1] = 0;
        m->m[0][2] = 0;
        m->m[1][0] = 0;
        m->m[1][1] = y;
        m->m[1][2] = 0;
        m->m[2][0] = 0;
        m->m[2][1] = 0;
        m->m[2][2] = z;
    }
    void SetTranslation(long x, long y, long z)
    {
        long* t = m_matrix->t;
        t[0] = x;
        t[1] = y;
        t[2] = z;
    }
    void SetParent(Coord* parent) { super = LibgsParent(parent); }
    /* recompute the matrices on the next GsGetLs() */
    void Invalidate() { flg = 0; }

    MATRIX* Matrix() { return m_matrix; }
    /* the matrix's rotation set from `angles` (RotMatrixZYX()) */
    void Rotate(SVECTOR* angles) { RotMatrixZYX(angles, m_matrix); }
    /* the matrix scaled along x, y and z (4096 is 1) */
    void Scale(const short* values)
    {
        VECTOR scale;
        for (int i = 0; i < 3; i++) {
            (&scale.vx)[i] = values[i];
        }
        ScaleMatrix(m_matrix, &scale);
    }

    static GsCOORDINATE2* LibgsParent(Coord* coord) { return coord == &s_world ? NULL : coord; }

    static Coord s_world;

private:
    MATRIX* m_matrix;
};

#endif
