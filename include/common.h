#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"
#include "types.h"

/*
 * The same sources make the four executables: MAIN_T.EXE, the title, with
 * none of KIOSK, GAME and LOADER defined (TITLE is then defined here),
 * MAIN_K.EXE, the kiosk, with KIOSK, MAIN_G.EXE, the game itself, with
 * GAME, and SCES_028.73, the loader, with LOADER.
 */
#if !defined(KIOSK) && !defined(GAME) && !defined(LOADER)
#define TITLE
#endif

#endif
