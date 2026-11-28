import struct as s, subprocess, csv, re, os
from pathlib import Path
p=Path('fixtures'); p.mkdir(exist_ok=True)
rows=[]; runs=[]
env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
def run(f,kind,n,m,i,expected,mode):
 cmd=['build/loader','--node',str(i),'--load-mode',mode,str(f)] if mode!='kernel' else [str(f)]
 q=subprocess.run(cmd,capture_output=True,text=True,timeout=5,env=env)
 regions={'monolithic':1,'scattered':4,'hybrid':2}.get(mode)
 ok=q.returncode==expected and (mode=='kernel' or (f'MATRIX_RELOC_OK runtime_header_addresses=4 regions={regions}' in q.stdout and re.search(r'actual mode:\s+'+mode+r'\b',q.stdout) and re.search(r'node:\s+'+str(i)+r' \(',q.stdout)))
 runs.append([n,m,kind,i,mode,q.returncode,expected,ok])
 if not ok: print(cmd,q.stdout,q.stderr);raise SystemExit(1)
for n in (2,4,8,16,32,64):
 for m in (16,256,4096,32768):
  # Each code has a unique immediate, reads both endpoints of common data.
  data=bytes([17])+bytes(m-2)+bytes([23]); codes=[]
  for i in range(n):
   # xor edi,edi; movabs rsi,addr; movzx eax,[rsi]; add edi,eax;
   # movzx eax,[rsi+disp32]; add edi,eax; add edi,imm8; exit
   code=b'\x31\xff\x48\xbe'+bytes(8)+b'\x0f\xb6\x06\x01\xc7\x0f\xb6\x86'+s.pack('<I',m-1)+b'\x01\xc7\x83\xc7'+bytes([i])+b'\xb8\x3c\x00\x00\x00\x0f\x05'
   codes.append(code)
  reloc=s.pack('<HHHHH',0,0,32,1,0x1004)
  for fmt,w,t in [(0,2,'H'),(1,4,'I'),(2,8,'Q')]:
   for shared in (True,False):
    names=[f'n{i:02d}'.encode() for i in range(n)]
    # share identical relocation bytes in both variants; only data sharing differs
    ds=[data] if shared else [data]*n
    sec=[(1,c) for c in codes]+[(4,d) for d in ds]+[(5,reloc)]
    h=8+6*n+18*n+(1+len(sec))*(2+2*w)+n+2*n+sum(map(len,names))
    b=s.pack('<3sBHH',b'_GM',1,n,n)+s.pack('<IH',(3<<27)|(2<<20)|(2<<18)|(10<<10)|0x2c,fmt)*n
    for i in range(n):
     b+=b''.join(s.pack('<HI',i,j) for j in (i+1,n+1+(0 if shared else i),len(sec)))
    b+=s.pack('<H'+t*2,0,h,h)+b''.join(s.pack('<H'+t*2,ty,len(v),len(v)) for ty,v in sec)
    b+=bytes(map(len,names))+b''.join(s.pack('<H',i) for i in range(n))+b''.join(names);assert len(b)==h
    kind=f'sh{w*8}_'+('shared' if shared else 'duplicated');f=p/f'{n}_{m}_{kind}.ueec';f.write_bytes(b+b''.join(v for _,v in sec))
    rows.append([n,m,kind,h,sum(map(len,codes)),len(ds)*m,len(reloc),f.stat().st_size]);assert f.stat().st_size==h+sum(map(len,codes))+len(ds)*m+len(reloc)
    for i in range(n):
     for mode in ('monolithic','scattered','hybrid'):run(f,kind,n,m,i,40+i,mode)
  total=0
  for i,c in enumerate(codes):
   code=bytearray(c); base=0x400000; start=120; end=start+len(code)+m;s.pack_into('<Q',code,4,base+start+len(code))
   hdr=s.pack('<16sHHIQQQIHHHHHH',b'\x7fELF'+bytes([2,1,1,0])+bytes(8),2,62,1,base+start,64,0,0,64,56,1,64,0,0)+s.pack('<IIQQQQQQ',1,5,0,base,base,end,end,4096)
   f=p/f'{n}_{m}_node{i:02d}.elf';f.write_bytes(hdr+code+data);f.chmod(0o755);total+=f.stat().st_size;run(f,'elf_set',n,m,i,40+i,'kernel')
  rows.append([n,m,'elf_set',120*n,sum(map(len,codes)),n*m,0,total]);print(f'nodes={n} data={m}: {(18+1)*n} PASS',flush=True)
for name,head,vals in [('results.csv',['nodes','data_per_node','format','headers','code','physical_data','relocation','file_size'],rows),('execution.csv',['nodes','data_per_node','format','node','mode','exit','expected','pass'],runs)]:
 with open(name,'w') as f: w=csv.writer(f);w.writerow(head);w.writerows(vals)
print(f'{len(runs)}/{len(runs)} PASS')
