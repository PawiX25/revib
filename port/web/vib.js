// The port's machine, shared by the page and the test runner: the discs (the
// game's, from a .cue and its .bin files, and audio CDs made of music files),
// the drive and its lid, and the loader, which runs the title, the game and
// the kiosk executables in turn with the LoaderData they hand on.

const SECTOR = 2352;
const SECTOR_SAMPLES = 588;

// --- byte sources: a .bin file, read whole or in chunks ---

const CHUNK = 256 * SECTOR; // 256 sectors, 3.4 s of CD audio

// a source read through `readAt(offset, length) -> Promise<Uint8Array>`, kept in chunks
export class ChunkedSource {
  constructor(size, readAt) {
    this.size = size;
    this.readAt = readAt;
    this.chunks = new Map();
    this.loading = new Map();
  }
  // the bytes, if their chunk is at hand
  get(offset, length) {
    const index = Math.floor(offset / CHUNK);
    const chunk = this.chunks.get(index);
    if (!chunk) return null;
    const start = offset - index * CHUNK;
    if (start + length > chunk.length) return null;
    // most recently used last
    this.chunks.delete(index);
    this.chunks.set(index, chunk);
    return chunk.subarray(start, start + length);
  }
  async load(offset) {
    const index = Math.floor(offset / CHUNK);
    if (this.chunks.has(index)) return;
    if (!this.loading.has(index)) {
      const start = index * CHUNK;
      const length = Math.min(CHUNK, this.size - start);
      this.loading.set(index, this.readAt(start, length).then((bytes) => {
        this.chunks.set(index, bytes);
        this.loading.delete(index);
        while (this.chunks.size > 64) this.chunks.delete(this.chunks.keys().next().value);
      }));
    }
    await this.loading.get(index);
  }
}

// a source wholly in memory
export class MemorySource {
  constructor(bytes) {
    this.bytes = bytes;
    this.size = bytes.length;
  }
  get(offset, length) {
    return offset + length <= this.size ? this.bytes.subarray(offset, offset + length) : null;
  }
  async load() {}
}

// --- .cue sheets ---

function msfToFrames(text) {
  const [m, s, f] = text.split(':').map(Number);
  return (m * 60 + s) * 75 + f;
}

function parseCue(text) {
  const files = [];
  let file = null;
  let track = null;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    let m;
    if ((m = line.match(/^FILE\s+"(.*)"\s+(\S+)/i)) || (m = line.match(/^FILE\s+(\S+)\s+(\S+)/i))) {
      file = { name: m[1], tracks: [] };
      files.push(file);
    } else if ((m = line.match(/^TRACK\s+(\d+)\s+(\S+)/i)) && file) {
      track = { number: Number(m[1]), mode: m[2].toUpperCase(), indexes: {} };
      file.tracks.push(track);
    } else if ((m = line.match(/^INDEX\s+(\d+)\s+(\d+:\d+:\d+)/i)) && track) {
      track.indexes[Number(m[1])] = msfToFrames(m[2]);
    }
  }
  return files;
}

// --- discs ---

// A disc: its tracks (start sector from 00:02:00, data or audio, where their
// sectors come from), and the lead-out.
export class Disc {
  constructor(tracks, leadOut, label) {
    // { firstSector (index 0), start (index 1), audio, and source, offset (bytes of `firstSector` in it), sectorSize, dataOffset; or pcm }
    this.tracks = tracks;
    this.leadOut = leadOut;
    this.label = label;
  }
  toc() {
    return [...this.tracks.map((t) => t.start), this.leadOut];
  }
  trackOf(lba) {
    for (let i = this.tracks.length - 1; i >= 0; i--) {
      if (lba >= this.tracks[i].firstSector) return this.tracks[i];
    }
    return this.tracks[0];
  }
  // a whole sector: Uint8Array(2352), null if not at hand (wait()), undefined if beyond the disc
  raw(lba) {
    if (lba < 0 || lba >= this.leadOut) return undefined;
    const t = this.trackOf(lba);
    if (t.pcm) return t.pcm.sector(lba - t.firstSector);
    return t.source.get(t.offset + (lba - t.firstSector) * t.sectorSize, t.sectorSize);
  }
  async wait(lba) {
    if (lba < 0 || lba >= this.leadOut) return;
    const t = this.trackOf(lba);
    if (t.pcm) return t.pcm.load();
    await t.source.load(t.offset + (lba - t.firstSector) * t.sectorSize);
  }
  // the 2048 bytes of data of a sector of the data track
  data(lba) {
    const t = this.tracks[0];
    if (t.audio || lba < 0) return null;
    const sector = t.source.get(t.offset + (lba - t.firstSector) * t.sectorSize, t.sectorSize);
    if (!sector) return null;
    return sector.subarray(t.dataOffset, t.dataOffset + 2048);
  }

  // `open(name) -> source` gives each .bin file of the sheet
  static fromCue(text, open, label = 'disc') {
    const files = parseCue(text);
    const tracks = [];
    const sizeOf = (mode) => (mode === 'AUDIO' ? SECTOR : Number(mode.split('/')[1] || SECTOR));
    let base = 0; // the sector of the start of the file
    for (const file of files) {
      const source = open(file.name);
      if (!source) throw new Error(`missing file: ${file.name}`);
      for (const track of file.tracks) {
        const sectorSize = sizeOf(track.mode);
        const index1 = track.indexes[1] ?? track.indexes[0] ?? 0;
        const index0 = track.indexes[0] ?? index1;
        tracks.push({
          number: track.number,
          audio: track.mode === 'AUDIO',
          source,
          sectorSize,
          firstSector: base + index0,
          start: base + index1,
          offset: index0 * sectorSize,
          dataOffset: track.mode.startsWith('MODE2') ? 24 : track.mode.startsWith('MODE1/2352') ? 16 : 0,
        });
      }
      base += Math.floor(source.size / (file.tracks.length ? sizeOf(file.tracks[0].mode) : SECTOR));
    }
    return new Disc(tracks, base, label);
  }
}

// An audio track of decoded music: interleaved 16-bit stereo, 44100 Hz,
// decoded when first needed (`decode() -> Promise<Int16Array>`), only a few at a time.
const decodedTracks = [];
export class PcmTrack {
  constructor(frames, decode, name) {
    this.frames = frames;
    this.decode = decode;
    this.name = name;
    this.samples = null;
    this.loading = null;
    this.sectors = Math.ceil(frames / SECTOR_SAMPLES);
  }
  sector(i) {
    if (!this.samples) return null;
    const out = new Uint8Array(SECTOR);
    const from = i * SECTOR_SAMPLES * 2;
    const part = this.samples.subarray(from, Math.min(from + SECTOR_SAMPLES * 2, this.samples.length));
    out.set(new Uint8Array(part.buffer, part.byteOffset, part.byteLength));
    return out;
  }
  async load() {
    if (this.samples) return;
    if (!this.loading) {
      this.loading = this.decode().then((samples) => {
        this.samples = samples;
        this.loading = null;
        decodedTracks.push(this);
        while (decodedTracks.length > 3) decodedTracks.shift().samples = null;
      });
    }
    await this.loading;
  }
}

// An audio CD of music: each track after two seconds of silence, as on a CD.
export function audioDisc(pcmTracks, label = 'music') {
  const GAP = 150;
  const silence = new PcmTrack(GAP * SECTOR_SAMPLES, async () => new Int16Array(GAP * SECTOR_SAMPLES * 2), 'gap');
  const tracks = [];
  let lba = 0;
  for (let i = 0; i < pcmTracks.length; i++) {
    const t = pcmTracks[i];
    if (i > 0) {
      tracks.push({ gap: true, audio: true, firstSector: lba, start: lba, pcm: silence });
      lba += GAP;
    }
    tracks.push({ number: i + 1, audio: true, firstSector: lba, start: lba, pcm: t });
    lba += t.sectors;
  }
  // the gaps belong to the tracks before them as far as the table of contents goes
  const disc = new Disc(tracks, lba, label);
  disc.toc = () => [...tracks.filter((t) => !t.gap).map((t) => t.start), lba];
  return disc;
}

// --- the machine ---

// LoaderData's params.game (GameParams.h's GameRecord): whether it is set, the
// high scores of the nine courses, the vibration and sound effects options.
// The console forgot it when switched off; the page may keep it (`record`).
const RECORD_AT = 4;
const RECORD_SIZE = 120;

export class Machine {
  // `gameDisc`: the game's disc; `load(name) -> Promise<factory>` gives a module's factory
  // `record`: { load() -> bytes or null, save(bytes) }, to keep the game's record from one visit to the next
  constructor({ gameDisc, load, present, audio, audioQueued, pad, log, menu = null, model = null, object = null, fast = false, record = null }) {
    this.record = record;
    this.gameDisc = gameDisc;
    this.disc = gameDisc; // in the drive
    this.lid = 0;         // 0 closed with a disc, 1 open, 2 closed and empty
    this.changes = 1;     // discs put in so far
    this.load = load;
    this.loaderData = null;
    this.module = null;
    this.hqMusic = true;
    this.gaussian = false;
    this.host = {
      fast,
      paused: false,
      present: present || (() => {}),
      audio: audio || (() => {}),
      audioQueued: audioQueued || (() => 0),
      pad: pad || (() => 0),
      log: log || ((t) => console.log(t)),
      menu,
      model,
      object,
      cdState: () => this.lid,
      cdChanges: () => this.changes,
      toc: () => (this.lid === 0 && this.disc ? this.disc.toc() : [0]),
      cdRead: (lba, dest) => {
        if (this.lid !== 0 || !this.disc) return 0;
        const s = this.disc.raw(lba);
        if (s === undefined) return 0;
        if (s === null) return -1;
        dest.set(s);
        return 1;
      },
      cdWait: async (lba) => {
        if (this.disc) await this.disc.wait(lba);
      },
      dataRead: (lba, dest) => {
        const s = this.gameDisc.data(lba);
        if (!s) return false;
        dest.set(s);
        return true;
      },
    };
  }

  // the music played as the CD has it (stereo, 44100 Hz), or as the console did (mono, 22050 Hz)
  setHqMusic(on) {
    this.hqMusic = on;
    if (this.module) this.module._port_set_hq_music(on ? 1 : 0);
  }

  // the sounds' interpolation: the PlayStation's (Gaussian) or linear
  setGaussian(on) {
    this.gaussian = on;
    if (this.module) this.module._port_set_gaussian(on ? 1 : 0);
  }

  // the lid opened, `disc` put in (null: none), the lid closed
  async swapDisc(disc, openMs = 1500) {
    this.lid = 1;
    await new Promise((r) => setTimeout(r, openMs));
    this.disc = disc;
    if (disc) {
      this.changes++;
      this.lid = 0;
    } else {
      this.lid = 2;
    }
  }

  // runs one executable: 'title', 'game' or 'kiosk'; its LoaderData's next one
  async runExecutable(name) {
    const factory = await this.load(name);
    let finish;
    const done = new Promise((r) => (finish = r));
    let ended = false;
    // main() has returned (boot.cpp tells, or else the runtime's exit): the LoaderData it leaves
    const end = (status) => {
      if (ended) return;
      ended = true;
      const ptr = module._port_loader_data();
      const size = module._port_loader_data_size();
      this.loaderData = module.HEAPU8.slice(ptr, ptr + size);
      finish(status);
    };
    this.host.mainDone = end;
    const module = await factory({
      vib: this.host,
      print: (t) => this.host.log(t),
      printErr: (t) => this.host.log(t),
      onExit: end,
      quit: (status, toThrow) => {
        if (status !== 0) throw toThrow;
      },
    });
    this.module = module;
    module._port_set_hq_music(this.hqMusic ? 1 : 0);
    module._port_set_gaussian(this.gaussian ? 1 : 0);
    const data = module._port_loader_data();
    if (this.loaderData) {
      module.HEAPU8.set(this.loaderData, data);
      module._port_loader_restored();
      // the record kept from before, if the game has none yet
      const kept = this.record && this.record.load();
      const saved = new DataView(this.loaderData.buffer).getInt32(RECORD_AT, true);
      if (!saved && kept && kept.length === RECORD_SIZE) module.HEAPU8.set(kept, data + RECORD_AT);
    }
    // the game writes its record as it goes: kept when it changes
    let last = null;
    const keep = () => {
      if (!this.record) return;
      const bytes = module.HEAPU8.slice(data + RECORD_AT, data + RECORD_AT + RECORD_SIZE);
      if (!new DataView(bytes.buffer).getInt32(0, true)) return;
      if (last && bytes.every((b, i) => b === last[i])) return;
      last = bytes;
      this.record.save(bytes);
    };
    const keeping = this.record ? setInterval(keep, 1000) : 0;
    module.callMain([]);
    await done;
    clearInterval(keeping);
    keep();
    this.module = null;
    const next = new DataView(this.loaderData.buffer).getInt32(0, true);
    return ['title', 'game', 'kiosk'][next] || 'title';
  }

  async run() {
    let next = 'title';
    for (;;) {
      this.host.log(`--- ${next} ---`);
      next = await this.runExecutable(next);
    }
  }
}

// --- a disc image from the player's files: its .cue and the .bin files it names ---

function fileSource(file, whole) {
  if (whole) return file.arrayBuffer().then((b) => new MemorySource(new Uint8Array(b)));
  return Promise.resolve(new ChunkedSource(file.size, async (offset, length) => new Uint8Array(await file.slice(offset, offset + length).arrayBuffer())));
}

export async function discFromFiles(files) {
  const cue = files.find((f) => /\.cue$/i.test(f.name));
  if (!cue) throw new Error('Choose the .cue file of the disc image together with its .bin files.');
  const text = await cue.text();
  const byName = new Map(files.map((f) => [f.name.toLowerCase(), f]));
  // the first file (the data track) is read whole; the others, CD audio, as they are played
  const names = [...text.matchAll(/^\s*FILE\s+"?(.*?)"?\s+\S+\s*$/gim)].map((m) => m[1]);
  const sources = new Map();
  for (const [i, name] of names.entries()) {
    const file = byName.get(name.toLowerCase()) || byName.get(name.split(/[\\/]/).pop().toLowerCase());
    if (!file) throw new Error(`The disc image needs "${name}": choose it together with the .cue.`);
    sources.set(name, await fileSource(file, i === 0 && file.size < 128 * 1024 * 1024));
  }
  const disc = Disc.fromCue(text, (name) => sources.get(name), cue.name.replace(/\.cue$/i, ''));
  if (!disc.data(16)) throw new Error('This disc image has no data track.');
  return disc;
}
