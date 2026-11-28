import struct as s, csv
from pathlib import Path
rows=[]
def add(name,total,code,header,padding,other,status):
 assert total==code+header+padding+other
 rows.append([name,total,code,header,padding,other,status])
for p in sorted(Path('fixtures').glob('*.ueec')):
 b=p.read_bytes(); fmt=s.unpack_from('<H',b,12)[0]; typ='HIQ'[fmt]; h,d=s.unpack_from('<'+typ*2,b,22); assert h==d and b[h:]==Path('fixtures/linux-code.bin').read_bytes(); add(p.name,len(b),len(b)-h,h,0,0,'PASS Linux 3 modes')
b=Path('fixtures/exit.elf').read_bytes(); e=s.unpack_from('<16sHHIQQQIHHHHHH',b); phoff,shoff=e[5:7]; eh,phsz,phn,shsz,shn,shstr=e[8:14]; intervals=[(0,eh),(phoff,phoff+phsz*phn),(shoff,shoff+shsz*shn)]; code=other=0
for i in range(shn):
 q=s.unpack_from('<IIQQQQIIQQ',b,shoff+i*shsz); t,flags,off,n=q[1],q[2],q[4],q[5]
 if t in (0,8) or not n: continue
 intervals.append((off,off+n))
 if flags&4: code+=n; assert b[off:off+n]==Path('fixtures/linux-code.bin').read_bytes()
 else: other+=n
assert sum(y-x for x,y in intervals)==eh+phsz*phn+shsz*shn+code+other
add('exit.elf',len(b),code,eh+phsz*phn+shsz*shn,len(b)-eh-phsz*phn-shsz*shn-code-other,other,'PASS Linux')
b=Path('fixtures/exit.exe').read_bytes(); pe=s.unpack_from('<I',b,60)[0]; assert b[pe:pe+4]==b'PE\0\0'; n=s.unpack_from('<H',b,pe+6)[0]; opt=s.unpack_from('<H',b,pe+20)[0]; o=pe+24; hdr=s.unpack_from('<I',b,o+60)[0]; end=o+opt+40*n; assert n==1; vs,va,raw,ptr=s.unpack_from('<IIII',b,o+opt+8); assert ptr==hdr and raw+ptr==len(b); assert b[ptr:ptr+vs].hex()=='49c7c2ffffffff31d2b82c0000000f05'; assert all(s.unpack_from('<II',b,o+112+i*8)==(0,0) for i in range(16)); add('exit.exe',len(b),vs,end,hdr-end+raw-vs,0,'Windows execution pending'); print('PE DOS prefix',pe,'post-DOS region',pe-64,'header padding',hdr-end,'section padding',raw-vs)
with open('results.csv','w') as f:
 w=csv.writer(f); w.writerow(['file','total','code','structural_headers','padding_or_gaps','other_section_data','status']); w.writerows(rows)
for r in rows: print(*r,sep=' | ')
