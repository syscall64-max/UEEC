import csv, os, platform, struct as s, subprocess
from pathlib import Path
F=Path('fixtures'); F.mkdir(exist_ok=True)
page=os.sysconf('SC_PAGE_SIZE'); W=(2,4,8); T=('H','I','Q'); modes=('monolithic','scattered','hybrid')
variants=[(0,0,0)]+[(1,i,4096) for i in range(3)]+[(2,i,1024) for i in range(3)]
rows=[]; runs=[]; conversions=[]; relr_rows=[]; blocksizes=[]; negative=[]
def label(v):return ('FLAT','BLOCK4096','BLOCKN')[v[0]]+str(16<<v[1])
def encode(offsets,v,e):
 d,f,span=v; w=W[f]; p='>' if e else '<'; typ=T[f]; limit=1<<(w*8-4)
 if d==0:
  if any(x>=limit for x in offsets):raise ValueError('flat offset overflow')
  return s.pack(p+'HHHH',f,d,65,len(offsets))+b''.join(s.pack(p+typ,x|limit) for x in offsets),0
 groups={}
 for x in offsets:
  base=x//span*span; delta=x-base
  if base>=1<<(w*8) or delta>=limit:raise ValueError('block width overflow')
  groups.setdefault(base,[]).append(delta)
 out=s.pack(p+'HHHH',f,d,65,len(groups))
 for base,ds in groups.items():out+=s.pack(p+typ*2,base,len(ds))+b''.join(s.pack(p+typ,x|limit) for x in ds)
 return out,len(groups)
def decode(blob,e):
 p='>' if e else '<';f,d,pair,n=s.unpack_from(p+'HHHH',blob);assert pair==65 and f<=2 and d<=2
 w=W[f];typ=T[f];limit=1<<(8*w-4);pos=8;out=[]
 for _ in range(n if d else 1):
  base,k=s.unpack_from(p+typ*2,blob,pos) if d else (0,n)
  if d:pos+=2*w
  for _ in range(k):
   x=s.unpack_from(p+typ,blob,pos)[0];pos+=w;assert x//limit==1;out.append(base+x%limit)
 assert pos==len(blob);return out
