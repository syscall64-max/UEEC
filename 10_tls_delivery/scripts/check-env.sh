#!/usr/bin/env bash
set -u
for a in arm32:arm-linux-gnueabihf-gcc:qemu-arm arm64:aarch64-linux-gnu-gcc:qemu-aarch64 loongarch64:loongarch64-linux-gnu-gcc-14:qemu-loongarch64 powerpc32:powerpc-linux-gnu-gcc:qemu-ppc powerpc64:powerpc64-linux-gnu-gcc:qemu-ppc64 riscv32:/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc:qemu-riscv32 riscv64:riscv64-linux-gnu-gcc:qemu-riscv64 sparc32:sparc64-linux-gnu-gcc:qemu-sparc32plus sparc64:sparc64-linux-gnu-gcc:qemu-sparc64 x86:i686-linux-gnu-gcc:qemu-i386 x86_64:gcc:-; do
  IFS=: read -r arch cc qemu <<< "$a"
  missing=(); command -v "$cc" >/dev/null || missing+=("$cc")
  [[ "$qemu" == - ]] || command -v "$qemu" >/dev/null || missing+=("$qemu")
  if ((${#missing[@]}));then printf 'MISSING %-12s %s\n' "$arch" "${missing[*]}";else printf 'AVAILABLE %s\n' "$arch";fi
done
