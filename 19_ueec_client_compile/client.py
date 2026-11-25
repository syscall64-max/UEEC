"""Fetch a UEEC VM node and persist a native Linux UEEC file."""
import argparse
from hashlib import sha256
from pathlib import Path
import subprocess
import sys
from tempfile import NamedTemporaryFile, TemporaryFile
from urllib.request import ProxyHandler, build_opener

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "16_ueec_vm"))
from generate import container

# UEEC CPU ID, ELF machine, ELF class, and machine code byte order.
CPU_HEADERS = {
    "ARM32": (0, 40, 1, "little"),
    "AArch64": (1, 183, 2, "little"),
    "LoongArch64": (2, 258, 2, "little"),
    "PowerPC32": (3, 20, 1, "big"),
    "PowerPC64": (4, 21, 2, "big"),
    "RISC-V32": (5, 243, 1, "little"),
    "RISC-V64": (6, 243, 2, "little"),
    "SPARC32": (7, 2, 1, "big"),
    "SPARC64": (8, 43, 2, "big"),
    "x86": (9, 3, 1, "little"),
    "x86-64": (10, 62, 2, "little"),
    "s390x": (11, 22, 2, "big"),
}
MAX_VM_FILE = 1024
MAX_NATIVE_CODE = 8192


def native_container(target, code, metadata=None, *, name="call", granule=1):
    if not 0 < len(code) <= MAX_NATIVE_CODE:
        raise ValueError("invalid generated CODE length")
    user_program = (1 << 2) | (1 << 3) | (1 << 5)
    linux = (2 << 18) | (2 << 20)
    machine, _, _, endian = CPU_HEADERS[target]
    bodies = [(1, code)]
    if metadata is not None:
        bodies.append((13, metadata))
    node_word = (endian == "big") | user_program | (machine << 10) | linux | (len(bodies) << 27)
    return container([(name, list(range(1, len(bodies) + 1)), node_word)], bodies, granule=granule)


def compile_code(command, payload, profile=0):
    # The compiler consumes a seekable FD and emits relocated CODE to stdout.
    with TemporaryFile() as source:
        source.write(payload)
        source.seek(0)
        result = subprocess.run(
            [*command, str(len(payload)), str(source.fileno()), "1", str(profile)],
            pass_fds=(source.fileno(),), capture_output=True, check=True, timeout=10,
        )
    if result.stderr:
        raise RuntimeError(f"unexpected compiler diagnostic: {result.stderr!r}")
    return result.stdout


def atomic_write(path, data):
    pending = NamedTemporaryFile(dir=path.parent, delete=False)
    temporary = Path(pending.name)
    try:
        with pending:
            pending.write(data)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, type=int)
    parser.add_argument("--target", required=True, choices=CPU_HEADERS)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--emulator", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be in 1..65535")
    with args.compiler.open("rb") as compiler:
        elf = compiler.read(20)
    _, elf_machine, elf_class, endian = CPU_HEADERS[args.target]
    elf_data = 2 if endian == "big" else 1
    if (elf[:6] != b"\x7fELF" + bytes((elf_class, elf_data)) or len(elf) != 20
            or int.from_bytes(elf[18:20], endian) != elf_machine):
        parser.error("compiler ELF class, byte order or machine does not match --target")

    http = build_opener(ProxyHandler({}))
    with http.open(f"http://127.0.0.1:{args.port}/nodes/call", timeout=5) as response:
        payload = response.read(MAX_VM_FILE + 1)
        if not 0 < len(payload) <= MAX_VM_FILE:
            raise ValueError("invalid VM response length")

    command = [str(args.compiler.resolve())]
    if args.emulator:
        command.insert(0, str(args.emulator))
    native = native_container(args.target, compile_code(command, payload))
    atomic_write(args.output, native)
    print(f"{args.target}: VM {len(payload)} B sha256={sha256(payload).hexdigest()}")
    print(f"{args.target}: native UEEC {len(native)} B sha256={sha256(native).hexdigest()}")


if __name__ == "__main__":
    main()
