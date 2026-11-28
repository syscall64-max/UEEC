/* Raw PPC64 ELFv1 payload. No function descriptors or TOC references.
 * All three entry points begin at the first byte of .text. */
.text
.balign 4
.globl payload
.type payload,@function
payload:
#if defined(APP)
    mflr 0
    std 0,16(1)
    stdu 1,-128(1)
    std 30,112(1)
    std 31,120(1)
    std 2,40(1)
    mr 30,3
    ld 12,0(30)
    mtctr 12
    li 3,20
    li 4,22
    bctrl
    ld 2,40(1)
    mr 31,3
    ld 12,8(30)
    mtctr 12
    li 3,7
    li 4,5
    bctrl
    ld 2,40(1)
    add 31,31,3
    ld 12,16(30)
    mtctr 12
    li 3,6
    li 4,7
    bctrl
    ld 2,40(1)
    add 3,31,3
    extsw 3,3
    ld 30,112(1)
    ld 31,120(1)
    addi 1,1,128
    ld 0,16(1)
    mtlr 0
#elif defined(ADD)
    add 3,3,4
    extsw 3,3
#elif defined(MUL)
    mullw 3,3,4
    extsw 3,3
#else
#error missing payload variant
#endif
    blr
.size payload,.-payload
.section .note.GNU-stack,"",@progbits
