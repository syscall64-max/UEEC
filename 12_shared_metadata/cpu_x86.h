#ifndef CPU_X86_H
#define CPU_X86_H
#include <cpuid.h>
#include <stdio.h>
#include <string.h>
#include "ueec_metadata.h"

typedef struct {
    uint32_t one[4], seven[4], seven_one[4], extended[4];
    uint64_t xcr0;
} cpu_registers;
typedef struct {
    char vendor[13], brand[49];
    cpu_registers regs;
    uint64_t hardware, usable;
} cpu_features;

static const char *const feature_names[UEEC_X86_FEATURE_COUNT] = {
    "x87", "mmx", "mmxext", "3dnow", "3dnowext",
    "sse", "sse2", "sse3", "ssse3", "sse4_1", "sse4_2", "sse4a",
    "avx", "avx2", "avx_vnni", "xop",
    "avx512f", "avx512cd", "avx512dq", "avx512bw", "avx512vl",
    "avx512ifma", "avx512vbmi", "avx512vbmi2", "avx512vnni",
    "avx512bitalg", "avx512vpopcntdq", "avx512bf16", "avx512fp16",
    "fma3", "fma4", "f16c", "bmi1", "bmi2", "tbm", "adx", "popcnt", "lzcnt",
    "aes_ni", "vaes", "pclmulqdq", "vpclmulqdq", "sha_ni", "rdrand", "rdseed",
    "amx_tile", "amx_int8", "amx_bf16", "amx_fp16"
};

static uint64_t cpu_hardware_mask(const cpu_registers *r)
{
    uint64_t m = 0;
#define ADD(leaf,reg,pos,feature) do { \
    if ((r->leaf[reg] & (UINT32_C(1) << (pos))) != 0U) \
        m |= UEEC_X86_FLAG(feature); \
} while (0)
    ADD(one,3,0,UEEC_X87); ADD(one,3,23,UEEC_MMX);
    ADD(extended,3,22,UEEC_MMXEXT); ADD(extended,3,31,UEEC_D3NOW);
    ADD(extended,3,30,UEEC_D3NOWEXT);
    ADD(one,3,25,UEEC_SSE); ADD(one,3,26,UEEC_SSE2);
    ADD(one,2,0,UEEC_SSE3); ADD(one,2,9,UEEC_SSSE3);
    ADD(one,2,19,UEEC_SSE4_1); ADD(one,2,20,UEEC_SSE4_2);
    ADD(extended,2,6,UEEC_SSE4A);
    ADD(one,2,28,UEEC_AVX); ADD(seven,1,5,UEEC_AVX2);
    ADD(seven_one,0,4,UEEC_AVX_VNNI); ADD(extended,2,11,UEEC_XOP);
    ADD(seven,1,16,UEEC_AVX512F); ADD(seven,1,28,UEEC_AVX512CD);
    ADD(seven,1,17,UEEC_AVX512DQ); ADD(seven,1,30,UEEC_AVX512BW);
    ADD(seven,1,31,UEEC_AVX512VL); ADD(seven,1,21,UEEC_AVX512IFMA);
    ADD(seven,2,1,UEEC_AVX512VBMI); ADD(seven,2,6,UEEC_AVX512VBMI2);
    ADD(seven,2,11,UEEC_AVX512VNNI); ADD(seven,2,12,UEEC_AVX512BITALG);
    ADD(seven,2,14,UEEC_AVX512VPOPCNTDQ); ADD(seven_one,0,5,UEEC_AVX512BF16);
    ADD(seven,3,23,UEEC_AVX512FP16);
    ADD(one,2,12,UEEC_FMA3); ADD(extended,2,16,UEEC_FMA4); ADD(one,2,29,UEEC_F16C);
    ADD(seven,1,3,UEEC_BMI1); ADD(seven,1,8,UEEC_BMI2);
    ADD(extended,2,21,UEEC_TBM); ADD(seven,1,19,UEEC_ADX);
    ADD(one,2,23,UEEC_POPCNT); ADD(extended,2,5,UEEC_LZCNT);
    ADD(one,2,25,UEEC_AES_NI); ADD(seven,2,9,UEEC_VAES);
    ADD(one,2,1,UEEC_PCLMULQDQ); ADD(seven,2,10,UEEC_VPCLMULQDQ);
    ADD(seven,1,29,UEEC_SHA_NI); ADD(one,2,30,UEEC_RDRAND);
    ADD(seven,1,18,UEEC_RDSEED);
    ADD(seven,3,24,UEEC_AMX_TILE); ADD(seven,3,25,UEEC_AMX_INT8);
    ADD(seven,3,22,UEEC_AMX_BF16); ADD(seven_one,0,21,UEEC_AMX_FP16);
