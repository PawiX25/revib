/*
 * libgte: the GTE's registers and the operations of the functions the game
 * calls (perspective transformation, normal clip, matrices), in the GTE's
 * fixed point (rotations in 4.12, angles 4096 a full turn).
 */
#include <math.h>
#include <string.h>
#include <sys/types.h>

extern "C" {
#include <libgte.h>
}

#include "psx.h"

static MATRIX s_rot;     /* RT and TR */
static long s_ofx, s_ofy; /* OFX, OFY: 16.16 */
static long s_h = 1000;   /* H */
static const long s_zsf3 = 0x155;

void gte_mul_matrix(const MATRIX* a, const MATRIX* b, MATRIX* out)
{
    MATRIX r;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            long long sum = 0;
            for (int k = 0; k < 3; k++) {
                sum += (long long)a->m[i][k] * b->m[k][j];
            }
            r.m[i][j] = (short)(sum >> 12);
        }
        long long t = 0;
        for (int k = 0; k < 3; k++) {
            t += (long long)a->m[i][k] * b->t[k];
        }
        r.t[i] = (long)(t >> 12) + a->t[i];
    }
    *out = r;
}

struct Projected {
    int sx, sy;
    long sz;
    long p;
    unsigned long flag;
};

static inline long Clamp(long long v, long lo, long hi, unsigned long* flag, unsigned long bit)
{
    if (v < lo) {
        *flag |= bit;
        return lo;
    }
    if (v > hi) {
        *flag |= bit;
        return hi;
    }
    return (long)v;
}

/* RTPS: one vertex through the rotation, the translation and the perspective */
static Projected Project(const SVECTOR* v)
{
    Projected out;
    out.flag = 0;
    long long mac[3];
    for (int i = 0; i < 3; i++) {
        mac[i] = ((long long)s_rot.t[i] << 12) + (long long)s_rot.m[i][0] * v->vx + (long long)s_rot.m[i][1] * v->vy +
                 (long long)s_rot.m[i][2] * v->vz;
        mac[i] >>= 12;
    }
    long ir1 = Clamp(mac[0], -0x8000, 0x7FFF, &out.flag, 1 << 24);
    long ir2 = Clamp(mac[1], -0x8000, 0x7FFF, &out.flag, 1 << 23);
    long sz = Clamp(mac[2], 0, 0xFFFF, &out.flag, 1 << 18);
    long long hsz;
    if (sz * 2 <= s_h) {
        hsz = 0x1FFFF;
        out.flag |= 1 << 17;
    } else {
        hsz = ((long long)s_h << 16) / sz;
        if (hsz > 0x1FFFF) {
            hsz = 0x1FFFF;
            out.flag |= 1 << 17;
        }
    }
    long long sx = (s_ofx + ir1 * hsz) >> 16;
    long long sy = (s_ofy + ir2 * hsz) >> 16;
    out.sx = Clamp(sx, -0x400, 0x3FF, &out.flag, 1 << 14);
    out.sy = Clamp(sy, -0x400, 0x3FF, &out.flag, 1 << 13);
    out.p = 0; /* no depth cueing: DQA and DQB are 0 */
    out.sz = sz;
    if (out.flag & 0x7F87E000) {
        out.flag |= 0x80000000;
    }
    return out;
}

static inline long PackXY(int x, int y)
{
    return (long)(((unsigned long)(unsigned short)y << 16) | (unsigned short)x);
}

extern "C" {

void SetRotMatrix(MATRIX* m)
{
    memcpy(s_rot.m, m->m, sizeof(s_rot.m));
}

void SetTransMatrix(MATRIX* m)
{
    memcpy(s_rot.t, m->t, sizeof(s_rot.t));
}

void SetGeomOffset(long ofx, long ofy)
{
    s_ofx = ofx << 16;
    s_ofy = ofy << 16;
}

void SetGeomScreen(long h)
{
    s_h = h;
}

long RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, long* sxy0, long* sxy1, long* sxy2, long* p, long* flag)
{
    Projected a = Project(v0), b = Project(v1), c = Project(v2);
    *sxy0 = PackXY(a.sx, a.sy);
    *sxy1 = PackXY(b.sx, b.sy);
    *sxy2 = PackXY(c.sx, c.sy);
    *p = c.p;
    *flag = a.flag | b.flag | c.flag;
    return (long)((s_zsf3 * (a.sz + b.sz + c.sz)) >> 12);
}

