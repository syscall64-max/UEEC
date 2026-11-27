/* Raw LoongArch64 payload: no local symbol references, alignment directives,
 * or linker-dependent instructions. LP64 ABI, 16-byte aligned stack. */
.text
.globl payload
.type payload, @function
payload:
#if defined(DATA)
    ld.d $t0, $a0, 0
    ld.d $t1, $a0, 8
    ld.d $t2, $a0, 16
    ld.bu $t3, $t0, 0
    ld.bu $t4, $t1, 0
    ld.bu $t5, $t2, 0
    addi.w $t4, $t4, 10
    addi.w $t5, $t5, 42
    st.b $t4, $t1, 0
    st.b $t5, $t2, 0
    add.w $a0, $t3, $t4
    add.w $a0, $a0, $t5
#elif defined(APP)
    addi.d $sp, $sp, -32
    st.d $ra, $sp, 24
    st.d $s0, $sp, 16
    st.d $s1, $sp, 8
    or $s0, $a0, $zero
    ld.d $t0, $s0, 0
    addi.w $a0, $zero, 20
    addi.w $a1, $zero, 22
    jirl $ra, $t0, 0
    or $s1, $a0, $zero
    ld.d $t0, $s0, 8
    addi.w $a0, $zero, 7
    addi.w $a1, $zero, 5
    jirl $ra, $t0, 0
    add.w $s1, $s1, $a0
    ld.d $t0, $s0, 16
    addi.w $a0, $zero, 6
    addi.w $a1, $zero, 7
    jirl $ra, $t0, 0
    add.w $a0, $s1, $a0
    ld.d $s1, $sp, 8
    ld.d $s0, $sp, 16
    ld.d $ra, $sp, 24
    addi.d $sp, $sp, 32
#elif defined(ADD)
    add.w $a0, $a0, $a1
#elif defined(MUL)
    mul.w $a0, $a0, $a1
#else
#error missing payload variant
#endif
    jirl $zero, $ra, 0
.size payload, .-payload
.section .note.GNU-stack,"",@progbits
