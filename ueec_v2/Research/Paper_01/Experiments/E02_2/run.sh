#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -m) == x86_64 && $(uname -s) == Linux ]]
mkdir -p build fixtures
as --64 linux.S -o build/linux.o
objcopy -O binary -j .text build/linux.o fixtures/linux-code.bin
ld -e _start -o fixtures/exit_linked.elf build/linux.o
ld -s -e _start -o fixtures/exit_stripped.elf build/linux.o
python3 compact.py
python3 generate.py
cc -O2 -Wall -Wextra reference/ueec_load.c -o build/ueec-load
for f in fixtures/*.elf; do
  "$f"; printf '%s exit=0 PASS\n' "$f"
  readelf -W -h -l -S "$f" > "${f%.elf}.inspection.txt"
done
for f in fixtures/*.ueec; do
 for mode in monolithic scattered hybrid; do
  build/ueec-load --load-mode "$mode" "$f" >/dev/null
  printf '%s %s exit=0 PASS\n' "$f" "$mode"
 done
done
python3 measure.py
