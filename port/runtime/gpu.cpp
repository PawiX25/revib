/*
 * The GPU: 1 MB of VRAM (1024 x 512 16-bit pixels) and the GP0 commands of
 * the primitives the game and libgs build, run when an ordering table is
 * drawn. Drawing is done in software, at the console's resolution.
 */
#include <string.h>
#include <stdlib.h>

#include "psx.h"

static uint16_t s_vram[512 * 1024];

/* the drawing environment (GP0 E1-E6) */
static struct {
    int texX, texY;       /* texture page base, in VRAM pixels */
    int abr;              /* semi-transparency mode */
    int texDepth;         /* 0: 4-bit, 1: 8-bit, 2: 15-bit */
    int areaX1, areaY1, areaX2, areaY2;
    int offX, offY;
    int texMaskX, texMaskY, texOffX, texOffY;
    int setMask, checkMask;
} s_env;

static struct {
    int x, y, w, h;
    int on;
} s_display = { 0, 0, 512, 288, 0 };

static uint8_t s_rgba[640 * 512 * 4];

extern "C" uint16_t* gpu_vram(void)
{
    return s_vram;
}

/*
 * What was drawn into each frame buffer, for the page to draw again at the
 * screen's own resolution (as vectors: Vib-Ribbon is lines): the frame's
 * fills, lines, boxes and sprites, in order, relative to the buffer's
 * corner. What cannot be drawn again that way (a copy within VRAM, a
 * texture taken from a frame buffer, polygons) marks the frame `fallback`:
 * the page shows the VRAM's picture instead.
 */
enum { OP_FILL = 1, OP_LINE, OP_TILE, OP_SPRITE };
#define MAX_OPS (64 * 1024)

struct OpList {
    int x, y;     /* the buffer's corner (the drawing area's) */
    int n;        /* words in `ops` */
    int fallback;
    int32_t ops[MAX_OPS];
};

static OpList s_lists[2] = { { -1, -1 }, { -1, -1 } };
static OpList* s_list = NULL; /* that of the drawing area */

/* the list of the buffer at (x, y); a new one replaces the list not on display */
static OpList* ListAt(int x, int y)
{
    for (int i = 0; i < 2; i++) {
        if (s_lists[i].x == x && s_lists[i].y == y) {
            return &s_lists[i];
        }
    }
    OpList* list = (s_lists[0].x == s_display.x && s_lists[0].y == s_display.y) ? &s_lists[1] : &s_lists[0];
    list->x = x;
    list->y = y;
    list->n = 0;
    list->fallback = 0;
    return list;
}

static int32_t* Record(OpList* list, int words)
{
    if (list == NULL || list->fallback) {
        return NULL;
    }
    if (list->n + words > MAX_OPS) {
        list->fallback = 1;
        return NULL;
    }
    int32_t* p = &list->ops[list->n];
    list->n += words;
    return p;
}

/* a VRAM rectangle overlaps a frame buffer with a list */
static int OverlapsBuffer(int x, int y, int w, int h)
{
    for (int i = 0; i < 2; i++) {
        const OpList& l = s_lists[i];
        if (l.x >= 0 && x < l.x + s_display.w && x + w > l.x && y < l.y + s_display.h && y + h > l.y) {
            return 1;
        }
    }
    return 0;
}

extern "C" void gpu_init(void)
{
    memset(s_vram, 0, sizeof(s_vram));
    memset(&s_env, 0, sizeof(s_env));
    s_env.areaX2 = 1023;
    s_env.areaY2 = 511;
}

extern "C" void gpu_set_display(int x, int y, int w, int h, int on)
{
    s_display.x = x;
    s_display.y = y;
    s_display.w = w > 640 ? 640 : w;
    s_display.h = h > 512 ? 512 : h;
    s_display.on = on;
}

