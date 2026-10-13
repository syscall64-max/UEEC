# UEEC — Verification matrix

Updated: 2026-10-13. Distinguish **configured**, **executed**, and **passed** tests.

## Current evidence

- Native x86-64: **9/9 stage jobs PASS** in GitHub Actions run
  [38430051185](https://github.com/syscall64-max/UEEC/actions/runs/38430051185),
  2026-10-13, PR #2, commit `5ebb8a08dc44d469c906a7c765782569c672e7c2`.
- Multi-architecture runs documented in stage READMEs were performed previously,
  including by the project author. Original logs, environment, and toolchain
  versions were not imported into this CI record.
- Cross-architecture workflow `.github/workflows/cross-architecture.yml`
  merged to main in PR #3. The nine packaged QEMU targets have **all passed** at commit
  `bc33f78f5257f9ffaada62202472fb6eefddd342` in
  [run 38432454430](https://github.com/syscall64-max/UEEC/actions/runs/38432454430).

| Architecture | Test runner | Toolchain provisioning | CI status |
| --- | --- | --- | --- |
| x86-64 | Native | gcc (Ubuntu 24.04) | PASS (9 stage jobs, run 38430051185) |
| x86 | qemu-i386 | Ubuntu gcc-i686-linux-gnu | PASS: run 38432454430 |
| ARM32 | qemu-arm | Ubuntu gcc-arm-linux-gnueabihf | PASS: run 38432454430 |
| ARM64 | qemu-aarch64 | Ubuntu gcc-aarch64-linux-gnu | PASS: run 38432454430 |
| LoongArch64 | qemu-loongarch64 | Ubuntu gcc-14-loongarch64-linux-gnu | PASS: run 38432454430 |
| PowerPC32 | qemu-ppc | Ubuntu gcc-powerpc-linux-gnu | PASS: run 38432454430 |
| PowerPC64 | qemu-ppc64 | Ubuntu gcc-powerpc64-linux-gnu | PASS: run 38432454430 |
| RISC-V32 | qemu-riscv32 | Source-built GCC/glibc (`scripts/build-riscv32-toolchain.sh`) | Source build: validating on dedicated PR, not yet PASS |
| RISC-V64 | qemu-riscv64 | Ubuntu gcc-riscv64-linux-gnu | PASS: run 38432454430 |
| SPARC32 | qemu-sparc32plus | Ubuntu SPARC64 GCC + 32-bit multilib | PASS: run 38432454430 |
| SPARC64 | qemu-sparc64 | Ubuntu gcc-sparc64-linux-gnu | PASS: run 38432454430 |

## Workflow behavior

- `native-ci.yml`: PR, push to main, manual; checks all nine implemented
  stages on native x86-64, plus explicit byte-identical conversion assertions.
- `cross-architecture.yml`: PRs that change the cross workflow, RV32 source
  build script, or this file trigger the **nine packaged** targets. The jobs
  test stages 01–09 against each architecture via QEMU.
- `workflow_dispatch` lets maintainers re-run packaged targets and optionally
  enable the expensive **RISC-V32 build from pinned upstream source**. Its
  default is off. The build installs at `/opt/riscv32`, matching existing
  Makefiles, and verifies a statically linked ELF32 RISC-V smoke program.
  Toolchain builds require significant disk/time; failure or resource
  exhaustion must be reported, not marked as a skipped success.
- Running machine-code test fixtures is inherent to these existing tests;
  do not use untrusted code or elevated secrets in cross-architecture runners.

## Result record (per job)

Date, commit SHA, stage, architecture, runner OS, compiler version, QEMU
version, exact command, PASS/FAIL/SKIP, log URL. Missing a toolchain is a
blocker or SKIP; a configured job is not a PASS.

## Next gates

1. Observe and record all packaged cross-job results from the PR.
2. Fix workflow or environment issues without changing reference source
   behavior to hide failures.
3. Merge only after review of CI evidence.
4. From the default branch, manually dispatch the source-built RISC-V32
   job; evaluate disk, build time, compile+link+QEMU execution results.
5. Consider caching the verified toolchain by source SHA and compiler
   version without trusting mutable or foreign binary downloads.

## RV32 source-build validation — trial branch

The dedicated branch `ci/validate-riscv32-source-20261013` temporarily
enables the RV32 source-build job for its pull request only. This allows
GitHub-hosted CI to run the real upstream build without manually dispatching
it or running the costly build for unrelated PRs. It downloads only the
pinned GCC, binutils, and glibc submodules and reclaims unused hosted-runner
disk space. Other PRs retain their existing packaged cross-architecture test
coverage. **Do not record RV32 as PASS until this job's Linux GCC build,
ELF32 smoke test, and nine UEEC stage tests all succeed.**
