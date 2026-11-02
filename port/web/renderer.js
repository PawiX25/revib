// Draws the game's frames on a canvas at the screen's own resolution: what
// the GPU drew into the frame buffer (port/runtime/gpu.cpp records it) is
// drawn again, the lines as smooth vectors. Frames that cannot be drawn
// that way, and the "PlayStation" picture setting, show the VRAM's picture,
// scaled.

const OP_FILL = 1;
const OP_LINE = 2;
const OP_TILE = 3;
const OP_SPRITE = 4;

const css = (rgb) => `rgb(${rgb & 255},${(rgb >> 8) & 255},${(rgb >> 16) & 255})`;

export class Renderer {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d', { alpha: false });
    this.native = document.createElement('canvas');
    this.nativeCtx = this.native.getContext('2d');
    this.nativeImage = null;
    this.sprite = document.createElement('canvas');
    this.spriteCtx = this.sprite.getContext('2d');
    this.highResolution = true;
    this.lineWidth = 0.75; // in the console's pixels
  }

  // the canvas as large as it is shown, in device pixels
  fit() {
    const ratio = window.devicePixelRatio || 1;
    const w = Math.max(1, Math.round(this.canvas.clientWidth * ratio));
    const h = Math.max(1, Math.round(this.canvas.clientHeight * ratio));
    if (this.canvas.width !== w || this.canvas.height !== h) {
      this.canvas.width = w;
      this.canvas.height = h;
    }
  }

  present(rgba, w, h, frame) {
    this.fit();
    const ctx = this.ctx;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
    // pictures (the logos are sprites side by side) are scaled whole: scaled one by one, their edges would show
    if (!this.highResolution || !frame || frame.fallback || this.hasSprites(frame.ops)) {
      this.drawNative(rgba, w, h);
      return;
    }
    const sx = this.canvas.width / w;
    const sy = this.canvas.height / h;
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    // as thick as the console's lines, but thin on large screens: they are vectors now
    const ratio = window.devicePixelRatio || 1;
    ctx.lineWidth = Math.max(1, Math.min(this.lineWidth * Math.min(sx, sy), 2.5 * ratio));
    const ops = frame.ops;
    let i = 0;
    while (i < ops.length) {
      switch (ops[i]) {
        case OP_FILL:
          this.blend(0, 0);
          ctx.fillStyle = css(ops[i + 5]);
          ctx.fillRect(ops[i + 1] * sx, ops[i + 2] * sy, ops[i + 3] * sx, ops[i + 4] * sy);
          i += 6;
          break;
        case OP_LINE:
          i = this.drawLines(ops, i, sx, sy);
          break;
        case OP_TILE: {
          const flags = ops[i + 6];
          const rgb = this.blend(flags, ops[i + 5]);
          ctx.fillStyle = css(rgb);
          ctx.fillRect(ops[i + 1] * sx, ops[i + 2] * sy, ops[i + 3] * sx, ops[i + 4] * sy);
          i += 7;
          break;
        }
        case OP_SPRITE:
          this.drawSprite(ops, i, frame.vram, sx, sy);
          i += 14;
          break;
        default:
          i = ops.length;
      }
    }
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  }

  hasSprites(ops) {
    const SIZES = { [OP_FILL]: 6, [OP_LINE]: 8, [OP_TILE]: 7, [OP_SPRITE]: 14 };
    for (let i = 0; i < ops.length; i += SIZES[ops[i]] || ops.length) {
      if (ops[i] === OP_SPRITE) return true;
    }
    return false;
  }

  drawNative(rgba, w, h) {
    if (this.native.width !== w || this.native.height !== h) {
      this.native.width = w;
      this.native.height = h;
      this.nativeImage = this.nativeCtx.createImageData(w, h);
    }
    this.nativeImage.data.set(rgba);
    this.nativeCtx.putImageData(this.nativeImage, 0, 0);
    this.ctx.imageSmoothingEnabled = this.highResolution;
    this.ctx.drawImage(this.native, 0, 0, this.canvas.width, this.canvas.height);
  }

  // the canvas's blending for a primitive's semi-transparency (flags: bit 0 on, bits 1-2 the mode); the colour to draw with
  blend(flags, rgb) {
    const ctx = this.ctx;
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
    if (!(flags & 1)) return rgb;
    switch ((flags >> 1) & 3) {
      case 0: // half the picture, half the primitive
        ctx.globalAlpha = 0.5;
        return rgb;
      case 1: // added
        ctx.globalCompositeOperation = 'lighter';
        return rgb;
      case 2: { // subtracted: the canvas multiplies by what is left
        ctx.globalCompositeOperation = 'multiply';
        return (255 - (rgb & 255)) | ((255 - ((rgb >> 8) & 255)) << 8) | ((255 - ((rgb >> 16) & 255)) << 16);
      }
      default: // a quarter added
        ctx.globalCompositeOperation = 'lighter';
        ctx.globalAlpha = 0.25;
        return rgb;
    }
  }

  // lines from op `i`, those of one colour and blending in one path; the op after them
  drawLines(ops, i, sx, sy) {
    const ctx = this.ctx;
    const color = ops[i + 5];
    const flags = ops[i + 7];
    if ((flags & 8) && ops[i + 5] !== ops[i + 6]) {
      // gouraud shaded, its ends of different colours
      this.blend(flags, 0);
      const x0 = (ops[i + 1] + 0.5) * sx, y0 = (ops[i + 2] + 0.5) * sy;
      const x1 = (ops[i + 3] + 0.5) * sx, y1 = (ops[i + 4] + 0.5) * sy;
      const g = ctx.createLinearGradient(x0, y0, x1, y1);
      g.addColorStop(0, css(ops[i + 5]));
      g.addColorStop(1, css(ops[i + 6]));
      ctx.strokeStyle = g;
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.stroke();
      return i + 8;
    }
    ctx.strokeStyle = css(this.blend(flags, color));
    ctx.beginPath();
    while (i < ops.length && ops[i] === OP_LINE && ops[i + 5] === color && ops[i + 7] === flags && (!(flags & 8) || ops[i + 6] === color)) {
      const x0 = (ops[i + 1] + 0.5) * sx, y0 = (ops[i + 2] + 0.5) * sy;
      const x1 = (ops[i + 3] + 0.5) * sx, y1 = (ops[i + 4] + 0.5) * sy;
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      i += 8;
    }
    ctx.stroke();
    return i;
  }

  // a sprite: its texels from VRAM (4-bit, 8-bit with their CLUT, or 15-bit), shaded by its colour unless raw
  drawSprite(ops, i, vram, sx, sy) {
    const [x, y, w, h, u0, v0, texX, texY, depth, clutX, clutY, rgb, flags] = ops.subarray(i + 1, i + 14);
    if (w <= 0 || h <= 0) return;
    if (this.sprite.width !== w || this.sprite.height !== h) {
      this.sprite.width = w;
      this.sprite.height = h;
    }
    const image = this.spriteCtx.createImageData(w, h);
    const out = image.data;
    const raw = flags & 8;
    const mr = rgb & 255, mg = (rgb >> 8) & 255, mb = (rgb >> 16) & 255;
    for (let ty = 0; ty < h; ty++) {
      const row = ((texY + ((v0 + ty) & 255)) & 511) * 1024;
      for (let tx = 0; tx < w; tx++) {
        const u = (u0 + tx) & 255;
        let texel;
        if (depth === 0) {
          const word = vram[row + ((texX + (u >> 2)) & 1023)];
          texel = vram[clutY * 1024 + ((clutX + ((word >> ((u & 3) * 4)) & 15)) & 1023)];
        } else if (depth === 1) {
          const word = vram[row + ((texX + (u >> 1)) & 1023)];
          texel = vram[clutY * 1024 + ((clutX + ((word >> ((u & 1) * 8)) & 255)) & 1023)];
        } else {
          texel = vram[row + ((texX + u) & 1023)];
        }
        const o = (ty * w + tx) * 4;
        if (texel === 0) {
          out[o + 3] = 0;
          continue;
        }
        let r = (texel & 31) << 3, g = ((texel >> 5) & 31) << 3, b = ((texel >> 10) & 31) << 3;
        if (!raw) {
          r = Math.min(255, (r * mr) >> 7);
          g = Math.min(255, (g * mg) >> 7);
          b = Math.min(255, (b * mb) >> 7);
        }
        out[o] = r;
        out[o + 1] = g;
        out[o + 2] = b;
        out[o + 3] = 255;
      }
    }
    this.spriteCtx.putImageData(image, 0, 0);
    this.blend(flags, 0);
    this.ctx.imageSmoothingEnabled = true;
    this.ctx.drawImage(this.sprite, x * sx, y * sy, w * sx, h * sy);
  }
}
