; IBM Z / Linux s390x (64-bit, big-endian instruction stream)
; Raw machine code, matching the user-verified objcopy output:
;   lghi %r2,0       -> A7 29 00 00    (4 bytes)
;   svc 1            -> 0A 01          (2 bytes, __NR_exit = 1)
;   nopr %r7         -> 07 07          (2 bytes, assembler alignment NOP)
; This file uses NASM only as a byte emitter; these are NOT x86 instructions.
[bits 64]

db 0xA7, 0x29, 0x00, 0x00   ; lghi %r2, 0

db 0x0A, 0x01               ; svc 1 (Linux exit)

db 0x07, 0x07               ; nopr %r7 (not executed after exit)
