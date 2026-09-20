# UEEC Format Specification

**Version:** 0.1 (draft — the extensibility mechanism this spec documents is
still being stress-tested; expect breaking changes before `1.0`. See
[`docs/UEEC_DESIGN.md`](docs/UEEC_DESIGN.md) for rationale.)

This document specifies the on-disk layout of a UEEC container: the file
header, the node/section model, and the currently defined `design` variants
for the RELOCATION and EXPORT section types. It reflects the reference
implementation in this repository (`01_ueec_info/`, `04_convert/`,
`08_export/`, `09_convert_export/`).

## 0. Conventions

- All multi-byte structures are declared `packed` (no implicit padding).
- Byte order is **explicit and self-declared** per file (see `ueec_hdr.endian`
  below) — a UEEC file states its own endianness rather than assuming the
  host's.
- Several structures carry a 16-bit **`format`** field. This field is
  deliberately an *index* into a small width table, not the width itself:

  | `format` value | width |
  | --- | --- |
  | `0` | 16-bit |
  | `1` | 32-bit |
  | other | 64-bit |

  Every structure that scales with address/count width (section headers,
  node-info mappings that reference large sections, RELOCATION and EXPORT
  section bodies) reuses this same table via its own `format` field. Adding
  a future width tier means adding one new struct variant and one new case
  in the reader/writer switch — the existing 16/32/64-bit code paths are
  untouched. This is the concrete mechanism behind UEEC's extensibility
  claim (see `docs/UEEC_DESIGN.md`).

## 1. File header — `ueec_hdr` (8 bytes)

```c
typedef struct UEEC_PACKED {
    uint8_t  magic[3];        // 0x5F 0x47 0x4D  ("_GM")
    uint8_t  version : 7,
             endian  : 1;     // 0 = little-endian, 1 = big-endian
    uint16_t nodes;           // number of nodes in this file
    uint16_t names;           // number of distinct names in the name table
} ueec_hdr;
```

Every multi-byte field after the header is encoded in the byte order
declared by `endian` — a reader determines endianness from these 8 bytes
before interpreting anything else in the file.

## 2. Nodes — `ueec_node_hdr` (6 bytes each)

A UEEC file holds one or more **nodes**. A node is a self-contained
execution image for one target (architecture + mode); the common use case
today is one node per CPU architecture in a multi-architecture ("fat")
container, but the model does not assume that.

Immediately following the file header is an array of `nodes` fixed-size
node headers:

```c
typedef struct UEEC_PACKED {
    uint32_t raw;      // bitfield, see below
    uint16_t format;   // section-header width selector for this node (§4)
} ueec_node_hdr;
```

`raw` packs the following fields (low bit first):

| bits | field | meaning |
| --- | --- | --- |
| 0 | `encode` | reserved / content-encoding flag |
| 1 | `kernel_mode` | node runs in kernel/privileged mode |
| 2 | `user_mode` | node runs in user mode |
| 3 | `program` | node is a standalone program |
| 4 | `library` | node is a library |
| 5 | `console` | console-mode subsystem hint |
| 6 | `service` | service/daemon subsystem hint |
| 7–9 | `machine_type` | execution-target class, see §2.1 |
| 10–17 | `machine` | specific machine within that class, see §2.2 |
| 18–19 | `system_type` | operating environment class, see §2.3 |
| 20–26 | `system` | specific operating system, see §2.4 |
| 27–31 | `section_count` | number of sections this node owns, **encoded as `real_count − 1`** (a node always owns at least the shared HEADER section plus one more) |

### 2.1 `machine_type`

| value | meaning |
| --- | --- |
| 0 | CPU |
| 1 | MPU |
| 2 | VM (virtual/byte-code machine) |
| 3 | GPU (compute kernel) |
| 4 | FPGA (bitstream) |

Only `CPU` is implemented by the tools in this repository today. The other
four values are reserved placeholders for the non-CPU execution targets
described in `docs/UEEC_DESIGN.md` — defining the enum now, ahead of any
implementation, costs nothing and avoids a later incompatible renumbering.

### 2.2 `machine` (when `machine_type == CPU`)

| value | architecture | value | architecture |
| --- | --- | --- | --- |
| 0 | arm32 | 6 | riscv64 |
| 1 | arm64 | 7 | sparc32 |
| 2 | loongarch64 | 8 | sparc64 |
| 3 | powerpc32 | 9 | x86 |
| 4 | powerpc64 | 10 | x86-64 |
| 5 | riscv32 | | |

These 11 values are exactly the architectures exercised by this
repository's QEMU validation matrix (see the root `README.md`).

### 2.3 `system_type`

| value | meaning |
| --- | --- |
| 0 | bare-metal |
| 1 | embedded |
| 2 | general-purpose OS |
| 3 | hypervisor |

### 2.4 `system` (when `system_type == general-purpose OS`)

