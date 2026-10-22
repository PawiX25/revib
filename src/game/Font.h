#ifndef FONT_H
#define FONT_H

#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

class TmdModel;
template <class T> class NumberParameter;

/* A number in the GTE's units: 4096 is 1. */
class GteFixed {
public:
    GteFixed() : m_value(0) {}
    GteFixed(double value) : m_value((int)(value * 4096.0)) {}

    /* a whole number */
    static GteFixed Int(int n)
    {
        GteFixed value;
        value.m_value = n << 12;
        return value;
    }

    static GteFixed FromRaw(int raw)
    {
        GteFixed value;
        value.m_value = raw;
        return value;
    }

    int Raw() const { return m_value; }
    GteFixed operator+(const GteFixed& other) const { return FromRaw(m_value + other.m_value); }
    /* the whole part */
    int Integer() const { return m_value >> 12; }

private:
    int m_value;
};

/*
 * The game's text, drawn with the line models of font/Font.tmd: one
 * object per character, jittered like everything else.
 */
class Font {
public:
    static void Init();
    static void Free();

    /* where the next text starts, and how big it is */
    static void SetPosition(GteFixed x, GteFixed y);
    static void SetScale(GteFixed scale);

    /*
     * Draws `text`, its characters jittered by `jitter` (4096 is 1). A
     * '~' gives the next character its accented or other second form,
     * and '\n' starts a new line.
     */
    static void Draw(const char* text, int jitter);

private:
    static GteFixed s_scale;
    static GteFixed s_x;
    static GteFixed s_y;
    /* debug settings no menu has: the passes of each character, and how much more they jitter */
    static NumberParameter<int> s_passes;
    static NumberParameter<float> s_extraJitter;
    static GsCOORDINATE2 s_coord;
    static TmdModel* s_model;
};

#endif
