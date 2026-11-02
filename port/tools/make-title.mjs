// Makes port/web/title-data.json from a disc image: the game's title (its
// title screen's lines while the name turns), so the page can show it before
// a disc is chosen.
//
//   node port/tools/make-title.mjs "disc.cue"
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const web = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'web');
const cue = process.argv[2];
const text = fs.readFileSync(cue, 'utf8');
const names = [...text.matchAll(/^\s*FILE\s+"?(.*?)"?\s+\S+\s*$/gim)].map((m) => m[1]);
const files = [new File([text], path.basename(cue)), ...names.map((n) => new File([fs.readFileSync(path.join(path.dirname(cue), n))], n))];

// compare-worker.js as in a worker: its message in, its answer out
globalThis.onmessage = null;
globalThis.postMessage = ({ logo, error }) => {
  if (error) throw new Error(error);
  const b64 = (a) => Buffer.from(a.buffer, a.byteOffset, a.byteLength).toString('base64');
  fs.writeFileSync(path.join(web, 'title-data.json'), JSON.stringify({ frames: logo.frames.map(b64) }));
  console.log(`title-data.json: ${logo.frames.length} frames`);
  process.exit(0);
};
await import(path.join(web, 'compare-worker.js'));
globalThis.onmessage({ data: { job: 'logo', files } });
