#include "common.h"

/* the "Info: status" and "Analog: Left" strings of a shared header (before the others') */
INCLUDE_RODATA("asm/main_k/nonmatchings/KioskMain", D_80019508);

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
#include "MemorySys.h"
#include "Movie.h" /* before Message.h: its asserts' strings come first */
#include "Message.h"
#include "VideoSys.h"

/* The allocator's handler is defined here. */
template void (*__malloc_alloc_template<0>::__malloc_alloc_oom_handler)();

/* the track the demo is played to, on a disc that has it */
#define KIOSK_TRACK 7

enum KioskState {
    KIOSK_WAITING_FOR_DISC, /* for the game's disc to be identified */
    KIOSK_STARTING_TRACK,
    KIOSK_PLAYING,
    KIOSK_OVER
};

/*
 * The kiosk version: the demo scene played to one of the game's tracks,
 * from the disc in the drive, after which it goes back to the title.
 */
int main(int argc, char** argv)
{
    printf("Kiosk main()\n");
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
    VideoSys::Init(512, fromLoader, 1, 1, 1);
    LanguageSelect::Init(&params->choices);
    FileSys::Init("kiosk/");
    Message* message = new Message;
    InputSys::Init();
    AudioSys::Init();
    AudioSys::SetVolume(0.9f);
    CdSys::Init(&params->homeDisc);
    InputControl* control = new InputControl("Play");
    AudioSys::LoadSounds("audio/kiosk.vh", "audio/kiosk.vb");
    FileSys::LoadArchive("files.pak");
    KioskScene* scene = new KioskScene;
    delete message;
    int state = KIOSK_WAITING_FOR_DISC;
    MemorySys::DumpUsage();
    int quit = 0;
    MemorySys::DumpHeap();
    AudioSys::SetCdVolume(1.0f);
    DiscInfo discInfo;
    TrackStartInfo trackStart;
    ShellOpen shellOpen;
    CdSys::SetDiscInfoReceiver(&discInfo);
    CdSys::SetTrackStartReceiver(&trackStart);
    CdSys::SetShellOpenReceiver(&shellOpen.ready);
    while (1) {
        CdSys::Update();
        control->Update();
        if (state == KIOSK_OVER) {
            break;
        }
        if (control->m_quit) {
            quit = 1;
            break;
        }
        if (shellOpen.ready) {
            /* the disc is being changed: wait for the next one */
            shellOpen.ready = 0;
            state = KIOSK_WAITING_FOR_DISC;
            AudioSys::SetCdMix(0);
        }
        switch (state) {
        case KIOSK_WAITING_FOR_DISC:
            if (discInfo.ready) {
                discInfo.ready = 0;
                if (discInfo.isHomeDisc) {
                    AudioSys::SetCdMix(1);
                    int track = discInfo.numTracks >= KIOSK_TRACK ? KIOSK_TRACK : 1;
                    trackStart.ready = 0;
                    CdSys::PlayTrack(track);
                    state = KIOSK_STARTING_TRACK;
                } else {
                    printf("wrong cd inserted\n");
                }
            }
            break;
        case KIOSK_STARTING_TRACK:
            /* the scene starts with the music */
            if (trackStart.ready) {
                trackStart.ready = 0;
                state = KIOSK_PLAYING;
                scene->Reset();
            }
            break;
        case KIOSK_PLAYING:
            VideoSys::Flip(1);
            scene->Update();
            if (scene->IsFinished()) {
                state = KIOSK_OVER;
            }
            break;
        }
    }
    if (!quit) {
        VideoSys::Reset();
    }
    AudioSys::SetCdMix(0);
    delete scene;
    delete control;
    AudioSys::FreeSounds();
    FileSys::Quit();
    CdSys::Quit();
    AudioSys::Quit();
    InputSys::Quit();
    VideoSys::Quit();
    /* back to the title's mode menu */
    params->returning = 1;
    int toLoader = argc == LOADER_ARGC;
    if (!toLoader) {
        delete params;
    }
    if (quit) {
        MemorySys::Quit();
    }
    if (toLoader) {
        ((LoaderData*)argv)->next = EXECUTABLE_TITLE;
    }
    return 0;
}
