// The page: the disc image and music files the player gives, the screen, the
// sound, the keyboard and gamepads, the settings with their examples, and the
// machine (vib.js) that runs the game.
import { Machine, discFromFiles, PcmTrack, audioDisc } from './vib.js';
import { Renderer } from './renderer.js';
import { MenuDriver } from './menus.js';
import { makeExamples, AbPlayer, Wipe, run } from './compare.js';
import { TitleRing } from './logo.js';

const $ = (id) => document.getElementById(id);
// for testing: ?cue=<url of a .cue next to its .bin files> starts at once (at the end)
const testCue = new URLSearchParams(location.search).get('cue');
let machine = null; // the game, once started
let settingsOpen = false;
let lastPlayer = null;

// --- the name: the game's own title, turning on the first screen: from
// title-data.json if the page has it (port/tools/make-title.mjs), else made
// from the player's disc once it is known ---

let title = null; // the TitleRing
function showTitle(data) {
  if (title || !data || !data.frames || !data.frames.length) return;
  title = new TitleRing($('game-logo'), data.frames);
  $('game-logo').hidden = false;
}
// title-data.json's frames, in base64
const fromBase64 = (s) => new Int32Array(Uint8Array.from(atob(s), (c) => c.charCodeAt(0)).buffer);
fetch('title-data.json')
  .then((r) => (r.ok ? r.json() : null))
  .then((d) => d && showTitle({ frames: d.frames.map(fromBase64) }))
  .catch(() => {});

// --- the settings ---

const settings = (() => {
  try {
    return { picture: 'high', music: 'cd', sounds: 'bright', ...JSON.parse(localStorage.getItem('vib-settings')) };
  } catch {
    return { picture: 'high', music: 'cd', sounds: 'bright' };
  }
})();
function saveSettings() {
  try {
    localStorage.setItem('vib-settings', JSON.stringify(settings));
  } catch {}
}

const canvas = $('screen');
const renderer = new Renderer(canvas);
const players = {}; // the examples' players, by setting

