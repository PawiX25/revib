#include "common.h"

/* the "Info: status" and "Analog: Left" strings of a shared header (before the others') */
INCLUDE_RODATA("asm/game/nonmatchings/TitleMain", D_80019508);

#include <stdio.h>
#include <stdlib.h>

#include "GameParams.h"
#include "Format.h"
#include "AudioSys.h"
#include "CdSys.h"
#include "FileSys.h"
#include "InputControl.h"
#include "InputSys.h"
#include "LanguageSelect.h"
#include "Logos.h"
#include "MemorySys.h"
#include "Movie.h" /* before Message.h: its asserts' strings come first */
#include "Message.h"
#include "VideoSys.h"

/* `value`, counted from `to` rather than from `from` */
template <class From, class To>
To Rebase(From value, From from, To to)
{
    return To(value + (to - from));
}

/* in the demos' list: a demo of the kiosk executable */
#define DEMO_KIOSK -99

/* the levels played in turn, with the kiosk executable's demo now and then */
static const int s_demoLevels[] = { 1, 2, 3, DEMO_KIOSK, 4, 5, 6, DEMO_KIOSK };

int main(int argc, char** argv)
{
    printf("Title main()\n");
    GameParams* params;
    if (argc == LOADER_ARGC) {
        params = &((LoaderData*)argv)->params;
    } else {
        /* run on its own: parameters of its own */
        GameParams* own = new GameParams;
        own->game.saved = 0;
        params = own;
    }
    int fromLoader = argc == LOADER_ARGC;
    int firstRun = 0;
    if (fromLoader) {
        firstRun = params->firstRun != 0;
    }
    Message* message = NULL;
    MemorySys::Info();
    VideoSys::Init(512, fromLoader, 1, 1, 1);
    LanguageSelect::Init(&params->choices);
    FileSys::Init("title/");
    if (!firstRun) {
        message = new Message;
    }
    InputSys::Init();
    AudioSys::Init();
    AudioSys::SetVolume(0.9f);
    CdSys::Init(&params->homeDisc);
    AudioSys::LoadSounds("audio/title.vh", "audio/title.vb");
    FileSys::LoadArchive("files.pak");
    /* mode 1, the European version: it has the language menu */
    int languages = LanguageSelect::GetMode() == 1;
    LanguageSelect::InitMenus();
    if (languages) {
        LanguageSelect::SetLanguageCursor(Rebase(LanguageSelect::GetLanguage(), 0, 1));
    }
    if (firstRun) {
        params->firstRun = 0;
        LoadLogos();
        ShowSonyLogos(1.0, 2.0, 1.0);
        ShowNanaOnShaLogos(1.0, 2.0, 1.0);
        FreeLogos();
    }
    InputControl* control = new InputControl("Play");
    TitleScene* title = new TitleScene;
    if (fromLoader && params->returning) {
        LanguageSelect::Close();
        params->returning = 0;
        title->Seek(1);
    }
    delete message;
    DiscInfo discInfo;
    ShellOpen shellOpen;
    CdSys::SetDiscInfoReceiver(&discInfo);
    CdSys::SetShellOpenReceiver(&shellOpen.ready);
    int homeDisc = 0;
    MemorySys::DumpUsage();
    int idle = 0;
    MemorySys::DumpHeap();
    int demoStarted = 0;
    int quit = 0;
    int introOver = 0;
    int next = EXECUTABLE_GAME;
    int inLanguageMenu = 0;
    while (1) {
        VideoSys::Flip(1);
        CdSys::Update();
        if (discInfo.ready) {
            idle = 0;
            discInfo.ready = 0;
            homeDisc = discInfo.isHomeDisc;
        }
        if (shellOpen.ready) {
            shellOpen.ready = 0;
            homeDisc = 0;
        }
        title->Update();
        LanguageSelect::Update();
        if (!introOver && title->IntroOver()) {
            introOver = 1;
            LanguageSelect::Show(firstRun && languages ? LanguageSelect::MENU_LANGUAGE : LanguageSelect::MENU_MODE);
            inLanguageMenu = firstRun;
        }
        LanguageSelect::Result result = LanguageSelect::GetResult();
        switch (result) {
        case LanguageSelect::RESULT_LANGUAGE:
        case LanguageSelect::RESULT_LANGUAGE + 1:
        case LanguageSelect::RESULT_LANGUAGE + 2:
        case LanguageSelect::RESULT_LANGUAGE + 3:
        case LanguageSelect::RESULT_LANGUAGE + 4:
            inLanguageMenu = 0;
            LanguageSelect::SetLanguage(Rebase(result, LanguageSelect::RESULT_LANGUAGE, 0));
            LanguageSelect::Show(LanguageSelect::MENU_MODE);
            break;
        case LanguageSelect::RESULT_BACK:
            if (languages) {
                LanguageSelect::Show(LanguageSelect::MENU_LANGUAGE);
                inLanguageMenu = 1;
            }
            break;
        case LanguageSelect::RESULT_KIOSK:
            LanguageSelect::Show(LanguageSelect::MENU_START);
            title->Leave();
            next = EXECUTABLE_KIOSK;
            break;
        case LanguageSelect::RESULT_PLAY:
            LanguageSelect::Show(LanguageSelect::MENU_START);
            title->Leave();
            next = EXECUTABLE_GAME;
            break;
        }
        if (LanguageSelect::GetResult() == LanguageSelect::RESULT_STARTED) {
            break;
        }
        control->Update();
        if (control->m_quit) {
            quit = 1;
            break;
        }
        Buttons held = InputSys::GetPad("Play")->m_fourWay.held;
        if (held.Bits()) {
            idle = 0;
        }
        /* left alone with the game's disc in: a demo */
        if (fromLoader && homeDisc && !inLanguageMenu && !demoStarted && ++idle > 96 * VideoSys::Fps()) {
            demoStarted = 1;
            int i = params->demoCount % 8;
            params->demoCount = i + 1;
            int level = s_demoLevels[i];
            LanguageSelect::Result choice = LanguageSelect::RESULT_KIOSK;
            if (level != DEMO_KIOSK) {
                choice = LanguageSelect::RESULT_PLAY;
                params->demo = demoStarted;
                params->demoLevel = level;
            }
            LanguageSelect::Choose(choice);
        }
    }
    if (!quit) {
        VideoSys::Reset();
    }
    delete title;
    delete control;
    LanguageSelect::FreeMenus();
    AudioSys::FreeSounds();
    CdSys::Quit();
    AudioSys::Quit();
    InputSys::Quit();
    FileSys::Quit();
    VideoSys::Quit();
    if (argc == LOADER_ARGC) {
        ((LoaderData*)argv)->next = next;
    } else {
        delete params;
    }
    if (quit) {
        MemorySys::Quit();
    }
    return 0;
}

/*
 * The allocator's handler, defined in asm here, at the end of the file, so
 * that it is the last word of the file's small data, as in the game. maspsx
 * does not take what is defined between .pushsection and .popsection for
 * small data, so _S_oom_malloc() (compiled at the end of the file) loads it
 * with lui/lw, as the game does.
 */
__asm__(".pushsection .sdata\n"
        "\t.align\t2\n"
        "\t.globl\t_t23__malloc_alloc_template1i0.__malloc_alloc_oom_handler\n"
        "_t23__malloc_alloc_template1i0.__malloc_alloc_oom_handler:\n"
        "\t.word\t0\n"
        ".popsection");
