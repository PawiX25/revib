/*
 * libgpu and libgs: the drawing and display environments, ordering tables,
 * the primitives libgs builds (boxes, sprites, clears), VRAM transfers, and
 * libgs's coordinate systems and view.
 */
#include <string.h>
#include <math.h>
#include <sys/types.h>

extern "C" {
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libetc.h>
}

#include "psx.h"

/* gte.cpp */
void gte_mul_matrix(const MATRIX* a, const MATRIX* b, MATRIX* out);

extern "C" {

/* --- libetc --- */

static long s_videoMode = MODE_NTSC;

long SetVideoMode(long mode)
{
    long previous = s_videoMode;
    s_videoMode = mode;
    return previous;
}

/* --- libgpu --- */

static int s_displayOn = 0;
static DISPENV s_dispEnv;

static void UpdateDisplay(void)
{
    gpu_set_display(s_dispEnv.disp.x, s_dispEnv.disp.y, s_dispEnv.disp.w ? s_dispEnv.disp.w : 512,
                    s_dispEnv.disp.h ? s_dispEnv.disp.h : 288, s_displayOn);
}

int ResetGraph(int mode)
{
    return 0;
}

void SetDispMask(int mask)
{
    s_displayOn = mask != 0;
    UpdateDisplay();
}

DISPENV* PutDispEnv(DISPENV* env)
{
    s_dispEnv = *env;
    UpdateDisplay();
    return env;
}

DRAWENV* PutDrawEnv(DRAWENV* env)
{
    uint32_t words[6];
    words[0] = 0xE1000000 | (env->tpage & 0x7FF) | (env->dtd ? 0x200 : 0) | (env->dfe ? 0x400 : 0);
    words[1] = 0xE2000000;
    words[2] = 0xE3000000 | (env->clip.x & 1023) | ((env->clip.y & 511) << 10);
    words[3] = 0xE4000000 | ((env->clip.x + env->clip.w - 1) & 1023) | (((env->clip.y + env->clip.h - 1) & 511) << 10);
    words[4] = 0xE5000000 | (env->ofs[0] & 0x7FF) | ((env->ofs[1] & 0x7FF) << 11);
    words[5] = 0xE6000000;
    gpu_command(words, 6);
    if (env->isbg) {
        uint32_t fill[3] = { 0x02000000u | env->r0 | (env->g0 << 8) | (env->b0 << 16),
                             (uint32_t)(env->clip.x & 0xFFFF) | ((uint32_t)env->clip.y << 16),
                             (uint32_t)(env->clip.w & 0xFFFF) | ((uint32_t)env->clip.h << 16) };
        gpu_command(fill, 3);
    }
    return env;
}

int DrawSync(int mode)
{
    return 0;
}

u_long DrawSyncCallback(void (*func)(void))
{
    return 0;
}

int LoadImage(RECT* rect, u_long* p)
{
    uint16_t* vram = gpu_vram();
    const uint16_t* src = (const uint16_t*)p;
    for (int y = 0; y < rect->h; y++) {
        for (int x = 0; x < rect->w; x++) {
            vram[((rect->y + y) & 511) * 1024 + ((rect->x + x) & 1023)] = *src++;
        }
    }
    return 0;
}

int ClearImage(RECT* rect, u_char r, u_char g, u_char b)
{
    uint32_t fill[3] = { 0x02000000u | r | (g << 8) | (b << 16), (uint32_t)(rect->x & 0xFFFF) | ((uint32_t)rect->y << 16),
                         (uint32_t)(rect->w & 0xFFFF) | ((uint32_t)rect->h << 16) };
    gpu_command(fill, 3);
    return 0;
}

static inline void SetLen(void* p, int len)
{
    ((P_TAG*)p)->len = len;
}

void SetDrawArea(DR_AREA* p, RECT* r)
{
    SetLen(p, 2);
    p->code[0] = 0xE3000000 | (r->x & 1023) | ((r->y & 511) << 10);
    p->code[1] = 0xE4000000 | ((r->x + r->w - 1) & 1023) | (((r->y + r->h - 1) & 511) << 10);
}

void SetDrawOffset(DR_OFFSET* p, u_short* ofs)
{
    SetLen(p, 2);
    p->code[0] = 0xE5000000 | (ofs[0] & 0x7FF) | ((ofs[1] & 0x7FF) << 11);
    p->code[1] = 0;
}

void SetDrawMove(DR_MOVE* p, RECT* rect, int x, int y)
{
    SetLen(p, 5);
    p->code[0] = 0x01000000;
    p->code[1] = 0x80000000;
    p->code[2] = (rect->x & 0xFFFF) | ((u_long)rect->y << 16);
    p->code[3] = (x & 0xFFFF) | ((u_long)y << 16);
    p->code[4] = (rect->w & 0xFFFF) | ((u_long)rect->h << 16);
}

/* the debug font: not drawn */
void FntLoad(int tx, int ty) {}
int FntOpen(int x, int y, int w, int h, int isbg, int n) { return 0; }
int FntPrint(...) { return 0; }
u_long* FntFlush(int id) { return NULL; }

/* --- libgs --- */

short PSDBASEX[2], PSDBASEY[2];
short PSDIDX;
u_long PSDCNT;
DRAWENV GsDRAWENV;
DISPENV GsDISPENV;
MATRIX GsWSMATRIX;
long HWD0, VWD0;
MATRIX GsIDMATRIX = { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } };
MATRIX GsIDMATRIX2 = { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } };
PACKET* GsOUT_PACKET_P;

