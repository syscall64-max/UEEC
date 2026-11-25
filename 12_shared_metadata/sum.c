/* Relocation-free leaf function. The same unsigned sum in every ISA. */
#include <stddef.h>
#include <stdint.h>

#if defined(UEEC_SVE)
#include <arm_sve.h>
#elif defined(UEEC_RVV)
#include <riscv_vector.h>
#endif

__attribute__((noinline, used, section(".text.ueec_sum")))
uint32_t ueec_sum(const uint32_t *p, size_t n)
{
    uint32_t result = 0;
#if defined(UEEC_SVE)
    while (n) {
        svbool_t active = svwhilelt_b32((uint64_t)0, (uint64_t)n);
        svuint32_t value = svld1_u32(active, p);
        result += (uint32_t)svaddv_u32(active, value);
        size_t step = svcntw();
        if (step >= n) break;
        p += step;
        n -= step;
    }
#elif defined(UEEC_RVV)
    while (n) {
        size_t vl = __riscv_vsetvl_e32m1(n);
        vuint32m1_t value = __riscv_vle32_v_u32m1(p, vl);
        vuint32m1_t zero = __riscv_vmv_v_x_u32m1(0, 1);
        vuint32m1_t total = __riscv_vredsum_vs_u32m1_u32m1(value, zero, vl);
        result += __riscv_vmv_x_s_u32m1_u32(total);
        p += vl;
        n -= vl;
    }
#elif defined(UEEC_VECTOR_BYTES)
    /* Peel to the actual vector alignment. Never read before/after the array.
       This also supports PPC lvx and SPARC ldd, which require alignment. */
    typedef uint32_t vector_t __attribute__((vector_size(UEEC_VECTOR_BYTES), may_alias));
    enum { lanes = UEEC_VECTOR_BYTES / 4 };
    vector_t acc = {0};
    while (n && ((uintptr_t)p & (UEEC_VECTOR_BYTES - 1U))) {
        result += *p++;
        --n;
    }
    while (n >= (size_t)lanes) {
#if defined(UEEC_VIS)
        typedef int vis_t __attribute__((vector_size(8)));
        acc = (vector_t)__builtin_vis_fpadd32((vis_t)acc,
                                             (vis_t)*(const vector_t *)p);
#else
        acc += *(const vector_t *)p;
#endif
        p += lanes;
        n -= (size_t)lanes;
    }
    for (size_t i = 0; i < (size_t)lanes; ++i) result += acc[i];
    while (n--) result += *p++;
#else
    while (n--) result += *p++;
#endif
    return result;
}
