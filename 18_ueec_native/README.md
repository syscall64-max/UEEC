# UEEC load-time compilation experiment

This archive assigns stage 18 to the load-time compilation experiment. The benchmark is stage 17. The corresponding [GitHub source](https://github.com/syscall64-max/UEEC/tree/b81afffb4e6048c077745c51992636d684192f42/17_ueec_native) is under `17_ueec_native/` in PR #10.

This stage compiles a small UEEC VM program to machine code while loading it on Linux. The same three fixture files are used on all 12 CPU targets, including IBM Z (s390x). Each fixture exercises a forward call followed by a nested backward call. The inner function sets VM register r1. After both functions return, VM service 0 exits with the value in r1.

1. Map the compact UEEC file read-only and locate its VM CODE.
2. Allocate an anonymous writable mapping for the generated CODE and a second anonymous mapping for the temporary relocation table.
3. Translate the VM instructions to host instructions. Record each CALL site and its target as a VM byte offset. After laying out the native instructions, patch each call using the host's displacement encoding.
4. Synchronize the instruction cache as required and change the native CODE to read-execute. Unmap the source UEEC and the temporary relocation table. Before using the native entry address, check both former mapping addresses with Linux mincore.
5. Enter the generated code. SYSCALL 0 lowers to the Linux exit syscall for the target ABI. Compare the resulting process status with the stage-16 interpreter's result for that same file.

## Reproducing the runs

Use Linux with Python 3, a host C compiler and Make for the reference interpreter, Clang 19 and LLD 19 for the target builds, GNU SPARC binutils for the SPARC link, and QEMU user-mode executables for the non-host runs. The recorded run used Debian 13 on x86-64 with QEMU 10.0.13 and Clang 19.1.7.

Run from the parent of the two stage directories:

```sh
python3 18_ueec_native/run_matrix.py --clang clang-19 --lld ld.lld-19
```

Use repeated `--arch` options to select targets. For a native-only run:

```sh
python3 18_ueec_native/run_matrix.py --clang clang-19 --lld ld.lld-19 --arch x86-64
```

`--qemu-dir` and `--sparc-ld` accept alternate tool locations. The matrix writes the fixtures and builds to `18_ueec_native/build` by default. It verifies the three process statuses (0, 42, 255), the before-entry mapping check, file byte equality, and rejection of a CALL into an operand and a truncated section. See `results.txt` for the recorded run.

## Scope

The experiment reads the existing compact little-endian SH16 container with one VM node and a single CODE section. It implements CALL FORMAT16, RET FORMAT16 with zero cleanup, MOV FORMAT16 into r1 for status values 0–255, and SYSCALL 0. The source CALL operand already identifies a VM-local target. Native relocation entries are generated from those operands in memory. This experiment does not consume an on-disk RELOCATION section. The interpreter and fixture generator come from the stage-16 snapshot at `74a282982129ec2cc199d5db6756f5fbba970a77`.

A later lazy-compilation experiment can give each block a bytecode start offset and a byte length. A transfer can compile a block on first entry and use its existing native entry address on subsequent visits. A loop back edge can use the compiled block, and only the selected branch of a conditional needs to be compiled. The VM must retain access to source blocks that are still eligible for a first entry. A persistent cache would need a key for the input bytecode and target ISA/ABI and a representation of outstanding relocations so that an absolute address from one process is not reused in another. These extensions are design directions.