// each setting: what it does when chosen
const APPLY = {
  picture: (v) => (renderer.highResolution = v === 'high'),
  music: (v) => machine && machine.setHqMusic(v === 'cd'),
  sounds: (v) => machine && machine.setGaussian(v === 'playstation'),
};
function choose(setting, value) {
  settings[setting] = value;
  saveSettings();
  APPLY[setting](value);
  for (const b of document.querySelectorAll(`.choice[data-setting="${setting}"] button`)) {
    b.setAttribute('aria-checked', String(b.dataset.value === value));
    b.tabIndex = b.dataset.value === value ? 0 : -1;
  }
  if (players[setting]) players[setting].switched();
}
for (const group of document.querySelectorAll('.choice')) {
  const setting = group.dataset.setting;
  group.addEventListener('click', (e) => {
    const b = e.target.closest('button');
    if (b) choose(setting, b.dataset.value);
  });
  // the arrow keys move between the two ways, as in a radio group
  group.addEventListener('keydown', (e) => {
    if (!['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.code)) return;
    e.preventDefault();
    const buttons = [...group.querySelectorAll('button')];
    const next = buttons.find((b) => b.dataset.value !== settings[setting]);
    choose(setting, next.dataset.value);
    next.focus();
  });
}
for (const setting of Object.keys(APPLY)) choose(setting, settings[setting]);

let frames = 0;
function present(rgba, w, h, frame) {
  if (++frames % 500 === 0 && testCue) console.log(`frame ${frames}`);
  renderer.present(rgba, w, h, frame);
}

// --- the sound ---

let audioCtx = null;
let audioNode = null;
let pushed = 0;
let played = 0;
async function startAudio() {
  audioCtx = new AudioContext({ sampleRate: 44100, latencyHint: 'interactive' });
  await audioCtx.audioWorklet.addModule('audio-worklet.js');
  audioNode = new AudioWorkletNode(audioCtx, 'vib-audio', { outputChannelCount: [2] });
  audioNode.port.onmessage = (e) => (played = e.data);
  audioNode.connect(audioCtx.destination);
  await audioCtx.resume();
}
function audio(samples, frames) {
  if (!audioNode) return;
  // (a long way behind, as after the tab was hidden: let the sound start again from now)
  if (pushed - played > 44100) return;
  const f = new Float32Array(frames * 2);
  for (let i = 0; i < frames * 2; i++) f[i] = samples[i] / 32768;
  audioNode.port.postMessage(f, [f.buffer]);
  pushed += frames;
}
const audioQueued = () => (audioNode ? Math.max(0, pushed - played) : 4096);

// --- the menus by mouse (menus.js) ---

const menuDriver = new MenuDriver();
window.vibMenu = menuDriver; // (for testing the menus from outside)
// the mouse's place in the picture (512 x 288)
function gamePoint(e) {
  const r = canvas.getBoundingClientRect();
  return [((e.clientX - r.left) / r.width) * 512, ((e.clientY - r.top) / r.height) * 288];
}
// an item under the mouse only changes the pointer (with the game's cursor moved there, the menus that turn would turn under the mouse)
canvas.addEventListener('mousemove', (e) => {
  canvas.style.cursor = menuDriver.itemAt(...gamePoint(e)) ? 'pointer' : '';
});
canvas.addEventListener('click', (e) => menuDriver.click(...gamePoint(e)));
canvas.addEventListener('wheel', (e) => {
  if (!menuDriver.current) return;
  e.preventDefault();
  menuDriver.press(e.deltaY > 0 ? 'down' : 'up');
}, { passive: false });
canvas.addEventListener('contextmenu', (e) => {
  e.preventDefault();
  menuDriver.back();
});

// --- the controller: keyboard and gamepads ---

const BUTTON = {
  select: 0x0001, start: 0x0008, up: 0x0010, right: 0x0020, down: 0x0040, left: 0x0080,
  l2: 0x0100, r2: 0x0200, l1: 0x0400, r1: 0x0800, triangle: 0x1000, circle: 0x2000, cross: 0x4000, square: 0x8000,
};
// each button's keys (three at most), changed in the settings
const USUAL_KEYS = {
  up: ['ArrowUp', 'KeyW'], down: ['ArrowDown', 'KeyS'], left: ['ArrowLeft'], right: ['ArrowRight'],
  cross: ['KeyX', 'Space', 'Enter'], l1: ['KeyA'], r1: ['KeyD'], circle: ['KeyC', 'Backspace'], triangle: ['KeyV'], square: ['KeyZ'],
  l2: ['KeyQ'], r2: ['KeyE'], start: ['KeyP'], select: ['ShiftLeft', 'ShiftRight'],
};
const BUTTON_NAMES = [
  ['up', 'Up'], ['down', 'Down: pits'], ['left', 'Left'], ['right', 'Right'],
  ['cross', '✕: waves'], ['l1', 'L1: blocks'], ['r1', 'R1: loops'],
  ['circle', '○'], ['triangle', '△'], ['square', '□'], ['l2', 'L2'], ['r2', 'R2'],
  ['start', 'Start: pause'], ['select', 'Select'],
];
let buttonKeys = { ...USUAL_KEYS, ...(settings.keys || {}) };
let KEYS = {};
function mapKeys() {
  KEYS = {};
  for (const [button, codes] of Object.entries(buttonKeys)) for (const code of codes) KEYS[code] = button;
}
mapKeys();

// the keys in the settings: a button for each, clicked to listen for the new one
const KEY_LABELS = {
  ArrowUp: '↑', ArrowDown: '↓', ArrowLeft: '←', ArrowRight: '→', ShiftLeft: 'Left Shift', ShiftRight: 'Right Shift',
  ControlLeft: 'Left Ctrl', ControlRight: 'Right Ctrl', AltLeft: 'Left Alt', AltRight: 'Right Alt',
};
const keyLabel = (code) => KEY_LABELS[code] || code.replace(/^(Key|Digit)/, '').replace(/^Numpad/, 'Num ');
let listening = null; // { button, index }: the key being changed
function showKeys() {
  const root = $('keymap');
  root.innerHTML = '';
  for (const [button, name] of BUTTON_NAMES) {
    const label = document.createElement('span');
    label.textContent = name;
    const keys = document.createElement('span');
    keys.className = 'keys-of';
    const codes = buttonKeys[button] || [];
    for (let i = 0; i < Math.min(codes.length + 1, 3); i++) {
      const code = codes[i];
      const here = listening && listening.button === button && listening.index === i;
      const b = document.createElement('button');
      b.type = 'button';
      b.className = `key${code ? '' : ' empty'}${here ? ' listening' : ''}`;
      b.textContent = here ? 'Press a key' : code ? keyLabel(code) : '+';
      b.setAttribute('aria-label', here ? `${name}: press the new key` : code ? `${name}: ${keyLabel(code)}. Change it` : `${name}: add a key`);
      b.addEventListener('click', () => {
        listening = { button, index: i };
        showKeys();
        root.querySelector('.listening').focus();
      });
      keys.appendChild(b);
    }
    root.append(label, keys);
  }
}
// the key pressed while listening (null: the key cleared)
function setKey(code) {
  const { button, index } = listening;
  listening = null;
  const codes = [...(buttonKeys[button] || [])];
  if (code) {
    codes[index] = code;
    // (a key for one button only)
    for (const b of Object.keys(buttonKeys)) if (b !== button) buttonKeys[b] = buttonKeys[b].filter((c) => c !== code);
  } else {
    codes.splice(index, 1);
  }
  buttonKeys[button] = [...new Set(codes.filter(Boolean))].slice(0, 3);
  settings.keys = buttonKeys;
  saveSettings();
  mapKeys();
  showKeys();
}
showKeys();
$('keys-reset').addEventListener('click', () => {
  buttonKeys = structuredClone(USUAL_KEYS);
  delete settings.keys;
  saveSettings();
  listening = null;
  mapKeys();
  showKeys();
});
const held = new Set();
// keys go to the game unless a box of the page has them
const pageHasKeys = (e) => e.target.closest && e.target.closest('button, input, label, .alert-layer:not([hidden])') && e.target !== canvas;
addEventListener('keydown', (e) => {
  if (listening) {
    e.preventDefault();
    e.stopPropagation();
    if (e.code === 'Escape') {
      listening = null;
      showKeys();
    } else {
      setKey(e.code === 'Delete' ? null : e.code);
    }
    return;
  }
  if (e.code === 'Escape') {
    if (settingsOpen) closeSettings();
    else if (menuDriver.current) menuDriver.back();
    else return;
    e.preventDefault();
    return;
  }
  if (settingsOpen) {
    // the examples: Space plays or pauses the one last used
    const player = lastPlayer && players[lastPlayer];
    if (player && e.code === 'Space' && !e.target.closest('button, input')) {
      e.preventDefault();
      player.sources ? player.stop() : player.play(player.offset);
    }
    return;
  }
  const b = KEYS[e.code];
  if (!b || !machine || pageHasKeys(e)) return;
  held.add(b);
  e.preventDefault();
});
addEventListener('keyup', (e) => {
  const b = KEYS[e.code];
  if (b) held.delete(b);
});
addEventListener('blur', () => held.clear());
// the standard gamepad's buttons, in its order
const PAD = ['cross', 'circle', 'square', 'triangle', 'l1', 'r1', 'l2', 'r2', 'select', 'start', null, null, 'up', 'down', 'left', 'right'];
function pad() {
  let bits = menuDriver.step();
  for (const b of held) bits |= BUTTON[b];
  for (const gp of navigator.getGamepads ? navigator.getGamepads() : []) {
    if (!gp) continue;
    gp.buttons.forEach((button, i) => {
      if (button.pressed && PAD[i]) bits |= BUTTON[PAD[i]];
    });
    const [x, y] = gp.axes;
    if (x < -0.5) bits |= BUTTON.left;
    if (x > 0.5) bits |= BUTTON.right;
    if (y < -0.5) bits |= BUTTON.up;
    if (y > 0.5) bits |= BUTTON.down;
  }
  return bits;
}

// --- discs ---

// an audio CD of the chosen music files, in the order chosen
async function musicDiscFrom(files, progress) {
  const decodeCtx = audioCtx || new AudioContext({ sampleRate: 44100 });
  const decode = async (file) => {
    const buffer = await decodeCtx.decodeAudioData(await file.arrayBuffer());
    const left = buffer.getChannelData(0);
    const right = buffer.numberOfChannels > 1 ? buffer.getChannelData(1) : left;
    const out = new Int16Array(buffer.length * 2);
    for (let i = 0; i < buffer.length; i++) {
      out[i * 2] = Math.max(-32768, Math.min(32767, Math.round(left[i] * 32767)));
      out[i * 2 + 1] = Math.max(-32768, Math.min(32767, Math.round(right[i] * 32767)));
    }
    return out;
  };
  const tracks = [];
  for (const [i, file] of files.entries()) {
    progress(`Reading ${file.name}, ${i + 1} of ${files.length}`);
    let samples;
    try {
      samples = await decode(file);
    } catch (err) {
      console.warn(`${file.name}:`, err);
      progress(`Skipped ${file.name}: this browser cannot play it.`);
      continue;
    }
    tracks.push(new PcmTrack(samples.length / 2, () => decode(file), file.name));
  }
  if (!tracks.length) throw new Error('None of these files can be played. Choose songs this browser can play.');
  return audioDisc(tracks.slice(0, 99), tracks.length === 1 ? tracks[0].name || 'your music' : `your music (${tracks.length} tracks)`);
}

let gameDisc = null;
let gameFiles = null; // the disc image's files, for the examples

function status(text) {
  $('status').textContent = text;
}

function driveText() {
  if (!machine) return 'The game has not started.';
  if (machine.lid === 1) return 'The drive is open.';
  const disc = machine.lid === 0 ? machine.disc : null;
  return disc ? `In the drive: ${disc === gameDisc ? 'the Vib-Ribbon disc' : disc.label}.` : 'The drive is empty.';
}

// --- the disc image kept in the browser (IndexedDB), for the next visit ---

function discStore(mode, use) {
  return new Promise((resolve, reject) => {
    const open = indexedDB.open('vib-ribbon', 1);
    open.onupgradeneeded = () => open.result.createObjectStore('disc');
    open.onerror = () => reject(open.error);
    open.onsuccess = () => {
      const tx = open.result.transaction('disc', mode);
      const request = use(tx.objectStore('disc'));
      tx.oncomplete = () => resolve(request && request.result);
      tx.onerror = tx.onabort = () => reject(tx.error);
    };
  });
}
const rememberDisc = (files) => discStore('readwrite', (s) => s.put(files, 'game'));
const rememberedDisc = () => discStore('readonly', (s) => s.get('game'));
const forgetDisc = () => discStore('readwrite', (s) => s.delete('game'));
// the title's lines made from the disc (the same for every copy of the game), kept too
const keptTitle = () => discStore('readonly', (s) => s.get('title'));
const keepTitle = (title) => discStore('readwrite', (s) => s.put(title, 'title'));

async function makeTitle() {
  if (title) return;
  const { logo } = await run(gameFiles, 'logo');
  showTitle(logo);
  await keepTitle(logo).catch(() => {});
}
keptTitle().then(showTitle, () => {});

function showKept(kept) {
  $('forget').hidden = !kept;
  $('forget-2').hidden = !kept;
}

async function useDisc(files, kept) {
  gameDisc = await discFromFiles(files);
  gameFiles = files.filter((f) => /\.(cue|bin|img)$/i.test(f.name));
  status(kept ? 'Your disc is in, kept from last time.' : 'Your disc is in.');
  $('start').disabled = false;
  showKept(kept);
  $('start').focus();
  if (!title) makeTitle().catch((err) => console.warn('the title could not be made:', err.message));
}

async function discChosen(files) {
  if (!files.length) return;
  try {
    status('Reading the disc image…');
    await useDisc(files, false);
    if ($('remember').checked) {
      status('Your disc is in. Keeping it in this browser…');
      if (navigator.storage && navigator.storage.persist) await navigator.storage.persist();
      await rememberDisc(gameFiles);
      status('Your disc is in, and kept for next time.');
      showKept(true);
    }
  } catch (err) {
    status(err.message);
  }
}
$('disc-files').addEventListener('change', (e) => discChosen([...e.target.files]));

// the files dropped on the page
const setupBox = $('setup');
addEventListener('dragover', (e) => {
  if ($('setup-layer').hidden) return;
  e.preventDefault();
  setupBox.classList.add('dragging');
});
addEventListener('dragleave', (e) => {
  if (!e.relatedTarget) setupBox.classList.remove('dragging');
});
addEventListener('drop', (e) => {
  if ($('setup-layer').hidden) return;
  e.preventDefault();
  setupBox.classList.remove('dragging');
  discChosen([...e.dataTransfer.files]);
});

async function forget() {
  await forgetDisc();
  showKept(false);
  status('Forgotten. Next time, choose the disc image again.');
}
$('forget').addEventListener('click', forget);
$('forget-2').addEventListener('click', forget);

// a disc image kept from before
if (!testCue) {
  rememberedDisc()
    .then((files) => (files && files.length ? useDisc(files, true) : null))
    .catch(() => {});
}

$('start').addEventListener('click', async () => {
  $('setup-layer').hidden = true;
  await startAudio();
  machine = new Machine({
    gameDisc,
    load: async (name) => (await import(`./build/${name}.js`)).default,
    present,
    audio,
    audioQueued,
    pad,
    menu: (m) => menuDriver.report(m),
    model: (id, name) => menuDriver.model(id, name),
    object: (id, object, x0, y0, x1, y1) => menuDriver.object(id, object, x0, y0, x1, y1),
    record: { load: loadRecord, save: saveRecord },
  });
  window.vibMachine = machine; // (for testing)
  machine.setHqMusic(settings.music === 'cd');
  machine.setGaussian(settings.sounds === 'playstation');
  $('game-disc').disabled = false;
  setInterval(watchScreens, 200);
  machine.run().catch((err) => {
    console.error(err);
    $('setup-layer').hidden = false;
    status(`The game stopped: ${err.message}`);
  });
  canvas.focus();
});

// --- the game asking for a disc: its alerts, answered in boxes over the picture ---

const SCREEN_ALERT_VIB_CD_IN = 7;
const SCREEN_ALERT_AUDIO_CD_IN = 8;
let readingMusic = false;
function watchScreens() {
  const m = menuDriver.current;
  const screen = m ? m.screen : 0;
  const closed = machine.lid === 0 || machine.lid === 2;
  $('music-layer').hidden = !(readingMusic || (screen === SCREEN_ALERT_AUDIO_CD_IN && closed));
  $('vib-layer').hidden = !(screen === SCREEN_ALERT_VIB_CD_IN && closed);
  $('in-drive').textContent = driveText();
}

async function insertMusic(input, say) {
  const files = [...input.files];
  input.value = '';
  if (!files.length || !machine) return;
  readingMusic = true;
  try {
    const disc = await musicDiscFrom(files, say);
    say(`Putting in ${disc.label}…`);
    await machine.swapDisc(disc);
    say('MP3, FLAC, WAV, OGG or anything else your browser plays.');
  } catch (err) {
    say(err.message);
  } finally {
    readingMusic = false;
  }
}
$('music-files').addEventListener('change', (e) => insertMusic(e.target, (t) => ($('music-status').textContent = t)));
$('music-files-2').addEventListener('change', (e) => insertMusic(e.target, (t) => ($('in-drive').textContent = t)));
$('music-back').addEventListener('click', () => menuDriver.press('triangle'));
$('vib-back').addEventListener('click', () => menuDriver.press('triangle'));
$('vib-in').addEventListener('click', () => machine.swapDisc(gameDisc));
$('game-disc').addEventListener('click', () => machine && machine.swapDisc(gameDisc));

// --- the game's record (its high scores and options), kept in this browser ---

function loadRecord() {
  try {
    const kept = JSON.parse(localStorage.getItem('vib-record'));
    return kept ? Uint8Array.from(atob(kept.bytes), (c) => c.charCodeAt(0)) : null;
  } catch {
    return null;
  }
}
let recordForgotten = false; // (until the page is reloaded, the game's still in memory)
function saveRecord(bytes) {
  if (recordForgotten) return;
  try {
    localStorage.setItem('vib-record', JSON.stringify({ bytes: btoa(String.fromCharCode(...bytes)), at: Date.now() }));
  } catch {}
  showRecord();
}
function showRecord() {
  const kept = loadRecord();
  // the best of each of the game's own levels (GameParams.h: the courses' high scores, Bronze, Silver and Gold first)
  const top = (course) => new DataView(kept.buffer).getInt32(4 + course * 12, true);
  $('record-note').textContent = kept && kept.length >= 32 ? `Best now: Bronze ${top(0)}, Silver ${top(1)}, Gold ${top(2)}.` : '';
  $('record-forget').hidden = !kept;
}
showRecord();
$('record-forget').addEventListener('click', () => {
  try {
    localStorage.removeItem('vib-record');
  } catch {}
  showRecord();
  if (machine) {
    recordForgotten = true;
    $('record-note').textContent = 'Forgotten: the game has them until this page is reloaded.';
  }
});

// --- the settings sheet; the game waits while it is open ---

function openSettings() {
  settingsOpen = true;
  held.clear();
  $('settings').classList.add('open');
  $('settings').setAttribute('aria-hidden', 'false');
  $('scrim').hidden = false;
  $('open-settings').setAttribute('aria-expanded', 'true');
  $('in-drive').textContent = driveText();
  if (machine) {
    machine.host.paused = true;
    $('paused-note').hidden = false;
    if (audioCtx) audioCtx.suspend();
  }
  $('close-settings').focus();
  showExamples();
}
function closeSettings() {
  settingsOpen = false;
  for (const p of Object.values(players)) p.stop();
  $('settings').classList.remove('open');
  $('settings').setAttribute('aria-hidden', 'true');
  $('scrim').hidden = true;
  $('open-settings').setAttribute('aria-expanded', 'false');
  $('paused-note').hidden = true;
  if (machine) {
    machine.host.paused = false;
    if (audioCtx) audioCtx.resume();
  }
  (machine ? canvas : $('open-settings')).focus();
}
$('open-settings').addEventListener('click', () => (settingsOpen ? closeSettings() : openSettings()));
$('close-settings').addEventListener('click', closeSettings);
$('scrim').addEventListener('click', closeSettings);

// the examples, made from the player's disc the first time the settings open with a disc in
let examples = null;
let wipe = null; // the picture's example
function exampleNote(text) {
  for (const k of ['picture', 'music', 'sounds']) {
    if (!players[k] && !(k === 'picture' && wipe)) $(`${k}-example`).innerHTML = `<p class="status">${text}</p>`;
  }
}
function showExamples() {
  if (examples) return;
  if (!gameFiles) {
    exampleNote('Put your disc in and an example appears here, made from it.');
    return;
  }
  exampleNote('Making an example from your disc…');
  examples = makeExamples(gameFiles, (done, of) => exampleNote(`Making an example from your disc: ${done} of ${of} recordings ready…`))
    .then((made) => {
      wipe = new Wipe(Object.assign($('picture-example'), { className: 'example wipe' }), made.picture);
      players.music = new AbPlayer(Object.assign($('music-example'), { className: 'example ab' }), made.music, () => settings.music);
      players.sounds = new AbPlayer(Object.assign($('sounds-example'), { className: 'example ab' }), made.sounds, () => settings.sounds);
      lastPlayer = 'sounds';
      for (const k of ['music', 'sounds']) {
        $(`${k}-example`).addEventListener('pointerdown', () => (lastPlayer = k));
        $(`${k}-example`).addEventListener('focusin', () => (lastPlayer = k));
        const hint = document.createElement('p');
        hint.className = 'note';
        hint.textContent = 'Both ways play at once: switch while it plays to hear the very same moment. Drag across the line to repeat another stretch.';
        $(`${k}-example`).after(hint);
      }
    })
    .catch((err) => {
      examples = null;
      exampleNote(`The example could not be made: ${err.message}`);
    });
}

// --- full screen ---

$('fullscreen').addEventListener('click', () => {
  if (document.fullscreenElement) document.exitFullscreen();
  else $('screen-box').requestFullscreen().then(() => canvas.focus(), () => {});
});

// ?cue=: the disc image fetched from there, and the game started
if (testCue) {
  (async () => {
    const base = new URL(testCue, location.href);
    const text = await (await fetch(base)).text();
    const names = [...text.matchAll(/^\s*FILE\s+"?(.*?)"?\s+\S+\s*$/gim)].map((m) => m[1]);
    const files = [new File([text], decodeURIComponent(base.pathname.split('/').pop()))];
    for (const name of names) files.push(new File([await (await fetch(new URL(name, base))).blob()], name));
    await useDisc(files, false);
    $('start').click();
  })().catch((err) => status(err.message));
}
