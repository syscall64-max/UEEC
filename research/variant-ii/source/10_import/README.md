# Stage 10.1 — external library import (COMPACT_GROUP)

Research checkpoint for Variant II, not a general production dynamic loader.

## Wire layout

All multibyte metadata is stored in the container's declared byte order. The IMPORT header deliberately uses `design` then `format` (unlike the existing EXPORT header). Design 0 is COMPACT_GROUP; format 0/1/2 means width W = 2/4/8 bytes. The import format is independent of global SH and EXPORT widths.

| Field | Size |
|---|---|
| `ueec_import_hdr` | 4 |
| `library_count` N | W |
| `libraries[N]` | N*W |
| `object_counts[N]` | N*W |
| `objects[M]` | M*3*W |
| `names` | Remaining data bytes |

M is the sum of all object counts. Each library word contains an external flag in the MSB (0=internal, 1=external) and a name offset in the remaining bits. `import.h` uses fixed-width integers and masks, avoiding implementation-defined bit-field layout. Each object has `section`, `offset`, `name_offset` of width W. Records are grouped in library order. Section indices are zero-based, node-local **resident** section indices (CODE, INIT_DATA, UNINIT_DATA, CONSTANT_DATA), as in the existing export design. Offsets into resident sections are byte offsets; imported addresses are written using the target ABI pointer width and native byte order. All name offsets are relative to the common names blob at the end of the IMPORT section, and names are nonempty NUL-terminated strings. These choices are provisional research rules. No relocation or EXPORT wire structure was changed.

## Experiment

Three independent single-node UEEC files are created in each test directory:

* `program.ueec`: CODE, writable DATA (four pointer slots), IMPORT.
* `libmath.ueec`: two CODE sections, named exports `add`, `sub`.
* `libdata.ueec`: DATA with two native-endian int32 values 20 and 22, named exports `value1`, `value2`.

The loader maps independent resident sections, maps code RX and data RW, resolves the four symbols from the libraries and writes addresses to the program DATA slots. The executable payload is compiled from `payload.c` with the target compiler and extracted from its three `.text.*` sections. Unresolved code relocations are rejected during extraction. Actual target machine code computes `sub(add(*value1,*value2),2)` through the imported pointers and returns 40. The experiment uses a provisional `int entry(const struct slots *)` ABI; the loader passes the program's DATA address. PPC64 ELFv1 uses function descriptors for the self-contained raw code. The fixture does not use an external C runtime or general-purpose relocation mechanism.

For each target, run 6 original configurations (16/32/64-bit metadata x LE/BE), 6 swapped-design configurations (same program bytes; library EXPORT and SH widths vary independently), 36 negative tests (missing library, missing symbol, invalid destination section, invalid destination offset, invalid name offset, truncated file), and 2 host conversions using the existing stage 09 converter on the LE SH16 libraries followed by one further program execution. Expected: 13 positive loader executions + 36 rejected inputs + 2 host conversions = 51 subprocess checks per architecture, 612 across the 12 targets if all pass. The test fails rather than reporting missing compilers/runners as passes.

## Commands

```
make x86_64
make sanitize
make all
# or run one already-generated example:
./build/x86_64/loader build/x86_64/f16-le/program.ueec
```

`make build`, `make test`, `make clean`, and `make help` follow the common convention. Each target compiler must have a matching objdump/objcopy. Stage 10 source payloads are built on demand instead of reusing the unrelated earlier answer/exit binaries. No changes to existing shared binary resources.

## Validation and limits

Native x86-64 on GCC 14.2: 51/51 checks passed in optimized and ASan/UBSan loader builds; stage 09 converter is a normal host build in both cases, and the raw payload is not sanitizer-instrumented. Logs in `../validation/stage10-*.log`. Other 11 targets have Makefile configurations but were not built or executed in this environment (cross toolchains/QEMU absent). Full cross-platform support is pending real tests. The new loader only supports one node per file, named imports from independent external libraries located beside the program, no library dependencies, no relocations, no ORDINAL resolution, no internal imports, no merging, no granulation, no monolithic/hybrid mode. Experimental resource limits: 64 external library records, 4096 import records, 64 MiB per file. Group lookup uses a sequential scan of group descriptors and binary search within a matching group; flat lookup uses binary search. No performance measurements yet. Negative cases are targeted checks, not comprehensive malformed-input or security validation. Execute trusted UEEC code only.
