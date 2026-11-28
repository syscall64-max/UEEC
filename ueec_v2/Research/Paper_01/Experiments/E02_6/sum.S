.intel_syntax noprefix
.global _start
.text
_start:
    movabs rsi, 0x1122334455667788
    movabs rdi, 0x2233445566778899
    movabs rdx, 0x33445566778899aa
    mov ecx, 0x12345678
    xor eax,eax
check:
    cmp byte ptr [rdx+rax],0
    jne fail
    inc eax
    cmp eax,ecx
    jb check
    xor eax,eax
    xor r8d,r8d
sum:
    movzx r9d,byte ptr [rsi+r8]
    add eax,r9d
    movzx r9d,byte ptr [rdi+r8]
    add eax,r9d
    inc r8d
    cmp r8d,16
    jb sum
    add byte ptr [rdi],5
    movzx r9d,byte ptr [rdi]
    cmp r9d,6
    jne fail
    add eax,5
    mov dword ptr [rdx],eax
    mov byte ptr [rdx+rcx-1],1
    cmp dword ptr [rdx],eax
    jne fail
    cmp byte ptr [rdx+rcx-1],1
    jne fail
    mov edi,dword ptr [rdx]
    mov eax,60
    syscall
fail:
    mov edi,255
    mov eax,60
    syscall
