# UEEC — Universal Extensible Executable Container

UEEC is a compact, architecture-neutral binary container format for executable
code — an alternative to ELF/PE/Mach-O built around explicit, self-declared
endianness and pluggable, versioned encodings for individual structural
sections, instead of one fixed layout per platform. Each structurally
variable section carries an explicit **`design` tag**, so the same logical
data (a relocation table, an export table, ...) can be stored in more than
one wire-format, and the format can grow new section types and new encodings
without breaking existing readers.

This repository holds the format specification, the reference toolchain, and
the multi-architecture validation harness. It does not yet include a kernel
loader or virtual-machine layer — see [Roadmap](#roadmap) below.

> **Status:** early, actively developed. The format is versioned `0.x` while
> its extensibility mechanism (see [`FORMAT_SPECIFICATION.md`](FORMAT_SPECIFICATION.md))
> is still being stress-tested; expect breaking changes before `1.0`.

## Why UEEC

ELF, PE/COFF and Mach-O each fix their relocation and export/import table
layouts at the format's design time. Extending or replacing one of these
tables — to support a more compact export representation, say, or a new
relocation encoding for an emerging ISA — normally requires either a new
format version that breaks old tooling, or vendor-specific extensions bolted
on outside the spec. UEEC's `design`-tag mechanism, together with
**universal, lossless converters** between the designs of a given section,
is meant to make that kind of change routine rather than format-breaking.

See [`docs/UEEC_DESIGN.md`](docs/UEEC_DESIGN.md) for the full design
rationale and comparison with prior art.

## What's in this repository

The project is developed as a sequence of self-contained stages, each in
its own directory with its own `Makefile` and `README.md`:

| Path | Contents |
| --- | --- |
| `FORMAT_SPECIFICATION.md` | The versioned container format spec: header, endianness, node/section model, currently defined `design` variants |
| `docs/UEEC_DESIGN.md` | Design rationale, novelty, and how UEEC compares to ELF/PE/Mach-O and prior art |
| `00_shell_code/` | Earliest prototype — minimal shell-code-style payload |
| `01_ueec_info/` | Reference reader (`ueec-info`): parses the `_GM` header, nodes, sections and name table; validated on 11 architectures |
| `02_ueec_load/` | First working Linux loader for a `_GM`-header UEEC file |
| `03_reloc/` | First RELOCATION section (`FLAT` design) integrated into the core format |
| `04_convert/` | **Universal RELOC converter** — decode → intermediate → encode between `FLAT`, `BLOCK_4096`, `BLOCK_N`; round-trip validated on 11 architectures |
| `05_align/` | Section-granularity/alignment tooling |
| `06_extract/` | Extracts a single node out of a multi-node UEEC file into a standalone one-node file |
| `07_pe/` | The PE-style 4th RELOC design (`reloc_v4.h`) — kept separate; see `FORMAT_SPECIFICATION.md` §6.1 for why it's out of scope for the universal converter |
| `08_export/` | The three EXPORT designs (`COMPACT_GROUP`, `SIMPLE_FLAT`, `ORDINAL`) — parsers, generators, loaders |
| `09_convert_export/` | **Universal EXPORT converter** — decode → intermediate → encode between `COMPACT_GROUP` and `SIMPLE_FLAT`; round-trip validated on 11 architectures, including an executed machine-code function call through the loader |

## Building

Each stage directory is self-contained and builds with `make`. For example,
the universal EXPORT converter and its full architecture matrix:

```sh
cd 09_convert_export
make all
```

This builds the test-binary generator, the converter, and a loader for
every supported architecture, then exercises them (via QEMU user-mode
emulation) against the test binaries under `arch/`, in both EXPORT
designs. See each directory's own `README.md` for its specific `make`
targets (e.g. `01_ueec_info/` also has `make native` for a quick local-only
test, and `04_convert/` / `09_convert_export/` both have
`make run-all-directions` for a single-architecture round-trip check).

**Requirements:** NASM, the relevant cross-compilers (`*-linux-gnu(eabihf)-gcc`
per target architecture) and QEMU user-mode emulators (`qemu-<arch>`).
Static linking is used throughout, so a target sysroot is normally not
required.

## Validation

Correctness is demonstrated end-to-end: working loaders execute real UEEC
binaries — including an actual machine-code function call — under QEMU on
11 CPU architectures (x86, x86-64, ARM32, ARM64, RISC-V32/64, PowerPC32/64,
SPARC32/64, LoongArch64), with byte-identical round-trip checks
(`encode(decode(x)) == x`).

## Roadmap

This repository covers the first stage of a longer-term plan: format
specification, reference toolchain hardening, and three new section types
(typed import/export, debug information, architecture metadata). A Linux
kernel-level loader, a microcontroller loader ecosystem, and a
virtual-processor language/VM are planned as later, separately-scoped
stages — see `docs/UEEC_DESIGN.md` for a short summary.

## License

MIT — see [`LICENSE`](LICENSE). (A future Linux kernel-module loader, in a
later stage of this project, is expected to be GPLv2, as is conventional for
kernel-space code; it is out of scope for this repository.)

## Author

George Modebadze — Tbilisi, Georgia.
