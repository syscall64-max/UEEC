# Linux / MPU — UEEC v2

Linux is the host environment for these tools; the AVR target is bare metal. The AVR tree is an exact copy of the stage 14 research snapshot at `64d352d35d774113e69ee61b9803f6ffac172601`. No source, header or Makefile changes. Central Core headers are not yet adopted.

## AVR (stage 14)

| Substage | Scope |
|---|---|
| [14.1](AVR/14.1_metadata_definition/) | Metadata definition |
| [14.2](AVR/14.2_device_identification/) | Device identification |
| [14.3](AVR/14.3_memory_metadata/) | Memory metadata |
| [14.4](AVR/14.4_node_selection/) | Node selection |
| [14.5](AVR/14.5_layout_validation/) | Layout validation |
| [14.6](AVR/14.6_integration/) | Flash/SRAM/Intel HEX image integration |

Reported historical successful executions: 143688 main matrix + 11974 x86-64 ASan/UBSan = 155662, including repeated regression and sanitizer tests. Final 14.6: 36096 regression + 1800 new positive + 1440 new rejection cases = 39336 main; 3278 sanitizer; total 42614 (included in stage total, not additional). x86-64 native; eleven other Linux host targets via QEMU user mode. No physical AVR execution is implied. Source: [Variant II snapshot](../../../variant-II/README.md), [historical logs](../../../variant-II/results/) and [validation](../../Validation/README.md).

Five experimental device profiles; vendor data not fully independently verified. No EEPROM image, AVR relocations, automatic startup/reset vector or physical execution. Images are not necessarily bootable firmware.

Reproduction: run `make test` and `make sanitize` from AVR with configured cross compilers and QEMU. No new run or relocation of build dependencies has been verified.

## STM32

Variant II stage 15 not started; v1 experiments remain historical and unchanged.
