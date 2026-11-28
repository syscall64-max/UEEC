# Linux / CPU — UEEC v2

The three historical research stage trees below are copied byte-for-byte from baseline commit `64d352d35d774113e69ee61b9803f6ffac172601`. Original stage numbering and relative paths within each tree are retained. No include or Makefile changes; central Core headers are not yet adopted. No new test run is claimed.

| Directory | Stage | Main executions | x86-64 sanitizer executions |
|---|---|---:|---:|
| [ISA_Dispatch](ISA_Dispatch/) | 11 | 3468 | 289 |
| [Relocation](Relocation/) | 12 | 82728 | 6894 |
| [Import_Export](Import_Export/) | 13 | 54228 | 4519 |
| Total | | 140424 | 11702 |

Total 152126 reported successful executions including repetitions. Evidence: [Variant II snapshot](../../../variant-II/README.md) and [validation](../../Validation/README.md). Native x86-64; eleven other Linux architectures via QEMU user mode. Separate relocation stress matrix excluded. Individual run coverage and historical raw log availability vary.

## Substage inventory

- 11.1: standalone source unavailable; historical results included in stage total.
- 11.2_node_selection
- 11.3_node_loading
- 11.4_node_execution
- 11.5_dispatch_integration
- 12.1_flat_absolute
- 12.2_multiple_relocations
- 12.3_instruction_relocation
- 12.4_block_relocation
- 12.5_relocation_conversion
- 12.6_part_a: preserved intermediate snapshot
- 12.6_full_integration: final snapshot
- 13.1_export_processing
- 13.2_import_processing
- 13.3_symbol_resolution
- 13.4_external_calls
- 13.5_external_data
- 13.6_full_integration

## Reproduction

Run `make test` and `make sanitize` from the respective stage roots, after configuring cross toolchains and QEMU. These commands are inherited from the original snapshots; portability and dependency paths have not been revalidated in the new location. Keep historical source snapshots intact when integrating Core.
