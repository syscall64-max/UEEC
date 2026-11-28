from pathlib import Path
import struct as s
p=Path('fixtures'); p.mkdir(exist_ok=True)
code=Path('build/code.bin').read_bytes(); marker=bytes.fromhex('8877665544332211'); assert code.count(marker)==1
patch=code.index(marker); assert patch==2
for label, data in [('base',bytes(range(1,8))+bytes([14])+bytes(8)),('control',bytes(range(1,8))+bytes([15])+bytes(8))]:
    assert len(data)==16 and sum(data) in (42,43)
    name=b'sum'; reloc=s.pack('<5H',0,0,1<<5,1,0x1000|patch)
    payload=code[:patch]+bytes(8)+code[patch+8:]
    for fmt,w,typ in [(0,2,'H'),(1,4,'I'),(2,8,'Q')]:
        h=8+6+18+4*(2+2*w)+3+len(name)
        b=s.pack('<3sBHH',b'_GM',1,1,1)+s.pack('<IH',(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)
        b+=b''.join(s.pack('<HI',0,i) for i in (1,2,3))
        b+=b''.join(s.pack('<H'+typ*2,t,n,n) for t,n in [(0,h),(1,len(code)),(2,len(data)),(5,len(reloc))])
        b+=s.pack('<BH',len(name),0)+name
        assert len(b)==h
        (p/f'{label}_sh{w*8}.ueec').write_bytes(b+payload+data+reloc)
    # ELF compact RWX segment, and separate writable non-executable page.
    for kind in ('single','split'):
        base=0x400000; phnum=1 if kind=='single' else 2; start=64+56*phnum
        off=start+len(code) if kind=='single' else 4096
        data_va=base+off
        c=code[:patch]+s.pack('<Q',data_va)+code[patch+8:]
        ident=b'\x7fELF'+bytes([2,1,1,0])+bytes(8)
        hdr=s.pack('<16sHHIQQQIHHHHHH',ident,2,62,1,base+start,64,0,0,64,56,phnum,64,0,0)
        end=off+len(data) if kind=='single' else start+len(code)
        hdr+=s.pack('<IIQQQQQQ',1,7 if kind=='single' else 5,0,base,base,end,end,4096)
        if kind=='split': hdr+=s.pack('<IIQQQQQQ',1,6,off,data_va,data_va,len(data),len(data),4096)
        f=p/f'{label}_{kind}.elf'; f.write_bytes(hdr+c+bytes(off-len(hdr)-len(c))+data); f.chmod(0o755)
