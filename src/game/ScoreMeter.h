#ifndef SCOREMETER_H
#define SCOREMETER_H

#include "common.h"

#include "Fixed.h"

struct ObjectSwap;
class TmdModel;
class ModelMovie;

/*
 * The score, shown as a row of NUM_MARKS marks, each one of NUM_SHAPES
 * shapes, no mark's shape above the one before it: the score is the rank of
 * that row among all such rows (combinations with repetition).
 */
class ScoreMeter {
public:
    enum {
        NUM_MARKS = 7,
        NUM_SHAPES = 15,
        MAX_SCORE = 116279 /* the number of rows, less one */
    };

    static void Init();
    static void Draw(int score, Fixed speed);
    static void Free();

    /* the shape of mark `i` of the last score drawn */
    static int GetMark(int i) { return s_marks[i]; }

private:
    static u_int ToMarks(u_int score);
    static void SetMarks(int score);

    /* s_rows[i][n]: how many rows the marks from i on make with shapes up to n */
    static u_int s_rows[NUM_MARKS][16];
    static int s_marks[NUM_MARKS];
    static Fixed s_position;
    static ObjectSwap* s_swap;
    static TmdModel* s_model;
    static ModelMovie* s_movie;
};

#endif