extern "C" void gpu_present(void)
{
    int w = s_display.w;
    int h = s_display.h;
    uint8_t* out = s_rgba;
    for (int y = 0; y < h; y++) {
        const uint16_t* row = &s_vram[((s_display.y + y) & 511) * 1024];
        for (int x = 0; x < w; x++) {
            uint16_t c = s_display.on ? row[(s_display.x + x) & 1023] : 0;
            int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
            out[0] = (r << 3) | (r >> 2);
            out[1] = (g << 3) | (g >> 2);
            out[2] = (b << 3) | (b >> 2);
            out[3] = 255;
            out += 4;
        }
    }
    const OpList* list = NULL;
    for (int i = 0; i < 2; i++) {
        if (s_lists[i].x == s_display.x && s_lists[i].y == s_display.y) {
            list = &s_lists[i];
        }
    }
    if (list != NULL && s_display.on) {
        js_present(s_rgba, w, h, list->ops, list->n, list->fallback, s_vram);
    } else {
        js_present(s_rgba, w, h, NULL, 0, 1, s_vram);
    }
}

/* --- pixels --- */

static inline uint16_t To15(uint32_t rgb)
{
    return ((rgb >> 3) & 0x1F) | (((rgb >> 11) & 0x1F) << 5) | (((rgb >> 19) & 0x1F) << 10);
}

static inline uint16_t Blend(uint16_t back, uint16_t front, int abr)
{
    int br = back & 31, bg = (back >> 5) & 31, bb = (back >> 10) & 31;
    int fr = front & 31, fg = (front >> 5) & 31, fb = (front >> 10) & 31;
    int r, g, b;
    switch (abr) {
    case 0:
        r = (br + fr) >> 1; g = (bg + fg) >> 1; b = (bb + fb) >> 1;
        break;
    case 1:
        r = br + fr; g = bg + fg; b = bb + fb;
        break;
    case 2:
        r = br - fr; g = bg - fg; b = bb - fb;
        break;
    default:
        r = br + (fr >> 2); g = bg + (fg >> 2); b = bb + (fb >> 2);
        break;
    }
    r = r < 0 ? 0 : r > 31 ? 31 : r;
    g = g < 0 ? 0 : g > 31 ? 31 : g;
    b = b < 0 ? 0 : b > 31 ? 31 : b;
    return r | (g << 5) | (b << 10) | (front & 0x8000);
}

static inline void Plot(int x, int y, uint16_t color, int semi)
{
    if (x < s_env.areaX1 || x > s_env.areaX2 || y < s_env.areaY1 || y > s_env.areaY2) {
        return;
    }
    uint16_t* p = &s_vram[(y & 511) * 1024 + (x & 1023)];
    if (s_env.checkMask && (*p & 0x8000)) {
        return;
    }
    if (semi) {
        color = Blend(*p, color, s_env.abr);
    }
    *p = color | (s_env.setMask ? 0x8000 : 0);
}

/* a texel of the page at (`tx`, `ty`), 0 if transparent */
static inline uint16_t Texel(int tx, int ty, int u, int v, int depth, int clutX, int clutY)
{
    u = (u & ~(s_env.texMaskX * 8)) | ((s_env.texOffX & s_env.texMaskX) * 8);
    v = (v & ~(s_env.texMaskY * 8)) | ((s_env.texOffY & s_env.texMaskY) * 8);
    u &= 255;
    v &= 255;
    int y = (ty + v) & 511;
    switch (depth) {
    case 0: {
        uint16_t word = s_vram[y * 1024 + ((tx + (u >> 2)) & 1023)];
        int index = (word >> ((u & 3) * 4)) & 15;
        return s_vram[clutY * 1024 + ((clutX + index) & 1023)];
    }
    case 1: {
        uint16_t word = s_vram[y * 1024 + ((tx + (u >> 1)) & 1023)];
        int index = (word >> ((u & 1) * 8)) & 255;
        return s_vram[clutY * 1024 + ((clutX + index) & 1023)];
    }
    default:
        return s_vram[y * 1024 + ((tx + u) & 1023)];
    }
}

static inline uint16_t Modulate(uint16_t texel, int r, int g, int b)
{
    int tr = texel & 31, tg = (texel >> 5) & 31, tb = (texel >> 10) & 31;
    tr = (tr * r) >> 7; tg = (tg * g) >> 7; tb = (tb * b) >> 7;
    if (tr > 31) tr = 31;
    if (tg > 31) tg = 31;
    if (tb > 31) tb = 31;
    return tr | (tg << 5) | (tb << 10) | (texel & 0x8000);
}

/* --- primitives --- */

struct Vertex {
    int x, y;
    int r, g, b;
    int u, v;
};

