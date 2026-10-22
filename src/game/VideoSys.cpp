#include "common.h"

#include <libapi.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
/* <kernel.h> renames the BIOS's delete() to erase() for C, and the compiler driver takes a .cpp file for C */
#undef delete

#include "Format.h"
#include "VideoSys.h"
#include <algorithm>

/* A GsOT with its tags. */
class OrderingTable : public GsOT {
public:
    OrderingTable(int bits)
    {
        length = bits;
        org = new GsOT_TAG[1 << bits];
        offset = 0;
    }
    ~OrderingTable() { delete[] org; }

    /* the lowest priority: drawn first */
    int Last() const { return (1 << length) - 1; }
    GsOT_TAG* LastTag() const { return org + Last(); }
};

/* The memory libgs builds a frame's primitives in. */
class PacketArea {
public:
    PacketArea(int size)
    {
        m_size = size;
        m_maxUsed = 0;
        m_inUse = 0;
        m_base = new PACKET[size];
    }
    ~PacketArea() { delete m_base; /* (delete, not delete[], as in the original) */ }

    void Begin()
    {
#line 50 "C:/psj/dev/locale/game/VideoSys.cpp"
        assert(!m_inUse);
#line 52 "src/game/VideoSys.cpp"
        m_inUse = 1;
        m_savedBase = GsGetWorkBase();
        GsSetWorkBase(m_base);
    }

    void End()
    {
#line 58 "C:/psj/dev/locale/game/VideoSys.cpp"
        assert(m_inUse);
#line 62 "src/game/VideoSys.cpp"
        m_inUse = 0;
        int used = GsGetWorkBase() - m_base;
#line 62 "C:/psj/dev/locale/game/VideoSys.cpp"
        assert(used <= m_size);
#line 67 "src/game/VideoSys.cpp"
        m_maxUsed = used > m_maxUsed ? used : m_maxUsed;
        GsSetWorkBase(m_savedBase);
    }

    int m_size;
    PACKET* m_base;
    int m_maxUsed;

private:
    PACKET* m_savedBase;
    int m_inUse;
};

Fixed VideoSys::s_vsyncRate;
Fixed VideoSys::s_vsyncPeriod;
Fixed VideoSys::s_frameRate;
Fixed VideoSys::s_framePeriod;
vector<VideoSys::Callback> VideoSys::s_vsyncCallbacks;
VideoSys::Buffer VideoSys::s_buffers[2];
int VideoSys::s_buffer = 0;
FontHack VideoSys::s_fontHack(FntFlush);

static DR_MOVE s_blurCopy; /* the blur's copy of the picture on display */
static DR_AREA s_drawArea;
static DR_OFFSET s_drawOffset;

void VideoSys::SwapBuffers()
{
    s_buffer = (s_buffer + 1) % 2;
}

void VideoSys::OnDrawSync() {}

void VideoSys::OnVSync()
{
    for (vector<Callback>::iterator it = s_vsyncCallbacks.begin(); it < s_vsyncCallbacks.end(); ++it) {
        (*it)();
    }
}

/* Where frame buffer `index` is in VRAM. */
inline int VideoSys::BufferX(short index)
{
    return s_disp.x * index;
}

inline int VideoSys::BufferY(short index)
{
    return s_disp.y * index;
}

#define VSYNC_RATE (s_videoMode == MODE_NTSC ? 59.825f : 49.759f)

