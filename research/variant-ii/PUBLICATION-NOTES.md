# Publication package

- `source/`: latest source snapshot, extracted unchanged from the ELF-comparison archive. Earlier README status statements are historical, not updated retrospectively.
- `archives/source/`: 28 cumulative development snapshots, including the initial design and all performance extensions.
- `archives/results/`: 11 unique user-result archives, including failed perf attempts. Identical duplicate submissions are recorded by hash in the inventory rather than duplicated.
- `validation/user-logs/`: available Variant II terminal output. Earlier Variant I logs are excluded.
- `evidence/`: original summaries, trace status records, archive inventory, independently recomputed ELF medians and perf correction.

21,008 raw timing rows were counted across eight distinct physical-machine result archives. This excludes trace observations and pilot runs. The baseline 6,006 raw timing rows are not available as a separate archive in this package. No cumulative functional test counts are summed as independent tests. The historical source archives and supplied results are preserved byte-for-byte; no new test execution is claimed by packaging.

Research limitations: fixed-address direct mapping; no preprocessing cost in timing; extra 64 MiB lazy payload not read except in ELF full-access case (one byte per page); user-mode hardware counters only. Experimental code, not a production loader or security proof. Review logs and environment metadata before public publication; they may contain local paths and machine details.
