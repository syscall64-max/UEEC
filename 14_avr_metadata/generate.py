#!/usr/bin/env python3
"""One bare-metal AVR UEEC: three CODE sections and one shared trailing MD."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from models import MODELS
ROOT=Path(__file__).resolve().parent

def build(width=2,endian="little",models=None,reverse=False,overrides=None):
    e="<" if endian=="little" else ">"
    fmt={2:"H",4:"I",8:"Q"}[width]
    chosen=[m for m in MODELS if models is None or m.name in models]
    if reverse: chosen.reverse()
    if not chosen: raise ValueError("no models")
    nodes=bytearray();maps=bytearray();codes=[];entries=bytearray();data=bytearray()
    def record(node,kind,blob):
        nonlocal entries,data
        entries+=struct.pack(e+"HH"+fmt,node,kind,len(data));data+=blob
    for node,m in enumerate(chosen):
        p=json.loads((ROOT/"payloads"/(m.name+".json")).read_text())
        code=(ROOT/"payloads"/(m.name+".bin")).read_bytes()
        if hashlib.sha256(code).hexdigest()!=p['sha256']: raise ValueError("bad payload checksum: "+m.name)
        if overrides and m.name in overrides: p.update(overrides[m.name])
        # MPU=1, AVR8=0, bare metal=0, system=0; standalone program=1.
        word=(2<<27)|(1<<7)|(1<<3)
        nodes+=struct.pack(e+"IH",word,0)
        maps+=struct.pack(e+"HIHI",node,node+1,node,len(chosen)+1)
        codes.append(code)
        target=bytes(p['signature'])+struct.pack(e+"BBB H IIII",p['gcc_arch'],p['pc_bytes'],p['sp_bytes'],0,
                p['flash_bytes'],p['sram_bytes'],p['eeprom_bytes'],p['clock_hz'])
        assert len(target)==24
        record(node,0x200,target)
        record(node,0x201,struct.pack(e+"Q",p['required']))
        record(node,0x202,struct.pack(e+"BBH",ord(p['port']),p['pin'],p['half_period_ms']))
        record(node,2,struct.pack(e+"H",p['priority']))
    entries+=struct.pack(e+"HH"+fmt,0,0,len(data))
    md=struct.pack(e+"HH"+fmt,{2:0,4:1,8:2}[width],1,len(chosen)*4)+entries+data
    names=b"\x05"+struct.pack(e+str(len(chosen))+"H",*([0]*len(chosen)))+b"blink"
    header_size=8+len(nodes)+len(maps)+(len(chosen)+2)*6+len(names)
    desc=struct.pack(e+"HHH",0,header_size,header_size)
    for code in codes: desc+=struct.pack(e+"HHH",1,len(code),len(code))
    desc+=struct.pack(e+"HHH",13,len(md),len(md))
    header=b"_GM"+bytes([1|(0x80 if endian=="big" else 0)])+struct.pack(e+"HH",len(chosen),1)+nodes+maps+desc+names
    assert len(header)==header_size
    return header+b"".join(codes)+md

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output",type=Path,default=ROOT/"build/blink.ueec")
    p.add_argument("--md-width",type=int,choices=(2,4,8),default=2)
    p.add_argument("--endian",choices=("little","big"),default="little")
    p.add_argument("--reverse",action="store_true")
    a=p.parse_args();blob=build(a.md_width,a.endian,reverse=a.reverse)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(blob)
    print(f"{a.output}: 3 AVR models, 3 CODE + 1 shared METADATA, {len(blob)} bytes")

if __name__=="__main__": main()
