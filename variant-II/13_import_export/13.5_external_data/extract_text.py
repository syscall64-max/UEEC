"""Extract relocation-free .text from ELF32/ELF64 assembler objects."""
import struct, sys
from pathlib import Path
b=Path(sys.argv[1]).read_bytes()
assert b[:4]==b'\x7fELF' and b[4] in (1,2) and b[5] in (1,2)
p='<' if b[5]==1 else '>'
wide=b[4]==2
h=struct.unpack_from(p+('HHIQQQIHHHHHH' if wide else 'HHIIIIIHHHHHH'),b,16)
assert h[0]==1
shoff, entsize, count, names=h[5],h[10],h[11],h[12]
fmt=p+('IIQQQQIIQQ' if wide else 'IIIIIIIIII')
sh=[struct.unpack_from(fmt,b,shoff+i*entsize) for i in range(count)]
st=sh[names]; strings=b[st[4]:st[4]+st[5]]
def name(s): return strings[s[0]:].split(b'\0',1)[0]
idx=next(i for i,s in enumerate(sh) if name(s)==b'.text')
assert not any(s[1] in (4,9) and s[7]==idx and s[5] for s in sh), 'text relocations not supported'
s=sh[idx]; data=b[s[4]:s[4]+s[5]]
assert 0<len(data)<=4096
Path(sys.argv[2]).write_bytes(data)
