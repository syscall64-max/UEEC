# UEEC Variant II — 12.6 Full Relocation Integration (Part B)

Run `make test` and `make sanitize` from `12_relocation`. Same cross compilers and QEMU runners as Part A. No generated payloads or fixtures included.

## Tests

Per architecture: 1728 positive + 84 rejection conversion regression cases; 288 mixed positive + 6 rejection cases; 12 final positive + 12 rejection cases. The 12 final fixtures cover both metadata byte orders, SH16/32/64, and Couple32/64. Each runs three load cycles with two native calls per cycle, in monolithic mode. The entry branch is relocated to a second CODE section; absolute pointers are also relocated into CONSTANT_DATA and UNINIT_DATA and checked by the loader. Children probe write permissions for each runtime section (including HEADER); expected faults are checked with waitpid. Core dumps are disabled by the test harness. This tests write protection, not execution from non-executable pages.

The couple format field (upper 6 bits of the couple word) uses experimental values 0/1/2 for 16/32/64-bit count, consistent with the previous Couple16 encoding. The converter decodes all three and emits the smallest representable count width. The original v1 structures remain unchanged. All pending patch ranges are collected and sorted by a heapsort before overlapping ranges are rejected, then applied. Overlap checking is O(n log n) worst-case, O(n) range storage. Invalid addends/opcodes prevent execution and release the private image.

`--stress` uses the actual C decoder, range validator and patch application with synthetic BLOCK_N_64 relocation sections (1000, 10000, 100000 entries, five samples each). It verifies every pointer and checks overlap rejection at each size. Timings use CLOCK_MONOTONIC and exclude fixture creation, clearing and final pointer verification. This is a C relocation microbenchmark, not full loader or all-format C timing. QEMU and native timings must be distinguished.

`build/integration-report.json` contains regression, mixed and final summaries. `build/<arch>/final-report.json` contains stress timings and medians. `build/benchmark.json` retains the seven-format Python decoder benchmark. `make sanitize` instruments only the x86-64 loader and can replace its per-architecture reports; the aggregated normal report remains unchanged.

Scope: cross-CODE branches are tested only in monolithic mode and only for the supported unconditional branch encodings; other layouts may exceed ISA displacement range and are rejected. Absolute patches support pointer-sized addresses in INIT_DATA, CONSTANT_DATA and UNINIT_DATA; addends remain unsigned. The test program's entry ABI is unchanged, and final data pointers are checked by the loader, not all dereferenced by the native payload. Import/export and full executable ABI remain outside this stage. The final fixture generator depends on the same cross toolchains as Part A.
