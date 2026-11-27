#ifndef UEEC_H
#define UEEC_H

#include <stdint.h>

/*
 * UEEC wire-format definitions.
 *
 * The bit-field declarations below follow the GCC/Clang ABI used by the
 * supported Linux targets. Multi-byte values must be converted according to
 * ueec_hdr.endian before they are used on a host with the opposite byte order.
 */

#if defined(_MSC_VER)
# define UEEC_PACKED
# pragma pack(push, 1)
#elif defined(__GNUC__) || defined(__clang__)
# define UEEC_PACKED __attribute__((packed))
#else
# error "UEEC: packed structure support is required"
#endif

#define UEEC_ENDIAN_LITTLE 0u
#define UEEC_ENDIAN_BIG    1u

#if defined(_WIN32)
# define UEEC_HOST_ENDIAN UEEC_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
# define UEEC_HOST_ENDIAN UEEC_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
# define UEEC_HOST_ENDIAN UEEC_ENDIAN_BIG
#else
# error "UEEC: unsupported or unknown host byte order"
#endif

/* The byte signature at file offset zero is exactly: 5f 47 4d ("_GM"). */
#define UEEC_MAGIC_SIZE 3u
#define UEEC_MAGIC_0    UINT8_C(0x5f)
#define UEEC_MAGIC_1    UINT8_C(0x47)
#define UEEC_MAGIC_2    UINT8_C(0x4d)
#define UEEC_MAGIC_INIT { UEEC_MAGIC_0, UEEC_MAGIC_1, UEEC_MAGIC_2 }

#define UEEC_VERSION_1 UINT8_C(1)

#define UEEC_MAX_NODES           UINT16_MAX
#define UEEC_MAX_NAMES           UINT16_MAX
#define UEEC_MAX_NAME_SIZE       UINT8_MAX
#define UEEC_MAX_NODE_SECTIONS   UINT8_C(32)

/* machine_type namespace */
#define UEEC_MACHINE_TYPE_CPU  UINT8_C(0)
#define UEEC_MACHINE_TYPE_MPU  UINT8_C(1)
#define UEEC_MACHINE_TYPE_VM   UINT8_C(2)
#define UEEC_MACHINE_TYPE_GPU  UINT8_C(3)
#define UEEC_MACHINE_TYPE_FPGA UINT8_C(4)

/* CPU machine namespace */
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

/* MPU machine namespace */
#define UEEC_MACHINE_AVR8   UINT8_C(0)
#define UEEC_MACHINE_STM32  UINT8_C(1)
#define UEEC_MACHINE_PIC8   UINT8_C(2)
#define UEEC_MACHINE_ESP32  UINT8_C(3)
#define UEEC_MACHINE_MSP430 UINT8_C(4)
#define UEEC_MACHINE_NORDIC UINT8_C(5)

/* VM machine namespace */
#define UEEC_MACHINE_VM_UEEC    UINT8_C(0)
#define UEEC_MACHINE_VM_JAVA    UINT8_C(1)
#define UEEC_MACHINE_VM_DOT_NET UINT8_C(2)
#define UEEC_MACHINE_VM_WASM    UINT8_C(3)

/* GPU machine namespace */
#define UEEC_MACHINE_GPU_NVIDIA  UINT8_C(0)
#define UEEC_MACHINE_GPU_RADEON  UINT8_C(1)
#define UEEC_MACHINE_GPU_INTEL   UINT8_C(2)
#define UEEC_MACHINE_GPU_APPLE   UINT8_C(3)
#define UEEC_MACHINE_GPU_ARM_MALI UINT8_C(4)

/* FPGA machine namespace */
#define UEEC_MACHINE_FPGA_XILINX    UINT8_C(0)
#define UEEC_MACHINE_FPGA_ALTERA    UINT8_C(1)
#define UEEC_MACHINE_FPGA_LATTICE   UINT8_C(2)
#define UEEC_MACHINE_FPGA_MICROCHIP UINT8_C(3)
#define UEEC_MACHINE_FPGA_ACHRONIX  UINT8_C(4)
#define UEEC_MACHINE_FPGA_EFINIX    UINT8_C(5)

