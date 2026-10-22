#include "common.h"

#include "ModelMovie.h"
#include "ObjectSwap.h"
#include "ScoreMeter.h"
#include "TmdLineObj.h"

/* the camera at (x, y, z), looking along z to depth `targetZ` (in unk_28A88.cpp) */
void LookAlongZ(Fixed x, Fixed y, Fixed z, Fixed targetZ, Fixed fov);

/* the shapes' objects in font/mark.tmd */
static const int s_shapes[ScoreMeter::NUM_SHAPES] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };

u_int ScoreMeter::s_rows[NUM_MARKS][16] = {
    { 1, 8, 36, 120, 330, 792, 1716, 3432, 6435, 11440, 19448, 31824, 50388, 77520, 116280, 170544 },
    { 1, 7, 28, 84, 210, 462, 924, 1716, 3003, 5005, 8008, 12376, 18564, 27132, 38760, 54264 },
    { 1, 6, 21, 56, 126, 252, 462, 792, 1287, 2002, 3003, 4368, 6188, 8568, 11628, 15504 },
    { 1, 5, 15, 35, 70, 126, 210, 330, 495, 715, 1001, 1365, 1820, 2380, 3060, 3876 },
    { 1, 4, 10, 20, 35, 56, 84, 120, 165, 220, 286, 364, 455, 560, 680, 816 },
    { 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 66, 78, 91, 105, 120, 136 },
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 },
};
int ScoreMeter::s_marks[NUM_MARKS];
Fixed ScoreMeter::s_position;
ObjectSwap* ScoreMeter::s_swap;
TmdModel* ScoreMeter::s_model;
ModelMovie* ScoreMeter::s_movie;

/* the shapes of the row of marks for a score, a nibble each */
u_int ScoreMeter::ToMarks(u_int score)
{
    u_int marks = 0;
    for (int i = 0; i < NUM_MARKS; i++) {
        marks <<= 4;
        int shape;
        for (shape = 0; shape < 16; shape++) {
            if (score < s_rows[i][shape]) {
                break;
            }
        }
        if (shape != 0) {
            score -= s_rows[i][shape - 1];
        }
        marks |= shape;
    }
    return marks;
}

static ModelMovie* NewMovie(TmdModel* model, MemPtr<short> file)
{
    return new ModelMovie(model, file, 0);
}

void ScoreMeter::SetMarks(int score)
{
    u_int marks = ToMarks(score);
    u_int mask = 15;
    for (int i = 0; i < NUM_MARKS; i++) {
        s_marks[NUM_MARKS - 1 - i] = (marks & mask) >> (4 * i);
        mask <<= 4;
    }
}

void ScoreMeter::Init()
{
    s_position = Fixed();
    for (int i = 0; i < NUM_MARKS; i++) {
        s_marks[i] = 0;
    }
    s_model = LoadTmdModel("font/mark.tmd");
    MemPtr<short> file("meters/score.anm");
    s_movie = NewMovie(s_model, file);
    s_movie->SetLoop(1);
    s_swap = new ObjectSwap(s_model, s_shapes);
}

/*
 * The marks go round, `speed` frames of the movie a frame. (The statics
 * copied into locals, again in the loop, and `i` declared before `offset`
 * give the game's code.)
 */
void ScoreMeter::Draw(int score, Fixed speed)
{
    LookAlongZ(0.0, 0.0, -181.625, 0.0, 1.625);
    if (score > MAX_SCORE) {
        score = MAX_SCORE;
    }
    if (score < 0) {
        score = 0;
    }
    SetMarks(score);

    /* the first mark moves the movie on */
    int shape = s_marks[0];
    ObjectSwap* swap = s_swap;
    ModelMovie* movie = s_movie;
    TmdModel* model = s_model;
    swap->Show(shape);
    model->SetJitter(4.0, 1);
    movie->Seek(s_position);
    movie->SetSpeed(-speed);
    movie->Draw();
    s_position = movie->m_position;

    /* the others follow it, `spacing` frames apart */
    int frames = s_movie->m_numFrames;
    int spacing = 20;
    int i = 1;
    Fixed offset = Fixed::Int(spacing);
    for (; i < NUM_MARKS; i++) {
        int shape = s_marks[i];
        ObjectSwap* swap = s_swap;
        ModelMovie* movie = s_movie;
        TmdModel* model = s_model;
        swap->Show(shape);
        model->SetJitter(4.0, 1);
        movie->Seek(Fixed::FromRaw((s_position + offset).Raw() % (frames << 16)));
        movie->Draw();
        offset += Fixed::Int(spacing);
    }
}

void ScoreMeter::Free()
{
    delete s_swap;
    delete s_movie;
    delete s_model;
}
