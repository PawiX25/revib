#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#include "MemorySys.h"
#include "InputSys.h"
#include "VideoSys.h"
#include "GameCamera.h"
#include "SceneDraw.h"

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/SceneDraw", D_8001D9FC);

int SceneDraw::s_params[NUM_PARAMS] = { 1, 15, 0, 0, 50, 0 };
Coord SceneDraw::s_cameraCoord(ONE);
Coord SceneDraw::s_pieceCoord(ONE);
Coord SceneDraw::s_characterCoord(ONE);

int SceneDraw::s_selected = 0;

void SceneDraw::PrintParam(const char* name, int param)
{
    static char line[48];
    char mark = ' ';
    if (s_selected == param) {
        mark = '>';
    }
    sprintf(line, "%11s:%c %d\n", name, mark, s_params[param]);
    VideoSys::Print(line);
}

void SceneDraw::PrintParams()
{
    PrintParam("sublines", PARAM_SUBLINES);
    PrintParam("deviation", PARAM_DEVIATION);
    PrintParam("bb mode", PARAM_BB_MODE);
    PrintParam("blur on", PARAM_BLUR_ON);
    PrintParam("blur", PARAM_BLUR);
    PrintParam("markers on", PARAM_MARKERS_ON);
}

/* how long the buttons have been held, in frames and in tens of frames */
int SceneDraw::s_held = 0;
int SceneDraw::s_repeats = 0;
/* how much a parameter changes */
int SceneDraw::s_step = 1;

/* Up and down choose a parameter; circle and square change it, faster as they are held. */
void SceneDraw::UpdateParams()
{
    if (!InputSys::IsPlayerName("BaraBara")) {
        return;
    }
    Pad* pad = InputSys::GetPad("BaraBara");
    Buttons pressed = pad->m_fourWay.pressed;
    Buttons held = pad->m_fourWay.held;
    if (pressed.up) {
        s_selected = (s_selected + NUM_PARAMS - 1) % NUM_PARAMS;
    }
    if (pressed.down) {
        s_selected = (s_selected + 1) % NUM_PARAMS;
    }
    if (pressed.circle) {
        s_params[s_selected] += s_step;
    }
    if (pressed.square) {
        s_params[s_selected] -= s_step;
    }
    if (s_held == 10) {
        if (held.circle) {
            s_params[s_selected] += s_step;
        }
        if (held.square) {
            s_params[s_selected] -= s_step;
        }
        s_held = 0;
        s_repeats++;
    }
    if (s_repeats == 2) {
        s_repeats = 0;
        s_step *= 2;
    }
    if (held.circle || held.square) {
        s_held++;
    } else {
        s_repeats = 0;
        s_held = 0;
        s_step = 1;
    }
    s_params[PARAM_DEVIATION] = s_params[PARAM_DEVIATION] < 0 ? 0 : s_params[PARAM_DEVIATION];
    s_params[PARAM_SUBLINES] = s_params[PARAM_SUBLINES] < 0 ? 0 : s_params[PARAM_SUBLINES];
    s_params[PARAM_BB_MODE] = 0;
    s_params[PARAM_BLUR_ON] &= 1;
    s_params[PARAM_BLUR] = s_params[PARAM_BLUR] < 0 ? 0 : s_params[PARAM_BLUR];
    s_params[PARAM_BLUR] = s_params[PARAM_BLUR] <= 100 ? s_params[PARAM_BLUR] : 100;
    s_params[PARAM_MARKERS_ON] &= 1;
    PrintParams();
}

const char* SceneDraw::s_roadFile = "road/road.tmd";
const char* SceneDraw::s_markFile = "road/mark.tmd";
const char* SceneDraw::s_pointFile = "road/point.tmd";