/* system_type namespace */
#define UEEC_SYSTEM_TYPE_BARE_METAL UINT8_C(0)
#define UEEC_SYSTEM_TYPE_EMBEDDED   UINT8_C(1)
#define UEEC_SYSTEM_TYPE_GENERAL_OS UINT8_C(2)
#define UEEC_SYSTEM_TYPE_HYPERVISOR UINT8_C(3)

/* GENERAL_OS system namespace */
#define UEEC_SYSTEM_ANDROID UINT8_C(0)
#define UEEC_SYSTEM_IOS     UINT8_C(1)
#define UEEC_SYSTEM_LINUX   UINT8_C(2)
#define UEEC_SYSTEM_MACOS   UINT8_C(3)
#define UEEC_SYSTEM_WINDOWS UINT8_C(4)

/* EMBEDDED system namespace */
#define UEEC_SYSTEM_FREE_RTOS UINT8_C(0)

/* Section-header size formats selected by ueec_node_hdr.format. */
#define UEEC_FORMAT_SH16 UINT16_C(0)
#define UEEC_FORMAT_SH32 UINT16_C(1)
#define UEEC_FORMAT_SH64 UINT16_C(2)

/* UEEC v1 section types. HEADER is always global physical section index 0. */
#define UEEC_SECTION_HEADER        UINT16_C(0)
#define UEEC_SECTION_CODE          UINT16_C(1)
#define UEEC_SECTION_INIT_DATA     UINT16_C(2)
#define UEEC_SECTION_UNINIT_DATA   UINT16_C(3)
#define UEEC_SECTION_CONSTANT_DATA UINT16_C(4)
#define UEEC_SECTION_RELOCATION    UINT16_C(5)
#define UEEC_SECTION_EXPORT        UINT16_C(6)
#define UEEC_SECTION_IMPORT        UINT16_C(7)
#define UEEC_SECTION_DEBUG         UINT16_C(8)
#define UEEC_SECTION_TLS           UINT16_C(9)
#define UEEC_SECTION_STACK         UINT16_C(10)
#define UEEC_SECTION_HEAP          UINT16_C(11)
#define UEEC_SECTION_RESOURCE      UINT16_C(12)
#define UEEC_SECTION_METADATA      UINT16_C(13)

#define UEEC_HEADER_SECTION_INDEX UINT32_C(0)

typedef struct UEEC_PACKED {
    uint8_t magic[UEEC_MAGIC_SIZE];

#if UEEC_HOST_ENDIAN == UEEC_ENDIAN_LITTLE
    uint8_t version : 7,
            endian  : 1;
#elif UEEC_HOST_ENDIAN == UEEC_ENDIAN_BIG
    uint8_t endian  : 1,
            version : 7;
#endif

    /* Real counts; neither field uses count-minus-one encoding. */
    uint16_t nodes;
    uint16_t names;
} ueec_hdr, *pueec_hdr;

typedef struct UEEC_PACKED {
#if UEEC_HOST_ENDIAN == UEEC_ENDIAN_LITTLE
    uint32_t encode        : 1,
             kernel_mode   : 1,
             user_mode     : 1,
             program       : 1,
             library       : 1,
             console       : 1,
             service       : 1,
             machine_type  : 3,
             machine       : 8,
             system_type   : 2,
             system        : 7,
             section_count : 5;
#elif UEEC_HOST_ENDIAN == UEEC_ENDIAN_BIG
    uint32_t section_count : 5,
             system        : 7,
             system_type   : 2,
             machine       : 8,
             machine_type  : 3,
             service       : 1,
             console       : 1,
             library       : 1,
             program       : 1,
             user_mode     : 1,
             kernel_mode   : 1,
             encode        : 1;
#endif

    /* UEEC_FORMAT_SH16, UEEC_FORMAT_SH32 or UEEC_FORMAT_SH64. */
    uint16_t format;
} ueec_node_hdr, *pueec_node_hdr;

/*
 * Maps a non-HEADER global section to a node. HEADER membership is implicit,
 * so a record with section == UEEC_HEADER_SECTION_INDEX is invalid.
 */
