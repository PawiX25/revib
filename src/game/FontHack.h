#ifndef FONTHACK_H
#define FONTHACK_H

#include "common.h"

#include <libgte.h>
#include <libgpu.h>

/*
 * Lets the PSY-Q debug font live anywhere in VRAM.
 *
 * FntFlush() gives its sprites texture coordinates for a font loaded at the
 * top-left corner of a texture page. FontHack redirects the AddPrim() call
 * inside FntFlush() to AddSprite(), which first moves each sprite's texture
 * coordinates to where Load() really put the font. The destructor puts the
 * original call back.
 */
class FontHack {
public:
    typedef u_long* (*FlushFunc)(int id);

    FontHack(FlushFunc fntFlush);
    ~FontHack();

    /* FntLoad() for a font at any position in VRAM. */
    void Load(int x, int y);

private:
    static void AddSprite(u_long* ot, SPRT_8* sprite);

    FlushFunc m_flush;
    u_long m_original; /* address the patched call went to */

    /* The compiler lays these out in reverse order of declaration (.sdata). */
    static int s_v;
    static int s_u;
};

#endif
