# UEEC v2 — current experimental wire contract
Status: draft inventory, 2026-11-28. Project generation v2 is not wire version 2; current magic/version is _GM/1. Source baseline 64d352d35d774113e69ee61b9803f6ffac172601. This document describes observed prototypes, not a ratified specification or backward compatibility guarantee.

## Container
All multibyte metadata uses declared file endian (0 little, 1 big). Payload instruction endian is separately represented by node encode. Explicit wire decoding is authoritative; C bitfields depend on compiler ABI.
- File header (8 bytes): magic[3] = 5f 47 4d; byte 3 low 7 bits version (1), high bit endian; u16 nodes and u16 names (real counts).
- Node header (6 bytes): u32 packed word, u16 format. Word bit positions from LSB: encode 0; kernel_mode 1; user_mode 2; program 3; library 4; console 5; service 6; machine_type 7–9; machine 10–17; system_type 18–19; system 20–26; section_count 27–31 (stored count minus one).
- Node mapping (6 bytes): u16 node, u32 global section. HEADER index 0 implicit; explicit mappings exclude it. Real section count 1–32 includes HEADER.
- Section headers: u16 type followed by section_size and data_size, each 2/4/8 bytes for SH16/32/64 (format 0/1/2), total 6/10/18 bytes. Current parser requires uniform SH width. data_size <= section_size; physical spans use section_size, including padding.
- Name table: name_len array (u8 per name), node_name_index array (u16 per node), then name bytes. Header precedes node headers, mapping table, section headers, name table, then section payload spans. Maximum nodes/names 65535; maximum name length 255; additional parser restrictions apply.
- Section types 0–13: HEADER, CODE, INIT_DATA, UNINIT_DATA, CONSTANT_DATA, RELOCATION, EXPORT, IMPORT, DEBUG, TLS, STACK, HEAP, RESOURCE, METADATA. Identifiers do not imply implementation.

## Namespaces
machine_type: CPU 0, MPU 1, VM 2, GPU 3, FPGA 4.
CPU IDs 0–11: ARM32, ARM64, LOONGARCH64, POWERPC32, POWERPC64, RISCV32, RISCV64, SPARC32, SPARC64, X86, X86_64, S390X.
MPU IDs 0–5: AVR8, STM32, PIC8, ESP32, MSP430, NORDIC.
VM IDs 0–3: UEEC, JAVA, DOT_NET, WASM.
GPU IDs 0–4: NVIDIA, RADEON, INTEL, APPLE, ARM_MALI.
FPGA IDs 0–5: XILINX, ALTERA, LATTICE, MICROCHIP, ACHRONIX, EFINIX.
system_type: BARE_METAL 0, EMBEDDED 1, GENERAL_OS 2, HYPERVISOR 3.
GENERAL_OS IDs 0–4: ANDROID, IOS, LINUX, MACOS, WINDOWS. EMBEDDED: FREE_RTOS 0.
Identifiers are scoped to their parent namespace.

## Section contracts
- [Relocation](Core/Specification/Relocation.md): seven representations, independent couple count width; architecture-specific type audit pending.
- [Export](Core/Specification/Export.md): three designs, three widths; explicit endian serialization.
- [Import](Core/Specification/Import.md): experimental design 0, named or ordinal symbol.
- AVR METADATA: u16 format=0, design=0; signature[3], u8 pc_bits; seven u32 fields flash_bytes, sram_bytes, eeprom_bytes, flash_page_bytes, interrupt_vectors, isa_features, addressing_features. Total 36 bytes. Feature namespaces provisional.
- CPU metadata and exact relocation opcode semantics require further source audit. Entry ABI, mixed SH widths, full security rules and additional section payload contracts are not finalized.

Sources: [Core header](Core/Headers/ueec.h), [AVR header](Core/Metadata/MPU/AVR/avr_metadata.h), historical stage codecs and [inventory](Documentation/INVENTORY.md). No new wire version or format migration is authorized by this document.