void RotTransPersN(SVECTOR* v0, DVECTOR* v1, u_short* sz, u_short* p, u_short* flag, long n)
{
    for (long i = 0; i < n; i++) {
        Projected a = Project(&v0[i]);
        v1[i].vx = a.sx;
        v1[i].vy = a.sy;
        sz[i] = (u_short)a.sz;
        p[i] = (u_short)a.p;
        flag[i] = (u_short)(a.flag >> 12);
    }
}

long NormalClip(long sxy0, long sxy1, long sxy2)
{
    long long x0 = (short)sxy0, y0 = (short)(sxy0 >> 16);
    long long x1 = (short)sxy1, y1 = (short)(sxy1 >> 16);
    long long x2 = (short)sxy2, y2 = (short)(sxy2 >> 16);
    return (long)(x0 * y1 + x1 * y2 + x2 * y0 - x0 * y2 - x1 * y0 - x2 * y1);
}

long Lzc(long data)
{
    unsigned long v = (unsigned long)data;
    if (data < 0) {
        v = ~v;
    }
    if (v == 0) {
        return 32;
    }
    return __builtin_clz(v);
}

int rsin(int a)
{
    return (int)lround(sin(a * (2 * M_PI / 4096.0)) * 4096.0);
}

int rcos(int a)
{
    return (int)lround(cos(a * (2 * M_PI / 4096.0)) * 4096.0);
}

int csqrt(int a)
{
    if (a <= 0) {
        return 0;
    }
    return (int)sqrt((double)a * 4096.0);
}

static void Rx(int a, MATRIX* m)
{
    int s = rsin(a), c = rcos(a);
    memset(m, 0, sizeof(*m));
    m->m[0][0] = 4096;
    m->m[1][1] = c;
    m->m[1][2] = -s;
    m->m[2][1] = s;
    m->m[2][2] = c;
}

static void Ry(int a, MATRIX* m)
{
    int s = rsin(a), c = rcos(a);
    memset(m, 0, sizeof(*m));
    m->m[0][0] = c;
    m->m[0][2] = s;
    m->m[1][1] = 4096;
    m->m[2][0] = -s;
    m->m[2][2] = c;
}

static void Rz(int a, MATRIX* m)
{
    int s = rsin(a), c = rcos(a);
    memset(m, 0, sizeof(*m));
    m->m[0][0] = c;
    m->m[0][1] = -s;
    m->m[1][0] = s;
    m->m[1][1] = c;
    m->m[2][2] = 4096;
}

static void Rotation(const MATRIX* a, const MATRIX* b, const MATRIX* c, MATRIX* m)
{
    MATRIX ab, abc;
    gte_mul_matrix(a, b, &ab);
    gte_mul_matrix(&ab, c, &abc);
    memcpy(m->m, abc.m, sizeof(m->m));
}

/* Rx * Ry * Rz */
MATRIX* RotMatrix(SVECTOR* r, MATRIX* m)
{
    MATRIX x, y, z;
    Rx(r->vx, &x);
    Ry(r->vy, &y);
    Rz(r->vz, &z);
    Rotation(&x, &y, &z, m);
    return m;
}

/* Rz * Ry * Rx */
MATRIX* RotMatrixZYX(SVECTOR* r, MATRIX* m)
{
    MATRIX x, y, z;
    Rx(r->vx, &x);
    Ry(r->vy, &y);
    Rz(r->vz, &z);
    Rotation(&z, &y, &x, m);
    return m;
}

MATRIX* TransMatrix(MATRIX* m, VECTOR* v)
{
    m->t[0] = v->vx;
    m->t[1] = v->vy;
    m->t[2] = v->vz;
    return m;
}

/* the columns scaled: M * diag(v) */
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v)
{
    long s[3] = { v->vx, v->vy, v->vz };
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            m->m[i][j] = (short)(((long long)m->m[i][j] * s[j]) >> 12);
        }
    }
    return m;
}

/* --- the polygon clipper: the view's frustum, in the coordinates of the rotation and translation --- */

static EVECTOR* s_clipPool;
static long s_clipHw, s_clipVw, s_clipH, s_clipNear, s_clipFar;

