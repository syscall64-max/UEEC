# Experimental environment (user-supplied snapshot, 2026-11-28)

- Intel Core i5-10400, x86-64, 6 cores / 12 logical CPUs, 2.90 GHz base, 4.30 GHz reported maximum.
- Little-endian; 39-bit physical and 48-bit virtual address size; one NUMA node.
- L1i and L1d each 192 KiB total; L2 1.5 MiB total; shared L3 12 MiB.
- RAM reported by OS: 14 GiB; swap: 4 GiB; page size: 4096 B.
- Ubuntu 26.04.1 LTS (Resolute Raccoon); Linux 7.0.0-44-generic; GCC 15.2.0; glibc 2.43; Python 3.14.4; QEMU AArch64 10.2.1.
- x86-64 target executed directly; other eleven CPU targets were executed using QEMU user-mode during V01. The exact versions of all cross-compilers and QEMU targets were not collected.
- The snapshot does not establish CPU frequency, affinity, or load at each measurement; cold-cache testing was not performed.

This is a research-relevant extract of the supplied environment snapshot, not a copy of host-specific system diagnostics.
