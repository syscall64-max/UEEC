# Feature registry — experimental v2
IDs below are organizational identifiers, not wire values. Status refers to historical research snapshots, not new regression tests.

| ID | Feature | Evidence | Status |
|---|---|---|---|
| CPU-01 | Multi-ISA node selection | Linux/CPU/ISA_Dispatch/11.2_node_selection | tested prototype |
| CPU-02 | Loading and native entry execution | 11.3–11.5 | tested prototype |
| CPU-03 | Shared constants, initialized data and BSS; repeat-load state reset | 11.5 | tested prototype |
| REL-01 | Absolute and multiple relocations | 12.1–12.2 | tested prototype |
| REL-02 | Instruction relocation | 12.3 | tested subset |
| REL-03 | Block encoding and conversion | 12.4–12.6 | seven representations tested |
| REL-04 | Overlap rejection and selected write protection | 12.6 | tested subset; NX not established |
| EXP-01 | Compact group, simple flat, ordinal | 13.1–13.6 | tested prototype |
| IMP-01 | Named/ordinal import and resolution | 13.2–13.3 | experimental design |
| IMP-02 | External calls and data | 13.4–13.5 | restricted test ABI |
| IMP-03 | Recursive dependencies and shared state | 13.6 | explicit registry; cycle rejection |
| AVR-01 | Metadata and device identification | 14.1–14.2 | five provisional profiles |
| AVR-02 | Memory geometry and node selection | 14.3–14.4 | tested host tools |
| AVR-03 | Layout validation | 14.5 | tested host tools |
| AVR-04 | Flash/SRAM/Intel HEX image generation | 14.6 | tested host tools; not necessarily bootable |

Sources: [research snapshot](../variant-II/README.md), [CPU inventory](Linux/CPU/README.md), [MPU inventory](Linux/MPU/README.md). Stage 11.1 standalone source missing. No general ABI, production loader, physical AVR validation or GPU/FPGA implementation claimed.
