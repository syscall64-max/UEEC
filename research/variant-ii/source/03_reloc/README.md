# 03_reloc — Variant II

Shared DATA plus 12 CODE/RELOC pairs, migrated from the existing SECTION_FLAT example. SH16 little-endian container. Two executions per architecture (monolithic and scattered) plus structural assertions. Architecture-specific relocation binary fixtures are shared in `../test_resources/reloc/`.

Run `make all`, `make x86_64`, `make sanitize`, or `make help`. Native optimized and ASan/UBSan tests passed; cross execution pending. Leak detection is disabled for this legacy example. See root README for limitations.
