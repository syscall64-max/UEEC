#!/bin/sh
set -eu
cd "$(dirname "$0")"
CC=${CC:-cc}
"$CC" -std=c11 -Wall -Wextra -Werror -pedantic measure.c -o measure
./measure
