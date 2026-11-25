"""Serve one compact UEEC VM node over HTTP on loopback."""
import argparse
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "18_ueec_native"))
from run_matrix import guest_fixture


class VMServer(HTTPServer):
    def __init__(self, port, payload):
        self.payload = payload
        super().__init__(("127.0.0.1", port), VMHandler)


class VMHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/nodes/call":
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        payload = self.server.payload
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--status", type=int, default=42)
    args = parser.parse_args()
    if not 0 <= args.status <= 255:
        parser.error("status must be in 0..255")
    with VMServer(args.port, guest_fixture(args.status)) as server:
        print(server.server_port, flush=True)
        server.serve_forever()


if __name__ == "__main__":
    main()
