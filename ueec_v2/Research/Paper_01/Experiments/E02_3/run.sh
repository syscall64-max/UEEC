#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]]
mkdir -p build fixtures
as --64 sum.S -o build/sum.o
objcopy -O binary -j .text build/sum.o build/code.bin
cc -O2 -Wall -Wextra reference/loader.c -o build/loader
python3 generate.py
python3 test.py | tee test.log
