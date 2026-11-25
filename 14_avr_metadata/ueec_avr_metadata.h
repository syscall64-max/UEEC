#ifndef UEEC_AVR_METADATA_H
#define UEEC_AVR_METADATA_H
#include "ueec_metadata.h"

/* Existing UEEC MPU namespace, not the CPU machine namespace. */
#define UEEC_MACHINE_TYPE_MPU UINT8_C(1)
#define UEEC_MACHINE_AVR8 UINT8_C(0)
#define UEEC_SYSTEM_TYPE_BARE_METAL UINT8_C(0)

/* Experimental metadata IDs for this demonstration. */
#define UEEC_MD_TYPE_AVR_TARGET UINT16_C(0x200)
#define UEEC_MD_TYPE_AVR_REQUIRED UINT16_C(0x201)
#define UEEC_MD_TYPE_AVR_BLINK UINT16_C(0x202)

typedef struct UEEC_PACKED {
    uint8_t signature[3];
    uint8_t gcc_arch;
    uint8_t pc_bytes, sp_bytes;
    uint16_t reserved;
    uint32_t flash_bytes, sram_bytes, eeprom_bytes, clock_hz;
} ueec_md_avr_target;

/* C view only. The wire value is an explicitly encoded uint64_t. */
typedef struct UEEC_PACKED {
    uint64_t movw:1, lpmx:1, adiw_sbiw:1, mul:1, jmp_call:1,
             elpm:1, elpmx:1, eijmp_eicall:1, reserved:56;
} ueec_md_avr_caps;

/* Application test binding, separate from MCU/ISA capabilities. */
typedef struct UEEC_PACKED {
    uint8_t port, pin;
    uint16_t half_period_ms;
} ueec_md_avr_blink;

#define UEEC_AVR_KNOWN_MASK UINT64_C(0xff)
_Static_assert(sizeof(ueec_md_avr_target)==24,"AVR target wire size");
_Static_assert(sizeof(ueec_md_avr_caps)==8,"AVR capability wire size");
_Static_assert(sizeof(ueec_md_avr_blink)==4,"AVR GPIO binding wire size");
#endif
