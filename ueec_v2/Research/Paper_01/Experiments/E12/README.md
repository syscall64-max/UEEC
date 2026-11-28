# UEEC Paper 01 — E12: Performance and Resource Consumption

Linux x86-64; requires C compiler, Python 3 with CLOCK_MONOTONIC_RAW and Linux procfs. Run `sha256sum -c SHA256SUMS` before `./run.sh` (which replaces results); optionally `./run.sh 200` for 200 image/node repetitions per configuration. No root privileges needed. No network or cache flushing. Timing results vary by machine and run. Do not treat the included timings as acceptance thresholds.

## Method and provenance
- loading: E10 loader and E02.7 fixtures; 6 section counts × 3 widths × 3 modes = 54 configurations.
- relocation: E07 loader, 24 representable LE base fixtures (dense1, dense256, dense1024, page64) × 3 modes = 72 configurations. Only the E07 restricted ABS64 decoder; no claim about general relocation engine speed.
- Each image configuration has 10 warm-up cycles and 100 timed cycles by default. A child executes the entry and verifies the expected exit status once per configuration before measured samples. Timed cycles load and release but do not execute the entry. The original and instrumented C sources and instrumentation patches are included. Open/fstat/file mapping are outside the internal timed intervals; the file is mapped once per configuration. Diagnostic printf in image construction was removed for instrumentation. Internal `ready` = parse + select + build, excluding the timing harness, file open/mmap, execution and release. `build` includes initial layout preparation and checks; individual phases do not sum exactly to build due to timing overhead and untimed layout preparation. `copy` includes offset calculations, pread and associated cache-clear operations; `relocate` includes validation and patch application; `verify_header` includes address checks and cache-clear; `protect` includes mprotect. Release is timed separately. No performance thresholds.
- nodes: E03 original parser/selector; first and last name plus last numeric index for 2,4,8,16,32,64 nodes (18 configurations, 100 samples each). This does not measure whole loading.
- launch: unchanged E02.7 experimental loader and compact/section-table/split ELF fixtures. 72 configurations, 3 warm-ups + 20 measured subprocess calls each by default; checks every exit status. Includes Python subprocess overhead, OS exec/wait, loader startup, diagnostics directed to /dev/null, application execution and exit. Not a measurement of kernel loader internals alone. UEEC loader and ELF native execution are different mechanisms.
- symbols: E08/E09 Python reference encoder/decoder/binder functions, extracted without their top-level test loops. 6 counts × 3 export designs; 16-bit metadata, LE, fixed-length names or ordinals. 3 warm-ups + 20 samples each by default. Dictionary-based lookup and binding to synthetic addresses, not native function execution or C binary search. Registry construction and slot allocation excluded from bind timing. Every iteration validates all lookup results and slot values. Not a comparison to ELF/PE dynamic loader performance.
- memory: allocated bytes and region count are from loader state; mincore reports resident pages of those regions, not PSS or exclusive physical memory. RSS before/after is process-wide from smaps_rollup in a separate warm-up iteration, includes other memory and is not an exact image-only metric. RSS may be unavailable (-1). No ELF RSS comparison.
- All timings use CLOCK_MONOTONIC_RAW. Configuration order randomized with fixed seed for image and process launch groups; repetitions within configuration sequential. No CPU pinning, no frequency control, no cold-cache measurements, no independent host repetitions. Timing call overhead not subtracted. The platform, CPU, page size, affinity, compiler and repeat counts are recorded. For publication collect independent host runs and report variability.

## Outputs
- `samples.csv`: one row per measured phase; not independent correctness tests.
- `summary.csv`: min, median, arithmetic mean, nearest-rank P95, max in ns.
- `memory.csv`: one snapshot per image configuration, outside timed samples.
- `environment.txt`, `test.log`: environment and aggregate results.
- `SHA256SUMS`: archive integrity at delivery (results will change after rerun).

Default run: 234 validated configurations, 12,600 timed image cycles, 1,440 measured process-launch samples, 145,440 timing observations (multiple phases per cycle). The image, node and symbol tests have different validation units; these counts must not be added to prior native execution totals as if they were independent programs.

No official format changes, no GitHub publication. E12 uses experimental first-generation loaders, not unchanged v2. E04/E09 user-side reproduction remains pending as of preparation.
