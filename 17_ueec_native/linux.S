/* Freestanding Linux startup and system-call ABI for the experiment. */
.text
.globl _start
.globl linux_call
.globl enter_code

#if defined(__x86_64__)
_start:
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    and $-16, %rsp
    call run
    ud2
linux_call:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10
    mov %r9, %r8
    mov 8(%rsp), %r9
    syscall
    ret
enter_code:
    jmp *%rdi
#elif defined(__i386__)
_start:
    mov (%esp), %eax
    lea 4(%esp), %ecx
    and $-16, %esp
    sub $8, %esp
    push %ecx
    push %eax
    call run
    ud2
linux_call:
    push %ebp
    push %ebx
    push %esi
    push %edi
    mov 20(%esp), %eax
    mov 24(%esp), %ebx
    mov 28(%esp), %ecx
    mov 32(%esp), %edx
    mov 36(%esp), %esi
    mov 40(%esp), %edi
    mov 44(%esp), %ebp
    int $0x80
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret
enter_code:
    jmp *4(%esp)
#elif defined(__s390x__)
_start:
    lg %r2, 0(%r15)
    la %r3, 8(%r15)
    aghi %r15, -160
    brasl %r14, run
    .short 0
linux_call:
    /* The Linux ABI takes the syscall number in r1 and arguments in r2-r7. */
    stmg %r6, %r7, 48(%r15)
    lgr %r1, %r2
    lgr %r2, %r3
    lgr %r3, %r4
    lgr %r4, %r5
    lgr %r5, %r6
    lg %r6, 160(%r15)
    lg %r7, 168(%r15)
    svc 0
    lmg %r6, %r7, 48(%r15)
    br %r14
enter_code:
    br %r2
#elif defined(__arm__)
.syntax unified
.arm
_start:
    ldr r0, [sp]
    add r1, sp, #4
    bl run
    udf #0
linux_call:
    push {r4-r7,lr}
    mov r7, r0
    mov r0, r1
    mov r1, r2
    mov r2, r3
    ldr r3, [sp, #20]
    ldr r4, [sp, #24]
    ldr r5, [sp, #28]
    svc #0
    pop {r4-r7,pc}
enter_code:
    bx r0
#elif defined(__aarch64__)
_start:
    ldr x0, [sp]
    add x1, sp, #8
    bl run
    brk #0
linux_call:
    mov x8, x0
    mov x0, x1
    mov x1, x2
    mov x2, x3
    mov x3, x4
    mov x4, x5
    mov x5, x6
    svc #0
    ret
enter_code:
    br x0
#elif defined(__riscv)
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
#if __riscv_xlen == 64
    ld a0, 0(sp)
    addi a1, sp, 8
#else
    lw a0, 0(sp)
    addi a1, sp, 4
#endif
    call run
    unimp
linux_call:
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    mv a3, a4
    mv a4, a5
    mv a5, a6
    ecall
    ret
enter_code:
    jr a0
#elif defined(__loongarch__)
_start:
    ld.d $a0, $sp, 0
    addi.d $a1, $sp, 8
    bl run
    break 0
linux_call:
    move $a7, $a0
    move $a0, $a1
    move $a1, $a2
    move $a2, $a3
    move $a3, $a4
    move $a4, $a5
    move $a5, $a6
    syscall 0
    ret
enter_code:
    jr $a0
#elif defined(__powerpc__)
#if defined(__powerpc64__)
.abiversion 2
#endif
_start:
#if defined(__powerpc64__)
    lis 2, .TOC.@ha
    addi 2, 2, .TOC.@l
    ld 3, 0(1)
    addi 4, 1, 8
    li 0, 0
    stdu 0, -32(1)
#else
    lwz 3, 0(1)
    addi 4, 1, 4
    li 0, 0
    stwu 0, -32(1)
#endif
    bl run
    nop
    trap
linux_call:
    mr 0, 3
    mr 3, 4
    mr 4, 5
    mr 5, 6
    mr 6, 7
    mr 7, 8
    mr 8, 9
    sc
    bns 1f
    neg 3, 3
1:  blr
enter_code:
    mtctr 3
    bctr
#elif defined(__sparc__)
/* Linux starts SPARC with a register-window save area below argc. */
#if __SIZEOF_POINTER__ == 8
#define ARGBASE 2175
#define PTR_BYTES 8
#define LOAD ldx
#define TRAPNO 0x6d
#define FRAME 176
#else
#define ARGBASE 64
#define PTR_BYTES 4
#define LOAD ld
#define TRAPNO 0x10
#define FRAME 96
#endif
_start:
    mov 0, %fp
    LOAD [%sp + ARGBASE], %o0
    add %sp, ARGBASE + PTR_BYTES, %o1
    sub %sp, FRAME, %sp
    call run
     nop
    unimp 0
linux_call:
    /* Six arguments after the system-call number. The final argument is
       always zero for this experiment (offset in the mmap call). */
    mov %o0, %g1
    mov %o1, %o0
    mov %o2, %o1
    mov %o3, %o2
    mov %o4, %o3
    mov %o5, %o4
    clr %o5
    ta TRAPNO
#if __SIZEOF_POINTER__ == 8
    bcc %xcc, 1f
#else
    bcc 1f
#endif
     nop
    neg %o0
1:  retl
     nop
enter_code:
    jmp %o0
     nop
#endif

#if defined(__arm__)
.section .note.GNU-stack,"",%progbits
#else
.section .note.GNU-stack,"",@progbits
#endif
