BITS 64

%define UEF_MAGIC        0x6575
%define UEF_SH_HEADER    0
%define UEF_SH_CODE      1
%define UEF_SH_FORMAT_16 0

%define CMD_CALL 6
%define FORMAT_16 1

%define INS(cmd, fmt) (((cmd) & 0x07FF) | (((fmt) & 0x1F) << 11))
%define SH16(type, size) (((type) & 0xF) | (((size) & 0xFFF) << 4))

%define UEF_HDR_VALUE ( \
      UEF_MAGIC \
    | (1 << 43) \
    | (1 << 45) \
    | (1 << 46) \
    | (0 << 48) \
    | (UEF_SH_FORMAT_16 << 53) )

uef_begin:
    dq UEF_HDR_VALUE

    dw SH16(0, header_end - uef_begin)
    dw SH16(1, code_end - code_begin)

header_end:

code_begin:
recurse:
    dw INS(CMD_CALL, FORMAT_16)
    dw (recurse - recurse_next) / 2
recurse_next:
code_end:
