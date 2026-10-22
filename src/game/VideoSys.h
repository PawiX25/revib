#ifndef VIDEOSYS_H
#define VIDEOSYS_H

#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#include "MemorySys.h"
#include <vector>

#include "Fixed.h"
#include "FontHack.h"

class OrderingTable;
class PacketArea;

/*
 * Display and drawing through libgs, with two frame buffers that are swapped
 * by hand, a blur, the debug font, and functions to call at every vertical
 * blank.
 */
class VideoSys {
public:
    typedef void (*Callback)();

    /*
     * `reset`: the display is already set up (started from the loader);
     * `interval`: vertical blanks per frame.
     */
    static void Init(int width, int reset, int interval, int doubleBuffer, int init3D);
    static void Quit();
    /* Blank the display, as when leaving for another executable. */
    static void Reset();
    /* Draw the frame and start the next one. */
    static void Flip(int wait);
    static void Print(const char* text);
    static GsOT* GetOT();

    /*
     * The blur of the game's "BaraBara" debug controller: the picture on
     * display is drawn under the next one at `fade` percent of its
     * brightness (0: no blur). Other than at 50, Flip() takes it from frame
     * buffer 0, which holds it only every other frame. Only the game has it.
     */
    static void SetFade(int fade);

    /* Functions called at every vertical blank. */
    static void AddVSyncCallback(Callback callback);
    static void RemoveVSyncCallback(Callback callback);

    struct Point {
        int x, y;
    };

    static Fixed FrameRate() { return s_frameRate; }
    static Fixed VsyncPeriod() { return s_vsyncPeriod; }
    static int Fps() { return s_fps; }
    static const Fixed& FramePeriod() { return s_framePeriod; }
    /* frames drawn ahead of the one on screen */
    static int NumBuffers() { return s_numBuffers; }
    /* the drawing coordinates of the screen's top left corner */
    static const Point& Origin() { return s_origin; }
    static const Point& Size() { return s_size; }
    static int Width() { return s_size.x; }
    static int Height() { return s_size.y; }

private:
    struct Buffer {
        PacketArea* packets;
        OrderingTable* ot;
    };

    static void SwapBuffers();
    static void OnDrawSync();
    static void OnVSync();
    static void FlushFont();
    static int BufferX(short index);
    static int BufferY(short index);

    static vector<Callback> s_vsyncCallbacks;
    static Buffer s_buffers[2];

    static Fixed s_vsyncRate;     /* vertical blanks per second */
    static Fixed s_vsyncPeriod;   /* seconds */
    static Fixed s_frameRate;
    static Fixed s_framePeriod;
    static int s_buffer;
    static FontHack s_fontHack;

    /* The compiler lays these out in reverse order of declaration (.sdata). */
    static int s_videoMode;     /* MODE_PAL */
    static int s_fade;          /* the blur: see SetFade() */
    static int s_doubleBuffer;
    static int s_interval;
    static int s_font;
    static Point s_disp;        /* position of the second frame buffer */
    static int s_numBuffers;
    static Point s_origin;      /* the drawing coordinates of the screen's top left corner */
    static int s_hz;
    static int s_fps;
    static Point s_size;
};

#endif
