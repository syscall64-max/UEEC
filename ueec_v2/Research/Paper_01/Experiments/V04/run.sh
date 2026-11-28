#!/bin/sh
set -eu
cd "$(dirname "$0")"
sha256sum -c SOURCE.sha256
sed "s/^int main(/int original_resolver_main(/" resolve.c > resolve_bench.inc
cc -std=c11 -O2 -Wall -Wextra -Werror bench.c -o bench
python3 experiment.py "${1:-20}"
sha256sum -c SOURCE.sha256
