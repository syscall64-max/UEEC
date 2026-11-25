#!/usr/bin/env python3
"""No skips: a missing compiler/emulator or failed target fails test-all."""
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from architectures import ARCHES,native_arch

for arch in ARCHES:
    subprocess.run([sys.executable,str(ROOT/"test_dispatch.py"),"--arch",arch,"--qemu"],check=True)
# Also covers AVX-512 on the actual host, when it is OS-usable.
subprocess.run([sys.executable,str(ROOT/"test_dispatch.py"),"--arch",native_arch()],check=True)
print("UEEC ISA dispatch: 12 architecture suites + native suite PASS (no skipped targets)")
