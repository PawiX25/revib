#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "LanguageSelect.h"

MenuChoices* LanguageSelect::s_choices = NULL;

/*
 * Only the title has the language and mode menus. The kiosk and the game
 * keep only the choices they are given: the language, and the confirm and
 * cancel buttons.
 */
#ifdef TITLE
#include "Format.h"
#include "AudioSys.h"
#include "Menu.h"
#include "ModelMovie.h"
#include "TmdLineObj.h"

const char* LanguageSelect::s_modelFiles[2] = { "menu/lang/lang.tmd", "menu/mode/mode.tmd" };
const char* LanguageSelect::s_selectFiles[2] = { "menu/lang/select.anm", "menu/mode/select.anm" };
const char* LanguageSelect::s_scaleFiles[2] = { "menu/lang/scale.anm", "menu/mode/scale.anm" };

/* where the cursor goes for each language */
static const struct {
    int x, y;
} s_languagePositions[] = {
    { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 1 }, { 2, 1 },
};

static inline int LanguageColumn(int i)
{
    return s_languagePositions[i].x;
}

static inline int LanguageRow(int i)
{
    return s_languagePositions[i].y;
}

/* the languages, in two rows (the fourth is wider) */
static const int s_languageCells[] = {
    0, 1, 2,
    3, 3, 4,
};

static const int s_modeCells[] = { 0, 1, 2 };

static MenuGrid s_languageGrid(3, 2, s_languageCells);
MenuCursor LanguageSelect::s_languageCursor;
static MenuGrid s_modeGrid(1, 3, s_modeCells);
MenuCursor LanguageSelect::s_modeCursor;

MenuInput LanguageSelect::s_input;
ModelMovie* LanguageSelect::s_scale[2];
ModelMovie* LanguageSelect::s_select[2];
TmdModel* LanguageSelect::s_models[2];
int LanguageSelect::s_chosen;
LanguageSelect::Menu LanguageSelect::s_next;
LanguageSelect::Menu LanguageSelect::s_current;
LanguageSelect::Result LanguageSelect::s_result;
int LanguageSelect::s_closing;
int LanguageSelect::s_drawn;
int LanguageSelect::s_reversed;
int LanguageSelect::s_frame;
int LanguageSelect::s_busy;
#endif

void MenuChoices::Init()
{
    if (!valid) {
        mode = 1;
        language = 1;
        valid = 1;
    }
}

void LanguageSelect::Init(MenuChoices* choices)
{
    choices->Init();
    s_choices = choices;
}

#ifdef TITLE
int LanguageSelect::GetMode()
{
    return s_choices->mode;
}
#endif

int LanguageSelect::GetLanguage()
{
    return s_choices->language;
}

#ifdef TITLE
void LanguageSelect::SetLanguage(int language)
{
    s_choices->language = language;
}
#endif

/*
 * 0x2000: BUTTON_CIRCLE, 0x4000: BUTTON_CROSS, 0x1000: BUTTON_TRIANGLE. The
 * kiosk and the game build this file without InputSys.h: its <string> would
 * add a "" to their data.
 */
int LanguageSelect::ConfirmButton()
{
    return s_choices->mode == 0 ? 0x2000 : 0x4000;
}

int LanguageSelect::CancelButton()
{
    return s_choices->mode == 0 ? 0x4000 : 0x1000;
}

#ifdef TITLE
/* the mode menu's lines jittering more with each `frame`: counted up as the game or the kiosk starts, down after Close() */
void LanguageSelect::SetModeJitter(int frame)
{
    int t = PerFrames(120, frame);
    s_models[MENU_MODE]->SetJitter(Fixed::Int(t * t * 7 + 10), 1);
}

void LanguageSelect::ResetTransition()
{
    s_busy = 0;
    s_frame = 0;
    s_reversed = 0;
    s_drawn = 0;
    s_closing = 0;
}

void LanguageSelect::StartTransition()
{
    s_busy = 1;
    if (s_next == MENU_START) {
        s_frame = 0;
    }
}

void LanguageSelect::UpdateTransition()
{
    if (s_busy) {
        if (s_next == MENU_START) {
            SetModeJitter(s_frame++);
            if (!AudioSys::IsPlaying(1)) {
                s_busy = 0;
                s_result = RESULT_STARTED;
                s_current = s_next;
            }
        } else if (s_closing) {
            SetModeJitter(s_frame--);
            if (s_frame == 0) {
                s_models[MENU_MODE]->SetJitter(10.0, 1);
                s_busy = 0;
                s_closing = 0;
            }
        } else if (s_current == MENU_NONE) {
            s_current = s_next;
            s_busy = 0;
        } else {
            s_drawn = 1;
            s_scale[s_current]->Draw();
            if (s_scale[s_current]->m_finished) {
                int reversed = s_reversed != 1;
                ModelMovie* next = s_scale[s_next];
                s_reversed = reversed;
                s_current = s_next;
                next->Rewind(reversed);
                int direction = s_reversed == 1 ? -1 : 1;
                ModelMovie* movie = s_scale[s_current];
                movie->SetSpeed(Fixed::Int(direction));
                s_busy = s_reversed;
            }
        }
    } else {
        s_drawn = 0;
    }
}

