#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
sha256sum -c SOURCE.sha256
[ "$(uname -m)" = x86_64 ] || { echo 'Requires Linux x86-64'; exit 1; }
cc -std=c11 -O2 -Wall -Wextra -Werror probe.c -o probe
python3 run.py
sha256sum -c SOURCE.sha256
