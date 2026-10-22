#ifndef SCENEDRAW_H
#define SCENEDRAW_H

#include "common.h"

#include "MemorySys.h"
#include <deque>

#include "Coord.h"
#include "Fixed.h"
#include "Gameplay.h"
#include "ModelMovie.h"
#include "TmdLineObj.h"

class CGameCamera;

/*
 * The scene of a course: the road, its pieces and the character running
 * along it (a model and a movie for each of its moves), and the "evo" meter
 * over it. A controller named "BaraBara" tunes a few parameters of the look.
 */
class SceneDraw {
public:
    enum {
        NUM_CHARACTERS = 4, /* Vibri in each of her forms (chara/Super, Peeloo, Frog, Snake) */
        NUM_MOVES = 11,
        MOVE_NONE = 10      /* running on */
    };

    /* the road `length` long, from `start` */
    static void Init(Fixed length, Fixed start);
    static void Quit();
    /* `speedCourse` is kept but never read */
    static void Reset(int speedCourse);
    /* `alternate`: the move's other version, if it has one */
    static void SetMove(int move, int alternate);
    /* the move has gone `fraction` of the way (or there is none) */
    static int IsMoveDone(Fixed fraction);
    static void Update();
    /*
     * `still`: nothing jittered but the missed parts' pieces; `shake`: from 1
     * down to 0 after a miss, Vibri jittering more; `character`: her form;
     * `stage`: her level in it (0 to 2), the scene jittering more the higher
     * it is.
     */
    static void Draw(Fixed speed, deque<RoadPiece>* road, int still, Fixed shake, int character,
                     int stage, CGameCamera* camera);
    /* the evo meter at `frame`, stopped (after its movie, if that is playing) */
    static void ShowEvo(int frame);
    static void PlayEvo();

private:
    /* the parameters the "BaraBara" controller tunes */
    enum {
        PARAM_SUBLINES,  /* passes the character is drawn in */
        PARAM_DEVIATION,
        PARAM_BB_MODE,
        PARAM_BLUR_ON,
        PARAM_BLUR,      /* the fade of the screen */
        PARAM_MARKERS_ON,
        NUM_PARAMS
    };

    /* The files of a move for each character: two versions, "" for none (the first one again). */
    struct MoveFiles {
        int move;
        const char* files[NUM_CHARACTERS][2];
    };

    static void PrintParam(const char* name, int param);
    static void PrintParams();
    static void UpdateParams();

    /* the local screen matrix of what follows `coord` */
    static void SetLsMatrix(Coord* coord)
    {
        MATRIX ls;
        GsGetLs(Coord::LibgsParent(coord), &ls);
        GsSetLsMatrix(&ls);
    }
    static ModelMovie* CurrentMove() { return s_moves[s_character][s_move][s_alternate]; }

    static void DrawMark(int mark, int zOffset);
    /* how far a piece at `position` leans */
    static Fixed Lean(Fixed position, int side);
    /* `missed`: a missed part's piece, jittering hard */
    static void DrawObject(TmdLineObj* object, Fixed position, int missed, SVECTOR* join, int angle);
    static void DrawCharacter(Fixed position, Fixed speed, Fixed shake);

    static int s_params[NUM_PARAMS];
    static Coord s_cameraCoord;
    static Coord s_pieceCoord;
    static Coord s_characterCoord;
    static MoveFiles s_moveFiles[NUM_MOVES];
    static const char* s_characterDirs[NUM_CHARACTERS];
    static const int s_evoEnds[NUM_CHARACTERS];

    static int s_selected;
    static int s_held;
    static int s_repeats;
    static int s_step;
    static const char* s_roadFile;
    static const char* s_markFile;
    static const char* s_pointFile;
    static const char* s_modelFile;
    static Fixed s_offset;
    static Fixed s_speedJitter;
    static Fixed s_jitter;
    static Fixed s_position;
    static Fixed s_length;
    static Fixed s_start;

    /* The compiler lays these out in reverse order of declaration. */
    static int s_speedCourse; /* never read */
    static int s_evoFrame;    /* the frame the evo meter shows when it is not playing */
    static ModelMovie* s_evo;
    static TmdModel* s_evoModel;
    static TmdLineObj* s_point;
    static TmdModel* s_marks;
    static TmdModel* s_road;
    static int s_lastCharacter;
    static int s_alternate;
    static int s_move;
    static int s_character;
    static ModelMovie* s_moves[NUM_CHARACTERS][NUM_MOVES][2];
    static TmdModel* s_models[NUM_CHARACTERS];
};

#endif
