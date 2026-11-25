#ifndef UEEC_METADATA_H
#define UEEC_METADATA_H

#include <stdint.h>
#include "reference/ueec.h"

/* Prototype assignments, local to this demo. No existing index is changed. */
#define UEEC_SECTION_METADATA UINT16_C(13)
#define UEEC_MD_DESIGN_INDEXED UINT16_C(0)
#define UEEC_MD_DESIGN_NODE_INDEXED UINT16_C(1)
#define UEEC_MD_TYPE_END UINT16_C(0)
#define UEEC_MD_TYPE_PRIORITY UINT16_C(2)

typedef struct UEEC_PACKED {
    uint16_t format;
    uint16_t design;
} ueec_md_hdr, *pueec_md_hdr;
typedef struct UEEC_PACKED { uint16_t count; } ueec_mdcf16, *pueec_mdcf16;
typedef struct UEEC_PACKED { uint32_t count; } ueec_mdcf32, *pueec_mdcf32;
typedef struct UEEC_PACKED { uint64_t count; } ueec_mdcf64, *pueec_mdcf64;
typedef struct UEEC_PACKED { uint16_t type, offset; } ueec_element16, *pueec_element16;
typedef struct UEEC_PACKED { uint16_t type; uint32_t offset; } ueec_element32, *pueec_element32;
typedef struct UEEC_PACKED { uint16_t type; uint64_t offset; } ueec_element64, *pueec_element64;

/* Shared section: the node index is ALWAYS 16-bit, independently of format.
   Offsets are relative to the data area after the complete index table.
   The final sentinel has node_index=0, type=END and offset=data_size. */
typedef struct UEEC_PACKED {
    uint16_t node_index, type, offset;
} ueec_md_node_element16, *pueec_md_node_element16;
typedef struct UEEC_PACKED {
    uint16_t node_index, type;
    uint32_t offset;
} ueec_md_node_element32, *pueec_md_node_element32;
typedef struct UEEC_PACKED {
    uint16_t node_index, type;
    uint64_t offset;
} ueec_md_node_element64, *pueec_md_node_element64;

typedef struct UEEC_PACKED { uint16_t value; } ueec_md_priority;

_Static_assert(sizeof(ueec_md_hdr) == 4, "metadata header size");
_Static_assert(sizeof(ueec_element16) == 4, "16-bit metadata element size");
_Static_assert(sizeof(ueec_element32) == 6, "32-bit metadata element size");
_Static_assert(sizeof(ueec_element64) == 10, "64-bit metadata element size");
_Static_assert(sizeof(ueec_md_node_element16) == 6, "shared 16-bit element size");
_Static_assert(sizeof(ueec_md_node_element32) == 8, "shared 32-bit element size");
_Static_assert(sizeof(ueec_md_node_element64) == 12, "shared 64-bit element size");
#endif
