#ifndef DEVICES_H
#define DEVICES_H
/* Conservative subset of instruction capabilities; masks represent requirements. */
#define AVR_ISA_MUL UINT32_C(1)
#define AVR_ISA_JMP_CALL UINT32_C(2)
#define AVR_ISA_MOVW UINT32_C(4)
#define AVR_ADDR_ELPM UINT32_C(1)
#define AVR_ADDR_EIJMP_EICALL UINT32_C(2)
typedef struct { const char *name; uint8_t sig[3],pc; uint32_t fields[7]; } avr_device;
/* Vector count includes RESET; ATmega128 normal (not compatibility) mode. */
static const avr_device devices[]={
 {"ATtiny85",{0x1e,0x93,0x0b},12,{8192,512,512,64,15,4,0}},
 {"ATmega8",{0x1e,0x93,0x07},12,{8192,1024,512,64,19,5,0}},
 {"ATmega328P",{0x1e,0x95,0x0f},14,{32768,2048,1024,128,26,7,0}},
 {"ATmega128",{0x1e,0x97,0x02},16,{131072,4096,4096,256,35,7,1}},
 {"ATmega2560",{0x1e,0x98,0x01},17,{262144,8192,4096,256,57,7,3}}
};
#endif
