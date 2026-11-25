/* Four implementations of uint32_t sum(const uint32_t *, size_t).
 * SysV AMD64 ABI. No globals, syscalls, external calls or relocations.
 * Addition wraps modulo 2^32 in every variant.
 */
.intel_syntax noprefix
.section .text.ueec_sum,"ax",@progbits
.global ueec_sum
.type ueec_sum,@function
ueec_sum:
    xor eax, eax
#if UEEC_VARIANT == 0
    jmp .Ltail
#else
    sub rsp, 64
#if UEEC_VARIANT == 1
    pxor xmm0, xmm0
#define LANES 4
#define STRIDE 16
#elif UEEC_VARIANT == 2
    vpxor ymm0, ymm0, ymm0
#define LANES 8
#define STRIDE 32
#elif UEEC_VARIANT == 3
    vpxord zmm0, zmm0, zmm0
#define LANES 16
#define STRIDE 64
#else
#error Invalid UEEC_VARIANT
#endif
    cmp rsi, LANES
    jb .Lreduce
.Lvector:
#if UEEC_VARIANT == 1
    movdqu xmm1, XMMWORD PTR [rdi]
    paddd xmm0, xmm1
#elif UEEC_VARIANT == 2
    vmovdqu ymm1, YMMWORD PTR [rdi]
    vpaddd ymm0, ymm0, ymm1
#else
    vmovdqu32 zmm1, ZMMWORD PTR [rdi]
    vpaddd zmm0, zmm0, zmm1
#endif
    add rdi, STRIDE
    sub rsi, LANES
    cmp rsi, LANES
    jae .Lvector
.Lreduce:
#if UEEC_VARIANT == 1
    movdqu XMMWORD PTR [rsp], xmm0
#elif UEEC_VARIANT == 2
    vmovdqu YMMWORD PTR [rsp], ymm0
#else
    vmovdqu32 ZMMWORD PTR [rsp], zmm0
#endif
    mov ecx, LANES
    mov rdx, rsp
.Llanes:
    add eax, DWORD PTR [rdx]
    add rdx, 4
    dec ecx
    jne .Llanes
    add rsp, 64
#if UEEC_VARIANT >= 2
    vzeroupper
#endif
#endif
.Ltail:
    test rsi, rsi
    je .Ldone
.Lscalar:
    add eax, DWORD PTR [rdi]
    add rdi, 4
    dec rsi
    jne .Lscalar
.Ldone:
    ret
.size ueec_sum, .-ueec_sum
.section .note.GNU-stack,"",@progbits
