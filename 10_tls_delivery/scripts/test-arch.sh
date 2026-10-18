#!/usr/bin/env bash
set -euo pipefail
arch=${1:?arch}; port=${2:-9443}; granule=${3:-4096}; cert=${4:-cert.pem}; policy=${5:-strict}
case "$arch" in
    arm32) machine=0;cc=arm-linux-gnueabihf-gcc;emu=qemu-arm;target=arm32;;
    arm64) machine=1;cc=aarch64-linux-gnu-gcc;emu=qemu-aarch64;target=arm64;;
    loongarch64) machine=2;cc=loongarch64-linux-gnu-gcc-14;emu=qemu-loongarch64;target=loongarch64;;
    powerpc32) machine=3;cc=powerpc-linux-gnu-gcc;emu=qemu-ppc;target=powerpc32;;
    powerpc64) machine=4;cc=powerpc64-linux-gnu-gcc;emu=qemu-ppc64;target=powerpc64;;
    riscv32) machine=5;cc=/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc;emu=qemu-riscv32;target=riscv32;;
    riscv64) machine=6;cc=riscv64-linux-gnu-gcc;emu=qemu-riscv64;target=riscv64;;
    sparc32) machine=7;cc=sparc64-linux-gnu-gcc;emu=qemu-sparc32plus;target=sparc32;;
    sparc64) machine=8;cc=sparc64-linux-gnu-gcc;emu=qemu-sparc64;target=sparc64;;
    x86) machine=9;cc=i686-linux-gnu-gcc;emu=qemu-i386;target=x86;;
    x86_64) machine=10;cc=gcc;emu='';target=x86_64;;
    *) echo "Unknown architecture: $arch" >&2;exit 2;;
esac
miss=()
command -v "$cc" >/dev/null || miss+=("$cc")
[[ -z "$emu" ]] || command -v "$emu" >/dev/null || miss+=("$emu")
if ((${#miss[@]})); then
    if [[ "$policy" == skip-missing ]]; then
        echo "SKIP $arch: missing ${miss[*]}";exit 0
    else
        echo "FAIL $arch: missing ${miss[*]}" >&2;exit 1
    fi
fi
make -s -C reference/02_ueec_load "build-$target"
mkdir -p build/results
loader="reference/02_ueec_load/build/ueec-load-$target"
for mode in compact granulated; do
    output="build/results/${arch}_${mode}.ueec"
    rm -f -- "$output"
    ./build/ueec-net client --port "$port" --ca "$cert" --machine "$machine" --system 2 \
        --mode "$mode" --granule "$granule" --output "$output"
    if [[ -z "$emu" ]];then runner=("$loader");else runner=("$emu" "$loader");fi
    "${runner[@]}" --node exit --no-run "$output"
    "${runner[@]}" --node exit "$output"
    printf 'PASS arch=%s mode=%s bytes=%s\n' "$arch" "$mode" "$(stat -c %s "$output")"
done
