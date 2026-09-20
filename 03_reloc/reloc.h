//
#ifndef UEEC_RELOC_H
#define UEEC_RELOC_H
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
typedef struct UEEC_PACKED {
    uint16_t format;
    uint16_t design;
} ueec_reloc, *pueec_reloc;
//
typedef struct UEEC_PACKED {
    uint16_t    target : 5,
                source : 5,
                format : 6;
    uint16_t    count;
} ueec_reloc_couple16, *pueec_reloc_couple16;
//
typedef struct UEEC_PACKED {
    uint16_t    target : 5,
                source : 5,
                format : 6;
    uint32_t    count;
} ueec_reloc_couple32, *pueec_reloc_couple32;
//
typedef struct UEEC_PACKED {
    uint16_t    target : 5,
                source : 5,
                format : 6;
    uint64_t    count;
} ueec_reloc_couple64, *pueec_reloc_couple64;
//
typedef struct UEEC_PACKED {
    uint16_t    offset : 12,
                type : 4;
} ueec_reloc_info, *pueec_reloc_info;
//
typedef struct UEEC_PACKED {
    uint16_t    index;
    uint16_t    count;
} ueec_reloc_block16, *pueec_reloc_block16;
//
typedef struct UEEC_PACKED {
    uint32_t    index;
    uint32_t    count;
} ueec_reloc_block32, *pueec_reloc_block32;
//
typedef struct UEEC_PACKED {
    uint64_t    index;
    uint64_t    count;
} ueec_reloc_block64, *pueec_reloc_block64;
//
#endif /* UEEC_RELOC_H */
