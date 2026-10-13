# UEEC — Project Status and Handoff

**Snapshot date:** 2026-10-13  
**Repository:** https://github.com/syscall64-max/UEEC  
**Baseline `main` commit inspected:** `5d238a15400dcf5eb8452105c403252fd05bcec7` (2026-09-24)  
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

The present repository contains a **reference toolchain and test harness**,
not a production-grade loader, Linux kernel loader, or virtual machine.

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

These are **claims recorded in repository documentation**, not tests
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
6. **Absent later layers:** a kernel loader, embedded loader ecosystem,
   and VM layer are roadmap items, not implemented features here.

## Prioritized next work

- [ ] Review and merge the documentation-only project-memory PR.
- [ ] Record a baseline native build and tests from a clean checkout,
      with exact commands, platform, toolchain versions, and results.
- [ ] Add narrowly scoped CI (begin with native compilation/basic tests),
      then extend to QEMU architecture coverage with explicit skip rules.
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
