//
#ifndef UEEC_H
#define UEEC_H
//
#include <stdint.h>
//
#if defined(_MSC_VER)
# define UEEC_PACKED
# pragma pack(push, 1)
#elif defined(__GNUC__) || defined(__clang__)
# define UEEC_PACKED __attribute__((packed))
#else
# error "UEEC: packed structure support is required"
#endif
//
#define UEEC_MAGIC_0 0x5f
#define UEEC_MAGIC_1 0x47
#define UEEC_MAGIC_2 0x4d
//
#define UEEC_ENDIAN_LITTLE 0
#define UEEC_ENDIAN_BIG    1
//
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define UEEC_HOST_ENDIAN UEEC_ENDIAN_BIG
#else
# define UEEC_HOST_ENDIAN UEEC_ENDIAN_LITTLE
#endif
//
#define UEEC_MACHINE_TYPE_CPU UINT8_C(0)
//
#define UEEC_MACHINE_ARM32       UINT8_C(0)
#define UEEC_MACHINE_ARM64       UINT8_C(1)
#define UEEC_MACHINE_LOONGARCH64 UINT8_C(2)
#define UEEC_MACHINE_POWERPC32   UINT8_C(3)
#define UEEC_MACHINE_POWERPC64   UINT8_C(4)
#define UEEC_MACHINE_RISCV32     UINT8_C(5)
#define UEEC_MACHINE_RISCV64     UINT8_C(6)
#define UEEC_MACHINE_SPARC32     UINT8_C(7)
#define UEEC_MACHINE_SPARC64     UINT8_C(8)
#define UEEC_MACHINE_X86         UINT8_C(9)
#define UEEC_MACHINE_X86_64      UINT8_C(10)
//
#define UEEC_SYSTEM_TYPE_GENERAL_OS UINT8_C(2)
#define UEEC_SYSTEM_LINUX           UINT8_C(2)
//
#define UEEC_SECTION_HEADER      0x00
#define UEEC_SECTION_CODE        0x01
#define UEEC_SECTION_DATA        0x02
#define UEEC_SECTION_RELOCATION  0x05
#define UEEC_SECTION_EXPORT      0x06
//
typedef struct UEEC_PACKED {
    uint8_t     magic0, magic1, magic2;
    uint8_t     version : 7, endian : 1;
    uint16_t    nodes;
    uint16_t    names;
} ueec_hdr, *pueec_hdr;
//
// ueec_node_hdr (bitfield, 4 bytes) + format (uint16_t) = 6 bytes:
//   bit0: encode, bit1: kernel_mode, bit2: user_mode, bit3: program,
//   bit4: library, bit5: console, bit6: service, bit7-9: machine_type,
//   bit10-17: machine, bit18-19: system_type, bit20-26: system,
//   bit27-31: section_count (field = real_count - 1)
//
typedef struct UEEC_PACKED {
    uint32_t    raw;
    uint16_t    format;
} ueec_node_hdr, *pueec_node_hdr;
//
typedef struct UEEC_PACKED {
    uint16_t    node;
    uint32_t    section;
} ueec_node_info, *pueec_node_info;
//
typedef struct UEEC_PACKED {
    uint16_t    type;
    uint16_t    section_size;
    uint16_t    data_size;
} ueec_sh16, *pueec_sh16;
//
#endif /* UEEC_H */