SceneDraw::MoveFiles SceneDraw::s_moveFiles[NUM_MOVES] = {
    { 0, { { "n01_j.anm", "n01_j_f.anm" }, { "n01_j.anm", "n01_j_f.anm" }, { "n01_j.anm", "n01_j_f.anm" }, { "n01_j.anm", "n01_j_f.anm" } } },
    { 1, { { "n02_h.anm", "" }, { "n02_h.anm", "" }, { "n02_h.anm", "" }, { "n02_h.anm", "n02_h_f.anm" } } },
    { 2, { { "n03_l.anm", "n03_l_f.anm" }, { "n03_l.anm", "n03_l_f.anm" }, { "n03_l.anm", "n03_l_f.anm" }, { "n03_l.anm", "n03_l_f.anm" } } },
    { 3, { { "n04_w.anm", "" }, { "n04_w.anm", "" }, { "n04_w.anm", "n04_w_f.anm" }, { "n04_w.anm", "n04_w_f.anm" } } },
    { 4, { { "n05_jh.anm", "" }, { "n05_jh.anm", "" }, { "n05_jh.anm", "" }, { "n05_jh.anm", "" } } },
    { 5, { { "n06_jl.anm", "" }, { "n06_jl.anm", "" }, { "n06_jl.anm", "" }, { "n06_jl.anm", "" } } },
    { 6, { { "n07_jw.anm", "" }, { "n07_jw.anm", "" }, { "n07_jw.anm", "" }, { "n07_jw.anm", "" } } },
    { 7, { { "n08_hl.anm", "" }, { "n08_hl.anm", "" }, { "n08_hl.anm", "" }, { "n08_hl.anm", "" } } },
    { 8, { { "n09_hw.anm", "" }, { "n09_hw.anm", "" }, { "n09_hw.anm", "" }, { "n09_hw.anm", "" } } },
    { 9, { { "n10_lw.anm", "" }, { "n10_lw.anm", "" }, { "n10_lw.anm", "" }, { "n10_lw.anm", "" } } },
    { MOVE_NONE, { { "n00_skip.anm", "" }, { "n00_skip.anm", "" }, { "n00_skip.anm", "" }, { "n00_skip.anm", "" } } },
};

const char* SceneDraw::s_characterDirs[NUM_CHARACTERS] = {
    "chara/Super",
    "chara/Peeloo",
    "chara/Frog",
    "chara/Snake",
};

const char* SceneDraw::s_modelFile = "model.tmd";

Fixed SceneDraw::s_offset;
Fixed SceneDraw::s_speedJitter;
Fixed SceneDraw::s_jitter;
Fixed SceneDraw::s_position;
Fixed SceneDraw::s_length;
Fixed SceneDraw::s_start;

int SceneDraw::s_speedCourse;
int SceneDraw::s_evoFrame;
ModelMovie* SceneDraw::s_evo;
TmdModel* SceneDraw::s_evoModel;
TmdLineObj* SceneDraw::s_point;
TmdModel* SceneDraw::s_marks;
TmdModel* SceneDraw::s_road;
int SceneDraw::s_lastCharacter;
int SceneDraw::s_alternate;
int SceneDraw::s_move;
int SceneDraw::s_character;
ModelMovie* SceneDraw::s_moves[NUM_CHARACTERS][NUM_MOVES][2];
TmdModel* SceneDraw::s_models[NUM_CHARACTERS];

void SceneDraw::DrawMark(int mark, int zOffset)
{
    TmdLineObj* object = s_marks->m_objects[mark];
    object->SetJitter(Fixed());
    object->SetPasses(1);
    s_marks->m_objects[mark]->Draw(zOffset, NULL);
}

Fixed SceneDraw::Lean(Fixed position, int side)
{
    Fixed tenth = 409.6;
    Fixed distance = (position - s_start - 0.5) * 512;
    Fixed lean;
    if (Fixed() < distance) {
        lean = Fixed::FromRaw((short)((int)((long long)distance.Raw() * tenth.Raw() >> 25) / s_length.Raw()) * 8);
    } else {
        lean = Fixed();
    }
    return side ? lean : -lean;
}

