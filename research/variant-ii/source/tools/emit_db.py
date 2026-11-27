#!/usr/bin/env python3
"""Strict byte emitter for the existing NASM db-only fixtures, not a general assembler."""
import pathlib, re, sys
src, dst = map(pathlib.Path, sys.argv[1:])
data = bytearray()
for number, line in enumerate(src.read_text().splitlines(), 1):
    line = line.split(';', 1)[0].strip()
    if not line or re.fullmatch(r'\[bits (16|32|64)\]', line):
        continue
    if not re.fullmatch(r'db\s+0x[0-9a-fA-F]{1,2}(\s*,\s*0x[0-9a-fA-F]{1,2})*', line):
        raise SystemExit(f'{src}:{number}: unsupported directive: {line}')
    data.extend(int(v, 16) for v in line[2:].split(','))
if not data:
    raise SystemExit(f'{src}: empty fixture')
dst.parent.mkdir(parents=True, exist_ok=True)
dst.write_bytes(data)
