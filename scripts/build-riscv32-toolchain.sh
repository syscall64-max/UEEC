#!/usr/bin/env bash
# Reproducible experimental RV32 Linux GCC/glibc toolchain build.
# Intended for a disposable CI runner. NOT executed during documentation authoring.
set -euo pipefail

readonly UPSTREAM=https://github.com/riscv-collab/riscv-gnu-toolchain.git
readonly UPSTREAM_REV=d118e5335a33d4dc77fdc64e5a5223931ab422a0
readonly PREFIX=/opt/riscv32
readonly SRC="${RUNNER_TEMP:-/tmp}/riscv-gnu-toolchain-src"

if [[ ! -d "$PREFIX" || ! -w "$PREFIX" ]]; then
  echo "ERROR: $PREFIX must exist and be writable (CI provisions it first)" >&2
  exit 1
fi
if [[ -e "$SRC" ]]; then
  echo "ERROR: refusing to reuse a pre-existing source directory: $SRC" >&2
  exit 1
fi

echo "Building Linux RV32GC/ILP32D toolchain from $UPSTREAM @ $UPSTREAM_REV"
git clone --filter=blob:none --no-checkout "$UPSTREAM" "$SRC"
git -C "$SRC" fetch --depth=1 origin "$UPSTREAM_REV"
git -C "$SRC" checkout --detach "$UPSTREAM_REV"
git -C "$SRC" submodule update --init --recursive --depth 1

cd "$SRC"
./configure --prefix="$PREFIX" --with-arch=rv32gc --with-abi=ilp32d --with-languages=c --disable-gdb
make -j"$(nproc)" linux

# The upstream builder may name the compiler riscv64-* even for an RV32
# default target. Stage Makefiles expect an /opt/riscv32/bin/riscv32-* tool.
if [[ ! -x "$PREFIX/bin/riscv32-unknown-linux-gnu-gcc" ]]; then
  if [[ ! -x "$PREFIX/bin/riscv64-unknown-linux-gnu-gcc" ]]; then
    echo "ERROR: no Linux GCC produced by upstream source build" >&2
    exit 1
  fi
  cat > "$PREFIX/bin/riscv32-unknown-linux-gnu-gcc" <<'WRAPPER'
#!/usr/bin/env bash
exec /opt/riscv32/bin/riscv64-unknown-linux-gnu-gcc -march=rv32gc -mabi=ilp32d "$@"
WRAPPER
  chmod +x "$PREFIX/bin/riscv32-unknown-linux-gnu-gcc"
fi

if [[ ! -e "$PREFIX/bin/riscv32-unknown-linux-gnu-objcopy" && -x "$PREFIX/bin/riscv64-unknown-linux-gnu-objcopy" ]]; then
  ln -s riscv64-unknown-linux-gnu-objcopy "$PREFIX/bin/riscv32-unknown-linux-gnu-objcopy"
fi

"$PREFIX/bin/riscv32-unknown-linux-gnu-gcc" --version | head -n 1
echo "Linux RV32 compiler available: $PREFIX/bin/riscv32-unknown-linux-gnu-gcc"
