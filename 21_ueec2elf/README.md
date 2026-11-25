# UEEC to native Linux ELF

`ueec2elf.py` is a standalone Python 3 utility. It reads the native CPU,
section positions and byte lengths from the UEEC header and creates an
executable ELF for that Linux target. The original machine code becomes
the ELF `.text` section, with every CODE byte preserved. The conversion
uses no VM compiler, assembler, linker or server. The output has an RX
load segment and a non-executable stack.

```sh
python3 21_ueec2elf/ueec2elf.py program.ueec program.elf
./program.elf
```

Run the ELF on a Linux CPU matching its target. A pre-existing output
path is rejected. Whether the input was compact or granulated has no
effect on the resulting ELF bytes.

The supported wire profile is the one used in stage 20. It has a
little-endian SH16 header, one standalone Linux CPU node and one
position-independent CODE section, with optional indexed x86-64 feature
metadata for SSE2 or AVX2. Sections carrying data, imports or on-disk
relocations require additional conversion rules. UEEC VM bytecode is
rejected. An x86-64 ELF with feature metadata starts in the small check
in `x86_features.S`. If the required features are available, control
continues into the verbatim `.text` payload. The check exits with 126
on an unsupported CPU. AVX2 also requires OS-enabled XMM/YMM state.

## Reproducing the full test

Use Linux x86-64 and the Stage 20 tools, plus GNU `readelf` and `objcopy`.
The test obtains both UEEC layouts from stage 20, independently reads the
ELF section table, verifies that the ELF CODE bytes were copied verbatim,
then executes both identical ELF files after the server has stopped.

```sh
python3 21_ueec2elf/run_stage21.py --isolate-network
python3 21_ueec2elf/run_stage21.py --isolate-network --status 173
```

The test checks 12 CPU targets, plus SSE2 and AVX2 variants on x86-64.
The recorded results use native x86-64 and QEMU user mode for the other
eleven targets. Each invocation saves UEEC and ELF files beneath a new
directory under `21_ueec2elf/build/`. See `results.txt`.
