# Paper 01 / E01 revised: header inventory

Run `./run.sh` on Linux with Python 3, GCC, Clang and libc headers. `sha256sum -c SHA256SUMS` verifies the bundled fixtures. The CSV contains actual header measurements, not estimates. Empty cells mean not measured/not applicable, not zero. The analyzer handles the specific fixture formats, not arbitrary hostile files; Python assertions are required (do not run with -O).

## Fixture provenance and scope
- PE32 and PE32+: Clang cross-compiled the same `sample.c`, LLD linked freestanding entry points. DOS extra = e_lfanew minus 64, not necessarily pure stub. Optional directories are counted from the file. Header padding = SizeOfHeaders minus end of section table.
- ELF64: Clang/LLD freestanding entry point; counts ELF, program, and section header tables including null section. Tables need not be contiguous; sum is not a prefix length. No execution equivalence claimed.
- Mach-O64: Clang object only; installed ld64.lld rejected macOS linking. Not an executable comparison. Section descriptors counted separately from other load-command bytes (including segment command base). 
- UEEC: exact public first-generation fixture `UEEC-main/12_shared_metadata/examples/sum.ueec` from supplied UEEC-main.zip. Header parsing follows the stage generator. This is a 29-node workload, NOT equivalent to the one-function PE/ELF files. The C sizeof probe uses the unchanged first-generation 05_align header from E01.

No complete file-overhead comparison: relocation, symbols, string tables, imports/exports, payload padding and other auxiliary data outside the measured header structures are not fully classified here. No native execution, timing or second-generation test. Results cannot rank formats. No changes to the repository.

## Technical references
- https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- https://gabi.xinuos.com/elf/08-pheader.html
- https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/loader.h

`build_samples.sh` regenerates the conventional fixtures with Clang/LLD. Compiler versions may change output. The UEEC fixture is kept as supplied; verify its SHA separately if rebuilding others.
