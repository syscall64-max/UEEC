#!/usr/bin/env python3
"""Build real reset-vector AVR firmware and derive profiles from device headers."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from models import MODELS,HALF_PERIOD_MS,ISA_MACROS

def main():
    cc=shlex.split(os.environ.get("AVR_CC","avr-gcc"))
    objcopy=shlex.split(os.environ.get("AVR_OBJCOPY","avr-objcopy"))
    work=ROOT/"build/firmware";work.mkdir(parents=True,exist_ok=True)
    payloads=ROOT/"payloads";payloads.mkdir(exist_ok=True)
    for model in MODELS:
        text=subprocess.run(cc+["-mmcu="+model.name,"-dM","-E","-include","avr/io.h","-"],
                            input="",text=True,capture_output=True,check=True).stdout
        definitions={line.split()[1]:line.split(maxsplit=2)[2] if len(line.split())>2 else ""
                     for line in text.splitlines() if line.startswith("#define ")}
        def number(key): return int(definitions[key].strip("()"),0)
        elf=work/(model.name+".elf");raw=payloads/(model.name+".bin")
        subprocess.run(cc+["-mmcu="+model.name,"-Os","-std=gnu11","-Wall","-Wextra","-Werror",
                           "-ffunction-sections","-fdata-sections","-Wl,--gc-sections",
                           f"-DF_CPU={model.clock_hz}UL",f"-DLED_BIT={model.pin}",
                           f"-DBLINK_HALF_PERIOD_MS={HALF_PERIOD_MS}",str(ROOT/"firmware/blink.c"),"-o",str(elf)],check=True)
        subprocess.run(objcopy+["-O","binary","-j",".text","-j",".data",str(elf),str(raw)],check=True)
        profile=dict(model=model.name,signature=[number("SIGNATURE_"+str(i)) for i in range(3)],
                     gcc_arch=number("__AVR_ARCH__"),pc_bytes=3 if "__AVR_3_BYTE_PC__" in definitions else 2,
                     sp_bytes=2 if "__AVR_HAVE_16BIT_SP__" in definitions else 1,
                     flash_bytes=number("FLASHEND")+1,sram_bytes=number("RAMEND")-number("RAMSTART")+1,
                     eeprom_bytes=number("E2END")+1,clock_hz=model.clock_hz,
                     required=sum(1<<i for i,key in enumerate(ISA_MACROS) if key in definitions),
                     port=model.port,pin=model.pin,half_period_ms=HALF_PERIOD_MS,priority=100,
                     sha256=hashlib.sha256(raw.read_bytes()).hexdigest())
        (payloads/(model.name+".json")).write_text(json.dumps(profile,indent=2)+"\n")
        print(f"{model.name}: firmware build PASS; core=avr{profile['gcc_arch']} CODE={raw.stat().st_size}B "
              f"signature={bytes(profile['signature']).hex()} required=0x{profile['required']:x}",flush=True)

if __name__=="__main__": main()
