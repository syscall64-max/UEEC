.intel_syntax noprefix
.global _start
.text
_start:
    movabs rsi, 0x1122334455667788
    xor edi, edi
    mov ecx, 16
again:
    movzx eax, byte ptr [rsi]
    add edi, eax
    inc rsi
    loop again
    mov eax, 60
    syscall
