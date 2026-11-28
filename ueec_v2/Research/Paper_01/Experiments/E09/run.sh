#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
python3 test.py | tee test.log
