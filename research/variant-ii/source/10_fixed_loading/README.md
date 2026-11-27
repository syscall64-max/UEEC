# Stage 10.7.1 — Fixed Loading (experimental)

The previous stages remain unchanged. This stage adds a LOAD_INFO metadata section and fixed-address monolithic loading to the stage 10.6 loader. It does not implement pre-relocation, prebinding or file-backed mmap.

Wire layout (container byte order): uint16 type=1, uint16 format=0/1/2 for 16/32/64-bit addresses, uint16 count, then count records of uint16 node index and address of the selected width. Count is outside the metadata header. A single metadata section belongs to the program in the generated fixture. Exactly one record per node is required. Node address means the runtime address of its first linked physical section. Physical sections including header and nonresident metadata are laid out in physical-index order, rounding each section_size to the runtime page size. All requested node bases must imply the same container base. Re-granulation or page-size changes can invalidate the layout: regenerate metadata. The preparer accepts ungranulated input and produces the final granulated image. Shared sections and external dependencies in fixed mode remain unsupported. Ordinary loading without metadata retains the three previous modes.

The loader reserves the entire region with MAP_FIXED_NOREPLACE, verifies the returned address, copies bytes and applies page protections. It still resolves and binds imports at runtime. The test-only loader_test binary can reserve a page and verify that a conflicting request leaves every byte unchanged; the ordinary loader has no test hook. Fixed loading is opt-in by preparing metadata and using monolithic mode. Address conflicts are errors, not silently relocated. Fixed layout reduces ASLR for the image. Format16 encoding is defined but no successful Linux 16-bit-address mapping is claimed; the test suite additionally checks overflow in the preparer for the selected base. Both 32- and 64-bit metadata encodings are exercised, independent of SH width.

```
make x86_64
make sanitize
make
```

Manual example (from this directory after build):
```
python3 ueec_merge.py combined.ueec build/x86_64/f16-le/{program,libmath,libdata}.ueec
python3 ueec_fixed.py combined.ueec fixed.ueec --base 0x30000000 --page $(build/x86_64/loader --page-size) --format 64 --granularity 4096
build/x86_64/loader --load-mode monolithic fixed.ueec
```

Native and ASan/UBSan: 741 prior checks + 144 positive fixed checks + 240 negative fixed checks = 1125 per run. Additional host-side 16-bit overflow checks are not counted. Cross-architecture execution is pending. This is functional testing, not exhaustive malformed input verification or a benchmark.
