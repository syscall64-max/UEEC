# 07_pe — Variant II

PE_STYLE relocation (design 3): one node, monolithic loading only. Source is implicitly image_base; target is a local resident section. Patch offset = block + offset, patched value = image_base + addend. This is not a Windows PE file converter. SH16 little-endian fixtures use architecture-specific code encoding. Shared binaries are in ../test_resources/pe/.

`make all` tests all twelve targets, `make x86_64` selects one, and `make sanitize` tests native ASan/UBSan (leak detection disabled). Each target checks all twelve fixture layouts and executes its own fixture, rejecting old layout, unsupported section width, multiple nodes, wrong relocation design, and scattered-mode CLI. Native tests passed; cross execution pending. Malformed file validation is not exhaustive.
