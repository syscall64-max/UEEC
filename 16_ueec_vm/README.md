# UEEC VM — two programs in the existing multi-node format

This stage connects the supplied single-image UEF VM interpreter to the
existing UEEC v1 `_GM` container. Add `16_ueec_vm/` alongside the existing
stages. It is self-contained; earlier project directories are not replaced.

## Build and run

Requires a Linux/POSIX host, GCC or Clang, Make and Python 3. No NASM is
required for this stage. Build outputs are generated under `build/`.

```sh
cd 16_ueec_vm
make
make list
make run

# Select by program name or zero-based node index:
./build/ueec-vm --node hello build/programs.ueec
./build/ueec-vm --node stack build/programs.ueec
./build/ueec-vm --node 0 build/programs.ueec
./build/ueec-vm --node 1 build/programs.ueec

# Show the unchanged UEEC container layout:
./build/ueec-info build/programs.ueec

# Three allocation policies (auto tries monolithic, then scattered):
./build/ueec-vm --node stack --load-mode monolithic build/programs.ueec
./build/ueec-vm --node stack --load-mode scattered build/programs.ueec
./build/ueec-vm --node stack --load-mode hybrid build/programs.ueec

# Optional instruction trace or loading without execution:
./build/ueec-vm --node stack --trace build/programs.ueec
./build/ueec-vm --node stack --no-run build/programs.ueec

make test
make sanitize
```

With no `--node`, the first compatible program node is selected. This is a
demonstration default, not automatic inference of which task a user wants.

## Existing format and machine namespace

No new file signature, node structure or section descriptor is introduced.
`ueec.h` is copied byte-for-byte from the supplied UEEC project's
`hdrs/ueec.h`. Existing constants already define:

| Node field | Value | Meaning |
|---|---:|---|
| `machine_type` | 2 | VM |
| `machine` | 0 | UEEC VM |
| `encode` | 0 | VM bytecode and memory cells are little endian |
| `program` / `user_mode` | 1 / 1 | Runnable user program |
| `library` / `kernel_mode` | 0 / 0 | Not a library/kernel node |
| `system_type` / `system` | 0 / 0 | No guest OS ABI; VM-provided services |

The host implementation is a Linux user-space interpreter. The node's
bare-metal/none classification means its bytecode does not invoke a guest
Linux ABI. It does not mean this host loader runs on bare metal.

`programs.ueec` has **2 nodes, 4 physical sections and 262 compact bytes**:

| Global section | Type | Meaningful bytes | Referenced by |
|---:|---|---:|---|
| 0 | HEADER | 84 | Both nodes, implicitly |
| 1 | CODE | 32 | `hello` |
| 2 | CODE | 104 | `stack` |
| 3 | INIT_DATA | 42 | Both nodes |

The second node's CODE is `examples/stack16.asm`, converted from the original
`final_stack.asm`: former FORMAT8 PUSH/POP/MOV operations now use FORMAT16.
The demonstrated values, call arguments and result are preserved. CODE grows
from 102 to 104 bytes because one two-operand MOV is wider. The old `0x6575`
UEF header and packed section descriptors are not embedded in the new file.
The active assembly example contains CODE only.

The shared INIT_DATA section starts with the 16-bit value 1234, followed by
the message `Hello from UEEC VM!` and a newline in 16-bit cells. `hello`
reads 1234 into r4 and prints the message. `stack` exercises nested calls
with mixed-width arguments and produces:

```text
RESULT r4=111 r5=222 r6=85 r7=333 r8=111 r9=122 r10=305419896 sp=0 bp=0 exit=0
```

Both references identify one physical file section. Each CLI invocation
loads one program into its own fresh execution context; this is not a
demonstration of simultaneously shared mutable memory between programs.

The file parser and allocation policies are derived from stage 02. They
build the same reduced runtime HEADER and section-address table. Monolithic
uses one mapping, scattered one per selected section (three here), and
hybrid a separate HEADER plus one data/code mapping (two here). Bytecode is
read-only and never marked host-executable. CODE and optional INIT_DATA
are passed to the interpreter after the input mapping and file are closed.

This stage accepts exactly one CODE and at most one INIT_DATA section per
selected VM node. Other selected section types are rejected explicitly.
RELOCATION, EXPORT/IMPORT, VM capability records and joint library loading
are not integrated into this VM loader yet. SH16/SH32/SH64 describe container
sizes, independently of instruction operand formats.

## VM instructions and memory model

The supplied instruction numbers and FORMAT16/32/64 encodings are retained.
FORMAT8 is removed from the active interpreter and example encoder:

| Command | Instruction |
|---:|---|
| 0 | SYSCALL |
| 1 | MOV index,index |
| 2 | MOV index,immediate |
| 3 | PUSH index |
| 4 | PUSH immediate |
| 5 | POP index |
| 6 | CALL signed-relative |
| 7 | RET immediate cleanup |

The 16-bit instruction header stores command in bits 0–10 and operand
format in bits 11–15. This provides 2048 possible operation codes; only 0–7
are implemented. Unknown operation codes fail validation.

| Operand format code | Width | Encoded bytes | Status |
|---:|---:|---:|---|
| 0 | — | — | Reserved for operands; former FORMAT8 rejected |
| 1 | 16 bits | 2 | Implemented |
| 2 | 32 bits | 4 | Implemented |
| 3 | 64 bits | 8 | Implemented |
| 4–31 | — | — | Reserved; 128-bit and wider formats are future work |

SYSCALL is the special fixed four-byte instruction with header format=0
and a 16-bit service number; it does not perform an 8-bit operation.
An index's top two bits select register, IDATA, stack or reserved heap space.
All other bits are the unsigned index. Instructions remain two-byte aligned.

VM memory uses 16-bit physical cells: FORMAT16 occupies one cell,
FORMAT32 two and FORMAT64 four.
CODE IP, CALL displacements, SP, BP and memory indexes use these block
units where specified in the original reference README. Register values
are explicitly `uint64_t`, preserving FORMAT64 independently of host pointer
width. Memory-cell reads/writes are explicitly little endian.

CALL is relative to the next instruction; stack arguments and cleanup
retain the original behavior. Call frames remain private metadata outside
the data stack. There are 16 registers, a 4096-byte stack and a frame-depth
limit of 512. HEAP is reserved and rejected. One execution context is used;
there is no guest scheduler, source-language compiler or source interpreter.

VM services:

* SYSCALL 0: `exit(r1)`, status 0–255. This is the original exit service,
  now range-checked for a portable process exit code.
* SYSCALL 1: `write_stdout(r1, r2)`, added for the second distinct program.
  `r1` is an INIT_DATA cell index; `r2` is the number of output bytes.
  Each byte is read from the low byte of a cell. This is not a native
  system call and accepts no host pointer.

The code scanner rejects malformed instructions and CALL targets inside
operands. Runtime checks reject invalid indexes, stack errors and unknown
services. Normal completion requires explicit exit. The default execution
budget is 100000 instructions; override with `--max-steps N`.

## Alternative container encodings

The same bytecode can be packaged using different existing descriptor widths,
metadata byte order or file padding:

```sh
python3 generate.py --format sh32 --endian big --output build/programs-be32.ueec
./build/ueec-vm --node stack build/programs-be32.ueec

python3 generate.py --format sh16 --granule 4096 --output build/programs-page.ueec
./build/ueec-vm --node hello build/programs-page.ueec
```

UEEC metadata endianness does not change the VM target's LE instruction
encoding. This stage rejects a node with `encode=1` rather than incorrectly
interpreting it. No extra VM ISA version field is added; the documented
instruction set is experimental and not a stabilized ABI.

This revision deliberately rejects old FORMAT8 bytecode; regenerate programs
for FORMAT16 or wider. Historical examples remain under `reference/` solely
for comparison, and their original C interpreter is not the active runtime.

## Future widths and SIMD

The existing header leaves room for a future FORMAT128. This revision does
not execute 128-bit operands: wider register/value representations, memory
operations, ABI rules and tests would also be required. Reserved fields
alone do not implement a wider machine.

The intended SIMD direction is one VM operation over several elements, with
a host-native vector backend when usable and a scalar-loop fallback. Both
would implement the same bytecode semantics. Element width, lane count,
signedness, overflow and bounds behavior need specification and equivalence
tests. A native backend must check usable host/OS features. SIMD instructions,
arithmetic/conditional branches and vector dispatch are not implemented here.

## Validation and provenance

Fresh checks on 16 November 2026, Linux x86-64:

* **162 checks PASS** with the normal binary.
* **162 checks PASS** with AddressSanitizer and UndefinedBehaviorSanitizer.
* Both programs across SH16/SH32/SH64, little/big-endian metadata, compact
  and 4096-byte granules, and all four allocation options: 96 executions.
* The unmodified earlier UEEC reader independently parses all 12 container
  encodings; its old target-name display is ignored for structural checks.
* The old UEF interpreter runs the same original stack and recursion fixtures.
  The current VM runs the converted stack example and preserves its result.
* All seven operand commands are exercised at 16, 32 and 64 bits, including
  a full-width 64-bit value above UINT32_MAX. Each command is also checked
  for rejection of reserved format=0. Former FORMAT8 code and the
  unimplemented 128-bit format are rejected.
* Name/index selection, load-only behavior, correct VM display, shared-section
  references and malformed container/bytecode rejection are checked.

The project author additionally reported **165 checks PASS** on an x86-64
host for `make test`, `make sanitize`, and a sanitizer run with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`. NASM adds three independent
fixture-encoding comparisons. The supplied terminal excerpt is preserved in
`evidence/user-validation-20261116.txt`; these author-host results are distinct
from the locally reproduced 162-check results above.

LeakSanitizer is disabled in the locally recorded sanitizer run because it fails under
the workspace's ptrace supervisor. On a normal host, enable it explicitly:

```sh
make sanitize ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
```

NASM is unavailable on the evaluation host. `asm_subset.py` is a strict
data/expression/label encoder for the historical and converted NASM fixtures;
it is not a general assembler or a new language compiler. If NASM is
installed, `make test` additionally compares its output byte-for-byte with
the fixture encoder.

No fresh cross-architecture execution matrix, physical MCU run or performance
benchmark is claimed here. The explicit integer/byte-order changes improve
portability, but their execution was verified on x86-64 only.

`evidence/` contains the recorded check reports, sample outputs and source
hashes. `reference/` retains the original stack fixture, recursion fixture,
VM source/README and earlier UEEC information reader. `examples/stack16.asm`
is the active converted program. `generate.py` builds
the example container; `vm_runtime.c` implements execution; `ueec_vm.c`
implements UEEC parsing, program selection and section loading.
