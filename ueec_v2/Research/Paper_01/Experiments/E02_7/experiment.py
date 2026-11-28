import struct as s, subprocess, csv, re, os
from pathlib import Path
p=Path('fixtures'); p.mkdir(exist_ok=True)
rows=[]; runs=[]
for n in (2,4,8,16,24,32):
 k=0 if n==2 else n-3
 for case in ('base','control'):
  # xor edi,edi; repeated movabs rsi,address; movzx eax,byte [rsi]; add edi,eax; mov eax,60; syscall
  data=[bytes([j+1+(case=='control' and j==0)])+bytes(15) for j in range(k)]
  code=b'\x31\xff'+b''.join(b'\x48\xbe'+bytes(8)+b'\x0f\xb6\x06\x01\xc7' for _ in data)+b'\xb8\x3c\x00\x00\x00\x0f\x05'
  if not k: code=b'\xbf'+s.pack('<I',42+(case=='control'))+b'\xb8\x3c\x00\x00\x00\x0f\x05'
  expected=(sum(d[0] for d in data) if k else 42+(case=='control'))%256
  reloc=(s.pack('<HH',0,0)+b''.join(s.pack('<HHH',32*(j+1),1,0x1000| (4+15*j)) for j in range(k))) if k else b''
  def run(f,kind,h,sh,ph,gap,r):
   assert f.stat().st_size==h+len(code)+16*k+len(r)+gap
   if case=='base': rows.append([n,k,kind,ph,sh,h,len(code),16*k,len(r),gap,f.stat().st_size])
   ueec=kind.startswith('sh'); env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
   for mode in ('monolithic','scattered','hybrid') if ueec else ('kernel',):
    cmd=['build/loader','--load-mode',mode,str(f)] if ueec else [str(f)]
    q=subprocess.run(cmd,capture_output=True,text=True,timeout=5,env=env)
    regions={'monolithic':1,'scattered':n,'hybrid':2}.get(mode)
    ok=q.returncode==expected and (not ueec or f'MATRIX_RELOC_OK runtime_header_addresses={n} regions={regions}' in q.stdout and re.search(r'actual mode:\s+'+mode+r'\b',q.stdout))
    print(f'{f} {mode} exit={q.returncode} expected={expected} {"PASS" if ok else "FAIL"}');runs.append([n,case,kind,mode,q.returncode,expected,ok]);
    if not ok: print(q.stdout,q.stderr);raise SystemExit(1)
  for fmt,w,t in [(0,2,'H'),(1,4,'I'),(2,8,'Q')]:
   h=14+6*(n-1)+n*(2+2*w)+6
   b=s.pack('<3sBHH',b'_GM',1,1,1)+s.pack('<IH',((n-1)<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)
   b+=b''.join(s.pack('<HI',0,i) for i in range(1,n))
   sections=[(0,h,h),(1,len(code),len(code))]+[(4,16,16)]*k+([(5,len(reloc),len(reloc))] if k else [])
   b+=b''.join(s.pack('<H'+t*2,*v) for v in sections)+s.pack('<BH',3,0)+b'sum';assert len(b)==h
   kind=f'sh{w*8}';f=p/f'{n}_{case}_{kind}.ueec';f.write_bytes(b+code+b''.join(data)+reloc);run(f,kind,h,n,0,0,reloc)
  for kind in ('compact','section_table','split'):
   ph=1+k if kind=='split' else 1; start=64+56*ph; base=0x400000
   offsets=[4096*(j+1) if kind=='split' else start+len(code)+16*j for j in range(k)]
   c=bytearray(code)
   for j,o in enumerate(offsets): s.pack_into('<Q',c,4+15*j,base+o)
   end=(offsets[-1]+16) if k else start+len(c)
   sh=2+k if kind=='section_table' else 0; shoff=(end+7)//8*8 if sh else 0
   hdr=s.pack('<16sHHIQQQIHHHHHH',b'\x7fELF'+bytes([2,1,1,0])+bytes(8),2,62,1,base+start,64,shoff,0,64,56,ph,64,sh,0)
   first=start+len(c) if kind=='split' else end
   hdr+=s.pack('<IIQQQQQQ',1,5,0,base,base,first,first,4096)
   if kind=='split':
    for o in offsets: hdr+=s.pack('<IIQQQQQQ',1,4,o,base+o,base+o,16,16,4096)
   b=bytearray(hdr+c)
   for o,d in zip(offsets,data): b.extend(bytes(o-len(b)));b.extend(d)
   if sh:
    b.extend(bytes(shoff-len(b)));b.extend(bytes(64))
    for o,l,flags in [(start,len(c),6)]+[(o,16,2) for o in offsets]: b.extend(s.pack('<IIQQQQIIQQ',0,1,flags,base+o,o,l,0,0,1,0))
   f=p/f'{n}_{case}_{kind}.elf';f.write_bytes(b);f.chmod(0o755)
   h=len(hdr)+64*sh;run(f,kind,h,sh,ph,len(b)-h-len(c)-16*k,b'')
for name,head,vals in [('results.csv',['ueec_sections','data_sections','format','program_headers','section_headers','header_bytes','code','data','relocation','gap','file_size'],rows),('execution.csv',['ueec_sections','case','format','mode','exit','expected','pass'],runs)]:
 with open(name,'w') as f: w=csv.writer(f);w.writerow(head);w.writerows(vals)
print(Path('results.csv').read_text(),end='');print(f'{len(runs)}/{len(runs)} PASS')
