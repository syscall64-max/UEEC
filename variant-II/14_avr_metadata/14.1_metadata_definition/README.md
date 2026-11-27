# Stage 14.1 — AVR metadata definition (experimental)
Original hdrs/ueec.h is preserved verbatim. Section type METADATA=13, machine type MPU=1, machine AVR8=0. The original specification reserves METADATA and does not define its payload. This prototype uses a standalone payload reader; it does not parse a whole UEEC container or execute AVR code.

Wire offsets, multibyte integers in file byte order:

| Offset | Size | Field |
|---:|---:|---|
|0|2|format=0 (fixed 32-bit quantities)|
|2|2|design=0 (fixed AVR profile)|
|4|3|device signature, byte sequence|
|7|1|PC width in word-address bits|
|8|4|Flash size in bytes|
|12|4|SRAM size in bytes|
|16|4|EEPROM size in bytes|
|20|4|Flash page size in bytes|
|24|4|interrupt vector count|
|28|4|ISA feature mask (opaque)|
|32|4|addressing feature mask (opaque)|

Exactly 36 bytes. Zero SRAM/EEPROM allowed. Basic experimental checks: signature not all zero or FF; PC width 1–32; positive even Flash/page sizes, page divides Flash; Flash word count fits PC width. These are prototype assumptions, not device-specific compatibility checks. Feature bits not yet assigned. Synthetic fixtures are not a device database. No actual AVR compiler or hardware required; tests run the host-side decoder on the twelve Linux architectures. Device identification, memory regions, node selection, and full UEEC integration belong to later stages.

`make test`, `make sanitize` (native x86-64). Each architecture: 168 positive and 106 negative cases, including both byte orders, truncation, trailing bytes and invalid header/geometry. Native x86-64 validated; other targets pending.
