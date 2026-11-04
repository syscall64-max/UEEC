#!/usr/bin/env python3
"""Validate all 12 PE_STYLE single-node UEEC files, without NASM/QEMU."""
import pathlib
import re
import struct
import subprocess
import sys

ARCH = [
    ('arm32', 0, 32, 4), ('arm64', 1, 32, 8),
    ('longarm64', 2, 36, 8), ('powerpc32', 3, 40, 4),
    ('powerpc64', 4, 40, 8), ('riscv32', 5, 36, 4),
    ('riscv64', 6, 36, 8), ('sparc32', 7, 40, 4),
    ('sparc64', 8, 40, 8), ('x86', 9, 1, 4),
    ('x86_64', 10, 2, 8), ('s390x', 11, 32, 8),
]


def asm_to_bytes(source):
    out = bytearray()
    for line in source.splitlines():
        line = line.split(';', 1)[0].strip()
        m = re.fullmatch(r'(db|dw|dd)\s+(.+)', line, re.I)
        if not m:
            if line:
                raise ValueError(f'Unexpected assembly directive: {line}')
            continue
        size = {'db': 1, 'dw': 2, 'dd': 4}[m.group(1).lower()]
        for token in m.group(2).split(','):
            out += int(token.strip(), 0).to_bytes(size, 'little')
    return bytes(out)


def check(index, arch, machine, reloc_off, width):
    result = subprocess.run(['./gen_pe', str(index)], capture_output=True,
                            text=True, check=True)
    data = asm_to_bytes(result.stdout)
    assert data[:4] == b'_GM\x01'
    assert struct.unpack_from('<HH', data, 4) == (1, 1)
    node_hdr, fmt = struct.unpack_from('<IH', data, 8)
    assert fmt == 0 and ((node_hdr >> 10) & 255) == machine
    assert (node_hdr & 1) == (1 if arch in ('powerpc32', 'powerpc64', 'sparc32', 'sparc64', 's390x') else 0)
    assert ((node_hdr >> 27) & 31) == 3
    assert [struct.unpack_from('<HI', data, 14+i*6)[1] for i in range(3)] == [1,2,3]
    sizes = [struct.unpack_from('<HHH', data, 32+i*6) for i in range(4)]
    assert [s[0] for s in sizes] == [0,2,1,5]
    assert sizes[0][1] == 64 and sizes[1][1] == 13 and sizes[3][1] == 14
    pos = sum(s[1] for s in sizes[:-1])
    assert len(data) == pos + 14
    assert data[sizes[0][1]:sizes[0][1]+13] == b'Hello world!\n'
    code_at = sizes[0][1] + sizes[1][1]
    expected = bytearray(pathlib.Path('arch', f'{arch}.bin').read_bytes())
    expected[reloc_off:reloc_off+width] = b'\0' * width
    assert data[code_at:code_at+len(expected)] == expected
    reloc = struct.unpack_from('<7H', data, pos)
    assert reloc == (0, 3, 1, 1, 0, 1, reloc_off | (0x1000 if width == 8 else 0))
    print(f'UEEC PE_STYLE: {arch}: node=1 machine={machine} '
          f'CODE={len(expected)} offset=0x{reloc_off:x} ABS{width*8} PASS')
    return data


def main():
    if not pathlib.Path('gen_pe').exists():
        print('Build generator first: make gen_pe', file=sys.stderr)
        return 1
    for i, (name, machine, offset, width) in enumerate(ARCH):
        data = check(i, name, machine, offset, width)
        if name == 'x86_64':
            pathlib.Path('check_native_x86_64.ueec').write_bytes(data)
    print('UEEC PE_STYLE: 12/12 single-node structure + relocation PASS')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (AssertionError, ValueError, OSError, subprocess.CalledProcessError) as err:
        print(f'PE_STYLE validation failed: {err!r}', file=sys.stderr)
        sys.exit(1)
