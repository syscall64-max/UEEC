#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import struct
from models import MODELS
ROOT=Path(__file__).resolve().parent

def build(width=2,endian="little",models=None,reverse=False,overrides=None,model_sequence=None):
    e="<" if endian=="little" else ">";fmt={2:"H",4:"I",8:"Q"}[width]
    selected=list(model_sequence) if model_sequence is not None else [m for m in MODELS if models is None or m.name in models]
    if reverse: selected.reverse()
    if not selected: raise ValueError("no models")
    nodes=bytearray();maps=bytearray();entries=bytearray();data=bytearray();codes=[]
    def record(node,kind,body):
        nonlocal entries,data
        entries+=struct.pack(e+"HH"+fmt,node,kind,len(data));data+=body
    for n,m in enumerate(selected):
        p=json.loads((ROOT/"payloads"/(m.name+".json")).read_text())
        code=(ROOT/"payloads"/(m.name+".bin")).read_bytes()
        if hashlib.sha256(code).hexdigest()!=p['sha256']: raise ValueError("bad payload checksum: "+m.name)
        if overrides and m.name in overrides: p.update(overrides[m.name])
        nodes+=struct.pack(e+"IH",(2<<27)|(1<<7)|(1<<10)|(1<<3),0)
        maps+=struct.pack(e+"HIHI",n,n+1,n,len(selected)+1);codes.append(code)
        record(n,0x210,struct.pack(e+"HHIIIII",p['model_id'],p['core_part'],p['flash_base'],p['flash_bytes'],p['ram_base'],p['ram_bytes'],p['clock_hz']))
        record(n,0x211,struct.pack(e+"Q",p['required']))
        record(n,0x212,struct.pack(e+"IBBH",p['gpio_base'],ord(p['port']),p['pin'],p['half_period_ms']))
        record(n,2,struct.pack(e+"H",p['priority']))
    entries+=struct.pack(e+"HH"+fmt,0,0,len(data))
    md=struct.pack(e+"HH"+fmt,{2:0,4:1,8:2}[width],1,len(selected)*4)+entries+data
    names=b"\x05"+struct.pack(e+str(len(selected))+"H",*([0]*len(selected)))+b"blink"
    header_size=8+len(nodes)+len(maps)+(len(selected)+2)*6+len(names)
    descriptors=struct.pack(e+"HHH",0,header_size,header_size)
    for code in codes: descriptors+=struct.pack(e+"HHH",1,len(code),len(code))
    descriptors+=struct.pack(e+"HHH",13,len(md),len(md))
    header=b"_GM"+bytes([1|(0x80 if endian=="big" else 0)])+struct.pack(e+"HH",len(selected),1)+nodes+maps+descriptors+names
    assert len(header)==header_size
    return header+b"".join(codes)+md

def main():
    p=argparse.ArgumentParser(description="STM32 UEEC with three nodes and one shared metadata section")
    p.add_argument("--output",type=Path,default=ROOT/"build/blink.ueec")
    p.add_argument("--md-width",type=int,choices=(2,4,8),default=2)
    p.add_argument("--endian",choices=("little","big"),default="little")
    p.add_argument("--reverse",action="store_true")
    a=p.parse_args();data=build(a.md_width,a.endian,reverse=a.reverse)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(data)
    print(f"{a.output}: 3 STM32 models, 3 CODE + 1 shared METADATA, {len(data)} bytes")

if __name__=="__main__": main()
