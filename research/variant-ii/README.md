# UEEC Variant II — research checkpoint (2026-11-27)

Variant II is an experimental format design, not a product release or a replacement for Variant I. This directory is reserved for its implementation, reproducibility material and validation evidence.

## Research scope

- Global section-header format, multi-node containers and shared physical sections; 16/32/64-bit layouts and both byte orders.
- Relocation and export design conversions, node extraction, external/internal/mixed imports, merging, granulation and degranulation.
- Scattered, hybrid, monolithic, fixed-address, prelinked and direct-mapping loading.
- Functional tests across 12 architecture targets (native x86-64 and QEMU cross-target execution).
- Native x86-64 startup, section-size, relocation, import, library-count, combined-workload, cold/warm page-cache, hardware/OS metrics and ELF-comparison experiments.

## Evidence inventory

The local publication package contains 28 historical source archives, 11 distinct measurement/trace archives (including failed perf attempts), cross-target logs, the latest browsable source tree and SHA-256 manifests. Eight distinct performance-result archives contain 21,008 timing rows; the baseline raw measurements are not available in the current package. Pilot measurements and repeated cumulative functional tests must not be pooled as independent evidence.

## Interpretation

Prelinked UEEC uses fixed virtual addresses and excludes preparation cost from startup timing. Direct mapping uses file-backed private mappings and demand paging. The ELF comparison measures whole-process time, not equivalent loader-only phases; the dynamic ELF uses external shared libraries and performs runtime linking, whereas direct UEEC uses prelinked internal nodes. Full access means touching each page, not processing every byte. Hardware counters are user-mode only. These are workload-specific observations, not universal superiority claims or production security validation.

## Publication status

This index is the initial branch checkpoint. The archive bundle and source tree are prepared separately; their presence on GitHub must be verified after bulk import. Variant I on main is unchanged.
