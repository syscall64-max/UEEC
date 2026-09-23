# UEEC design rationale

This document explains *why* UEEC is built the way it is, and how it
compares to ELF, PE/COFF and Mach-O. For the exact on-disk layout, see
[`FORMAT_SPECIFICATION.md`](../FORMAT_SPECIFICATION.md).

## The problem

ELF, PE/COFF and Mach-O each fix their relocation and export/import table
layouts at the format's design time. Extending or replacing one of these
tables — to support a more compact export representation, say, or a new
relocation encoding for an emerging ISA — normally requires either a new
format version that breaks old tooling, or vendor-specific extensions
bolted on outside the spec (as has happened repeatedly in ELF's history
with GNU extensions, and in Mach-O with load-command proliferation).

The 32-bit → 64-bit transition is a concrete, expensive precedent: ELF, PE
and Mach-O each had to introduce a near-complete parallel format — ELF32 vs.
ELF64, PE32 vs. PE32+, and comparable duplication in Mach-O's 32-bit and
64-bit headers and load commands — because the address/offset width is
baked directly into the structures themselves rather than carried by a
separate, independently-sized selector field.

## UEEC's answer: `design` tags + universal converters

Every structurally variable section in UEEC carries an explicit `design`
tag (see `FORMAT_SPECIFICATION.md` §6), so the same logical data — a
relocation table, an export table — can be stored in more than one
wire-format. A UEEC producer can pick the most compact encoding for its
data (say, a two-level grouped export table for a library with many
same-length symbol names), while a consumer that only understands a
simpler encoding can losslessly convert into it first, with no
coordination between producer and consumer beyond the shared spec.

Crucially, this repository does not just define alternative encodings — it
delivers **universal, lossless converters** between the designs of a given
section, validated for round-trip correctness (`encode(decode(x)) == x`,
checked byte-for-byte). We make a narrow, checkable claim here: we are not
aware of a production binary format that ships validated, lossless
converters between multiple wire-encodings of the same relocation or
export table — as distinct from formats that merely allow extension, or
that support several encodings without converting between them. Related
but distinct prior art: LLVM's bitcode versions its own encoding but does
not convert between versions; WebAssembly's custom-section mechanism
allows extension but not alternative encodings of the *same* required
data; ELF's `SHT_*` section-type mechanism is closer in spirit but has no
equivalent converter tooling and no formal notion of lossless design
equivalence.

### `format` as an index, not a value

Every width-dependent structure in UEEC (section headers, RELOCATION and
EXPORT bodies) carries a `format` field that is deliberately an *index*
into a lookup table (`0` → 16-bit, `1` → 32-bit, else → 64-bit), not the
literal width. Adding a new width tier means defining one new struct
variant and adding one new case to the reader/writer switch — without
touching any existing 16/32/64-bit code path. This is the concrete
mechanism meant to avoid a repeat of the ELF32/ELF64-style format
duplication described above, and it is deliberately not yet proven beyond
16/32/64-bit — that is future validation work, not a settled result.

## What's already validated

Correctness is demonstrated end-to-end: working loaders execute real UEEC
binaries — including an actual machine-code function call, not just value
lookups — under QEMU on 11 CPU architectures (x86, x86-64, ARM32, ARM64,
RISC-V32/64, PowerPC32/64, SPARC32/64, LoongArch64), with byte-identical
round-trip checks for both the RELOCATION and EXPORT converters.

This breadth of testing was not free: it surfaced real, architecture-specific
bugs that a single-architecture test suite would not have caught, including
ARM32 Thumb-interworking misdetection, a PowerPC64 ELFv1 function-descriptor
calling-convention mismatch, and DATA-section content endianness being
independent of the format's own structural endianness bit. Two of these
recurred across independently-developed sibling tools (the RELOC and EXPORT
converters), which is itself evidence that this class of bug is easy to
introduce silently and needs to stay under continuous multi-architecture
test coverage, not a one-time check.

**A single container holding multiple target architectures at once is now
validated, not just a stated design goal.** `02_ueec_load/ueec.asm` builds one
physical `.ueec` file containing 11 architecture-specific CODE nodes (ARM32,
ARM64, LoongArch64, PowerPC32/64, RISC-V32/64, SPARC32/64, x86, x86-64), all
sharing one HEADER and one logical name. `02_ueec_load/ueec_load.c` is a
single loader implementation, cross-compiled once per target, that opens
that *same* file, selects the node matching its own host
`(machine_type, machine, system_type, system)`, and loads only that node.
Tested directly (native + QEMU user-mode) on 9 of the 11 architectures —
x86, x86-64, ARM32, ARM64, LoongArch64, PowerPC32, PowerPC64, RISC-V64,
SPARC64 — each correctly selects its own node from the shared file and runs
it to a real `exit(0)` syscall. (RISC-V32 and SPARC32 were not exercised in
every environment, for missing cross-toolchain components, not a code
defect.)

**The three loading strategies are now implemented and exercised, not just
named.** `02_ueec_load/ueec_load.c` supports `--load-mode
auto|monolithic|scattered|hybrid`: monolithic maps the HEADER and every
selected section into one contiguous anonymous mapping; scattered maps each
section into its own independent mapping at an address chosen by the kernel;
hybrid maps the HEADER separately from one combined mapping of the
remaining sections. All four modes (including `auto`, which tries
monolithic and falls back to scattered) build a correct runtime image and
locate a valid entry point from the same input file — `make test-modes`
exercises all four end to end.

## Forward-compatibility, stated but not yet built

UEEC's section and node model is designed with further additions in mind
that are **not yet implemented or empirically validated** — they shape the
design (see `machine_type` and the section-type enum in
`FORMAT_SPECIFICATION.md`) without being claimed as working today:

- the section-type space is deliberately left open for future non-CPU
  execution targets (MPU, VM byte-code, GPU compute kernels, FPGA
  bitstreams) — see the reserved `machine_type` values;
- the width model is meant to let future 128-bit or 512-bit extensions be
  added as new, additional structures alongside the existing ones, rather
  than by revising them.

## Open technical questions

- **Generalising beyond two section types.** RELOCATION and EXPORT have
  proven to support multiple lossless designs. It is not yet established
  that the same `design`-tag approach scales cleanly to structurally
  different kinds of section — a typed import/export model (carrying
  variables, structures, functions and classes, not just flat named
  symbols — for example, today's flat export table can say a symbol
  `Point_add` exists at some offset, but not that it takes two `Point`
  structs and returns one), a debug-information section, and an
  architecture-oriented metadata section. These are the next concrete
  tests of the mechanism.
- **Round-trip correctness at scale.** Current validation uses hand-built
  demo binaries with a handful of entries. Hardening the reference
  library further means fuzzing decoders against malformed input and
  property-based round-trip testing, materially more effort than the
  current demo-scale `cmp`-based checks.

## Roadmap beyond this repository

This repository covers the format specification and reference toolchain.
A longer-term plan exists beyond it — a Linux kernel-level loader
(`binfmt`-style, analogous to `binfmt_elf`), a microcontroller loader
ecosystem, and a virtual-processor instruction language and VM, later
ported to Windows and macOS — spanning several further years across
multiple, separately-scoped stages. Each later stage is intended to be
proposed and published on its own once the prior stage is validated, not
bundled into this repository.

## License

This repository (format specification + reference toolchain) is MIT
licensed — see [`../LICENSE`](../LICENSE). A future Linux kernel-module
loader, as a later, separate stage of this project, is expected to be
GPLv2, as is conventional for kernel-space code.
