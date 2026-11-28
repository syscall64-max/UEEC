.text
#if defined(__i386__) || defined(__x86_64__)
 mov $VALUE, %eax
 ret
#elif defined(__aarch64__)
 mov w0, #VALUE
 ret
#elif defined(__arm__)
 .syntax unified
 .arm
 mov r0, #VALUE
 bx lr
#elif defined(__riscv)
 li a0, VALUE
 ret
#elif defined(__powerpc__)
 li 3, VALUE
 blr
#elif defined(__sparc__)
 retl
 mov VALUE, %o0
#elif defined(__loongarch__)
 addi.w $a0, $zero, VALUE
 jirl $zero, $ra, 0
#elif defined(__s390x__)
 lghi %r2, VALUE
 br %r14
#else
#error Unsupported architecture
#endif
