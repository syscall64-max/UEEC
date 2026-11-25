# Shared metadata validation — 2026-11-07

This report applies to `12_shared_metadata`, not to the earlier stage-11 logs.

## Executed suites

| Target | Backend | Dispatch/format checks | CODE executed |
| --- | --- | ---: | --- |
| x86-64 | native, GCC 14.2 | 56 | scalar, SSE2, AVX2 |
| x86-64 | QEMU 10.0.13 | 59 | scalar, SSE2, AVX2 |
| PowerPC64, big-endian ELFv1 | QEMU 10.0.13, POWER8, GCC 14.2 | 54 | scalar, AltiVec |
| x86-64 | native, AddressSanitizer + UndefinedBehaviorSanitizer | 56 | scalar, SSE2, AVX2 |

All four suites completed successfully. Expected rejection cases require a
normal error exit; a signal termination always fails the suite. Sanitizer
diagnostics also fail the suite.

Evidence: `validation/native.log`, `validation/x86_64-qemu.log`,
`validation/powerpc64-qemu.log`, `validation/sanitizers.log`, and the matching
JSON result records in `validation/results/`.

LeakSanitizer cannot operate under this environment's ptrace supervision.
The sanitizer suite used `ASAN_OPTIONS=detect_leaks=0`; address bounds and
undefined-behavior checks remained enabled. No leak-check claim is made.

## Cases covered

- One shared trailing MD section; all nodes map to that same physical section.
- Six shared format combinations: LE/BE × 16/32/64-bit count/offset.
- Six legacy per-node combinations with the same endian/width coverage.
- Automatic selection, explicit technology, fallback, incompatible and absent
  technologies, and listing with the shared MD section index.
- Actual node 256 selected by priority, proving that node indexes preserve
  their upper byte.
- Invalid and 65535 node indexes; duplicate priority; unknown family/type;
  malformed design, sentinel, offsets and count; huge count at all widths in
  both byte orders; file truncation.
- Corrupt foreign-architecture metadata rejected before selection/execution.
- Higher-priority scalar override, unknown required feature bits and no
  compatible node.
- Each compatible CODE: 256 alignment/length cases, 32 guard-page cases,
  NULL/zero, tails and uint32 overflow.
- Whole-file inspection mapping released before CODE execution; metadata is
  not copied into the resident executable image.

## Artifact facts

- 12 architectures, 29 nodes, 31 physical sections including HEADER.
- 29 CODE sections plus one METADATA section at global index 30, at EOF.
- 58 MD records plus END; every record has a fixed 16-bit node index.
- Default shared file: 5719 bytes; equivalent legacy file: 6049 bytes.
- Original CODE payloads are unchanged and their SHA-256 manifests are checked
  by the generator. `examples/sum.ueec` is the new shared-layout file.
- `validation/layout.json` independently decodes the default file's descriptors,
  mappings, node IDs and record offsets.

## Coverage limits

This environment did not provide the other ten cross-compilers. Their updated
loaders were not rebuilt/executed here. The native listing validates metadata
for all 29 nodes, but that is not execution of every architecture's CODE.
The pre-existing 12-architecture source profiles and payload binaries are
retained. Run `make test-all` in the complete toolchain environment to validate
all updated loaders. Missing targets fail that command; they are not skipped.

AVX-512F was not OS-usable on this host or exposed by QEMU max, so this release
does not claim an AVX-512 execution test. It is listed and correctly rejected.
POWER5+ is outside the mandatory PowerPC64 profile; POWER8 was tested.

TLS delivery and metadata filtering/reindexing during network extraction are
the next integration, not part of this release.