static int s_offsetGpu = 1; /* GsOFSGPU: the GPU adds the screen's centre */

void GsInitGraph(unsigned short x, unsigned short y, unsigned short intmode, unsigned short dith, unsigned short vrammode)
{
    HWD0 = x;
    VWD0 = y;
    s_offsetGpu = (intmode & GsOFSGPU) != 0;
    memset(&GsDRAWENV, 0, sizeof(GsDRAWENV));
    memset(&GsDISPENV, 0, sizeof(GsDISPENV));
    GsDRAWENV.clip.w = x;
    GsDRAWENV.clip.h = y;
    GsDRAWENV.dtd = dith;
    GsDISPENV.disp.w = x;
    GsDISPENV.disp.h = y;
    GsDISPENV.screen.w = 256;
    GsDISPENV.screen.h = y;
    GsDISPENV.isinter = intmode & 1;
    PSDIDX = 0;
    PSDCNT = 1;
    GsWSMATRIX = GsIDMATRIX;
    /*
     * The pixels' aspect on a 4:3 screen: the view's y is scaled by it, so
     * that a circle stays round (512 x 288: 0.75).
     */
    GsIDMATRIX2 = GsIDMATRIX;
    GsIDMATRIX2.m[1][1] = (short)(4096 * (long)y * 4 / ((long)x * 3));
}

static void SetBuffer(int index)
{
    GsDRAWENV.clip.x = PSDBASEX[index];
    GsDRAWENV.clip.y = PSDBASEY[index];
    GsDRAWENV.clip.w = HWD0;
    GsDRAWENV.clip.h = VWD0;
    GsDRAWENV.ofs[0] = PSDBASEX[index] + (s_offsetGpu ? HWD0 / 2 : 0);
    GsDRAWENV.ofs[1] = PSDBASEY[index] + (s_offsetGpu ? VWD0 / 2 : 0);
    GsDISPENV.disp.x = PSDBASEX[index];
    GsDISPENV.disp.y = PSDBASEY[index];
}

void GsDefDispBuff(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1)
{
    PSDBASEX[0] = x0;
    PSDBASEY[0] = y0;
    PSDBASEX[1] = x1;
    PSDBASEY[1] = y1;
    PSDIDX = 0;
    SetBuffer(0);
    PutDrawEnv(&GsDRAWENV);
    PutDispEnv(&GsDISPENV);
}

/*
 * VideoSys::Flip() waits for the vertical blank (and the GPU) only when this
 * is 0; on the console every frame of the game lasts one vertical blank (its
 * movies' cues are timed in vertical blanks), so the port answers 0 every
 * frame. With PSDIDX, two frames of the game would go by in one.
 */
int GsGetActiveBuff(void)
{
    return 0;
}

void GsClearDispArea(unsigned char r, unsigned char g, unsigned char b)
{
    RECT rect = GsDISPENV.disp;
    ClearImage(&rect, r, g, b);
}

void GsSetWorkBase(PACKET* outpacketp)
{
    GsOUT_PACKET_P = outpacketp;
}

