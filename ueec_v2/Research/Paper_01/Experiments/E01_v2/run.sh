#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 analyze_headers.py fixtures/* > results.csv
for cc in gcc clang; do
 "$cc" -std=c11 -Wall -Wextra -Werror measure.c -o /tmp/ueec-e01-$$
 /tmp/ueec-e01-$$ > "$cc.csv"
 rm /tmp/ueec-e01-$$
done
