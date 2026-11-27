# Stage 10.2 — Internal library import

`make all` executes the same 12-architecture matrix as 10.1, plus 12 positive internal and 6 negative internal cases per architecture. `make x86_64` and `make sanitize` are available. Expected per architecture: 25 positive, 42 negative, 2 host conversions (69 total). Native x86-64 and native ASan/UBSan passed; other targets await validation.

The fixture generator creates three-node containers directly, with independent program, math library and data library nodes. Internal names match the container node names (libmath, libdata); their external counterparts use filenames. The import record structures remain unchanged. All nodes refer to one file buffer, but resident sections are individually mapped. Exports keep both named designs and swapped widths. Internal fixtures are in a subdirectory without external library files.

Scope: same fixture entry ABI as 10.1; no library dependencies, no shared physical sections, no general merge utility, no granulation or monolithic loading yet. The loader accepts internal and external imports, but mixed imports are not specifically tested here. Experimental caps: 65 images/nodes, 64 imports libraries, 4096 objects, 64 MiB per file. No performance or exhaustive security claims.
