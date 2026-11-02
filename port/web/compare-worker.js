// Makes the settings' before-and-after examples from the player's own disc,
// away from the page: the game run as fast as it goes, its sound recorded (and
// one picture kept), the same stretch twice with one setting changed.
//
//   sounds: the title's first minute, the sounds interpolated linearly or as the SPU does
//   music:  the first Bronze course as the game's own demo plays it (the language chosen,
//           then nothing pressed), from its start, its song from the CD or as the console
//           played it; with `picture`, a frame of it
//   logo:   the title screen's lines while the name turns once round

import { Machine, discFromFiles } from './vib.js';

const SECONDS = { sounds: 60, music: 45 };
const FRAMES_PER_SECOND = 49.76;
const DEMO_COURSE_STARTS = 19; // the frames from the game executable's start to the demo's course's
const fail = (err) => postMessage({ error: String(err && err.message ? err.message : err) });

onmessage = async ({ data: { job, files, setting, picture } }) => {
  const gameDisc = await discFromFiles(files);
  if (job === 'logo') return logo(gameDisc);
  const recorded = [];
  let samples = 0;
  const wanted = Math.round(SECONDS[job] * 44100);
  let frame = 0;
  let begun = job === 'sounds' ? 0 : -1; // the frame the recording starts at
  let choose = -1; // the frame the language is chosen at
  const pictureAt = Math.round((picture || 0) * FRAMES_PER_SECOND);
  let kept = null;

  const machine = new Machine({
    gameDisc,
    fast: true,
    load: async (name) => (await import(`./build/${name}.js`)).default,
    log: (t) => {
      if (job === 'music' && begun < 0 && t.startsWith('--- game ---')) begun = frame + DEMO_COURSE_STARTS;
    },
    menu: (m) => {
      if (job === 'music' && choose < 0 && m.screen === 100) choose = frame + 10;
    },
    // ✕ once, on the language menu
    pad: () => (choose >= 0 && frame >= choose && frame < choose + 4 ? 0x4000 : 0),
    audio: (s, n) => {
      if (begun < 0 || samples >= wanted) return;
      recorded.push(Int16Array.from(s));
      samples += n;
      if (samples >= wanted) done();
    },
    present: (rgba, w, h, f) => {
      frame++;
      if (picture && begun >= 0 && frame === begun + pictureAt && f) {
        kept = { rgba: Uint8Array.from(rgba), w, h, ops: Int32Array.from(f.ops), fallback: f.fallback, vram: Uint16Array.from(f.vram) };
      }
    },
  });
  if (job === 'sounds') machine.setGaussian(setting === 'playstation');
  else machine.setHqMusic(setting === 'cd');

  function done() {
    const out = new Int16Array(wanted * 2);
    let at = 0;
    for (const s of recorded) {
      const n = Math.min(s.length, out.length - at);
      out.set(s.subarray(0, n), at);
      at += n;
    }
    const transfer = [out.buffer];
    if (kept) transfer.push(kept.rgba.buffer, kept.ops.buffer, kept.vram.buffer);
    postMessage({ samples: out, picture: kept }, transfer);
  }

  machine.run().catch(fail);
};

// The title as the game shows it: the lines above the language menu in each
// frame while the name turns once round (398 frames), every other one.
const TURN = 398;
function logo(gameDisc) {
  let frame = 0;
  let from = -1; // the frame the language menu first showed
  const frames = [];
  const machine = new Machine({
    gameDisc,
    fast: true,
    load: async (name) => (await import(`./build/${name}.js`)).default,
    log: () => {},
    menu: (m) => {
      if (from < 0 && m.screen === 100) from = frame + 10;
    },
    present: (rgba, w, h, f) => {
      frame++;
      if (from < 0 || frame < from || (frame - from) % 2 || !f) return;
      const out = [];
      const o = f.ops;
      for (let i = 0; i < o.length; ) {
        const op = o[i];
        if (op === 2) {
          // (the menu starts at y 190)
          if (o[i + 2] < 180 && o[i + 4] < 180) out.push(o[i + 1], o[i + 2], o[i + 3], o[i + 4], o[i + 5]);
          i += 8;
        } else i += { 1: 6, 3: 7, 4: 14 }[op] || o.length;
      }
      frames.push(Int32Array.from(out));
      if (frames.length === TURN / 2) postMessage({ logo: { frames } }, frames.map((l) => l.buffer));
    },
  });
  machine.run().catch(fail);
}