void SceneDraw::DrawObject(TmdLineObj* object, Fixed position, int missed, SVECTOR* join, int angle)
{
    int zOffset = ((s_offset + position) * 600).Integer();
    SVECTOR angles;
    angles.vx = 0;
    angles.vy = 0;
    angles.vz = angle;
    s_pieceCoord.Rotate(&angles);
    s_pieceCoord.Invalidate();
    SetLsMatrix(&s_pieceCoord);
    Fixed jitter = s_jitter * Fixed(1.5);
    if (missed) {
        jitter = s_speedJitter * 5;
    }
    object->SetJitter(jitter);
    object->SetPasses(1);
    object->Draw(zOffset, join);
    if (s_params[PARAM_MARKERS_ON]) {
        DrawMark(1, zOffset);
    }
}

void SceneDraw::DrawCharacter(Fixed position, Fixed speed, Fixed shake)
{
    s_characterCoord.SetTranslation(0, 0, ((s_offset + position) * 600).Integer());
    s_characterCoord.Invalidate();
    Fixed jitter;
    if (Fixed() < shake) {
        jitter = s_jitter + shake * s_jitter * 8;
    } else {
        jitter = s_jitter;
    }
    s_models[s_character]->SetJitter(jitter, s_params[PARAM_SUBLINES]);
    ModelMovie* movie = CurrentMove();
    movie->SetSpeed(speed * Fixed(0.5));
    movie->Draw();
    if (movie->m_finished) {
        /* on to running */
        s_alternate = 0;
        s_move = MOVE_NONE;
        CurrentMove()->Rewind();
    }
    if (s_params[PARAM_MARKERS_ON]) {
        SetLsMatrix(&s_characterCoord);
        DrawMark(0, 0);
    }
}

void SceneDraw::Init(Fixed length, Fixed start)
{
    static char path[64];
    s_length = length;
    s_start = start;
    s_position = Fixed();
    s_offset = -length / 2;
    s_marks = LoadTmdModel(s_markFile);
    s_point = LoadTmdLineObj(s_pointFile);
    s_evoModel = LoadTmdModel("meters/evo.tmd");
    s_evo = new ModelMovie(s_evoModel, MemPtr<short>("meters/evo.anm"), 0);
    s_evo->SetParent(&s_characterCoord);
    for (int character = 0; character < NUM_CHARACTERS; character++) {
        for (int move = 0; move < NUM_MOVES; move++) {
            for (int alternate = 0; alternate < 2; alternate++) {
                s_moves[character][move][alternate] = NULL;
            }
        }
    }
    for (int character = 0; character < NUM_CHARACTERS; character++) {
        sprintf(path, "%s/%s", s_characterDirs[character], s_modelFile);
        s_models[character] = LoadTmdModel(path);
        for (int i = 0; i < NUM_MOVES; i++) {
            MoveFiles* files = &s_moveFiles[i];
            for (int alternate = 0; alternate < 2; alternate++) {
                const char* file = files->files[character][alternate];
                if (*file != '\0') {
                    sprintf(path, "%s/%s", s_characterDirs[character], file);
                    MemPtr<short> anm(path);
                    ModelMovie* movie = new ModelMovie(s_models[character], anm, 0);
                    movie->SetParent(&s_characterCoord);
                    if (files->move == MOVE_NONE) {
                        movie->SetLoop(1);
                    }
                    s_moves[character][files->move][alternate] = movie;
                } else {
#line 373 "C:/psj/dev/locale/game/SceneDraw.cpp"
                    assert(s_moves[character][files->move][0] != NULL);
#line 261 "src/game/SceneDraw.cpp"
                    s_moves[character][files->move][alternate] = s_moves[character][files->move][0];
                }
            }
        }
    }
    s_road = LoadTmdModel(s_roadFile);
#line 382 "C:/psj/dev/locale/game/SceneDraw.cpp"
    assert(s_road->m_numObjects == 10);
#line 270 "src/game/SceneDraw.cpp"
    s_cameraCoord.SetTranslation(0, 0, ((s_offset + s_start) * 600).Integer());
    Reset(0);
}

