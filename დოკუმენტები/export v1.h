//
#ifndef UEEC_EXPORT_H
#define UEEC_EXPORT_H
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
    uint16_t    format;
    uint16_t    design;
} ueec_export, *pueec_export;
//
typedef struct UEEC_PACKED {
    uint16_t    format;
    uint16_t    count;
} ueec_export_count16, *pueec_export_count16;
//
typedef struct UEEC_PACKED {
    uint16_t    format;
    uint32_t    count;
} ueec_export_count32, *pueec_export_count32;
//
typedef struct UEEC_PACKED {
    uint16_t    format;
    uint64_t    count;    
} ueec_export_count64, *pueec_export_count64;
//
typedef struct UEEC_PACKED {
    char        name[30];
    uint8_t     section : 5,
                reserved : 3;
    uint16_t    offset;
} ueec_export_info16, *pueec_export_info16;
//
typedef struct UEEC_PACKED {
    char        name[30];
    uint8_t     section : 5,
                reserved : 3;
    uint32_t    offset;
} ueec_export_info32, *pueec_export_info32;
//
typedef struct UEEC_PACKED {
    char        name[30];
    uint8_t     section : 5,
                reserved : 3;
    uint64_t    offset;
} ueec_export_info64, *pueec_export_info64;
//
#endif /* UEEC_EXPORT_H */
