#ifndef UEEC_CAPABILITIES_H
#define UEEC_CAPABILITIES_H
#include <inttypes.h>
#include <stdio.h>
#include "ueec_metadata.h"

/* Prototype family records. 32/64-bit members of a family share bit positions.
   These C views are descriptive; the wire is always explicit uint64 encoding. */
typedef struct UEEC_PACKED {
    uint64_t neon:1, vfp:1, vfpv3:1, vfpv4:1, idiva:1, idivt:1,
             aes:1, pmull:1, sha1:1, sha2:1, crc32:1, reserved:53;
} ueec_md_arm32_caps;
typedef struct UEEC_PACKED {
    uint64_t neon:1, sve:1, fp:1, aes:1, pmull:1, sha1:1, sha2:1,
             crc32:1, atomics:1, sve2:1, sha3:1, sha512:1, dotprod:1,
             i8mm:1, bf16:1, reserved:49;
} ueec_md_arm64_caps;
typedef struct UEEC_PACKED {
    uint64_t lsx:1, lasx:1, fpu:1, ual:1, cpucfg:1, reserved:59;
} ueec_md_loongarch_caps;
typedef struct UEEC_PACKED {
    uint64_t altivec:1, vsx:1, arch_2_06:1, arch_2_07:1, arch_3_00:1,
             arch_3_1:1, dfp:1, crypto:1, htm:1, reserved:55;
} ueec_md_powerpc_caps;
typedef struct UEEC_PACKED {
    uint64_t rvv:1, i:1, m:1, a:1, f:1, d:1, c:1, zba:1, zbb:1,
             zbs:1, reserved:54;
} ueec_md_riscv_caps;
typedef struct UEEC_PACKED {
    uint64_t vis:1, vis2:1, vis3:1, popc:1, fmaf:1, reserved:59;
} ueec_md_sparc_caps;
typedef struct UEEC_PACKED {
    uint64_t vx:1, vxe:1, vxe2:1, msa:1, stfle:1, dfp:1, reserved:58;
} ueec_md_s390x_caps;

_Static_assert(sizeof(ueec_md_arm32_caps)==8, "ARM32 capability size");
_Static_assert(sizeof(ueec_md_arm64_caps)==8, "ARM64 capability size");
_Static_assert(sizeof(ueec_md_loongarch_caps)==8, "LoongArch capability size");
_Static_assert(sizeof(ueec_md_powerpc_caps)==8, "PowerPC capability size");
_Static_assert(sizeof(ueec_md_riscv_caps)==8, "RISC-V capability size");
_Static_assert(sizeof(ueec_md_sparc_caps)==8, "SPARC capability size");
_Static_assert(sizeof(ueec_md_s390x_caps)==8, "S390X capability size");

static const char *const caps_x86[] = {
    "x87", "mmx", "mmxext", "3dnow", "3dnowext", "sse", "sse2", "sse3", "ssse3", "sse4_1", "sse4_2", "sse4a",
    "avx", "avx2", "avx_vnni", "xop", "avx512f", "avx512cd", "avx512dq", "avx512bw", "avx512vl",
    "avx512ifma", "avx512vbmi", "avx512vbmi2", "avx512vnni", "avx512bitalg", "avx512vpopcntdq", "avx512bf16", "avx512fp16",
    "fma3", "fma4", "f16c", "bmi1", "bmi2", "tbm", "adx", "popcnt", "lzcnt", "aes_ni", "vaes", "pclmulqdq", "vpclmulqdq", "sha_ni",
    "rdrand", "rdseed", "amx_tile", "amx_int8", "amx_bf16", "amx_fp16"
};
static const char *const caps_arm32[] = {"neon","vfp","vfpv3","vfpv4","idiva","idivt","aes","pmull","sha1","sha2","crc32"};
static const char *const caps_arm64[] = {"neon","sve","fp","aes","pmull","sha1","sha2","crc32","atomics","sve2","sha3","sha512","dotprod","i8mm","bf16"};
static const char *const caps_loong[] = {"lsx","lasx","fpu","ual","cpucfg"};
static const char *const caps_power[] = {"altivec","vsx","arch_2_06","arch_2_07","arch_3_00","arch_3_1","dfp","crypto","htm"};
static const char *const caps_riscv[] = {"rvv","i","m","a","f","d","c","zba","zbb","zbs"};
static const char *const caps_sparc[] = {"vis","vis2","vis3","popc","fmaf"};
static const char *const caps_s390[] = {"vx","vxe","vxe2","msa","stfle","dfp"};

typedef struct {
    const char *architecture;
    uint16_t metadata_type;
    const char *const *names;
    unsigned count;
} capability_family;

static capability_family family_for(unsigned machine)
{
    capability_family f={"unknown",0,NULL,0};
#define FAMILY(label,type,array) do { f.architecture=label; f.metadata_type=type; \
    f.names=array; f.count=(unsigned)(sizeof(array)/sizeof(array[0])); } while (0)
    switch (machine) {
    case 0: FAMILY("arm32",0x100,caps_arm32); break;
    case 1: FAMILY("arm64",0x101,caps_arm64); break;
    case 2: FAMILY("loongarch64",0x102,caps_loong); break;
    case 3: FAMILY("powerpc32",0x103,caps_power); break;
    case 4: FAMILY("powerpc64",0x103,caps_power); break;
    case 5: FAMILY("riscv32",0x105,caps_riscv); break;
    case 6: FAMILY("riscv64",0x105,caps_riscv); break;
    case 7: FAMILY("sparc32",0x107,caps_sparc); break;
    case 8: FAMILY("sparc64",0x107,caps_sparc); break;
    case 9: FAMILY("x86",1,caps_x86); break;
    case 10: FAMILY("x86_64",1,caps_x86); break;
    case 11: FAMILY("s390x",0x10b,caps_s390); break;
    default: break;
    }
#undef FAMILY
    return f;
}

static void print_capabilities(unsigned machine,uint64_t mask)
{
    capability_family f=family_for(machine);
    int any=0;
    for (unsigned i=0;i<f.count;++i) {
        if (mask & (UINT64_C(1)<<i)) {
            printf("%s%s",any ? "," : "",f.names[i]);any=1;
        }
    }
    uint64_t unknown=mask & ~((UINT64_C(1)<<f.count)-1U);
    if (unknown) {printf("%sunknown:0x%016" PRIx64,any ? "," : "",unknown);any=1;}
    if (!any) printf("baseline");
}
#endif
