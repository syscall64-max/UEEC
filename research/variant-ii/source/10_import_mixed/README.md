# Stage 10.3 — Mixed library import

The loader and import header are byte-identical to Stage 10.2. New fixtures combine a program and one internal library in a two-node container, with exactly one external library file alongside. Both directions (math internal/data external and vice versa), both export design assignments, all three import widths and both wire endiannesses are exercised. Missing external library cases must reject without running the program. Previous tests are retained.

`make x86_64`, `make sanitize`, `make all`. Per architecture: 49 positive (24 new mixed), 54 negative (12 new), 2 host conversions = 105. Native x86-64 and native ASan/UBSan passed. Remaining targets require user validation. The expected full matrix is 1260 checks, not yet a measured result.

Same experimental ABI and limitations as Stage 10.2: no recursive library dependencies, shared resident sections, general merge utility or unified granulated loading. No timing claims.
