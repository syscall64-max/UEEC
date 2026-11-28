import csv,struct as s,subprocess as sp
from pathlib import Path
F=Path('fixtures');F.mkdir(exist_ok=True)
A=(1,16,64,512,4096,65536,64,1); modes=('monolithic','scattered','hybrid')
def al(x,a):return (x+a-1)//a*a
def ueec(parts,a):
 for fmt,w in enumerate((2,4,8)):
  h=8+6+5*6+6*(2+2*w)+3+3
  sizes=[al(h,a)]+[al(max(len(b),16 if t==3 else 1),a) for t,b in parts]
  if max(al(x,4096) for x in sizes)<2**(8*w):break
 sh=lambda t,z,d:s.pack('<H'+'HIQ'[fmt]*2,t,z,d)
 head=s.pack('<3sBHHIH',b'_GM',1,1,1,12|(10<<10)|(2<<18)|(2<<20)|(5<<27),fmt)+b''.join(s.pack('<HI',0,i) for i in range(1,6))+sh(0,sizes[0],h)+b''.join(sh(t,z,len(b)) for (t,b),z in zip(parts,sizes[1:]))+b'\x03\0\0pad'
 assert len(head)==h
 return head.ljust(sizes[0],b'\0')+b''.join(b.ljust(z,b'\0') for (_,b),z in zip(parts,sizes[1:])),fmt,h,sum(sizes)-h-sum(len(b) for _,b in parts)
def decode(b):
 fmt=s.unpack_from('<H',b,12)[0];w=2<<fmt; hs=[s.unpack_from('<H'+'HIQ'[fmt]*2,b,44+i*(2+2*w)) for i in range(6)];pos=hs[0][1];out=[]
 assert not any(b[hs[0][2]:pos])
 for t,z,d in hs[1:]:
  out.append((t,b[pos:pos+d]));assert not any(b[pos+d:pos+z]);pos+=z
 assert pos==len(b);return out
BASE=0x400000
def elf(code,data,a,split):
 phnum=3 if split else 1; h=64+56*phnum; start=al(h,a); stubsize=34
 co=al(start+stubsize+len(code),max(a,4096) if split else a)
 rw=al(co+len(data),max(a,4096) if split else a); bss=rw+16
 stub=b'\x48\xbf'+s.pack('<Q',BASE+co)+b'\x48\xbe'+s.pack('<Q',BASE+rw)+b'\xe8'+s.pack('<i',9)+bytes.fromhex('89c7b83c0000000f05')
 assert len(stub)==stubsize
 init=bytes(8)+s.pack('<Q',BASE+bss)
 end=rw+16
 eh=s.pack('<16sHHIQQQIHHHHHH',b'\x7fELF'+bytes([2,1,1])+bytes(9),2,62,1,BASE+start,64,0,0,64,56,phnum,64,0,0)
 def ph(flags,off,fs,ms):return s.pack('<IIQQQQQQ',1,flags,off,BASE+off,BASE+off,fs,ms,max(4096,a))
 if split:eh+=ph(5,0,start+len(stub)+len(code),start+len(stub)+len(code))+ph(4,co,len(data),len(data))+ph(6,rw,16,32)
 else:eh+=ph(7,0,end,end+16)
 b=eh.ljust(start,b'\0')+stub+code;b=b.ljust(co,b'\0')+data;b=b.ljust(rw,b'\0')+init
 assert len(b)==end
 # Count mapped load pages, not RSS or loader overhead.
 spans=[(0,end+16)] if not split else [(0,start+len(stub)+len(code)),(co,co+len(data)),(rw,rw+32)]
 pages=set().union(*(set(range(lo//4096,al(hi,4096)//4096)) for lo,hi in spans))
 return b,h,len(b)-h-len(stub)-len(code)-len(data)-16,len(pages)*4096,co,rw,start
rows=[]; checks=0; native=0
for n in (16,511,4095,61441):
 for ctrl in (0,1):
  data=bytes([17+ctrl])+bytes(n-3)+bytes([23,0]);assert len(data)==n
  code=bytes.fromhex('0fb6070fb68f')+s.pack('<I',n-2)+bytes.fromhex('01c80fb68f')+s.pack('<I',n-1)+bytes.fromhex('01c8488b56080fb60a01c8c3')
  parts=[(1,code),(2,bytes(16)),(3,b''),(4,data),(5,s.pack('<5H',0,0,1|(2<<5),1,8|(1<<12)))];current=ueec(parts,1)[0]
  bases={k:elf(code,data,1,k)[0] for k in (False,True)}
  prev={k:elf(code,data,1,k) for k in (False,True)}
  for step,a in enumerate(A):
   assert decode(current)==parts
   b,fmt,h,pad=ueec(decode(current),a);assert decode(b)==parts
   if a==1:assert b==ueec(parts,1)[0]
   current=b;checks+=1
   f=F/f'{n}-{ctrl}-{step}.ueec';f.write_bytes(b)
   for mode in modes:
    r=sp.run(['./probe',mode,str(f)],capture_output=True,text=True,timeout=10);assert r.returncode==0 and f'result={40+ctrl} ' in r.stdout,(f,mode,r.stdout,r.stderr)
    alloc=int(r.stdout.split('allocation=')[1].split()[0]);assert f.read_bytes()==b;native+=1
    rows.append([n,ctrl,step,a,'UEEC',mode,16<<fmt,len(b),h,pad-al(16,a),alloc,al(16,a)])
   for split in (False,True):
    old,_,_,_,oldco,oldrw,oldstart=prev[split]
    oldcode=old[oldstart+34:oldstart+34+len(code)];olddata=old[oldco:oldco+n]
    assert oldcode==code and olddata==data and old[oldrw:oldrw+8]==bytes(8) and s.unpack_from('<Q',old,oldrw+8)[0]==BASE+oldrw+16
    prev[split]=elf(oldcode,olddata,a,split)
    e,eh,ep,em,co,rw,start=prev[split];assert e[co:co+n]==data and e[start+34:start+34+len(code)]==code
    if a==1:assert e==bases[split]
    checks+=1;f=F/f'{n}-{ctrl}-{step}-{split}.elf';f.write_bytes(e);f.chmod(0o755)
    r=sp.run([str(f)],timeout=10);assert r.returncode==40+ctrl,(f,r.returncode);assert f.read_bytes()==e;native+=1
    rows.append([n,ctrl,step,a,'ELF split' if split else 'ELF compact','kernel',64,len(e),eh,ep,em,0])
with open('results.csv','w') as f:
 w=csv.writer(f);w.writerow(['data_bytes','control','step','alignment','format','mode','width','file_bytes','file_header_bytes','padding_bytes','image_virtual_bytes','physical_uninit_section_bytes']);w.writerows(rows)
print(f'Layout checks: {checks} PASS; native executions: {native} PASS; total: {checks+native} PASS')
for r in rows:
 if r[0]==16 and r[1]==0 and r[2]<6 and r[5] in ('monolithic','kernel'):print(r[3],r[4],r[7],r[9],r[10])
