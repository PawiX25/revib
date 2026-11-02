// The game's title as its title screen draws it: the name turning round a
// ring, Vibri dancing by (compare-worker.js's `logo`: the lines drawn in
// each frame of one turn), drawn again at the canvas's resolution.
export class TitleRing {
  constructor(canvas, frames) {
    this.canvas = canvas;
    this.frames = frames;
    this.at = 0;
    // the lines' extent over the whole turn
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const f of frames) {
      for (let i = 0; i < f.length; i += 5) {
        x0 = Math.min(x0, f[i], f[i + 2]);
        x1 = Math.max(x1, f[i], f[i + 2]);
        y0 = Math.min(y0, f[i + 1], f[i + 3]);
        y1 = Math.max(y1, f[i + 1], f[i + 3]);
      }
    }
    this.box = [x0 - 2, y0 - 2, x1 + 2, y1 + 2];
    // the console's pixels are narrower than tall: 512 x 288 fill a 4:3 screen
    canvas.style.aspectRatio = String(((x1 - x0 + 4) * 0.75) / (y1 - y0 + 4));
    const reduced = matchMedia('(prefers-reduced-motion: reduce)');
    let last = 0;
    const tick = (t) => {
      if (!reduced.matches && t - last >= 40) {
        last = t;
        this.at = (this.at + 1) % frames.length;
        this.draw();
      }
      requestAnimationFrame(tick);
    };
    new ResizeObserver(() => this.draw()).observe(canvas);
    requestAnimationFrame(tick);
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
    g.clearRect(0, 0, w, h);
    const [x0, y0, x1, y1] = this.box;
    const sx = w / (x1 - x0);
    const sy = h / (y1 - y0);
    g.lineCap = 'round';
    g.lineJoin = 'round';
    g.lineWidth = Math.max(1, Math.min(0.9 * sy, 2.5 * ratio));
    const f = this.frames[this.at];
    // the lines, a path for each colour
    const colours = new Map();
    for (let i = 0; i < f.length; i += 5) {
      if (!colours.has(f[i + 4])) colours.set(f[i + 4], []);
      colours.get(f[i + 4]).push(i);
    }
    for (const [rgb, lines] of colours) {
      g.strokeStyle = `rgb(${rgb & 255},${(rgb >> 8) & 255},${(rgb >> 16) & 255})`;
      g.beginPath();
      for (const i of lines) {
        g.moveTo((f[i] + 0.5 - x0) * sx, (f[i + 1] + 0.5 - y0) * sy);
        g.lineTo((f[i + 2] + 0.5 - x0) * sx, (f[i + 3] + 0.5 - y0) * sy);
      }
      g.stroke();
    }
  }
}
