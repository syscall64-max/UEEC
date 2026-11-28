# UEEC v2 — organization workspace

Research generation name, not a new on-disk version. The current container header still uses UEEC_VERSION_1. This workspace is additive; legacy stages 00–21 and variant-II research snapshots remain untouched. No migration or new validation is claimed.

- Core: shared wire definitions and draft section contracts.
- Linux: CPU, MPU, VM, GPU, FPGA.
- Windows, macOS, Android, iOS: planned platform branches.
- Bare_Metal, Embedded, Hypervisor: separate target environments; Linux may be their host tool environment.
- Validation: historical evidence, distinct from new regression tests.

See [inventory](Documentation/INVENTORY.md) and [validation](Validation/README.md).
