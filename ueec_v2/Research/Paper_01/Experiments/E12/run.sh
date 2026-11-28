#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]]
CC=${CC:-cc}
"$CC" -O2 -Wall -Wextra loading/bench.c -o loading/bench
"$CC" -D_DEFAULT_SOURCE -O2 -Wall -Wextra relocation/bench.c -o relocation/bench
"$CC" -O2 -Wall -Wextra nodes/bench.c -o nodes/bench
"$CC" -O2 -Wall -Wextra loading/launch.c -o loading/launch
python3 benchmark.py "$@" | tee test.log
