#!/usr/bin/env python3
"""The game's FILES.PAK archives (read by src/game/PackedFiles.cpp).

    pak.py list <FILES.PAK>
    pak.py get  <FILES.PAK> <name> <out_file>
    pak.py put  <FILES.PAK> <name> <new_file> [out.pak]

Layout: int count; int offsets[count] (from the start of the archive); at
each offset the file's name (NUL-terminated, padded to 4 bytes), its size as
a 32-bit word, then its bytes. The entries are sorted by name, ignoring case
(PackedFiles looks them up by binary search).

`put` writes the archive anew, in the same layout. To go back onto the disc
(psxdisc.py patch) the archive must keep its size, so the new file must be
as big as the one it replaces: pad it with zeros (tmd.write(size=) does that
for a model).
"""
import struct
import sys


def read(path):
    data = open(path, 'rb').read()
    count = struct.unpack_from('<i', data, 0)[0]
    entries = []
    for offset in struct.unpack_from('<%di' % count, data, 4):
        end = data.index(b'\0', offset)
        name = data[offset:end].decode('latin-1')
        at = offset + ((end - offset + 1 + 3) & ~3)
        size = struct.unpack_from('<I', data, at)[0]
        entries.append((name, data[at + 4:at + 4 + size]))
    return entries


def write(path, entries):
    head = 4 + 4 * len(entries)
    body = b''
    offsets = []
    for name, contents in entries:
        offsets.append(head + len(body))
        raw = name.encode('latin-1') + b'\0'
        raw += b'\0' * (-len(raw) % 4)
        body += raw + struct.pack('<I', len(contents)) + contents + b'\0' * (-len(contents) % 4)
    open(path, 'wb').write(struct.pack('<i', len(entries)) + struct.pack('<%di' % len(entries), *offsets) + body)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else None
    if len(sys.argv) not in {'list': (3,), 'get': (5,), 'put': (5, 6)}.get(cmd, ()):
        sys.exit(__doc__)
    path = sys.argv[2]
    entries = read(path)
    if cmd == 'list':
        for name, contents in entries:
            print('%8d %s' % (len(contents), name))
    elif cmd == 'get':
        for name, contents in entries:
            if name.lower() == sys.argv[3].lower():
                open(sys.argv[4], 'wb').write(contents)
                print('wrote', sys.argv[4])
                return
        sys.exit('not in the archive: ' + sys.argv[3])
    else:
        new = open(sys.argv[4], 'rb').read()
        names = [n.lower() for n, c in entries]
        if sys.argv[3].lower() not in names:
            sys.exit('not in the archive: ' + sys.argv[3])
        entries = [(n, new if n.lower() == sys.argv[3].lower() else c) for n, c in entries]
        write(sys.argv[5] if len(sys.argv) > 5 else path, entries)
        print('replaced', sys.argv[3])


if __name__ == '__main__':
    main()
