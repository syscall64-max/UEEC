"""Build stage 20 fixtures and compare ELF execution with the UEEC results."""
import argparse
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory, mkdtemp

from ueec2elf import GUARDS, read_ueec

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "19_ueec_client_compile"))
from run_demo import emulator_for
from run_matrix import TARGETS


def run(command, **kwargs):
    return subprocess.run(command, capture_output=True, timeout=90, **kwargs)


def validate_text(path, expected):
    result = run(["readelf", "--wide", "--sections", str(path)], text=True, check=True)
    record = re.search(r"\]\s+\.text\s+PROGBITS\s+[0-9a-f]+\s+([0-9a-f]+)\s+([0-9a-f]+)",
                       result.stdout)
    if not record:
        raise RuntimeError("ELF .text section missing")
    offset, length = (int(value, 16) for value in record.groups())
    if path.read_bytes()[offset:offset + length] != expected:
        raise RuntimeError("CODE bytes changed during conversion")


def validate_guards(directory):
    obj = directory / "features.o"
    sse2, avx2 = directory / "sse2.bin", directory / "avx2.bin"
    run(["cc", "-c", str(ROOT / "x86_features.S"), "-o", str(obj)], check=True)
    run(["objcopy", "--dump-section", f".text.sse2={sse2}", "--dump-section",
         f".text.avx2={avx2}", str(obj)], check=True)
    if sse2.read_bytes() != GUARDS[1 << 6] or avx2.read_bytes() != GUARDS[(1 << 12) | (1 << 13)]:
        raise RuntimeError("ELF entry bytes differ from the assembled feature checks")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default="clang-19")
    parser.add_argument("--lld", default="ld.lld-19")
    parser.add_argument("--sparc-ld", default="sparc64-linux-gnu-ld")
    parser.add_argument("--qemu-dir", type=Path, default=Path("/usr/bin"))
    parser.add_argument("--isolate-network", action="store_true")
    parser.add_argument("--status", type=int, default=42)
    parser.add_argument("--out", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    out = Path(mkdtemp(prefix=f"status-{args.status}-", dir=args.out.resolve()))
    fixture_dir = out / "ueec"
    stage20 = ROOT.parent / "20_ueec_server_compile" / "run_stage20.py"
    command = [sys.executable, str(stage20), "--clang", args.clang, "--lld", args.lld,
               "--sparc-ld", args.sparc_ld, "--qemu-dir", str(args.qemu_dir.resolve()),
               "--status", str(args.status), "--out", str(fixture_dir)]
    if args.isolate_network:
        command.append("--isolate-network")
    stage20_result = run(command, text=True)
    if stage20_result.returncode or "12/12 architectures; 28 offline launches passed" not in stage20_result.stdout:
        raise RuntimeError(stage20_result.stdout + stage20_result.stderr)
    print(f"UEEC loader reference: {args.status}; stage 20 passed on 12 architectures", flush=True)

    offline = ["unshare", "-Urn", "--"] if args.isolate_network else []
    profiles = [(target, "baseline") for target in TARGETS]
    profiles += [("x86-64", "sse2"), ("x86-64", "avx2")]
    avx2_path = None
    qemu_x86_64 = args.qemu_dir / "qemu-x86_64"
    native_avx2 = "avx2" in Path("/proc/cpuinfo").read_text().split()
    converter = [sys.executable, str(ROOT / "ueec2elf.py")]
    with TemporaryDirectory(dir=out) as work:
        temporary = Path(work)
        validate_guards(temporary)
        for target, profile in profiles:
            saved = []
            for granule in (1, 4096):
                source = fixture_dir / f"{target}-{profile}-{granule}.ueec"
                destination = out / f"call-{args.status}-{target}-{profile}-{granule}.elf"
                result = run([*converter, str(source), str(destination)], text=True)
                if result.returncode:
                    raise RuntimeError(result.stderr)
                _, code, _ = read_ueec(source.read_bytes())
                validate_text(destination, code)
                saved.append(destination)
            if saved[0].read_bytes() != saved[1].read_bytes():
                raise RuntimeError(f"{target}/{profile}: compact and granulated outputs differ")

            emulator = emulator_for(target, args.qemu_dir, None)
            if profile == "avx2":
                avx2_path = saved[1]
                if not native_avx2:
                    emulator = [str(qemu_x86_64), "-cpu", "max"]
            for elf in saved:
                result = run([*offline, *emulator, str(elf)])
                if result.returncode != args.status or result.stderr or result.stdout:
                    raise RuntimeError(f"{elf.name}: exit={result.returncode} {result.stderr!r}")
            mode = "QEMU" if emulator else "native"
            print(f"PASS {target}/{profile}: identical ELF from both layouts; exit={args.status} ({mode})")

        result = run([*offline, str(qemu_x86_64), "-cpu", "max,-avx2", str(avx2_path)])
        if result.returncode != 126 or result.stderr or result.stdout:
            raise RuntimeError(f"AVX2 guard rejected incorrectly: {result.returncode} {result.stderr!r}")
        print("PASS AVX2 requirement: ELF exited 126 on QEMU without AVX2")

        truncated = temporary / "truncated.ueec"
        truncated.write_bytes((fixture_dir / "x86-64-baseline-4096.ueec").read_bytes()[:-1])
        rejected = temporary / "rejected.elf"
        for bad in (fixture_dir / "server-source.ueec", truncated):
            result = run([*converter, str(bad), str(rejected)])
            if result.returncode != 1 or rejected.exists():
                raise RuntimeError(f"invalid input accepted: {bad}")
        present = temporary / "existing.elf"
        present.write_bytes(b"earlier contents")
        valid = fixture_dir / "x86-64-baseline-4096.ueec"
        result = run([*converter, str(valid), str(present)])
        if result.returncode != 1 or present.read_bytes() != b"earlier contents":
            raise RuntimeError("existing output overwritten")
        print("PASS VM and truncated inputs rejected; existing output preserved")
    print(f"{len(TARGETS)}/{len(TARGETS)} architectures; {2 * len(profiles)} ELF launches passed")


if __name__ == "__main__":
    main()
