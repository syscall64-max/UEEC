# UEEC — Initial security review and threat model

Date: 2026-10-13. **Preliminary, documentation-based audit only**; not a completed line-by-line security audit or a penetration test.

## Trust boundary

UEEC file input -> parser -> node and section selection -> mappings and relocations -> executable memory -> control transfer. Even a structurally valid container may carry arbitrary machine code. Current loaders are experimental and should be used only with trusted fixtures in an isolated environment, not as a safe execution sandbox.

## Prioritized review checklist

| Priority | Surface | Risk / evidence needed |
| --- | --- | --- |
| Critical | Execution | Validated UEEC files can still execute arbitrary code; never execute untrusted UEEC files on a normal host |
| High | File parser | Check all file-size/count/offset arithmetic for overflow, underflow, out-of-bounds access and overlapping regions; inspect each stage's duplicated parsing logic |
| High | Memory permissions | `03_reloc/README.md` explicitly documents an RWX test-harness shortcut; production implementations should transition RW -> RX (W^X) and check all `mprotect` failures |
| High | Relocations | Confirm target/source node-local indexes, globally shared sections, patch widths, alignment, section bounds, and addends before writing |
| High | Format decoding | Reject malformed header, inconsistent endian tags, unknown/unsupported design, oversized allocation and ambiguous entry point |
| Medium | Portability | Test big-endian formats, ARM32 instruction-state assumptions, and PowerPC64 ELFv1 descriptors |
| Medium | CI supply chain | Prefer minimal permissions, pinned third-party actions, no secrets in PR tests; inspect dependency provenance |

## Evidence and limitations

- `02_ueec_load/README.md` documents `mprotect` policies for its loader. Those claims have not been independently revalidated in this audit.
- `03_reloc/README.md` explicitly acknowledges a writable/executable memory-mapping simplification.
- The format is pre-1.0; readers and converters need hardening before untrusted input handling.
- No fuzzing, sanitizers, exploit reproduction, or full static analysis were run as part of this report.
- This file identifies *review areas*, not confirmed vulnerabilities beyond the explicitly documented RWX shortcut.

## Next security gates

- Obtain and retain reproducible sanitizer and malformed-file corpus results.
- Trace all parser range checks, especially multiplication and addition of attacker-controlled counts/offsets.
- Review mappings and relocation application on every stage that implements loading.
- Add W^X transition tests to a branch before considering a production loader.
- Do not describe UEEC execution as sandboxed or safe for untrusted files without a separately justified isolation architecture.
