# Performance results

Implementation source lives in the numbered stage directories. The loading
benchmark source and its correctness tests live in [`17_ueec_bench/`](../17_ueec_bench/).
This directory holds observations from actual benchmark runs.

| Directory | Measurement |
| --- | --- |
| [`loader/2026-11-16-pilot/`](loader/2026-11-16-pilot/) | Original warm-cache pilot: 72 configurations, 43,200 samples; KVM plus container environment |
| `local/loader-<UTC>-<unique>/` | Automatically created local runs, ignored by Git |

Each `make bench` or `make quick` creates a new local run directory. Existing
reports are never overwritten, including when `--output` is supplied. The runner
prints the final path. `make clean` removes the compiled benchmark under the
stage's `build/`; it does not remove the separate result directories.
It also preserves any legacy reports under `17_ueec_bench/build/results/` from
the earlier runner. Such reports can be moved into their own named run folder.

A run includes environment metadata (CPU, memory, OS and virtualization),
compiler/build details, source/binary/fixture hashes, raw measurements and
summaries. Keep these together. Curated runs can be copied into a named directory
under `results/<benchmark>/` after review. Preserve original measurements and
identify the measured source revision; a directory move is not a new measurement.

The archived pilot predates the current output-directory policy. Its original
data files are unchanged; `manifest.json` records their hashes and measured
source commit. No timings are recalculated merely because code changes later.

Correctness tests remain beside the code they exercise. They are distinct from
the measured performance data here. Earlier stages retain their existing
validation records; this change separates the new loading-benchmark results.
