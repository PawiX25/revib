// Runs the port in Node, without a page: as fast as it goes, with pictures
// of the screen saved as PNG files, and buttons pressed on given frames.
//
//   node port/tools/run.mjs --cue "disc.cue" [--frames 3000] [--shots 100,200,...]
//        [--every 250] [--out dir] [--keys 400:start,500:cross+right,...] [--music a.wav,b.wav]
//        [--wav out.wav] [--hq 0] [--levels 1] [--objects 1]
//
// Frames are counted from the start, across the executables.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { Machine, Disc, MemorySource, ChunkedSource, PcmTrack, audioDisc } from '../web/vib.js';

const args = {};
for (let i = 2; i < process.argv.length; i += 2) args[process.argv[i].replace(/^--/, '')] = process.argv[i + 1];
const cuePath = args.cue;
const maxFrames = Number(args.frames || 3000);
const out = args.out || 'build/port/shots';
const every = Number(args.every || 0);
const shots = new Set((args.shots || '').split(',').filter(Boolean).map(Number));
fs.mkdirSync(out, { recursive: true });

const BUTTONS = {
  select: 0x0001, start: 0x0008, up: 0x0010, right: 0x0020, down: 0x0040, left: 0x0080,
  l2: 0x0100, r2: 0x0200, l1: 0x0400, r1: 0x0800, triangle: 0x1000, circle: 0x2000, cross: 0x4000, square: 0x8000,
};
// "frame:button+button[:length]": held for `length` frames (6 by default)
const presses = (args.keys || '').split(',').filter(Boolean).map((k) => {
  const [frame, buttons, length] = k.split(':');
  return { frame: Number(frame), bits: buttons.split('+').reduce((b, n) => b | BUTTONS[n], 0), length: Number(length || 6) };
});

function png(rgba, w, h) {
  const rows = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) {
    rows[y * (w * 4 + 1)] = 0;
    Buffer.from(rgba.buffer, rgba.byteOffset + y * w * 4, w * 4).copy(rows, y * (w * 4 + 1) + 1);
  }
  const crcTable = [];
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crcTable[n] = c >>> 0;
  }
  const crc = (buf) => {
    let c = 0xffffffff;
    for (const b of buf) c = crcTable[(c ^ b) & 255] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  };
  const chunk = (type, data) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const c = Buffer.alloc(4);
    c.writeUInt32BE(crc(td));
    return Buffer.concat([len, td, c]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(rows)), chunk('IEND', Buffer.alloc(0))]);
}

const dir = path.dirname(cuePath);
const openBin = (name) => {
  const p = path.join(dir, name);
  const size = fs.statSync(p).size;
  // the data track whole, the audio ones in chunks
  if (size < 64 * 1024 * 1024 && /Track 1\)|\.iso$|^[^()]*\.bin$/i.test(name)) return new MemorySource(new Uint8Array(fs.readFileSync(p)));
  const fd = fs.openSync(p, 'r');
  return new ChunkedSource(size, async (offset, length) => {
    const buf = Buffer.alloc(length);
    fs.readSync(fd, buf, 0, length, offset);
    return new Uint8Array(buf.buffer, buf.byteOffset, length);
  });
};
const gameDisc = Disc.fromCue(fs.readFileSync(cuePath, 'utf8'), openBin, 'Vib-Ribbon');

let frame = 0;
let audioFrames = 0;
let level = 0;
let levelCount = 0;
const recorded = [];
const modelNames = {};
let drawnObjects = [];
let lastMenu = null;

function finish() {
  if (args.wav) {
    // --wav out.wav [--wav-from frame]: the sound from that frame on, 16-bit stereo 44100 Hz
    const data = Buffer.concat(recorded.map((s) => Buffer.from(s.buffer, s.byteOffset, s.byteLength)));
    const head = Buffer.alloc(44);
    head.write('RIFF', 0);
    head.writeUInt32LE(36 + data.length, 4);
    head.write('WAVEfmt ', 8);
    head.writeUInt32LE(16, 16);
    head.writeUInt16LE(1, 20);
    head.writeUInt16LE(2, 22);
    head.writeUInt32LE(44100, 24);
    head.writeUInt32LE(44100 * 4, 28);
    head.writeUInt16LE(4, 32);
    head.writeUInt16LE(16, 34);
    head.write('data', 36);
    head.writeUInt32LE(data.length, 40);
    fs.writeFileSync(args.wav, Buffer.concat([head, data]));
  }
  console.log(`done: ${frame} frames, ${(audioFrames / 44100).toFixed(1)} s of sound`);
  process.exit(0);
}
const machine = new Machine({
  gameDisc,
  fast: true,
  load: async (name) => (await import(path.resolve('port/web/build', `${name}.js`))).default,
  log: (t) => process.stdout.write(`[${frame}] ${t}${t.endsWith('\n') ? '' : '\n'}`),
  // --objects 1: next to each picture, where the models' objects were drawn (for the mouse's menus)
  model: (id, name) => (modelNames[id] = name),
  object: (id, object, x0, y0, x1, y1) => drawnObjects.push({ model: modelNames[id], object, box: [x0 + 256, y0 + 144, x1 + 256, y1 + 144] }),
  menu: (m) => (lastMenu = m),
  audio: (samples, frames) => {
    if (args.wav && frame >= Number(args['wav-from'] || 0)) recorded.push(Int16Array.from(samples));
    audioFrames += frames;
    for (let i = 0; i < frames * 2; i++) level += Math.abs(samples[i]);
    levelCount += frames * 2;
    if (args.levels && audioFrames % (44100 * 2) < frames) {
      console.log(`[${frame}] sound level ${(level / levelCount).toFixed(0)}`);
      level = levelCount = 0;
    }
  },
  pad: () => presses.filter((p) => frame >= p.frame && frame < p.frame + p.length).reduce((b, p) => b | p.bits, 0),
  present: (rgba, w, h) => {
    frame++;
    if (shots.has(frame) || (every && frame % every === 0)) {
      fs.writeFileSync(path.join(out, `f${String(frame).padStart(6, '0')}.png`), png(rgba, w, h));
      if (args.objects) fs.writeFileSync(path.join(out, `f${String(frame).padStart(6, '0')}.json`), JSON.stringify({ menu: lastMenu, objects: drawnObjects }));
    }
    drawnObjects = [];
    if (frame >= maxFrames) finish();
  },
});

// --music a.wav,b.wav: an audio CD of 16-bit 44100 Hz stereo WAV files, put in at --swap frame
if (args.music) {
  const tracks = args.music.split(',').map((f) => {
    const buf = fs.readFileSync(f);
    const data = buf.indexOf('data') + 8;
    const samples = new Int16Array(buf.buffer.slice(buf.byteOffset + data, buf.byteOffset + buf.length - ((buf.length - data) % 4)));
    return new PcmTrack(samples.length / 2, async () => samples, path.basename(f));
  });
  const swapAt = Number(args.swap || 600);
  const timer = setInterval(() => {
    if (frame >= swapAt) {
      clearInterval(timer);
      machine.swapDisc(audioDisc(tracks), 200);
    }
  }, 5);
}

// --hq 0: the music as the PlayStation played it
machine.setHqMusic(args.hq !== '0');

machine.run().catch((e) => {
  console.error(e);
  process.exit(1);
});
