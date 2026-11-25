#!/usr/bin/env python3
"""Convert a compact or granulated single-node Linux UEEC to a static ELF."""
import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import struct
from tempfile import NamedTemporaryFile


@dataclass(frozen=True)
class Target:
    name: str
    machine: int
    bits: int
    endian: str
    flags: int = 0


# Numeric keys are the UEEC v1 CPU IDs. ELF flags match the stage 20 ABIs.
TARGETS = {
    0: Target("ARM32", 40, 32, "little", 0x05000400),
    1: Target("AArch64", 183, 64, "little"),
    2: Target("LoongArch64", 258, 64, "little", 0x43),
    3: Target("PowerPC32", 20, 32, "big"),
    4: Target("PowerPC64", 21, 64, "big", 2),
    5: Target("RISC-V32", 243, 32, "little", 1),
    6: Target("RISC-V64", 243, 64, "little", 1),
    7: Target("SPARC32", 2, 32, "big"),
    8: Target("SPARC64", 43, 64, "big"),
    9: Target("x86", 3, 32, "little"),
    10: Target("x86-64", 62, 64, "little"),
    11: Target("s390x", 22, 64, "big"),
}
MAX_FILE = 3 * 65535
MAX_CODE = 8192

# Relocation-free entry checks from x86_features.S. On success they fall
# through to the first byte of the original CODE section.
GUARDS = {
    1 << 6: bytes.fromhex(
        "b8 01 00 00 00 0f a2 0f ba e2 1a 72 0e b8 3c 00 "
        "00 00 bf 7e 00 00 00 0f 05 0f 0b"
    ),
    (1 << 12) | (1 << 13): bytes.fromhex(
        "31 c0 0f a2 83 f8 07 72 31 b8 01 00 00 00 0f a2 "
        "81 e1 00 00 00 1c 81 f9 00 00 00 1c 75 1c 31 c9 "
        "0f 01 d0 83 e0 06 83 f8 06 75 0f b8 07 00 00 00 "
        "31 c9 0f a2 0f ba e3 05 72 0e b8 3c 00 00 00 bf "
        "7e 00 00 00 0f 05 0f 0b"
    ),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_ueec(file):
    require(36 <= len(file) <= MAX_FILE and file[:4] == b"_GM\x01",
            "expected a little-endian UEEC v1 file")
    nodes, names, word, fmt = struct.unpack_from("<HHIH", file, 4)
    count = word >> 27
    require((nodes, names, fmt) == (1, 1, 0) and count in (1, 2),
            "expected one SH16 node with CODE and optional METADATA")
    cpu_id = (word >> 10) & 0xFF
    require(cpu_id in TARGETS, "unknown native UEEC CPU")
    target = TARGETS[cpu_id]
    linux_user_program = (2 << 20) | (2 << 18) | (cpu_id << 10) | (1 << 3) | (1 << 2)
    # Bit 5 is a console hint and does not affect the program's code.
    recorded = word & 0x07FFFFFF & ~(1 << 5)
    require(recorded == (linux_user_program | (target.endian == "big")),
            "expected a Linux user-mode CPU program")

    table = 14 + count * 6
    name_offset = table + (count + 1) * 6
    require(name_offset + 4 <= len(file), "truncated UEEC directory")
    mappings = struct.iter_unpack("<HI", file[14:table])
    require(list(mappings) == [(0, i + 1) for i in range(count)],
            "unexpected UEEC section mappings")
    name_size, name_index = struct.unpack_from("<BH", file, name_offset)
    require(name_size > 0 and name_index == 0, "invalid UEEC node name")
    section_types = (0, 1, 13)[:count + 1]
    spans = struct.iter_unpack("<HHH", file[table:name_offset])
    cursor = 0
    data = []
    for expected_type, (section_type, stored, meaningful) in zip(section_types, spans, strict=True):
        require(section_type == expected_type and 0 < meaningful <= stored <= len(file) - cursor,
                "invalid or unsupported UEEC section span")
        data.append(file[cursor:cursor + meaningful])
        cursor += stored
    require(len(data[0]) == name_offset + 3 + name_size and cursor == len(file),
            "inconsistent UEEC section directory")
    code = data[1]
    alignment = 1 if cpu_id in (9, 10) else 2 if cpu_id == 11 else 4
    require(len(code) <= MAX_CODE and len(code) % alignment == 0, "invalid UEEC CODE length")
    guard = b""
    if count == 2:
        require(cpu_id == 10 and len(data[2]) == 28, "unsupported UEEC feature metadata")
        *index, mask, _priority = struct.unpack("<9HQH", data[2])
        require(index == [0, 0, 2, 1, 0, 2, 8, 0, 10] and mask in GUARDS,
                "unsupported UEEC feature requirements")
        guard = GUARDS[mask]
    return target, code, guard


def align(value, boundary):
    return (value + boundary - 1) & -boundary


def build_elf(target, code, guard):
    endian = "<" if target.endian == "little" else ">"
    is_64 = target.bits == 64
    elf_header = "16sHHIQQQIHHHHHH" if is_64 else "16sHHIIIIIHHHHHH"
    program_header = "IIQQQQQQ" if is_64 else "IIIIIIII"
    section_header = "IIQQQQIIQQ" if is_64 else "IIIIIIIIII"
    ehsize, phsize, shsize = (struct.calcsize(endian + fmt)
                            for fmt in (elf_header, program_header, section_header))

    segment_alignment = 0x10000
    base = 0x100000
    entry_offset = align(ehsize + 2 * phsize, 16)
    code_offset = entry_offset + len(guard)
    load_end = code_offset + len(code)
    names = b"\0.text\0.shstrtab\0.ueec.guard\0"
    shoff = align(load_end + len(names), 8 if is_64 else 4)

    def pack(layout, *fields):
        return struct.pack(endian + layout, *fields)

    def program(kind, flags, offset, address, size, alignment):
        if is_64:
            return pack(program_header, kind, flags, offset, address, address, size, size, alignment)
        return pack(program_header, kind, offset, address, address, size, size, flags, alignment)

    def section(name, kind, flags, address, offset, size, alignment):
        return pack(section_header, names.index(name + b"\0"), kind, flags,
                    address, offset, size, 0, 0, alignment, 0)

    sections = [b"\0" * shsize]
    if guard:
        sections.append(section(b".ueec.guard", 1, 6, base + entry_offset, entry_offset, len(guard), 16))
    sections.extend((
        section(b".text", 1, 6, base + code_offset, code_offset, len(code), 1 if guard else 16),
        section(b".shstrtab", 3, 0, 0, load_end, len(names), 1),
    ))
    ident = b"\x7fELF" + bytes((2 if is_64 else 1, 1 if endian == "<" else 2, 1)) + bytes(9)
    header = pack(elf_header, ident, 2, target.machine, 1, base + entry_offset,
                  ehsize, shoff, target.flags, ehsize, phsize, 2, shsize,
                  len(sections), len(sections) - 1)
    headers = header + program(1, 5, 0, base, load_end, segment_alignment)
    headers += program(0x6474E551, 6, 0, 0, 0, 16)
    return (headers + bytes(entry_offset - len(headers)) + guard + code + names
            + bytes(shoff - load_end - len(names)) + b"".join(sections))


def write_executable(path, data):
    pending = NamedTemporaryFile(dir=path.parent, delete=False)
    temporary = Path(pending.name)
    try:
        with pending:
            pending.write(data)
        temporary.chmod(0o755)
        os.link(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, metavar="input.ueec")
    parser.add_argument("output", type=Path, metavar="output.elf")
    args = parser.parse_args()
    try:
        with args.input.open("rb") as source:
            target, code, guard = read_ueec(source.read(MAX_FILE + 1))
        elf = build_elf(target, code, guard)
        write_executable(args.output, elf)
    except (OSError, ValueError) as error:
        parser.exit(1, f"ueec2elf: {error}\n")
    print(f"{target.name}: {len(code)} CODE bytes -> {args.output} ({len(elf)} bytes)")


if __name__ == "__main__":
    main()
