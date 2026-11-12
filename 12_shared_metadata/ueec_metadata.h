#ifndef UEEC_METADATA_H
#define UEEC_METADATA_H

#include <stdint.h>
#include "reference/ueec.h"

/* Prototype assignments, local to this demo. No existing index is changed. */
#define UEEC_SECTION_METADATA UINT16_C(13)
#define UEEC_MD_DESIGN_INDEXED UINT16_C(0)
#define UEEC_MD_DESIGN_NODE_INDEXED UINT16_C(1)
#define UEEC_MD_TYPE_END UINT16_C(0)
#define UEEC_MD_TYPE_X86_REQUIRED UINT16_C(1)
#define UEEC_MD_TYPE_PRIORITY UINT16_C(2)

typedef struct UEEC_PACKED {
    uint16_t format;
    uint16_t design;
} ueec_md_hdr, *pueec_md_hdr;
typedef struct UEEC_PACKED { uint16_t count; } ueec_mdcf16, *pueec_mdcf16;
typedef struct UEEC_PACKED { uint32_t count; } ueec_mdcf32, *pueec_mdcf32;
typedef struct UEEC_PACKED { uint64_t count; } ueec_mdcf64, *pueec_mdcf64;
typedef struct UEEC_PACKED { uint16_t type, offset; } ueec_element16, *pueec_element16;
typedef struct UEEC_PACKED { uint16_t type; uint32_t offset; } ueec_element32, *pueec_element32;
typedef struct UEEC_PACKED { uint16_t type; uint64_t offset; } ueec_element64, *pueec_element64;

/* Shared section: the node index is ALWAYS 16-bit, independently of format.
   Offsets are relative to the data area after the complete index table.
   The final sentinel has node_index=0, type=END and offset=data_size. */
typedef struct UEEC_PACKED {
    uint16_t node_index, type, offset;
} ueec_md_node_element16, *pueec_md_node_element16;
typedef struct UEEC_PACKED {
    uint16_t node_index, type;
    uint32_t offset;
} ueec_md_node_element32, *pueec_md_node_element32;
typedef struct UEEC_PACKED {
    uint16_t node_index, type;
    uint64_t offset;
} ueec_md_node_element64, *pueec_md_node_element64;

/* One continuous bit space. This C view is not written with fwrite(). */
typedef struct UEEC_PACKED {
    uint64_t
        x87:1, mmx:1, mmxext:1, d3now:1, d3nowext:1,
        sse:1, sse2:1, sse3:1, ssse3:1, sse4_1:1, sse4_2:1, sse4a:1,
        avx:1, avx2:1, avx_vnni:1, xop:1,
        avx512f:1, avx512cd:1, avx512dq:1, avx512bw:1, avx512vl:1,
        avx512ifma:1, avx512vbmi:1, avx512vbmi2:1, avx512vnni:1,
        avx512bitalg:1, avx512vpopcntdq:1, avx512bf16:1, avx512fp16:1,
        fma3:1, fma4:1, f16c:1,
        bmi1:1, bmi2:1, tbm:1, adx:1, popcnt:1, lzcnt:1,
        aes_ni:1, vaes:1, pclmulqdq:1, vpclmulqdq:1, sha_ni:1,
        rdrand:1, rdseed:1,
        amx_tile:1, amx_int8:1, amx_bf16:1, amx_fp16:1,
        reserved:15;
} ueec_md_x86_caps, *pueec_md_x86_caps;
typedef ueec_md_x86_caps ueec_md_x86, *pueec_md_x86;
typedef ueec_md_x86_caps ueec_md_x86_64, *pueec_md_x86_64;
typedef struct UEEC_PACKED { uint16_t value; } ueec_md_priority;

/* Wire bit numbers match the declaration above, independent of C ABI. */
enum ueec_x86_bit {
    UEEC_X87, UEEC_MMX, UEEC_MMXEXT, UEEC_D3NOW, UEEC_D3NOWEXT,
    UEEC_SSE, UEEC_SSE2, UEEC_SSE3, UEEC_SSSE3, UEEC_SSE4_1, UEEC_SSE4_2, UEEC_SSE4A,
    UEEC_AVX, UEEC_AVX2, UEEC_AVX_VNNI, UEEC_XOP,
    UEEC_AVX512F, UEEC_AVX512CD, UEEC_AVX512DQ, UEEC_AVX512BW, UEEC_AVX512VL,
    UEEC_AVX512IFMA, UEEC_AVX512VBMI, UEEC_AVX512VBMI2, UEEC_AVX512VNNI,
    UEEC_AVX512BITALG, UEEC_AVX512VPOPCNTDQ, UEEC_AVX512BF16, UEEC_AVX512FP16,
    UEEC_FMA3, UEEC_FMA4, UEEC_F16C,
    UEEC_BMI1, UEEC_BMI2, UEEC_TBM, UEEC_ADX, UEEC_POPCNT, UEEC_LZCNT,
    UEEC_AES_NI, UEEC_VAES, UEEC_PCLMULQDQ, UEEC_VPCLMULQDQ, UEEC_SHA_NI,
    UEEC_RDRAND, UEEC_RDSEED,
    UEEC_AMX_TILE, UEEC_AMX_INT8, UEEC_AMX_BF16, UEEC_AMX_FP16,
    UEEC_X86_FEATURE_COUNT
};
#define UEEC_X86_FLAG(bit) (UINT64_C(1) << (bit))
#define UEEC_X86_KNOWN_MASK ((UINT64_C(1) << UEEC_X86_FEATURE_COUNT) - UINT64_C(1))

_Static_assert(sizeof(ueec_md_hdr) == 4, "metadata header size");
_Static_assert(sizeof(ueec_element16) == 4, "16-bit metadata element size");
_Static_assert(sizeof(ueec_element32) == 6, "32-bit metadata element size");
_Static_assert(sizeof(ueec_element64) == 10, "64-bit metadata element size");
_Static_assert(sizeof(ueec_md_node_element16) == 6, "shared 16-bit element size");
_Static_assert(sizeof(ueec_md_node_element32) == 8, "shared 32-bit element size");
_Static_assert(sizeof(ueec_md_node_element64) == 12, "shared 64-bit element size");
_Static_assert(sizeof(ueec_md_x86_caps) == 8, "x86 capability size");
_Static_assert(UEEC_X86_FEATURE_COUNT == 49, "wire bit count");
#endif
