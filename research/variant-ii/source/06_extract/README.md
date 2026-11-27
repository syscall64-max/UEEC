# 06_extract — Variant II

12 node extractions, mapping/payload checks, ambiguity and missing-node rejection, and both loading modes. Retains the forced-inline V3 relocation loader. Includes extraction from a 512-byte granulated fixture; section_size and data_size are preserved independently. The host extractor supports little-endian SH16 only. Tests do not establish general malformed-file safety.

Run `make all`, `make x86_64`, `make sanitize`, or `make help`. See ../README.md for common architecture names, toolchains, scope and limits.