void VideoSys::Init(int width, int reset, int interval, int doubleBuffer, int init3D)
{
    s_videoMode = MODE_PAL;
    SetVideoMode(MODE_PAL);
    s_interval = interval;
    s_doubleBuffer = doubleBuffer;
    if (doubleBuffer) {
        s_numBuffers = 2;
    } else {
        s_numBuffers = 1;
    }
    int hz = s_videoMode == MODE_NTSC ? 60 : 50;
    s_hz = hz;
    if (interval > 0) {
        s_fps = hz / s_interval;
    } else {
        s_fps = hz;
    }
    s_vsyncRate = Fixed(VSYNC_RATE);
    s_vsyncPeriod = Fixed(1.0 / VSYNC_RATE);
    s_frameRate = s_vsyncRate / interval;
    s_framePeriod = s_vsyncPeriod * interval;

    SetDispMask(0);
    u_short height = s_videoMode == MODE_NTSC ? 240 : 288;
    int flags = reset ? GsRESET3 | GsOFSGPU : GsOFSGPU;
    GsInitGraph(width, height, flags, 0, 0);
    GsDISPENV.screen.x = -2;
    GsDISPENV.screen.y = 0;
    GsDISPENV.screen.h = height;
    GsClearDispArea(0, 0, 0);
    if (doubleBuffer) {
        s_disp.x = width;
        s_disp.y = 0;
    } else {
        s_disp.x = 0;
        s_disp.y = 0;
    }
    GsDefDispBuff(0, 0, s_disp.x, s_disp.y);
    if (init3D) {
        GsInit3D();
    }
    SetDispMask(1);

    s_size.x = HWD0;
    s_size.y = VWD0;
    if (init3D) {
        s_origin.x = -s_size.x / 2;
    } else {
        s_origin.x = 0;
    }
    if (init3D) {
        s_origin.y = -s_size.y / 2;
    } else {
        s_origin.y = 0;
    }
    s_fontHack.Load(960, 288);
    s_font = FntOpen(s_origin.x + 16, s_origin.y + 16, s_size.x - 32, s_size.y - 32, 0, 1024);

    for (int i = 0; i < 2; i++) {
        s_buffers[i].ot = new OrderingTable(11);
        s_buffers[i].packets = new PacketArea(0x6000);
    }
    GsClearOt(0, 0, s_buffers[s_buffer].ot);
    s_buffers[s_buffer].packets->Begin();

    DrawSyncCallback(OnDrawSync);
    s_fade = 0;
    s_vsyncCallbacks.reserve(8);
    VSyncCallback(OnVSync);
}

void VideoSys::Quit()
{
    VSyncCallback(NULL);
    PacketArea* packets = s_buffers[0].packets;
    printf("Packet report: max %d of %d\n", packets->m_maxUsed, packets->m_size);
    for (int i = 0; i < 2; i++) {
        delete s_buffers[i].ot;
        delete s_buffers[i].packets;
    }
}

void VideoSys::Reset()
{
    ResetGraph(1);
    VSync(0);
    SetDispMask(0);
}

