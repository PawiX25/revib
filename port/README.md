# Vib-Ribbon in the browser

The decompiled game ([../src/game](../src/game)) built with Emscripten, with
a runtime standing in for the PlayStation, played in a web page:
**https://pawix25.github.io/revib/**

You need your own copy of the game: the page has none of its files. Choose
the disc image of *Vib-Ribbon* (PAL, SCES-02873), its `.cue` file together
with all of its `.bin` files. The files are read in the page and stay on
your computer; the page can keep them in the browser for the next visit.

## Playing

- **Your own music.** Choose *Play with my own CD* in the game. When it asks
  for a music CD, the page asks for files (MP3, FLAC, WAV, OGG, anything the
  browser plays): they become the tracks of an audio CD, in the order chosen.
- **Menus.** Click what you want in the game's own menus, turn the mouse
  wheel to move through them, right-click or <kbd>Esc</kbd> to go back.
- **Keys.** Arrows to move (<kbd>↓</kbd> for pits), <kbd>X</kbd> or
  <kbd>Space</kbd> for waves, <kbd>A</kbd> for blocks, <kbd>D</kbd> for
  loops, <kbd>P</kbd> to pause. They can be changed in the settings.
  Gamepads work with the PlayStation's layout.
- **High scores.** The game never saved anything: its high scores and
  options were gone when the console was switched off. The page keeps them
  in the browser.

## Settings

Each one is explained in the page, with an example made from your disc.

| Setting | The two ways                                                                              |
|---------|-------------------------------------------------------------------------------------------|
| Picture | the lines drawn again at the screen's resolution, or the console's 512 × 288, enlarged     |
| Music   | the disc's stereo 44.1 kHz, or as the console played it through its sound chip (mono, 22 kHz) |
| Sounds  | samples interpolated linearly, or with the sound chip's Gaussian filter                   |

## How it works

- Each executable on the disc (title, game, kiosk) becomes a WebAssembly
  module; `web/vib.js` runs them in turn with the data they hand on, as the
  loader does. The game's sources are unchanged but for `#ifdef PORT`
  blocks, so the original executables still build byte-identical.
- `runtime/` stands in for Sony's libraries: the GPU (which also records
  each frame's lines for the sharp picture), libgs, the GTE, the SPU (ADPCM
  voices, envelopes, reverb, the capture buffers the game analyses the music
  with), the CD drive and the controller. The game's sleeps and waits for
  the disc are made asynchronous with Asyncify; its sound thread is a fiber.
- The game's data is always read from your disc image. `web/title-data.json`
  holds the lines of the title screen's turning name, so the first screen
  can show it before a disc is chosen (`tools/make-title.mjs` makes it from
  a disc image).

## Building

Linux or WSL 2, with [Emscripten](https://emscripten.org) 6.0.13:

```sh
source ~/emsdk/emsdk_env.sh
make -f port/Makefile
cd port/web && python3 -m http.server 8123
```

Then open http://localhost:8123. Every push to the `port` branch builds the
page and puts it on GitHub Pages (`.github/workflows/pages.yml`).

## Tools

- `tools/run.mjs` runs the game in Node, as fast as it goes: pictures of the
  screen, its sound as WAV, buttons pressed on given frames.
- `tools/make-title.mjs` makes `web/title-data.json` from a disc image.
