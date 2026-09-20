; UEEC v1 multi-architecture exit(0) container
; 11 nodes, one implicit shared HEADER and 11 CODE sections
; Wire metadata byte order: little endian

[bits 64]

%define UEEC_VERSION                 1
%define UEEC_ENDIAN_LITTLE           0
%define UEEC_NODE_COUNT              11
%define UEEC_NAME_COUNT              1

%define UEEC_MACHINE_TYPE_CPU        0
%define UEEC_SYSTEM_TYPE_GENERAL_OS  2
%define UEEC_SYSTEM_LINUX            2

%define UEEC_MACHINE_ARM32           0
%define UEEC_MACHINE_ARM64           1
%define UEEC_MACHINE_LOONGARCH64     2
%define UEEC_MACHINE_POWERPC32       3
%define UEEC_MACHINE_POWERPC64       4
%define UEEC_MACHINE_RISCV32         5
%define UEEC_MACHINE_RISCV64         6
%define UEEC_MACHINE_SPARC32         7
%define UEEC_MACHINE_SPARC64         8
%define UEEC_MACHINE_X86             9
%define UEEC_MACHINE_X86_64          10

%define UEEC_FORMAT_SH16             0
%define UEEC_SECTION_HEADER          0
%define UEEC_SECTION_CODE            1

%define UEEC_NODE_USER_MODE          (1 << 2)
%define UEEC_NODE_PROGRAM            (1 << 3)
%define UEEC_NODE_CONSOLE            (1 << 5)
%define UEEC_NODE_COMMON_FLAGS       (UEEC_NODE_USER_MODE | UEEC_NODE_PROGRAM | UEEC_NODE_CONSOLE)

; Each node owns HEADER + one CODE section. Stored count is real count - 1.
%define UEEC_NODE_SECTION_COUNT      1

%macro UEEC_NODE_HEADER 2
    ; %1 machine, %2 code endianness
    dd ((UEEC_NODE_SECTION_COUNT & 0x1F) << 27) | \
       ((UEEC_SYSTEM_LINUX & 0x7F) << 20) | \
       ((UEEC_SYSTEM_TYPE_GENERAL_OS & 0x03) << 18) | \
       ((%1 & 0xFF) << 10) | \
       ((UEEC_MACHINE_TYPE_CPU & 0x07) << 7) | \
       UEEC_NODE_COMMON_FLAGS | (%2 & 0x01)
    dw UEEC_FORMAT_SH16
%endmacro

%macro UEEC_NODE_INFO 2
    ; uint16_t node; uint32_t global section
    dw %1
    dd %2
%endmacro

%macro UEEC_SH16 3
    ; uint16_t type, section_size, data_size
    dw %1
    dw %2
    dw %3
%endmacro

header_start:

; ueec_hdr: 8 bytes
db '_GM'
db ((UEEC_ENDIAN_LITTLE & 1) << 7) | (UEEC_VERSION & 0x7F)
dw UEEC_NODE_COUNT
dw UEEC_NAME_COUNT

; ueec_node_hdr[11]: 6 bytes each
UEEC_NODE_HEADER UEEC_MACHINE_ARM32,       0
UEEC_NODE_HEADER UEEC_MACHINE_ARM64,       0
UEEC_NODE_HEADER UEEC_MACHINE_LOONGARCH64, 0
UEEC_NODE_HEADER UEEC_MACHINE_POWERPC32,   1
UEEC_NODE_HEADER UEEC_MACHINE_POWERPC64,   1
UEEC_NODE_HEADER UEEC_MACHINE_RISCV32,     0
UEEC_NODE_HEADER UEEC_MACHINE_RISCV64,     0
UEEC_NODE_HEADER UEEC_MACHINE_SPARC32,     1
UEEC_NODE_HEADER UEEC_MACHINE_SPARC64,     1
UEEC_NODE_HEADER UEEC_MACHINE_X86,         0
UEEC_NODE_HEADER UEEC_MACHINE_X86_64,      0

; ueec_node_info[11]. HEADER membership is implicit; only CODE is mapped.
UEEC_NODE_INFO 0,  1
UEEC_NODE_INFO 1,  2
UEEC_NODE_INFO 2,  3
UEEC_NODE_INFO 3,  4
UEEC_NODE_INFO 4,  5
UEEC_NODE_INFO 5,  6
UEEC_NODE_INFO 6,  7
UEEC_NODE_INFO 7,  8
UEEC_NODE_INFO 8,  9
UEEC_NODE_INFO 9,  10
UEEC_NODE_INFO 10, 11

