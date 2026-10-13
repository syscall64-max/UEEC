# UEEC.zip vs GitHub — initial inventory

Date: 2026-10-13. Inputs: supplied `UEEC.zip` and repository `main` as observed at initial baseline commit `5d238a15400dcf5eb8452105c403252fd05bcec7`.

**This is not yet an exhaustive byte-by-byte reconciliation.** Do not merge local archive content into GitHub without a hash/content comparison for each changed file.

## Confirmed scope

- `UEEC.zip` contains 121 regular files (approximately 496,398 bytes uncompressed) and root `README.md`, `LICENSE`.
- Baseline GitHub tree contained additional top-level specification/design material, including `FORMAT_SPECIFICATION.md` and `docs/UEEC_DESIGN.md`, as well as later repository documentation.
- Previously checked matching examples: `02_ueec_load/ueec_load.c` and `docs/UEEC_DESIGN.md` (where an archive copy was available in a different location).
- Previously checked differences: `03_reloc/loader.c` and `03_reloc/README.md`.
- Files with differing paths are not necessarily differing contents.

## Safe reconciliation procedure

1. Pin both input versions (archive SHA-256, Git commit SHA) and collect complete file inventories.
2. Compare normalized relative paths, Git blobs, and archive SHA-256 content hashes.
3. Classify each item as identical, moved, GitHub-only, archive-only, or content-different.
4. For every content difference, inspect both versions and identify the intended canonical change.
5. Bring approved updates into a new branch, run relevant tests, then review a PR. Never overwrite `main` wholesale.

The archive is a historical snapshot, not a replacement for the checked-in source.
