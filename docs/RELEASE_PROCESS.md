# UEEC — Versioning and release policy (proposal)

Date: 2026-10-13. The wire specification is **draft v0.1**, not a stable 1.0 contract. This file is a release plan; **no Git tag or GitHub Release is created by adding this document**.

## Rules

- Use pre-1.0 SemVer-style tags such as `v0.1.0`, `v0.2.0`, but assign them only to a reviewed, verified commit.
- Each release notes the exact commit, the draft wire-spec revision, supported stage/toolchain matrix, and known limitations.
- For any on-disk incompatible change before 1.0, document what changed and whether readers/converters support migration; do not call it backward compatible by default.
- Distinguish reference toolchain maturity from wire-format stability.
- Attach checksums for any binaries or fixtures uploaded as release artifacts.
- Never publish a release simply because compilation succeeded locally.

## Release checklist

- [ ] Review open changes and corresponding spec/README updates.
- [ ] Native CI passed for proposed commit.
- [ ] Cross-architecture matrix and all SKIP/FAIL cases explicitly recorded.
- [ ] Security limitations and intended trust boundary reviewed.
- [ ] `PROJECT_STATUS.md` and changelog updated.
- [ ] Create annotated version tag on exact verified SHA, publish GitHub Release with documented scope, and verify assets/checksums.

The current work does not establish a stable release. First complete and evaluate CI.
