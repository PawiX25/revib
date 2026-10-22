#!/usr/bin/env python3
"""The game's TMD models: objects of white lines (read by src/game/TmdLineObj.cpp).

As a module: read(), write(), lines(), png(), preview(). As a script:

    tmd.py info <model.tmd>
    tmd.py png  <model.tmd> <out.png>

`png` draws every object in a cell of its own, in order from left to right
and down, seen along its thinnest axis (most objects are flat) and scaled to
fit: from the front (x right, y down), the side (z right, y down) or above
(x right, z down).

The game's files lay out the header, the object table, every object's
primitives, then every object's vertices; write() keeps that layout. A file
written back onto the disc must keep its size (see psxdisc.py): write(size=)
pads the model with zeros.
"""
import struct
import sys
import zlib

LINE = 0x40      # flat line: colour, then two vertex indices
QUAD = 0x29      # flat quadrangle, drawn as the line from its first to its last vertex
TRIANGLE = 0x21  # flat triangle; a black one marks the side that faces the camera


class Object:
    def __init__(self, vertices, primitives, scale=0):
        self.vertices = vertices      # [(x, y, z, pad)]
        self.primitives = primitives  # [(olen, ilen, flag, mode, data)]
        self.scale = scale


def read(path):
    """(ident, flags, objects) of a model file, or of its bytes."""
    data = path if isinstance(path, bytes) else open(path, 'rb').read()
    ident, flags, count = struct.unpack_from('<III', data, 0)
    objects = []
    for i in range(count):
        vtop, nvert, ntop, nnorm, ptop, nprim, scale = struct.unpack_from('<IIIIIIi', data, 12 + 28 * i)
        vertices = [struct.unpack_from('<hhhh', data, 12 + vtop + 8 * k) for k in range(nvert)]
        primitives = []
        p = 12 + ptop
        for k in range(nprim):
            olen, ilen, flag, mode = struct.unpack_from('<BBBB', data, p)
            primitives.append((olen, ilen, flag, mode, data[p + 4:p + 4 + 4 * ilen]))
            p += 4 + 4 * ilen
        objects.append(Object(vertices, primitives, scale))
    return ident, flags, objects


def write(path, ident, flags, objects, size=None):
    """Write the model; with `size`, pad it with zeros to that many bytes."""
    table_end = 28 * len(objects)
    prims = b''.join(b''.join(struct.pack('<BBBB', *p[:4]) + p[4] for p in o.primitives) for o in objects)
    verts = b''.join(b''.join(struct.pack('<hhhh', *v) for v in o.vertices) for o in objects)
    table = b''
    ptop, vtop, end = table_end, table_end + len(prims), table_end + len(prims) + len(verts)
    for o in objects:
        table += struct.pack('<IIIIIIi', vtop, len(o.vertices), end, 0, ptop, len(o.primitives), o.scale)
        ptop += sum(4 + 4 * p[1] for p in o.primitives)
        vtop += 8 * len(o.vertices)
    data = struct.pack('<III', ident, flags, len(objects)) + table + prims + verts
    if size is not None:
        if len(data) > size:
            raise ValueError('the model is %d bytes, more than %d' % (len(data), size))
        data += b'\0' * (size - len(data))
    open(path, 'wb').write(data)


def lines(o):
    """The object's lines as (vertex, vertex) pairs."""
    out = []
    for olen, ilen, flag, mode, data in o.primitives:
        if mode == LINE:
            out.append(struct.unpack_from('<hh', data, 4))
        elif mode == QUAD:
            v = struct.unpack_from('<hhhh', data, 4)
            out.append((v[0], v[3]))
    return out


def line(v0, v1, colour=(255, 255, 255)):
    """A white line primitive between two vertex indices."""
    return (3, 2, 1, LINE, struct.pack('<BBBBhh', colour[0], colour[1], colour[2], LINE, v0, v1))


def png(path, segments, width, height):
    """A white-on-black PNG of ((x0, y0), (x1, y1)) segments in pixels."""
    img = bytearray(width * height)
    for (x0, y0), (x1, y1) in segments:
        steps = int(max(abs(x1 - x0), abs(y1 - y0), 1))
        for s in range(steps + 1):
            x = int(round(x0 + (x1 - x0) * s / steps))
            y = int(round(y0 + (y1 - y0) * s / steps))
            if 0 <= x < width and 0 <= y < height:
                img[y * width + x] = 255
    raw = b''.join(b'\0' + bytes(img[y * width:(y + 1) * width]) for y in range(height))

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 0, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def view(o):
    """The axes an object is drawn on, (across, down): those of its two largest extents."""
    extents = [max(v[k] for v in o.vertices) - min(v[k] for v in o.vertices) for k in range(3)]
    thinnest = min((2, 0, 1), key=lambda k: extents[k])  # on a tie, from the front
    return {2: (0, 1), 0: (2, 1), 1: (0, 2)}[thinnest]


def preview(path, objects, cell=250, columns=4, margin=12):
    """Every object in a cell of its own, seen along its thinnest axis, scaled to fit."""
    columns = max(1, min(columns, len(objects)))
    segments = []
    for i, o in enumerate(objects):
        if not o.vertices:
            continue
        h, v = view(o)
        lo = [min(p[k] for p in o.vertices) for k in (h, v)]
        hi = [max(p[k] for p in o.vertices) for k in (h, v)]
        scale = (cell - 2 * margin) / max(hi[0] - lo[0], hi[1] - lo[1], 1)
        x0 = (i % columns + 0.5) * cell - (lo[0] + hi[0]) / 2 * scale
        y0 = (i // columns + 0.5) * cell - (lo[1] + hi[1]) / 2 * scale
        for a, b in lines(o):
            pa, pb = o.vertices[a], o.vertices[b]
            segments.append(((x0 + pa[h] * scale, y0 + pa[v] * scale), (x0 + pb[h] * scale, y0 + pb[v] * scale)))
    rows = max(1, (len(objects) + columns - 1) // columns)
    png(path, segments, cell * columns, cell * rows)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else None
    if len(sys.argv) not in {'info': (3,), 'png': (4,)}.get(cmd, ()):
        sys.exit(__doc__)
    ident, flags, objects = read(sys.argv[2])
    if cmd == 'info':
        for i, o in enumerate(objects):
            vs = o.vertices or [(0, 0, 0, 0)]
            ranges = ', '.join('%s %d..%d' % (axis, min(v[k] for v in vs), max(v[k] for v in vs)) for k, axis in enumerate('xyz'))
            print('object %d: %d vertices, %d primitives, %s' % (i, len(o.vertices), len(o.primitives), ranges))
    else:
        preview(sys.argv[3], objects)
        print('wrote', sys.argv[3])


if __name__ == '__main__':
    main()
