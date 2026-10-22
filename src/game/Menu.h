#ifndef MENU_H
#define MENU_H

#include "common.h"

#include "Fixed.h"
#include "InputSys.h"
#include "VideoSys.h"

/* the vertical blanks counted by libetc since start-up (what VSync(-1) returns) */
extern "C" volatile long Vcount;

/* Expires once `length` seconds have gone by since it was made; one made without a length never does. */
struct Timer {
    long start;  /* Vcount */
    int running;
    Fixed length;

    Timer() : running(0) { start = Vcount; }
    Timer(Fixed time) : start(Vcount), running(1), length(time) {}

    int Expired();
};

inline int Timer::Expired()
{
    return running && Fixed::Int(Vcount - start) * VideoSys::VsyncPeriod() >= length;
}

/* Turns a d-pad direction held down into presses at a rate. */
class KeyRepeat {
public:
    static void Init();
    /* after `delay` held, then every `rate` (in seconds) */
    static void Set(Fixed delay, Fixed rate);
    /* `pressed`, plus every button in `held` whenever a held direction repeats */
    static Buttons Apply(Buttons pressed, Buttons held);

private:
    static Fixed s_delay;
    static Fixed s_rate;
    static Timer s_timer;
    static int s_repeating;
};

/* The items of a menu, in rows; the same item may fill several cells. */
struct MenuGrid {
    int columns;
    int rows;
    const int* cells;

    MenuGrid(int c, int r, const int* items) : columns(c), rows(r), cells(items) {}
    ~MenuGrid() {}

    int Item(int x, int y) { return cells[y * columns + x]; }
};

/* what can be in a cell besides an item (no menu uses either wrap) */
#define MENU_WRAP_START -1 /* moving right onto it goes to the first column, left onto it to the last but one */
#define MENU_WRAP_END -2   /* moving right onto it goes to the last column but one, left onto it to the first */
#define MENU_BACK -99      /* the item cancelling the menu */

/* What happened in a menu this frame. */
struct MenuInput {
    int confirm;
    int up;
    int down;
    int right;
    int left;
    int back;    /* cancel was pressed off the back item */
    int changed; /* the cursor went to another item */
};

/* A position in a menu. */
struct MenuCursor {
    int x;
    int y;
    MenuGrid* grid;

    MenuCursor(MenuGrid* items = NULL, int column = 0, int row = 0) : x(column), y(row), grid(items) {}

    int Index() { return y * grid->columns + x; }
    int Item() { return grid->cells[Index()]; }

    /* to the first cell of the item */
    void GoTo(int item)
    {
        for (int row = 0; row < grid->rows; row++) {
            for (int column = 0; column < grid->columns; column++) {
                if (grid->cells[row * grid->columns + column] == item) {
                    x = column;
                    y = row;
                    return;
                }
            }
        }
    }

    /* `wrapVertical`, `wrapHorizontal`: go round at the edges rather than stop */
    void Move(int dx, int dy, int wrapVertical, int wrapHorizontal);
    /* Reads the "Play" pad into `input` and moves; returns 1, or nothing on some paths (no caller looks). */
    int Read(MenuInput* input, int wrapVertical, int wrapHorizontal, int active);
};

/* 1: confirm was pressed, 2: cancel, 0: neither */
int ReadConfirm();

/* how much of `perSecond` the given frames make */
int PerFrames(int perSecond, int frames);

#endif
