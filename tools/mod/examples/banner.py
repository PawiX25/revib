#!/usr/bin/env python3
"""The README's banner, drawn from the game's own data the way the game draws it.

    banner.py <your disc's Track 1 .bin> <out.png> [--gif out.gif]
              [--text REVIB] [--sub "VIB-RIBBON DECOMPILED"] [--at BEAT]

All of it comes from GAME/02_FILES.PAK:

- the letters: the game's vector font, FONT/FE_FONT.TMD, its characters
  mapped as src/game/Font.cpp maps them;
- the road: a piece of ROAD/ROAD.TMD for each obstacle, joined by straight
  lines as TmdLineObj::Draw() joins them;
- Vibri: CHARA/PEELOO/MODEL.TMD, running (N00_SKIP.ANM) and clearing each
  obstacle with its move (N01_J.ANM and the others), as src/game/SceneDraw.cpp
  plays them: a move takes a beat, in which the road comes a piece nearer;
- the view: the side camera, ROAD/S01.ANC.

Every vertex is jittered as TmdLineObj::DrawPass() jitters it, with the
game's random numbers, and as in Gameplay::Update() the scene holds still
from just before an obstacle until a beat after Vibri clears it. Needs Pillow.
"""
import argparse
import math
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import anm  # noqa: E402
import pak  # noqa: E402
import tmd  # noqa: E402

# Font.cpp: the font's object of each character from ' ' on
OBJECTS = [
    0, 63, 0, 0, 0, 0, 64, 67, 69, 70, 0, 0, 66, 68, 65, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 62,
    0, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0, 0, 0, 0, 0,
    0, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0,
]
CHARACTER_WIDTH = 90  # Font.cpp: the advance of a character
FONT_JITTER = 4       # Font.cpp: its extra jitter, (int)4.5

# ROAD.TMD's objects, one for each road (SceneDraw.cpp), and the moves that clear them
BLOCK, PIT, LOOP, WAVE, BLOCK_PIT, BLOCK_LOOP, BLOCK_WAVE, PIT_LOOP, PIT_WAVE, LOOP_WAVE = range(10)
MOVES = ['N01_J', 'N02_H', 'N03_L', 'N04_W', 'N05_JH', 'N06_JL', 'N07_JW', 'N08_HL', 'N09_HW', 'N10_LW']
RUN = 'N00_SKIP'

PIECE = 600          # SceneDraw.cpp: the length of a beat of road
WINDOW = 0.2         # Gameplay.cpp: how near (in beats) an obstacle is to be at Vibri
FRAMES_A_BEAT = 30   # a movie plays at the beats a second / 2, at 60 frames a second
STAGE = 1            # SceneDraw::Draw(): the jitter is 14 * stage + 10, the road's half as much again

# the road, round and round: (beat, obstacle)
ROAD = [(1, BLOCK), (3, WAVE), (4, PIT), (6, LOOP), (9, BLOCK_WAVE), (11, LOOP_WAVE), (13, PIT_WAVE), (14, BLOCK_PIT)]
PERIOD = 16
TICKS = 10           # frames of the animation a beat (at 5/100 s a frame, 120 beats a minute)

W, H, SS = 1280, 520, 3   # the banner, and how much finer it is drawn first
LINE = 2.0                # line width
SCALE = 0.165             # pixels a unit at Vibri
VIBRI = (250, 422)        # where Vibri stands


class Random:
    """Random.h: a linear congruential generator (the constants of Numerical Recipes)."""

    def __init__(self):
        self.seed = 0

    def next(self):
        self.seed = (self.seed * 1664525 + 1013904223) & 0xFFFFFFFF
        return self.seed


def jitter(vertices, amount, rnd):
    """TmdLineObj::DrawPass(): each vertex moved by less than `amount` on each axis."""
    r = int(amount)
    bits = (r * 2).bit_length()
    mask, high = (1 << bits) - 1, 32 - bits
    out = []
    for v in vertices:
        d = []
        for _ in range(3):
            k = ((rnd.next() >> high) & mask) - r
            if k >= r:
                k -= r * 2
            d.append(k)
        out.append((v[0] + d[0], v[1] + d[1], v[2] + d[2]))
    return out


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(a):
    n = math.sqrt(dot(a, a))
    return (a[0] / n, a[1] / n, a[2] / n)


class Camera:
    """GsSetRefView2() for a frame of a camera's movie: looking from `position` at
    `target`, down being +y; drawn `scale` pixels a unit at the origin, the origin at `anchor`."""

    def __init__(self, frame, scale, anchor):
        position, target, roll, fov = frame
        self.position = position
        self.forward = unit(sub(target, position))
        down = (0, 1, 0)
        down = unit(sub(down, tuple(c * dot(down, self.forward) for c in self.forward)))
        right = cross(down, self.forward)
        a = roll * 2 * math.pi / 4096
        self.right = tuple(math.cos(a) * r + math.sin(a) * d for r, d in zip(right, down))
        self.down = tuple(math.cos(a) * d - math.sin(a) * r for r, d in zip(right, down))
        self.centre = (0.0, 0.0)
        self.h = scale * dot(sub((0, 0, 0), position), self.forward)
        x, y = self.project((0, 0, 0))
        self.centre = (anchor[0] - x, anchor[1] - y)

    def project(self, v):
        d = sub(v, self.position)
        z = dot(d, self.forward)
        return (self.centre[0] + self.h * dot(d, self.right) / z, self.centre[1] + self.h * dot(d, self.down) / z)


