"""Verify download, client-side compilation, and offline UEEC execution."""
import argparse
from hashlib import sha256
from pathlib import Path
import select
import shutil
import subprocess
import sys
import tempfile
from urllib.error import HTTPError, URLError
from urllib.request import ProxyHandler, build_opener

ROOT = Path(__file__).resolve().parent
VM = ROOT.parent / "16_ueec_vm"
sys.path.insert(0, str(ROOT.parent / "18_ueec_native"))
from run_matrix import TARGETS, build_native, guest_fixture, native_run
from client import CPU_HEADERS

LOADER_READY = b"offline native UEEC CODE is RX\n"


def run(args, **kwargs):
    return subprocess.run(args, timeout=60, **kwargs)


def emulator_for(target, qemu_dir, aarch64_override):
    if target == "x86-64":
        return []
    if target == "AArch64" and aarch64_override:
        return [aarch64_override]
    name = "qemu-" + TARGETS[target][1]
    candidate = qemu_dir / name
    if candidate.is_file():
        return [str(candidate.resolve())]
    installed = shutil.which(name)
    if installed:
        return [installed]
    raise FileNotFoundError(f"{name} required for {target}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default="clang-19")
    parser.add_argument("--lld", default="ld.lld-19")
    parser.add_argument("--sparc-ld", default="sparc64-linux-gnu-ld")
    parser.add_argument("--qemu-dir", type=Path, default=Path("/usr/bin"))
    parser.add_argument("--qemu", help="override the AArch64 emulator path")
    parser.add_argument("--arch", action="append", choices=TARGETS)
    parser.add_argument("--isolate-network", action="store_true")
    parser.add_argument("--status", type=int, default=42)
    parser.add_argument("--out", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    if not 0 <= args.status <= 255:
        parser.error("status must be in 0..255")

    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    offline = ["unshare", "-Urn", "--"] if args.isolate_network else []
    fixture = guest_fixture(args.status)
    expected_digest = sha256(fixture).hexdigest()
    run(["make", "-s", "-C", str(VM), "build/ueec-vm"], check=True)

    with tempfile.TemporaryDirectory(dir=out) as work:
        reference = Path(work) / "reference.ueec"
        reference.write_bytes(fixture)
        baseline = run([str(VM / "build/ueec-vm"), str(reference)], capture_output=True)
        if baseline.returncode != args.status or f"exit={args.status}".encode() not in baseline.stdout:
            raise RuntimeError(f"VM baseline failed: {baseline.stderr!r} {baseline.stdout!r}")
    print(f"VM reference: exit={baseline.returncode}", flush=True)

    selected = list(dict.fromkeys(args.arch or TARGETS))
    tools = {}
    for target in selected:
        emulator = emulator_for(target, args.qemu_dir, args.qemu)
        build = (TARGETS[target], args.clang, args.lld, args.sparc_ld, out)
        compiler = build_native(target, *build)
        cpu_id = CPU_HEADERS[target][0]
        loader = build_native(f"ueec-load-{target}", *build, source=ROOT / "loader.c",
                              defines=(f"-DUEEC_CPU_MACHINE={cpu_id}",))
        tools[target] = (emulator, compiler, loader)

    outputs = {}
    server = subprocess.Popen(
        [sys.executable, str(ROOT / "server.py"), "--status", str(args.status)],
        stdout=subprocess.PIPE, text=True,
    )
    try:
        if not select.select([server.stdout], [], [], 5)[0]:
            raise RuntimeError("server did not start")
        port = int(server.stdout.readline())
        for target, (emulator, compiler, _) in tools.items():
            path = out / f"call-{target}.ueec"
            command = [
                sys.executable, str(ROOT / "client.py"), "--port", str(port),
                "--target", target, "--compiler", str(compiler), "--output", str(path),
            ]
            if emulator:
                command.extend(["--emulator", emulator[0]])
            result = run(command, check=True, text=True, capture_output=True)
            if f"VM {len(fixture)} B sha256={expected_digest}" not in result.stdout or result.stderr:
                raise RuntimeError(f"unexpected input received by {target}")
            print(result.stdout, end="", flush=True)
            outputs[target] = path
    finally:
        server.terminate()
        server.communicate(timeout=5)

    http = build_opener(ProxyHandler({}))
    try:
        with http.open(f"http://127.0.0.1:{port}/nodes/call", timeout=2):
            raise RuntimeError("VM server still answers")
    except HTTPError:
        raise RuntimeError("VM server address still serves HTTP") from None
    except URLError:
        print("VM server stopped; HTTP fetch failed as expected", flush=True)

    for index, (target, (emulator, _, loader)) in enumerate(tools.items()):
        command = [*offline, *emulator, str(loader)]
        result = native_run(command, outputs[target])
        if result.returncode != baseline.returncode or result.stdout or result.stderr != LOADER_READY:
            raise RuntimeError(f"{target} offline launch failed: {result.returncode}, "
                               f"{result.stderr!r} {result.stdout!r}")
        if len(selected) > 1:
            other = outputs[selected[(index + 1) % len(selected)]]
            rejected = native_run(command, other)
            if rejected.returncode != 126 or rejected.stderr != b"no compatible UEEC node\n":
                raise RuntimeError(f"{target} wrong-CPU check failed: {rejected.stderr!r}")
        mode = "QEMU user mode" if emulator else "native"
        isolation = ", isolated network namespace" if offline else ""
        mismatch_check = "; wrong CPU rejected" if len(selected) > 1 else ""
        print(f"PASS {target}: offline exit={result.returncode} ({mode}{isolation}){mismatch_check}")
    print(f"{len(selected)}/{len(selected)} targets passed")


if __name__ == "__main__":
    main()
