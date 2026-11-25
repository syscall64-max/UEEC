from dataclasses import dataclass

@dataclass(frozen=True)
class Model:
    name: str
    model_id: int
    cpu: str
    part: int
    flash_bytes: int
    ram_bytes: int
    clock_hz: int
    gpio_base: int
    port: str
    pin: int
    f1: bool
    capabilities: int

MODELS=(
    Model("stm32f103rb",1,"cortex-m3",0xc23,128*1024,20*1024,8_000_000,0x40010800,"A",5,True,1),
    Model("stm32f407vg",2,"cortex-m4",0xc24,1024*1024,128*1024,16_000_000,0x40020c00,"D",12,False,7),
    Model("stm32f746ng",3,"cortex-m7",0xc27,1024*1024,320*1024,16_000_000,0x40022000,"I",1,False,0x3b),
)
FLASH_BASE=0x08000000
RAM_BASE=0x20000000
HALF_PERIOD_MS=100
