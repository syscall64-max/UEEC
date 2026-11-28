#ifndef UEEC_RELOC_H
#define UEEC_RELOC_H
/* DRAFT wire views from 12.6 convert.py; explicit endian decoding required. */
#include "ueec.h"
#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
#define UEEC_RELOC_DESIGN_FLAT UINT16_C(0)
#define UEEC_RELOC_DESIGN_BLOCK_4096 UINT16_C(1)
#define UEEC_RELOC_DESIGN_BLOCK_N UINT16_C(2)
typedef struct UEEC_PACKED { uint16_t format, design; } ueec_reloc;
/* Raw word avoids assuming C bit-field ordering. */
typedef struct UEEC_PACKED { uint16_t pair_format, count; } ueec_reloc_couple16;
typedef struct UEEC_PACKED { uint16_t pair_format; uint32_t count; } ueec_reloc_couple32;
typedef struct UEEC_PACKED { uint16_t pair_format; uint64_t count; } ueec_reloc_couple64;
typedef struct UEEC_PACKED { uint16_t index, count; } ueec_reloc_block16;
typedef struct UEEC_PACKED { uint32_t index, count; } ueec_reloc_block32;
typedef struct UEEC_PACKED { uint64_t index, count; } ueec_reloc_block64;
#if defined(_MSC_VER)
#pragma pack(pop)
#endif
_Static_assert(sizeof(ueec_reloc)==4,"relocation header");
_Static_assert(sizeof(ueec_reloc_couple16)==4,"couple16");
_Static_assert(sizeof(ueec_reloc_couple32)==6,"couple32");
_Static_assert(sizeof(ueec_reloc_couple64)==10,"couple64");
#endif
