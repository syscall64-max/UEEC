# Successful hardware-counter rerun

The source snapshot retains the original `trace_metrics.py`. The author changed `cycles,instructions` to `cycles:u,instructions:u` locally for `results-trace-04`. The first two trace archives contain failed perf attempts and successful strace observations; the final trace archive records 60 completed perf and 60 completed strace runs. The successful perf measurements cover user mode only.

To reproduce the amendment (from `source/10_performance`):

```bash
sed -i 's/cycles,instructions/cycles:u,instructions:u/g' trace_metrics.py
```

Use an unused output directory and ensure local perf permissions permit user-mode events. Restore any temporarily changed system security setting afterwards.
