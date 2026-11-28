# Stage 14.3 — AVR Memory Metadata

Build: `make test`, `make sanitize`. Previous metadata and device tests retained unchanged.

`memory le|be metadata.bin info` prints device name, SRAM start, exclusive SRAM end, Flash word count, Flash page count and EEPROM byte count.

`memory le|be metadata.bin SPACE START COUNT` validates a nonempty interval; decimal unsigned integers only. SPACE: flash (bytes), word (Flash words), page (Flash pages), sram (absolute data-space byte addresses), eeprom (bytes), register (data-space 0..31), io (data-space 32..95), extended-io (data-space 96..SRAM start exclusive). Output is byte start and byte count; word/page are converted to Flash bytes. Return 1 for invalid metadata/ranges, 2 for invocation/I/O errors. No actual memory read or write. The I/O ranges describe address windows, not a guarantee every register exists or is accessible.

Experimental classic AVR profiles inherited from 14.2; SRAM starts 0x60 (ATtiny85, ATmega8), 0x100 (ATmega328P, ATmega128 normal mode), 0x200 (ATmega2560). No changes to wire format. Vendor documentation has not been independently verified for every field. No physical AVR execution, external RAM, boot/fuse restrictions, or reverse conversion API yet.

Per host: 1054 regression cases + 238 valid and 600 rejected memory cases = 1892. Native x86-64 and ASan/UBSan verified; other hosts pending.
