# CPU metadata validation — publication 2026-11-12

This report covers the exact standalone CPU metadata example proposed in
this PR. It separates fresh publication checks from earlier project-author
results. No kernel, network or MCU implementation is included.

## Fresh checks in this publication session

Environment: Linux x86-64, AMD EPYC 9V74, GCC 14.2.0, Python 3.12.14.

| Command, from `12_shared_metadata/` | Result |
| --- | --- |
| `make payloads-native` | PASS: scalar/SSE2/AVX2/AVX-512F fixtures rebuilt from assembly; entry/ISA checks passed |
| `make test` | PASS: 56 dispatch/format checks; executed scalar, SSE2 and AVX2 |
| `ASAN_OPTIONS=detect_leaks=0 make test-sanitize` | PASS: the same 56 checks with ASan + UBSan; executed scalar, SSE2 and AVX2 |
| `./build/ueec-loader --no-run build/sum.ueec` | PASS: selected AVX2 node 24, CODE 25, without executing it |
| `./build/ueec-loader --list-technologies build/sum.ueec` | PASS: 29 nodes, 12 architectures, every node references METADATA section 30 |

The default shared carrier is 5719 bytes. No ASan/UBSan diagnostic occurred.
Leak detection was explicitly disabled; there is no LeakSanitizer claim.
AVX-512F was rebuilt and its rejection path checked, but it was not OS-usable
on this host and is not claimed to have executed.

The sanitizer run repeats the same regression suite; it is not 56 additional
independent features. The suite exercises all six LE/BE × 16/32/64 metadata
encodings for both layouts, malformed counts/indexes/offsets/sentinels,
foreign-node metadata, unknown requirements, node 256, priority/fallback,
and actual selected leaf execution. Each executed CODE passes 256
alignment/length combinations, 32 guard-page cases, zero/NULL and overflow.

## Separately supplied project-author results

`validation/user-shared-metadata.log` is an earlier terminal transcript
supplied by the project author, not a fresh cross-architecture run here.
Host-specific paths have been redacted; its old `13_shared_metadata` name
is retained because that was the name at the time of execution.

That transcript reports 730 dispatch/format checks across 12 QEMU target
suites plus native x86-64. All target suites passed. RISC-V64 and S390X
negative CPU-model checks were unavailable because an independent libc probe
failed on the deliberately reduced CPU. Capability-mask rejection/fallback
checks still ran. PowerPC64/SPARC use the documented capability-mask negative
checks. LoongArch assembler warnings remain visible in the transcript.

The full 12-target matrix was NOT rerun for this PR: the publication session
used the native compiler. Reproduce cross-target execution with `make test-all`
when all toolchains/QEMU are available. Missing targets fail that command.

## Artifact and compatibility review

- Snapshot files were inspected and compared before import. The isolated
  stage-02 parser copy differs from current GitHub stage 02 only by local
  S390X target detection. Existing stages 00–09 are not overwritten.
- All 29 payloads are verified against their per-architecture SHA-256
  manifests by the generator. Native payloads were rebuilt in this session.
- Core `_GM`/node layouts and the global section mapping are unchanged.
  The new metadata reader supports design 0 and design 1; old loaders are
  not claimed to understand those metadata formats.
- Metadata is validated before selection, and is not copied into the
  selected leaf's executable mapping. RW loading is followed by RX.
- Record IDs remain experimental. This is a leaf-function demonstration,
  not a general application linker/import loader or a security audit.
- Source, tests and documentation were prepared with AI assistance; the
  results do not claim independent human review or physical-device testing.