| value | OS |
| --- | --- |
| 0 | Android |
| 1 | iOS |
| 2 | Linux |
| 3 | macOS |
| 4 | Windows |

Only `Linux` is exercised by the current loaders and validation harness.

## 3. Node → section mapping — `ueec_node_info` (6 bytes each)

Section index `0` is a single, **mandatory, global HEADER section**, implicitly
owned by every node — it is never repeated in the mapping table. Every
*other* section a node owns is one entry in a flat array of `ueec_node_info`
records, immediately following the node-header array:

```c
typedef struct UEEC_PACKED {
    uint16_t node;      // owning node index
    uint32_t section;   // global section index (never 0 — that's HEADER)
} ueec_node_info;
```

The total number of entries is `Σ (node.section_count − 1)` across all
nodes (`section_count` already stores `real_count − 1`, so this is
`Σ node.section_count`, i.e. the raw field values summed directly).

## 4. Section headers (format-selected width)

Immediately following the node-info array is an array of section headers,
one per distinct global section index referenced (plus the implicit HEADER
section at index 0). The **width** of each section-header entry is chosen
per file by `ueec_node_hdr.format` (currently required to be the same for
every node in a file — mixed-format files are rejected by the reference
parser pending a defined wire rule for that case):

```c
typedef struct UEEC_PACKED { uint16_t type; uint16_t section_size, data_size; } ueec_sh16;  //  6 bytes  (format = 0)
typedef struct UEEC_PACKED { uint16_t type; uint32_t section_size, data_size; } ueec_sh32;  // 10 bytes  (format = 1)
typedef struct UEEC_PACKED { uint16_t type; uint64_t section_size, data_size; } ueec_sh64;  // 18 bytes  (format = other)
```

`type` identifies the section's content model:

| value | name | implemented today |
| --- | --- | --- |
| 0 | HEADER | yes (implicit, mandatory) |
| 1 | CODE | yes |
| 2 | INIT_DATA | partially (a single generic DATA section is exercised) |
| 3 | UNINIT_DATA | reserved |
| 4 | CONSTANT_DATA | reserved |
| 5 | RELOCATION | yes — §6.1 |
| 6 | EXPORT | yes — §6.2 |
| 7 | IMPORT | reserved (planned: typed import/export, see `docs/UEEC_DESIGN.md`) |
| 8 | DEBUG | reserved (planned) |
| 9 | TLS | reserved |
| 10 | STACK | reserved |
| 11 | HEAP | reserved |
| 12 | RESOURCE | reserved |
| 13 | METADATA | reserved (planned: architecture-oriented metadata, see `docs/UEEC_DESIGN.md`) |

