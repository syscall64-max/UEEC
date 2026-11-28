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
def convert(b,target):
 _,sec,pad=decode(b);return make(target,sec,pad)
def run(f,fmt,sz,ds,mode,expected):
 q=subprocess.run(['build/loader','--load-mode',mode,str(f)],capture_output=True,text=True,env=env,timeout=10)
 ok=(q.returncode==expected and ('MATRIX_RELOC_OK runtime_header_addresses=4 regions='+str({'monolithic':1,'scattered':4,'hybrid':2}[mode]) in q.stdout if expected==40 else 'runtime HEADER does not fit section format' in q.stderr))
 runs.append([f.name,fmt,sz,ds,mode,expected,q.returncode,ok]);
 if not ok: raise RuntimeError((f,q.returncode,q.stdout,q.stderr))
# Isolated section record boundary checks, no giant allocation.
for fmt in range(3):
 for v in (0,1,65534,65535,65536,4294967294,4294967295,4294967296,2**64-2,2**64-1,2**64):
  for ds in (0,v):
   expected=v<=2**(8*W[fmt])-1
   try:
    b=sh(fmt,4,v,ds); ok=s.unpack('<H'+T[fmt]*2,b)==(4,v,ds)
   except ValueError: ok=False
   assert ok==expected;bounds.append([16*2**fmt,v,ds,expected,ok])
page=os.sysconf('SC_PAGE_SIZE');print('page_size=',page)
for sz in (16,256,4096,61440,61441,65534,65535,65536,65537):
 for ds in (sz,sz-2):
  data=bytes([17])+bytes(ds-2)+bytes([23])+bytes(sz-ds)
  code=b'\x31\xff\x48\xbe'+bytes(8)+b'\x0f\xb6\x06\x01\xc7\x0f\xb6\x86'+s.pack('<I',ds-1)+b'\x01\xc7\x83\xc7\x00\xb8\x3c\x00\x00\x00\x0f\x05'
  reloc=s.pack('<HHHHH',0,0,32,1,0x1004)
  sections=[(1,code,len(code)),(4,data,ds),(5,reloc,len(reloc))]
  for src in range(3):
   if sz>2**(8*W[src])-1:continue
   original=make(src,sections,b'\0\x7f\0'); source=F/f'{sz}_{ds}_sh{W[src]*8}.ueec';source.write_bytes(original)
   for dst in range(3):
    expected=sz<=2**(8*W[dst])-1
    try:
     out=convert(original,dst)
    except ValueError:
     assert not expected;conversions.append([sz,ds,W[src]*8,W[dst]*8,'rejected']);continue
    assert expected and decode(out)[1:]==decode(original)[1:] and convert(out,src)==original
    conversions.append([sz,ds,W[src]*8,W[dst]*8,'roundtrip_exact'])
    # Execute all ordered conversions including identity.
    f=F/f'{sz}_{ds}_{W[src]*8}_to_{W[dst]*8}.ueec';f.write_bytes(out)
    runtime_ok=((sz+page-1)//page)*page<=2**(8*W[dst])-1
    for mode in modes:run(f,W[dst]*8,sz,ds,mode,40 if runtime_ok else 1)
    rows.append([sz,ds,W[src]*8,W[dst]*8,len(original),len(out),runtime_ok])
for name,head,vals in [('boundaries.csv',['width','section_size','data_size','representable','encoded'],bounds),('conversion.csv',['section_size','data_size','source_width','target_width','result'],conversions),('execution.csv',['file','width','section_size','data_size','mode','expected_exit','actual_exit','pass'],runs),('results.csv',['section_size','data_size','source_width','target_width','source_file_size','target_file_size','runtime_representable'],rows)]:
 with open(name,'w') as f:w=csv.writer(f);w.writerow(head);w.writerows(vals)
print(f'field checks: {len(bounds)}/{len(bounds)} PASS')
print(f'conversion checks: {len(conversions)}/{len(conversions)} PASS; exact={sum(x[-1]=="roundtrip_exact" for x in conversions)} rejected={sum(x[-1]=="rejected" for x in conversions)}')
print(f'loader checks: {len(runs)}/{len(runs)} PASS; executed={sum(x[5]==40 for x in runs)} expected_rejections={sum(x[5]==1 for x in runs)}')
Path('environment.txt').write_text(platform.platform()+'\npage_size='+str(page)+'\n')
