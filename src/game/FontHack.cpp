#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "FontHack.h"

/* MIPS "jal": opcode 3, then bits 2..27 of the target, within the 256 MB segment of the call. */
#define OPCODE_MASK 0xFC000000
#define OPCODE_JAL 0x0C000000
#define SEGMENT_MASK 0xF0000000
#define JUMP_TARGET_MASK 0x03FFFFFF

/* the "jal AddPrim" in FntFlush() */
static inline u_long* CallSite(FontHack::FlushFunc flush)
{
    return (u_long*)((char*)flush + 0x218);
}

static inline void SetCallTarget(u_long* call, u_long target)
{
#line 60 "C:/psj/dev/locale/game/FontHack.cpp"
    assert(((u_long)call & SEGMENT_MASK) == (target & SEGMENT_MASK));
#line 26 "src/game/FontHack.cpp"
    *call = (*call & OPCODE_MASK) | (((target & 0x0FFFFFFF) >> 2) & JUMP_TARGET_MASK);
}

int FontHack::s_u;
int FontHack::s_v;

void FontHack::AddSprite(u_long* ot, SPRT_8* sprite)
{
    sprite->u0 += s_u;
    sprite->v0 += s_v;
    /* the CLUT is written back unchanged */
    u_short clut = sprite->clut;
    sprite->clut = clut;
    addPrim(ot, sprite);
}

FontHack::FontHack(FlushFunc fntFlush) : m_flush(fntFlush), m_original(0)
{
    u_long* call = CallSite(fntFlush);
    u_long insn = *call;
#line 106 "C:/psj/dev/locale/game/FontHack.cpp"
    assert((insn & OPCODE_MASK) == OPCODE_JAL);
#line 49 "src/game/FontHack.cpp"
    m_original = ((u_long)call & SEGMENT_MASK) + (insn & JUMP_TARGET_MASK) * 4;
    SetCallTarget(call, (u_long)AddSprite);
}

FontHack::~FontHack()
{
    SetCallTarget(CallSite(m_flush), m_original);
}

void FontHack::Load(int x, int y)
{
    s_u = x & 0x3F;
    s_v = y & 0xFF;
    FntLoad(x, y);
}

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/game/nonmatchings/FontHack", D_8001A188);
