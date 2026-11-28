#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p fixtures
clang -target x86_64-pc-windows-msvc -O2 -c sample.c -o "$tmp/w64.obj"
lld-link /entry:main /subsystem:console /nodefaultlib /out:fixtures/sample64.exe "$tmp/w64.obj"
clang -target i686-pc-windows-msvc -O2 -c sample.c -o "$tmp/w32.obj"
lld-link /entry:main /subsystem:console /nodefaultlib /safeseh:no /out:fixtures/sample32.exe "$tmp/w32.obj"
clang -target x86_64-linux-gnu -O2 -c sample.c -o "$tmp/l.o"
ld.lld -e main -o fixtures/sample.elf "$tmp/l.o"
clang -target x86_64-apple-macos11 -O2 -c sample.c -o fixtures/sample.macho.o
