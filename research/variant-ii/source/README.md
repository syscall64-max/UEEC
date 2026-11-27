# UEEC — Variant II (stages 00–10)

Research workspace, not a released product version. The source of the migrated information utility is the existing Variant I implementation. Stage 00 is retained unchanged from the preceding package.

## Research rationale — Variant II

During experimental development of Variant I, a structural ambiguity was identified: the section-header format was stored per node, while physical sections can be shared by multiple nodes. Nodes declaring different formats could therefore disagree on the interpretation of the same section header. Variant II moves the 16-bit format field to the global header, increasing its size from 8 to 10 bytes and reducing the node header from 6 to 4 bytes. The three-byte magic and remaining structure layouts are retained. The global endian indicator governs decoding of multibyte header fields. The wire discriminator 2 distinguishes incompatible layouts; it is not a released product version. Variant I and its research results remain unchanged.

## Commands

From the root or any included stage directory: `make`, `make all`, `make build`, `make test`, `make sanitize`, `make clean`, `make help`, or `make x86_64` (any architecture from `mk/architectures.mk`). Root commands cover all eleven included stages. Cross-compilers and QEMU are required; missing dependencies fail the test. Sanitizers run on native x86-64 only.

Stage 01 uses Python 3 to generate deterministic UEEC fixtures from the shared binary resources. The optional NASM example is retained but is not used by the automated test and was not assembled here. See `01_ueec_info/README.md`.

## Validation and scope

User logs confirm stages 00–02 pass on all twelve architecture targets (204 information cases and 1212 loader cases). GCC 14.3 with Binutils 2.46 emitted four LoongArch64 assembler warnings in stages 01–02. The user's clean GCC 15.2 retest passed all three LoongArch64 targets without warnings; the common toolchain now selects GCC 15. No C workaround or warning suppression was applied.

Stage 03 migrates the existing SECTION_FLAT relocation example to the Variant II global SH16 header. The generator builds one container containing 12 nodes, 36 node-section references and 26 sections, including one shared data section. Native x86-64 tests verify structure and execute monolithic and scattered modes; optimized and ASan/UBSan runs passed. User-provided stage 03 log confirms 12/12 architecture targets, 24 subprocess checks plus structural assertions, no warnings in the supplied log. This stage is intentionally limited to the original SH16 / little-endian container / SECTION_FLAT fixture, with architecture-specific code encoding; further widths, container byte orders and relocation designs are not covered by this checkpoint. Sanitizer leak detection is disabled for the legacy stage; tests are not exhaustive security validation. No GitHub changes. Stage 04 retains the width/endian-specialized relocation loader with forced-inline apply_reloc. Native x86-64 passed six conversions, twelve loader executions, four byte-identical comparisons and structural assertions, in optimized and ASan/UBSan builds. The converter accepts only little-endian SH16 containers on a little-endian host; the fixture does not establish full width/endian coverage. User stage 04 log confirms all 12 architecture targets (216 subprocess checks plus structural assertions). Stage 05 migrates granulation and degranulation; tests use granularities 1, 16, 512, 4096 and 8192, verify zero padding, unchanged payloads and node references, byte-identical round trips and both loader modes. Four invalid granularity arguments are rejected. Fixed copying of the name table to exclude header padding and added granularity/SH16 size checks. Native x86-64: 24 subprocess checks plus structural assertions passed in optimized and ASan/UBSan builds (leak detection disabled). User stage 05 log confirms 12/12 targets, 288 subprocess checks plus structural assertions. The align utility remains limited to little-endian SH16 on a little-endian host; malformed input validation is not exhaustive. Stage 06 extracts all 12 nodes and checks node words, references and payloads. It preserves section_size and data_size separately for padded sections, expands the node selection array and supports up to 31 explicit section references. Tests additionally extract a 512-byte granulated fixture, run both loader modes, and reject missing and ambiguous selectors. Native x86-64: 20 subprocess checks plus structural assertions passed in optimized and ASan/UBSan builds (leak detection disabled). User stage 06 log confirms 12/12 architecture targets (240 subprocess checks plus structural assertions). Extractor is restricted to little-endian SH16 on a little-endian host; malformed-file validation is not exhaustive. Stage 07 migrates PE_STYLE (design 3), an image-base-relative relocation design, not a Windows PE file converter. It retains single-node, monolithic-only execution and SH16. All 12 generated little-endian fixtures are checked for exact relocation records, node references and code bytes with zero addend. Native x86-64 passed one execution and five rejection checks in optimized and ASan/UBSan builds (leak detection disabled). User stage 07 log confirms 12/12 targets, 72 subprocess checks plus structural assertions. Malformed-file validation is not exhaustive.

Stage 08 migrates COMPACT_GROUP and ORDINAL export demonstrations, preserving the export record layouts. The shared export resources contain 12 architecture-specific binaries. Each architecture target checks 24 generated single-node SH16 little-endian container layouts, including code and data payloads, and executes the two export loaders for that target. Native x86-64 optimized and ASan/UBSan tests passed; leak detection is disabled. Root native regression stages 00–08 passed. User stage 08 log confirms all 12 architecture targets (24 subprocess checks plus structural assertions). SIMPLE_FLAT conversion is implemented in stage 09. Malformed input validation is not exhaustive. Import is not implemented.

Stage 09 migrates the existing COMPACT_GROUP to SIMPLE_FLAT converter and reverse conversion. Each architecture target checks 36 host conversions over 12 fixtures, 24 byte-identical round-trip comparisons, structural assertions, and four selected-architecture loader executions checking answer() = 42 and exported data values. Native x86-64 passed 40 subprocess checks in both optimized and ASan/UBSan builds (leak detection disabled); root native regression 00–09 passed. Cross execution is pending. Converter scope: SH16 little-endian containers on a little-endian host; malformed-input validation is not exhaustive. ORDINAL conversion is not covered. Import remains unimplemented.


Stage 10.1 introduces the first named IMPORT design, COMPACT_GROUP, for external single-node UEEC libraries. The loader binds function and data addresses and executes a real target-code program. It tests both existing named EXPORT designs, 16/32/64-bit metadata and both container byte orders, independently mixed widths, and the unchanged program after stage 09 EXPORT conversion. Native x86-64: 13 positive executions, 36 rejections, 2 host conversions (51/51 checks), in optimized and ASan/UBSan loader builds. Other 11 architectures remain unverified here. Internal imports, merging, granulation, ORDINAL imports, dependency graphs, relocation integration and performance research are deferred. See `10_import/README.md` for the exact wire layout and limits.

## Stage 10.3 — Mixed import

`10_import_mixed` preserves the Stage 10.2 loader and adds mixed internal/external fixtures in both directions, swapped export designs and missing-external-library rejection. Native and ASan/UBSan: 105/105 each. Full 12-target matrix expected 1260 checks; remaining targets unverified here. Stage 10.2 user log reported 12/12 architectures, 828/828 checks (including 24 host conversions).

## Stage 10.4

See `10_merge/README.md`. Native: 177 matrix checks passed; full 12-target matrix pending (2124 checks).

### Stage 10.5 — Granulation of merged containers
`cd 10_granulation && make all` repeats the previous 177 checks per target and adds 72 positive and 60 negative checks (309 total per target). `ueec_align.py input.ueec output.ueec --granularity 4096` rounds section sizes, including header, without changing data sizes; granularity 1 removes padding. Same wire format and Stage 10.4 loader. Full merge only in new execution tests; no direct mmap or shared-section loading yet. Native x86-64: 309/309 passed. Other targets pending.

## Stage 10.6
See [Unified Loading](10_unified_loading/README.md).

## Stage 10.7.1

Experimental fixed-address monolithic loading: see [10_fixed_loading](10_fixed_loading/README.md). Native and sanitizer checks: 1125 each. Cross validation pending.

### Stage 10.7.2 — Prelink (experimental)
See `10_prelink/README.md`. Native x86-64 and ASan/UBSan: 1485 checks each, zero failures. The 11 other target runners remain to be tested.

### Stage 10.8: Direct Mapping & Execution
See `10_direct_mapping/README.md`. File-backed private fixed mapping of page-aligned prelinked containers; no explicit section copies. Native x86-64 1725/1725, ASan/UBSan 1725/1725. Cross-target validation pending.
