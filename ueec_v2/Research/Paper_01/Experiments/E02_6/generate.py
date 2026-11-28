from pathlib import Path
import struct as s
p=Path('fixtures'); p.mkdir(exist_ok=True)
raw=Path('build/code.bin').read_bytes()
markers=[0x1122334455667788,0x2233445566778899,0x33445566778899aa]
assert [raw.index(s.pack('<Q',x)) for x in markers]==[2,12,22]
for n in (16,256,4096,32768):
 for label,ro in [('base',bytes(range(1,8))+bytes([14])+bytes(8)),('control',bytes(range(1,8))+bytes([15])+bytes(8))]:
    rw=bytes([1,2,3,4])+bytes(12)
    code=raw.replace(s.pack('<I',0x12345678),s.pack('<I',n))
    reloc=s.pack('<2H',0,0)+b''.join(s.pack('<3H',32*(j+1),1,0x1000|(2+10*j)) for j in range(3))
    def patch(addrs):
        c=code
        for m,v in zip(markers,addrs): c=c.replace(s.pack('<Q',m),s.pack('<Q',v))
        return c
    for fmt,w,typ in [(0,2,'H'),(1,4,'I'),(2,8,'Q')]:
        h=8+6+30+6*(2+2*w)+6
        b=s.pack('<3sBHH',b'_GM',1,1,1)+s.pack('<IH',(5<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)
        b+=b''.join(s.pack('<HI',0,i) for i in range(1,6))
        b+=b''.join(s.pack('<H'+typ*2,t,sz,d) for t,sz,d in [(0,h,h),(1,len(code),len(code)),(4,16,16),(2,16,16),(3,n,0),(5,len(reloc),len(reloc))])
        b+=s.pack('<BH',3,0)+b'sum'; assert len(b)==h
        for layout in ('physical','nobits'):
            (p/f'{n}_{label}_sh{w*8}_{layout}.ueec').write_bytes(b+patch([0,0,0])+ro+rw+(bytes([0xA5])*n if layout=='physical' else b'')+reloc)
    for kind in ('single','split'):
        base=0x400000; phnum=1 if kind=='single' else 3; start=64+56*phnum; end=start+len(code)
        rooff=end if kind=='single' else 4096; rwoff=rooff+16 if kind=='single' else 8192
        c=patch([base+rooff,base+rwoff,base+rwoff+16])
        ident=b'\x7fELF'+bytes([2,1,1,0])+bytes(8)
        hdr=s.pack('<16sHHIQQQIHHHHHH',ident,2,62,1,base+start,64,0,0,64,56,phnum,64,0,0)
        hdr+=s.pack('<IIQQQQQQ',1,7 if kind=='single' else 5,0,base,base,rwoff+16 if kind=='single' else end,rwoff+16+n if kind=='single' else end,4096)
        if kind=='split':
            hdr+=s.pack('<IIQQQQQQ',1,4,rooff,base+rooff,base+rooff,16,16,4096)
            hdr+=s.pack('<IIQQQQQQ',1,6,rwoff,base+rwoff,base+rwoff,16,16+n,4096)
        f=p/f'{n}_{label}_{kind}.elf'; f.write_bytes(hdr+c+bytes(rooff-end)+ro+bytes(rwoff-rooff-16)+rw); f.chmod(0o755)
