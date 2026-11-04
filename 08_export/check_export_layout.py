#!/usr/bin/env python3
"""Verify both UEEC export designs and byte orders for 12 CPU nodes."""
import pathlib, struct, subprocess
BASE=pathlib.Path(__file__).resolve().parent
ARCH=[('arm32',0,'le'),('arm64',1,'le'),('loongarch64',2,'le'),('powerpc32',3,'be'),('powerpc64',4,'be'),('riscv32',5,'le'),('riscv64',6,'le'),('sparc32',7,'be'),('sparc64',8,'be'),('x86',9,'le'),('x86_64',10,'le'),('s390x',11,'be')]
def parse_nasm(text):
    out=bytearray()
    for line in text.splitlines():
        line=line.split(';',1)[0].strip()
        if not line: continue
        op,_,values=line.partition(' ')
        fmts={'db':('<B',1),'dw':('<H',2),'dd':('<I',4)}
        assert op in fmts,(op,line)
        for val in values.split(','):
            out+=struct.pack(fmts[op][0],int(val.strip(),0))
    return bytes(out)
for name,machine,endian in ARCH:
    bincode=(BASE/f'arch/answer_{name}.bin').read_bytes()
    assert bincode
    for mode,executable in [('COMPACT_GROUP','gen_export'),('ORDINAL','gen_export_ordinal')]:
        r=subprocess.run([str(BASE/executable),str(BASE/f'arch/answer_{name}.bin'),str(machine),endian],text=True,capture_output=True,check=True)
        data=parse_nasm(r.stdout)
        assert data[:4]==b'_GM\x01', (name,mode)
        assert struct.unpack_from('<H',data,4)[0]==1
        nh=struct.unpack_from('<I',data,8)[0]
        assert (nh>>10)&255==machine
        assert ((nh>>27)&31)+1==4
        # Four sections are HEADER DATA CODE EXPORT
        sh=[struct.unpack_from('<HHH',data,32+i*6) for i in range(4)]
        assert [x[0] for x in sh]==[0,2,1,6]
        assert len(data)==sum(x[1] for x in sh)
        assert sh[0][1]==64
        section_data=data[sh[0][1]:sh[0][1]+sh[1][1]]
        fmt='>' if endian=='be' else '<'
        vals=struct.unpack(fmt+('4i' if mode=='COMPACT_GROUP' else 'i'),section_data)
        assert vals==((42,11,22,33) if mode=='COMPACT_GROUP' else (42,)),(name,mode,vals)
        code_start=sh[0][1]+sh[1][1]
        assert data[code_start:code_start+len(bincode)]==bincode
        exp=data[code_start+len(bincode):]
        design=0 if mode=='COMPACT_GROUP' else 2
        assert struct.unpack_from('<HH',exp)==(0,design)
        if design==2:
            assert struct.unpack_from('<HH',exp,4)==(0,2)
            assert exp[8:18]==bytes.fromhex('01 00 00 01 00 00 00 00 05 00')
        else:
            assert struct.unpack_from('<HH',exp,4)==(0,3)
        print(f'UEEC EXPORT: {name:11s} machine={machine:2d} {mode:13s} DATA={endian.upper()} PASS')
print('UEEC EXPORT: 12 architectures x 2 designs = 24 layout checks PASS')
