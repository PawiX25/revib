#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "Format.h"
#include "Font.h"
#include "LanguageSelect.h"
#include "Parameter.h"
#include "TmdLineObj.h"

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Font", D_8001D1A0);

/* distance between two characters at scale 1, and between two lines */
#define CHARACTER_WIDTH 90
#define LINE_HEIGHT 230
/* how far away text is drawn */
#define TEXT_DEPTH 2690

/* the font's objects of the Japanese characters, from 0x90 on */
static u_char s_japaneseObjects[80] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  10, 11, 12,
    0,  14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
    13, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
    44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75,
};

/* the font's objects of the other characters, from ' ' on (the second forms from 0x80 on) */
static u_char s_objects[148] = {
    0,   63,  0,   0,   0,   0,   64,  67,  69,  70,  0,   0,   66,  68,  65,  0,
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   0,   0,   0,   0,   0,   62,
    0,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
    25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  0,   0,   0,   0,   0,
    0,   36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,
    51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  0,   0,   0,   0,   0,
    0,   71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,
    86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  0,   0,   0,   0,   0,
    0,   97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    102, 0,   0,   0,
};

GteFixed Font::s_scale;
GteFixed Font::s_x;
GteFixed Font::s_y;
NumberParameter<int> Font::s_passes("param1", 1, 2, 0, 10);
NumberParameter<float> Font::s_extraJitter("param2", 4.5f, 0.5f);
GsCOORDINATE2 Font::s_coord;
TmdModel* Font::s_model;

static inline void SetLook(TmdLineObj* object, Fixed jitter, int passes)
{
    object->SetJitter(jitter);
    object->SetPasses(passes);
}

void Font::Init()
{
    s_scale = GteFixed::FromRaw(ONE);
    s_x = GteFixed();
    s_y = GteFixed();
    s_model = LoadTmdModel("font/Font.tmd");
    GsInitCoordinate2(WORLD, &s_coord);
}

void Font::Free()
{
    delete s_model;
}

void Font::SetPosition(GteFixed x, GteFixed y)
{
    s_x = x;
    s_y = y;
}

void Font::SetScale(GteFixed scale)
{
    s_scale = scale;
}

void Font::Draw(const char* text, int jitter)
{
    int scale = s_scale.Raw();
    SVECTOR rotation;
    rotation.vx = rotation.vy = rotation.vz = 0;
    RotMatrix(&rotation, &s_coord.coord);
    VECTOR scaling;
    scaling.vx = scaling.vy = scaling.vz = scale;
    ScaleMatrix(&s_coord.coord, &scaling);
    s_coord.flg = 0;

    int column = 0;
    int marks = 0; /* of the line, drawn over the character before */
    int second = 0;
    int width = (CHARACTER_WIDTH * scale) >> 12;
    int row = 0;
    while (*text != 0) {
        char first = LanguageSelect::GetLanguage() == LANGUAGE_JAPANESE ? '\x90' : ' ';
        u_char glyph = *text - first;
        if (LanguageSelect::GetLanguage() == LANGUAGE_JAPANESE) {
            /* the voicing marks */
            if (*text == '\xDE' || *text == '\xDF') {
                marks++;
            }
            if (*text >= '0' && *text <= '?') {
                glyph = *text - '0';
            }
            if (*text == '!') {
                glyph = 13;
            }
            if (*text == '&') {
                glyph = 14;
            }
        }
        if (second) {
            second = 0;
            glyph = *text + 32;
        }

        if (*text == '~') {
            second = 1;
        } else if (*text == ' ') {
            column++;
        } else if (*text == '\n') {
            row++;
            column = 0;
            marks = 0;
        } else {
            VECTOR position;
            position.vx = s_x.Integer() + width * (column - marks);
            position.vy = s_y.Integer() + row * LINE_HEIGHT;
            position.vz = TEXT_DEPTH;
            TransMatrix(&s_coord.coord, &position);
            MATRIX local;
            GsGetLs(&s_coord, &local);
            GsSetLsMatrix(&local);
            u_char* objects = LanguageSelect::GetLanguage() == LANGUAGE_JAPANESE ? s_japaneseObjects : s_objects;
            TmdLineObj* object = s_model->m_objects[objects[glyph]];
            column++;
            SetLook(object, Fixed::FromRaw((jitter + ((int)s_extraJitter.m_value << 12)) << 4), s_passes.m_value);
            object->Draw(0, NULL);
        }
        text++;
    }
}
