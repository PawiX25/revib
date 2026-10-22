#ifndef SCRIPTEVENT_H
#define SCRIPTEVENT_H

#include "common.h"

/* Something that happens during a track, at a time of its script. */
struct ScriptEvent {
    enum Type {
        PART,   /* a road part: `value` is its road */
        CAMERA, /* `value`: loops * 10 + the camera's id */
        EFFECT  /* `value`: an Effect */
    };
    /* the effects */
    enum Effect { SCREW, REVERSE, TEMPO };

    Type type;
    int time;   /* in samples */
    int value;
    int length; /* in samples */
    int scale;  /* of the length of a road part, 16.16 */
};

#endif