void SceneDraw::Quit()
{
    delete s_marks;
    delete s_point;
    delete s_road;
    delete s_evoModel;
    delete s_evo;
    for (int character = 0; character < NUM_CHARACTERS; character++) {
        delete s_models[character];
        for (int i = 0; i < NUM_MOVES; i++) {
            MoveFiles* files = &s_moveFiles[i];
            for (int alternate = 0; alternate < 2; alternate++) {
                if (*files->files[character][alternate] != '\0') {
                    delete s_moves[character][files->move][alternate];
                }
            }
        }
    }
}

void SceneDraw::Reset(int speedCourse)
{
    s_speedCourse = speedCourse;
    s_alternate = 0;
    s_move = MOVE_NONE;
    CurrentMove()->Rewind();
    s_character = 0;
    s_lastCharacter = -1;
    s_evoFrame = 0;
    s_evo->Rewind();
    s_evo->SetSpeed(Fixed());
}

void SceneDraw::SetMove(int move, int alternate)
{
    s_alternate = alternate != 0;
    s_move = move;
    CurrentMove()->Rewind();
}

int SceneDraw::IsMoveDone(Fixed fraction)
{
    if (s_move == MOVE_NONE) {
        return 1;
    }
    ModelMovie* movie = CurrentMove();
    return movie->m_position >= fraction * movie->m_numFrames;
}

void SceneDraw::Update()
{
    UpdateParams();
    VideoSys::SetFade(s_params[PARAM_BLUR_ON] ? s_params[PARAM_BLUR] : 0);
}

/* the last frame of the evo meter's movie for each character */
const int SceneDraw::s_evoEnds[NUM_CHARACTERS] = { 58, 23, 19, 0 };

void SceneDraw::Draw(Fixed speed, deque<RoadPiece>* road, int still, Fixed shake, int character,
                     int stage, CGameCamera* camera)
{
    if (character != s_lastCharacter) {
        /* the move goes on in the new character's version */
        ModelMovie* from = s_moves[s_character][s_move][s_alternate];
        s_character = character;
        ModelMovie* to = s_moves[character][s_move][s_alternate];
        if (from->m_position < Fixed::Int(to->m_numFrames)) {
            to->Seek(from->m_position);
        } else {
            to->Rewind();
        }
    }
    s_speedJitter = Fixed::Int(14) * stage + Fixed::Int(10);
    s_jitter = still ? Fixed() : s_speedJitter;
    camera->Update(&s_cameraCoord);
    Fixed before = -s_length * 4;
    SVECTOR join;
    join.vx = 0;
    join.vy = 0;
    join.vz = (before * 600).Integer();
    Fixed after = s_length * 5;
    int side = camera->Side();
    DrawObject(s_point, before, 0, &join, 0);
    for (deque<RoadPiece>::const_iterator piece = road->begin(), end = road->end(); piece != end; ++piece) {
        int lean = piece->leaning ? Lean(piece->position, side).Raw() : 0;
        TmdLineObj* object = s_road->m_objects[piece->object];
        int missed = piece->missed;
        DrawObject(object, piece->position, missed, &join, lean);
    }
    DrawObject(s_point, after, 0, &join, 0);
    DrawCharacter(s_position + s_start, speed, shake);
    s_evo->Draw();
    if (Fixed() < s_evo->m_speed && Fixed::Int(s_evoEnds[s_character]) < s_evo->m_position) {
        s_evo->SetSpeed(Fixed());
        s_evo->Seek(Fixed::Int(s_evoFrame));
    }
}

void SceneDraw::ShowEvo(int frame)
{
    s_evoFrame = frame;
    if (s_evo->m_speed == Fixed() || s_evo->m_finished) {
        s_evoModel->SetJitter(Fixed::Int(10), 1);
        s_evo->SetSpeed(Fixed());
        s_evo->Seek(Fixed::Int(frame));
    }
}

void SceneDraw::PlayEvo()
{
    s_evoFrame = 0;
    s_evoModel->SetJitter(Fixed::Int(5), 1);
    s_evo->SetSpeed(Fixed::Int(1));
}
