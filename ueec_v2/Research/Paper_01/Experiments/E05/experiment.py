import csv, os, platform, struct, subprocess
from pathlib import Path
F=Path('fixtures'); F.mkdir(exist_ok=True)
# Same x86-64 little-endian code for both container byte orders.
# xor edi,edi; movabs rsi,0; cmp dword [rsi],0x12345678;
# setne dil; add edi,42; mov eax,60; syscall
code=bytes.fromhex('31ff48be0000000000000000813e78563412400f95c783c72ab83c0000000f05')
widths=(2,4,8); modes=('monolithic','scattered','hybrid'); runs=[]; sizes=[]
env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
def make(w, endian, control=False, payload_endian=0):
 p='>' if endian else '<'; fmt=widths.index(w); name=b'endian'; data=struct.pack('<I',0x12345679 if control else 0x12345678)+bytes(12)
 reloc=struct.pack(p+'HHHHH',0,0,32,1,0x1004)
 h=8+6+18+4*(2+2*w)+3+len(name)
 b=struct.pack(p+'3sBHHIH',b'_GM',1|(endian<<7),1,1,(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c|payload_endian,fmt)
 b+=b''.join(struct.pack(p+'HI',0,i) for i in (1,2,3))
 for t,n in [(0,h),(1,len(code)),(4,len(data)),(5,len(reloc))]: b+=struct.pack(p+'H'+{2:'H',4:'I',8:'Q'}[w]*2,t,n,n)
 b+=bytes([len(name)])+struct.pack(p+'H',0)+name
 assert len(b)==h
 return b+code+data+reloc,h,data

def run(path,w,endian,kind,expected):
 for mode in modes:
  q=subprocess.run(['build/loader','--load-mode',mode,str(path)],capture_output=True,text=True,env=env,timeout=10)
  marker='MATRIX_RELOC_OK runtime_header_addresses=4 regions='+str(dict(monolithic=1,scattered=4,hybrid=2)[mode])
  ok=q.returncode==expected and (marker in q.stdout if expected in (42,43) else marker not in q.stdout)
  runs.append([path.name,w*8,endian,kind,mode,expected,q.returncode,ok])
  if not ok: raise RuntimeError((path,mode,q.returncode,q.stdout,q.stderr))
for w in widths:
 for control in (False,True):
  le,h,d=make(w,0,control);be,h2,d2=make(w,1,control)
  assert h==h2 and d==d2 and le[h:h+len(code)+16]==be[h:h+len(code)+16] and len(le)==len(be)
  for e,b in enumerate((le,be)):
   tag=f'sh{w*8}_{"be" if e else "le"}_{"control" if control else "base"}'
   f=F/(tag+'.ueec');f.write_bytes(b);run(f,w,e,'control' if control else 'base',43 if control else 42)
   sizes.append([w*8,e,control,h,len(code),16,10,len(b)])
 for e in (0,1):
  b,_,_=make(w,e,payload_endian=1);f=F/f'sh{w*8}_{e}_bad_payload_flag.ueec';f.write_bytes(b);run(f,w,e,'bad_payload_flag',1)
  b,_,_=make(w,e);b=bytearray(b);b[3]^=128;f=F/f'sh{w*8}_{e}_bad_container_flag.ueec';f.write_bytes(b);run(f,w,e,'bad_container_flag',1)
for filename,header,rows in [('execution.csv',['file','width','container_endian','case','mode','expected_exit','actual_exit','pass'],runs),('results.csv',['width','container_endian','control','header_bytes','code_bytes','constant_bytes','relocation_bytes','file_size'],sizes)]:
 with open(filename,'w') as f: wr=csv.writer(f);wr.writerow(header);wr.writerows(rows)
print('Native executions:',sum(r[5] in (42,43) for r in runs))
print('Expected rejections:',sum(r[5]==1 for r in runs))
print(f'{len(runs)}/{len(runs)} PASS')
for w in widths:
 print(f'SH{w*8}: LE={len(make(w,0)[0])} B BE={len(make(w,1)[0])} B')
Path('environment.txt').write_text(platform.platform()+'\npage_size='+str(os.sysconf('SC_PAGE_SIZE'))+'\n')
