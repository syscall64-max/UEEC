# UEEC Variant II — 11.4 Native Node Execution

Run `make test` and `make sanitize` from 11_isa_dispatch. The compiler and QEMU names match stage 11.3. All twelve cross-compilers are required even for the individual architecture test, because each valid fixture contains twelve real machine-code nodes. Python extracts `.text` from the assembled relocatable ELF objects and rejects text relocations. No placeholder code is shipped.

Every fixture contains twelve nodes, shared constant/data/BSS sections and architecture-specific leaf routines returning an int (0, 42, 255). Return values rotate by machine ID; the valid fixture bytes for each metadata format and value index are identical across architecture runs. The loader invokes the selected code using the target C ABI, with a temporary ELFv1 function descriptor on PPC64 when needed. ARM32 uses ARM state. The inherited cache flush and page protection logic is retained. `--no-run` preserves loading-only behavior.

Per architecture: 72 execution tests (2 metadata byte orders x 3 section header widths x 4 load modes x 3 return values), 6 loading-only tests and 48 rejection tests. Total 126 per architecture, 1512 across 12 architectures (864 execution tests). Sanitizers repeat 126 cases on x86-64. Return values are printed rather than used as process exit codes.

Local validation: native x86-64 126/126 normal and 126/126 ASan/UBSan with sentinel bytes in the eleven unselected nodes, since cross toolchains are not installed here. The distributed Makefile always assembles actual code for all twelve nodes. Cross compilation and QEMU execution must be verified in the target environment. No relocations, imports, exports, arbitrary program startup ABI, or process isolation are implemented. Execute only trusted fixtures; UEEC machine code is not sandboxed. The metadata byte order does not change the instruction byte order.

### PPC64 ELFv1 hotfix
The raw entry is invoked through an assembly bridge with a real ELFv1
function descriptor, preserving LR and r2. This is intended for the existing
TOC-independent leaf payloads; general TOC-dependent code is not supported.
Bridge assembly and host C syntax were checked; PPC64 execution requires
confirmation on the cross-toolchain/QEMU test machine.
