# CPU architecture metadata — experimental implementation

This update implements the previously reserved METADATA section (type 13)
in the standalone `12_shared_metadata/` example. It adds a typed description
of each node's required CPU instruction groups and selection priority.
It does not change the `_GM` core header, node header, global section mapping,
or existing stages 00–09. This is a pre-1.0 prototype, not a stable registry.

## Implemented representation

The default example has 12 CPU architectures and 29 leaf CODE variants. All
29 nodes map to one physical METADATA section, at global index 30. The
generator places it at EOF; the reader locates it through section descriptors
and does not require that physical location.

```text
METADATA header: uint16 format, uint16 design
record count:   uint16 / uint32 / uint64
index:          count records followed by one END record
data:           concatenated record bodies
```

`format` values 0, 1 and 2 select 16-, 32- and 64-bit count/offset fields.
Other values are rejected by this prototype. `design=1` is node-indexed:

```c
typedef struct UEEC_PACKED {
    uint16_t node_index;
    uint16_t type;
    uint32_t offset; /* this example is the format=1 index entry */
} ueec_md_node_element32;
```

The node index is **always uint16_t**, independently of count/offset width.
Index entry sizes are 6, 8 and 12 bytes. Offsets are relative to the record
data area, after the complete index table. A record's length is the difference
between its offset and the following entry's offset. The first offset is zero.
The END entry has node_index=0, type=0 and offset=data_size; it is a sentinel,
not metadata belonging to node zero. The count excludes this sentinel.

All multibyte metadata fields use the container's declared endianness.
Target CODE/data endianness is separate. C bitfield structures describe named
capabilities for programmers; the wire is serialized with explicit integer
masks and byte-order operations, never by writing bitfield memory directly.

## Record types and capabilities

Every node in this demonstration has a REQUIRED record containing a uint64_t
mask in its CPU family's bit space. PRIORITY is type 2 with a uint16_t body;
it defaults to zero when absent. The default carrier contains 58 records:
29 REQUIRED and 29 PRIORITY records, followed by END.

| REQUIRED type | CPU family | Examples of described groups |
| --- | --- | --- |
| 1 | x86 / x86-64 | x87, MMX, SSE family, AVX/AVX2/AVX-512 groups, FMA, BMI, AES, SHA, random-number and AMX groups |
| 0x100 | ARM32 | NEON, VFP generations, divide, AES/PMULL, SHA, CRC |
| 0x101 | ARM64 | NEON, SVE/SVE2, FP, atomics, cryptographic and dot-product groups |
| 0x102 | LoongArch64 | LSX, LASX, FPU, UAL, CPUCFG |
| 0x103 | PowerPC32 / PowerPC64 | AltiVec, VSX, ISA levels, DFP, crypto, HTM |
| 0x105 | RISC-V32 / RISC-V64 | RVV, I/M/A/F/D/C, Zba/Zbb/Zbs |
| 0x107 | SPARC32 / SPARC64 | VIS generations, POPC, FMAF |
| 0x10b | S390X | VX generations, MSA, STFLE, DFP |

The complete bit definitions are in `12_shared_metadata/ueec_metadata.h` and
`ueec_capabilities.h`. Presence in this catalog does not claim that every
instruction group is detected, enabled or executed by the example. AMX is
reported where available but is not enabled for execution by this demo.
The S390X machine value 11 is used locally by this example; older stages and
their existing machine catalogs have not been upgraded in this metadata PR.

## Selection and execution behavior

The loader filters nodes by CPU architecture, Linux environment, CODE byte
order and this example's ABI/application profile. It then requires:

```c
(required & ~os_usable) == 0
```

Among compatible nodes, the highest author-assigned priority wins. This is
an ordering policy, not a benchmark or a guarantee of best performance.
`--technology NAME` requests a technology; incompatibility is an error unless
`--fallback` is supplied. `--groups`, `--list-technologies`, `--host-only`,
`--no-run` and the test-only reducing `--limit-isa` option expose the decision.

The loader validates every physical metadata section and every indexed record,
including foreign-architecture and lower-priority nodes, before execution.
It rejects invalid node ownership/indexes, duplicate or unknown record types,
wrong record lengths, missing REQUIRED, invalid sentinels, nonmonotonic or
out-of-range offsets, truncated tables and oversized counts. An unknown
requirement bit makes a node incompatible rather than inventing support.

Metadata is read during inspection and selection. Only the chosen CODE is
copied into its execution mapping; permissions change from RW to RX. The
whole-file inspection mapping is released before the leaf function is called.
The emitted execution marker is `resident_sections=1 metadata_resident=no`.
EOF placement by itself does not establish nonresidency; loader policy does.

## Compatibility and footprint

The new reader also accepts `design=0`, the per-node indexed representation
whose entries contain type/offset without a node index. A design-0 metadata
section must have exactly one owner. The generator can produce both layouts
using `--metadata-layout shared|per-node`.

Older readers are not claimed to understand the new section layouts. Existing
stages 00–09 are unchanged and remain separate. Supported count/offset widths
and little/big-endian encodings are tested for both metadata designs.

With identical 29 CODE payloads, the example's default file is **5719 bytes**;
the equivalent per-node-metadata carrier is **6049 bytes**. This 330-byte
difference measures metadata consolidation within UEEC, not a size benchmark
against PE, ELF or Mach-O.

## Reproduction and validation scope

```sh
cd 12_shared_metadata
make test
ASAN_OPTIONS=detect_leaks=0 make test-sanitize
./build/ueec-loader --list-technologies build/sum.ueec
./build/ueec-loader --groups
```

Native x86-64 and sanitizer results for this publication are recorded in
`12_shared_metadata/VALIDATION.md`. The project author's previously supplied
12-target QEMU plus native transcript is separately labelled; those cross
targets were not rerun in the publication environment. Full reproduction is
`make test-all` with all required cross-compilers and QEMU installed.

This update publishes the CPU metadata implementation and its standalone
validation example only. It does not publish TLS delivery, microcontroller,
board/PCB, FPGA, VM or kernel-loader work, and it does not implement a general
application linker/import loader. New code and documentation were prepared
with AI assistance; this is not an independent human code review.
