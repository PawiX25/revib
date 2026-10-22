#ifndef INPUTSYS_H
#define INPUTSYS_H

#include "common.h"

#include "MemorySys.h"
#include <set>
#include <string>

/* Controller buttons, in the order the controller sends them. */
struct Buttons {
    char select : 1, l3 : 1, r3 : 1, start : 1, up : 1, right : 1, down : 1, left : 1;
    char l2 : 1, r2 : 1, l1 : 1, r1 : 1, triangle : 1, circle : 1, cross : 1, square : 1;

    u_short Bits() const { return *(u_short*)this; }
    /* the buttons as one word, in place */
    const u_short& Word() const { return *(const u_short*)this; }
    int operator&(int mask) const { return Bits() & mask; }
    void Set(u_short bits) { *(u_short*)this = bits; }
};

/* the bits of Buttons::Bits() */
#define BUTTON_SELECT 0x0001
#define BUTTON_L3 0x0002
#define BUTTON_R3 0x0004
#define BUTTON_START 0x0008
#define BUTTON_UP 0x0010
#define BUTTON_RIGHT 0x0020
#define BUTTON_DOWN 0x0040
#define BUTTON_LEFT 0x0080
#define BUTTON_L2 0x0100
#define BUTTON_R2 0x0200
#define BUTTON_L1 0x0400
#define BUTTON_R1 0x0800
#define BUTTON_TRIANGLE 0x1000
#define BUTTON_CIRCLE 0x2000
#define BUTTON_CROSS 0x4000
#define BUTTON_SQUARE 0x8000
#define BUTTONS_DPAD (BUTTON_UP | BUTTON_RIGHT | BUTTON_DOWN | BUTTON_LEFT)

/* The buttons held, and what changed since the previous frame. */
struct ButtonState {
    Buttons held;
    Buttons previous;
    Buttons changed;
    Buttons pressed;
    Buttons released;

    void UpdateChanges()
    {
        changed.Set(previous.Bits() ^ held.Bits());
        pressed.Set(held.Bits() & changed.Bits());
        released.Set(changed.Bits() & ~held.Bits());
    }
};

/* What the pad library receives from one controller. */
struct PadData {
    struct Info {
        u_char status; /* 0: valid */
        u_char type;
    } info;
    u_short buttons; /* 0: pressed */
    u_char analog[4];
};

/* What the pad library receives on one port: a controller, or a multitap with four. */
struct PadBuffer {
    union {
        PadData pad;
        struct {
            PadData::Info info;
            PadData taps[4];
        } multitap;
    };
};

/* A controller. */
class Pad {
public:
    Pad();
    Pad(int port);
    ~Pad();

    void Read(const PadData* data);
    /* the small motor of a DualShock: on or off */
    void SetVibration(bool on) { m_actuators[0] = on; }

    PadData::Info m_info;
    ButtonState m_buttons;
    ButtonState m_fourWay; /* the same, with the d-pad giving one direction at a time (none on a diagonal) */

private:
    void Update(u_short buttons);
    u_short FourWay(u_short buttons);

    int m_port;
    int m_alignActuators;
    int m_lastState;
    u_char m_actuators[2];
};

/* A port, with the controllers of a multitap. */
struct PadPort {
    Pad* pads[4];
    PadBuffer* buffer;

    PadPort(int port, PadBuffer* buf) : buffer(buf)
    {
        buffer->pad.info.status = 1;
        for (int i = 0; i < 4; i++) {
            pads[i] = new Pad(port * 16 + i);
        }
    }

    void Read()
    {
        if ((buffer->multitap.info.type & 0xF0) == 0x80) { /* a multitap */
            for (int i = 0; i < 4; i++) {
                if ((buffer->multitap.taps + i)->info.status == 0) {
                    pads[i]->Read(&buffer->multitap.taps[i]);
                }
            }
        } else {
            pads[0]->Read(&buffer->pad);
        }
    }
};

/*
 * The two controller ports. Parts of the game ask for a controller by name;
 * the names of the two players' controllers are set with SetPlayerName().
 */
class InputSys {
public:
    static void Init();
    static void Quit();
    static void Update();
    static Pad* GetPad(int player);
    static void SetPlayerName(string name, int player);
    /* the names GetPad() and IsPlayerName() have been asked for */
    static set<string>& Names();
    /* the player's controller if the name is one of theirs, else one with no buttons; the name joins Names() */
    static Pad* GetPad(string name);
    static bool IsPlayerName(string name);

private:
    static string s_playerNames[2];
    static set<string> s_names;
    static Pad s_nullPad;
    static PadBuffer s_buffers[2];
    static PadPort* s_ports[2];
};

#endif
