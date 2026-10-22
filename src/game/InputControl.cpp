#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Format.h"
#include "InputControl.h"
#include "VideoSys.h"

/* The name is given to the first player at once. */
InputControl::InputControl(const char* name)
    : m_quit(0), m_state(STATE_IDLE), m_names(InputSys::Names()), m_current(m_names.begin())
{
    InputSys::SetPlayerName(name, 0);
}

InputControl::~InputControl()
{
    InputSys::SetPlayerName("", 0);
}

void InputControl::Update()
{
    InputSys::Update();
    Pad* pad = InputSys::GetPad(0);
    Buttons pressed = pad->m_fourWay.pressed;
    Buttons held = pad->m_fourWay.held; /* not used (the original copies it too) */
    switch (m_state) {
    case STATE_CHOOSING:
        if (pressed.select) {
            m_state = STATE_CHOSEN;
        }
        if (pressed.up) {
            Previous();
        }
        if (pressed.down) {
            Next();
        }
#ifdef DEBUG_MENU
        VideoSys::Print("ESCAPE\n");
        VideoSys::Print("  CONTROL = ");
        VideoSys::Print(m_current->c_str());
        VideoSys::Print("\n");
#else
        VideoSys::Print((string("ESCAPE\n") + string("  CONTROL = ") + *m_current + string("\n")).c_str());
#endif
        break;
    case STATE_CHOSEN:
        InputSys::SetPlayerName(*m_current, 0);
        m_state = STATE_IDLE;
        break;
    case STATE_IDLE:
#ifdef DEBUG_MENU
        /* not in the game, where nothing opens the choice: select does, at the name in use */
        if (pressed.select && !m_names.empty()) {
            m_current = m_names.begin();
            for (set<string>::iterator i = m_names.begin(); i != m_names.end(); ++i) {
                if (*i == InputSys::PlayerName(0)) {
                    m_current = i;
                }
            }
            m_state = STATE_CHOOSING;
        }
#endif
        break;
    }
}

/* the name after the current one, back to the first after the last */
void InputControl::Next()
{
    if (++m_current == m_names.end()) {
        m_current = m_names.begin();
    }
#line 87 "C:/psj/dev/locale/game/InputControl.cpp"
    assert(m_current != m_names.end());
#line 79 "src/game/InputControl.cpp"
}

void InputControl::Previous()
{
    if (m_current == m_names.begin()) {
        m_current = m_names.end();
    }
    --m_current;
#line 96 "C:/psj/dev/locale/game/InputControl.cpp"
    assert(m_current != m_names.end());
#line 90 "src/game/InputControl.cpp"
}

#ifdef DEBUG_MENU
/*
 * The code of this file is shorter than the game's with DEBUG_MENU: the
 * rest is filled, so that nothing after it moves. The game does not run if
 * its code and data move (its start-up code clears memory and places the
 * heap at fixed addresses).
 */
__asm__(".text\n\t.space 0x6f8\n");
#endif
