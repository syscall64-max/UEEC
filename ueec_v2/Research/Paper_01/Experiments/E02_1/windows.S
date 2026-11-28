.text
.globl start
start:
 mov $-1, %r10
 xor %edx, %edx
 mov $0x2c, %eax
 syscall