PACKET* GsGetWorkBase(void)
{
    return GsOUT_PACKET_P;
}

void GsClearOt(unsigned short offset, unsigned short point, GsOT* ot)
{
    int n = 1 << ot->length;
    ot->offset = offset;
    ot->point = point;
    GsOT_TAG* org = ot->org;
    /* each tag leads to the one before it: the last is drawn first */
    org[0].p = 0xFFFFFF;
    org[0].num = 0;
    for (int i = 1; i < n; i++) {
        org[i].p = (u_long)(uintptr_t)&org[i - 1];
        org[i].num = 0;
    }
    ot->tag = &org[n - 1];
}

void GsDrawOt(GsOT* ot)
{
    gpu_run((const uint32_t*)ot->tag);
}

static inline void AddPrim(void* tag, void* prim, int len)
{
    P_TAG* t = (P_TAG*)tag;
    P_TAG* p = (P_TAG*)prim;
    p->addr = t->addr;
    p->len = len;
    t->addr = (u_long)(uintptr_t)prim;
}

static inline GsOT_TAG* Tag(GsOT* ot, unsigned short pri)
{
    int n = 1 << ot->length;
    return &ot->org[pri < n ? pri : n - 1];
}

/* a word of drawing mode: its texture page, semi-transparency and colour depth */
static inline u_long DrawMode(int tpage, int abr, int depth)
{
    return 0xE1000000 | (tpage & 0x1F) | ((abr & 3) << 5) | ((depth & 3) << 7) | 0x400;
}

void GsSortBoxFill(GsBOXF* bp, GsOT* ot, unsigned short pri)
{
    if (bp->attribute & GsDOFF) {
        return;
    }
    u_long* p = (u_long*)GsOUT_PACKET_P;
    int semi = (bp->attribute >> 30) & 1;
    int abr = (bp->attribute >> 28) & 3;
    p[1] = DrawMode(0, abr, 0);
    p[2] = ((0x60 | (semi << 1)) << 24) | bp->r | (bp->g << 8) | (bp->b << 16);
    p[3] = (bp->x & 0xFFFF) | ((u_long)bp->y << 16);
    p[4] = (bp->w & 0xFFFF) | ((u_long)bp->h << 16);
    AddPrim(Tag(ot, pri), p, 4);
    GsOUT_PACKET_P = (PACKET*)(p + 5);
}

void GsSortFastSprite(GsSPRITE* sp, GsOT* ot, unsigned short pri)
{
    if (sp->attribute & GsDOFF) {
        return;
    }
    u_long* p = (u_long*)GsOUT_PACKET_P;
    int semi = (sp->attribute >> 30) & 1;
    int abr = (sp->attribute >> 28) & 3;
    int depth = (sp->attribute >> 24) & 3;
    int raw = (sp->attribute >> 6) & 1;
    p[1] = DrawMode(sp->tpage, abr, depth);
    p[2] = ((0x64 | (semi << 1) | raw) << 24) | sp->r | (sp->g << 8) | (sp->b << 16);
    p[3] = (sp->x & 0xFFFF) | ((u_long)sp->y << 16);
    u_long clut = ((sp->cy & 0x1FF) << 6) | ((sp->cx >> 4) & 0x3F);
    p[4] = sp->u | (sp->v << 8) | (clut << 16);
    p[5] = (sp->w & 0xFFFF) | ((u_long)sp->h << 16);
    AddPrim(Tag(ot, pri), p, 5);
    GsOUT_PACKET_P = (PACKET*)(p + 6);
}

void GsSortClear(unsigned char r, unsigned char g, unsigned char b, GsOT* ot)
{
    u_long* p = (u_long*)GsOUT_PACKET_P;
    p[1] = 0x02000000 | r | (g << 8) | (b << 16);
    p[2] = (GsDRAWENV.clip.x & 0xFFFF) | ((u_long)GsDRAWENV.clip.y << 16);
    p[3] = (GsDRAWENV.clip.w & 0xFFFF) | ((u_long)GsDRAWENV.clip.h << 16);
    AddPrim(ot->tag, p, 3);
    GsOUT_PACKET_P = (PACKET*)(p + 4);
}

/* --- libgs's 3D: coordinate systems, the view --- */

void GsInit3D(void)
{
    SetGeomOffset(0, 0);
    SetGeomScreen(1000);
    GsWSMATRIX = GsIDMATRIX;
}

