import struct as s
from pathlib import Path
code=Path('fixtures/linux-code.bin').read_bytes()
assert code.hex()=='b83c00000031ff0f05'
for fmt,typ in enumerate('HIQ'):
    name=b'exit'; sh=2+2*s.calcsize('<'+typ)
    h=8+6+6+2*sh+3+len(name)
    b=s.pack('<3sBHH',b'_GM',1,1,1)
    b+=s.pack('<IH',(1<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)
    b+=s.pack('<HI',0,1)
    b+=s.pack('<H'+typ*2,0,h,h)+s.pack('<H'+typ*2,1,len(code),len(code))
    b+=s.pack('<BH',len(name),0)+name
    assert len(b)==h
    Path(f'fixtures/exit_sh{16<<fmt}.ueec').write_bytes(b+code)
