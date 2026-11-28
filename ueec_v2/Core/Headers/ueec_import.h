#ifndef UEEC_IMPORT_H
#define UEEC_IMPORT_H
/* DRAFT wire views derived from 13.6 resolve.c; not yet regression tested. */
#include "ueec.h"
#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
#define UEEC_IMPORT_DESIGN_0 UINT16_C(0)
#define UEEC_IMPORT_BY_NAME UINT8_C(0)
#define UEEC_IMPORT_BY_ORDINAL UINT8_C(1)
typedef struct UEEC_PACKED { uint16_t format, design; } ueec_import;
typedef struct UEEC_PACKED { uint8_t kind; uint16_t section, offset; } ueec_import_entry16;
typedef struct UEEC_PACKED { uint8_t kind; uint32_t section, offset; } ueec_import_entry32;
typedef struct UEEC_PACKED { uint8_t kind; uint64_t section, offset; } ueec_import_entry64;
#if defined(_MSC_VER)
#pragma pack(pop)
#endif
_Static_assert(sizeof(ueec_import)==4,"import header");
_Static_assert(sizeof(ueec_import_entry16)==5,"import16");
_Static_assert(sizeof(ueec_import_entry32)==9,"import32");
_Static_assert(sizeof(ueec_import_entry64)==17,"import64");
#endif
