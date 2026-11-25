"""Compile a stored UEEC VM file for the requested Linux target."""
import argparse
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import subprocess
from urllib.parse import parse_qs, urlsplit

from protocol import PROFILES, requirement_metadata, select_profile
from client import MAX_VM_FILE, compile_code, native_container
from run_demo import emulator_for


class CompileServer(HTTPServer):
    def __init__(self, port, source, compiler_dir, qemu_dir):
        self.source = source
        self.compiler_dir = compiler_dir
        self.qemu_dir = qemu_dir
        super().__init__(("127.0.0.1", port), CompileHandler)


class CompileHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        url = urlsplit(self.path)
        if url.path != "/nodes/call":
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        try:
            fields = parse_qs(url.query, keep_blank_values=True, strict_parsing=True,
                              max_num_fields=4)
            if fields.keys() != {"os", "abi", "arch", "features"} or any(
                len(value) != 1 for value in fields.values()
            ):
                raise ValueError("expected os, abi, arch and features")
            if fields["os"] != ["Linux"] or fields["abi"] != ["linux-user"]:
                raise ValueError("only Linux user ABI is supported")
            target = fields["arch"][0]
            features = set(fields["features"][0].split(",")) if fields["features"][0] else set()
            profile = select_profile(target, features)
        except ValueError as error:
            self.send_error(HTTPStatus.BAD_REQUEST, str(error))
            return
        try:
            compiler = self.server.compiler_dir / target
            command = [*emulator_for(target, self.server.qemu_dir, None), str(compiler)]
            code = compile_code(command, self.server.source, PROFILES[profile][0])
            response = native_container(target, code, requirement_metadata(profile))
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError):
            self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR, "target compilation failed")
            return
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.wfile.write(response)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--compiler-dir", type=Path, required=True)
    parser.add_argument("--qemu-dir", type=Path, default=Path("/usr/bin"))
    args = parser.parse_args()
    with args.source.open("rb") as file:
        source = file.read(MAX_VM_FILE + 1)
    if not 0 < len(source) <= MAX_VM_FILE:
        parser.error("invalid VM source file length")
    with CompileServer(args.port, source, args.compiler_dir.resolve(), args.qemu_dir) as server:
        print(server.server_port, flush=True)
        server.serve_forever()


if __name__ == "__main__":
    main()
