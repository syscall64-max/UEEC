.text
/* int entry(const unsigned char *ro, unsigned char *rw, unsigned char *bss)
   Return ro[0]+rw[0]+bss[0]+VALUE; store result modulo 256 in both writable sections. */
#if defined(__x86_64__)
 movzbl (%rdi), %eax
 movzbl (%rsi), %ecx
 add %ecx, %eax
 movzbl (%rdx), %ecx
 add %ecx, %eax
 add $VALUE, %eax
 mov %al,(%rsi)
 mov %al,(%rdx)
 ret
#elif defined(__i386__)
 mov 4(%esp),%ecx
 movzbl (%ecx),%eax
 mov 8(%esp),%edx
 movzbl (%edx),%ecx
 add %ecx,%eax
 mov 12(%esp),%ecx
 push %ebx
 movzbl (%ecx),%ebx
 add %ebx,%eax
 pop %ebx
 add $VALUE,%eax
 mov %al,(%edx)
 mov %al,(%ecx)
 ret
#elif defined(__aarch64__)
 ldrb w3,[x0]
 ldrb w4,[x1]
 add w3,w3,w4
 ldrb w4,[x2]
 add w3,w3,w4
 add w0,w3,#VALUE
 strb w0,[x1]
 strb w0,[x2]
 ret
#elif defined(__arm__)
 .syntax unified
 .arm
 ldrb r3,[r0]
 ldrb r0,[r1]
 add r3,r3,r0
 ldrb r0,[r2]
 add r3,r3,r0
 add r0,r3,#VALUE
 strb r0,[r1]
 strb r0,[r2]
 bx lr
#elif defined(__riscv)
 lbu t0,0(a0)
 lbu t1,0(a1)
 add t0,t0,t1
 lbu t1,0(a2)
 add t0,t0,t1
 addi a0,t0,VALUE
 sb a0,0(a1)
 sb a0,0(a2)
 ret
#elif defined(__powerpc__)
 lbz 6,0(3)
 lbz 7,0(4)
 add 6,6,7
 lbz 7,0(5)
 add 6,6,7
 addi 3,6,VALUE
 stb 3,0(4)
 stb 3,0(5)
 blr
#elif defined(__sparc__)
 ldub [%o0],%o3
 ldub [%o1],%o4
 add %o3,%o4,%o3
 ldub [%o2],%o4
 add %o3,%o4,%o3
 add %o3,VALUE,%o0
 stb %o0,[%o1]
 retl
 stb %o0,[%o2]
#elif defined(__loongarch__)
 ld.bu $t0,$a0,0
 ld.bu $t1,$a1,0
 add.w $t0,$t0,$t1
 ld.bu $t1,$a2,0
 add.w $t0,$t0,$t1
 addi.w $a0,$t0,VALUE
 st.b $a0,$a1,0
 st.b $a0,$a2,0
 jirl $zero,$ra,0
#elif defined(__s390x__)
 llgc %r0,0(%r2)
 llgc %r1,0(%r3)
 ar %r0,%r1
 llgc %r1,0(%r4)
 ar %r0,%r1
 ahi %r0,VALUE
 stc %r0,0(%r3)
 stc %r0,0(%r4)
 lgfr %r2,%r0
 br %r14
#else
#error Unsupported architecture
#endif