def align(n,a):return (n+a-1)//a*a
def build(code,data,reloc,e):
 p='>' if e else '<';name=b'reloc7';parts=[(1,code),(4,data),(5,reloc)]
 for f,w in enumerate(W):
  h=8+6+18+4*(2+2*w)+3+len(name)
  if max(align(x,page) for x in [h]+[len(b) for _,b in parts])<1<<(8*w):break
 head=s.pack(p+'3sBHHIH',b'_GM',1|(e<<7),1,1,(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,f)
 head+=b''.join(s.pack(p+'HI',0,i) for i in (1,2,3))
 for ty,b in [(0,bytes(h))]+parts:head+=s.pack(p+'H'+T[f]*2,ty,len(b),len(b))
 head+=bytes([len(name)])+s.pack(p+'H',0)+name;assert len(head)==h
 return head+code+data+reloc,f
# All pointer slots are read by a fixed loop; each slot stores an implicit addend.
def payload(n,stride,ctrl):
 first=128;offsets=[first+i*stride for i in range(n)]
 code=bytearray(b'\x48\x8d\x35'+s.pack('<i',first-7)+b'\xb9'+s.pack('<I',n)+b'\x31\xff')
 loop=len(code);code+=bytes.fromhex('488b060fb60001c74881c6')+s.pack('<I',stride)+bytes.fromhex('ffc9');code+=b'\x75'+s.pack('b',loop-(len(code)+2))+bytes.fromhex('b83c0000000f05')
 assert len(code)<first;code.extend(bytes(offsets[-1]+8-len(code)))
 data=bytes([3+i+ctrl for i in range(16)])
 for i,x in enumerate(offsets):s.pack_into('<Q',code,x,i%16)
 return bytes(code),data,offsets,sum(data[i%16] for i in range(n))%256
def relr_encode(offsets):
 # ELF64 relative offsets, 63 pointer slots per bitmap, no ELF file/loader involved.
 out=[];i=0
 while i<len(offsets):
  x=offsets[i];assert x%8==0;out.append(x);i+=1;cursor=x+8
  while i<len(offsets) and offsets[i]<cursor+63*8:
   bits=1
   while i<len(offsets) and offsets[i]<cursor+63*8:
    bits|=1<<((offsets[i]-cursor)//8+1);i+=1
   out.append(bits);cursor+=63*8
 return out
def relr_decode(words):
 out=[];cursor=0
 for v in words:
  if v&1:
   out.extend(cursor+8*i for i in range(63) if v&(1<<(i+1)));cursor+=63*8
  else:out.append(v);cursor=v+8
 return out
cases=[('dense1',1,8),('dense8',8,8),('dense64',64,8),('dense256',256,8),('dense512',512,8),('dense1024',1024,8),('sparse16',16,256),('sparse64',64,256),('page16',16,4096),('page64',64,4096)]
env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
for case,n,stride in cases:
 _,_,offsets,_=payload(n,stride,0)
 words=relr_encode(offsets);assert relr_decode(words)==offsets
 relr_rows.append([case,n,stride,16*n,24*n,8*len(words)])
 for span in (128,512,1024,4096,16384):
  for f in range(3):
   try:b,ng=encode(offsets,(2,f,span),0);assert decode(b,0)==offsets;blocksizes.append([case,span,16<<f,ng,len(b),'PASS'])
   except ValueError:blocksizes.append([case,span,16<<f,'','','UNREPRESENTABLE'])
 for e in (0,1):
  available={}
  for v in variants:
   try:b,ng=encode(offsets,v,e)
   except ValueError:rows.append([case,n,stride,e,label(v),'','','','UNREPRESENTABLE']);continue
   assert decode(b,e)==offsets;available[v]=b
   for ctrl in (0,1):
    code,data,_,expected=payload(n,stride,ctrl);blob,fmt=build(code,data,b,e)
    path=F/f'{case}_e{e}_{label(v)}_c{ctrl}.ueec';path.write_bytes(blob)
    for mode in modes:
     q=subprocess.run(['build/loader','--load-mode',mode,str(path)],capture_output=True,text=True,env=env,timeout=10)
     marker='MATRIX_RELOC_OK runtime_header_addresses=4 regions='+str(dict(monolithic=1,scattered=4,hybrid=2)[mode])
     assert q.returncode==expected and marker in q.stdout,(path,mode,q.returncode,q.stdout,q.stderr)
     runs.append([path.name,mode,expected,q.returncode,'PASS'])
   rows.append([case,n,stride,e,label(v),ng,len(b),len(blob),'PASS'])
  for src,b in available.items():
   for dst in variants:
    decoded=decode(b,e)
    try:out,_=encode(decoded,dst,e)
    except ValueError:
     assert dst not in available;conversions.append([case,e,label(src),label(dst),'EXPECTED_REJECTION']);continue
    assert out==available[dst] and decode(out,e)==offsets
    conversions.append([case,e,label(src),label(dst),'PASS'])
 # corrupt the first valid design: trailing byte, bad pair, invalid type, offset beyond code (block64)
 for e in (0,1):
  code,data,_,_=payload(n,stride,0);b,_=encode(offsets,(1,2,4096),e);p='>' if e else '<'
  for kind,bad in [('trailing',b+b'\0'),('pair',b[:4]+s.pack(p+'H',0)+b[6:]),('type',b[:24]+s.pack(p+'Q',2<<60)+b[32:]),('base',b[:8]+s.pack(p+'Q',1<<40)+b[16:])]:
   blob,_=build(code,data,bad,e);path=F/f'{case}_e{e}_bad_{kind}.ueec';path.write_bytes(blob)
   for mode in modes:
    q=subprocess.run(['build/loader','--load-mode',mode,str(path)],capture_output=True,text=True,env=env,timeout=10)
    assert q.returncode==1 and 'E07 relocation rejected' in q.stderr and 'MATRIX_RELOC_OK' not in q.stdout,(path,mode,q.returncode,q.stdout,q.stderr)
    negative.append([path.name,mode,'PASS'])
for name,head,values in [('results.csv',['case','count','stride','endian','design','blocks','relocation_bytes','file_bytes','status'],rows),('execution.csv',['file','mode','expected','actual','status'],runs),('conversion.csv',['case','endian','source','destination','status'],conversions),('elf_structural.csv',['case','count','stride','REL64_bytes','RELA64_bytes','RELR64_bytes'],relr_rows),('block_sizes.csv',['case','block_size','width','blocks','bytes','status'],blocksizes),('rejections.csv',['file','mode','status'],negative)]:
 with open(name,'w') as f:wr=csv.writer(f);wr.writerow(head);wr.writerows(values)
print('Native executions:',len(runs),'PASS');print('Conversion checks:',len(conversions),'PASS; expected rejections:',sum(x[-1]=='EXPECTED_REJECTION' for x in conversions));print('Malformed relocation rejections:',len(negative),'PASS');print('ELF RELR encode/decode checks:',len(relr_rows),'PASS');print('BLOCK_N parameter checks:',len(blocksizes),'PASS; unrepresentable:',sum(x[-1]=='UNREPRESENTABLE' for x in blocksizes));total=len(runs)+len(conversions)+len(negative)+len(relr_rows)+len(blocksizes);print(f'Total: {total}/{total} PASS')
for r in rows:
 if r[0] in ('dense256','page64') and r[3]==0:print(r[0],r[4],r[6],r[-1])
Path('environment.txt').write_text(platform.platform()+'\npage_size='+str(page)+'\n')
