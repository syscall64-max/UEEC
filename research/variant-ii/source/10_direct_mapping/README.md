# Stage 10.7.2 — Pre-relocation & Prebinding

Experimental closed-container prelink for the Stage 10 fixture ABI. The preceding stages are unchanged. `ueec_prelink.py` reads a fixed-layout container offline, applies a SECTION_FLAT pointer-sized ABS32/ABS64 relocation and resolves four named internal imports (COMPACT_GROUP or SIMPLE_FLAT exports). It writes a separate output file, preserving section sizes and original source. Existing metadata type 1 means fixed layout; experimental type 2 uses the same layout and indicates prelinked slots. Import and relocation records are retained; they are not applied again by the prepared execution path. No automatic rebase/fallback. Original fixed input is the recovery source.

The new fixture calls two imported functions and reads two imported data values plus a fifth pointer produced by relocation. Its result is 40. The normal loader path performs 1 relocation and 4 lookups/bindings; the prepared path performs zero of these operations. Counters report actual calls/patches in this experiment, not syscall totals. PPC64 ELFv1 function descriptors are reserved in the fixture's own DATA section for offline preparation (not yet cross-tested). Relocation local indexes include the header, unlike IMPORT/EXPORT resident indexes. Target endianness is independent of wire endianness.

Scope: monolithic fixed layout, three-node fixture with no recursive imports, unique physical sections, a single relocation couple and four import slots. This is not a general prelinker or a security verifier of untrusted executable code. Type 2 is an experimental metadata discriminator, not an integrity guarantee; prepared pointers receive basic image-range checks only. Fixed addresses reduce image ASLR and can conflict; MAP_FIXED_NOREPLACE rejects conflicts. Do not regranulate or move the prepared image without rebuilding from its original source. Page size must match the prepared layout. No direct file-backed mapping yet; memory is still copied. The new runtime relocation path supports the fixture's writable data only.

## Run

```
make x86_64
make sanitize
make
```

Each architecture: 1125 existing checks + 288 positive + 72 negative = 1485 checks. All 12 would total 17820 (expected, not yet observed). Native x86-64 and ASan/UBSan logs under validation/ show 1485/1485 each. The other 11 target toolchains are unavailable in the development container; run the matrix on the research host. No benchmark claims.

## Manual fixture

After `make build-x86_64`:

```
python3 ueec_merge.py combined.ueec build/x86_64/f16-le/{program,libmath,libdata}.ueec
python3 -c 'from pathlib import Path; from test_prelink import fixture; Path("reloc.ueec").write_bytes(fixture(Path("combined.ueec"),Path("build/x86_64"),"x86_64"))'
python3 ueec_fixed.py reloc.ueec fixed.ueec --base 0x30000000 --page $(build/x86_64/loader --page-size) --format 64 --granularity 4096
python3 ueec_prelink.py fixed.ueec prelinked.ueec --page $(build/x86_64/loader --page-size)
build/x86_64/loader --load-mode monolithic fixed.ueec
build/x86_64/loader --load-mode monolithic prelinked.ueec
```

## Stage 10.8 — Direct Mapping & Execution

`loader --load-mode direct FILE.ueec` accepts the Stage 10.7.2 experimental prelinked fixture when every physical section (including header) is a nonempty multiple of the runtime page size. Prepare using `ueec_fixed.py` with `--granularity` equal to the runtime page size or a supported multiple, then `ueec_prelink.py` with the same page size. The existing modes remain available. Direct mode opens a regular file read-only, maps it privately for inspection, validates bounds, fixed layout and zero padding, then maps the same file descriptor privately at the requested base with MAP_FIXED_NOREPLACE. It applies per-section protections, executes the existing entry ABI, and unmaps both regions at cleanup. Writable data is private (copy-on-write). The loader performs no section memcpy and no runtime relocation, symbol lookup or binding. MAPPINGS=1 counts the execution-region mapping only; INSPECTION_MAPS=1 counts the separate inspection mapping. This is not a claim of zero kernel copies or page faults. Padding validation touches file pages; concurrent file modification/truncation is unsupported (use trusted immutable inputs). The inherited experimental prelink validation is not a security verifier for arbitrary executable inputs. No automatic fallback or performance claim.

`make x86_64`: 1485 regression + 96 positive (48 direct and 48 copied comparison runs) + 144 negative checks = 1725, all passed. `make sanitize`: same matrix passed under ASan/UBSan. Logs in validation/. Other target architectures await validation. Test granules are runtime page and twice runtime page (fixture generator limits granules to 16384); environments with larger page sizes require further fixture work. Negative tests include unprepared file, unaligned sections, occupied fixed region, nonzero padding and truncation. Test harness corrected to read actual merged SH width instead of requested fixture width (swapped mixed-width inputs can widen).