static inline int SignExtend11(uint32_t value)
{
    return ((int)(value << 21)) >> 21;
}

static void ReadXY(uint32_t word, Vertex* v)
{
    v->x = SignExtend11(word) + s_env.offX;
    v->y = SignExtend11(word >> 16) + s_env.offY;
}

static void ReadColor(uint32_t word, Vertex* v)
{
    v->r = word & 255;
    v->g = (word >> 8) & 255;
    v->b = (word >> 16) & 255;
}

static void Line(const Vertex& a, const Vertex& b, int gouraud, int semi)
{
    int dx = b.x - a.x, dy = b.y - a.y;
    if (abs(dx) >= 1024 || abs(dy) >= 512) {
        return;
    }
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (steps == 0) {
        Plot(a.x, a.y, To15(a.r | (a.g << 8) | (a.b << 16)), semi);
        return;
    }
    /* 16.16 steps, as the GPU does */
    int64_t x = ((int64_t)a.x << 16) + 0x8000, y = ((int64_t)a.y << 16) + 0x8000;
    int64_t sx = ((int64_t)dx << 16) / steps, sy = ((int64_t)dy << 16) / steps;
    if (dx < 0 && abs(dx) >= abs(dy)) x -= 1;
    if (dy < 0 && abs(dy) >= abs(dx)) y -= 1;
    for (int i = 0; i <= steps; i++) {
        uint16_t color;
        if (gouraud) {
            int r = a.r + (b.r - a.r) * i / steps;
            int g = a.g + (b.g - a.g) * i / steps;
            int bl = a.b + (b.b - a.b) * i / steps;
            color = To15(r | (g << 8) | (bl << 16));
        } else {
            color = To15(a.r | (a.g << 8) | (a.b << 16));
        }
        Plot((int)(x >> 16), (int)(y >> 16), color, semi);
        x += sx;
        y += sy;
    }
}

static int s_tpageX, s_tpageY, s_tpageDepth; /* of the textured primitive being drawn */
static int s_clutX, s_clutY;

static void Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, int gouraud, int textured, int raw, int semi)
{
    int area = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
    if (area == 0) {
        return;
    }
    const Vertex* a = &v0;
    const Vertex* b = &v1;
    const Vertex* c = &v2;
    if (area < 0) {
        const Vertex* t = b;
        b = c;
        c = t;
        area = -area;
    }
    int minX = v0.x < v1.x ? v0.x : v1.x; minX = minX < v2.x ? minX : v2.x;
    int maxX = v0.x > v1.x ? v0.x : v1.x; maxX = maxX > v2.x ? maxX : v2.x;
    int minY = v0.y < v1.y ? v0.y : v1.y; minY = minY < v2.y ? minY : v2.y;
    int maxY = v0.y > v1.y ? v0.y : v1.y; maxY = maxY > v2.y ? maxY : v2.y;
    if (maxX - minX >= 1024 || maxY - minY >= 512) {
        return;
    }
    if (minX < s_env.areaX1) minX = s_env.areaX1;
    if (minY < s_env.areaY1) minY = s_env.areaY1;
    if (maxX > s_env.areaX2) maxX = s_env.areaX2;
    if (maxY > s_env.areaY2) maxY = s_env.areaY2;
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            /* edge functions; the right and bottom edges are left out */
            int w0 = (c->x - b->x) * (y - b->y) - (c->y - b->y) * (x - b->x);
            int w1 = (a->x - c->x) * (y - c->y) - (a->y - c->y) * (x - c->x);
            int w2 = (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x);
            if (w0 < 0 || w1 < 0 || w2 < 0) {
                continue;
            }
            if ((w0 == 0 && !((c->y > b->y) || (c->y == b->y && c->x < b->x))) ||
                (w1 == 0 && !((a->y > c->y) || (a->y == c->y && a->x < c->x))) ||
                (w2 == 0 && !((b->y > a->y) || (b->y == a->y && b->x < a->x)))) {
                continue;
            }
            int r = a->r, g = a->g, bl = a->b;
            if (gouraud) {
                r = (a->r * w0 + b->r * w1 + c->r * w2) / area;
                g = (a->g * w0 + b->g * w1 + c->g * w2) / area;
                bl = (a->b * w0 + b->b * w1 + c->b * w2) / area;
            }
            uint16_t color;
            int pixelSemi = semi;
            if (textured) {
                int u = (a->u * w0 + b->u * w1 + c->u * w2) / area;
                int v = (a->v * w0 + b->v * w1 + c->v * w2) / area;
                uint16_t texel = Texel(s_tpageX, s_tpageY, u, v, s_tpageDepth, s_clutX, s_clutY);
                if (texel == 0) {
                    continue;
                }
                pixelSemi = semi && (texel & 0x8000);
                color = raw ? texel : Modulate(texel, r, g, bl);
            } else {
                color = To15(r | (g << 8) | (bl << 16));
            }
            Plot(x, y, color, pixelSemi);
        }
    }
}

