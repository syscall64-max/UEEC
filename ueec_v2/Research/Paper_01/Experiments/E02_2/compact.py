import struct
from pathlib import Path
p=Path('fixtures'); code=(p/'linux-code.bin').read_bytes()
assert code.hex()=='b83c00000031ff0f05'
base=0x400000; eh=64; ph=56; start=eh+ph; size=start+len(code)
ident=b'\x7fELF'+bytes([2,1,1,0])+bytes(8)
h=struct.pack('<16sHHIQQQIHHHHHH',ident,2,62,1,base+start,eh,0,0,eh,ph,1,64,0,0)
s=struct.pack('<IIQQQQQQ',1,5,0,base,base,size,size,4096)
assert len(h)==eh and len(s)==ph
f=p/'exit_compact.elf'; f.write_bytes(h+s+code); f.chmod(0o755)
assert f.stat().st_size==size
