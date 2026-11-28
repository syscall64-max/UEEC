.intel_syntax noprefix
.global _start
.text
_start:
    movabs rsi, 0x1122334455667788
    movzx eax, byte ptr [rsi]
    add eax, 5
    mov byte ptr [rsi], al
    cmp byte ptr [rsi], al
    jne fail
    xor edi, edi
    mov ecx, 16
again:
    movzx eax, byte ptr [rsi]
    add edi, eax
    inc rsi
    loop again
    mov eax, 60
    syscall
fail:
    mov edi, 255
    mov eax, 60
    syscall
