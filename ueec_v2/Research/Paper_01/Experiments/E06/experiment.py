import csv, os, platform, struct as s, subprocess
from pathlib import Path
F=Path('fixtures');F.mkdir(exist_ok=True)
W=(2,4,8); T=('H','I','Q'); modes=('monolithic','scattered','hybrid')
page=os.sysconf('SC_PAGE_SIZE'); env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
rows=[]; runs=[]; checks=[]
def align(n,a): return (n+a-1)//a*a
def choose(sizes):
 m=max(sizes)
 for i,w in enumerate(W):
  if m<2**(8*w):return i
 raise ValueError('size overflow')
def build(parts,a,e):
 p='>' if e else '<';name=b'padding'
 for fmt,w in enumerate(W):
  h=8+6+18+4*(2+2*w)+3+len(name)
  sizes=[align(h,a)]+[align(len(b),a) for _,b in parts]
  # Producer accounts for the existing loader's page-rounded runtime representation.
  if max(align(x,page) for x in sizes)<2**(8*w):break
 else:raise ValueError('size overflow')
 head=s.pack(p+'3sBHHIH',b'_GM',1|(e<<7),1,1,(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)
 head+=b''.join(s.pack(p+'HI',0,i) for i in (1,2,3))
 for (ty,b),sz in zip([(0,bytes(h))]+parts,sizes):head+=s.pack(p+'H'+T[fmt]*2,ty,sz,len(b))
 head+=bytes([len(name)])+s.pack(p+'H',0)+name
 assert len(head)==h
 return head.ljust(sizes[0],b'\0')+b''.join(b.ljust(sz,b'\0') for (_,b),sz in zip(parts,sizes[1:])),fmt,sizes,h

def decode(b):
 e=b[3]>>7;p='>' if e else '<';fmt=s.unpack_from(p+'H',b,12)[0];w=W[fmt];step=2+2*w
 hs=[s.unpack_from(p+'H'+T[fmt]*2,b,32+i*step) for i in range(4)]
 pos=hs[0][1];parts=[]
 for ty,sz,ds in hs[1:]:
  assert 0<ds<=sz and pos+sz<=len(b) and not any(b[pos+ds:pos+sz])
  parts.append((ty,b[pos:pos+ds]));pos+=sz
 assert pos==len(b) and not any(b[hs[0][2]:hs[0][1]])
 return parts,e

def run(f,expected):
 for mode in modes:
  q=subprocess.run(['build/loader','--load-mode',mode,str(f)],capture_output=True,text=True,env=env,timeout=10)
  marker='MATRIX_RELOC_OK runtime_header_addresses=4 regions='+str(dict(monolithic=1,scattered=4,hybrid=2)[mode])
  ok=q.returncode==expected and marker in q.stdout
  runs.append([f.name,mode,expected,q.returncode,ok]);assert ok,(f,mode,q.returncode,q.stdout,q.stderr)
for e in (0,1):
 for n in (16,63,511,4095,61441,65535):
  # Reads first and last nonzero byte, verifies the trailing logical zero.
  # xor edi,edi; movabs rsi,0; movzx eax,[rsi]; add edi,eax;
  # movzx eax,[rsi+n-3]; add edi,eax; movzx eax,[rsi+n-1]; add edi,eax; exit
  code=bytes.fromhex('31ff48be')+bytes(8)+bytes.fromhex('0fb60601c70fb686')+s.pack('<I',n-3)+bytes.fromhex('01c70fb686')+s.pack('<I',n-1)+bytes.fromhex('01c7b83c0000000f05')
  for ctrl in (0,1):
   data=bytes([17+ctrl])+bytes(n-4)+bytes([23,0,0]);assert len(data)==n
   parts=[(1,code),(4,data),(5,s.pack(('>' if e else '<')+'HHHHH',0,0,32,1,0x1004))]
   base=build(parts,1,e)[0]
   current=base
   for a in (1,16,64,512,4096,65536,64,1):
    decoded,endian=decode(current);assert decoded==parts and endian==e
    out,fmt,sizes,h=build(decoded,a,e);assert decode(out)==(parts,e)
    if a==1:assert out==base
    checks.append([e,n,ctrl,a,'PASS']);current=out
    # execute every transformation, including repeated alignments
    f=F/f'e{e}_n{n}_c{ctrl}_a{a}.ueec';f.write_bytes(out);run(f,40+ctrl)
    rows.append([e,n,ctrl,a,W[fmt]*8,h,len(code),n,10,sum(sizes)-h-len(code)-n-10,len(out)])
# Pure representability selection independent of runtime alignment, no allocation.
for n,expected in [(65535,0),(65536,1),(2**32-1,1),(2**32,2),(2**64-1,2)]:assert choose([1,n])==expected
try:choose([2**64]);raise AssertionError('overflow accepted')
except ValueError:pass
for name,head,values in [('results.csv',['endian','data_bytes','control','alignment','width','header_bytes','code_bytes','constant_bytes','relocation_bytes','padding_bytes','file_size'],rows),('execution.csv',['file','mode','expected','actual','pass'],runs),('transformations.csv',['endian','data_bytes','control','alignment','result'],checks)]:
 with open(name,'w') as f:wr=csv.writer(f);wr.writerow(head);wr.writerows(values)
print('Page size:',page)
print(f'Transformations: {len(checks)}/{len(checks)} PASS')
print(f'Native executions: {len(runs)}/{len(runs)} PASS')
print('Width boundary checks: 6/6 PASS')
print(f'Total: {len(checks)+len(runs)+6}/{len(checks)+len(runs)+6} PASS')
for r in rows:
 if r[:3]==[0,16,0]:print('alignment=%d SH%d padding=%d file=%d'%(r[3],r[4],r[-2],r[-1]))
Path('environment.txt').write_text(platform.platform()+'\npage_size='+str(page)+'\n')
