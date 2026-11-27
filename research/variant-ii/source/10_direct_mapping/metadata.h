#ifndef UEEC_METADATA_H
#define UEEC_METADATA_H
#include "../hdrs/ueec.h"
#define UEEC_METADATA_LOAD_INFO UINT16_C(1)
#define UEEC_METADATA_PRELINKED UINT16_C(2) /* experimental, same layout, absolute slots */
#define UEEC_METADATA_FORMAT16 UINT16_C(0)
#define UEEC_METADATA_FORMAT32 UINT16_C(1)
#define UEEC_METADATA_FORMAT64 UINT16_C(2)
typedef struct UEEC_PACKED { uint16_t type, format; } ueec_metadata_hdr, *pueec_metadata_hdr;
typedef struct UEEC_PACKED { uint16_t node, address; } ueec_load_info16, *pueec_load_info16;
typedef struct UEEC_PACKED { uint16_t node; uint32_t address; } ueec_load_info32, *pueec_load_info32;
typedef struct UEEC_PACKED { uint16_t node; uint64_t address; } ueec_load_info64, *pueec_load_info64;
#endif
