# 01 — UEEC Information / Variant II

Reads a mapped file, checks its wire layout and prints nodes, names, mappings and sections. The 10-byte global header stores the section format at offset 4, node count at 6 and name count at 8. Node headers are 4 bytes. The node mapping and section descriptor layouts are unchanged. Identifier 2 is required. The metadata byte order is independent of machine-code byte order.

`make all` runs all twelve architecture targets; `make x86_64` runs one. `make sanitize` runs the native sanitizer build. `make fixtures` generates the test containers from the common BIN library. For every parser architecture, tests cover 6 ordinary and 6 shared-constant-section fixtures (SH16/32/64 × LE/BE), and 5 rejection cases (bad magic, identifier, format, truncated header and payload). The shared constant section belongs to all twelve nodes. Stage 01 does not execute the embedded code. `ueec.asm` is an optional NASM example, not part of the automated test.