void VideoSys::Flip(int wait)
{
    if (GsGetActiveBuff() == 0) {
        DrawSync(0);
    }

    OrderingTable* ot = s_buffers[s_buffer].ot;
    if (s_fade == 50) {
        /* the blur at 50: the picture on display copied into the one being drawn, under a half-transparent black box */
        GsBOXF box;
        box.attribute = 1 << 30; /* semi-transparent, half and half */
        box.x = -HWD0 / 2;
        box.y = -VWD0 / 2;
        box.w = HWD0;
        box.h = VWD0;
        box.r = 0;
        box.b = 0;
        box.g = 0;
        GsSortBoxFill(&box, ot, ot->Last());

        /* (zeroed, filled in and copied, as the original does) */
        RECT from;
        RECT rect;
        memset(&rect, 0, sizeof(rect));
        rect.x = BufferX(PSDIDX);
        rect.y = BufferY(PSDIDX);
        rect.w = HWD0;
        rect.h = VWD0;
        from = rect;
        SetDrawMove(&s_blurCopy, &from, BufferX(PSDIDX ^ 1), BufferY(PSDIDX ^ 1));
        addPrim(s_buffers[s_buffer].ot->LastTag(), &s_blurCopy);
    } else if (s_fade > 0) {
        /*
         * other blurs: the picture drawn as sprites at `s_fade` percent of its
         * brightness, always from frame buffer 0 (the texture pages leave out
         * BufferX()), which holds the picture on display only every other
         * frame
         */
        GsSPRITE sprite;
        sprite.attribute = 2 << 24; /* 15-bit direct colour */
        sprite.u = 0;
        sprite.v = 0;
        sprite.y = -VWD0 / 2;
        sprite.h = VWD0;
        sprite.g = sprite.b = sprite.r = (s_fade << 7) / 100;
        for (int x = 0; x < HWD0; x += 256) {
            sprite.x = -HWD0 / 2 + x;
            int w = HWD0 - x;
            if (w > 256) {
                w = 256;
            }
            sprite.w = w;
            sprite.tpage = (x >> 6) + (BufferY(PSDIDX) >> 8) * 16;
            OrderingTable* spriteOt = s_buffers[s_buffer].ot;
            GsSortFastSprite(&sprite, spriteOt, spriteOt->Last());
        }
    }

    if (wait && s_interval > 0 && GsGetActiveBuff() == 0) {
        VSync(s_interval == 1 ? 0 : s_interval);
    }

    if (s_doubleBuffer) {
        GsDISPENV.disp.x = PSDIDX ? s_disp.x : 0;
        GsDISPENV.disp.y = PSDIDX ? s_disp.y : 0;
        PutDispEnv(&GsDISPENV);
        GsIncFrame();
        GsDRAWENV.clip.x = PSDIDX ? s_disp.x : 0;
        GsDRAWENV.clip.y = PSDIDX ? s_disp.y : 0;
        GsDRAWENV.ofs[0] = (PSDIDX ? s_disp.x : 0) + HWD0 / 2;
        GsDRAWENV.ofs[1] = (PSDIDX ? s_disp.y : 0) + VWD0 / 2;
        GsOT_TAG* last = s_buffers[s_buffer].ot->LastTag();
        SetDrawArea(&s_drawArea, &GsDRAWENV.clip);
        addPrim(last, &s_drawArea);
        SetDrawOffset(&s_drawOffset, (u_short*)GsDRAWENV.ofs);
        addPrim(last, &s_drawOffset);
    }

    GsSortClear(0, 0, 0, s_buffers[s_buffer].ot);
    s_buffers[s_buffer].packets->End();
    GsDrawOt(s_buffers[s_buffer].ot);
    FlushFont();
    SwapBuffers();
    GsClearOt(0, 0, s_buffers[s_buffer].ot);
    s_buffers[s_buffer].packets->Begin();
    Print("\n\n");
}

void VideoSys::Print(const char* text)
{
    FntPrint(s_font, "%s", text);
}

void VideoSys::FlushFont()
{
    FntFlush(s_font);
}

GsOT* VideoSys::GetOT()
{
    return s_buffers[s_buffer].ot;
}

#ifdef GAME
void VideoSys::SetFade(int fade)
{
    s_fade = fade;
}
#endif

void VideoSys::AddVSyncCallback(Callback callback)
{
    /* never grows: that would move the array the interrupt is reading */
#line 574 "C:/psj/dev/locale/game/VideoSys.cpp"
    assert(s_vsyncCallbacks.size() < s_vsyncCallbacks.capacity());
#line 326 "src/game/VideoSys.cpp"
    SwEnterCriticalSection();
    s_vsyncCallbacks.push_back(callback);
    SwExitCriticalSection();
}

void VideoSys::RemoveVSyncCallback(Callback callback)
{
    SwEnterCriticalSection();
    s_vsyncCallbacks.erase(find(s_vsyncCallbacks.begin(), s_vsyncCallbacks.end(), callback));
    SwExitCriticalSection();
}

int VideoSys::s_videoMode;
int VideoSys::s_fade;
int VideoSys::s_doubleBuffer;
int VideoSys::s_interval;
int VideoSys::s_font;
VideoSys::Point VideoSys::s_disp;
int VideoSys::s_numBuffers;
VideoSys::Point VideoSys::s_origin;
int VideoSys::s_hz;
int VideoSys::s_fps;
VideoSys::Point VideoSys::s_size;