static void Rect(int x0, int y0, int w, int h, uint32_t color, int textured, int raw, int semi, int u0, int v0)
{
    int r = color & 255, g = (color >> 8) & 255, b = (color >> 16) & 255;
    uint16_t flat = To15(color);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int px = x0 + x, py = y0 + y;
            if (px < s_env.areaX1 || px > s_env.areaX2 || py < s_env.areaY1 || py > s_env.areaY2) {
                continue;
            }
            if (textured) {
                uint16_t texel = Texel(s_env.texX, s_env.texY, u0 + x, v0 + y, s_env.texDepth, s_clutX, s_clutY);
                if (texel == 0) {
                    continue;
                }
                Plot(px, py, raw ? texel : Modulate(texel, r, g, b), semi && (texel & 0x8000));
            } else {
                Plot(px, py, flat, semi);
            }
        }
    }
}

static void SetDrawMode(uint32_t word)
{
    s_env.texX = (word & 15) * 64;
    s_env.texY = ((word >> 4) & 1) * 256;
    s_env.abr = (word >> 5) & 3;
    s_env.texDepth = (word >> 7) & 3;
    if (s_env.texDepth == 3) {
        s_env.texDepth = 2;
    }
}

static void CopyVram(uint32_t from, uint32_t to, uint32_t size)
{
    int sx = from & 1023, sy = (from >> 16) & 511;
    int dx = to & 1023, dy = (to >> 16) & 511;
    int w = ((size & 0xFFFF) - 1) & 1023, h = (((size >> 16) & 0xFFFF) - 1) & 511;
    w++;
    h++;
    static uint16_t line[1024];
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            line[x] = s_vram[((sy + y) & 511) * 1024 + ((sx + x) & 1023)];
        }
        for (int x = 0; x < w; x++) {
            uint16_t* p = &s_vram[((dy + y) & 511) * 1024 + ((dx + x) & 1023)];
            if (s_env.checkMask && (*p & 0x8000)) {
                continue;
            }
            *p = line[x] | (s_env.setMask ? 0x8000 : 0);
        }
    }
}

static void Fill(uint32_t color, uint32_t xy, uint32_t size)
{
    int x0 = xy & 0x3F0, y0 = (xy >> 16) & 0x1FF;
    int w = ((size & 0x3FF) + 15) & ~15, h = (size >> 16) & 0x1FF;
    uint16_t c = To15(color);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            s_vram[((y0 + y) & 511) * 1024 + ((x0 + x) & 1023)] = c;
        }
    }
}

/* the words of a GP0 command, from its first */
static int CommandWords(const uint32_t* p, int left)
{
    uint32_t op = p[0] >> 24;
    if (op >= 0x20 && op < 0x40) {
        int vertices = (op & 8) ? 4 : 3;
        int perVertex = 1 + ((op & 4) ? 1 : 0) + ((op & 0x10) ? 1 : 0);
        return 1 + vertices * perVertex - ((op & 0x10) ? 1 : 0);
    }
    if (op >= 0x40 && op < 0x60) {
        if (op & 8) {
            /* a polyline: up to its 0x5xxx5xxx end */
            int n = 1;
            while (n < left && (p[n] & 0xF000F000) != 0x50005000) {
                n++;
            }
            return n + 1 > left ? left : n + 1;
        }
        return (op & 0x10) ? 4 : 3;
    }
    if (op >= 0x60 && op < 0x80) {
        int n = 2;
        if (op & 4) n++;
        if (((op >> 3) & 3) == 0) n++;
        return n;
    }
    if (op >= 0x80 && op < 0xA0) {
        return 4;
    }
    if (op == 0x02) {
        return 3;
    }
    return 1;
}

