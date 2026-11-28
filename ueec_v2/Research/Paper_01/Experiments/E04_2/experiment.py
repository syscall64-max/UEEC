import struct as s, subprocess, csv, os, platform
from pathlib import Path
F=Path('fixtures');F.mkdir(exist_ok=True)
T=['H','I','Q']; W=[2,4,8]; modes=['monolithic','scattered','hybrid']; rows=[]; runs=[]; bounds=[]; conversions=[]
env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
def sh(fmt,ty,sz,ds):
 if not 0<=ds<=sz<=2**(8*W[fmt])-1: raise ValueError('size outside target representation')
 return s.pack('<H'+T[fmt]*2,ty,sz,ds)
def make(fmt,sections,pad=b''):
 name=b'width'; h=8+6+18+4*(2+2*W[fmt])+3+len(name)
 b=s.pack('<3sBHHIH',b'_GM',1,1,1,(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)
 b+=b''.join(s.pack('<HI',0,i) for i in (1,2,3))
 b+=sh(fmt,0,h+len(pad),h)+b''.join(sh(fmt,ty,len(v),ds) for ty,v,ds in sections)
 b+=bytes([len(name)])+s.pack('<H',0)+name
 assert len(b)==h
 return b+pad+b''.join(v for ty,v,ds in sections)
def decode(b):
 # Restricted converter: little-endian, one node, fixed experiment topology.
 if b[:8]!=s.pack('<3sBHH',b'_GM',1,1,1) or len(b)<32: raise ValueError('unsupported header')
 word,fmt=s.unpack_from('<IH',b,8)
 if word!=(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c or fmt not in range(3): raise ValueError('unsupported node')
 if b[14:32]!=b''.join(s.pack('<HI',0,i) for i in (1,2,3)): raise ValueError('unsupported mappings')
 step=2+2*W[fmt]; end=32+4*step
 hs=[s.unpack_from('<H'+T[fmt]*2,b,32+i*step) for i in range(4)]
 h=end+8
 if b[end:h]!=b'\x05\x00\x00width' or hs[0][0]!=0 or hs[0][2]!=h or hs[0][1]<h or hs[0][1]>len(b): raise ValueError('bad HEADER')
 pos=hs[0][1]; sec=[]
 for (ty,sz,ds),expected in zip(hs[1:],(1,4,5)):
  if ty!=expected or not 0<=ds<=sz or not sz or pos+sz>len(b): raise ValueError('invalid section')
  sec.append((ty,b[pos:pos+sz],ds));pos+=sz
 if pos!=len(b):raise ValueError('trailing bytes')
 return fmt,sec,b[h:hs[0][1]]
def run(f,fmt,sz,ds,mode,expected):
 q=subprocess.run(['build/loader','--load-mode',mode,str(f)],capture_output=True,text=True,env=env,timeout=10)
 ok=(q.returncode==expected and ('MATRIX_RELOC_OK runtime_header_addresses=4 regions='+str({'monolithic':1,'scattered':4,'hybrid':2}[mode]) in q.stdout if expected==40 else 'runtime HEADER does not fit section format' in q.stderr))
 runs.append([f.name,fmt,sz,ds,mode,expected,q.returncode,ok]);
 if not ok: raise RuntimeError((f,q.returncode,q.stdout,q.stderr))
def choose(pairs, alignment=1):
 if alignment<1: raise ValueError('alignment')
 if any(not 0<=d<=z for z,d in pairs): raise ValueError('invalid sizes')
 m=max(((z+alignment-1)//alignment*alignment for z,d in pairs),default=0)
 for f,w in enumerate(W):
  if m<=2**(8*w)-1: return f
 raise ValueError('overflow')
page=os.sysconf('SC_PAGE_SIZE')
for n,expected in [(0,0),(1,0),(65534,0),(65535,0),(65536,1),(2**32-1,1),(2**32,2),(2**64-1,2),(2**64,None)]:
 for pos in range(5):
  pairs=[(0,0)]*5;pairs[pos]=(n,n)
  try: f=choose(pairs)
  except ValueError: f=None
  assert f==expected
  if f is not None:
   records=[sh(f,4,z,d) for z,d in pairs]
   assert all(len(x)==2+2*W[f] for x in records)
   assert [s.unpack('<H'+T[f]*2,x)[1:] for x in records]==pairs
  bounds.append([n,pos,expected,f,'PASS'])
for pairs in [[(5,6)],[(-1,0)],[(2**64,0)]]:
 try: choose(pairs)
 except ValueError: bounds.append([str(pairs),-1,None,None,'PASS'])
 else: raise AssertionError('invalid accepted')
for n in (16,256,4096,61440,61441,65534,65535,65536,65537):
 for ds in (n,n-2):
  data=bytes([17])+bytes(ds-2)+bytes([23])+bytes(n-ds)
  code=b'\x31\xff\x48\xbe'+bytes(8)+b'\x0f\xb6\x06\x01\xc7\x0f\xb6\x86'+s.pack('<I',ds-1)+b'\x01\xc7\x83\xc7\x00\xb8\x3c\x00\x00\x00\x0f\x05'
  reloc=s.pack('<HHHHH',0,0,32,1,0x1004)
  sec=[(1,code,len(code)),(4,data,ds),(5,reloc,len(reloc))]
  pairs=[(len(v),d) for _,v,d in sec]+[(100,100)]
  for policy,a in [('logical',1),('runtime_compatible',page)]:
   fmt=choose(pairs,a);out=make(fmt,sec);assert decode(out)[0]==fmt
   file=F/f'{n}_{ds}_{policy}.ueec';file.write_bytes(out)
   ok=choose(pairs,page)<=fmt
   for mode in modes:run(file,fmt,n,ds,mode,40 if ok else 1)
   rows.append([n,ds,policy,W[fmt]*8,len(out),ok])
for name,head,vals in [('boundaries.csv',['value','position','expected_format','actual_format','result'],bounds),('results.csv',['section_size','data_size','policy','width','file_size','runtime_representable'],rows),('execution.csv',['file','format_id','section_size','data_size','mode','expected_exit','actual_exit','pass'],runs)]:
 with open(name,'w') as f:w=csv.writer(f);w.writerow(head);w.writerows(vals)
print(f'Width selection: {len(bounds)}/{len(bounds)} PASS')
print(f'Generated files: {len(rows)}')
print(f'Loader checks: {len(runs)}/{len(runs)} PASS; executed={sum(x[5]==40 for x in runs)} expected_rejections={sum(x[5]==1 for x in runs)}')
print(f'Total: {len(bounds)+len(runs)} PASS')
Path('environment.txt').write_text(platform.platform()+'\npage_size='+str(page)+'\n')
