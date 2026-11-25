# POWER8 required-profile correction — 2026-11-07

The user's new run reached the POWER5+ negative CPU-model case after the main
POWER8 tests. Its independent C/ABI probe succeeded but the complete loader
still segfaulted. The probe was therefore insufficient to establish complete
loader compatibility on that legacy model. The exact POWER5+ fault is not
claimed to be diagnosed or fixed by this change.

POWER5+ is now unconditionally absent from mandatory PowerPC64 CPU-model
cases. The supported test profile is POWER8. The three capability-mask
selection/rejection/fallback cases remain, as do scalar and AltiVec execution,
metadata/endian/width tests and malformed-input rejection: 33 checks total.

Fresh validation in this workspace:

- PowerPC64 POWER8, Debian GCC 14.2 / libc 2.41 / QEMU 10.0.13: 33 checks PASS.
- Both scalar and AltiVec CODE execute their guard-page and arithmetic checks.
- Repeated in a path containing spaces and parentheses: 33 checks PASS.
- Report confirms cpu=power8, negative_test_mode=capability-mask and
  abi_preflight=null: POWER5+ is not attempted at all.
- Native x86-64: 35 checks PASS, scalar/SSE2/AVX2/AVX-512F executed.

The full 12-target matrix was not rebuilt again for this one-row policy change.
The earlier full-matrix evidence below is historical; it is not a claim of
POWER5+ support. All other targets and runtime-profile changes are retained.
Evidence for the new targeted runs is in validation/20261107/.

---

# Validation — 2026-11-06

All 12 targets built and completed their QEMU suites. No target was skipped.
The native x86-64 suite additionally executed AVX-512F on AMD EPYC 9V74.
This is emulator validation for the other architectures, not testing on their physical hardware.

- 457 CLI/selection/format checks passed in the current modern-runtime matrix; 466 in the Debian-runtime matrix. Counts vary with valid older-CPU coverage.
- All 29 distinct architecture/technology CODE payloads were executed.
- Every execution tests 256 alignment/length cases, 32 guard-page cases, NULL/zero input, tails and uint32 overflow.
- One 6049-byte container: 29 nodes, 29 CODE, 29 METADATA, one HEADER.
- All outer byte orders and 16/32/64-bit METADATA widths executed on every target.
- Strict technology selection, missing/incompatible requests, fallback, priority changes, reordered nodes, unknown required bits and malformed metadata passed.

| Target | Backend | CLI/format checks | Executed CODE |
| --- | --- | ---: | --- |
| arm32 | QEMU | 36 | neon, scalar |
| arm64 | QEMU | 37 | neon, scalar, sve |
| loongarch64 | QEMU | 37 | lasx, lsx, scalar |
| powerpc32 | QEMU | 36 | altivec, scalar |
| powerpc64 | QEMU | 33 | altivec, scalar |
| riscv32 | QEMU | 36 | rvv, scalar |
| riscv64 | QEMU | 33 | rvv, scalar |
| s390x | QEMU | 33 | scalar, vx |
| sparc32 | QEMU | 33 | scalar, vis |
| sparc64 | QEMU | 33 | scalar, vis |
| x86 | QEMU | 37 | avx2, scalar, sse2 |
| x86_64 | QEMU | 38 | avx2, scalar, sse2 |
| x86_64 | native | 35 | avx2, avx512f, scalar, sse2 |

## Build environment

- Debian 13 (trixie), x86-64 host.
- Current modern-runtime matrix: Ubuntu QEMU user 10.2.1 (1:10.2.1+ds-1ubuntu3.3).
- Debian-runtime regression matrix: QEMU user 10.0.13. CPU models are recorded in `validation/results/`.
- Debian GCC 14.2 and matching cross binutils 2.44 for most loaders.
- Current PowerPC64: Bootlin GCC 15.3 with its modern POWER8-targeted libc.
- Current RISC-V64 and S390X: linked against Ubuntu 26.04 libc 2.43-2ubuntu2cross1, using GCC 14.2 to compile the loader itself.
- RISC-V32: Bootlin riscv32-ilp32d musl stable-2026.08-1, GCC 15.3, binutils 2.45.1.
- Native x86-64 host supports AVX-512F; QEMU max in this version exposes AVX2 and rejects AVX-512F as expected.
- All loaders compile with C11, -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror.
- Cross loaders link statically; native loader links to the host libc.

## Payload evidence

Each payload was linked at entry zero with no separate DATA/constant section.
The build checks the indicated instruction in the disassembly, then extracts raw .text.
SHA-256 values for the supplied bytes are in the corresponding `payloads/*/manifest.json`.

| Target | Technology | CODE bytes | Instruction evidence |
| --- | --- | ---: | --- |
| arm32 | scalar | 52 | `scalar integer loop` |
| arm32 | neon | 200 | `\bvadd\.i32\b` |
| arm64 | scalar | 48 | `scalar integer loop` |
| arm64 | neon | 168 | `\badd\s+v\d+\.4s` |
| arm64 | sve | 68 | `\buaddv\b` |
| loongarch64 | scalar | 68 | `scalar integer loop` |
| loongarch64 | lsx | 228 | `\bvadd\.w\b` |
| loongarch64 | lasx | 268 | `\bxvadd\.w\b` |
| powerpc32 | scalar | 152 | `scalar integer loop` |
| powerpc32 | altivec | 464 | `\bvadduwm\b` |
| powerpc64 | scalar | 184 | `scalar integer loop` |
| powerpc64 | altivec | 440 | `\bvadduwm\b` |
| riscv32 | scalar | 28 | `scalar integer loop` |
| riscv32 | rvv | 48 | `\bvredsum\.vs\b` |
| riscv64 | scalar | 28 | `scalar integer loop` |
| riscv64 | rvv | 48 | `\bvredsum\.vs\b` |
| s390x | scalar | 128 | `scalar integer loop` |
| s390x | vx | 448 | `\bvaf\b` |
| sparc32 | scalar | 52 | `scalar integer loop` |
| sparc32 | vis | 192 | `\bfpadd32\b` |
| sparc64 | scalar | 44 | `scalar integer loop` |
| sparc64 | vis | 192 | `\bfpadd32\b` |
| x86 | scalar | 45 | `scalar integer loop` |
| x86 | sse2 | 184 | `\bpaddd\b` |
| x86 | avx2 | 233 | `\bvpaddd\b.*%ymm` |
| x86_64 | scalar | 21 | `scalar integer loop` |
| x86_64 | sse2 | 82 | `\bpaddd\b` |
| x86_64 | avx2 | 85 | `\bvpaddd\b.*%ymm` |
| x86_64 | avx512f | 93 | `%zmm` |

## Scope

The example runs a leaf library function with a fixed native ABI. It is not a new general-purpose executable linker.
Feature masks are demo namespaces, not a finalized UEEC feature registry.
Priority is author-supplied preference; these tests do not benchmark which technology is fastest.
The Linux/toolchain ABI has a baseline of its own. A scalar payload does not imply support for every historical CPU.
SPARC negative tests remove features with --limit-isa; disabling VIS in the tested Debian libc environment prevents libc startup.
The LoongArch negative CPU model keeps LSX and removes LASX.
RISC-V uses a child-process vsetvli probe only when auxv advertises V but V_GET_CONTROL is unavailable.
No thread enablement policy is changed. A reported OFF state always prevents RVV selection.

## PowerPC64 GCC 15 regression fix

The user reported a first-call SIGSEGV with GCC 15.2 and QEMU 10.2.1, for
both scalar and AltiVec. Reproduced locally with Bootlin GCC 15.3 / glibc,
QEMU 10.0.13, power8. The original binary branched through an uninitialized
ELFv1 descriptor. Its disassembly contained descriptor loads but no stores
initializing the three descriptor words.

The C construction hid those ABI-level descriptor reads from dead-store
analysis. Added an empty GNU asm with a descriptor memory input and memory
clobber. The compiler must now materialize the descriptor before use.
No executable instruction or ISA requirement is added.

- Original GCC 15.3 build: scalar and AltiVec both SIGSEGV at first call.
- Fixed GCC 15.3: both variants PASS at -O0, -O2, -O3, -Os (8 execution runs).
- Fixed GCC 15.3 -O2 -flto: 33 PowerPC64 dispatch/format checks PASS, including
  execution of scalar and AltiVec and all metadata endian/width combinations.
- GCC 14.2 matrix: all 12 QEMU target suites plus native x86-64 rerun PASS;
  466 dispatch/format checks, all 29 distinct payloads covered.

The GCC 15 Bootlin toolchain's libc targets POWER8; its positive suite uses
--cpu power8. Older-CPU negative checks remain covered by the Debian GCC 14
matrix. This is not a claim that the user's exact GCC 15.2/QEMU 10.2.1 pair
has been run locally.

Evidence: validation/powerpc64-gcc15-*.log and
validation/powerpc64-gcc15-lto-suite.json.

## PowerPC64 negative CPU model portability fix

The fixed loader passed its POWER8 cases in the user's environment but the
additional POWER5+ negative case failed before producing selection output.
This older-CPU case had an invalid portability assumption: a loader linked
with a modern static libc is not guaranteed to start on POWER5+, regardless
of which UEEC payload is selected or which -mcpu flag compiles loader.c.

Removed POWER5+ from mandatory CPU-model tests. The existing three reduced-mask
cases remain: automatic scalar execution, strict AltiVec rejection, and
requested-AltiVec fallback to scalar. These are capability-mask tests and are
reported as such; they do not claim CPU-model coverage on POWER5+.

An independent puts-only program, linked with the same Bootlin GCC 15.3
Power8-targeted libc, passes on POWER8 and fails on POWER5+ (SIGILL locally).
It contains no UEEC code. The user's reported failure is SIGSEGV; the exact
Ubuntu GCC 15.2/libc/QEMU 10.2.1 combination has not been reproduced locally.

Validation after this change:

- Full Debian GCC 14 matrix: 12 QEMU targets + native suite PASS, 463 checks.
- Full matrix with PowerPC64 rebuilt using Bootlin GCC 15.3 and its modern libc:
  all 12 targets + native suite PASS, 463 checks, all 29 payloads covered.
- No custom --cpu override is needed to run the GCC 15 PowerPC64 suite.
- The PowerPC64 target completes 33 checks; both scalar and AltiVec execute.
- Per-target JSON reports now include negative_test_mode to distinguish
  capability masking from real CPU-model feature removal.

Evidence: validation/test-all-gcc15-powerpc64.log and
validation/powerpc64-libc-probe.c / .log.

## RVA23 and general libc-baseline preflight

Reproduced the reported RISC-V64 SIGILL with the exact Ubuntu QEMU package
10.2.1+ds-1ubuntu3.3 and Ubuntu RISC-V libc 2.43-2ubuntu2cross1.
An independent C program linked against that libc fails with
rv64,v=true,vlen=128,elen=64 and succeeds with max,vlen=128,elen=64.
The default RISC-V64 test CPU now uses max,vlen=128,elen=64.

Each build now also produces abi-probe-ARCH from abi_probe.c, using the same
compiler, baseline flags and link mode as its loader. It contains ordinary
libc calls and no UEEC parser, feature-detection code or raw CODE call.
An older-CPU negative case is eligible only after that probe succeeds.
When the independent probe faults, its exact result is recorded as
unavailable-libc-cpu-baseline, not as a successful hardware-negative test.
All three capability-mask selection/rejection/fallback tests still run.
A UEEC failure after a successful probe remains fatal to the suite.
Unknown model arguments and missing programs remain errors.

Modern-runtime full matrix (QEMU 10.2.1):

- All 12 targets plus native x86-64 PASS; 457 CLI/format checks.
- All 29 distinct CODE variants executed.
- PowerPC64 GCC 15.3: 33 checks, scalar and AltiVec executed; independent
  libc probe cannot run on POWER5+.
- RISC-V64 Ubuntu libc 2.43: 33 checks, scalar and RVV executed; independent
  libc probe cannot run with V disabled.
- S390X Ubuntu libc 2.43: 33 checks, scalar and VX executed; independent
  libc probe faults with VX disabled (SIGFPE in this environment).
- Other targets use the earlier Debian cross libc, except RISC-V32's Bootlin
  musl toolchain. This is not an all-Ubuntu-compiler validation claim.

The final Debian-runtime regression matrix completed 466 checks, including
the older CPU-model cases that its libc can execute. The CPU-model and
capability-mask coverage are distinguished in each results JSON.

Evidence: validation/test-all-ubuntu-runtime-qemu102.log,
validation/test-all-debian-runtime.log, validation/riscv64-ubuntu-libc-models.log,
and validation/results/*.json. No emulators, compilers or libc packages are
bundled in the archive.

Ubuntu platform requirements:
https://ubuntu.com/project/docs/how-ubuntu-is-made/concepts/supported-architectures/
https://documentation.ubuntu.com/release-notes/26.04/summary-for-lts-users/