`section_size` and `data_size` are given in the width selected by `format`
(§4's own table above, independent of any `format` field inside the
section's own content — see §6).

## 5. Name table

Following the section-header array, in order:

1. `name_lengths[names]` — one `uint8_t` per distinct name, its length in bytes.
2. `node_name_index[nodes]` — one `uint16_t` per node, indexing into the name table.
3. `name_data[]` — the concatenated name bytes, in the order given by `name_lengths`.

## 6. Section-content designs

Two section types currently have more than one defined wire encoding, each
tagged by its own leading `design` field so a reader can dispatch correctly
without out-of-band knowledge. This `design`-tag mechanism — plus the
*lossless converters* this repository provides between the designs of a
given section type — is UEEC's central novelty; see `docs/UEEC_DESIGN.md`.

### 6.1 RELOCATION section

Header:

```c
typedef struct UEEC_PACKED { uint16_t format; uint16_t design; } ueec_reloc;
```

`design`:

| value | name |
| --- | --- |
| 0 | FLAT |
| 1 | BLOCK_4096 |
| 2 | BLOCK_N |
| *(3, PE-style)* | **deliberately excluded** — see below |

Followed by a couple header giving the `{target, source}` node-local
resident-section pair and an entry count:

```c
typedef struct UEEC_PACKED { uint16_t target:5, source:5, format:6; uint16_t count; } ueec_reloc_couple16; // format(top-level) == 0
typedef struct UEEC_PACKED { uint16_t target:5, source:5, format:6; uint32_t count; } ueec_reloc_couple32; // format(top-level) == 1
typedef struct UEEC_PACKED { uint16_t target:5, source:5, format:6; uint64_t count; } ueec_reloc_couple64; // format(top-level) == other
```

**FLAT** (`design == 0`): `count` entries follow directly, each a 16-bit
`{offset:12, type:4}` record.

**BLOCK_4096 / BLOCK_N** (`design == 1` / `2`): `count` blocks follow, each
a `{index, count}` pair (width per the top-level `format` field) followed by
that many `{offset, type}` entries packed into the remaining bits of an
index-width word. `block.index` is defined as an **absolute base offset**
(not a page number), which makes decoding self-contained — a reader does
not need to know the producer's page size to reconstruct exact offsets.
This is what makes round-trip conversion between BLOCK_4096 and BLOCK_N
exact.

> **Why the PE-style 4th design is out of scope for now:** a fourth,
> PE-style relocation design (keyed by `section` rather than by
> `{target, source}`) only makes sense for single-node files where every
> section of that node is treated as one unified, page-aligned virtual
> image — a different structural assumption from the multi-node model
> above. It is left for a separate, later piece of work rather than folded
> into the current universal converter.

Reference implementation: `04_convert/convert.c` (decode → common
intermediate `{target, source, offset, type}` list → re-encode into any of
the three designs), validated round-trip-exact (byte-identical `cmp`)
across all 11 architectures.

### 6.2 EXPORT section

Header:

```c
typedef struct UEEC_PACKED { uint16_t format; uint16_t design; } ueec_export;
```

`design`:

| value | name |
| --- | --- |
| 0 | COMPACT_GROUP |
| 1 | SIMPLE_FLAT |
| 2 | ORDINAL |

All three logically export `{section, offset, name}` (COMPACT_GROUP,
SIMPLE_FLAT) or `{section, offset, ordinal}` (ORDINAL) entries; they differ
only in wire layout.

**COMPACT_GROUP** (`design == 0`): entries are grouped by name length
(`groups` groups, each `{length, count}`), each group's entries sorted by
name for a two-level binary search (by length, then by name within the
group):

```
ueec_export → ueec_export_groups{16,32,64}                        (format-width count of groups)
  → ueec_export_group{16,32,64}[groups]                            (length-sorted)
    → ueec_export_info{16,32,64}[group.count]                      (name-sorted within group)
    → name_data[group.count × group.length]                        (fixed-width names, same order as info[])
```

`ueec_export_info` carries a 5-bit node-local resident-section index and an
`offset` (export address, relative to the section start) at the selected
width.

**SIMPLE_FLAT** (`design == 1`): a single, name-sorted array for a
one-level binary search:

```
ueec_export → ueec_export_names{16,32,64}                          (format-width count of names)
  → ueec_export_flat{16,32,64}[names]                               (name-sorted)
  → name_data[]                                                     (all name text, concatenated)
```

`ueec_export_flat` carries `{section:5, length:11}`, `offset_rva`
(export address) and `offset_name` (offset into the trailing `name_data`
blob), each at the selected width.

**ORDINAL** (`design == 2`): a single, ordinal-sorted array, no name field
at all — a modernized, simplified analogue of PE's
`IMAGE_EXPORT_DIRECTORY` (one flat array rather than PE's separate
address/name/ordinal tables, since ordinal-only export is rare in practice):

```
ueec_export → ueec_export_count{16,32,64}
  → ueec_export_ordinal{16,32,64}[count]                            (ordinal-sorted)
```

> **Why COMPACT_GROUP ↔ SIMPLE_FLAT convert losslessly but ORDINAL does
> not:** COMPACT_GROUP and SIMPLE_FLAT store exactly the same
> `{section, offset, name}` data in different layouts, so converting
> between them is lossless in both directions. ORDINAL is a *different
> keyspace* — it has no name field at all. Going from a named design to
> ORDINAL would require inventing ordinal values that aren't stored
> anywhere; going from ORDINAL to a named design would require inventing
> names that don't exist in the file. Neither is a conversion — both would
> be fabrication. `convert_export` therefore only accepts
> `--design compact|flat`, and refuses `ordinal` explicitly.

Reference implementation: `09_convert_export/convert_export.c` (decode →
common intermediate `{section, offset, name, namelen}` list → re-encode
into COMPACT_GROUP or SIMPLE_FLAT), validated round-trip-exact across all
11 architectures, including an executed machine-code function call through
the loader (not just value lookups).

## 7. Implementation status

| Area | Status |
| --- | --- |
| File header, node/section model (§1–§5) | Implemented, `01_ueec_info/` |
| RELOCATION: FLAT / BLOCK_4096 / BLOCK_N, universal converter | Implemented and validated, `04_convert/` |
| EXPORT: COMPACT_GROUP / SIMPLE_FLAT, universal converter | Implemented and validated, `09_convert_export/` |
| EXPORT: ORDINAL | Parseable; deliberately not converted (see §6.2) |
| RELOCATION: PE-style 4th design | Not implemented (see §6.1) |
| IMPORT, DEBUG, METADATA section types | Defined in the enum (§4), not yet implemented — planned work |
| MPU / VM / GPU / FPGA `machine_type` values | Defined in the enum (§2.1), not yet implemented — planned work |

## 8. Versioning

This spec is `0.x` while the `design`-tag extensibility mechanism is still
being stress-tested against structurally different section types (typed
import/export, debug information, architecture metadata — see
`docs/UEEC_DESIGN.md`). It will move to `1.0` once that work is complete
and no further mechanism-level changes are anticipated.
