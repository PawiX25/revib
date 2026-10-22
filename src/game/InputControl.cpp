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
        VideoSys::Print((string("ESCAPE\n") + string("  CONTROL = ") + *m_current + string("\n")).c_str());
        break;
    case STATE_CHOSEN:
        InputSys::SetPlayerName(*m_current, 0);
        m_state = STATE_IDLE;
        break;
    case STATE_IDLE:
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
#line 60 "src/game/InputControl.cpp"
}

void InputControl::Previous()
{
    if (m_current == m_names.begin()) {
        m_current = m_names.end();
    }
    --m_current;
#line 96 "C:/psj/dev/locale/game/InputControl.cpp"
    assert(m_current != m_names.end());
#line 71 "src/game/InputControl.cpp"
}
