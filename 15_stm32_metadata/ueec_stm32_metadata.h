#ifndef UEEC_STM32_METADATA_H
#define UEEC_STM32_METADATA_H
#include "ueec_metadata.h"
#define UEEC_MD_TYPE_STM32_TARGET UINT16_C(0x210)
#define UEEC_MD_TYPE_STM32_REQUIRED UINT16_C(0x211)
#define UEEC_MD_TYPE_STM32_BLINK UINT16_C(0x212)

typedef struct UEEC_PACKED {
    uint16_t model_id,core_part;
    uint32_t flash_base,flash_bytes,ram_base,ram_bytes,clock_hz;
} ueec_md_stm32_target;
/* Semantic C bitfield view only. Wire encoding uses explicit integer masks
   and the container byte order, never fwrite() of a C bitfield structure. */
typedef struct UEEC_PACKED {
    uint64_t thumb2:1,dsp:1,fpu_v4_sp:1,fpu_v5_sp:1,icache:1,dcache:1,reserved:58;
} ueec_md_stm32_caps;
enum ueec_stm32_bit {
    UEEC_STM32_THUMB2,UEEC_STM32_DSP,UEEC_STM32_FPU_V4_SP,
    UEEC_STM32_FPU_V5_SP,UEEC_STM32_ICACHE,UEEC_STM32_DCACHE
};
#define UEEC_STM32_FLAG(bit) (UINT64_C(1) << (bit))
typedef struct UEEC_PACKED {
    uint32_t gpio_base;
    uint8_t port,pin;
    uint16_t half_period_ms;
} ueec_md_stm32_blink;
_Static_assert(sizeof(ueec_md_stm32_target)==24,"STM32 target wire size");
_Static_assert(sizeof(ueec_md_stm32_caps)==8,"STM32 capability wire size");
_Static_assert(sizeof(ueec_md_stm32_blink)==8,"STM32 GPIO wire size");
#endif
