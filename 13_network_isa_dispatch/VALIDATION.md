# Network ISA dispatch validation — 2026-11-07

This report concerns stage 13. The user separately ran stage 12 successfully
on all 12 QEMU architectures plus native x86-64: 736 dispatch/format checks.
Those results are background, not a substitute for testing this new network code.

## Executed stage-13 checks

| Target/profile | Dispatch/format | TLS target checks | TLS global cases |
| --- | ---: | ---: | ---: |
| Native x86-64 | 56 | 23 | 26 |
| QEMU x86-64 | 59 | 23 | 25 |
| QEMU PowerPC64, big-endian ELFv1 / POWER8 | 54 | 21 | 25 |
| Native x86-64 TLS binary under ASan + UBSan | — | 23 | 26 |

All completed successfully. GCC 14.2, QEMU user 10.0.13 and native OpenSSL 3.5.7
were used. Native CODE coverage: scalar, SSE2, AVX2. PowerPC64 CODE coverage:
scalar and AltiVec. AVX-512F was not OS-usable/exposed and is not claimed executed.

Cross-target profiles are produced by the C request generator running under
the target QEMU CPU model. Actual TLS transport is performed by the native
OpenSSL program. Downloaded CODE is then executed by that target's loader
under the same QEMU profile. This does not claim a cross-architecture OpenSSL test.

Separately, `make test-transfer-all` passed **58** transfer/byte-layout cases:
12 architectures, all 29 CODE variants, compact and granulated mode. These
profiles are synthetic. Each received CODE is byte-compared to its original
payload and its metadata is independently decoded, but no target CODE is
executed in that particular suite.

## Behaviors verified

- Selection by client machine, Linux OS ID, CODE endian, ABI/application,
  OS-usable feature mask and metadata priority.
- Automatic selection, explicit available technology, unavailable technology
  rejection, fallback, absent technology and scalar capability restriction.
- Only one output node, one CODE and one MD; both MD records target node zero.
- Source node 256 is selected, reported as 256 and reindexed to zero, including MD.
- Wrong OS and wrong CODE endian receive no compatible-node response.
- Invalid/reserved client profiles are rejected; malformed raw requests are
  also rejected by the server after TLS negotiation.
- Existing output files remain unchanged.
- All twelve source-format combinations: LE/BE × MD16/32/64 × shared/legacy.
- Compact wire payload; client-local granulation, including three 4096-byte
  sections (12288 bytes) with no padding transferred over the network.
- Certificate issuer and DNS/IP identity verification.
- A trusted test TLS peer returns nine hostile responses: bad digest, priority
  mismatch, truncated body, invalid MD node index, unusable ISA mask, wrong OS,
  wrong CODE endian, oversized length and nonzero reserved header bytes.
  Every case fails without creating an output.
- Automatic CPU/OS detection by the native network client itself, independently
  of the optional request-file path.

The listening socket has no receive timeout; accepted/client sockets have
30-second I/O timeouts. Idle listening is not treated as an accept failure.

## Sanitizers and evidence

The native TLS server and client were compiled with AddressSanitizer and
UndefinedBehaviorSanitizer, with recovery disabled. No sanitizer errors were
reported. LeakSanitizer is unavailable under this environment's ptrace
supervision, so `ASAN_OPTIONS=detect_leaks=0` was used. No leak-check claim is made.

Evidence is in `validation/*.log`, `validation/dispatch-results/` and
`validation/network-results/`. The latter includes source-server logs, real
target profile records and explicitly labelled synthetic transfer records.
Certificates and private keys are generated for each test and are not shipped.

## Limits and reproduction

The other ten cross-compilers are not available in this environment. Stage-13
request generators/loaders and real target execution for those architectures
have not been tested here. No stage-13 all-12 execution claim is made.

Run `make test-all` in the complete existing 12-toolchain/QEMU environment.
The target fails on missing compilers/emulators; no targets are silently skipped.
It automatically runs the dispatch matrix, TLS matrix and native suite, starting
and stopping temporary local TLS servers itself. Cross OpenSSL is not required.

`make test` reproduces the host dispatch and real local TLS suites.
`make test-transfer-all` reproduces all 58 synthetic transfer cases.
`make cert` was also checked: its SAN contains localhost, 127.0.0.1 and ::1.

Scope remains Linux `sum` leaf CODE plus ISA metadata. Windows/macOS runtime
support, general reloc/import/data extraction, multiclient scheduling and a
stable public network protocol are not claimed.
