import csv, struct, sys
from pathlib import Path

def inspect(p):
 b=p.read_bytes(); r={'file':p.name,'file_bytes':len(b)}
 def u(fmt,o): return struct.unpack_from(fmt,b,o)
 if b[:2]==b'MZ':
  off,=u('<I',60); assert off>=64 and b[off:off+4]==b'PE\0\0'
  n,=u('<H',off+6); opt,=u('<H',off+20); o=off+24
  magic,=u('<H',o); fixed={0x10b:96,0x20b:112}[magic]; dirs,=u('<I',o+fixed-4); hs,=u('<I',o+60)
  end=o+opt+40*n; assert dirs*8+fixed<=opt and end<=hs<=len(b)
  r.update(format='PE32' if magic==0x10b else 'PE32+',dos_header=64,dos_extra=off-64,signature=4,base_header=20,optional_fixed=fixed,directories=dirs*8,optional_extra=opt-fixed-dirs*8,section_headers=40*n,header_bytes=end,header_padding=hs-end,section_count=n)
 elif b[:4]==b'\x7fELF':
  e='<' if b[5]==1 else '>'; is64=b[4]==2
  ph,sh=u(e+('QQ' if is64 else 'II'),32 if is64 else 28)
  eh,pe,pn,se,sn,si=u(e+'6H',52 if is64 else 40)
  assert sn and pn!=65535 and ph+pe*pn<=len(b) and sh+se*sn<=len(b)
  r.update(format='ELF64' if is64 else 'ELF32',base_header=eh,program_headers=pe*pn,section_headers=se*sn,header_bytes=eh+pe*pn+se*sn,section_count=sn)
 elif b[:4]==b'\xcf\xfa\xed\xfe':
  n,s=u('<II',16); off=32; sc=0
  for _ in range(n):
   cmd,size=u('<II',off); assert size>=8 and off+size<=32+s
   if cmd==0x19:
    k,=u('<I',off+64); assert size==72+80*k; sc+=k
   off+=size
  assert off==32+s
  r.update(format='Mach-O64 object' if u('<I',12)[0]==1 else 'Mach-O64',base_header=32,other_load_commands=s-80*sc,section_headers=80*sc,header_bytes=32+s,section_count=sc)
 elif b[:3]==b'_GM':
  e='>' if b[3]&128 else '<'; n,names=u(e+'HH',4); assert n
  nodes=[u(e+'IH',8+6*i) for i in range(n)]; fmt=nodes[0][1]; assert all(f==fmt for _,f in nodes); m=sum(w>>27 for w,_ in nodes); off=8+6*n
  maps=[u(e+'HI',off+6*i) for i in range(m)]; assert all(i<n and j>0 for i,j in maps); sc=1+max(j for i,j in maps); off+=6*m
  sf={0:'HHH',1:'HII',2:'HQQ'}[fmt]; ss=struct.calcsize(e+sf); sections=[u(e+sf,off+i*ss) for i in range(sc)]; off+=ss*sc
  ns=names+2*n+sum(b[off:off+names]); h=off+ns; assert sections[0][0]==0 and sections[0][2]==h and sum(s for _,s,_ in sections)==len(b)
  r.update(format='UEEC v1',base_header=8,node_headers=6*n,node_mappings=6*m,section_headers=ss*sc,name_table=ns,header_bytes=h,header_padding=sections[0][1]-h,section_count=sc,node_count=n)
 else: raise ValueError(p)
 return r
if __name__=='__main__':
 rows=[inspect(Path(p)) for p in sys.argv[1:]]; cols=list(dict.fromkeys(k for r in rows for k in r)); w=csv.DictWriter(sys.stdout,cols); w.writeheader(); w.writerows(rows)
