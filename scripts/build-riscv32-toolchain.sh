#!/usr/bin/env bash
# Build an RV32GC/ILP32D Linux GCC/glibc toolchain from pinned upstream sources.
# Intended for the isolated GitHub Actions runner. Does not touch UEEC sources.
set -euo pipefail

readonly UPSTREAM="https://github.com/riscv-collab/riscv-gnu-toolchain.git"
readonly UPSTREAM_REV="d118e5335a33d4dc77fdc64e5a5223931ab422a0"
readonly PREFIX="/opt/riscv32"
readonly SRC="${RUNNER_TEMP:-/tmp}/riscv-gnu-toolchain-src"

if [[ ! -d "$PREFIX" || ! -w "$PREFIX" ]]; then
  echo "ERROR: $PREFIX must exist and be writable" >&2
  exit 1
fi
if [[ -e "$SRC" ]]; then
  echo "ERROR: source directory already exists: $SRC" >&2
  exit 1
fi

echo "Pinned upstream $UPSTREAM_REV"
echo "Building only Linux toolchain dependencies: binutils, gcc, glibc"
df -h /

git clone --filter=blob:none --no-checkout "$UPSTREAM" "$SRC"
git -C "$SRC" fetch --depth=1 origin "$UPSTREAM_REV"
git -C "$SRC" checkout --detach "$UPSTREAM_REV"
[[ "$(git -C "$SRC" rev-parse HEAD)" == "$UPSTREAM_REV" ]]

# Intentionally avoid cloning unrelated submodules (LLVM, QEMU, GDB, Newlib,
# Spike, etc.). QEMU is supplied by Ubuntu for executing UEEC test binaries.
git -C "$SRC" submodule update --init --depth 1 --filter=blob:none binutils gcc glibc
for part in binutils gcc glibc; do
  test -f "$SRC/$part/configure"
  git -C "$SRC/$part" rev-parse HEAD
done
df -h /

cd "$SRC"
./configure --prefix="$PREFIX" --with-arch=rv32gc --with-abi=ilp32d \
  --with-languages=c --disable-gdb
make -j"$(nproc)" linux
df -h /

# Upstream uses the riscv64-unknown-linux-gnu name even when targeting RV32.
# The existing UEEC stage Makefiles expect the riscv32-* name at /opt/riscv32.
if [[ ! -x "$PREFIX/bin/riscv32-unknown-linux-gnu-gcc" ]]; then
  test -x "$PREFIX/bin/riscv64-unknown-linux-gnu-gcc" || {
    echo "ERROR: upstream Linux GCC was not produced" >&2
    exit 1
  }
  cat > "$PREFIX/bin/riscv32-unknown-linux-gnu-gcc" <<'WRAPPER'
#!/usr/bin/env bash
set -euo pipefail
exec /opt/riscv32/bin/riscv64-unknown-linux-gnu-gcc -march=rv32gc -mabi=ilp32d "$@"
WRAPPER
  chmod +x "$PREFIX/bin/riscv32-unknown-linux-gnu-gcc"
fi

if [[ ! -e "$PREFIX/bin/riscv32-unknown-linux-gnu-objcopy" ]]; then
  test -x "$PREFIX/bin/riscv64-unknown-linux-gnu-objcopy" || {
    echo "ERROR: objcopy was not produced" >&2
    exit 1
  }
  ln -s riscv64-unknown-linux-gnu-objcopy \
    "$PREFIX/bin/riscv32-unknown-linux-gnu-objcopy"
fi

"$PREFIX/bin/riscv32-unknown-linux-gnu-gcc" --version | head -n 1
echo "RV32 Linux toolchain source build completed"
