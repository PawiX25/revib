#!/usr/bin/env python3
"""Vibri's four forms running, and every piece of road, from the disc.

    cast.py <your disc's Track 1 .bin> <out.gif> [--png out.png]

The forms are the models of GAME/02_FILES.PAK's CHARA/ (SceneDraw.cpp's
chara/Super, chara/Peeloo, which is Vibri herself, chara/Frog and
chara/Snake), each playing its N00_SKIP.ANM, seen from the front camera,
ROAD/F01.ANC. Below them is a road with each of the ten pieces of
ROAD/ROAD.TMD, named as their moves' files name them: J the block, H the
pit, L the loop, W the wave, and two letters for two at once. Drawn and
jittered as banner.py draws them. Needs Pillow.
"""
import argparse
import os
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import anm  # noqa: E402
import pak  # noqa: E402
import tmd  # noqa: E402
from banner import PIECE, Camera, Random, jitter, render, text_lines, text_width  # noqa: E402

FORMS = [('SUPER', 'SUPER'), ('PEELOO', 'VIBRI'), ('FROG', 'FROG'), ('SNAKE', 'SNAKE')]
PIECES = ['J', 'H', 'L', 'W', 'JH', 'JL', 'JW', 'HL', 'HW', 'LW']

W, H = 1280, 640
FORM_SCALE, FORM_GROUND = 0.3, 300     # the forms: pixels a unit, where they stand
ROAD_SCALE, ROAD_GROUND = 0.098, 500   # the road
SPACING = 2                            # beats between pieces
FORM_JITTER, ROAD_JITTER = 10, 15      # SceneDraw::Draw() at the first stage
TICKS = 20                             # frames of the GIF: one run, the whole of its movie


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('image')
    ap.add_argument('gif')
    ap.add_argument('--png')
    args = ap.parse_args()

    disc = os.path.join(os.path.dirname(HERE), 'psxdisc.py')
    with tempfile.TemporaryDirectory() as work:
        subprocess.run([sys.executable, disc, 'extract', args.image, work, 'GAME/02_FILES.PAK'],
                       check=True, stdout=subprocess.DEVNULL)
        files = dict(pak.read(os.path.join(work, 'GAME', '02_FILES.PAK')))
    font = tmd.read(files['FONT/FE_FONT.TMD'])[2]
    road = tmd.read(files['ROAD/ROAD.TMD'])[2]
    point = tmd.read(files['ROAD/POINT.TMD'])[2][0]
    forms = [(tmd.read(files['CHARA/%s/MODEL.TMD' % d])[2], anm.read(files['CHARA/%s/N00_SKIP.ANM' % d]), name)
             for d, name in FORMS]
    front = anm.read_camera(files['ROAD/F01.ANC']).frames[0]
    side = anm.read_camera(files['ROAD/S01.ANC']).frames[0]

    first = W / 2 - (len(PIECES) - 1) * SPACING * PIECE * ROAD_SCALE / 2 - PIECE / 2 * ROAD_SCALE
    road_camera = Camera(side, ROAD_SCALE, (first, ROAD_GROUND))

    def frame(n, rnd):
        segs = []
        for i, (model, run, name) in enumerate(forms):
            x = W / 8 + i * W / 4
            cam = Camera(front, FORM_SCALE, (x, FORM_GROUND))
            for obj, m, move in anm.place(run, n * len(run.frames) // TICKS):
                o = model[obj]
                work = jitter(o.vertices, FORM_JITTER, rnd)
                world = [tuple(sum(m[r][c] * v[c] for c in range(3)) + move[r] for r in range(3)) for v in work]
                for a, b in tmd.lines(o):
                    segs.append((cam.project(world[a]), cam.project(world[b])))
            segs += text_lines(font, name, x - text_width(name, 0.2) / 2, FORM_GROUND + 40, 0.2, rnd)

        # the road, as banner.py draws it, to a point past the last piece
        join = jitter([(0, 0, -2 * PIECE)], ROAD_JITTER, rnd)[0]
        stops = [(i * SPACING * PIECE, o, name) for i, (o, name) in enumerate(zip(road, PIECES))]
        for z, o, name in stops + [((len(PIECES) * SPACING + 1) * PIECE, point, None)]:
            work = [(v[0], v[1], v[2] + z) for v in jitter(o.vertices, ROAD_JITTER, rnd)]
            segs.append((road_camera.project(join), road_camera.project(work[0])))
            for a, b in tmd.lines(o):
                segs.append((road_camera.project(work[a]), road_camera.project(work[b])))
            join = work[-1]
            if name:
                x = road_camera.project((0, 0, z + PIECE / 2))[0]
                segs += text_lines(font, name, x - text_width(name, 0.18) / 2, ROAD_GROUND + 95, 0.18, rnd)
        return render(segs, (W, H))

    rnd = Random()
    if args.png:
        frame(0, rnd).save(args.png)
        print('wrote', args.png)
    grays = Image.new('P', (1, 1))
    grays.putpalette([v for i in range(4) for v in (i * 85,) * 3] + [0] * 3 * 252)
    frames = [frame(n, rnd).convert('RGB').quantize(palette=grays, dither=Image.Dither.NONE) for n in range(TICKS)]
    frames[0].save(args.gif, save_all=True, append_images=frames[1:], duration=50, loop=0, optimize=True)
    print('wrote', args.gif)


if __name__ == '__main__':
    main()
