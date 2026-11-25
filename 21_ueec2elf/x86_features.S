/* The payload follows the end label immediately. A missing feature exits 126. */
.macro unsupported
    mov $60, %eax
    mov $126, %edi
    syscall
    ud2
.endm

.section .text.sse2,"ax",@progbits
    mov $1, %eax
    cpuid
    bt $26, %edx
    jc 1f
    unsupported
1:

.section .text.avx2,"ax",@progbits
    xor %eax, %eax
    cpuid
    cmp $7, %eax
    jb 1f
    mov $1, %eax
    cpuid
    and $0x1c000000, %ecx
    cmp $0x1c000000, %ecx
    jne 1f
    xor %ecx, %ecx
    xgetbv
    and $6, %eax
    cmp $6, %eax
    jne 1f
    mov $7, %eax
    xor %ecx, %ecx
    cpuid
    bt $5, %ebx
    jc 2f
1:
    unsupported
2:
