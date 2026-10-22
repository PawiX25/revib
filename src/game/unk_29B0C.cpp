#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libetc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Logos.h"
#include "Fixed.h"
#include "MemPtr.h"
#include "VideoSys.h"

struct TimBlock;

/* A TIM image file: this header, then its blocks. */
struct Tim {
    u_long id;
    u_long mode : 3;    /* 0: 4 bits per pixel, 1: 8, 2: 16 */
    u_long hasClut : 1; /* the pixels are CLUT indices; the CLUT comes first */

    int HasClut();
    TimBlock* FirstBlock();
};

#define GS_SUBTRACT 0x60000000 /* semi-transparent, subtracting */
#define SPRITE_NO_SHADING 0x40 /* GsSPRITE attribute: draw the colours as they are */

/* the CLUT or the pixels of a TIM, with where they go in VRAM */
struct TimBlock {
    u_long size; /* in bytes, this header included */
    u_short x, y, w, h;
    u_long data[1];

    TimBlock* Next() { return (TimBlock*)((u_char*)this + size); }
    RECT Rect()
    {
        RECT rect;
        memset(&rect, 0, sizeof(rect));
        rect.x = x;
        rect.y = y;
        rect.w = w;
        rect.h = h;
        return rect;
    }
};

/*
 * Where a picture of a logo screen goes, next to the others: LogoSet centres
 * the pictures on the screen by their size, then moves them all by the first
 * one's x and y.
 */
struct LogoPlacement {
    const char* file;
    int x, y; /* LOGO_NEXT: right of (below) the previous picture */
};

#define LOGO_NEXT -9999

struct LogoSet;

LogoSet* g_sonyLogos;
LogoSet* g_nanaOnShaLogos;

/* a box over the whole screen, taking away `shade` from what is under it */
static inline GsBOXF ScreenShade(int shade)
{
    GsBOXF box;
    memset(&box, 0, sizeof(box));
    box.r = shade;
    box.g = shade;
    box.b = shade;
    box.attribute = GS_SUBTRACT;
    box.x = VideoSys::Origin().x;
    box.y = VideoSys::Origin().y;
    box.w = VideoSys::Size().x;
    box.h = VideoSys::Size().y;
    return box;
}

static inline void SortBox(GsBOXF* box, int priority)
{
    GsSortBoxFill(box, VideoSys::GetOT(), priority);
}

/* Copies the pixels of the image, and its CLUT with `withClut`, into VRAM. */
static void LoadTim(Tim* tim, int withClut)
{
    TimBlock* block = (TimBlock*)(tim + 1);
    int hasClut = tim->hasClut;
    for (int i = !hasClut; i < 2; i++) {
        if (withClut || i == 1) {
            RECT rect = block->Rect();
            LoadImage(&rect, block->data);
        }
        block = block->Next();
    }
}

static inline TimBlock* Pixels(Tim* tim)
{
    return tim->HasClut() ? tim->FirstBlock()->Next() : tim->FirstBlock();
}

static inline TimBlock* Clut(Tim* tim)
{
    return tim->HasClut() ? tim->FirstBlock() : NULL;
}

/* in pixels; a 16-bit unit of VRAM holds 4, 2 or 1 */
static inline int PixelsPerUnit(Tim* tim)
{
    return 4 / (1 << tim->mode);
}

/* in pixels */
static inline int Width(Tim* tim)
{
    return Pixels(tim)->w * PixelsPerUnit(tim);
}

/* the texture page of a VRAM position */
static inline int TexturePage(int x, int y)
{
    return (x >> 6) + 16 * (y >> 8);
}

static void DrawTim(Tim* tim, int x, int y, int priority)
{
    int mode = tim->mode;
    u_long colorMode = mode << 24;
    int perUnit = 4 / (1 << mode);
    TimBlock* pixels = Pixels(tim);
    GsSPRITE sprite;
    sprite.attribute = colorMode | SPRITE_NO_SHADING;
    sprite.x = VideoSys::Origin().x + x;
    sprite.y = VideoSys::Origin().y + y;
    sprite.w = pixels->w * perUnit;
    sprite.h = pixels->h;
    sprite.tpage = TexturePage(pixels->x, pixels->y);
    sprite.u = (pixels->x & 0x3F) * perUnit;
    sprite.v = pixels->y;
    int hasClut = tim->hasClut;
    if (hasClut) {
        TimBlock* clut = Clut(tim);
        sprite.cx = clut->x;
        sprite.cy = clut->y;
    }
    GsSortFastSprite(&sprite, VideoSys::GetOT(), priority);
}

