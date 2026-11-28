#ifndef AVR_METADATA_H
#define AVR_METADATA_H
#include "ueec.h"
/* Experimental section payload, not a ratified UEEC wire design. */
#define UEEC_AVR_META_FORMAT32 UINT16_C(0)
#define UEEC_AVR_META_DESIGN_FIXED UINT16_C(0)
#define UEEC_AVR_META_SIZE 36u
typedef struct UEEC_PACKED {
    uint16_t format, design;
    uint8_t signature[3], pc_bits;
    uint32_t flash_bytes, sram_bytes, eeprom_bytes, flash_page_bytes;
    uint32_t interrupt_vectors, isa_features, addressing_features;
} ueec_avr_meta32;
_Static_assert(sizeof(ueec_avr_meta32)==UEEC_AVR_META_SIZE,"wire size");
/* Feature masks are opaque until their namespace is specified. */
#endif
