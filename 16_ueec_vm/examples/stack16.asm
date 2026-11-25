; UEEC VM CODE only: operand formats 16/32/64; no container header.
BITS 64
%define CMD_SYSCALL          0
%define CMD_MOV_INDEX_INDEX  1
%define CMD_MOV_INDEX_IMM    2
%define CMD_PUSH_INDEX       3
%define CMD_PUSH_IMM         4
%define CMD_POP_INDEX        5
%define CMD_CALL             6
%define CMD_RET              7
%define FORMAT_16 1
%define FORMAT_32 2
%define FORMAT_64 3
%define REG_INDEX 0
%define IDATA_INDEX 1
%define STACK_INDEX 2
%define HEAP_INDEX 3
%define INS(cmd, fmt) (((cmd) & 0x07FF) | (((fmt) & 0x1F) << 11))
%define INDEX16(type, index) (((index) & 0x3FFF) | (((type) & 3) << 14))
code_begin:

    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 0x7A               ; CODE is 16-bit aligned

    dw INS(CMD_POP_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 9)               ; CODE alignment

    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 111

    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 222

call_function1:
    dw INS(CMD_CALL, FORMAT_16)
    dw (function1 - call_function1_next) / 2
call_function1_next:
    dw INS(CMD_MOV_INDEX_IMM, FORMAT_16)
    dw INDEX16(REG_INDEX, 1)
    dw 0

call_exit:
    dw INS(CMD_CALL, FORMAT_16)
    dw (sys_exit - call_exit_next) / 2
call_exit_next:
    dw INS(CMD_SYSCALL, 0)
    dw 0xFFFF

function1:

    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 4)
    dw INDEX16(STACK_INDEX, 0)

    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 5)
    dw INDEX16(STACK_INDEX, 1)
    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 0x55
    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 333
    dw INS(CMD_PUSH_IMM, FORMAT_32)
    dd 0x12345678

call_function2:
    dw INS(CMD_CALL, FORMAT_16)
    dw (function2 - call_function2_next) / 2
call_function2_next:
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 8)
    dw INDEX16(STACK_INDEX, 0)
    dw INS(CMD_RET, FORMAT_16)
    dw 2

function2:
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 6)
    dw INDEX16(STACK_INDEX, 0)
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 7)
    dw INDEX16(STACK_INDEX, 1)
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_32)
    dd ((REG_INDEX << 30) | 10)
    dd ((STACK_INDEX << 30) | 2)
    dw INS(CMD_RET, FORMAT_16)
    dw 4

sys_exit:
    dw INS(CMD_SYSCALL, 0)
    dw 0

code_end:
