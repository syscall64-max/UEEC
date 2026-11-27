# UEEC Variant II — 13.4 External Function Calls

Based on the tested 13.3 resolver. No wire-format changes. `make test` builds raw freestanding payloads for all 12 targets and runs the original resolver suite plus the external-call suite. `make sanitize` repeats x86-64 using ASan/UBSan on the loader (not the raw payloads). Cross compilers and QEMU are required as in previous stages.

The application imports libalpha::answer, libalpha::ordinal 7 and libbeta::value. Both alpha exports refer to an addition function; beta exports multiplication. Native application code calls them with (20,22), (7,5), (6,7), returning 96. Three application entry invocations per positive case, nine external calls total. Function addresses are written into INIT_DATA at offsets 0,8,16. PPC64 ELFv1 calls use temporary descriptors for raw leaf functions without TOC dependencies. The test ABI is intentionally restricted to int(int,int); it is not a general ABI or a signature validation mechanism.

Per architecture: 396 original regression cases, 144 new positive cases, 108 new negative cases (missing symbol, incorrect slot, data instead of code). All three files are checked for unchanged SHA-256. All nodes have their own CODE sections. Raw payloads must have no .text relocations; extraction rejects such objects. Only x86-64 execution was verified locally; other target payloads must be compiled and tested on the user's system. Local nonselected node payloads were placeholders, not included in distribution. No recursive dependencies, general ELF loading, or external data access tested here. Sanitizers instrument the loader only.

## Fix 1 — LoongArch64
The user observed .text relocations in the original GCC 15 object. Exact relocation types were not supplied. LoongArch64 now uses explicit relocation-free assembly and -mno-relax; the extractor still rejects any .text relocations. Other targets retain their original C payload. Run `make payload-loongarch64` first, then `make test` and `make sanitize`. The LoongArch64 compiler and emulator are unavailable locally; this fix requires target verification.

### Fix 2 — PPC64 raw calls
PPC64 APP/ADD/MUL now use explicit assembly. The APP loads raw code
addresses from slots 0, 8, 16 and branches using CTR; it saves LR, TOC,
and nonvolatile registers in an ELFv1-compatible stack frame. No synthetic
C function descriptor is used. The existing Loader bridge is unchanged.
Clang cross-assembly and relocation-free extraction verified; PPC64 runtime
execution requires the user's GCC/QEMU environment.

# Stage 13.5 — External data access
Run `make test` and `make sanitize` as before. `data.py` adds 144 positive and 108 negative cases per target, retaining all 648 previous cases. The new `--data` test ABI passes three raw pointers at slot offsets 0, 8, 16. The application reads an exported constant byte (100), increments an initialized byte (20) by 10, and increments a zero-initialized byte by 42. The three calls must return 172, 224, 276; the loader independently checks the actual exported bytes after each call. Metadata endianness is independent across modules. Both name designs and ordinal imports are tested. Negative cases cover missing symbols, wrong slots, and wrong section type. This is a byte-sized data test, not a general typed data ABI. No separate forbidden-write test or multiple-consumer sharing test is added. Library functions do not modify the exported data in this stage. Local x86-64 tests used placeholder payloads for nonselected nodes; the archive contains source only. All other targets need actual execution verification. Existing experimental IMPORT format unchanged.

## Stage 13.6 — dependency integration

`graph.py` adds 72 successful cases and 72 rejection cases per architecture, plus one separate write-protection check. All earlier tests are retained (900 per architecture). The positive fixtures load four UEEC files; the app and two libraries import the same common data, and each executes the existing DATA payload once in sequence. The app also imports marker symbols from both libraries, and carries a non-overlapping absolute relocation verified against its own constant section. Negative fixtures cover a dependency cycle, a missing transitive library, a duplicate import slot, and an import/relocation overlap. SHA-256 is checked for all four input files. Metadata endian, SH and import widths, and all four loading modes vary.

The `--graph` and `--graph-protect` modes use an explicitly supplied registry (maximum 128 libraries), map each registry entry once, walk dependencies with DFS and reject cycles, bind dependencies before consumers, then run the fixture-specific shared-data checks. They are not a general-purpose application ABI. Registry entries are eagerly mapped, unused entries may be mapped, and different aliases of the same file are not deduplicated. The protection mode additionally runs the existing fork-based write protection probe on the common module. The graph test uses byte-sized data and does not combine imported function calls with imported data in one native function. Multi-width data, full PPC64 TOC ABI, NX, and generalized dependency discovery remain outside this package. No on-disk format changes.

Local x86-64: all 900 inherited cases passed in ordinary and ASan/UBSan builds; all 144 new graph cases and the separate protection check passed in both builds. Other architecture payloads were represented by placeholders only for local x86-64 fixture generation; no generated files are shipped. Run `make test` and `make sanitize` in the parent directory to compile all real payloads on your toolchain.
