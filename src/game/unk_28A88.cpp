#include "common.h"

/* the "Info: status" and "Analog: Left" strings of a shared header */
#ifndef KIOSK
INCLUDE_RODATA("asm/game/nonmatchings/unk_28A88", D_80019F70);
#else
INCLUDE_RODATA("asm/main_k/nonmatchings/unk_28A88", D_80019F30);
#endif

#include <stdio.h>
#include <stdlib.h>

#include "Format.h"
#include "Menu.h"
#include "LanguageSelect.h"
#include "VideoSys.h"

Fixed KeyRepeat::s_delay;
Fixed KeyRepeat::s_rate;
Timer KeyRepeat::s_timer;
int KeyRepeat::s_repeating;

/* The kiosk has no menus: of this file it keeps only KeyRepeat's statics, ReadConfirm() and PerFrames(). */
#ifndef KIOSK
/* Moves on in a direction until another item is reached; back one step if that took it round an edge. */
void SkipItem(int from, MenuCursor* cursor, int dx, int dy, int wrapHorizontal, int wrapVertical)
{
    for (;;) {
        cursor->Move(dx, dy, 1, 1);
        if (cursor->Item() != cursor->grid->cells[from]) {
            if (cursor->x == cursor->grid->columns - 1 && dx == -1) {
                cursor->Move(1, dy, 1, 1);
            } else if (cursor->x == 0 && dx == 1) {
                cursor->Move(-1, dy, 1, 1);
            } else if (cursor->y == cursor->grid->rows - 1 && dy == -1) {
                cursor->Move(dx, 1, 1, 1);
            } else if (cursor->y == 0 && dy == 1) {
                cursor->Move(dx, -1, 1, 1);
            }
            return;
        }
    }
}

void MenuCursor::Move(int dx, int dy, int wrapVertical, int wrapHorizontal)
{
    x += dx;
    y += dy;
    if (wrapHorizontal) {
        if (x < 0) {
            x = grid->columns - 1;
        }
        if (x >= grid->columns) {
            x = 0;
        }
    } else {
        if (x < 0) {
            x = 0;
        }
        if (x >= grid->columns) {
            x = grid->columns - 1;
        }
    }
    if (wrapVertical) {
        if (y < 0) {
            y = grid->rows - 1;
        }
        if (y >= grid->rows) {
            y = 0;
        }
    } else {
        if (y < 0) {
            y = 0;
        }
        if (y >= grid->rows) {
            y = grid->rows - 1;
        }
    }
}

void KeyRepeat::Set(Fixed delay, Fixed rate)
{
    s_delay = delay;
    s_rate = rate;
    s_repeating = 0;
}

Buttons KeyRepeat::Apply(Buttons pressed, Buttons held)
{
    const u_short& bits = pressed.Word();
    switch (s_repeating) {
    case 0:
        if (bits & BUTTONS_DPAD) {
            s_timer = Timer(s_delay);
            s_repeating = 1;
        }
        break;
    case 1:
        if (bits & BUTTONS_DPAD) {
            s_timer = Timer(s_delay);
        }
        if (held.Bits() & BUTTONS_DPAD) {
            if (s_timer.Expired()) {
                pressed.Set(pressed.Bits() | held.Bits());
                s_timer = Timer(s_rate);
            }
        } else {
            s_repeating = 0;
        }
        break;
    }
    return pressed;
}

void KeyRepeat::Init()
{
    Set(0.66, 0.33);
}

int MenuCursor::Read(MenuInput* input, int wrapVertical, int wrapHorizontal, int active)
{
    input->confirm = 0;
    input->back = 0;
    if (!active) {
        input->down = 0;
        input->right = 0;
        input->up = 0;
        input->left = 0;
        /* no return here in the game either: what comes back is undefined (no caller looks) */
    } else {
        Pad* pad = InputSys::GetPad("Play");
        Buttons pressed = pad->m_fourWay.pressed;
        Buttons held = pad->m_fourWay.held;
        pressed = KeyRepeat::Apply(pressed, held);
        int dx = 0;
        int dy = 0;
        int from = 1;
        if (pressed & LanguageSelect::ConfirmButton()) {
            input->confirm = 1;
        } else if (pressed & LanguageSelect::CancelButton()) {
            input->back = Item() != MENU_BACK;
            for (int row = 0; row < grid->rows; row++) {
                for (int column = 0; column < grid->columns; column++) {
                    if (grid->cells[row * grid->columns + column] == MENU_BACK) {
                        x = column;
                        y = row;
                        goto found;
                    }
                }
            }
        }
    found:
        input->up = pressed.up != 0;
        input->down = pressed.down != 0;
        input->right = pressed.right != 0;
        input->left = pressed.left != 0;
        if (pressed.up | pressed.down | pressed.right | pressed.left) {
            from = Index();
        }
        if (pressed.up) {
            dy = -1;
        }
        if (pressed.down) {
            dy = 1;
        }
        if (pressed.right) {
            dx = 1;
        }
        if (pressed.left) {
            dx = -1;
        }
        Move(dx, dy, wrapVertical, wrapHorizontal);
        int index = Index();
        if (grid->cells[index] == grid->cells[from] && index != from) {
            SkipItem(from, this, dx, dy, wrapHorizontal, wrapVertical);
        }
        if (Item() != grid->cells[from]) {
            input->changed = 1;
        } else {
            input->changed = 0;
        }
        if (Item() == MENU_WRAP_START) {
            if (dx == 1) {
                x = 0;
            }
            if (dx == -1) {
                x = grid->columns - 2;
            }
        }
        if (Item() != MENU_WRAP_END) {
            return 1;
        }
        if (dx == 1) {
            x = grid->columns - 2;
        }
        if (dx == -1) {
            x = 0;
        }
        /* nor here: past the end of a row, what comes back is undefined too */
    }
}
#endif

int ReadConfirm()
{
    Pad* pad = InputSys::GetPad("Play");
    Buttons pressed = pad->m_fourWay.pressed;
    if (!(pressed & LanguageSelect::ConfirmButton())) {
        return 2 * ((pressed & LanguageSelect::CancelButton()) != 0);
    }
    return 1;
}

/* The game executable has no PerFrames(): it has LookAlongZ() in its place. */
#ifndef GAME
int PerFrames(int perSecond, int frames)
{
    return frames * perSecond / VideoSys::Fps();
}
#else
/* (in unk_22EE8.cpp) */
void SetProjection(Fixed fov);

/* The camera at (x, y, z), looking straight along z to depth `targetZ`. */
void LookAlongZ(Fixed x, Fixed y, Fixed z, Fixed targetZ, Fixed fov)
{
    GsRVIEW2 view;
    view.vpx = view.vrx = x.Raw() >> 12;
    view.vpy = view.vry = y.Raw() >> 12;
    view.vpz = z.Raw() >> 12;
    view.vrz = targetZ.Raw() >> 12;
    view.rz = 0;
    view.super = WORLD;
    SetProjection(fov);
    GsSetRefView2(&view);
}
#endif
