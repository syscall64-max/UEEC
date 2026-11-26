# UEEC — Project status and handoff

**Snapshot:** 2026-11-26 · `main` at `ef79ff6` (PR #12 merged) · tag
`v0.1.0-alpha.1` at `1fa3c6e` (PR #11) · experimental stages 00–21

The current repository includes the reference format/toolchain, architecture
metadata and selective delivery, AVR/STM32 simulation experiments, a minimal
VM, benchmarks, native compilation on client/server/load paths and UEEC-to-ELF.
See [README](README.md) for the stage map. Format stability and production
security are not established.

## Verified CI evidence

At `c5f070bdfd33b0bdb24cddd6683db69d2ab8b714`, all three workflows completed successfully:
[native](https://github.com/syscall64-max/UEEC/actions/runs/45641085862),
[cross](https://github.com/syscall64-max/UEEC/actions/runs/45641085879),
[extended](https://github.com/syscall64-max/UEEC/actions/runs/45641085864).
The RV32 GCC/glibc source-build job passed, including ELF32 verification and configured legacy tests. All 33 jobs passed. See
[TEST_MATRIX](docs/TEST_MATRIX.md).

ARM32 stages 18–21 use QEMU 10.0.3 explicitly via --qemu-dir; the cacheflush
success check remains enabled. AVR cleanup releases the caller-owned simavr core, embedded IRQ resources,
and nine-element iomem IRQ arrays. Sanitizer checks remain enabled; the
AVR job also passed a repeat run at commit 46c0833.
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

## 2026-11-26 — PR #11 and PR #12 merged

PR #11 was merged to `main` on 2026-11-25 (`1fa3c6e`, tagged
`v0.1.0-alpha.1`). PRs 5–10 were closed unmerged after consolidation into
PR #11; their branches still exist on the remote.

[PR #12](https://github.com/syscall64-max/UEEC/pull/12) was merged on
2026-11-26 (`ef79ff6`). It changes runtime/converter code:

- `04_convert/loader.c`, `06_extract/loader.c`: block relocation application
  specialized by record width and byte order, then inlined (V1 candidate).
- `04_convert/convert.c`: rejects FLAT offsets that cannot be represented,
  accounts for block bases, and fixes misplaced block-width selection.
- `research/article-01/`: experiment index plus raw archives (~9 MB of ZIPs).
  The README states experiments 01–10 ran on local snapshots, not on the
  merged commit, and timings are preliminary x86-64 virtual-environment
  measurements only.

No spec, fixture or on-disk structure change was made in PR #12. The
encoder now rejects some inputs it previously accepted (unrepresentable FLAT
offsets).

**CI evidence (checked via GitHub Actions on 2026-11-26):**

| Commit | Workflow | Result |
| --- | --- | --- |
| `757c66f` (PR head) | [native](https://github.com/syscall64-max/UEEC/actions/runs/45834095797) | 9/9 jobs passed |
| `757c66f` (PR head) | [cross](https://github.com/syscall64-max/UEEC/actions/runs/45834095852) | 10/10 packaged arch jobs passed; RV32 source build **skipped** (workflow only enables it for specific branches) |
| `757c66f` (PR head) | [extended](https://github.com/syscall64-max/UEEC/actions/runs/45834095873) | 13/13 jobs passed |
| `ef79ff6` (main) | [native](https://github.com/syscall64-max/UEEC/actions/runs/45837098984), [extended](https://github.com/syscall64-max/UEEC/actions/runs/45837099190) | passed |
| `ef79ff6` (main) | cross | not run (workflow triggers on PR/dispatch only) |

Relocation round-trip ran only in the `04_convert` native job; named-export
round-trip only in `09_convert_export`. Emulation only; no physical hardware.

**Archive integrity (verified locally on 2026-11-26):** the ten inner
`UEEC_article_experiment_NN.zip` files inside
`UEEC_article_01_all_experiments.zip` match the SHA-256 manifest in
`research/article-01/README.md`. `UEEC_experiment_11.zip` and
`UEEC_native_validation_2026-11-26.zip` match their `.sha256` files. The
bundle itself has no recorded checksum. That README still says raw archives
are "not included in this commit". This is now stale.

## Outstanding gates

- Review remaining documentation differences and native semantic edge cases.
- Add regression fixtures for the new FLAT-offset rejection and block-width
  selection in `04_convert` (malformed, boundary and cross-endian cases).
- RV32 was not exercised for PR #12; run the cross workflow via
  `workflow_dispatch` with `build_riscv32` if RV32 coverage is required.
- Decide whether stale remote branches from PRs 1–12 should be deleted
  (not done; requires owner decision).
- Update the `research/article-01/README.md` statement about archives not
  being included.
- Audit bounds, overflow, mapping permissions, relocations and untrusted input.
- Distinguish emulator results from physical hardware validation.
- Preserve historical benchmark data; no universal speed/size claim.

## Historical handoff

The October 2026 baseline notes remain in Git history and the stage evidence.
