"""Exercise server-side compilation and client-side granulation."""
import argparse
from pathlib import Path
import platform
import select
import subprocess
import sys
from urllib.error import HTTPError, URLError
from urllib.request import ProxyHandler, build_opener

from protocol import CPU_HEADERS, transform_received
from fetch import native_url
from run_demo import LOADER_READY, emulator_for, run
from run_matrix import TARGETS, build_native, guest_fixture, native_run

ROOT = Path(__file__).resolve().parent
VM = ROOT.parent / "16_ueec_vm"


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
    if platform.machine() != "x86_64":
        parser.error("the test harness requires an x86-64 host")
    if not 0 <= args.status <= 255:
        parser.error("status must be in 0..255")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    offline = ["unshare", "-Urn", "--"] if args.isolate_network else []
    source = out / "server-source.ueec"
    fixture = guest_fixture(args.status)
    source.write_bytes(fixture)
    run(["make", "-s", "-C", str(VM), "build/ueec-vm"], check=True)
    expected = run([str(VM / "build/ueec-vm"), str(source)], capture_output=True)
    if expected.returncode != args.status or f"exit={args.status}".encode() not in expected.stdout:
        raise RuntimeError("VM reference execution failed")
    print(f"Stored VM: {len(fixture)} B; interpreter exit={expected.returncode}", flush=True)

    loaders = {}
    for target in TARGETS:
        build = (TARGETS[target], args.clang, args.lld, args.sparc_ld, out)
        build_native(target, *build)
        loader = build_native(
            f"ueec-load-{target}", *build, source=ROOT.parent / "19_ueec_client_compile" / "loader.c",
            defines=(f"-DUEEC_CPU_MACHINE={CPU_HEADERS[target][0]}",),
        )
        loaders[target] = [*emulator_for(target, args.qemu_dir, None), str(loader)]
    cases = [(target, "baseline", []) for target in TARGETS]
    cases.extend((("x86-64", "sse2", ["sse2"]), ("x86-64", "avx2", ["sse2", "avx", "avx2"])))

    http = build_opener(ProxyHandler({}))
    files = {}
    server = subprocess.Popen(
        [sys.executable, str(ROOT / "server.py"), "--source", str(source),
         "--compiler-dir", str(out), "--qemu-dir", str(args.qemu_dir.resolve())],
        stdout=subprocess.PIPE, text=True,
    )
    try:
        if not select.select([server.stdout], [], [], 5)[0]:
            raise RuntimeError("server did not start")
        port = int(server.stdout.readline())
        for target, profile, features in cases:
            wire = set()
            for granule in (1, 4096):
                path = out / f"{target}-{profile}-{granule}.ueec"
                command = [sys.executable, str(ROOT / "fetch.py"), "--port", str(port),
                           "--target", target, "--granule", str(granule), "--output", str(path)]
                for feature in features:
                    command.extend(["--feature", feature])
                result = run(command, capture_output=True, text=True)
                if result.returncode:
                    raise RuntimeError(result.stderr)
                wire.add(result.stdout.splitlines()[0])
                files[target, profile, granule] = path
            compact, padded = (files[target, profile, n].read_bytes() for n in (1, 4096))
            if len(wire) != 1 or transform_received(compact, target, profile, 4096) != padded:
                raise RuntimeError(f"wire response or granulation changed for {target}/{profile}")
            print(f"{target}/{profile}: HTTP {len(compact)} B; local padded {len(padded)} B; "
                  f"{wire.pop().split()[-1]}", flush=True)
        for bad_url in (native_url(port, "x86-64", [], os="Windows"),
                        native_url(port, "ARM32", ["sse2"]),
                        native_url(port, "x86-64", ["avx2"])):
            try:
                with http.open(bad_url, timeout=4):
                    raise RuntimeError("server accepted an invalid profile")
            except HTTPError as error:
                if error.code != 400:
                    raise
    finally:
        server.terminate()
        server.communicate(timeout=5)
    try:
        with http.open(native_url(port, "x86-64", []), timeout=2):
            raise RuntimeError("server still answers")
    except HTTPError:
        raise RuntimeError("server address still serves HTTP") from None
    except URLError:
        print("Server stopped; HTTP connection failed as expected", flush=True)

    # The byte patterns are independently assembled and correspond to
    # pshufd (SSE2) and vpbroadcastd (AVX2), used by the lowering of MOV.
    checks = (("baseline", b""), ("sse2", bytes.fromhex("660f70c000")),
              ("avx2", bytes.fromhex("c4e27d58c0")))
    code_blobs = []
    for profile, opcode in checks:
        blob = files["x86-64", profile, 1].read_bytes()
        header_offset = 20 if profile == "baseline" else 26
        code_offset = int.from_bytes(blob[header_offset + 2:header_offset + 4], "little")
        code_length = int.from_bytes(blob[header_offset + 10:header_offset + 12], "little")
        code = blob[code_offset:code_offset + code_length]
        if opcode not in code:
            raise RuntimeError(f"{profile} did not use the expected instruction")
        code_blobs.append(code)
    if len(set(code_blobs)) != len(checks):
        raise RuntimeError("CPU profiles emitted the same code")
    print("x86-64 profile instructions verified: baseline, pshufd, vpbroadcastd", flush=True)

    x86_emulator = args.qemu_dir / "qemu-x86_64"
    native_x86_flags = Path("/proc/cpuinfo").read_text().split()
    for target, profile, _ in cases:
        loader = loaders[target]
        mode = "native" if target == "x86-64" else "QEMU"
        if profile == "avx2" and "avx2" not in native_x86_flags:
            loader = [str(x86_emulator), "-cpu", "max", *loader]
            mode = "QEMU"
        for granule in (1, 4096):
            result = native_run([*offline, *loader], files[target, profile, granule])
            if result.returncode != expected.returncode or result.stderr != LOADER_READY or result.stdout:
                raise RuntimeError(f"{target}/{profile}/{granule} failed: "
                                   f"exit={result.returncode} {result.stderr!r} {result.stdout!r}")
        print(f"PASS {target}/{profile}: compact and granulated exit={args.status} ({mode})")
    rejected = native_run([*offline, str(x86_emulator), "-cpu", "max,-avx2", *loaders["x86-64"]],
                          files["x86-64", "avx2", 1])
    if rejected.returncode != 126 or rejected.stderr != b"AVX2 unavailable\n":
        raise RuntimeError(f"AVX2 rejection failed: {rejected.returncode} {rejected.stderr!r}")
    print("PASS feature mismatch: AVX2 file rejected before entry on QEMU without AVX2")
    print(f"{len(TARGETS)}/{len(TARGETS)} architectures; {len(cases) * 2} offline launches passed")


if __name__ == "__main__":
    main()