inline TimBlock* Tim::FirstBlock()
{
    return (TimBlock*)(this + 1);
}

inline int Tim::HasClut()
{
    return hasClut != 0;
}

/* A picture of a logo screen. */
struct Logo {
    MemPtr<Tim> tim;
    int x, y;
};

/* The pictures making up one logo screen; `x`, `y`: what their positions are counted from. */
struct LogoSet {
    int count;
    Logo* logos;
    int x, y;
    int width, height;

    LogoSet(const LogoPlacement* placements, int n);
    ~LogoSet() { delete[] logos; }
};

static int Frames(Fixed time)
{
    return (time * VideoSys::FrameRate()).Integer();
}

/* Shows the logos while the screen goes from `from` to `to` (0: black, 255: as drawn). */
static void Fade(LogoSet* set, int from, int to, Fixed time)
{
    int frames = Frames(time);
    int step = 0;
    if (frames != 0) {
        step = ((to - from) << 16) / frames;
    } else {
        step = 0;
        from = to;
    }
    for (int i = 0; i < frames + 3; i++) {
        VideoSys::Flip(1);
        int level = from + ((step * (i < frames ? i : frames)) >> 16);
        for (int j = 0; j < set->count; j++) {
            DrawTim(set->logos[j].tim.Data(), set->logos[j].x + set->x, set->logos[j].y + set->y, 1);
        }
        GsBOXF box = ScreenShade(255 - level);
        SortBox(&box, 0);
    }
}

static void Show(LogoSet* set, Fixed fadeIn, Fixed hold, Fixed fadeOut)
{
    Fade(set, 0, 247, fadeIn);
    int frames = Frames(hold);
    if (frames != 0) {
        VSync(frames != 1 ? frames : 0);
    }
    Fade(set, 247, 0, fadeOut);
}

static const LogoPlacement s_sony[] = {
    { "logos/sony_1.tim", 0, 14 },
    { "logos/sony_2.tim", LOGO_NEXT, 14 },
};

static const LogoPlacement s_nanaOnSha[] = {
    { "logos/7on_1.tim", -6, 0 },
    { "logos/7on_2.tim", LOGO_NEXT, 0 },
    { "logos/7on_3.tim", LOGO_NEXT, 0 },
    { "logos/7on_4.tim", LOGO_NEXT, 0 },
};

/* Loads the pictures and places them on the screen (see LogoPlacement). */
inline LogoSet::LogoSet(const LogoPlacement* placements, int n) : count(n), x(0), y(0)
{
    int right = 0;
    int bottom = 0;
    logos = new Logo[count];
    for (int i = 0; i < count; i++) {
        logos[i].tim = MemPtr<Tim>(placements[i].file);
        LoadTim(logos[i].tim.Data(), 1);
        logos[i].x = placements[i].x != LOGO_NEXT ? placements[i].x : right;
        logos[i].y = placements[i].y != LOGO_NEXT ? placements[i].y : bottom;
        right = logos[i].x + Width(logos[i].tim.Data());
        bottom = logos[i].y + Pixels(logos[i].tim.Data())->h;
    }
    width = right - placements[0].x;
    height = bottom - placements[0].y;
    x = (VideoSys::Width() - width) / 2;
    y = (VideoSys::Height() - height) / 2;
}

void LoadLogos()
{
    g_sonyLogos = new LogoSet(s_sony, 2);
    g_nanaOnShaLogos = new LogoSet(s_nanaOnSha, 4);
}

void FreeLogos()
{
    delete g_sonyLogos;
    delete g_nanaOnShaLogos;
}

void ShowSonyLogos(Fixed fadeIn, Fixed hold, Fixed fadeOut)
{
    Show(g_sonyLogos, fadeIn, hold, fadeOut);
}

void ShowNanaOnShaLogos(Fixed fadeIn, Fixed hold, Fixed fadeOut)
{
    Show(g_nanaOnShaLogos, fadeIn, hold, fadeOut);
}
