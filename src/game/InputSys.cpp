#include "common.h"

#include <libetc.h>
#include <libpad.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "InputSys.h"

/* actuator 0: small motor, on/off; actuator 1: large motor, speed */
static u_char s_actuatorAlign[6] = { 0, 1, 0xFF, 0xFF, 0xFF, 0xFF };

Pad::Pad()
{
    m_info.status = 1;
    m_buttons.held.Set(0);
    Update(0);
}

Pad::Pad(int port)
{
    m_port = port;
    m_alignActuators = 1;
    m_lastState = -1;
    m_info.status = 1;
    m_buttons.held.Set(0);
    Update(0);
    for (int i = 0; i < 2; i++) {
        m_actuators[i] = 0;
    }
    if ((m_port & 3) == 0) {
        PadSetAct(m_port, m_actuators, 2);
    }
}

Pad::~Pad()
{
    if ((m_port & 3) == 0) {
        PadSetAct(m_port, NULL, 0);
    }
}

void Pad::Read(const PadData* data)
{
    m_info = data->info;
    if (m_info.status == 0) {
        Update(~data->buttons);
    } else {
        Update(0);
    }

    int state = PadGetState(m_port);
    if (state != m_lastState) {
        switch (state) {
        case PadStateFindPad:
            m_alignActuators = 1;
            break;
        case PadStateStable:
            if (m_alignActuators) {
                m_alignActuators = 0;
                PadSetActAlign(m_port, s_actuatorAlign);
            }
            break;
        case PadStateDiscon:
            Update(0);
            break;
        }
    }
    m_lastState = state;
}

void Pad::Update(u_short buttons)
{
    m_buttons.previous = m_buttons.held;
    m_buttons.held.Set(buttons);
    /* opposite directions at once */
    if ((m_buttons.held.up && m_buttons.held.down) || (m_buttons.held.right && m_buttons.held.left)) {
        m_buttons.held.Set(buttons & ~BUTTONS_DPAD);
    }
    m_buttons.UpdateChanges();

    m_fourWay.previous = m_fourWay.held;
    m_fourWay.held.Set(FourWay(m_buttons.held.Bits()));
    m_fourWay.UpdateChanges();
}

u_short Pad::FourWay(u_short buttons)
{
    bool horizontal = (buttons & (BUTTON_RIGHT | BUTTON_LEFT)) != 0;
    bool vertical = (buttons & (BUTTON_UP | BUTTON_DOWN)) != 0;
    if (horizontal) {
        buttons &= ~(BUTTON_UP | BUTTON_DOWN);
    }
    if (vertical) {
        buttons &= ~(BUTTON_RIGHT | BUTTON_LEFT);
    }
    return buttons;
}

void InputSys::Init()
{
    PadInitDirect((u_char*)&s_buffers[0], (u_char*)&s_buffers[1]);
    for (int port = 0; port < 2; port++) {
        s_ports[port] = new PadPort(port, &s_buffers[port]);
    }
    PadStartCom();
    for (int i = 0; i < 5; i++) {
        VSync(0);
        for (int port = 0; port < 2; port++) {
            s_ports[port]->Read();
        }
    }
}

void InputSys::Quit()
{
    PadStopCom();
    for (int port = 0; port < 2; port++) {
        PadPort* p = s_ports[port];
        if (p != NULL) {
            for (int i = 0; i < 4; i++) {
                delete p->pads[i];
            }
            delete p;
        }
    }
}

void InputSys::Update()
{
    for (int port = 0; port < 2; port++) {
        s_ports[port]->Read();
    }
}

Pad* InputSys::GetPad(int player)
{
#line 253 "C:/psj/dev/locale/game/InputSys.cpp"
    assert(player < 2);
#line 142 "src/game/InputSys.cpp"
    return s_ports[player]->pads[0];
}

void InputSys::SetPlayerName(string name, int player)
{
#line 259 "C:/psj/dev/locale/game/InputSys.cpp"
    assert(player < 2);
#line 150 "src/game/InputSys.cpp"
    s_playerNames[player] = name;
}

set<string>& InputSys::Names()
{
    return s_names;
}

Pad* InputSys::GetPad(string name)
{
    s_names.insert(name);
    for (int player = 0; player < 2; player++) {
        if (s_playerNames[player] == name) {
            return GetPad(player);
        }
    }
    return &s_nullPad;
}

#ifdef GAME
/* `name` is one of the players' controllers; like GetPad(), it adds `name` to Names(). */
bool InputSys::IsPlayerName(string name)
{
    s_names.insert(name);
    for (int player = 0; player < 2; player++) {
        if (s_playerNames[player] == name) {
            return true;
        }
    }
    return false;
}
#endif

string InputSys::s_playerNames[2];
set<string> InputSys::s_names;
Pad InputSys::s_nullPad;
PadBuffer InputSys::s_buffers[2];

PadPort* InputSys::s_ports[2];
