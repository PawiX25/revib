#ifndef INPUTCONTROL_H
#define INPUTCONTROL_H

#include "common.h"

#include "InputSys.h"

/*
 * Picks which controller (by the names InputSys knows) the first player
 * uses: up and down go through the names, select takes the one shown.
 * Nothing switches it to STATE_CHOOSING, so all it does is name the first
 * player's controller and update InputSys every frame.
 */
class InputControl {
public:
    enum State {
        STATE_CHOOSING,
        STATE_CHOSEN,
        STATE_IDLE
    };

    InputControl(const char* name);
    ~InputControl();

    void Update();

    int m_quit; /* never set; the title, kiosk and game loops stop when it is */

private:
    void Next();
    void Previous();

    State m_state;
    set<string>& m_names;
    set<string>::iterator m_current;
};

#endif
