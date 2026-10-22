#!/usr/bin/env python3
"""The game's movies: models' (.ANM, src/game/unk_25590.cpp, ModelMovie) and
cameras' (.ANC, src/game/CameraMovie.cpp).

As a module: read(), pose(), read_camera(). As a script:

    anm.py info <movie.anm|movie.anc>
    anm.py png  <model.tmd> <movie.anm> <frame> <out.png>   (seen from the side)

A file that gives its rate starts with -0x8000, the rate and the number of
frames; others start with the number of frames and play at 60 a second.

In a model's movie, a table of where each frame starts (in 16-bit words from
the start of the file) follows, with one more entry where the last frame
ends (ModelMovie::Draw() reads a frame up to the start of the next one). A
frame lists the objects of the model drawn in it, each a byte for the object
and a byte of flags, then, as the flags say, its rotation (3 angles, 4096 a
full turn; RotMatrixZYX, the matrix Rz Ry Rx), its scale (4096 is 1) and its
translation. Every object is placed on its own, relative to the model.

A camera's movie has 8 words a frame: where the camera is, what it looks at,
its roll and its field of view.
"""
import math
import struct
import sys

import tmd

HEADER = -0x8000
ROTATE, SCALE, MOVE = 1, 2, 4


class Movie:
    def __init__(self, rate, frames):
        self.rate = rate      # frames a second
        self.frames = frames  # model: [[(object, angles, scale, move)]], None for what is not given
                              # camera: [(position, target, roll, fov)]


def _shorts(path):
    data = path if isinstance(path, bytes) else open(path, 'rb').read()
    return struct.unpack('<%dh' % (len(data) // 2), data[:len(data) // 2 * 2])


def _header(shorts):
    """(rate, frames, where what follows starts)"""
    if shorts[0] == HEADER:
        return shorts[1], shorts[2], 3
    return 60, shorts[0], 1


def read(path):
    """The model's movie in a file, or in its bytes."""
    shorts = _shorts(path)
    rate, count, table = _header(shorts)
    offsets = [s & 0xFFFF for s in shorts[table:table + count + 1]]
    frames = []
    for f in range(count):
        at, end = offsets[f], offsets[f + 1]
        entries = []
        while at < end:
            head = shorts[at] & 0xFFFF
            obj, flags = head & 0xFF, head >> 8
            at += 1
            angles = scale = move = None
            if flags & ROTATE:
                angles = shorts[at:at + 3]
                at += 3
            if flags & SCALE:
                scale = shorts[at:at + 3]
                at += 3
            if flags & MOVE:
                move = shorts[at:at + 3]
                at += 3
            entries.append((obj, angles, scale, move))
        frames.append(entries)
    return Movie(rate, frames)


def read_camera(path):
    """The camera's movie in a file, or in its bytes."""
    shorts = _shorts(path)
    rate, count, at = _header(shorts)
    frames = []
    for f in range(count):
        s = shorts[at + 8 * f:at + 8 * f + 8]
        frames.append((s[0:3], s[3:6], s[6], s[7]))
    return Movie(rate, frames)


def rotation(angles):
    """RotMatrixZYX: the matrix Rz Ry Rx, so a vertex turns about X, then Y, then Z."""
    ax, ay, az = (a * 2 * math.pi / 4096 for a in angles)
    cx, sx, cy, sy, cz, sz = math.cos(ax), math.sin(ax), math.cos(ay), math.sin(ay), math.cos(az), math.sin(az)
    return [[cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz],
            [cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz],
            [-sy, sx * cy, cx * cy]]


def place(movie, frame):
    """(object, matrix, translation) of each object drawn in a frame."""
    out = []
    for obj, angles, scale, move in movie.frames[frame % len(movie.frames)]:
        m = rotation(angles) if angles else [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
        if scale:
            m = [[m[i][j] * scale[j] / 4096 for j in range(3)] for i in range(3)]
        out.append((obj, m, move or (0, 0, 0)))
    return out


def pose(objects, movie, frame):
    """3D segments ((x, y, z), (x, y, z)) of the model in a frame of its movie."""
    segs = []
    for obj, m, t in place(movie, frame):
        if obj >= len(objects):
            continue
        o = objects[obj]
        world = [tuple(sum(m[i][j] * v[j] for j in range(3)) + t[i] for i in range(3)) for v in o.vertices]
        for a, b in tmd.lines(o):
            segs.append((world[a], world[b]))
    return segs


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else None
    if len(sys.argv) not in {'info': (3,), 'png': (6,)}.get(cmd, ()):
        sys.exit(__doc__)
    if cmd == 'info':
        if sys.argv[2].lower().endswith('.anc'):
            movie = read_camera(sys.argv[2])
            print('%d frames at %d a second' % (len(movie.frames), movie.rate))
            for f, (position, target, roll, fov) in enumerate(movie.frames):
                print('%4d  at %s  looking at %s  roll %d  fov %d' % (f, position, target, roll, fov))
        else:
            movie = read(sys.argv[2])
            print('%d frames at %d a second' % (len(movie.frames), movie.rate))
            for f, entries in enumerate(movie.frames):
                print('%4d  objects %s' % (f, ' '.join(str(e[0]) for e in entries)))
    else:
        objects = tmd.read(sys.argv[2])[2]
        segs = [((a[2], a[1]), (b[2], b[1])) for a, b in pose(objects, read(sys.argv[3]), int(sys.argv[4]))]
        xs = [p[0] for s in segs for p in s] or [0]
        ys = [p[1] for s in segs for p in s] or [0]
        k = 700 / max(max(xs) - min(xs), max(ys) - min(ys), 1)
        tmd.png(sys.argv[5], [((50 + (a[0] - min(xs)) * k, 50 + (a[1] - min(ys)) * k),
                               (50 + (b[0] - min(xs)) * k, 50 + (b[1] - min(ys)) * k)) for a, b in segs], 800, 800)
        print('wrote', sys.argv[5])


if __name__ == '__main__':
    main()
