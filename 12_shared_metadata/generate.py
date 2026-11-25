#!/usr/bin/env python3
"""Build UEEC v1 with per-node CODE and one shared, trailing METADATA section."""
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

def shared_metadata(rows, width=2, endian="little"):
    e = "<" if endian == "little" else ">"
    fmt = {2: "H", 4: "I", 8: "Q"}[width]
    entries, data = bytearray(), bytearray()
    for node, (_, _, required, priority, kind) in enumerate(rows):
        entries += struct.pack(e + "HH" + fmt, node, kind, len(data))
        data += struct.pack(e + "Q", required)
        entries += struct.pack(e + "HH" + fmt, node, 2, len(data))
        data += struct.pack(e + "H", priority)
    entries += struct.pack(e + "HH" + fmt, 0, 0, len(data))
    return (struct.pack(e + "HH" + fmt, {2: 0, 4: 1, 8: 2}[width], 1, 2 * len(rows))
            + entries + data)

def build(directory=ROOT/"payloads", width=2, endian="little", architectures=None,
          reverse=False, overrides=None, metadata_layout="shared"):
    if metadata_layout not in ("shared", "per-node"):
        raise ValueError("unknown metadata layout")
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
            rows.append((spec.machine, code, req, priority, spec.metadata_type))
    if reverse: rows.reverse()
    if not rows: raise ValueError("empty container")
    nodes, mappings, payloads = bytearray(), bytearray(), []
    if len(rows) > 65535:
        raise ValueError("node index/count exceeds 16-bit UEEC limit")
    for i,(machine,code,req,priority,kind) in enumerate(rows):
        # UEEC v1: implicit HEADER + CODE + METADATA, user-space library node.
        code_big = machine in (3,4,7,8,11)
        word = (2<<27) | (2<<20) | (2<<18) | (machine<<10) | 0x14 | int(code_big)
        nodes += struct.pack(e+"IH", word, 0)
        if metadata_layout == "shared":
            mappings += struct.pack(e+"HIHI", i,1+i,i,1+len(rows))
            payloads.append((1,code))
        else:
            mappings += struct.pack(e+"HIHI", i,1+2*i,i,2+2*i)
            payloads.extend(((1,code),(13,metadata(req,priority,kind,width,endian))))
    if metadata_layout == "shared":
        payloads.append((13,shared_metadata(rows,width,endian)))
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
    p.add_argument("--metadata-layout", choices=("shared", "per-node"), default="shared",
                   help="per-node generates legacy design-0 files for compatibility checks")
    a=p.parse_args()
    data=build(a.directory,a.md_width,a.endian,[a.arch] if a.arch else None,a.reverse,
               metadata_layout=a.metadata_layout)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_bytes(data)
    count=struct.unpack_from(("<" if a.endian=="little" else ">")+"H",data,4)[0]
    md_count=1 if a.metadata_layout=="shared" else count
    print(f"{a.output}: {len([a.arch] if a.arch else ARCHES)} architectures, {count} nodes, "
          f"{count} CODE + {md_count} METADATA, layout={a.metadata_layout}, {len(data)} bytes")

if __name__=="__main__": main()
