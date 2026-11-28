from pathlib import Path
import struct as s
p=Path('fixtures'); p.mkdir(exist_ok=True)
raw=Path('build/code.bin').read_bytes(); marker=bytes.fromhex('8877665544332211'); assert raw.index(marker)==2
for n in (16,256,4096,32768):
    code=raw.replace(s.pack('<I',0x12345678),s.pack('<I',n)); assert code!=raw
    name=b'sum'; reloc=s.pack('<5H',0,0,32,1,0x1002); payload=code[:2]+bytes(8)+code[10:]
    for fmt,w,typ in [(0,2,'H'),(1,4,'I'),(2,8,'Q')]:
        h=8+6+18+4*(2+2*w)+6
        b=s.pack('<3sBHH',b'_GM',1,1,1)+s.pack('<IH',(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)
        b+=b''.join(s.pack('<HI',0,i) for i in (1,2,3))
        b+=b''.join(s.pack('<H'+typ*2,t,sz,d) for t,sz,d in [(0,h,h),(1,len(code),len(code)),(3,n,0),(5,10,10)])
        b+=s.pack('<BH',len(name),0)+name; assert len(b)==h
        for layout in ('physical','nobits'):
            (p/f'{n}_sh{w*8}_{layout}.ueec').write_bytes(b+payload+(bytes([0xA5])*n if layout=='physical' else b'')+reloc)
    for kind in ('single','split'):
        base=0x400000; phnum=1 if kind=='single' else 2; start=64+56*phnum; end=start+len(code)
        off=end if kind=='single' else 4096; va=base+off
        c=code[:2]+s.pack('<Q',va)+code[10:]
        ident=b'\x7fELF'+bytes([2,1,1,0])+bytes(8)
        hdr=s.pack('<16sHHIQQQIHHHHHH',ident,2,62,1,base+start,64,0,0,64,56,phnum,64,0,0)
        hdr+=s.pack('<IIQQQQQQ',1,7 if kind=='single' else 5,0,base,base,end,end+n if kind=='single' else end,4096)
        if kind=='split': hdr+=s.pack('<IIQQQQQQ',1,6,off,va,va,0,n,4096)
        f=p/f'{n}_{kind}.elf'; f.write_bytes(hdr+c); f.chmod(0o755)
