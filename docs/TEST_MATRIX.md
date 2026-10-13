# UEEC — Verification matrix

Snapshot: 2026-10-13. This file documents the **test plan**, not a claim of successful CI execution.

The existing stage READMEs report prior multi-architecture results; the author reports having run UEEC successfully in their own environment. Exact commands, dates, toolchain versions, and logs for that run were not collected here.

| Architecture | Linux native/ QEMU target | GitHub native CI | Full cross-architecture status |
| --- | --- | --- | --- |
| x86-64 | Native | Configured: stages 01–09 | CI result pending |
| x86 | qemu-i386 | Not configured | Not run in this handoff |
| ARM32 | qemu-arm | Not configured | Not run in this handoff |
| ARM64 | qemu-aarch64 | Not configured | Not run in this handoff |
| LoongArch64 | qemu-loongarch64 | Not configured | Not run in this handoff |
| PowerPC32 | qemu-ppc | Not configured | Not run in this handoff |
| PowerPC64 | qemu-ppc64 | Not configured | Not run in this handoff |
| RISC-V32 | qemu-riscv32 | Not configured | Not run in this handoff |
| RISC-V64 | qemu-riscv64 | Not configured | Not run in this handoff |
| SPARC32 | qemu-sparc32plus | Not configured | Not run in this handoff |
| SPARC64 | qemu-sparc64 | Not configured | Not run in this handoff |

## Native CI scope

Workflow: `.github/workflows/native-ci.yml`. Runs on pull requests, pushes to main, and manual dispatch. Uses Ubuntu 24.04, gcc, make and NASM; no cross-QEMU infrastructure yet. Each of 01–09 has a distinct matrix job, with only the corresponding Makefile's x86-64 target. The workflow also checks byte-identical relocation and named-export round-trips.

**Caveat:** execution of generated, checked-in machine-code fixtures is part of existing stage tests. CI should be run only on trusted code; use review before enabling untrusted fork PR jobs and avoid elevated workflow credentials.

## Recording a result

For each target record: date, commit SHA, stage, architecture, runner OS, compiler version, emulator version, command, PASS/FAIL/SKIP, and log URL. Missing toolchains count as SKIP, not PASS. A configured job is *not* a passed job.

## Extension plan

1. Confirm native CI works without source changes and resolve environment-only dependencies in the workflow.
2. Introduce ARM64 and other available cross-toolchain/QEMU pairs as separately labelled jobs.
3. Account for RISC-V32 and SPARC32 toolchain availability explicitly; never silently omit failures.
4. Keep a clean separation between structural checks, no-run loading checks, and actual code execution.