void GsSetProjection(long h)
{
    SetGeomScreen(h);
}

void GsInitCoordinate2(GsCOORDINATE2* super, GsCOORDINATE2* base)
{
    base->flg = 0;
    base->coord = GsIDMATRIX;
    base->workm = GsIDMATRIX;
    base->param = NULL;
    base->super = super;
    base->sub = NULL;
    if (super != NULL) {
        super->sub = base;
    }
}

/* the matrix from a coordinate system to the world */
static void WorldMatrix(GsCOORDINATE2* c, MATRIX* out)
{
    if (c == NULL) {
        *out = GsIDMATRIX;
        return;
    }
    if (c->super == NULL) {
        *out = c->coord;
    } else {
        MATRIX parent;
        WorldMatrix(c->super, &parent);
        gte_mul_matrix(&parent, &c->coord, out);
    }
    c->workm = *out;
    c->flg = PSDCNT;
}

void GsGetLs(GsCOORDINATE2* m, MATRIX* out)
{
    MATRIX world;
    WorldMatrix(m, &world);
    gte_mul_matrix(&GsWSMATRIX, &world, out);
}

void GsSetLsMatrix(MATRIX* mp)
{
    SetRotMatrix(mp);
    SetTransMatrix(mp);
}

/* the inverse of a rotation and translation (the rotation's transpose) */
static void Invert(const MATRIX* m, MATRIX* out)
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out->m[i][j] = m->m[j][i];
        }
    }
    for (int i = 0; i < 3; i++) {
        long long sum = 0;
        for (int j = 0; j < 3; j++) {
            sum += (long long)out->m[i][j] * m->t[j];
        }
        out->t[i] = (long)(-(sum >> 12));
    }
}

int GsSetRefView2(GsRVIEW2* pv)
{
    /* z along the line of sight, x across it level with the world, y down the screen; then the twist around z */
    double fx = pv->vrx - pv->vpx, fy = pv->vry - pv->vpy, fz = pv->vrz - pv->vpz;
    double len = sqrt(fx * fx + fy * fy + fz * fz);
    if (len == 0) {
        return -1;
    }
    fx /= len;
    fy /= len;
    fz /= len;
    /* x = (0, 1, 0) x f */
    double xx = fz, xy = 0, xz = -fx;
    double xl = sqrt(xx * xx + xz * xz);
    if (xl < 1e-9) {
        xx = 1;
        xz = 0;
        xl = 1;
    }
    xx /= xl;
    xz /= xl;
    /* y = f x x */
    double yx = fy * xz - fz * xy, yy = fz * xx - fx * xz, yz = fx * xy - fy * xx;
    /* the twist: rz in 1/4096 degrees */
    double twist = pv->rz / 4096.0 * M_PI / 180.0;
    double c = cos(twist), s = sin(twist);
    double rx[3] = { c * xx + s * yx, c * xy + s * yy, c * xz + s * yz };
    double ry[3] = { -s * xx + c * yx, -s * xy + c * yy, -s * xz + c * yz };
    double rf[3] = { fx, fy, fz };
    MATRIX view;
    for (int j = 0; j < 3; j++) {
        view.m[0][j] = (short)lround(rx[j] * 4096);
        view.m[1][j] = (short)lround(ry[j] * 4096);
        view.m[2][j] = (short)lround(rf[j] * 4096);
    }
    long vp[3] = { pv->vpx, pv->vpy, pv->vpz };
    for (int i = 0; i < 3; i++) {
        long long sum = 0;
        for (int j = 0; j < 3; j++) {
            sum += (long long)view.m[i][j] * vp[j];
        }
        view.t[i] = (long)(-(sum >> 12));
    }
    /* the pixels' aspect (GsIDMATRIX2) */
    gte_mul_matrix(&GsIDMATRIX2, &view, &view);
    if (pv->super != NULL) {
        /* the view moves with its coordinate system: world -> super -> view */
        MATRIX superWorld, inverse;
        WorldMatrix(pv->super, &superWorld);
        Invert(&superWorld, &inverse);
        gte_mul_matrix(&view, &inverse, &GsWSMATRIX);
    } else {
        GsWSMATRIX = view;
    }
    return 0;
}

} /* extern "C" */
