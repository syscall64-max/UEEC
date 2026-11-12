#!/usr/bin/env python3
"""Build native/cross loaders or raw CODE. No toolchain download or installation."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from architectures import ARCHES, native_arch

COMMON = shlex.split("-std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror")
RAW = shlex.split("-ffreestanding -fno-builtin -fno-tree-vectorize -fno-tree-slp-vectorize "
                  "-fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables "
                  "-fno-pic -fno-pie -fno-ident -ffunction-sections")

def compiler(arch):
    spec = ARCHES[arch]
    override = os.environ.get("CC_" + arch.upper())
    if override:
        return shlex.split(override)
    if arch == "riscv32" and Path("/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc").is_file():
        return ["/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc"]
    if arch == native_arch():
        return shlex.split(os.environ.get("CC", "gcc"))
    if arch == "loongarch64" and not shutil.which(spec.cc):
        return ["loongarch64-linux-gnu-gcc"]
    return [spec.cc]

def run(args, **kw):
    return subprocess.run([str(x) for x in args], check=True, **kw)

def output(args):
    return subprocess.check_output([str(x) for x in args], text=True)

def binutil(cc, name):
    # GCC returns its matching cross binutil (also works for /opt toolchains).
    return output(cc + ["-print-prog-name=" + name]).strip()

def build_loader(arch):
    spec, cc = ARCHES[arch], compiler(arch)
    dest = ROOT / "build" / ("ueec-loader-" + arch)
    dest.parent.mkdir(exist_ok=True)
    args = cc + COMMON + ["-D_DEFAULT_SOURCE", "-D_FILE_OFFSET_BITS=64"]
    args += shlex.split(spec.baseline)
    if arch != native_arch() or os.environ.get("STATIC") == "1":
        args += ["-static"]
    run(args + [ROOT / "loader.c", "-o", dest])
    run(args + [ROOT / "abi_probe.c", "-o", ROOT / "build" / ("abi-probe-" + arch)])
    if arch == native_arch():
        shutil.copyfile(dest, ROOT / "build/ueec-loader")
        (ROOT / "build/ueec-loader").chmod(0o755)
    print(f"{arch}: loader build PASS", flush=True)

def build_payloads(arch):
    spec, cc = ARCHES[arch], compiler(arch)
    work = ROOT / "build" / "objects" / arch
    dest = ROOT / "payloads" / arch
    work.mkdir(parents=True, exist_ok=True)
    dest.mkdir(parents=True, exist_ok=True)
    ld, objcopy, objdump, nm = (binutil(cc, x) for x in ("ld", "objcopy", "objdump", "nm"))
    reports = []
    for idx, variant in enumerate(spec.variants):
        obj = work / (variant.name + ".o")
        elf = work / (variant.name + ".elf")
        raw = dest / (variant.name + ".bin")
        flags = COMMON + RAW + shlex.split(spec.baseline + " " + variant.flags)
        if arch == "x86_64":
            run(cc + ["-c", "-DUEEC_VARIANT=" + str(idx), ROOT / "sum_x86_64.S", "-o", obj])
        else:
            run(cc + flags + ["-c", ROOT / "sum.c", "-o", obj])
        # Link ONLY this leaf; no CRT, libc, GOT, TOC data or separate constants.
        args = [ld]
        if arch == "sparc32": args += ["-m", "elf32_sparc"]
        if arch == "riscv32": args += ["-m", "elf32lriscv"]
        if arch.startswith("riscv"): args += ["--no-relax"]
        entry = ".ueec_sum" if arch == "powerpc64" else "ueec_sum"
        run(args + ["-T", ROOT / "payload.ld", "-e", entry, obj, "-o", elf])
        syms = output([nm, "-n", elf])
        if not re.search(r"^0+ [Tt] " + re.escape(entry) + r"$", syms, re.M):
            raise RuntimeError(f"{arch}/{variant.name}: CODE entry must be zero: {syms}")
        if re.search(r"(?m)^\s+U[ \t]+\S+", syms):
            raise RuntimeError("unresolved payload symbols: " + syms)
        dis = output([objdump, "-d", elf])
        (work / (variant.name + ".dis")).write_text(dis)
        if variant.evidence and not re.search(variant.evidence, dis):
            raise RuntimeError(f"{arch}/{variant.name}: missing instruction {variant.evidence}\n{dis}")
        # Reject runtime-address-dependent relocations before linking. Local PC
        # branches/literal references inside .text are resolved by the linker.
        reloc = output([binutil(cc, "readelf"), "-rW", obj])
        if re.search(r"R_(?:PPC(?:64)?_(?:ADDR|TOC|GOT)|SPARC_(?:HI22|LO10|32|64)\b|"
                     r"RISCV_(?:HI20|LO12|GOT)|AARCH64_ADR_GOT|390_(?:GOT|PLT)|"
                     r"386_(?:GOT|32)\b|X86_64_(?:32|64)\b)", reloc) and arch != "powerpc64":
            raise RuntimeError(f"{arch}/{variant.name}: address-dependent relocation\n{reloc}")
        # PPC64 ELFv1 .opd relocations are intentionally discarded. The raw
        # .ueec_sum code must not use TOC/GOT or call another function.
        text = dis.split("Disassembly of section .text:")[-1]
        if arch == "powerpc64" and re.search(r"\b(?:ld|addis|addi)\s+\w+,[-\w]*(?:\(r2\)|,r2,)", text):
            raise RuntimeError("payload depends on TOC: " + text)
        run([objcopy, "-O", "binary", "-j", ".text", elf, raw])
        import hashlib
        reports.append(dict(technology=variant.name, size=raw.stat().st_size,
                            sha256=hashlib.sha256(raw.read_bytes()).hexdigest(),
                            instruction=variant.evidence))
        print(f"{arch}/{variant.name}: CODE={raw.stat().st_size} entry=0 ISA check PASS", flush=True)
    (dest / "manifest.json").write_text(json.dumps(reports, indent=2) + "\n")

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=("loader", "payloads"))
    p.add_argument("--arch", default=native_arch(), choices=tuple(ARCHES) + ("all",))
    args = p.parse_args()
    for arch in ARCHES if args.arch == "all" else (args.arch,):
        (build_loader if args.action == "loader" else build_payloads)(arch)

if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, OSError, RuntimeError) as e:
        print("BUILD FAILED:", e, file=sys.stderr)
        sys.exit(1)
