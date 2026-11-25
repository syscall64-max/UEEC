#!/usr/bin/env python3
"""Build one UEEC v1 container with CODE/METADATA for all 12 architectures."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from architectures import ARCHES

ROOT = Path(__file__).resolve().parent

def metadata(required, priority, kind, width=2, endian="little"):
    e = "<" if endian == "little" else ">"
    fmt = {2: "H", 4: "I", 8: "Q"}[width]
    return (struct.pack(e + "HH" + fmt, {2: 0, 4: 1, 8: 2}[width], 0, 2)
            + struct.pack(e + "H" + fmt, kind, 0)
            + struct.pack(e + "H" + fmt, 2, 8)
            + struct.pack(e + "H" + fmt, 0, 10)
            + struct.pack(e + "QH", required, priority))

def build(directory=ROOT/"payloads", width=2, endian="little", architectures=None,
          reverse=False, overrides=None):
    e = "<" if endian == "little" else ">"
    rows = []
    for arch, spec in ARCHES.items():
        if architectures is not None and arch not in architectures: continue
        manifest = {x["technology"]: x for x in json.loads((directory/arch/"manifest.json").read_text())}
        for variant in reversed(spec.variants):
            code = (directory/arch/(variant.name+".bin")).read_bytes()
            if hashlib.sha256(code).hexdigest() != manifest[variant.name]["sha256"]:
                raise ValueError(f"payload checksum mismatch: {arch}/{variant.name}; rebuild or restore")
            req, priority = variant.required, variant.priority
            if overrides and (arch,variant.name) in overrides:
                req, priority = overrides[(arch,variant.name)]
            rows.append((spec.machine, code, metadata(req,priority,spec.metadata_type,width,endian)))
    if reverse: rows.reverse()
    if not rows: raise ValueError("empty container")
    nodes, mappings, payloads = bytearray(), bytearray(), []
    for i,(machine,code,md) in enumerate(rows):
        # UEEC v1: implicit HEADER + CODE + METADATA, user-space library node.
        code_big = machine in (3,4,7,8,11)
        word = (2<<27) | (2<<20) | (2<<18) | (machine<<10) | 0x14 | int(code_big)
        nodes += struct.pack(e+"IH", word, 0)
        mappings += struct.pack(e+"HIHI", i,1+2*i,i,2+2*i)
        payloads.extend(((1,code),(13,md)))
    names = b"\x03" + struct.pack(e+str(len(rows))+"H", *([0]*len(rows))) + b"sum"
    count = 1+len(payloads)
    header_size = 8+len(nodes)+len(mappings)+count*6+len(names)
    if max(header_size, *(len(d) for _,d in payloads))>65535:
        raise ValueError("SH16 limit; this demo needs SH32 for larger payloads")
    descriptors = struct.pack(e+"HHH",0,header_size,header_size)
    descriptors += b"".join(struct.pack(e+"HHH",kind,len(data),len(data)) for kind,data in payloads)
    header = (b"_GM"+bytes([1|(0x80 if endian=="big" else 0)])+struct.pack(e+"HH",len(rows),1)
              +nodes+mappings+descriptors+names)
    assert len(header)==header_size
    return bytes(header)+b"".join(data for _,data in payloads)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output",type=Path,default=ROOT/"build/sum.ueec")
    p.add_argument("--directory",type=Path,default=ROOT/"payloads")
    p.add_argument("--md-width",type=int,choices=(2,4,8),default=2)
    p.add_argument("--endian",choices=("little","big"),default="little")
    p.add_argument("--arch",choices=tuple(ARCHES))
    p.add_argument("--reverse",action="store_true")
    a=p.parse_args()
    data=build(a.directory,a.md_width,a.endian,[a.arch] if a.arch else None,a.reverse)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_bytes(data)
    count=struct.unpack_from(("<" if a.endian=="little" else ">")+"H",data,4)[0]
    print(f"{a.output}: {len([a.arch] if a.arch else ARCHES)} architectures, {count} nodes, {count} CODE + {count} METADATA, {len(data)} bytes")

if __name__=="__main__": main()
