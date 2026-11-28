# Platform matrix
Status refers to v2 only. Linux CPU: historical tested prototypes; Linux MPU: host tools for bare-metal AVR, not AVR running Linux.

| Host/platform | CPU | MPU | VM | GPU | FPGA |
|---|---|---|---|---|---|
| Linux | experimental tested (12 targets) | AVR host tools tested; STM32 planned | planned | planned | planned |
| Windows | planned | planned | planned | planned | planned |
| macOS | planned | planned | planned | planned | planned |
| Android | planned | planned | planned | planned | planned |
| iOS | planned | planned | planned | planned | planned |

Linux CPU host targets: x86-64 native; x86, ARM32, AArch64, RISC-V32/64, PowerPC32/64, SPARC32/64, LoongArch64, s390x emulated. Scope: stage 11–13 experimental matrices, not full ISA/ABI coverage.
Bare metal AVR: metadata, selection, layout and image generation only; no physical execution. Embedded (including FreeRTOS) and hypervisor: identifiers/planned, no verified v2 runtime. v1 capabilities are separate.
