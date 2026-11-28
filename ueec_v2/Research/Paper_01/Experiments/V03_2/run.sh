#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Linux && $(uname -m) == x86_64 && $(getconf PAGESIZE) == 4096 ]]
sha256sum -c SOURCE.sha256
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror probe.c -o probe
python3 experiment.py | tee test.log
sha256sum -c SOURCE.sha256
