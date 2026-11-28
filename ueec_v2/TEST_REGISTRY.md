# Test registry — historical baseline
Baseline commit 64d352d35d774113e69ee61b9803f6ffac172601; reported 2026-11-27. Counts are successful test-case executions including repetitions, not unique scenarios. No fresh CI or execution in the organized tree.

| ID | Stage | Main | x86-64 ASan/UBSan | Total |
|---|---|---:|---:|---:|
| HIST-11 | 11 | 3468 | 289 | 3757 |
| HIST-12 | 12 | 82728 | 6894 | 89622 |
| HIST-13 | 13 | 54228 | 4519 | 58747 |
| HIST-14 | 14 | 143688 | 11974 | 155662 |
| Total | | 284112 | 23676 | 307788 |

Final substage subsets (already included, not additive): 13.6 12540 + 1045; 14.6 39336 + 3278. The latter main matrix includes 36096 regressions, 1800 positive and 1440 rejection cases. Separate stage 12 stress matrix excluded.
Host matrix: x86-64 native; x86, ARM32, AArch64, RISC-V32/64, PowerPC32/64, SPARC32/64, LoongArch64, s390x QEMU user mode. Sanitizers on x86-64 only.

## Available evidence
Historical extracted logs in [results](../variant-II/results/): stage-13.6-2026-11-27.log, stage-14.3-2026-11-27.log, stage-14.4-fix1-2026-11-27.log, stage-14.5-2026-11-27.log, stage-14.6-2026-11-27.log. SHA256SUMS-original-archives.txt records archive digests. Not every historical run has a raw log. Stage 11.1 standalone archive unavailable. Per-test timings, toolchain versions and raw outcomes not inferred where absent.

## Future run record
Assign run ID, date, commit, feature ID, stage/substage, host/target, native/emulated, toolchain versions, command, fixture digest, expected and actual outcome, exit code, sanitizer settings, log path and timing if measured. Distinguish skipped, failed and passed cases. Store new runs separately from historical totals.
