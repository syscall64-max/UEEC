# Local exploratory pilot (2026-11-16)

Command: `make -C 17_ueec_bench bench`, using the normal GCC 14.2.0 `-O2`
binary. Completed 72 configurations and 43,200 measured samples. No CPU
affinity pinning or frequency locking was requested. This is one environment,
with within-run randomized rounds, not independent host replications.

Guest-visible environment: Intel Xeon Platinum 8573C, 9 logical CPUs,
9.73 GiB OS-visible RAM, no swap, Debian 13.6 / Linux 6.18.57 x86-64,
Python 3.12.14. Detectors reported `kvm` and `container-other`; physical DIMM
parameters, host resource constraints and hypervisor version were unavailable.
See `pilot-environment.json` for exact values and detector availability.

- `pilot-summary.json`: complete metadata, SHA-256 source/binary/fixture hashes,
  statistics and round medians. `git_head` is null because this pilot ran in a
  prepared source directory before GitHub publication; source hashes identify
  the actual measured files. Stage-16 source was verified against VM branch
  commit `74a282982129ec2cc199d5db6756f5fbba970a77` before reuse.
- `pilot-summary.csv`: compact median-time table.
- `pilot-samples.csv.gz`: **all** raw samples, gzip-compressed; the decompressed
  file is the runner's unmodified `samples.csv`. Recompute statistics from it
  if checking results. Keep mode/case/round distinctions.

Correctness validation on the same prepared source: 37 normal harness/report
checks and 36 ASan/UBSan harness checks passed. Leak detection was disabled
locally because this supervised environment can block LeakSanitizer. Foreign
architectures, physical devices and the author's own host were not benchmarked.

Timings are noisy warm-cache loader measurements. The workload never executes
VM instructions. Runtime virtual allocation counters are not RSS. No universal
ranking of modes or comparison against other executable formats is inferred.