void InitClip(EVECTOR* evbfad, long hw, long vw, long h, long near, long far)
{
    s_clipPool = evbfad;
    s_clipHw = hw;
    s_clipVw = vw;
    s_clipH = h;
    s_clipNear = near;
    s_clipFar = far;
}

struct ClipVertex {
    double local[3];
    double view[3];
};

static void ToView(const SVECTOR* v, ClipVertex* out)
{
    out->local[0] = v->vx;
    out->local[1] = v->vy;
    out->local[2] = v->vz;
    for (int i = 0; i < 3; i++) {
        out->view[i] = s_rot.t[i] + (s_rot.m[i][0] * (double)v->vx + s_rot.m[i][1] * (double)v->vy + s_rot.m[i][2] * (double)v->vz) / 4096.0;
    }
}

/* the distance inside plane `k` (>= 0: inside) */
static double Inside(const ClipVertex& v, int k)
{
    double x = v.view[0], y = v.view[1], z = v.view[2];
    double kx = (double)s_clipHw / 2 / s_clipH, ky = (double)s_clipVw / 2 / s_clipH;
    switch (k) {
    case 0: return z - s_clipNear;
    case 1: return s_clipFar - z;
    case 2: return z * kx - x;
    case 3: return z * kx + x;
    case 4: return z * ky - y;
    default: return z * ky + y;
    }
}

static ClipVertex Lerp(const ClipVertex& a, const ClipVertex& b, double t)
{
    ClipVertex r;
    for (int i = 0; i < 3; i++) {
        r.local[i] = a.local[i] + (b.local[i] - a.local[i]) * t;
        r.view[i] = a.view[i] + (b.view[i] - a.view[i]) * t;
    }
    return r;
}

static void Store(const ClipVertex& v, EVECTOR* e)
{
    e->v.vx = (short)lround(v.local[0]);
    e->v.vy = (short)lround(v.local[1]);
    e->v.vz = (short)lround(v.local[2]);
    e->v.pad = 0;
    e->sxyz.vx = (long)v.view[0];
    e->sxyz.vy = (long)v.view[1];
    e->sxyz.vz = (long)v.view[2];
}

long Clip3F(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, EVECTOR** evmx)
{
    ClipVertex in[16], out[16];
    int n = 3;
    ToView(v0, &in[0]);
    ToView(v1, &in[1]);
    ToView(v2, &in[2]);

    if (v1->vx == v2->vx && v1->vy == v2->vy && v1->vz == v2->vz) {
        /* a line (the triangle's last two vertices the same): its two ends, clipped */
        double t0 = 0, t1 = 1;
        for (int k = 0; k < 6; k++) {
            double a = Inside(in[0], k), b = Inside(in[1], k);
            if (a < 0 && b < 0) {
                return 0;
            }
            if (a < 0) {
                double t = a / (a - b);
                if (t > t0) t0 = t;
            } else if (b < 0) {
                double t = a / (a - b);
                if (t < t1) t1 = t;
            }
        }
        if (t0 > t1) {
            return 0;
        }
        ClipVertex a = Lerp(in[0], in[1], t0), b = Lerp(in[0], in[1], t1);
        Store(a, &s_clipPool[0]);
        Store(b, &s_clipPool[1]);
        Store(b, &s_clipPool[2]);
        evmx[0] = &s_clipPool[0];
        evmx[1] = &s_clipPool[1];
        evmx[2] = &s_clipPool[2];
        return 3;
    }

    for (int k = 0; k < 6 && n > 0; k++) {
        int m = 0;
        for (int i = 0; i < n; i++) {
            const ClipVertex& a = in[i];
            const ClipVertex& b = in[(i + 1) % n];
            double da = Inside(a, k), db = Inside(b, k);
            if (da >= 0) {
                out[m++] = a;
            }
            if ((da >= 0) != (db >= 0) && m < 15) {
                out[m++] = Lerp(a, b, da / (da - db));
            }
        }
        memcpy(in, out, m * sizeof(ClipVertex));
        n = m;
    }
    for (int i = 0; i < n && i < 16; i++) {
        Store(in[i], &s_clipPool[i]);
        evmx[i] = &s_clipPool[i];
    }
    return n;
}

} /* extern "C" */
