# UEEC — Contributor and AI Agent Instructions

Applies to the **entire repository**. Its purpose is to make changes
traceable, reproducible, minimally invasive, and safe to review.

## Ground rules

1. Treat GitHub's current checked-out code, the format specification,
   and reproducible test results as the evidence base. Read
   `PROJECT_STATUS.md` before starting; **do not assume it updates itself**.
   Check the actual branch and HEAD commit.
2. State the requested goal and inspect the smallest relevant files before
   editing. Prefer minimal, local changes over large refactors or renames.
   Preserve the stage-by-stage layout `00_shell_code/`–`09_convert_export/`.
3. Do not change the `main` branch directly for substantive changes.
   Create a descriptive feature/fix/docs branch; propose a pull request.
   Do not force-push, delete branches, rewrite history, or overwrite
   existing files without verifying the current version and necessity.
4. Treat any local ZIP/snapshot as an **unverified input**, not as the
   authoritative GitHub state. Diff files before importing anything.
5. Never invent tests, results, benchmarks, support claims, or architecture
   coverage. Clearly distinguish **verified in this session** from
   **documented previously**, **not run**, and **blocked/skipped**.

## Architecture and compatibility invariants

- The UEEC container wire format is documented in
  `FORMAT_SPECIFICATION.md` (currently draft 0.1).
  The `_GM` magic, self-declared endianness, packed layouts, format-width
  selectors, node→global-section mappings, and `design` discriminants
  are compatibility-sensitive. Do not change their meanings silently.
- Multi-node containers may share physical sections. Maintain global
  section indexes vs node-local section indexes, and architecture-specific
  relocation correctness when editing parsers, generators, or loaders.
- Keep format metadata endianness distinct from target-code/data
  endianness. In particular, retain cross-endian PowerPC/SPARC coverage.
- Do not assume a target's calling convention or instruction-set mode is
  identical across architectures; the docs record ARM32 interworking and
  PowerPC64 ELFv1 function-descriptor pitfalls.
- RELOCATION `FLAT`/`BLOCK_4096`/`BLOCK_N` represent convertible
  designs in the current tools; PE-style relocation is a different,
  constrained model. Named EXPORT `COMPACT_GROUP`/`SIMPLE_FLAT` are
  convertible; `ORDINAL` has a different keyspace. Do not silently
  synthesize missing semantic information to force conversions.
- When changing an on-disk structure, update **readers, writers,
  converters, fixtures, spec, and relevant READMEs together**, and
  explicitly consider migration/backward compatibility.

## Input and execution safety

- Treat external UEEC binaries as untrusted. Check every length, count,
  offset, addition/multiplication, alignment, index, overlap, and mapped
  range before dereferencing or executing it.
- Exercise generated executable fixtures only in a controlled environment;
  never run arbitrary user-supplied machine code during routine validation.
- Avoid persistent writable-and-executable memory. Some existing test
  harnesses document W+X as an expedient; do not copy that pattern into
  production paths. Where possible, use writable loading/relocation
  followed by RX permissions, with appropriate failure handling.
- Avoid host-specific undefined behavior, unaligned accesses, overflow,
  and assumptions about bitfield layout without architecture validation.
- Never commit credentials, tokens, private keys, local environment files,
  or machine-specific absolute paths. A code change must not introduce
  unrelated network operations or access to external systems.

## Testing and reporting

Read the **stage's own** `README.md` and `Makefile` first. Suggested
targets already documented in the repository include:

| Scope | Existing documented target |
| --- | --- |
| `01_ueec_info/` | `make native` |
| `02_ueec_load/` | `make test-modes` |
| `04_convert/` | `make run-all-directions` |
| `09_convert_export/` | `make run-all-directions` |
| Per-stage broader matrix | `make all` where the stage Makefile defines it |

- Run the narrow relevant native test first, then relevant full tests
  where toolchains and QEMU are available. Do **not** assume these targets
  work in every environment or that `make all` means the same thing in
  every stage.
- For format/parser changes, add malformed, truncated, overflow,
  cross-endian, and round-trip regression cases as appropriate.
- Log exactly which command ran, OS/architecture/toolchain context,
  result, and any unavailable dependency. `Not run` is an acceptable,
  honest result; a missing emulator is **SKIP**, never **PASS**.
- Documentation-only work should be checked for internal consistency,
  valid relative links, and no unsupported validation claims.

## Change delivery and handoff

Before proposing a PR:

1. Summarize files changed and why; keep unrelated changes out.
2. State format/ABI/behavior compatibility impact (or `None — docs only`).
3. List tests actually run with explicit pass/fail/skip/not-run status.
4. Update `PROJECT_STATUS.md` when progress, limitations, decisions,
   evidence, or next actions changed. Do not rewrite history; append
   concise dated entries and update stale snapshots.
5. Include a clear reviewer-oriented PR description and do not merge
   without review of potentially destructive, security-sensitive, or
   format-breaking changes.

When a conversation or session is lost, recover in this order:
current GitHub HEAD → `PROJECT_STATUS.md` → `AGENTS.md` →
specification/design documents → affected source/Makefiles →
recent commits and PRs. Never infer GitHub state from chat memory alone.
