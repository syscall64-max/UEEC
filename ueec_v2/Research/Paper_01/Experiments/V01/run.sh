#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
sha256sum -c SOURCE_SHA256SUMS > /dev/null
mkdir -p results
ulimit -c 0
mode=${1:-full}
case "$mode" in
  native)
    (
      cd source/Import_Export/13.6_full_integration
      mkdir -p build/x86_64
      gcc -std=c11 -O2 -Wall -Wextra -Werror resolve.c -static -o build/x86_64/resolve
      python3 test.py x86_64 build/x86_64/resolve ''
      gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie resolve.c -o build/x86_64/resolve-san
      python3 test.py x86_64 build/x86_64/resolve-san ''
    ) 2>&1 | tee results/native.log
    ;;
  full)
    for stage in ISA_Dispatch Relocation Import_Export; do
      make -C "source/$stage" test 2>&1 | tee "results/$stage-test.log"
      make -C "source/$stage" sanitize 2>&1 | tee "results/$stage-sanitize.log"
    done
    ;;
  *) echo 'Usage: ./run.sh [full|native]' >&2; exit 2;;
esac
sha256sum -c SOURCE_SHA256SUMS > /dev/null
echo 'SOURCE INTEGRITY: PASS'
