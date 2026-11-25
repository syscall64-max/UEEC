# UEEC — Verification matrix

Snapshot: 2026-11-25. Configured, executed and passed are distinct.

Evidence at commit `1e3e87bf2a114643ef3b142165d43c29409a5d3b` (PR #11):

| Workflow | Coverage | Outcome |
| --- | --- | --- |
| [Native CI](https://github.com/syscall64-max/UEEC/actions/runs/45638223898) | x86-64, stages 01–09 | PASS |
| [Cross architecture](https://github.com/syscall64-max/UEEC/actions/runs/45638223822) | Ten packaged GCC/QEMU targets, stages 00–13 as configured | PASS |
| [Extended CI](https://github.com/syscall64-max/UEEC/actions/runs/45638223827) | Host 00, 10–17; 12-target 18–21 | PASS |
| RV32 source-build job | Source-built Linux GCC/glibc plus legacy stage tests | SKIP |

## Linux target matrix

| Target | Legacy packaged/native CI | LLVM native stages 18–21 |
| --- | --- | --- |
| x86-64 | PASS native | PASS native |
| x86 | PASS QEMU | PASS QEMU |
| ARM32 | PASS QEMU | PASS QEMU 10.0.3 |
| AArch64 | PASS QEMU | PASS QEMU |
| LoongArch64 | PASS QEMU | PASS QEMU |
| PowerPC32 | PASS QEMU | PASS QEMU |
| PowerPC64 | PASS QEMU | PASS QEMU |
| RISC-V32 | Source-build SKIP | PASS QEMU |
| RISC-V64 | PASS QEMU | PASS QEMU |
| SPARC32 | PASS QEMU | PASS QEMU |
| SPARC64 | PASS QEMU | PASS QEMU |
| s390x | PASS QEMU | PASS QEMU |

The table reports configured suites, not every combination of stage and target.
AVR/STM32 experiments are separate from the 12 Linux targets. Physical devices
are not certified by these jobs. Benchmark CI checks correctness, not speed
thresholds. Sanitizers are enabled where specified by the workflows.

## RV32 source-build

The cross workflow supports manual dispatch with build_riscv32=true and
run_packaged=false. It builds pinned upstream toolchain sources, verifies
ELF32 output and executes the configured legacy suite. This expensive job
is not a default PR gate. Do not report it as passed until the actual build
and tests succeed. Manual dispatch availability depends on the default-branch
workflow and maintainer permissions.

Historical author evidence: [12 architecture validation](VALIDATION_12_ARCH_20261104.md).
