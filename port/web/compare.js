// The settings' before-and-after examples: made from the player's disc by
// compare-worker.js, then heard (two recordings playing together, one of them
// audible) and seen (one frame drawn both ways, a divider between them).

import { Renderer } from './renderer.js';

// one job of compare-worker.js, in a worker of its own
export function run(files, job, setting, picture) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL('./compare-worker.js', import.meta.url), { type: 'module' });
    worker.onmessage = ({ data }) => {
      worker.terminate();
      if (data.error) reject(new Error(data.error));
      else resolve(data);
    };
    worker.onerror = (e) => {
      worker.terminate();
      reject(new Error(e.message || 'The examples could not be made.'));
    };
    worker.postMessage({ job, files, setting, picture });
  });
}

// the four recordings, made side by side; `progress(done, of)` after each
export async function makeExamples(files, progress) {
  let done = 0;
  const step = (p) => p.then((r) => (progress(++done, 4), r));
  const [bright, gauss, cd, ps] = await Promise.all([
    step(run(files, 'sounds', 'bright')),
    step(run(files, 'sounds', 'playstation')),
    step(run(files, 'music', 'cd', 23)),
    step(run(files, 'music', 'playstation')),
  ]);
  return {
    sounds: { bright: bright.samples, playstation: gauss.samples },
    music: { cd: cd.samples, playstation: ps.samples },
    picture: cd.picture,
  };
}

// --- heard ---

let context = null;
let playing = null; // the player playing
const audioContext = () => (context ||= new AudioContext({ sampleRate: 44100 }));

function toBuffer(samples) {
  const n = samples.length / 2;
  const buffer = audioContext().createBuffer(2, n, 44100);
  const left = buffer.getChannelData(0);
  const right = buffer.getChannelData(1);
  for (let i = 0; i < n; i++) {
    left[i] = samples[i * 2] / 32768;
    right[i] = samples[i * 2 + 1] / 32768;
  }
  return buffer;
}

const time = (t) => `${Math.floor(t / 60)}:${(t % 60).toFixed(1).padStart(4, '0')}`;

// Two recordings of one stretch of the game, sample for sample alike but for
// one setting: both play at once, and the setting chosen (`which()`) is the
// one heard, so switching it while listening compares the very same moment.
export class AbPlayer {
  constructor(root, recordings, which, { from = 23, to = 35 } = {}) {
    this.root = root;
    this.which = which;
    this.buffers = Object.fromEntries(Object.entries(recordings).map(([k, s]) => [k, toBuffer(s)]));
    this.duration = Object.values(this.buffers)[0].duration;
    this.from = from;
    this.to = to;
    this.offset = from;
    this.sources = null;
    root.innerHTML = `
      <button class="ab-play" type="button" aria-label="Play">
        <svg viewBox="0 0 20 20" aria-hidden="true"><path class="ab-play-icon" d="M6 4 L16 10 L6 16 Z"/></svg>
      </button>
      <canvas class="ab-wave" aria-label="The recording: click to jump, drag to choose a stretch to repeat"></canvas>
      <span class="ab-time"></span>
      <button class="ab-whole" type="button">All ${Math.round(this.duration)} seconds</button>`;
    this.canvas = root.querySelector('.ab-wave');
    this.button = root.querySelector('.ab-play');
    this.button.addEventListener('click', () => (this.sources ? this.stop() : this.play(this.offset)));
    root.querySelector('.ab-whole').addEventListener('click', () => {
      this.from = 0;
      this.to = this.duration;
      this.restart(this.position());
    });
    this.peaks = this.makePeaks();
    this.drag(this.canvas);
    new ResizeObserver(() => this.draw()).observe(this.canvas);
  }

  makePeaks() {
    const d = Object.values(this.buffers)[0].getChannelData(0);
    const n = 600;
    const peaks = new Float32Array(n);
    const step = Math.floor(d.length / n);
    for (let i = 0; i < n; i++) {
      let m = 0;
      for (let j = i * step; j < (i + 1) * step; j += 8) m = Math.max(m, Math.abs(d[j]));
      peaks[i] = m;
    }
    return peaks;
  }

  // the place in the recording, looping over the stretch
  position() {
    if (!this.sources) return this.offset;
    const t = this.offset + (context.currentTime - this.startedAt);
    return t >= this.to ? this.from + ((t - this.from) % (this.to - this.from)) : t;
  }

  async play(at) {
    if (playing && playing !== this) playing.stop();
    await audioContext().resume();
    if (at < this.from || at >= this.to) at = this.from;
    const when = context.currentTime + 0.05;
    this.gains = {};
    this.sources = Object.entries(this.buffers).map(([k, buffer]) => {
      const source = context.createBufferSource();
      const gain = context.createGain();
      source.buffer = buffer;
      source.loop = true;
      source.loopStart = this.from;
      source.loopEnd = this.to;
      gain.gain.value = k === this.which() ? 1 : 0;
      source.connect(gain).connect(context.destination);
      source.start(when, at);
      this.gains[k] = gain;
      return source;
    });
    this.startedAt = when;
    this.offset = at;
    playing = this;
    this.root.classList.add('is-playing');
    this.button.setAttribute('aria-label', 'Pause');
    this.tick();
  }

  stop() {
    if (!this.sources) return;
    this.offset = this.position();
    for (const s of this.sources) s.stop();
    this.sources = null;
    if (playing === this) playing = null;
    this.root.classList.remove('is-playing');
    this.button.setAttribute('aria-label', 'Play');
    this.draw();
  }

