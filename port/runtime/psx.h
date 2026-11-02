/*
 * The port's runtime: what stands in for the PlayStation under the game.
 *
 * The game's code calls the PSY-Q libraries (libgs, libgpu, libgte, libspu,
 * libds, libcd, libpad, libetc, libapi); the runtime implements the
 * functions it uses, over a small model of the console: VRAM and a GPU that
 * runs the game's ordering tables, the GTE's arithmetic, an SPU with its
 * sound memory, voices and CD capture buffers, a CD drive reading the disc
 * image given by the page, and the controllers. Time is counted in vertical
 * blanks (50 a second, PAL) and in the SPU's samples (44100 a second).
 */
#ifndef PORT_PSX_H
#define PORT_PSX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- the page (library.js) --- */

/*
 * Shows the frame: its picture of `w` x `h` pixels (RGBA), and what was
 * drawn into it (`ops`, `n` words: see gpu.cpp), to draw again at the
 * screen's resolution unless `fallback`; `vram` for the sprites' textures.
 */
void js_present(const uint8_t* rgba, int w, int h, const int32_t* ops, int n, int fallback, const uint16_t* vram);
/* plays `frames` stereo samples (44100 Hz, interleaved) */
void js_audio_push(const int16_t* samples, int frames);
/* the stereo samples queued and not played yet */
int js_audio_queued(void);
/* milliseconds, from any start */
double js_now(void);
/* 1: as fast as possible (tests), 0: in real time */
int js_fast(void);
/* 1: the game is paused (the page's settings are open) */
int js_paused(void);
/* reads raw sector `lba` (2352 bytes) of the disc in the drive: 1, 0 if there is none, -1 if it is not at hand yet (js_cd_wait()) */
int js_cd_read(int lba, uint8_t* dest);
/* waits (asynchronously: the game is paused) until sector `lba` can be read */
void js_cd_wait(int lba);
/* reads sector `lba` (its 2048 bytes of data) of the game's disc, which is always at hand */
int js_data_read(int lba, uint8_t* dest);
/* the drive: 0 lid closed with a disc, 1 lid open, 2 lid closed, no disc */
int js_cd_state(void);
/* the number of times a disc has been put in */
int js_cd_changes(void);
/* the table of contents of the disc in the drive: number of tracks; starts[i] of track i+1, starts[n] the lead-out (sectors from 00:02:00) */
int js_cd_toc(int* starts, int max);
/* the controller: buttons held, PlayStation order (bit set: held) */
int js_pad(int port);
/* a menu read this frame: its screen (Menu.h's port_menu_screen), its grid and cursor (rows 0: confirm or cancel only) */
void js_menu(int screen, int rows, int columns, int x, int y, int active, int wrap);
/* a model loaded from file `name` (TmdLineObj.h), known as `id` */
void js_model(int id, const char* name);
/* object `object` of model `id` was drawn in this rectangle of the screen (from its centre) */
void js_object(int id, int object, int x0, int y0, int x1, int y1);
/* the executable's main() has returned (its LoaderData says which comes next) */
void js_main_done(int status);
/* the game's printf() */
void js_log(const char* text);

/* --- the runtime's parts --- */

/* gpu.cpp */
void gpu_init(void);
void gpu_run(const uint32_t* packet); /* an ordering table, from its first tag */
void gpu_command(const uint32_t* words, int count);
void gpu_present(void);
uint16_t* gpu_vram(void);
void gpu_set_display(int x, int y, int w, int h, int on);

/* spu.cpp */
void spu_init(void);
/* runs the SPU for `frames` samples, with the interrupts they bring */
void spu_run(int frames);
/* the samples of CD audio for the next `frames` (cd.cpp gives them) */
void cd_audio(int16_t* dest, int frames);

/* cd.cpp */
void cd_vsync(void);

/* pad.cpp */
void pad_vsync(void);

/* sys.cpp */
/* waits for the next vertical blank: the display, the sound, the interrupts */
void sys_vblank(void);
/* time passing in a busy loop of the game: a little of the sound, and its interrupts */
void sys_idle(void);
/* interrupts: run now, or once the critical section ends */
void sys_interrupt(void (*handler)(void));
extern volatile long Vcount;

void sys_printf(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
