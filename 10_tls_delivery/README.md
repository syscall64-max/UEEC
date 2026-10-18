# UEEC TLS selective delivery — C + assembly (research prototype)

Standalone Linux localhost TCP/TLS client-server prototype for **UEEC v1**.
Server and client run natively on the host (e.g. Ubuntu x86-64); the **downloaded machine-code payload** is tested by the relevant cross-built UEEC loader, under QEMU for the other 10 CPUs.

**Status (locally verified 2026-10-18):**

- C/OpenSSL client/server build PASS (OpenSSL 3).
- Assembly-specified 11-node UEEC fixture PASS, **371 bytes** (GNU as fallback in local environment; original NASM fixture included).
- C unit tests PASS on 11 selectors, 2 layouts, shared sections, and invalid/truncated files.
- Local TLS selective transfer/reconstruction PASS on **11 architectures x 2 modes = 22 requests**.
- End-to-end **native x86-64 loader** `--no-run` and actual `exit(0)` PASS in compact and granulated modes. Compact payload=51 bytes; 4 KiB-granulated file=8192 bytes.
- **User-provided Ubuntu run (2026-10-18): `make test-all` reported PASS on all 11 architectures, compact and granulated (22/22), including cross-loader/QEMU `--no-run` and execution.** This was observed in the user-provided console log, not independently reproduced in this build environment or GitHub Actions. See `docs/LOCAL_TEST_RESULTS.md`.
- The other 10 CPU cross-loader/QEMU execution tests were **not run in the package authoring environment**, which lacked those toolchains; the user-provided Ubuntu run establishes the separate 11/11 result.

**Scope restrictions:** only v1, little-endian wire metadata, SH16 section descriptors. Output section payloads are copied byte-for-byte; CODE, INIT_DATA, UNINIT_DATA, CONSTANT_DATA only. Nontrivial RELOCATION, IMPORT, EXPORT or unknown sections are **intentionally rejected** when selected: embedded global section index rewriting is not implemented. This is a local demonstrator, not a production network service or safe execution sandbox. Do not process or execute untrusted input.

## Requirements

On Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential libssl-dev openssl make nasm binutils
```

For the cross-architecture execution suite, install the relevant Linux cross-compilers, libc development packages, and `qemu-user`. This project uses the same target names and default toolchain locations as UEEC `02_ueec_load/Makefile`; `make check-env` reports what is missing. RISC-V32 additionally expects a **source-built** Linux GCC/glibc at `/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc` (see upstream UEEC `scripts/build-riscv32-toolchain.sh`). Neither `make test-all` nor `make test-available` automatically installs system packages.

The assembly fixture is available as the original NASM `reference/02_ueec_load/ueec.asm` and a byte-identical intended GNU assembler source `ueec_fixture.S`. When `nasm` is absent, Makefile uses `as` + `objcopy` to create the 371-byte fixture (little-endian x86 assembler host). Reference C loader source and Makefile are **copied unchanged** from the user's provided UEEC reference snapshot; original MIT LICENSE is included in `reference/LICENSE`. Source copies in this archive are kept separate from the main UEEC repository.

## Two terminals

```bash
# Extract archive, then cd to ueec_tls_c
make all
make cert
```

`make cert` generates a 7-day self-signed localhost-only development key pair; never commit `key.pem` or use it for public-facing servers.

**Terminal 1 — server (remains running):**

```bash
make serve
```

Starts TLS server at `127.0.0.1:9443`, using `build/multi.ueec`. Stop with Ctrl+C.

**Terminal 2 — client + tests:**

```bash
make test-unit            # C unit tests, no network connection
make test-transfer-all    # 11 architecture requests x compact/granulated (22 TLS transfers)
make test-x86_64          # builds x86-64 C loader; checks and executes both downloaded versions
make test-arm64           # builds ARM64 C loader; QEMU executes both versions
make check-env            # show cross compiler and QEMU availability
make test-all             # strict: build and execute both modes on all 11 CPUs
```

`make test-all` exits **FAIL** on missing toolchains/QEMU or bad execution; `make test-available` labels missing targets **SKIP** and must **not** be reported as 11/11 success.

Test output files are under `build/results/` and `build/transfers/`. Existing per-architecture test outputs there are removed by the test script before re-creating them, but files in other directories are not overwritten. `make clean` removes both generated `build/` and `reference/02_ueec_load/build/` directories.

### Manual client examples

```bash
# Terminal 2 while server is running
./build/ueec-net client --ca cert.pem --machine 1 --system 2 --mode compact \
  --output build/arm64_compact.ueec
./build/ueec-net client --ca cert.pem --machine 1 --system 2 --mode granulated \
  --granule 4096 --output build/arm64_granulated.ueec
```

Machine values from reference `ueec.h`:

| id | CPU | loader runtime |
|---:|---|---|
| 0 | ARM32 | qemu-arm |
| 1 | ARM64 | qemu-aarch64 |
| 2 | LoongArch64 | qemu-loongarch64 |
| 3 | PowerPC32 | qemu-ppc |
| 4 | PowerPC64 | qemu-ppc64 |
| 5 | RISC-V32 | qemu-riscv32 |
| 6 | RISC-V64 | qemu-riscv64 |
| 7 | SPARC32 | qemu-sparc32plus |
| 8 | SPARC64 | qemu-sparc64 |
| 9 | x86 | qemu-i386 |
| 10 | x86-64 | native loader |

## What is tested

1. Server parses the compact multi-node UEEC, selects every matching Linux/CPU node, and keeps its referenced sections, including shared sections; old section numbers are remapped in the node link table.
2. On localhost TLS 1.2+ with explicit certificate and hostname validation, the server sends a new *compact UEEC* payload (not a tar or custom section stream).
3. Client checks the declared length and SHA-256 of the TLS payload, parses the result, confirms the requested architecture, and optionally repacks section spans to `GRANULE` (power of two, <=32768 for SH16) without transmitting padding over TLS.
4. It writes a complete UEEC file using exclusive creation (`O_EXCL`).
5. The Makefile builds the matching **C loader** and runs `--no-run` plus real execution, natively or via QEMU. A nonzero status fails that target.

Payload economization for the **tiny exit(0) fixture**: 371B original -> 51B selected = **86.25% smaller** UEEC payload. This figure excludes TLS/TCP/IP protocol overhead and is **not** a performance improvement claim for larger software. A granulated local file is 8192B for this fixture. It remains to be shown whether this physical layout changes real file-backed mapping behavior in the loader.

## Security, ABI and scientific limits

- Listener is hardcoded to **127.0.0.1**. Only the server is authenticated; there is no client auth or role-based access control. TLS is not a substitute for signed, trusted code packages.
- No shell invocation of client-supplied strings. Bounded input/output at 64 MiB and bounded node/header descriptors. Invalid inputs should fail closed, but this is **not** a complete security audit or fuzzed parser.
- Safe section extraction depends on the referenced section types. Real relocation/import/export entries may embed global section indices; their rewriting requires format-aware converters and exhaustive validation before execution.
- All 11 CPU fixture variants perform an `exit(0)` syscall; they demonstrate end-to-end selection, container reconstruction, and loader execution only, **not** full arbitrary-program linking or dependency resolution.
- Cross-architecture CI results in the UEEC main repository refer to the **original loader** and **original input**. They do not automatically prove these **new network-transformed files** execute on other 10 CPUs. Use `make test-all` and retain its logs for that claim.
- No claim that compact or granulated is faster; scientific evaluation should include larger representative code, cold/warm caches, actual wire bytes including protocol overhead, CPU/QEMU variations, and correctness of mapping and dynamic relocation.
