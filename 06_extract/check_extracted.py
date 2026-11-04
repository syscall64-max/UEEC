#!/usr/bin/env python3
"""Small structural regression check for the Stage 06 compact SH16 fixture."""
import pathlib
import struct
import sys


def parse(path):
    b = pathlib.Path(path).read_bytes()
    if b[:3] != b'_GM' or b[3] != 1:
        raise ValueError('expected UEEC v1 little-endian fixture')
    nodes, names = struct.unpack_from('<HH', b, 4)
    o = 8
    words = []
    for _ in range(nodes):
        word, fmt = struct.unpack_from('<IH', b, o)
        assert fmt == 0
        words.append(word)
        o += 6
    count = sum(((w >> 27) & 31) for w in words)
    mapping = []
    for _ in range(count):
        mapping.append(struct.unpack_from('<HI', b, o))
        o += 6
    num_sections = max(s for _, s in mapping) + 1
    headers = []
    for _ in range(num_sections):
        headers.append(struct.unpack_from('<HHH', b, o))
        o += 6
    lengths = list(b[o:o+names])
    o += names
    name_idxs = struct.unpack_from('<' + 'H' * nodes, b, o)
    o += 2 * nodes
    o += sum(lengths)
    assert o == headers[0][1], (o, headers[0][1])
    cursor = 0
    sections = []
    for typ, size, data in headers:
        assert size >= data and cursor+size <= len(b)
        sections.append(b[cursor:cursor+data])
        cursor += size
    assert cursor == len(b)
    return words, mapping, headers, sections


def main():
    original, extracted, machine = sys.argv[1], sys.argv[2], int(sys.argv[3])
    a = parse(original)
    e = parse(extracted)
    candidates = [i for i, w in enumerate(a[0]) if ((w >> 10)&255)==machine]
    assert len(candidates)==1, candidates
    selected = candidates[0]
    assert len(e[0]) == 1
    assert e[0][0] == a[0][selected]
    src_sections = [s for n, s in a[1] if n == selected]
    assert len(src_sections) == 3, src_sections
    assert e[1] == [(0, 1), (0, 2), (0, 3)]
    assert [h[0] for h in e[2]] == [0, 2, 1, 5]
    for ix, source_ix in enumerate(src_sections, 1):
        assert e[2][ix] == a[2][source_ix]
        assert e[3][ix] == a[3][source_ix]
    # Relocation: target local[1] CODE, source local[0] DATA.
    reloc = e[3][3]
    assert len(reloc) == 10
    assert struct.unpack_from('<HHHH', reloc) == (0, 0, 1, 1)
    if machine == 11:
        assert struct.unpack_from('<H', reloc, 8)[0] == 0x1020
        assert len(e[3][2]) == 40
    print(f'UEEC extract layout: machine={machine} single node / DATA+CODE+RELOC PASS')


if __name__ == '__main__':
    main()
