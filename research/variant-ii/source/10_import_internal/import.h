#ifndef UEEC_IMPORT_H
#define UEEC_IMPORT_H
#include "../hdrs/ueec.h"
#define UEEC_IMPORT_DESIGN_COMPACT_GROUP UINT16_C(0)
#define UEEC_IMPORT_FORMAT_16 UINT16_C(0)
#define UEEC_IMPORT_FORMAT_32 UINT16_C(1)
#define UEEC_IMPORT_FORMAT_64 UINT16_C(2)
#define UEEC_IMPORT_LIBRARY_INTERNAL 0
#define UEEC_IMPORT_LIBRARY_EXTERNAL 1
#define UEEC_IMPORT_EXTERNAL16 UINT16_C(0x8000)
#define UEEC_IMPORT_EXTERNAL32 UINT32_C(0x80000000)
#define UEEC_IMPORT_EXTERNAL64 UINT64_C(0x8000000000000000)
typedef struct UEEC_PACKED { uint16_t design, format; } ueec_import_hdr, *pueec_import_hdr;
#define IMPORT_TYPES(B,T) \
 typedef T ueec_import_library_count##B; \
 typedef T ueec_import_object_count##B; \
 typedef struct UEEC_PACKED { T location; } ueec_import_library##B, *pueec_import_library##B; \
 typedef struct UEEC_PACKED { T section, offset, name_offset; } ueec_import_object##B, *pueec_import_object##B;
IMPORT_TYPES(16,uint16_t)
IMPORT_TYPES(32,uint32_t)
IMPORT_TYPES(64,uint64_t)
#undef IMPORT_TYPES
_Static_assert(sizeof(ueec_import_hdr)==4,"import header");
_Static_assert(sizeof(ueec_import_object16)==6,"import16");
_Static_assert(sizeof(ueec_import_object32)==12,"import32");
_Static_assert(sizeof(ueec_import_object64)==24,"import64");
#endif