; ueec_sh16[12]. Descriptor order equals physical section order.
UEEC_SH16 UEEC_SECTION_HEADER, header_end - header_start, header_data_end - header_start
UEEC_SH16 UEEC_SECTION_CODE, code_arm32_end - code_arm32,             code_arm32_end - code_arm32
UEEC_SH16 UEEC_SECTION_CODE, code_arm64_end - code_arm64,             code_arm64_end - code_arm64
UEEC_SH16 UEEC_SECTION_CODE, code_loongarch64_end - code_loongarch64, code_loongarch64_end - code_loongarch64
UEEC_SH16 UEEC_SECTION_CODE, code_powerpc32_end - code_powerpc32,     code_powerpc32_end - code_powerpc32
UEEC_SH16 UEEC_SECTION_CODE, code_powerpc64_end - code_powerpc64,     code_powerpc64_end - code_powerpc64
UEEC_SH16 UEEC_SECTION_CODE, code_riscv32_end - code_riscv32,         code_riscv32_end - code_riscv32
UEEC_SH16 UEEC_SECTION_CODE, code_riscv64_end - code_riscv64,         code_riscv64_end - code_riscv64
UEEC_SH16 UEEC_SECTION_CODE, code_sparc32_end - code_sparc32,         code_sparc32_end - code_sparc32
UEEC_SH16 UEEC_SECTION_CODE, code_sparc64_end - code_sparc64,         code_sparc64_end - code_sparc64
UEEC_SH16 UEEC_SECTION_CODE, code_x86_end - code_x86,                 code_x86_end - code_x86
UEEC_SH16 UEEC_SECTION_CODE, code_x86_64_end - code_x86_64,           code_x86_64_end - code_x86_64

; One unique logical name is shared by all architecture variants.
db node_name_end - node_name

; node_name_index[11]
times UEEC_NODE_COUNT dw 0

node_name: db 'exit'
node_name_end:

header_data_end:
; No optimization padding in this baseline file.
header_end:

; CODE sections[11]
code_arm32:
db 0x01, 0x70, 0xA0, 0xE3, 0x00, 0x00, 0xA0, 0xE3, 0x00, 0x00, 0x00, 0xEF
code_arm32_end:

code_arm64:
db 0xA8, 0x0B, 0x80, 0xD2, 0x00, 0x00, 0x80, 0xD2, 0x01, 0x00, 0x00, 0xD4
code_arm64_end:

code_loongarch64:
db 0x0B, 0x74, 0xC1, 0x02, 0x04, 0x00, 0xC0, 0x02, 0x00, 0x00, 0x2B, 0x00
code_loongarch64_end:

code_powerpc32:
db 0x38, 0x00, 0x00, 0x01, 0x38, 0x60, 0x00, 0x00, 0x44, 0x00, 0x00, 0x02
code_powerpc32_end:

code_powerpc64:
db 0x38, 0x00, 0x00, 0x01, 0x38, 0x60, 0x00, 0x00, 0x44, 0x00, 0x00, 0x02
code_powerpc64_end:

code_riscv32:
db 0x93, 0x08, 0xD0, 0x05, 0x13, 0x05, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00
code_riscv32_end:

code_riscv64:
db 0x93, 0x08, 0xD0, 0x05, 0x13, 0x05, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00
code_riscv64_end:

code_sparc32:
db 0x82, 0x10, 0x20, 0x01, 0x90, 0x10, 0x20, 0x00, 0x91, 0xD0, 0x20, 0x10
code_sparc32_end:

code_sparc64:
db 0x82, 0x10, 0x20, 0x01, 0x90, 0x10, 0x20, 0x00, 0x91, 0xD0, 0x20, 0x6D
code_sparc64_end:

code_x86:
db 0xB8, 0x01, 0x00, 0x00, 0x00, 0xBB, 0x00, 0x00, 0x00, 0x00, 0xCD, 0x80
code_x86_end:

code_x86_64:
db 0xB8, 0x3C, 0x00, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x05
code_x86_64_end:
