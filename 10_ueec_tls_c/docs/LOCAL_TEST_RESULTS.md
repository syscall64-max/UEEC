# Local 11-architecture TLS UEEC validation — 2026-10-18

Source: console output supplied by the project owner from Ubuntu, running `make test-all` in `ueec_tls_c`; the captured output finishes with `PASS: all 11 architectures, compact + granulated`.

| CPU | machine | compact | 4 KiB-granulated |
|---|---:|:---:|:---:|
| ARM32 | 0 | PASS | PASS |
| ARM64 | 1 | PASS | PASS |
| LoongArch64 | 2 | PASS | PASS |
| PowerPC32 | 3 | PASS | PASS |
| PowerPC64 | 4 | PASS | PASS |
| RISC-V32 | 5 | PASS | PASS |
| RISC-V64 | 6 | PASS | PASS |
| SPARC32 | 7 | PASS | PASS |
| SPARC64 | 8 | PASS | PASS |
| x86 | 9 | PASS | PASS |
| x86-64 | 10 | PASS | PASS |

For every one of the 22 combinations, the log includes: loader build PASS; a TLS client response selecting the requested machine/system; loader invocation with `--node exit --no-run`; loader invocation with `--node exit` executing actual machine code; and a `PASS arch=... mode=...` line. Both invocations returned successfully to the test script. For all reported requests, the delivered TLS UEEC payload is 51 bytes; the compact local UEEC is 51 bytes and the granulated UEEC is 8192 bytes. The original fixture is 371 bytes (86.25% payload size reduction). These are payload/file sizes, not measured full network traffic.

Compiler warnings for LoongArch64 about right-shifting negative numbers appear in the log without a failed build or execution.

**Evidence scope:** the source is the project owner's console log, not a GitHub Actions run. Source-built RISC-V32 GCC/glibc provenance for the user's environment is not reverified here. No claim is made for arbitrary executable programs, payload-aware relocation/import/export rewriting, throughput, latency, or production security. The fixture is an 11-node `exit(0)` demonstration.
