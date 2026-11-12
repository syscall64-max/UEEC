"""Explicit UEEC machine IDs, build baselines and technology requirements.

Wire feature positions belong to a family, NOT to Linux HWCAP/CPUID.
Metadata type IDs are experimental; x86 requirements use record type 1.
"""
from dataclasses import dataclass

@dataclass(frozen=True)
class Variant:
    name: str
    required: int
    priority: int
    flags: str
    evidence: str

@dataclass(frozen=True)
class Architecture:
    machine: int
    cc: str
    baseline: str
    metadata_type: int
    variants: tuple
    qemu: str
    cpu: str

def vector(name, flags, evidence, size=16, required=1, priority=200):
    return Variant(name, required, priority,
                   f"-DUEEC_VECTOR_BYTES={size} {flags}", evidence)

SCALAR = Variant("scalar", 0, 100, "", "")
SSE2 = vector("sse2", "-msse2", r"\bpaddd\b", required=1 << 6)
AVX2 = vector("avx2", "-mavx2", r"\bvpaddd\b.*%ymm", size=32,
              required=(1 << 6) | (1 << 12) | (1 << 13), priority=300)
AVX512 = Variant("avx512f", (1 << 6) | (1 << 12) | (1 << 16), 400, "", r"%zmm")
ARCHES = {
    "arm32": Architecture(0, "arm-linux-gnueabihf-gcc", "-marm -march=armv7-a -mfpu=vfpv3-d16", 0x100,
        (SCALAR, vector("neon", "-mfpu=neon", r"\bvadd\.i32\b")), "qemu-arm", "max"),
    "arm64": Architecture(1, "aarch64-linux-gnu-gcc", "-march=armv8-a", 0x101,
        (SCALAR, vector("neon", "", r"\badd\s+v\d+\.4s"),
         Variant("sve", 3, 300, "-DUEEC_SVE -march=armv8-a+sve", r"\buaddv\b")), "qemu-aarch64", "max"),
    "loongarch64": Architecture(2, "loongarch64-linux-gnu-gcc-14", "-march=loongarch64 -mabi=lp64d -msimd=none", 0x102,
        (SCALAR, vector("lsx", "-mlsx", r"\bvadd\.w\b"),
         vector("lasx", "-mlasx", r"\bxvadd\.w\b", 32, 3, 300)), "qemu-loongarch64", "max"),
    "powerpc32": Architecture(3, "powerpc-linux-gnu-gcc", "-mcpu=powerpc -mno-altivec -mno-vsx", 0x103,
        (SCALAR, vector("altivec", "-mcpu=7400 -maltivec -mno-vsx", r"\bvadduwm\b")), "qemu-ppc", "g4"),
    "powerpc64": Architecture(4, "powerpc64-linux-gnu-gcc", "-mcpu=power4 -mno-altivec -mno-vsx", 0x103,
        (SCALAR, vector("altivec", "-mcpu=power4 -maltivec -mno-vsx", r"\bvadduwm\b")), "qemu-ppc64", "power8"),
    "riscv32": Architecture(5, "riscv32-unknown-linux-gnu-gcc", "-march=rv32gc -mabi=ilp32d", 0x105,
        (SCALAR, Variant("rvv", 1, 200, "-DUEEC_RVV -march=rv32gcv", r"\bvredsum\.vs\b")), "qemu-riscv32", "rv32,v=true,vlen=128,elen=64"),
    "riscv64": Architecture(6, "riscv64-linux-gnu-gcc", "-march=rv64gc -mabi=lp64d", 0x105,
        (SCALAR, Variant("rvv", 1, 200, "-DUEEC_RVV -march=rv64gcv", r"\bvredsum\.vs\b")), "qemu-riscv64", "max,vlen=128,elen=64"),
    "sparc32": Architecture(7, "sparc64-linux-gnu-gcc", "-m32 -mcpu=v8 -mno-vis", 0x107,
        (SCALAR, vector("vis", "-mcpu=ultrasparc -mvis -DUEEC_VIS", r"\bfpadd32\b", 8)), "qemu-sparc32plus", "TI UltraSparc II"),
    "sparc64": Architecture(8, "sparc64-linux-gnu-gcc", "-m64 -mcpu=v9 -mno-vis", 0x107,
        (SCALAR, vector("vis", "-mcpu=ultrasparc -mvis -DUEEC_VIS", r"\bfpadd32\b", 8)), "qemu-sparc64", "TI UltraSparc II"),
    "x86": Architecture(9, "i686-linux-gnu-gcc", "-march=i686 -mno-sse -mno-sse2 -mno-avx", 1,
        (SCALAR, SSE2, AVX2), "qemu-i386", "max"),
    "x86_64": Architecture(10, "gcc", "-march=x86-64 -mno-avx", 1,
        (SCALAR, SSE2, AVX2, AVX512), "qemu-x86_64", "max"),
    "s390x": Architecture(11, "s390x-linux-gnu-gcc", "-march=z196 -mno-vx", 0x10b,
        (SCALAR, vector("vx", "-march=z13 -mvx", r"\bvaf\b")), "qemu-s390x", "max"),
}

def native_arch():
    import platform
    name = platform.machine().lower()
    return {"amd64": "x86_64", "i386": "x86", "i686": "x86", "aarch64": "arm64",
            "armv7l": "arm32", "ppc": "powerpc32", "ppc64": "powerpc64",
            "sparc": "sparc32", "sparc64": "sparc64"}.get(name, name)
