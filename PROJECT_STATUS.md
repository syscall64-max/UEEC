# UEEC — Project Status and Handoff

**Snapshot date:** 2026-11-16  
**Repository:** https://github.com/syscall64-max/UEEC  
**Baseline `main` commit inspected:** `623fd685adad674d42ccf9e41b6903197c838a05` (inspected 2026-11-16; earlier baselines remain in the dated history below)  
**Format specification:** `FORMAT_SPECIFICATION.md`, draft v0.1 (pre-1.0)

> This document is the project's **manually maintained working memory**. It records
> what was checked, what is only documented, outstanding work, and decisions.
> It is not an automatic state tracker. Check the current branch and commit
> before relying on the snapshot; code, tests, and the format specification
> take precedence over stale notes.

## Project purpose

UEEC (Universal Extensible Executable Container) is a proposed
architecture-neutral executable container. Its key design points are:
a self-declared file endianness, multiple CPU-specific nodes in one
container, file-level section sharing, and versionable `design` encodings
for relocations and exports. See `README.md`, `FORMAT_SPECIFICATION.md`,
and `docs/UEEC_DESIGN.md` for details and the limits of the claims.

The repository contains a **reference toolchain and test harness** plus an
experimental UEEC VM in stage 16. It is not a production-grade runtime or
Linux kernel loader.

## Current implementation map

| Path | Current role / documented result |
| --- | --- |
| `00_shell_code/` | Initial architecture-specific shell-code prototype |
| `01_ueec_info/` | Container metadata reader; documentation reports an 11-architecture validation matrix |
| `02_ueec_load/` | Linux user-space loader; shared 11-node container, host-node selection, `monolithic`, `scattered`, and `hybrid` strategies; design notes report execution on 9/11 architectures in the referenced environment |
| `03_reloc/` | `SECTION_FLAT` relocation and a shared physical DATA section across 11 nodes; the latest root documentation describes the shared-section result as **structural validation**, not a fresh complete execution run of this stage |
| `04_convert/` | RELOCATION conversion among `FLAT`, `BLOCK_4096`, and `BLOCK_N`; documented byte-identical round-trip checks |
| `05_align/` | Section alignment tooling |
| `06_extract/` | Extract a node from a multi-node container |
| `07_pe/` | PE-style relocation design; intentionally constrained to one-node, monolithic images |
| `08_export/` | `COMPACT_GROUP`, `SIMPLE_FLAT`, and `ORDINAL` export representations and example loaders |
| `09_convert_export/` | Lossless `COMPACT_GROUP` ↔ `SIMPLE_FLAT` export conversion; documented cross-architecture and function-call tests |
| `16_ueec_vm/` | Experimental VM, two selectable programs and shared INIT_DATA; 162 locally reproduced checks, plus a separate author-reported 165-check NASM run including leak detection |

The earlier-stage entries are **claims recorded in repository documentation**, not tests
re-executed during creation of this status file. For exact coverage and
exceptions, consult the relevant stage `README.md` and Makefile.

## What was verified for this handoff (2026-10-13)

- The GitHub repository is accessible, public, on default branch `main`.
- The baseline `main` head was `5d238a15400dcf5eb8452105c403252fd05bcec7`.
- At that baseline, no `AGENTS.md`, `PROJECT_STATUS.md`, or
  `.github/workflows/` entries existed in the repository tree.
- There were no open pull requests when inspected.
- The supplied local `UEEC.zip` and GitHub `main` should **not** be
  assumed identical: some paths and some file contents differ. No archive
  synchronization has been performed.
- **No build, QEMU matrix, runtime, or security test was executed** in
  preparing this documentation-only handoff.

## Open issues and limitations

1. **Repeatable verification:** Native GitHub Actions CI passed 9/9 stage jobs
   on 2026-10-13 (run 38430051185). A separate cross-architecture matrix
   is configured on a review branch; results are pending. Preserve exact
   toolchain/QEMU versions and explicit pass/skip/fail records.
2. **Execution safety:** the test loader paths run generated machine code.
   Keep tests isolated and run only trusted fixture binaries. The
   `03_reloc/README.md` explicitly documents a writable-and-executable
   (`W+X`) test-harness simplification; do not treat it as production-safe.
3. **Format stability:** the wire specification is draft v0.1; changes
   before 1.0 may break compatibility. Align readers, writers, converters,
   and test fixtures with any format change.
4. **Scope of converters:** PE-style relocation and ordinal-only exports
   are not automatically losslessly interchangeable with the other
   encodings; preserve their documented distinct semantic models.
5. **Environment availability:** some cross-toolchains/QEMU targets are
   environment-dependent. Report missing tools as skips/blockers, not passes.
6. **Later layers:** stage 16 now implements a limited bytecode VM. A kernel
   loader, a complete embedded ecosystem, joint library linking, a source
   language/compiler and VM SIMD/128-bit operations remain future work.

## Prioritized next work

- [x] Review and merge the documentation-only project-memory PR (#1).
- [x] Record a baseline native x86-64 CI run with GitHub logs (run 38430051185).
      Cross-architecture CI results still pending.
- [x] Add narrowly scoped native CI (9/9 stage jobs PASS).
- [ ] Verify proposed cross-architecture QEMU matrix and the RV32 source-build job.
- [ ] Audit `UEEC.zip` against the GitHub tree and classify each difference
      before bringing over any file. Never copy the archive over `main`.
- [ ] Review file parsing bounds/overflow checks, mapping permissions,
      relocation safety, and error handling before considering untrusted
      binaries or deployment.
- [ ] Expand test fixtures for corrupt/truncated containers, malformed
      headers, cross-endian representations, and section sharing.

## Decisions and change log

| Date | Decision or observation | Evidence / state |
| --- | --- | --- |
| 2026-09-24 | Baseline documentation update covering file-level section-sharing validation | `main` commit `5d238a1` |
| 2026-10-13 | Introduce repository-owned handoff notes and coding/verification guidelines via a reviewable branch, leaving `main` unchanged pending review | Documentation-only change |
| 2026-10-13 | Do not treat uploaded `UEEC.zip` as authoritative without a file-by-file comparison | Comparison still pending |

## Session handoff template

Update this section **after every meaningful work session**, ideally in
the same PR as the code changes, so another session can resume safely:

- **Date / branch / starting and ending commit:**
- **Goal and files changed:**
- **Design decisions and reasons:**
- **Tests actually executed:** command, environment, PASS/FAIL/SKIP, logs
- **Known regressions or unresolved questions:**
- **Next smallest safe action:**

If no tests were run, say **"Not run"** explicitly. Never convert an old
documented result into a claim about the current checkout.

## Reading order for a new collaborator or AI agent

1. `PROJECT_STATUS.md` (this file) — handoff and blockers.
2. `AGENTS.md` — project workflow and safe-edit requirements.
3. `README.md` and `FORMAT_SPECIFICATION.md` — public introduction
   and draft wire format.
4. `docs/UEEC_DESIGN.md` — rationale and validation limits.
5. Relevant stage `README.md`, `Makefile`, source, and latest commits.


## Follow-up work log — 2026-10-13

- PR #1 merged as `273b305d66ee41e12181dc8aeaed6c32342cdf06`.
- PR #2 merged as `ae365b3ead19d4c29ceceecb3e9d13320e509c41`.
- x86-64 native GitHub Actions run 38430051185 passed all nine matrix jobs
  (01–09) on 2026-10-13. This **does not establish** 11-architecture CI.
- Cross-architecture CI and pinned upstream RV32 Linux toolchain builder
  proposed on branch `ci/cross-architecture-toolchains-20261013`.
  Results **pending**. See `docs/TEST_MATRIX.md` for target coverage.
- Archive reconciliation remains incomplete; no source files were imported
  from the archive during this work.
- Security review is a preliminary checklist, not a full security audit.
- No version tag or GitHub Release has been published by these PRs.

## Follow-up work log — 2026-11-16

- Goal: publish the experimental UEEC VM as standalone stage `16_ueec_vm/`
  through branch `feat/ueec-vm-min16-20261116` and a review PR, following
  `AGENTS.md`. Starting `main`: `623fd685adad674d42ccf9e41b6903197c838a05`.
- Compared the imported canonical header, original stage-02 loader and
  stage-01 reader with the current GitHub blob SHAs; all three match. Only
  the new stage and scoped documentation/CI updates are proposed. Stages
  00–09 are preserved. Separate stages 10–15 are not imported.
- Container wire layout unchanged. New runtime selects VM type 2 / machine
  0. Operand FORMAT8 is removed from the active VM; formats 16/32/64 retain
  codes 1/2/3. Original single-image UEF sources are retained as references.
- Two program nodes (`hello`, `stack`) share one physical INIT_DATA section.
  The compact fixture is 262 bytes. Monolithic, scattered and hybrid loading,
  all three descriptor widths and both metadata byte orders are covered.
- Local Linux x86-64: `make run`, `make test`, `make sanitize` PASS; 162
  checks without NASM. Local sanitizer leak detection is disabled because
  LeakSanitizer is blocked by the environment's ptrace supervisor.
- Project-author x86-64 transcript: 165 checks PASS normally and with
  ASan/UBSan, including an additional run with leak detection enabled.
  NASM provides the extra three fixture-encoding comparisons. These are
  author-host results, not local reproductions.
- A VM-specific GitHub Actions workflow is added; remote CI results are
  pending until its run is observed. No fresh foreign-CPU VM execution or
  physical MCU measurement is claimed.
- Remaining work: ABI versioning, arithmetic and branches, joint linking,
  HEAP, source-language tools, FORMAT128 and SIMD specification/implementation.

### Remote CI observed on 2026-11-16

For VM source commit `46bde6f9193f75923e3181452cdbd14a93f0fbcb` (PR #8), both workflows completed successfully:

- [UEEC VM run 43821943172](https://github.com/syscall64-max/UEEC/actions/runs/43821943172): Ubuntu 24.04 x86-64, GCC 13.3.0, Python 3.12.3, NASM 2.16.01. Both program examples ran; 165 checks passed normally and 165 with ASan/UBSan and leak detection enabled.
- [UEEC native CI run 43821943064](https://github.com/syscall64-max/UEEC/actions/runs/43821943064): completed successfully for the existing native stage workflow. This is not a foreign-architecture VM matrix.

These GitHub-hosted results are distinct from both the 162-check local run and the author-provided 165-check terminal transcript. This follow-up changes this status note only; VM source is unchanged.

## Selected-image loading benchmark — 2026-11-16

- New standalone `17_ueec_bench/`, based on VM branch commit
  `74a282982129ec2cc199d5db6756f5fbba970a77`. Proposed on separate branch
  `perf/ueec-loader-bench-20261116`, stacked on the VM PR branch.
- Measures selected-image loading/release only: open/map, parse/select,
  runtime-image construction, input close and image release. No guest VM
  instruction executes and no production loader or wire format changes.
- Matrix: monolithic/scattered/hybrid; 1/8 nodes; shared/duplicated IDATA;
  1 KiB/64 KiB/1 MiB payloads; file granules 1/4096. SH32/LE fixed.
  Defaults: 20 warmups, 200 samples, 3 randomized rounds per case.
- Every report captures CPU model/features/topology/affinity, available
  frequency/cache evidence, RAM/swap/page size, OS/kernel, separate VM and
  container detection, available cgroup-v2 limits, compiler/build command,
  source/binary hashes and before/after memory/load observations. Unavailable
  DIMM parameters, hypervisor versions and hidden host limits remain unknown.
- Verified locally: `make -C 17_ueec_bench test` PASS (37 checks, including
  report/raw-CSV integration); `make -C 17_ueec_bench sanitize` PASS (36
  checks, ASan/UBSan with leak detection disabled in this supervised host).
  Sanitizer runs exclude the timing-report integration and are not timings.
- Local optimized pilot: `make -C 17_ueec_bench bench` completed 72 cases /
  43,200 measured samples. Guest-visible Xeon Platinum 8573C, 9 logical CPUs,
  approximately 9.73 GiB OS-visible RAM, no swap; Debian 13.6 x86-64,
  Linux 6.18.57, GCC 14.2.0, Python 3.12.14. Detection reports KVM plus
  `container-other`; hidden host resources/hypervisor version are unknown.
  Full evidence and compressed raw samples are in `17_ueec_bench/evidence/`.
- This is an exploratory warm-cache pilot, not VM throughput, physical-memory
  measurement, cross-format comparison, or evidence of a general speedup.
  Independent host repetitions and additional workloads remain to be run.
- A benchmark-correctness workflow is proposed. Its remote result is pending
  until observed; it deliberately has no performance PASS threshold.

### Benchmark correctness CI observed on 2026-11-16

For benchmark source commit `c76b748fca9a368399eb828449584a79fcb62488`,
[run 43848952938](https://github.com/syscall64-max/UEEC/actions/runs/43848952938)
completed successfully on Ubuntu 24.04.5 x86-64: 37 normal checks and 36
ASan/UBSan checks, with `detect_leaks=1:halt_on_error=1`. This verifies harness
correctness on another environment; it is not a remote performance result.
The run checked the PR's generated merge revision `a6359c10930cc0e2d76f59fb8509e731a0062060`.
This follow-up changes documentation only; measured benchmark source is unchanged.
