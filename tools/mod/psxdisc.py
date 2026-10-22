#!/usr/bin/env python3
"""The data track of a PlayStation disc image (raw 2352-byte Mode 2 Form 1 sectors).

    psxdisc.py list    <track1.bin>
    psxdisc.py extract <track1.bin> <out_dir> [glob ...]   (default: every file)
    psxdisc.py patch   <track1.bin> <path-on-disc> <new_file>

`extract` writes each file whose path on the disc matches one of the globs
under <out_dir>, at that path.

`patch` writes a file over one on the disc, in place, and recomputes the EDC
and ECC of every sector it changes, as a real disc has them. The new file
must be exactly as big as the old one (it refuses another size): pad a
smaller one with zeros. Work on a copy of your image.
"""
import fnmatch
import os
import struct
import sys

RAW = 2352
DATA = 2048
DATA_OFFSET = 24  # 12 sync + 4 header + 8 subheader


def read_sector(fp, lba):
    fp.seek(lba * RAW + DATA_OFFSET)
    return fp.read(DATA)


def directory(fp, lba, size):
    data = b''.join(read_sector(fp, lba + i) for i in range((size + DATA - 1) // DATA))[:size]
    i = 0
    while i < len(data):
        length = data[i]
        if length == 0:  # records do not cross sectors: skip to the next one
            i = (i // DATA + 1) * DATA
            continue
        rec = data[i:i + length]
        name = rec[33:33 + rec[32]].decode('ascii', 'replace').split(';')[0]
        yield name, struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0], bool(rec[25] & 2)
        i += length


def walk(fp, lba=None, size=None, prefix=''):
    if lba is None:
        pvd = read_sector(fp, 16)
        if pvd[1:6] != b'CD001':
            sys.exit('not an ISO 9660 data track')
        lba, size = struct.unpack_from('<I', pvd, 158)[0], struct.unpack_from('<I', pvd, 166)[0]
    for name, l, s, is_dir in directory(fp, lba, size):
        if name in ('', '\x00', '\x01'):
            continue
        path = prefix + '/' + name if prefix else name
        if is_dir:
            yield from walk(fp, l, s, path)
        else:
            yield path, l, s


# EDC (a CRC-32) and ECC (Reed-Solomon P and Q parity) of a Mode 2 Form 1 sector
ECC_F, ECC_B, EDC_LUT = [0] * 256, [0] * 256, [0] * 256
for i in range(256):
    j = ((i << 1) ^ (0x11D if i & 0x80 else 0)) & 0xFF
    ECC_F[i] = j
    ECC_B[i ^ j] = i
    e = i
    for _ in range(8):
        e = (e >> 1) ^ (0xD8018001 if e & 1 else 0)
    EDC_LUT[i] = e


def edc(data):
    e = 0
    for b in data:
        e = (e >> 8) ^ EDC_LUT[(e ^ b) & 0xFF]
    return e


def ecc_block(s, major_count, minor_count, major_mult, minor_inc, dest):
    size = major_count * minor_count
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        a = b = 0
        for _ in range(minor_count):
            t = s[0xC + index]
            index += minor_inc
            if index >= size:
                index -= size
            a ^= t
            b ^= t
            a = ECC_F[a]
        a = ECC_B[ECC_F[a] ^ b]
        s[dest + major] = a
        s[dest + major + major_count] = a ^ b


def fix_sector(s):
    """Recompute the EDC and ECC of a raw Mode 2 Form 1 sector (a bytearray)."""
    s[0x818:0x81C] = struct.pack('<I', edc(s[0x10:0x818]))
    header = bytes(s[12:16])
    s[12:16] = b'\0\0\0\0'  # Mode 2 leaves the address out of the ECC
    ecc_block(s, 86, 24, 2, 86, 0x81C)
    ecc_block(s, 52, 43, 86, 88, 0x8C8)
    s[12:16] = header


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else None
    if not (cmd == 'list' and len(sys.argv) == 3 or cmd == 'extract' and len(sys.argv) >= 4 or
            cmd == 'patch' and len(sys.argv) == 5):
        sys.exit(__doc__)
    image = sys.argv[2]
    with open(image, 'r+b' if cmd == 'patch' else 'rb') as fp:
        files = list(walk(fp))
        if cmd == 'list':
            for path, lba, size in files:
                print('%-40s lba %6d %9d bytes' % (path, lba, size))
        elif cmd == 'extract':
            out, patterns = sys.argv[3], sys.argv[4:] or ['*']
            for path, lba, size in files:
                if not any(fnmatch.fnmatch(path, p) for p in patterns):
                    continue
                dest = os.path.join(out, path)
                os.makedirs(os.path.dirname(dest) or '.', exist_ok=True)
                data = b''.join(read_sector(fp, lba + i) for i in range((size + DATA - 1) // DATA))[:size]
                open(dest, 'wb').write(data)
                print('wrote', dest)
        else:
            path, new = sys.argv[3], open(sys.argv[4], 'rb').read()
            match = [f for f in files if f[0].lower() == path.lower()]
            if not match:
                sys.exit('not on the disc: ' + path)
            _, lba, size = match[0]
            if len(new) != size:
                sys.exit('%s is %d bytes, the new file %d: the sizes must be the same' % (path, size, len(new)))
            changed = 0
            for k in range((size + DATA - 1) // DATA):
                fp.seek((lba + k) * RAW)
                s = bytearray(fp.read(RAW))
                chunk = new[k * DATA:(k + 1) * DATA]
                if s[DATA_OFFSET:DATA_OFFSET + len(chunk)] != chunk:
                    s[DATA_OFFSET:DATA_OFFSET + len(chunk)] = chunk
                    fix_sector(s)
                    fp.seek((lba + k) * RAW)
                    fp.write(s)
                    changed += 1
            print('%s: %d sectors written' % (path, changed))


if __name__ == '__main__':
    main()