#undef ADD
    return m;
}

static uint64_t cpu_usable_mask(const cpu_registers *r, uint64_t hardware)
{
    uint64_t m = hardware;
    const uint32_t xsave_osxsave = (UINT32_C(1) << 26) | (UINT32_C(1) << 27);
    const uint64_t avx512 = ((UINT64_C(1) << 29) - 1U) & ~((UINT64_C(1) << 16) - 1U);
    const uint64_t avx_state = UEEC_X86_FLAG(UEEC_AVX) | UEEC_X86_FLAG(UEEC_AVX2) |
        UEEC_X86_FLAG(UEEC_AVX_VNNI) | UEEC_X86_FLAG(UEEC_XOP) | avx512 |
        UEEC_X86_FLAG(UEEC_FMA3) | UEEC_X86_FLAG(UEEC_FMA4) |
        UEEC_X86_FLAG(UEEC_F16C) | UEEC_X86_FLAG(UEEC_VAES) |
        UEEC_X86_FLAG(UEEC_VPCLMULQDQ);
    if ((r->one[2] & xsave_osxsave) != xsave_osxsave ||
        (hardware & UEEC_X86_FLAG(UEEC_AVX)) == 0U || (r->xcr0 & 6U) != 6U)
        m &= ~avx_state;
    if ((r->xcr0 & UINT64_C(0xe6)) != UINT64_C(0xe6) ||
        (m & UEEC_X86_FLAG(UEEC_AVX512F)) == 0U)
        m &= ~avx512;
    /* Linux AMX needs additional per-thread permission. Not enabled by this demo. */
    m &= ~(UEEC_X86_FLAG(UEEC_AMX_TILE) | UEEC_X86_FLAG(UEEC_AMX_INT8) |
           UEEC_X86_FLAG(UEEC_AMX_BF16) | UEEC_X86_FLAG(UEEC_AMX_FP16));
    return m;
}

static cpu_features detect_cpu(void)
{
    cpu_features c;
    unsigned a,b,d,e,max_basic,max_ext;
    memset(&c,0,sizeof(c));
    max_basic = __get_cpuid_max(0,NULL);
    __cpuid_count(0,0,a,b,e,d);
    memcpy(c.vendor,&b,4); memcpy(c.vendor+4,&d,4); memcpy(c.vendor+8,&e,4);
    if (max_basic >= 1U)
        __cpuid_count(1,0,c.regs.one[0],c.regs.one[1],c.regs.one[2],c.regs.one[3]);
    if (max_basic >= 7U) {
        __cpuid_count(7,0,c.regs.seven[0],c.regs.seven[1],c.regs.seven[2],c.regs.seven[3]);
        if (c.regs.seven[0] >= 1U)
            __cpuid_count(7,1,c.regs.seven_one[0],c.regs.seven_one[1],c.regs.seven_one[2],c.regs.seven_one[3]);
    }
    max_ext = __get_cpuid_max(UINT32_C(0x80000000),NULL);
    if (max_ext >= UINT32_C(0x80000001))
        __cpuid_count(0x80000001U,0,c.regs.extended[0],c.regs.extended[1],c.regs.extended[2],c.regs.extended[3]);
    if (max_ext >= UINT32_C(0x80000004)) {
        for (unsigned i=0;i<3U;++i) {
            unsigned regs[4];
            __cpuid_count(0x80000002U+i,0,regs[0],regs[1],regs[2],regs[3]);
            memcpy(c.brand+16U*i,regs,16);
        }
    }
    if ((c.regs.one[2] & ((1U<<26)|(1U<<27))) == ((1U<<26)|(1U<<27))) {
        unsigned lo,hi;
        __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        c.regs.xcr0 = ((uint64_t)hi<<32)|lo;
    }
    c.hardware = cpu_hardware_mask(&c.regs);
    c.usable = cpu_usable_mask(&c.regs,c.hardware);
    return c;
}

static void print_features(uint64_t mask)
{
    int any=0;
    for (unsigned i=0;i<UEEC_X86_FEATURE_COUNT;++i) {
        if ((mask & (UINT64_C(1)<<i)) != 0U) {
            printf("%s%s",any ? "," : "",feature_names[i]); any=1;
        }
    }
    if (!any) printf("baseline");
}
#endif
