# UEEC Variant II — 11.3 Native Node Loading

Derived from the original stage 02 loader and stage 11.2 architecture matching.
Run `make test` and `make sanitize` in 11_isa_dispatch. Each architecture runs
72 cases: both wire byte orders, all three uniform section header formats,
four successful loading modes and eight rejection cases per combination.
The runtime header is rebuilt in host byte order with aligned section addresses.
Selected sections are copied with pread, zero tails are checked, and mprotect
sets HEADER/constant R, CODE RX, other sections RW. The first nonempty CODE
is the entry point. No CODE is executed; fixture CODE is synthetic data.
Shared sections refer to the same global file sections across nodes, not
shared mappings across processes. Mixed section-header formats, relocations,
imports and exports are outside this stage. Protection tests check successful
mprotect calls, not deliberate fault probes. Input files must remain unchanged
while loading. Only x86_64 was tested in the preparation environment.
