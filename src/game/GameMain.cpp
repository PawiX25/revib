#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "Format.h"
#include "MemorySys.h"
#include <string>

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/GameMain", D_80019600);

/* before GameParams.h: its Update() comes after GameParams' constructor */
#include "TrackDisplay.h"
#include "GameParams.h"
#include "AudioStream.h"
#include "AudioSys.h"
#include "CdSys.h"
#include "FileSys.h"
#include "InputControl.h"
#include "InputSys.h"
#include "LanguageSelect.h"
/* after TrackDisplay.h: the "MemPtr: file %s not found." of its inline code comes after the track's strings */
#include "Message.h"
#include "VideoSys.h"
#include "Game.h"
#include "Glue.h"
#include "Title.h"

/* empty in the game */
void BeginPlay()
{
}

void EndPlay()
{
}

int main(int argc, char** argv)
{
    printf("Game main()\n");
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
    MemorySys::Info();
    /* the controllers: the player's, and the one that shows the track */
    const char* mainPad = "Main";
    const char* playPad = "Play";
    VideoSys::Init(512, fromLoader, 1, 1, 1);
    LanguageSelect::Init(&params->choices);
    FileSys::Init("game/");
    Message* message = new Message;
    InputSys::Init();
    AudioSys::Init();
    CdSys::Init(&params->homeDisc);
    AudioStream::Init(8000);
    AudioSys::SetVolume(0.9f);
    AudioSys::LoadSounds("audio/psj_se.vh", "audio/psj_se.vb");
    FileSys::LoadArchive("files.pak");
    InputSys::SetPlayerName(playPad, 1);
    int quit = 0;
    {
        TrackDisplay track;
        InputControl control(playPad);
        Title::Init();
        GameSys::Init();
        Glue::Init(params);
        delete message;
        MemorySys::DumpUsage();
        MemorySys::DumpHeap();
        BeginPlay();
        while (1) {
            VideoSys::Flip(1);
            int time = AudioStream::GetPosition();
            Glue::Update(time);
            if (Glue::IsDone()) {
                break;
            }
            CdSys::Update();
            control.Update();
            if (control.m_quit) {
                quit = 1;
                break;
            }
            if (InputSys::IsPlayerName(mainPad)) {
                VideoSys::Print((track.Text().c_str() + string("\n")).c_str());
            }
            GameSys::Update(time);
            Title::Update();
            GameSys::Draw();
        }
        EndPlay();
        if (!quit) {
            VideoSys::Reset();
        }
        Glue::Quit();
        GameSys::Quit();
        Title::Free();
    }
    AudioSys::FreeSounds();
    AudioStream::Quit();
    FileSys::Quit();
    CdSys::Quit();
    AudioSys::Quit();
    InputSys::Quit();
    VideoSys::Quit();
    int toLoader = argc == LOADER_ARGC;
    if (!toLoader) {
        delete params;
    }
    if (quit) {
        MemorySys::Quit();
    }
    if (toLoader) {
        /* back to the title */
        ((LoaderData*)argv)->next = EXECUTABLE_TITLE;
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