static void Run(const uint32_t* p)
{
    uint32_t word = p[0];
    uint32_t op = word >> 24;
    if (op == 0x02) {
        Fill(word, p[1], p[2]);
        int x = p[1] & 0x3F0, y = (p[1] >> 16) & 0x1FF;
        int w = ((p[2] & 0x3FF) + 15) & ~15, h = (p[2] >> 16) & 0x1FF;
        OpList* list = NULL;
        if (w >= s_display.w && h >= s_display.h) {
            list = ListAt(x, y);
            list->n = 0;
            list->fallback = 0;
        } else if (s_list != NULL && x >= s_list->x && y >= s_list->y && x < s_list->x + s_display.w && y < s_list->y + s_display.h) {
            list = s_list;
        }
        if (int32_t* r = Record(list, 6)) {
            r[0] = OP_FILL; r[1] = x - list->x; r[2] = y - list->y; r[3] = w; r[4] = h; r[5] = word & 0xFFFFFF;
        }
    } else if (op >= 0x20 && op < 0x40) {
        int gouraud = op & 0x10, quad = op & 8, textured = op & 4, semi = op & 2, raw = op & 1;
        Vertex v[4];
        int n = quad ? 4 : 3;
        int i = 0;
        uint32_t color = word & 0xFFFFFF;
        for (int k = 0; k < n; k++) {
            if (gouraud && k > 0) {
                color = p[++i] & 0xFFFFFF;
            }
            ReadColor(color, &v[k]);
            ReadXY(p[++i], &v[k]);
            if (textured) {
                uint32_t uv = p[++i];
                v[k].u = uv & 255;
                v[k].v = (uv >> 8) & 255;
                if (k == 0) {
                    s_clutX = ((uv >> 16) & 0x3F) * 16;
                    s_clutY = (uv >> 22) & 0x1FF;
                } else if (k == 1) {
                    uint32_t page = uv >> 16;
                    s_tpageX = (page & 15) * 64;
                    s_tpageY = ((page >> 4) & 1) * 256;
                    s_tpageDepth = (page >> 7) & 3;
                    if (s_tpageDepth == 3) s_tpageDepth = 2;
                    s_env.abr = (page >> 5) & 3;
                }
            }
        }
        if (s_list != NULL) {
            s_list->fallback = 1;
        }
        Triangle(v[0], v[1], v[2], gouraud, textured, raw, semi);
        if (quad) {
            Triangle(v[1], v[2], v[3], gouraud, textured, raw, semi);
        }
    } else if (op >= 0x40 && op < 0x60) {
        int gouraud = op & 0x10, poly = op & 8, semi = op & 2;
        Vertex a, b;
        ReadColor(word, &a);
        int i = 1;
        ReadXY(p[i++], &a);
        for (;;) {
            if (gouraud) {
                if ((p[i] & 0xF000F000) == 0x50005000) break;
                ReadColor(p[i++], &b);
            } else {
                b.r = a.r; b.g = a.g; b.b = a.b;
            }
            if (poly && (p[i] & 0xF000F000) == 0x50005000) break;
            ReadXY(p[i++], &b);
            Line(a, b, gouraud, semi);
            if (int32_t* r = Record(s_list, 8)) {
                r[0] = OP_LINE;
                r[1] = a.x - s_list->x; r[2] = a.y - s_list->y; r[3] = b.x - s_list->x; r[4] = b.y - s_list->y;
                r[5] = a.r | (a.g << 8) | (a.b << 16);
                r[6] = b.r | (b.g << 8) | (b.b << 16);
                r[7] = (semi ? 1 : 0) | (s_env.abr << 1) | (gouraud ? 8 : 0);
            }
            a = b;
            if (!poly) break;
        }
    } else if (op >= 0x60 && op < 0x80) {
        int textured = op & 4, semi = op & 2, raw = op & 1;
        int i = 1;
        Vertex v;
        ReadXY(p[i++], &v);
        int u = 0, vv = 0;
        if (textured) {
            uint32_t uv = p[i++];
            u = uv & 255;
            vv = (uv >> 8) & 255;
            s_clutX = ((uv >> 16) & 0x3F) * 16;
            s_clutY = (uv >> 22) & 0x1FF;
        }
        int w, h;
        switch ((op >> 3) & 3) {
        case 0: {
            uint32_t size = p[i++];
            w = size & 0x3FF;
            h = (size >> 16) & 0x1FF;
            break;
        }
        case 1: w = h = 1; break;
        case 2: w = h = 8; break;
        default: w = h = 16; break;
        }
        Rect(v.x, v.y, w, h, word & 0xFFFFFF, textured, raw, semi, u, vv);
        int flags = (semi ? 1 : 0) | (s_env.abr << 1) | (raw ? 8 : 0);
        if (!textured) {
            if (int32_t* r = Record(s_list, 7)) {
                r[0] = OP_TILE; r[1] = v.x - s_list->x; r[2] = v.y - s_list->y; r[3] = w; r[4] = h; r[5] = word & 0xFFFFFF; r[6] = flags;
            }
        } else if (s_list != NULL) {
            /* the texels it reads, in VRAM units */
            int perUnit = s_env.texDepth == 0 ? 4 : s_env.texDepth == 1 ? 2 : 1;
            if (OverlapsBuffer(s_env.texX + u / perUnit, s_env.texY + vv, (w + perUnit - 1) / perUnit + 1, h)) {
                s_list->fallback = 1;
            } else if (int32_t* r = Record(s_list, 14)) {
                r[0] = OP_SPRITE; r[1] = v.x - s_list->x; r[2] = v.y - s_list->y; r[3] = w; r[4] = h;
                r[5] = u; r[6] = vv; r[7] = s_env.texX; r[8] = s_env.texY; r[9] = s_env.texDepth;
                r[10] = s_clutX; r[11] = s_clutY; r[12] = word & 0xFFFFFF; r[13] = flags;
            }
        }
    } else if (op >= 0x80 && op < 0xA0) {
        CopyVram(p[1], p[2], p[3]);
        int dx = p[2] & 1023, dy = (p[2] >> 16) & 511;
        int w = p[3] & 0xFFFF, h = p[3] >> 16;
        for (int k = 0; k < 2; k++) {
            OpList& l = s_lists[k];
            if (l.x >= 0 && dx < l.x + s_display.w && dx + w > l.x && dy < l.y + s_display.h && dy + h > l.y) {
                l.fallback = 1;
            }
        }
    } else {
        switch (op) {
        case 0xE1:
            SetDrawMode(word);
            break;
        case 0xE2:
            s_env.texMaskX = word & 31;
            s_env.texMaskY = (word >> 5) & 31;
            s_env.texOffX = (word >> 10) & 31;
            s_env.texOffY = (word >> 15) & 31;
            break;
        case 0xE3:
            s_env.areaX1 = word & 1023;
            s_env.areaY1 = (word >> 10) & 511;
            s_list = ListAt(s_env.areaX1, s_env.areaY1);
            break;
        case 0xE4:
            s_env.areaX2 = word & 1023;
            s_env.areaY2 = (word >> 10) & 511;
            break;
        case 0xE5:
            s_env.offX = SignExtend11(word);
            s_env.offY = SignExtend11(word >> 11);
            break;
        case 0xE6:
            s_env.setMask = word & 1;
            s_env.checkMask = (word >> 1) & 1;
            break;
        }
    }
}

extern "C" void gpu_command(const uint32_t* words, int count)
{
    while (count > 0) {
        int n = CommandWords(words, count);
        if (n > count) {
            break;
        }
        Run(words);
        words += n;
        count -= n;
    }
}

/* the primitives of an ordering table, from `packet`: a header word (its length, the next one's address), then the commands */
extern "C" void gpu_run(const uint32_t* packet)
{
    int guard = 0;
    while (packet != NULL && guard++ < 1000000) {
        uint32_t header = packet[0];
        int length = header >> 24;
        if (length > 0) {
            gpu_command(packet + 1, length);
        }
        uint32_t next = header & 0xFFFFFF;
        if (next == 0xFFFFFF) {
            break;
        }
        packet = (const uint32_t*)(uintptr_t)next;
    }
}
