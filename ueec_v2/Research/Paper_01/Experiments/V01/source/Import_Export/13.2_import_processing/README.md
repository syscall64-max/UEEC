# Stage 13.2 — experimental IMPORT processing
The original UEEC spec reserves section type 7, without defining its wire layout. This proposal is not a standard. All multibyte fields use file metadata endianness, packed without padding.

Header: u16 format (0/1/2), u16 design (0), width-sized library count. Width is 2/4/8 bytes.
Each library: u16 name length, name bytes (nonempty, no NUL), width-sized entry count.
Each entry: u8 kind (0 name, 1 ordinal), width-sized resident section index (excluding HEADER), width-sized target offset; then u16 name length and name bytes for kind 0, or width-sized ordinal for kind 1.
Target must be aligned to native pointer width, within INIT_DATA or UNINIT_DATA. Duplicate/overlapping target slots rejected. Exactly one IMPORT section per selected node. IMPORT is excluded from runtime HEADER. No symbol binding or writes occur; diagnostic output before validation completes is not a commitment to validity. No typed signatures, library loading, cross-table relocation overlap validation or multiple IMPORT tables yet.

`make test` tests all 12 architectures with same toolchains as 13.1; `make sanitize` tests x86-64. Each architecture: 72 valid fixtures/modes, 216 rejection tests. Only x86-64 was verified in the authoring environment. All test files generated from source; payload CODE not executed.
