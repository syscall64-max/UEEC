#!/usr/bin/env python3
"""Test actual AVR execution/GPIO plus shared metadata selection and rejection."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import generate
from generate import ROOT,build
from models import MODELS,HALF_PERIOD_MS

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--loader",type=Path,default=ROOT/"build/ueec-avr")
    args=p.parse_args();loader=args.loader.resolve();checks=0;executed=[]
    report=ROOT/"build/results";report.mkdir(parents=True,exist_ok=True)
    traces=report/("sanitized" if args.loader.name.endswith("sanitize") else "normal");traces.mkdir(exist_ok=True)
    def call(options=(),file=ROOT/"build/blink.ueec",ok=True,need=None):
        nonlocal checks
        command=[str(loader),*map(str,options)]+([str(file)] if file else [])
        r=subprocess.run(command,capture_output=True,text=True,timeout=30)
        text=r.stdout+r.stderr
        if r.returncode<0 or (r.returncode==0)!=ok or (need and need not in text):
            raise AssertionError(f"{command}\nexit={r.returncode}\n{text}")
        if "ERROR: AddressSanitizer" in text or "runtime error:" in text:
            raise AssertionError(text)
        checks+=1;return text
    def location(data,endian="little"):
        e="<" if endian=="little" else ">"
        n=struct.unpack_from(e+"H",data,4)[0];table=8+n*18
        descriptors=[struct.unpack_from(e+"HHH",data,table+6*i) for i in range(n+2)]
        return sum(d[1] for d in descriptors[:-1]),table,descriptors

    call(["--list-devices"],None,need="atmega2560")
    call(["--mcu","attiny85","--groups"],None,need="ISA available: movw,lpmx,adiw_sbiw")
    with tempfile.TemporaryDirectory(prefix="ueec-avr-",dir=ROOT/"build") as directory:
        work=Path(directory)
        def fixture(name,**options):
            path=work/(name+".ueec");path.write_bytes(build(**options));return path
        for index,model in enumerate(MODELS):
            profile=json.loads((ROOT/"payloads"/(model.name+".json")).read_text())
            text=call(["--mcu",model.name,"--list-nodes"],need="METADATA=4")
            rows=[line for line in text.splitlines() if line.startswith("node=")]
            assert len(rows)==3 and sum("compatible=yes" in line for line in rows)==1
            text=call(["--mcu",model.name,"--no-run"],need=f"node={index} CODE={index+1}")
            assert "FLASH " not in text and "GPIO " not in text
            raw=work/(model.name+".bin")
            call(["--mcu",model.name,"--no-run","--extract",raw],need="mode=automatic")
            assert raw.read_bytes()==(ROOT/"payloads"/(model.name+".bin")).read_bytes()
            vcd=work/(model.name+".vcd")
            text=call(["--mcu",model.name,"--vcd",vcd],need="PASS: virtual Flash + AVR execution + 8 GPIO transitions")
            assert "verified=yes metadata_written=no" in text
            levels=[int(v) for v in re.findall(r"GPIO edge=\d+ .*?level=(\d)",text)]
            assert levels==[1,0,1,0,1,0,1,0]
            cycles=list(map(int,re.findall(r"GPIO edge=\d+ .*?cycle=(\d+)",text)))
            expected=model.clock_hz*HALF_PERIOD_MS//1000
            assert len(cycles)==8 and all(abs((b-a)-expected)<=expected//100+32 for a,b in zip(cycles,cycles[1:]))
            states=[];now=0
            for line in vcd.read_text().splitlines():
                if line.startswith("#"): now=int(line[1:])
                if line in ("0!","1!"): states.append((now,int(line[0])))
            assert len(states)>=8 and [s[1] for s in states[-8:]]==levels
            assert all(b[0]>a[0] for a,b in zip(states,states[1:]))
            (traces/(model.name+".log")).write_text(text)
            (traces/(model.name+".vcd")).write_bytes(vcd.read_bytes())
            executed.append(dict(model=model.name,signature=bytes(profile['signature']).hex(),
                gcc_arch=profile['gcc_arch'],edges=8,half_period_ms=HALF_PERIOD_MS,cycles=cycles,
                code_sha256=hashlib.sha256(raw.read_bytes()).hexdigest()))
            for endian in ("little","big"):
                for width in (2,4,8):
                    f=fixture(f"{model.name}-{endian}-{width}",endian=endian,width=width,reverse=True)
                    text=call(["--mcu",model.name,"--no-run"],f,need=f"node={2-index} CODE={3-index}")
                    assert "FLASH " not in text
            # A file containing only another model must never be flashed.
            other=MODELS[(index+1)%3]
            call(["--mcu",model.name],fixture(model.name+"-wrong-device",models=[other.name]),ok=False,need="no compatible AVR node")
            call(["--mcu",model.name,"--frequency","1000000"],ok=False,need="no compatible AVR node")
            print(f"{model.name}: signature selection, virtual Flash, 8 timed GPIO edges and VCD PASS",flush=True)

        # Node 256 is a real match after 256 nodes for a different device.
        saved=generate.MODELS
        try:
            generate.MODELS=(MODELS[0],)*256+(MODELS[1],)
            high=fixture("node-256")
        finally: generate.MODELS=saved
        call(["--mcu","atmega328p","--no-run"],high,need="node=256 CODE=257 METADATA=258")

        # Inspect the format without the C parser, including the fixed node IDs.
        good=build();md,table,desc=location(good)
        assert struct.unpack_from("<HH",good,4)==(3,1)
        assert [d[0] for d in desc]==[0,1,1,1,13] and md+desc[-1][1]==len(good)
        assert struct.unpack_from("<HHH",good,md)==(0,1,12)
        for node in range(3):
            assert struct.unpack_from("<HIHI",good,26+node*12)==(node,node+1,node,4)
            assert all(struct.unpack_from("<H",good,md+6+(node*4+i)*6)[0]==node for i in range(4))
        checks+=1
        sentinel=md+6+12*6
        changes=(("node-range",md+6,b"\x03\x00"),("node-65535",md+6,b"\xff\xff"),
                 ("design",md+2,b"\x00\x00"),("format",md,b"\x03\x00"),
                 ("type",md+8,b"\xff\x7f"),("offset",md+10,b"\xff\xff"),
                 ("sentinel-type",sentinel+2,b"\x01\x00"),("sentinel-node",sentinel,b"\x01\x00"),
                 ("duplicate-priority",md+6+7*6,b"\x00\x00"),
                 ("odd-code",table+6+4,struct.pack("<H",desc[1][2]-1)),
                 ("not-mpu",8,b"\x08"))
        for name,at,value in changes:
            bad=bytearray(good);bad[at:at+len(value)]=value
            f=work/(name+".ueec");f.write_bytes(bad)
            call(["--mcu","atmega328p"],f,ok=False,need="malformed")
        for endian in ("little","big"):
            for width in (2,4,8):
                bad=bytearray(build(width=width,endian=endian));at,_,_=location(bad,endian)
                bad[at+4:at+4+width]=b"\xff"*width
                f=work/(f"count-{endian}-{width}.ueec");f.write_bytes(bad)
                call(["--no-run"],f,ok=False,need="malformed")
        for key,value in (("required",1<<63),("signature",[0x1e,0x95,0]),("gcc_arch",25),
                          ("pc_bytes",3),("sp_bytes",1),("flash_bytes",65536),
                          ("sram_bytes",4096),("eeprom_bytes",2048),("clock_hz",8000000)):
            f=fixture("mismatch-"+key,overrides={"atmega328p":{key:value}})
            text=call(["--mcu","atmega328p"],f,ok=False,need="no compatible AVR node")
            assert "FLASH " not in text
        for size in (0,3,7,20,len(good)-1):
            f=work/("truncated-"+str(size)+".ueec");f.write_bytes(good[:size])
            call([],f,ok=False)
        wrong_pin=fixture("wrong-pin",overrides={"atmega328p":{"pin":4}})
        call(["--max-cycles","2000000","--edges","3"],wrong_pin,ok=False,need="FAIL: AVR simulation")
        wrong_timing=fixture("wrong-timing",overrides={"atmega328p":{"half_period_ms":200}})
        call(["--edges","3"],wrong_timing,ok=False,need="timing_error=1")
        existing=work/"existing.vcd";existing.write_bytes(b"keep")
        call(["--vcd",existing],ok=False,need="will not overwrite");assert existing.read_bytes()==b"keep"
        call(["--extract",existing,"--no-run"],ok=False,need="will not overwrite");assert existing.read_bytes()==b"keep"
        call(["--mcu","atmega88"],ok=False,need="unsupported virtual device")
    record=dict(checks=checks,simulator="simavr",physical_hardware=False,models=executed)
    (traces/"results.json").write_text(json.dumps(record,indent=2)+"\n")
    print(f"UEEC AVR: 3 models, 24 observed GPIO transitions, {checks} selection/format/simulation checks PASS (no skips)")

if __name__=="__main__": main()
