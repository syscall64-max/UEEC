#!/bin/sh
set -eu
cd "$(dirname "$0")"
if python3 test.py > test.log 2>&1; then
    cat test.log
else
    cat test.log >&2
    exit 1
fi
