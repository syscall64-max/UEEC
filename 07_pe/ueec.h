#ifndef UEEC_H
#define UEEC_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
# define UEEC_PACKED __attribute__((packed))
#else
# error "UEEC requires GCC or Clang packed-structure support"
#endif

#define UEEC_ENDIAN_LITTLE 0u
#define UEEC_ENDIAN_BIG    1u

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
# define UEEC_HOST_ENDIAN UEEC_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
# define UEEC_HOST_ENDIAN UEEC_ENDIAN_BIG
#else
# error "Unsupported byte order"
#endif

#define UEEC_MAGIC_SIZE 3u
#define UEEC_MAGIC_0 UINT8_C(0x5f)
#define UEEC_MAGIC_1 UINT8_C(0x47)
#define UEEC_MAGIC_2 UINT8_C(0x4d)
#define UEEC_VERSION_1 UINT8_C(1)

#define UEEC_MACHINE_TYPE_CPU UINT8_C(0)
#define UEEC_SYSTEM_TYPE_GENERAL_OS UINT8_C(2)
#define UEEC_SYSTEM_LINUX UINT8_C(2)

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

#define UEEC_FORMAT_SH16 UINT16_C(0)
#define UEEC_FORMAT_SH32 UINT16_C(1)
#define UEEC_FORMAT_SH64 UINT16_C(2)

#define UEEC_SECTION_HEADER        UINT16_C(0)
#define UEEC_SECTION_CODE          UINT16_C(1)
#define UEEC_SECTION_INIT_DATA     UINT16_C(2)
#define UEEC_SECTION_UNINIT_DATA   UINT16_C(3)
#define UEEC_SECTION_CONSTANT_DATA UINT16_C(4)
#define UEEC_SECTION_RELOCATION    UINT16_C(5)
#define UEEC_SECTION_EXPORT        UINT16_C(6)
#define UEEC_SECTION_IMPORT        UINT16_C(7)

#define UEEC_HEADER_SECTION_INDEX UINT32_C(0)
#define UEEC_MAX_NODE_SECTIONS UINT8_C(32)

typedef struct UEEC_PACKED {
    uint8_t magic[UEEC_MAGIC_SIZE];
#if UEEC_HOST_ENDIAN == UEEC_ENDIAN_LITTLE
    uint8_t version : 7, endian : 1;
#else
    uint8_t endian : 1, version : 7;
#endif
    uint16_t nodes;
    uint16_t names;
} ueec_hdr;

typedef struct UEEC_PACKED {
#if UEEC_HOST_ENDIAN == UEEC_ENDIAN_LITTLE
    uint32_t encode : 1, kernel_mode : 1, user_mode : 1, program : 1,
             library : 1, console : 1, service : 1, machine_type : 3,
             machine : 8, system_type : 2, system : 7, section_count : 5;
#else
    uint32_t section_count : 5, system : 7, system_type : 2, machine : 8,
             machine_type : 3, service : 1, console : 1, library : 1,
             program : 1, user_mode : 1, kernel_mode : 1, encode : 1;
#endif
    uint16_t format;
} ueec_node_hdr;

typedef struct UEEC_PACKED {
    uint16_t node;
    uint32_t section;
} ueec_node_info;

typedef uint8_t ueec_name_len;
typedef uint16_t ueec_node_name_index;

typedef struct UEEC_PACKED {
    uint16_t type, section_size, data_size;
} ueec_sh16;

typedef struct UEEC_PACKED {
    uint16_t type;
    uint32_t section_size, data_size;
} ueec_sh32;

typedef struct UEEC_PACKED {
    uint16_t type;
    uint64_t section_size, data_size;
} ueec_sh64;

_Static_assert(sizeof(ueec_hdr) == 8, "ueec_hdr must be 8 bytes");
_Static_assert(sizeof(ueec_node_hdr) == 6, "ueec_node_hdr must be 6 bytes");
_Static_assert(sizeof(ueec_node_info) == 6, "ueec_node_info must be 6 bytes");
_Static_assert(sizeof(ueec_sh16) == 6, "ueec_sh16 must be 6 bytes");
_Static_assert(sizeof(ueec_sh32) == 10, "ueec_sh32 must be 10 bytes");
_Static_assert(sizeof(ueec_sh64) == 18, "ueec_sh64 must be 18 bytes");

#endif
