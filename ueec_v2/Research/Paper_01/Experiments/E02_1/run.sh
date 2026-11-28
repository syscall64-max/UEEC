#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p fixtures build
clang -target x86_64-linux-gnu -c linux.S -o build/linux.o
objcopy -O binary -j .text build/linux.o fixtures/linux-code.bin
ld -e _start -o fixtures/exit.elf build/linux.o
clang -target x86_64-pc-windows-msvc -c windows.S -o build/windows.obj
lld-link /entry:start /subsystem:console /nodefaultlib /fixed /out:fixtures/exit.exe build/windows.obj
python3 generate.py
cc -O2 -Wall -Wextra reference/ueec_load.c -o build/ueec-load
fixtures/exit.elf
printf 'ELF exit=0 PASS\n'
for f in fixtures/*.ueec; do
 for mode in monolithic scattered hybrid; do
  build/ueec-load --load-mode "$mode" "$f" > /dev/null
  printf '%s %s exit=0 PASS\n' "$f" "$mode"
 done
done
python3 measure.py