typedef struct UEEC_PACKED {
    uint16_t node;
    uint32_t section;
} ueec_node_info, *pueec_node_info;

/* Name-table wire arrays: name_len[], node_name_index[], then name_data[]. */
typedef uint8_t  ueec_name_len;
typedef uint16_t ueec_node_name_index;

typedef struct UEEC_PACKED {
    uint16_t type;
    uint16_t section_size;
    uint16_t data_size;
} ueec_sh16, *pueec_sh16;

typedef struct UEEC_PACKED {
    uint16_t type;
    uint32_t section_size;
    uint32_t data_size;
} ueec_sh32, *pueec_sh32;

typedef struct UEEC_PACKED {
    uint16_t type;
    uint64_t section_size;
    uint64_t data_size;
} ueec_sh64, *pueec_sh64;

#if defined(_MSC_VER)
# pragma pack(pop)
#endif

_Static_assert(sizeof(ueec_hdr) == 8, "ueec_hdr must be 8 bytes");
_Static_assert(sizeof(ueec_node_hdr) == 6,
               "ueec_node_hdr must be 6 bytes");
_Static_assert(sizeof(ueec_node_info) == 6,
               "ueec_node_info must be 6 bytes");
_Static_assert(sizeof(ueec_sh16) == 6, "ueec_sh16 must be 6 bytes");
_Static_assert(sizeof(ueec_sh32) == 10, "ueec_sh32 must be 10 bytes");
_Static_assert(sizeof(ueec_sh64) == 18, "ueec_sh64 must be 18 bytes");

/* Stored section_count is count - 1; the real range is 1..32. */
#define UEEC_NODE_SECTION_COUNT(node_header) \
    ((uint8_t)((node_header)->section_count + UINT8_C(1)))

#define UEEC_NODE_SECTION_COUNT_STORED(real_count) \
    ((uint8_t)((real_count) - UINT8_C(1)))

#if defined(__GNUC__) || defined(__clang__)
# define UEEC_BSWAP16(value) __builtin_bswap16((uint16_t)(value))
# define UEEC_BSWAP32(value) __builtin_bswap32((uint32_t)(value))
# define UEEC_BSWAP64(value) __builtin_bswap64((uint64_t)(value))
#elif defined(_MSC_VER)
# include <stdlib.h>
# define UEEC_BSWAP16(value) _byteswap_ushort((uint16_t)(value))
# define UEEC_BSWAP32(value) _byteswap_ulong((uint32_t)(value))
# define UEEC_BSWAP64(value) _byteswap_uint64((uint64_t)(value))
#endif

#define UEEC_VALUE16(file_endian, value) \
    (((file_endian) == UEEC_HOST_ENDIAN) ? (uint16_t)(value) : \
                                          UEEC_BSWAP16(value))
#define UEEC_VALUE32(file_endian, value) \
    (((file_endian) == UEEC_HOST_ENDIAN) ? (uint32_t)(value) : \
                                          UEEC_BSWAP32(value))
#define UEEC_VALUE64(file_endian, value) \
    (((file_endian) == UEEC_HOST_ENDIAN) ? (uint64_t)(value) : \
                                          UEEC_BSWAP64(value))

#define UEEC_SECTION_HEADER_SIZE(format) \
    ((format) == UEEC_FORMAT_SH16 ? (uint32_t)sizeof(ueec_sh16) : \
     (format) == UEEC_FORMAT_SH32 ? (uint32_t)sizeof(ueec_sh32) : \
     (format) == UEEC_FORMAT_SH64 ? (uint32_t)sizeof(ueec_sh64) : UINT32_C(0))

#define UEEC_MAGIC_VALID(header) \
    ((header)->magic[0] == UEEC_MAGIC_0 && \
     (header)->magic[1] == UEEC_MAGIC_1 && \
     (header)->magic[2] == UEEC_MAGIC_2)

/* Runtime invariant after the loader builds its metadata block. */
#define UEEC_SET_HEADER_ADDRESS(section_address, runtime_header_base) \
    ((section_address)[UEEC_HEADER_SECTION_INDEX] = \
         (uintptr_t)(runtime_header_base))

#endif /* UEEC_H */
