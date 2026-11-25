"""Target profiles and compact native UEEC responses for stage 20."""
from pathlib import Path
import struct
import sys

STAGE19 = Path(__file__).resolve().parents[1] / "19_ueec_client_compile"
sys.path.insert(0, str(STAGE19))
from client import CPU_HEADERS, native_container

SSE2 = 1 << 6
AVX = 1 << 12
AVX2 = 1 << 13
PROFILES = {"baseline": (0, 0), "sse2": (1, SSE2), "avx2": (2, AVX | AVX2)}
X86_FEATURES = {"sse2", "avx", "avx2"}
MAX_RESPONSE = 8192 + 320


def select_profile(target, features):
    if target not in CPU_HEADERS:
        raise ValueError("unknown target architecture")
    if target != "x86-64":
        if features:
            raise ValueError("this target accepts only the baseline profile")
        return "baseline"
    if features - X86_FEATURES or ("avx2" in features and "avx" not in features):
        raise ValueError("invalid x86-64 feature set")
    if "avx2" in features:
        return "avx2"
    return "sse2" if "sse2" in features else "baseline"


def requirement_metadata(profile):
    _, mask = PROFILES[profile]
    if not mask:
        return None
    # Existing indexed SH16 metadata: required mask, priority, sentinel.
    return struct.pack("<9HQH", 0, 0, 2, 1, 0, 2, 8, 0, 10, mask, 0)


def transform_received(data, target, profile, granule):
    metadata = requirement_metadata(profile)
    count = 1 + (metadata is not None)
    header_desc = 14 + 6 * count
    code_desc = header_desc + 6
    if len(data) < code_desc + 6:
        raise ValueError("truncated UEEC response")
    header_span = struct.unpack_from("<H", data, header_desc + 2)[0]
    code_length = struct.unpack_from("<H", data, code_desc + 4)[0]
    code = data[header_span:header_span + code_length]
    if data != native_container(target, code, metadata):
        raise ValueError("expected a compact UEEC matching the requested profile")
    return native_container(target, code, metadata, granule=granule)