class Assets:
    def __init__(self, image):
        disc = os.path.join(os.path.dirname(HERE), 'psxdisc.py')
        with tempfile.TemporaryDirectory() as work:
            subprocess.run([sys.executable, disc, 'extract', image, work, 'GAME/02_FILES.PAK'],
                           check=True, stdout=subprocess.DEVNULL)
            files = dict(pak.read(os.path.join(work, 'GAME', '02_FILES.PAK')))
        self.font = tmd.read(files['FONT/FE_FONT.TMD'])[2]
        self.road = tmd.read(files['ROAD/ROAD.TMD'])[2]
        self.point = tmd.read(files['ROAD/POINT.TMD'])[2][0]
        self.vibri = tmd.read(files['CHARA/PEELOO/MODEL.TMD'])[2]
        self.run = anm.read(files['CHARA/PEELOO/%s.ANM' % RUN])
        self.moves = [anm.read(files['CHARA/PEELOO/%s.ANM' % m]) for m in MOVES]
        self.camera = Camera(anm.read_camera(files['ROAD/S01.ANC']).frames[0], SCALE, VIBRI)


def text_lines(font, text, x, y, scale, rnd):
    """Font::Draw(): `text` from (x, y) (its vertical middle), each character jittered."""
    segs = []
    for i, ch in enumerate(text):
        if ch == ' ':
            continue
        o = font[OBJECTS[ord(ch) - 32]]
        cx = x + i * CHARACTER_WIDTH * scale
        work = jitter(o.vertices, FONT_JITTER, rnd)
        for a, b in tmd.lines(o):
            segs.append(((cx + work[a][0] * scale, y + work[a][1] * scale), (cx + work[b][0] * scale, y + work[b][1] * scale)))
    return segs


def text_width(text, scale):
    return (len(text) - 1) * CHARACTER_WIDTH * scale


def scene(assets, t, rnd):
    """The road and Vibri at `t` beats."""
    cam = assets.camera
    pieces = []
    for k in range(-1, 3):
        for beat, road in ROAD:
            pieces.append((beat + k * PERIOD - t, road))
    pieces.sort()
    # Gameplay::Update(): still while an obstacle is near or was just cleared
    still = any(-1 < p < WINDOW for p, _ in pieces)
    level = 0 if still else 14 * STAGE + 10
    segs = []

    # the road, from a point behind Vibri to one far ahead, each piece joined to the last
    join = jitter([(0, 0, -6 * PIECE)], level * 1.5, rnd)[0]
    for p, road in pieces + [(30, None)]:
        if p < -6 or p > 30:
            continue
        o = assets.point if road is None else assets.road[road]
        z = p * PIECE
        work = [(v[0], v[1], v[2] + z) for v in jitter(o.vertices, level * 1.5, rnd)]
        segs.append((cam.project(join), cam.project(work[0])))
        for a, b in tmd.lines(o):
            segs.append((cam.project(work[a]), cam.project(work[b])))
        join = work[-1]

    # Vibri: the move of the obstacle she is at, or running since the last one
    behind = [(p, road) for p, road in pieces if p <= 0]
    p, road = behind[-1]
    if -p < 1:
        movie, frame = assets.moves[road], int(-p * FRAMES_A_BEAT)
    else:
        movie, frame = assets.run, int((-p - 1) * FRAMES_A_BEAT) % len(assets.run.frames)
    for obj, m, move in anm.place(movie, frame):
        o = assets.vibri[obj]
        work = jitter(o.vertices, level, rnd)
        world = [tuple(sum(m[i][j] * v[j] for j in range(3)) + move[i] for i in range(3)) for v in work]
        for a, b in tmd.lines(o):
            segs.append((cam.project(world[a]), cam.project(world[b])))
    return segs


def render(segs, size=(W, H)):
    """White lines on black, drawn SS times finer and scaled down."""
    img = Image.new('L', (size[0] * SS, size[1] * SS), 0)
    draw = ImageDraw.Draw(img)
    for (x0, y0), (x1, y1) in segs:
        draw.line([(x0 * SS, y0 * SS), (x1 * SS, y1 * SS)], fill=255, width=int(LINE * SS))
    return img.resize(size, Image.LANCZOS)


def frame(assets, args, t, rnd):
    big, small = 0.78, 0.2
    segs = text_lines(assets.font, args.text, W / 2 - text_width(args.text, big) / 2, 100, big, rnd)
    segs += text_lines(assets.font, args.sub, W / 2 - text_width(args.sub, small) / 2, 205, small, rnd)
    segs += scene(assets, t, rnd)
    return render(segs)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('image')
    ap.add_argument('png')
    ap.add_argument('--gif')
    ap.add_argument('--text', default='REVIB')
    ap.add_argument('--sub', default='VIB-RIBBON DECOMPILED')
    ap.add_argument('--at', type=float, default=6.45, help='the beat the PNG shows')
    args = ap.parse_args()

    assets = Assets(args.image)
    rnd = Random()
    frame(assets, args, args.at, rnd).save(args.png)
    print('wrote', args.png)
    if args.gif:
        rnd = Random()
        grays = Image.new('P', (1, 1))
        grays.putpalette([v for i in range(4) for v in (i * 85,) * 3] + [0] * 3 * 252)
        frames = [frame(assets, args, n / TICKS, rnd).convert('RGB').quantize(palette=grays, dither=Image.Dither.NONE)
                  for n in range(PERIOD * TICKS)]
        frames[0].save(args.gif, save_all=True, append_images=frames[1:], duration=1000 // (2 * TICKS), loop=0,
                       optimize=True)
        print('wrote', args.gif)


if __name__ == '__main__':
    main()
