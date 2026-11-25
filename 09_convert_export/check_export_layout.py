#!/usr/bin/env python3
"""Validate name-export conversion for all 12 CPUs without a QEMU dependency."""
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parent
ARCHS=[('arm32',0,'le'),('arm64',1,'le'),('loongarch64',2,'le'),
('powerpc32',3,'be'),('powerpc64',4,'be'),('riscv32',5,'le'),
('riscv64',6,'le'),('sparc32',7,'be'),('sparc64',8,'be'),
('x86',9,'le'),('x86_64',10,'le'),('s390x',11,'be')]

def asm_to_bytes(source):
    out=bytearray()
    for line in source.splitlines():
        line=line.split(';',1)[0].strip()
        m=re.match(r'^(db|dw|dd|dq)\s+(.+)$',line)
        if not m:
            if line and not line.startswith(('BITS','org','section')):
                raise ValueError('Unknown NASM directive: '+line)
            continue
        code,nums=m.groups()
        w={'db':1,'dw':2,'dd':4,'dq':8}[code]
        for t in nums.split(','):
            n=int(t.strip(),0)
            out.extend(n.to_bytes(w,'little'))
    return bytes(out)

def run(*args):
    return subprocess.run([str(a) for a in args],cwd=ROOT,check=True,
                          capture_output=True,text=True).stdout

def section_data(blob):
    assert blob[:4]==bytes.fromhex('5f474d01')
    assert int.from_bytes(blob[4:6],'little')==1
    nh=int.from_bytes(blob[8:12],'little')
    ninfo=((nh>>27)&31)
    off=14+6*ninfo
    section_sizes=[]
    for _ in range(ninfo+1):
        t,sz,_=struct.unpack_from('<HHH',blob,off)
        section_sizes.append((t,sz))
        off+=6
    off+=1+2+5
    assert off==64, off
    slices=[(section_sizes[0][0],blob[:section_sizes[0][1]])]
    for t,sz in section_sizes[1:]:
        slices.append((t,blob[off:off+sz]))
        off+=sz
    assert off==len(blob), (off,len(blob))
    return slices

with tempfile.TemporaryDirectory(prefix='ueec_exp_') as td:
    td=Path(td)
    for name,machine,endian in ARCHS:
        code=ROOT/'arch'/f'answer_{name}.bin'
        assert code.exists(), code
        text=run(ROOT/'gen_export',code,machine,endian)
        initial=asm_to_bytes(text)
        src=td/(name+'.ueec'); src.write_bytes(initial)
        sections=section_data(initial)
        assert [t for t,_ in sections]==[0,2,1,6],name
        assert sections[2][1]==code.read_bytes(),name
        assert len(sections[1][1])==16
        for i,v in enumerate((42,11,22,33)):
            assert int.from_bytes(sections[1][1][i*4:i*4+4],
                                  'big' if endian=='be' else 'little')==v
        assert sections[3][1][2:4]==b'\0\0'
        assert len(initial)==64+16+code.stat().st_size+60,name
        flat=td/(name+'_flat.ueec')
        back=td/(name+'_back.ueec')
        flat2=td/(name+'_flat2.ueec')
        run(ROOT/'convert_export',src,flat,'--design','flat')
        run(ROOT/'convert_export',flat,back,'--design','compact')
        run(ROOT/'convert_export',back,flat2,'--design','flat')
        assert back.read_bytes()==initial, f'{name}: compact round-trip differs'
        assert flat.read_bytes()==flat2.read_bytes(),f'{name}: flat round-trip differs'
        flat_sections=section_data(flat.read_bytes())
        assert [t for t,_ in flat_sections]==[0,2,1,6]
        assert flat_sections[1][1]==sections[1][1]
        assert flat_sections[2][1]==sections[2][1]
        assert flat_sections[3][1][2:4]==b'\x01\x00'
        if name=='x86_64':
            for v in (src,flat,back,flat2):
                output=run(ROOT/'loader_convert_export',v)
                for expected in ('answer() = 42','value = 42','value = 11','value = 22','value = 33'):
                    assert expected in output,(v,expected,output)
        print(f'UEEC export conversion: {name} machine={machine} {endian.upper()} '
              f'COMPACT_GROUP <-> SIMPLE_FLAT bit-identical round trips PASS')
print('UEEC export conversion: 12/12 architectures, 24/24 export layouts PASS')
