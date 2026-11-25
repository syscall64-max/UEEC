"""Request and save native UEEC, optionally granulating it locally."""
import argparse
from hashlib import sha256
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import ProxyHandler, build_opener

from protocol import CPU_HEADERS, MAX_RESPONSE, X86_FEATURES, select_profile, transform_received
from client import atomic_write


def native_url(port, target, features, *, os="Linux"):
    query = urlencode({"os": os, "abi": "linux-user", "arch": target,
                       "features": ",".join(sorted(features))})
    return f"http://127.0.0.1:{port}/nodes/call?{query}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--target", choices=CPU_HEADERS, required=True)
    parser.add_argument("--feature", action="append", choices=sorted(X86_FEATURES), default=[])
    parser.add_argument("--granule", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be in 1..65535")
    if not 1 <= args.granule <= 32768 or args.granule & (args.granule - 1):
        parser.error("granule must be a power of two in 1..32768")
    try:
        profile = select_profile(args.target, set(args.feature))
    except ValueError as error:
        parser.error(str(error))

    http = build_opener(ProxyHandler({}))
    with http.open(native_url(args.port, args.target, args.feature), timeout=15) as response:
        received = response.read(MAX_RESPONSE + 1)
    if not 0 < len(received) <= MAX_RESPONSE:
        raise ValueError("invalid response length")
    file = transform_received(received, args.target, profile, args.granule)
    atomic_write(args.output, file)
    print(f"received={len(received)} sha256={sha256(received).hexdigest()}")
    print(f"saved={len(file)} granule={args.granule} profile={profile}")


if __name__ == "__main__":
    main()