  restart(at) {
    if (this.sources) {
      this.stop();
      this.play(at);
    } else {
      this.offset = Math.max(this.from, Math.min(at, this.to));
      this.draw();
    }
  }

  // the setting changed: hear the other recording, at once
  switched() {
    if (!this.sources) return;
    const t = context.currentTime;
    for (const [k, gain] of Object.entries(this.gains)) gain.gain.setTargetAtTime(k === this.which() ? 1 : 0, t, 0.004);
  }

  tick() {
    this.draw();
    if (this.sources) requestAnimationFrame(() => this.tick());
  }

  drag(canvas) {
    const at = (e) => {
      const r = canvas.getBoundingClientRect();
      return Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)) * this.duration;
    };
    let down = null;
    canvas.addEventListener('pointerdown', (e) => {
      down = at(e);
      canvas.setPointerCapture(e.pointerId);
    });
    canvas.addEventListener('pointermove', (e) => {
      if (down === null || Math.abs(at(e) - down) < 0.3) return;
      this.from = Math.min(down, at(e));
      this.to = Math.max(down, at(e));
      this.draw();
    });
    canvas.addEventListener('pointerup', (e) => {
      const t = at(e);
      if (Math.abs(t - down) >= 0.3) {
        this.restart(this.from);
      } else {
        // a click: from there (outside the stretch, the stretch is the whole)
        if (t < this.from || t > this.to) {
          this.from = 0;
          this.to = this.duration;
        }
        this.restart(t);
      }
      down = null;
    });
  }

  draw() {
    const c = this.canvas;
    const ratio = window.devicePixelRatio || 1;
    const w = Math.round(c.clientWidth * ratio);
    const h = Math.round(c.clientHeight * ratio);
    if (!w || !h) return;
    if (c.width !== w || c.height !== h) {
      c.width = w;
      c.height = h;
    }
    const g = c.getContext('2d');
    const style = getComputedStyle(this.root);
    const ink = style.getPropertyValue('--ribbon').trim() || '#fff';
    const dim = style.getPropertyValue('--ash').trim() || '#777';
    g.clearRect(0, 0, w, h);
    const x = (t) => (t / this.duration) * w;
    const whole = this.from <= 0 && this.to >= this.duration;
    // the recording's outline: one line above its middle and one below, the stretch in white
    for (const [part, color] of [['out', dim], ['in', ink]]) {
      g.strokeStyle = color;
      g.lineWidth = ratio;
      g.beginPath();
      for (const sign of [-1, 1]) {
        let started = false;
        for (let i = 0; i < this.peaks.length; i++) {
          const t = (i / this.peaks.length) * this.duration;
          const inside = whole || (t >= this.from && t <= this.to);
          if ((part === 'in') !== inside) {
            started = false;
            continue;
          }
          const px = (i / (this.peaks.length - 1)) * w;
          const py = h / 2 + sign * Math.max(0.5 * ratio, this.peaks[i] * h * 0.46);
          if (started) g.lineTo(px, py);
          else g.moveTo(px, py);
          started = true;
        }
      }
      g.stroke();
    }
    // the stretch's ends, as brackets
    if (!whole) {
      g.strokeStyle = ink;
      g.lineWidth = ratio;
      g.beginPath();
      for (const [t, d] of [[this.from, 1], [this.to, -1]]) {
        const bx = Math.round(x(t)) + 0.5;
        g.moveTo(bx + d * 5 * ratio, 0.5);
        g.lineTo(bx, 0.5);
        g.lineTo(bx, h - 0.5);
        g.lineTo(bx + d * 5 * ratio, h - 0.5);
      }
      g.stroke();
    }
    // where it plays
    const p = this.position();
    g.fillStyle = ink;
    g.fillRect(Math.round(x(p)) - ratio, 0, 2 * ratio, h);
    this.root.querySelector('.ab-time').textContent = time(p);
  }
}

// --- seen ---

// One frame of the game drawn both ways, whole, the sharp picture left of a
// divider: as the game itself draws it either way, at this size.
export class Wipe {
  constructor(root, frame) {
    this.root = root;
    this.frame = frame;
    root.innerHTML = `
      <canvas class="wipe-b"></canvas><canvas class="wipe-a"></canvas>
      <div class="wipe-line"></div>
      <span class="wipe-label wipe-label-a">Sharp</span><span class="wipe-label wipe-label-b">PlayStation</span>
      <input class="wipe-range" type="range" min="0" max="1000" value="500" aria-label="Divider between the sharp picture and the PlayStation's">`;
    this.sharp = new Renderer(root.querySelector('.wipe-a'));
    this.native = new Renderer(root.querySelector('.wipe-b'));
    this.native.highResolution = false;
    const range = root.querySelector('.wipe-range');
    range.addEventListener('input', () => this.place(range.value / 1000));
    root.addEventListener('pointerdown', (e) => {
      if (e.target === range) return;
      root.setPointerCapture(e.pointerId);
      const move = (ev) => {
        const b = root.getBoundingClientRect();
        this.place(Math.max(0, Math.min(1, (ev.clientX - b.left) / b.width)));
        range.value = Math.round(this.split * 1000);
      };
      move(e);
      root.addEventListener('pointermove', move);
      root.addEventListener('pointerup', () => root.removeEventListener('pointermove', move), { once: true });
    });
    this.place(0.5);
    new ResizeObserver(() => this.draw()).observe(root);
  }

  place(split) {
    this.split = split;
    this.root.style.setProperty('--split', `${split * 100}%`);
  }

  draw() {
    const f = this.frame;
    this.sharp.present(f.rgba, f.w, f.h, f);
    this.native.present(f.rgba, f.w, f.h, f);
  }
}
