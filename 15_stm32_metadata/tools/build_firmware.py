#!/usr/bin/env python3
import hashlib
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from models import MODELS,FLASH_BASE,RAM_BASE,HALF_PERIOD_MS

def main():
    cc=shlex.split(os.environ.get("ARM_CC","arm-none-eabi-gcc"))
    objcopy=shlex.split(os.environ.get("ARM_OBJCOPY","arm-none-eabi-objcopy"))
    work=ROOT/"build/firmware";work.mkdir(parents=True,exist_ok=True)
    for m in MODELS:
        linker=work/(m.name+".ld")
        linker.write_text(f"""ENTRY(Reset_Handler)
MEMORY {{ FLASH (rx) : ORIGIN = 0x{FLASH_BASE:x}, LENGTH = 0x{m.flash_bytes:x}
         RAM (rwx) : ORIGIN = 0x{RAM_BASE:x}, LENGTH = 0x{m.ram_bytes:x} }}
_stack_top = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {{
  .isr_vector : {{ KEEP(*(.isr_vector)) }} > FLASH
  .text : {{ *(.text*) *(.rodata*) . = ALIGN(4); }} > FLASH
  .data : {{ . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; }} > RAM AT>FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : {{ . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; }} > RAM
  /DISCARD/ : {{ *(.comment*) *(.ARM.exidx*) *(.ARM.extab*) }}
}}
""")
        elf=work/(m.name+".elf");raw=ROOT/"payloads"/(m.name+".bin")
        args=cc+["-mcpu="+m.cpu,"-mthumb","-mfloat-abi=soft","-std=gnu11","-Os","-g",
                 "-ffreestanding","-fno-builtin","-nostdlib","-fno-unwind-tables","-fno-asynchronous-unwind-tables",
                 "-ffunction-sections","-fdata-sections","-Wall","-Wextra","-Werror",
                 f"-DGPIO_BASE=0x{m.gpio_base:x}u",f"-DGPIO_PIN={m.pin}u",f"-DGPIO_PORT_INDEX={ord(m.port)-ord('A')}u",
                 f"-DGPIO_F1={int(m.f1)}",f"-DCPU_CLOCK_HZ={m.clock_hz}u",f"-DBLINK_HALF_PERIOD_MS={HALF_PERIOD_MS}u",
                 "-Wl,--gc-sections","-T",str(linker),str(ROOT/"firmware/startup.S"),str(ROOT/"firmware/blink.c"),"-o",str(elf)]
        subprocess.run(args,check=True)
        subprocess.run(objcopy+["-O","binary",str(elf),str(raw)],check=True)
        data=raw.read_bytes();sp,reset=struct.unpack_from("<II",data)
        assert sp==RAM_BASE+m.ram_bytes and reset&1 and FLASH_BASE<=(reset&~1)<FLASH_BASE+len(data)
        profile=dict(model=m.name,model_id=m.model_id,core_part=m.part,flash_base=FLASH_BASE,flash_bytes=m.flash_bytes,
                     ram_base=RAM_BASE,ram_bytes=m.ram_bytes,clock_hz=m.clock_hz,required=1,gpio_base=m.gpio_base,
                     port=m.port,pin=m.pin,half_period_ms=HALF_PERIOD_MS,priority=100,
                     code_bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
        (ROOT/"payloads"/(m.name+".json")).write_text(json.dumps(profile,indent=2)+"\n")
        print(f"{m.name}: firmware build PASS; CPU={m.cpu}; CODE={len(data)}B SP=0x{sp:08x} reset=0x{reset:08x}",flush=True)

if __name__=="__main__": main()
