#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]]
mkdir -p build
for tag in single multi; do ${CC:-cc} -O2 -Wall -Wextra reference/$tag.c -o build/$tag; done
python3 experiment.py | tee test.log
