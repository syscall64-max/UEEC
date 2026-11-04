# UEEC — 12-architecture experimental validation (2026-11-04)

This document updates the earlier 11-CPU baseline. The current experimental matrix contains **12 CPU architectures**, with **IBM Z / s390x (machine ID 11, Big-Endian)** added to the existing eleven:

| ID | Architecture | Endianness |
|---:|---|---|
| 0 | ARM32 | LE |
| 1 | AArch64 / ARM64 | LE |
| 2 | LoongArch64 | LE |
| 3 | PowerPC32 | BE |
| 4 | PowerPC64 | BE |
| 5 | RISC-V32 | LE |
| 6 | RISC-V64 | LE |
| 7 | SPARC32 | BE |
| 8 | SPARC64 | BE |
| 9 | x86 | LE |
| 10 | x86-64 | LE |
| 11 | IBM Z / S390X | BE |

## Functional results from Ubuntu/QEMU logs, 2026-11-03–04

The author's local Ubuntu cross-compilation and QEMU test output was reviewed in the associated research session. These are **author-run tests**, not claims about current GitHub Actions CI.

| Stage | Scope reported |
|---|---|
| 00_shell_code | 12-architecture shell-code execution |
| 01_ueec_info | 12-node container parsing and architecture checks |
| 02_ueec_load | 12-architecture loading and node selection |
| 03_reloc | 12 CPUs × monolithic/scattered = 24/24 executions |
| 04_convert | 12 CPUs × 2 modes = 24/24 principal executions; FLAT/BLOCK_4096/BLOCK_N conversion |
| 05_align | 12 CPUs × 2 layouts × 2 loading modes = 48/48 |
| 06_extract | 12 structure checks and 24/24 execution scenarios |
| 07_pe | 12/12 structural checks and 12/12 PE_STYLE execution scenarios |
| 08_export | 12 CPUs × COMPACT_GROUP/ORDINAL = 24/24 |
| 09_convert_export | 12 CPUs × COMPACT_GROUP/SIMPLE_FLAT = 24/24; byte-identical round-trip structural checks |
| 10_ueec_tls_c | 12 CPUs × compact/granulated = 24/24 TLS transfer and 24/24 loader execution scenarios |

The TLS prototype's 12-node source is 397 bytes; the s390x selected compact payload is 45 bytes; the client-side granulated fixture is 8192 bytes (4096-byte granules). The current transfer demo uses loopback 127.0.0.1. Separate-machine network tests remain outstanding.

These tests show successful **functional prototype execution**, not formal format correctness, security, performance superiority, or production-readiness. QEMU emulation is not native physical-hardware validation.

## Proposed next work

1. **11_metadata:** Define optional indexed METADATA section, preserving backward compatibility.
2. **12_kernel_launcher:** Investigate safe Linux user-mode helper invocation without arbitrary privileged execution.
3. **13_binfmt:** Integrate direct UEEC file invocation using Linux binfmt_misc.
4. **14_benchmark:** Measure node selection, placement, relocation, memory, total startup, and network transfer.

## Synchronization and security notes

- The project files and cross-architecture source directories were refreshed in Google Drive on 2026-11-04.
- Do **not** publish the Drive demo `key.pem` private key or generated build executables.
- Publishing or verifying a full source reconciliation from Drive to this GitHub branch must be reported separately from these test-log claims.
- Existing GitHub `main` remains the authoritative reviewed baseline until new changes are reviewed/merged.
