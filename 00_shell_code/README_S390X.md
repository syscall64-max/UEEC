# IBM Z / Linux s390x — 12th raw-code architecture (stage 00)

This extension follows the exact `00_shell_code` structure from the supplied
archive. `s390x.asm` emits a raw instruction stream (not ELF). The existing
`loader.c` is cross-compiled to a static s390x ELF executable which maps
`s390x.bin` with executable permissions and jumps to its first byte. QEMU
runs that loader and executes the raw s390x instructions.

## Instruction boundaries

| Bytes | Instruction | Meaning |
| --- | --- | --- |
| `A7 29 00 00` | `lghi %r2,0` | exit status = 0 |
| `0A 01` | `svc 1` | Linux syscall 1 (`exit`) |
| `07 07` | `nopr %r7` / `bcr 0,7` | padding NOP, unreachable after `exit` |

On Linux/s390x, syscall numbers below 256 may be encoded directly in `svc N`.
For `svc 0`, the syscall number is instead passed in register `r1`.

## Dependencies (Ubuntu)

```sh
sudo apt update
sudo apt install -y nasm gcc-s390x-linux-gnu binutils-s390x-linux-gnu qemu-user
```

## Test only the new architecture

```sh
cd 00_shell_code
make test_s390x
echo "Exit code: $?"       # expected 0
xxd -g 1 s390x.bin          # a7 29 00 00 0a 01 07 07
```

The stage's existing 11 `test_*` targets remain unchanged. `make all` now
includes this 12th test (and still requires the original 11 toolchains).

**Scope:** passing this test demonstrates raw s390x machine code execution
through the stage-00 loader in QEMU. It does **not** yet establish that the
UEEC node/section loader supports 12 architectures; that requires updating
`01_ueec_info`, `02_ueec_load` and relevant metadata/fixtures separately.
It also does not exercise z17-specific ISA extensions or IBM z17 hardware.
