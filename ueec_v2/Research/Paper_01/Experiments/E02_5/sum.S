.intel_syntax noprefix
.global _start
.text
_start:
    movabs rsi, 0x1122334455667788
    mov ecx, 0x12345678
    xor eax,eax
check:
    cmp byte ptr [rsi+rax],0
    jne fail
    inc eax
    cmp eax,ecx
    jb check
    mov byte ptr [rsi],42
    mov byte ptr [rsi+rcx-1],1
    movzx edi,byte ptr [rsi]
    movzx eax,byte ptr [rsi+rcx-1]
    add edi,eax
    mov eax,60
    syscall
fail:
    mov edi,255
    mov eax,60
    syscall
