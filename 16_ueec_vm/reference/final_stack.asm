BITS 64

; ============================================================
; UEF basic constants
; ============================================================

%define UEF_MAGIC        0x6575
%define UEF_SH_HEADER    0
%define UEF_SH_CODE      1
%define UEF_SH_DATA      2
%define UEF_SH_FORMAT_16 0

; ============================================================
; UEF_VM commands
; ============================================================

%define CMD_SYSCALL          0
%define CMD_MOV_INDEX_INDEX  1
%define CMD_MOV_INDEX_IMM    2
%define CMD_PUSH_INDEX       3
%define CMD_PUSH_IMM         4
%define CMD_POP_INDEX        5
%define CMD_CALL             6
%define CMD_RET              7

; value formats
%define FORMAT_8   0
%define FORMAT_16  1
%define FORMAT_32  2
%define FORMAT_64  3

; index spaces
%define REG_INDEX    0
%define IDATA_INDEX  1
%define STACK_INDEX  2
%define HEAP_INDEX   3

%define INS(cmd, fmt) (((cmd) & 0x07FF) | (((fmt) & 0x1F) << 11))

; FORMAT16 index:
; bits 0..13 = index
; bits 14..15 = type
%define INDEX16(type, index) (((index) & 0x3FFF) | (((type) & 3) << 14))

%define SH16(type, size) (((type) & 0xF) | (((size) & 0xFFF) << 4))

; HEADER + CODE + DATA => actual sections = 3 => hdr.sections = 1
%define UEF_HDR_VALUE ( \
      UEF_MAGIC                 \
    | (1 << 43)                 \
    | (1 << 45)                 \
    | (1 << 46)                 \
    | (1 << 48)                 \
    | (UEF_SH_FORMAT_16 << 53) )

uef_begin:
    dq UEF_HDR_VALUE

section_table:
    dw SH16(UEF_SH_HEADER, header_end - uef_begin)
    dw SH16(UEF_SH_CODE,   code_end - code_begin)
    dw SH16(UEF_SH_DATA,   data_end - data_begin)

header_end:

; ============================================================
; CODE
; ============================================================

code_begin:

    ; --------------------------------------------------------
    ; test physical stack cells
    ;
    ; PUSH8 consumes 1 physical 16-bit cell.
    ; 0x7A is stored as 0x007A.
    ; --------------------------------------------------------

    dw INS(CMD_PUSH_IMM, FORMAT_8)
    db 0x7A
    db 0x00               ; CODE is 16-bit aligned

    dw INS(CMD_POP_INDEX, FORMAT_8)
    db ((REG_INDEX << 6) | 9)
    db 0x00               ; CODE alignment

    ; --------------------------------------------------------
    ; function1(111, 222)
    ; --------------------------------------------------------

    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 111

    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 222

call_function1:
    dw INS(CMD_CALL, FORMAT_16)
    dw (function1 - call_function1_next) / 2
call_function1_next:

    ; after RET16 2, caller stack must be empty

    ; sys_exit(0)
    dw INS(CMD_MOV_INDEX_IMM, FORMAT_16)
    dw INDEX16(REG_INDEX, 1)
    dw 0

call_exit:
    dw INS(CMD_CALL, FORMAT_16)
    dw (sys_exit - call_exit_next) / 2
call_exit_next:

    ; should never execute
    dw INS(CMD_SYSCALL, 0)
    dw 0xFFFF


; ============================================================
; function1
;
; STACK_INDEX[0] = 111
; STACK_INDEX[1] = 222
;
; nested call:
;   function2(0x55, 333, 0x12345678)
; ============================================================

function1:

    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 4)
    dw INDEX16(STACK_INDEX, 0)

    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 5)
    dw INDEX16(STACK_INDEX, 1)

    ; argument 0: FORMAT8 => physical 1 block, 0x0055
    dw INS(CMD_PUSH_IMM, FORMAT_8)
    db 0x55
    db 0x00

    ; argument 1: FORMAT16 => 1 block
    dw INS(CMD_PUSH_IMM, FORMAT_16)
    dw 333

    ; argument 2: FORMAT32 => 2 blocks
    dw INS(CMD_PUSH_IMM, FORMAT_32)
    dd 0x12345678

call_function2:
    dw INS(CMD_CALL, FORMAT_16)
    dw (function2 - call_function2_next) / 2
call_function2_next:

    ; BP must be restored to function1 parameters
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 8)
    dw INDEX16(STACK_INDEX, 0)

    ; cleanup own 2 incoming argument blocks
    dw INS(CMD_RET, FORMAT_16)
    dw 2


; ============================================================
; function2
;
; arg0 = FORMAT8,  physical 1 block
; arg1 = FORMAT16, physical 1 block
; arg2 = FORMAT32, physical 2 blocks
;
; STACK_INDEX is a 16-bit BLOCK index.
; Therefore:
;   arg0 begins at STACK_INDEX[0]
;   arg1 begins at STACK_INDEX[1]
;   arg2 begins at STACK_INDEX[2] and spans blocks 2..3
; ============================================================

function2:

    ; MOV8 r6, STACK_INDEX[0]
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_8)
    db ((REG_INDEX << 6) | 6)
    db ((STACK_INDEX << 6) | 0)

    ; MOV16 r7, STACK_INDEX[1]
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_16)
    dw INDEX16(REG_INDEX, 7)
    dw INDEX16(STACK_INDEX, 1)

    ; MOV32 r10, STACK_INDEX[2]
    dw INS(CMD_MOV_INDEX_INDEX, FORMAT_32)
    dd ((REG_INDEX << 30) | 10)
    dd ((STACK_INDEX << 30) | 2)

    ; 1 + 1 + 2 = 4 physical blocks
    dw INS(CMD_RET, FORMAT_16)
    dw 4


; ============================================================
; sys_exit
; ============================================================

sys_exit:
    dw INS(CMD_SYSCALL, 0)
    dw 0

code_end:

; ============================================================
; IDATA
; ============================================================

data_begin:
    dw 0
data_end:
