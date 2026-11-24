"""Build and run the load-time compiler on the 11 original UEEC CPU targets."""
import argparse
from hashlib import sha256
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
VM = ROOT.parent / "16_ueec_vm"
sys.path.insert(0, str(VM))
from generate import container, instruction

TARGETS = {
    "x86-64": ("x86_64-linux-gnu", "x86_64", []),
    "x86": ("i386-linux-gnu", "i386", ["-march=i686"]),
    "ARM32": ("armv7-linux-gnueabihf", "arm", ["-marm"]),
    "AArch64": ("aarch64-linux-gnu", "aarch64", []),
    "LoongArch64": ("loongarch64-linux-gnu", "loongarch64", []),
    "PowerPC32": ("powerpc-linux-gnu", "ppc", []),
    "PowerPC64": ("powerpc64-linux-gnu", "ppc64", ["-mabi=elfv2"]),
    "RISC-V32": ("riscv32-linux-gnu", "riscv32", ["-march=rv32gc", "-mabi=ilp32"]),
    "RISC-V64": ("riscv64-linux-gnu", "riscv64", ["-march=rv64gc", "-mabi=lp64"]),
    "SPARC32": ("sparc-linux-gnu", "sparc", ["-mcpu=v8"]),
    "SPARC64": ("sparcv9-linux-gnu", "sparc64", []),
}
CLEAN = b"native CODE is RX; VM CODE and relocations unmapped\n"


def run(*args, **kwargs):
    return subprocess.run(args, cwd=ROOT, timeout=60, **kwargs)


def build_native(name, target, clang, lld, sparc_ld, out):
    triple, _, flags = target
    output = out / name
    parts = []
    for source in ("linux.S", "native.c"):
        obj = output.with_suffix("." + source[-1] + ".o")
        run(clang, "--target=" + triple, *flags, "-O2", "-ffreestanding", "-fno-pic",
            "-fno-stack-protector", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
            "-Wall", "-Wextra", "-Werror", "-c", source, "-o", str(obj), check=True)
        parts.append(str(obj))
    if name.startswith("SPARC"):
        linker = [sparc_ld, "-m", "elf32_sparc" if name == "SPARC32" else "elf64_sparc"]
    else:
        linker = [lld]
    run(*linker, "-static", "-e", "_start", "-o", str(output), *parts, check=True)
    return output


def guest_fixture(status):
    # main calls A (forward), A calls B (backward). B sets r1.
    # The links and offsets change in each native instruction set.
    code = b"".join((
        instruction(6, 1, 7),         # 0 -> 18: call A
        instruction(0, 0, 0),         # 4: exit(r1)
        instruction(2, 1, 1, status), # 8: B
        instruction(7, 1, 0),         # 14: return from B
        instruction(6, 1, -7),        # 18 -> 8: A calls B
        instruction(7, 1, 0),         # 22: return from A
    ))
    return container([("call", [1])], [(1, code)])


def native_run(command, path):
    with path.open("rb") as source:
        fd = source.fileno()
        length = os.fstat(fd).st_size
        return run(*command, str(length), str(fd), pass_fds=(fd,), capture_output=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--lld", default="ld.lld")
    parser.add_argument("--sparc-ld", default="sparc64-linux-gnu-ld")
    parser.add_argument("--qemu-dir", type=Path, default=Path("/usr/bin"))
    parser.add_argument("--out", type=Path, default=ROOT / "build")
    parser.add_argument("--arch", action="append", choices=TARGETS)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    run("make", "-C", str(VM), "build/ueec-vm", check=True)
    cases = {}
    for status in (0, 42, 255):
        blob = guest_fixture(status)
        path = out / f"call-{status}.ueec"
        path.write_bytes(blob)
        baseline = run(str(VM / "build/ueec-vm"), "--load-mode", "scattered", str(path), capture_output=True)
        if baseline.returncode != status or baseline.stderr or b"exit=" + str(status).encode() not in baseline.stdout:
            raise RuntimeError(f"VM baseline failed for {status}: {baseline.stdout!r} {baseline.stderr!r}")
        print(f"VM status {status} sha256 {sha256(blob).hexdigest()}", flush=True)
        cases[status] = (path, blob)

    # The first CALL's target now points into the following SYSCALL operand.
    bad_call = out / "bad-call.ueec"
    broken = bytearray(cases[0][1])
    broken[-24:-22] = b"\x01\x00"
    bad_call.write_bytes(broken)
    truncated = out / "truncated.ueec"
    truncated.write_bytes(cases[0][1][:-1])

    failed = []
    selected = args.arch or list(TARGETS)
    for name in selected:
        try:
            target = TARGETS[name]
            output = build_native(name, target, args.clang, args.lld, args.sparc_ld, out)
            native = name == "x86-64" and platform.machine() == "x86_64"
            command = [str(output)]
            if not native:
                emulator = str(args.qemu_dir / ("qemu-" + target[1]))
                if not Path(emulator).exists():
                    emulator = shutil.which("qemu-" + target[1])
                if not emulator:
                    raise FileNotFoundError("QEMU " + target[1])
                command.insert(0, emulator)
            for status, (path, blob) in cases.items():
                result = native_run(command, path)
                if result.returncode != status or result.stderr != CLEAN or result.stdout:
                    raise RuntimeError(f"expected {status}, got {result.returncode}: "
                                       f"{result.stdout!r} {result.stderr!r}")
                if path.read_bytes() != blob:
                    raise RuntimeError("input fixture changed")
            for path, diagnostic in ((bad_call, b"invalid VM instruction boundary"),
                                     (truncated, b"invalid compact UEEC section spans")):
                result = native_run(command, path)
                if result.returncode != 126 or diagnostic not in result.stderr or CLEAN in result.stderr:
                    raise RuntimeError(f"expected rejection of {path.name}: {result.stderr!r}")
            print(f"PASS {name:11} {'native' if native else 'QEMU':6} "
                  "exit=0,42,255; both mappings absent before entry; 2 invalid fixtures rejected", flush=True)
        except (OSError, subprocess.SubprocessError, RuntimeError) as error:
            print(f"FAIL {name}: {error}", flush=True)
            failed.append(name)
    print(f"{len(selected) - len(failed)}/{len(selected)} targets passed")
    return bool(failed)


if __name__ == "__main__":
    sys.exit(main())
