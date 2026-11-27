# UEEC Variant II — research snapshot (2026-11-27)

This directory preserves the successful experimental snapshots separately from the existing UEEC stages 00–21. It does not replace the reference wire-format specification or establish production readiness. All source files are copied from the original research ZIP archives without changing their contents. The top-level Makefile of each stage is from the final snapshot; individual substages retain their own Makefiles. The earlier 12.6 snapshot is preserved as `12.6_part_a`; the final snapshot is `12.6_full_integration`. Superseded failed versions are excluded. Stage 11.1's standalone archive is unavailable in the publication workspace; its previously reported result is included in the cumulative count, not as a standalone source directory.

## Results reported by the researcher

| Stage | Main matrix executions | x86-64 sanitizer executions |
|---|---:|---:|
| 11 — ISA Dispatch | 3,468 | 289 |
| 12 — Relocation | 82,728 | 6,894 |
| 13 — Import & Export | 54,228 | 4,519 |
| 14 — AVR Metadata | 143,688 | 11,974 |
| Total | 284,112 | 23,676 |

Total: **307,788 successful test-case executions** (including repeated regression and sanitizer executions, not distinct scenarios). The stage totals are cumulative figures from the research session, not newly executed GitHub CI tests. Stage 12 also had a separately reported stress matrix, not included here. The included extracted console logs cover 13.6, 14.3, 14.4 Fix 1, 14.5 and 14.6; not every historical run has a raw log in this snapshot.

Final-stage results: 13.6: 12,540 main matrix cases and 1,045 sanitizer cases; 14.6: 39,336 main matrix cases and 3,278 sanitizer cases. The final 14.6 matrix includes 36,096 regression cases, 1,800 new positive cases and 1,440 new rejection cases.

Targets: x86, x86-64, ARM32, AArch64, RISC-V32, RISC-V64, PowerPC32, PowerPC64, SPARC32, SPARC64, LoongArch64 and s390x. x86-64 was native; the other 11 were run through QEMU user-mode emulation. ASan/UBSan runs were x86-64 only. No physical AVR execution or GitHub CI run is claimed by this snapshot.

## Scope and reproduction

Run `make test` and `make sanitize` in each final stage directory (`11_isa_dispatch`, `12_relocation`, `13_import_export`, `14_avr_metadata`), with the cross compilers and QEMU commands configured in the corresponding Makefiles. Some original test Makefiles contain environment-specific toolchain defaults (e.g. `/opt/riscv32`); adapt them to the local installation as needed. See the stage-specific READMEs and `results/` for evidence. Archive SHA-256 digests are recorded in `results/SHA256SUMS-original-archives.txt`.

- 11: multi-ISA node dispatch, loading and native execution.
- 12: absolute and instruction relocations, multiple relocation designs, conversion and integration.
- 13: experimental IMPORT/EXPORT processing, symbol resolution, external calls/data, recursive library dependencies and shared-library state. Cycle rejection and selected memory-protection checks were exercised. Limitations include the experimental IMPORT wire format, bounded registry, no full PPC64 TOC ABI, no independent NX check, and incomplete multi-byte/mixed function-data cases.
- 14: experimental AVR metadata, five device profiles, memory geometry, node selection, layout validation and Flash/SRAM/Intel HEX image generation. The device table is not fully independently datasheet-verified. No EEPROM image, AVR relocations, automatic startup/reset vector or physical AVR execution is implemented or verified in this snapshot. Generated images are not necessarily bootable firmware.

Next planned Variant II stage: 15 (not started in this snapshot).
