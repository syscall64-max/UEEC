# UEEC — Project status and handoff

**Snapshot:** 2026-11-25 · PR #11 · experimental stages 00–21

The current repository includes the reference format/toolchain, architecture
metadata and selective delivery, AVR/STM32 simulation experiments, a minimal
VM, benchmarks, native compilation on client/server/load paths and UEEC-to-ELF.
See [README](README.md) for the stage map. Format stability and production
security are not established.

## Verified CI evidence

At `1e3e87bf2a114643ef3b142165d43c29409a5d3b`, all three workflows completed successfully:
[native](https://github.com/syscall64-max/UEEC/actions/runs/45638223898),
[cross](https://github.com/syscall64-max/UEEC/actions/runs/45638223822),
[extended](https://github.com/syscall64-max/UEEC/actions/runs/45638223827).
The separate RV32 GCC/glibc source-build job was skipped. See
[TEST_MATRIX](docs/TEST_MATRIX.md).

ARM32 stages 18–21 use QEMU 10.0.3 explicitly via --qemu-dir; the cacheflush
success check remains enabled. AVR cleanup releases the caller-owned simavr
core and remaining embedded IRQ resources; sanitizer checks remain enabled.
Physical hardware testing is not implied by emulation.

## Consolidation of PRs 5–10

Historical validation evidence and benchmark raw samples were restored from
original Git blobs in commit `1e3e87bf2a114643ef3b142165d43c29409a5d3b`. Older TLS and native stage paths
were mapped to 10_ueec_tls_c and 18_ueec_native. Separate VM/benchmark workflow
files were superseded by the integrated workflow.

Changed TLS source fixtures and tests expand 11 to 12 targets; the clean
target differs from the older version. CPU metadata source differences
reviewed were comments only. Native compilation refactors the common runtime
and adds profiles and code output; passing tests provide regression evidence,
not a complete semantic equivalence proof. Do not close old PRs until review
and consolidation are accepted.

## Outstanding gates

- Run the source-built RV32 GCC/glibc workflow and record actual outcome.
- Review remaining documentation differences and native semantic edge cases.
- Keep PR #11 in draft until review; main is not changed by this handoff.
- Audit bounds, overflow, mapping permissions, relocations and untrusted input.
- Distinguish emulator results from physical hardware validation.
- Preserve historical benchmark data; no universal speed/size claim.

## Historical handoff

The October 2026 baseline notes remain in Git history and the stage evidence.
