#ifndef LANGUAGESELECT_H
#define LANGUAGESELECT_H

#include "common.h"

/*
 * The languages, as GetLanguage() numbers them: the five of the language
 * menu (German, English, Spanish, French, Italian), then Japanese, which the
 * European version does not offer.
 */
#define NUM_LANGUAGES 6
#define LANGUAGE_JAPANESE 5

/* The language and the button layout: part of GameParams, which the loader keeps for the executables it runs. */
struct MenuChoices {
    MenuChoices() : valid(0) {}

    int valid;
    /*
     * 0: confirm with circle, cancel with cross, no language menu; 1 (the
     * European version, set by Init() and never changed): confirm with cross,
     * cancel with triangle, and the language menu
     */
    int mode;
    int language;

    /* the defaults, unless something was chosen already */
    void Init();
};

class TmdModel;
class ModelMovie;
struct MenuCursor;
struct MenuInput;

/*
 * The language and mode select menus: a model per menu, moved by a
 * movie as the cursor goes, and a second movie zooming between the
 * menus. What was chosen goes to GetResult().
 */
class LanguageSelect {
public:
    enum Menu {
        MENU_LANGUAGE,
        MENU_MODE,
        MENU_START,
        MENU_NONE
    };
    enum Result {
        RESULT_NONE,
        RESULT_LANGUAGE,    /* RESULT_LANGUAGE + n: the language n (of 5) */
        RESULT_PLAY = 6,    /* the mode menu's items: the game, */
        RESULT_KIOSK,       /* the kiosk executable, */
        RESULT_BACK,        /* back to the language menu */
        RESULT_STARTED      /* the mode menu has gone: the game or the kiosk can start */
    };

    static void Init(MenuChoices* choices);
    static int GetMode();
    /* number of the selected language */
    static int GetLanguage();
    static void SetLanguage(int language);
    /* the buttons that confirm and cancel in menus, as the mode wants them */
    static int ConfirmButton();
    static int CancelButton();

    static void InitMenus();
    static void FreeMenus();
    static void Update();
    static void Show(Menu menu);
    static Result GetResult();
    /* puts the language cursor on a language (counted from 1) */
    static void SetLanguageCursor(int language);
    /* as if `result` had been chosen */
    static void Choose(Result result);
    /* back on the title from the kiosk demo: the mode menu, its lines settling */
    static void Close();

private:
    static void SetModeJitter(int frame);
    static void ResetTransition();
    static void StartTransition();
    static void UpdateTransition();
    static void StartClosing();
    static void PlaySounds();
    static void UpdateLanguageMenu();
    static void UpdateModeMenu();
    static void LoadMenus();

    static MenuChoices* s_choices;
    static const char* s_modelFiles[2];
    static const char* s_selectFiles[2];
    static const char* s_scaleFiles[2];
    static MenuCursor s_languageCursor;
    static MenuCursor s_modeCursor;

    /* The compiler lays these out in reverse order of declaration. */
    static MenuInput s_input;
    static ModelMovie* s_scale[2];  /* zooming in and out of each menu */
    static ModelMovie* s_select[2]; /* each menu, moved to the cursor's item */
    static TmdModel* s_models[2];
    static int s_chosen;
    static Menu s_next;
    static Menu s_current;
    static Result s_result;
    static int s_closing;
    static int s_drawn;
    static int s_reversed;
    static int s_frame;
    static int s_busy;
};

#endif