void LanguageSelect::StartClosing()
{
    s_busy = 1;
    s_frame = 20;
    s_closing = 1;
}

void LanguageSelect::PlaySounds()
{
    if ((s_input.left || s_input.right || s_input.up || s_input.down) && s_input.changed) {
        Sound move(0, 6);
        AudioSys::PlaySound(&move, 5, 64, -1);
    }
    if (s_input.confirm) {
        Sound confirm(0, 0);
        AudioSys::PlaySound(&confirm, 1, 64, -1);
    }
}

void LanguageSelect::UpdateLanguageMenu()
{
    if (!s_busy && !s_chosen) {
        s_languageCursor.Read(&s_input, 0, 0, 1);
        PlaySounds();
        s_select[MENU_LANGUAGE]->m_position = Fixed::Int(s_languageCursor.Item());
        if (s_input.confirm) {
            s_result = Result(RESULT_LANGUAGE + s_languageCursor.Item());
        }
        s_select[MENU_LANGUAGE]->Draw();
    }
}

void LanguageSelect::UpdateModeMenu()
{
    if (!s_busy && !s_chosen) {
        s_modeCursor.Read(&s_input, 0, 0, 1);
        PlaySounds();
        s_select[MENU_MODE]->m_position = Fixed::Int(s_modeCursor.Index());
        if (s_input.confirm) {
            s_result = Result(RESULT_PLAY + s_modeCursor.Index());
        }
    }
    if (!s_drawn) {
        s_select[MENU_MODE]->Draw();
    }
}

void LanguageSelect::LoadMenus()
{
    int i, j, k;
    for (i = 0; i < 2; i++) {
        delete s_models[i];
        s_models[i] = LoadTmdModel(s_modelFiles[i]);
        s_models[i]->SetJitter(10.0, 1);
    }
    for (j = 0; j < 2; j++) {
        delete s_select[j];
        s_select[j] = new ModelMovie(s_models[j], MemPtr<short>(s_selectFiles[j]), 0);
        s_select[j]->SetSpeed(0.0);
    }
    for (k = 0; k < 2; k++) {
        delete s_scale[k];
        s_scale[k] = new ModelMovie(s_models[k], MemPtr<short>(s_scaleFiles[k]), 0);
    }
}

void LanguageSelect::InitMenus()
{
    KeyRepeat::Init();
    ResetTransition();
    s_next = MENU_NONE;
    s_current = MENU_NONE;
    s_result = RESULT_NONE;
    s_chosen = 0;
    int i;
    for (i = 0; i < 2; i++) {
        s_models[i] = NULL;
    }
    for (i = 0; i < 2; i++) {
        s_select[i] = NULL;
    }
    for (i = 0; i < 2; i++) {
        s_scale[i] = NULL;
    }
    LoadMenus();
    s_languageCursor = MenuCursor(&s_languageGrid);
    s_modeCursor = MenuCursor(&s_modeGrid);
}

void LanguageSelect::FreeMenus()
{
    int i, j, k;
    for (i = 0; i < 2; i++) {
        delete s_models[i];
    }
    for (j = 0; j < 2; j++) {
        delete s_select[j];
    }
    for (k = 0; k < 2; k++) {
        delete s_scale[k];
    }
}

void LanguageSelect::Update()
{
    UpdateTransition();
    switch (s_current) {
    case MENU_LANGUAGE:
        UpdateLanguageMenu();
        break;
    case MENU_MODE:
        UpdateModeMenu();
        break;
    case MENU_START:
    case MENU_NONE:
        break;
    }
}

void LanguageSelect::Show(Menu menu)
{
    if (menu == MENU_MODE) {
        LoadMenus();
    }
    s_next = menu;
    StartTransition();
    s_result = RESULT_NONE;
    s_chosen = 0;
}

LanguageSelect::Result LanguageSelect::GetResult()
{
    return s_result;
}

void LanguageSelect::SetLanguageCursor(int language)
{
    int i = language - 1;
    s_languageCursor.x = LanguageColumn(i);
    s_languageCursor.y = LanguageRow(i);
}

void LanguageSelect::Choose(Result result)
{
    s_input.confirm = 1;
    s_input.changed = 0;
    PlaySounds();
    s_chosen = 1;
    s_result = result;
}

void LanguageSelect::Close()
{
    StartClosing();
    s_current = MENU_MODE;
}
#endif
