# Stage 14.3 — AVR Memory Metadata

Build: `make test`, `make sanitize`. Previous metadata and device tests retained unchanged.

`memory le|be metadata.bin info` prints device name, SRAM start, exclusive SRAM end, Flash word count, Flash page count and EEPROM byte count.

`memory le|be metadata.bin SPACE START COUNT` validates a nonempty interval; decimal unsigned integers only. SPACE: flash (bytes), word (Flash words), page (Flash pages), sram (absolute data-space byte addresses), eeprom (bytes), register (data-space 0..31), io (data-space 32..95), extended-io (data-space 96..SRAM start exclusive). Output is byte start and byte count; word/page are converted to Flash bytes. Return 1 for invalid metadata/ranges, 2 for invocation/I/O errors. No actual memory read or write. The I/O ranges describe address windows, not a guarantee every register exists or is accessible.

Experimental classic AVR profiles inherited from 14.2; SRAM starts 0x60 (ATtiny85, ATmega8), 0x100 (ATmega328P, ATmega128 normal mode), 0x200 (ATmega2560). No changes to wire format. Vendor documentation has not been independently verified for every field. No physical AVR execution, external RAM, boot/fuse restrictions, or reverse conversion API yet.

Per host: 1054 regression cases + 238 valid and 600 rejected memory cases = 1892. Native x86-64 and ASan/UBSan verified; other hosts pending.

## Stage 14.4: AVR node selection
`select <lowercase-hex-device-signature> <file.ueec>` parses the full container using the Stage 11.3 parser, selects a unique native bare-metal AVR8 program node and validates the associated 36-byte METADATA against the known device profile. Container endian is independent of AVR instruction endian. Other machine/system types are ignored. Malformed eligible AVR nodes fail closed, including those for other known devices. Multiple matching nodes are rejected, even if they have different feature subsets. No code is executed or flashed. Uniform SH16/32/64 formats are supported; mixed formats are rejected. The inherited parser is a prototype, not a hardened untrusted-file loader (quadratic mapping scans, no concurrent file modification protection). Device database is unchanged and not fully independently vendor-verified.
New tests: 180 successful selections, 396 rejections per host; 2468 total including regressions. Tests compare file hashes before/after.

Fix 1: restored experimental S390X machine ID 11 from Stage 13.6. No wire layout changes.

## Stage 14.5: experimental AVR sequential layout
`layout <lowercase-signature> <file>` selects a bare-metal AVR node and validates a deterministic proposed layout. It does not write or execute firmware. Sections are placed by ascending global section index, CODE in Flash aligned to two bytes, CONSTANT_DATA in Flash, INIT_DATA in SRAM with initialized bytes also budgeted in Flash, UNINIT_DATA in SRAM. STACK reserves its section_size at the top of SRAM (one section, no payload, minimum two bytes); absent STACK uses an experimental 32-byte default. Entry is the first CODE start in byte units; no explicit entry field is defined. Metadata and debug are not resident. Other types are rejected, including RESOURCE (no agreed EEPROM encoding), relocation, heap and TLS. No fixed-address placement or arbitrary overlap validation is implied; sequential allocation and stack bounds prevent overlap in this policy. No startup copy/zero code, reset vector semantics, instruction decoding or actual stack usage analysis is performed. Original section_size file span rules remain; even uninitialized sections occupy padded file spans under this parser. Device profiles inherited without new datasheet validation.

Tests per host: 2468 regression + 150 positive + 390 negative = 3008. Full 12-host target: 36096. `make sanitize` runs the same suite on x86-64. Tests include exact Flash capacity split across SH16 sections, one-byte overflow, SRAM/stack collision, odd code, initialized data Flash cost, padding, unsupported types and malformed stack. SHA-256 checks source unchanged.
