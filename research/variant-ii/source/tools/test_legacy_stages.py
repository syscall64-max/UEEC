#!/usr/bin/env python3
"""Run trusted fixtures for migrated experimental SH16 stages 03–09."""
from pathlib import Path
import argparse, struct, re, shlex, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
ARCH=('arm32','aarch64','loongarch64','ppc32','ppc64','riscv32','riscv64','sparc32','sparc64','x86','x86_64','s390x')
BIG={'ppc32','ppc64','sparc32','sparc64','s390x'}
p=argparse.ArgumentParser();p.add_argument('stage',type=int);p.add_argument('arch',choices=ARCH);p.add_argument('build',type=Path);p.add_argument('--runner',default='');a=p.parse_args()
b=a.build.resolve();runner=shlex.split(a.runner);n=0

def run(argv,expected=None,runner_=(),bad=False):
 global n
 cmd=list(runner_)+[str(x) for x in argv]
 res=subprocess.run(cmd,check=False,capture_output=True,timeout=45)
 if (bad and res.returncode!=1) or (not bad and res.returncode!=0) or (expected and not expected(res.stdout)) or b'runtime error:' in res.stderr or b'AddressSanitizer' in res.stderr:
  print('FAIL',cmd,res.returncode,res.stdout.decode(errors='replace'),res.stderr.decode(errors='replace'),file=sys.stderr);sys.exit(1)
 n+=1
 return res.stdout

def asm(text):
 out=bytearray()
 for line in text.decode().splitlines():
  line=line.split(';',1)[0].strip()
  if not line:continue
  m=re.fullmatch(r'(db|dw|dd|dq)\s+(.+)',line)
  if not m: raise ValueError(line)
  w={'db':1,'dw':2,'dd':4,'dq':8}[m[1]]
  for token in m[2].split(','):out.extend(int(token.strip(),0).to_bytes(w,'little'))
 return bytes(out)

def fixture(name,gen,args=(),cwd=None):
 cmd=[str(gen)]+[str(x) for x in args]
 # Generator 03 uses paths relative to stage 03; all other generators use current stage.
 res=subprocess.run(cmd,cwd=cwd,check=True,capture_output=True,timeout=30)
 if b'runtime error:' in res.stderr or b'AddressSanitizer' in res.stderr:
  raise RuntimeError(res.stderr.decode(errors='replace'))
 path=b/name; path.write_bytes(asm(res.stdout)); return path

def parse(path):
 data=path.read_bytes(); assert data[:4]==b'_GM\x02'
 fmt,nodes,names=struct.unpack_from('<HHH',data,4); assert fmt==0 and nodes and names
 off=10; words=list(struct.unpack_from('<'+'I'*nodes,data,off)); off+=4*nodes
 info=[]
 for i in range(sum((w>>27)&31 for w in words)):
  info.append(struct.unpack_from('<HI',data,off));off+=6
 assert all(0<=i<nodes and j>0 for i,j in info)
 sections=max(j for i,j in info)+1; headers=[]
 for _ in range(sections):headers.append(struct.unpack_from('<HHH',data,off));off+=6
 lengths=data[off:off+names];off+=names
 name_idxs=struct.unpack_from('<'+'H'*nodes,data,off);off+=nodes*2
 assert all(i<names for i in name_idxs)
 off+=sum(lengths);assert off==headers[0][2] and headers[0][0]==0
 cursor=0; payload=[]
 for t,sz,ds in headers:
  assert sz>=ds and cursor+sz<=len(data)
  payload.append(data[cursor:cursor+ds]);cursor+=sz
 assert cursor==len(data)
 return words,info,headers,payload

def runloader(name,path,mode=None,export=False,ordinal=False):
 cmd=[b/name]+([f'--L={mode}'] if mode else [])+[path]
 def check(s):
  if export:
   vals=[int(x) for x in re.findall(rb'value = (-?\d+)',s)]
   return b'answer() = 42' in s and sorted(vals)==([42] if ordinal else [11,22,33,42])
  return b'Hello world!\n' in s
 run(cmd,expected=check,runner_=runner)

if a.stage in (3,4,5,6):
 src=fixture('input.ueec',b/'gen_reloc',cwd=ROOT/'03_reloc'); original=parse(src)
 assert len(original[0])==12 and len(original[1])==36 and len(original[2])==26
 assert original[3][1]==b'Hello world!\n'
 for i,m in enumerate(ARCH):
  assert ((original[0][i]>>10)&255)==i
  assert [s for j,s in original[1] if j==i]==[1,2+2*i,3+2*i]
 if a.stage==3:
  for mode in ('monolithic','scattered'):runloader('loader',src,mode)
 elif a.stage==4:
  paths=[b/x for x in ('flat.ueec','block.ueec','blockn.ueec','back.ueec','block2.ueec','blockn2.ueec')]
  for inp,out,design,extra in [(src,paths[0],'flat',[]),(src,paths[1],'block4096',[]),(src,paths[2],'blockn',['--page-size','1000000']),(paths[1],paths[3],'flat',[]),(paths[2],paths[4],'block4096',[]),(paths[1],paths[5],'blockn',['--page-size','500000'])]:
   run([b/'convert',inp,out,'--design',design]+extra);parse(out)
  for x,y in [(src,paths[0]),(src,paths[3]),(paths[1],paths[4]),(paths[2],paths[5])]:assert x.read_bytes()==y.read_bytes()
  for path in paths:
   for mode in ('monolithic','scattered'):runloader('loader',path,mode)
 elif a.stage==5:
  for g in (1,16,512,4096,8192):
   out=b/f'g{g}.ueec';run([b/'align',src,out,'--granularity',g]); q=parse(out)
   assert q[:2]==original[:2]
   assert all(h[1]==((h[2]+g-1)//g)*g for h in q[2])
   assert [h[::2] for h in q[2]]==[h[::2] for h in original[2]] and q[3][1:]==original[3][1:]
   cursor=0; raw=out.read_bytes()
   for _,sz,ds in q[2]:
    assert not any(raw[cursor+ds:cursor+sz]);cursor+=sz
   back=b/f'g{g}-back.ueec';run([b/'align',out,back,'--granularity',1]);assert back.read_bytes()==src.read_bytes()
   for mode in ('monolithic','scattered'):runloader('loader',out,mode)
  for g in ('0','-1','65536','abc'):
   run([b/'align',src,b/'invalid.ueec','--granularity',g],bad=True)
 elif a.stage==6:
  for i,m in enumerate(ARCH):
   out=b/f'{m}.ueec';run([b/'extract',src,out,'--machine',i]); q=parse(out)
   assert q[0]==[original[0][i]] and q[1]==[(0,1),(0,2),(0,3)]
   assert q[3][1:]==[original[3][1],original[3][2+2*i],original[3][3+2*i]]
   if m==a.arch:
    for mode in ('monolithic','scattered'):runloader('loader',out,mode)
  run([b/'extract',src,b/'ambiguous.ueec','--name','hello'],bad=True)
  padded=b/'padded.ueec';run([b/'align',src,padded,'--granularity',512]); pq=parse(padded)
  i=ARCH.index(a.arch); out=b/'padded-extracted.ueec';run([b/'extract',padded,out,'--machine',i]); q=parse(out)
  assert q[0]==[original[0][i]] and q[1]==[(0,1),(0,2),(0,3)]
  assert q[2][1:]==[pq[2][1],pq[2][2+2*i],pq[2][3+2*i]]
  assert q[3][1:]==[original[3][1],original[3][2+2*i],original[3][3+2*i]]
  for mode in ('monolithic','scattered'):runloader('loader',out,mode)
  run([b/'extract',src,b/'missing.ueec','--machine',255],bad=True)
else:
 i=ARCH.index(a.arch)
 if a.stage==7:
  for j,m in enumerate(ARCH):
   src=fixture(f'{m}.ueec',b/'gen_pe',[j]);q=parse(src)
   assert len(q[0])==1 and (q[0][0]>>10)&255==j and [h[0] for h in q[2]]==[0,2,1,5]
   assert q[3][1]==b'Hello world!\n' and struct.unpack_from('<HH',q[3][3])==(0,3)
   assert q[1]==[(0,1),(0,2),(0,3)] and (q[0][0]&1)==(m in BIG)
   offset,width=((32,4),(32,8),(36,8),(40,4),(40,8),(36,4),(36,8),(40,4),(40,8),(1,4),(2,8),(32,8))[j]
   code=bytearray((ROOT/'test_resources/pe'/f'{m}.bin').read_bytes());code[offset:offset+width]=bytes(width)
   assert q[3][2]==code
   assert struct.unpack('<7H',q[3][3])==(0,3,1,1,0,1,offset | ((width==8)<<12))
   if j==i:
    runloader('loader_pe',src)
    for label, pos, value in [('old-layout',3,1),('wrong-format',4,1),('multiple-nodes',6,2),('wrong-design',len(src.read_bytes())-12,0)]:
     raw=bytearray(src.read_bytes());raw[pos]=value;out=b/f'{label}.ueec';out.write_bytes(raw)
     run([b/'loader_pe',out],runner_=runner,bad=True)
    run([b/'loader_pe','--L=scattered',src],runner_=runner,bad=True)
 elif a.stage in (8,9):
  for j,m in enumerate(ARCH):
   args=[ROOT/'test_resources/export'/f'{m}.bin',j,'be' if m in BIG else 'le']
   for gen,design in [('gen_export',0),('gen_export_ordinal',2)] if a.stage==8 else [('gen_export',0)]:
    src=fixture(f'{m}-{design}.ueec',b/gen,args);q=parse(src)
    assert len(q[0])==1 and (q[0][0]>>10)&255==j and [h[0] for h in q[2]]==[0,2,1,6]
    assert (q[0][0]&1)==(m in BIG) and struct.unpack_from('<HH',q[3][3])==(0,design)
    assert q[3][2]==args[0].read_bytes(); e='>' if m in BIG else '<'
    assert struct.unpack(e+('i' if design==2 else '4i'),q[3][1])==((42,) if design==2 else (42,11,22,33))
    if a.stage==8 and j==i:runloader('loader_export_ordinal' if design==2 else 'loader_export',src,export=True,ordinal=design==2)
    if a.stage==9:
     flat=b/f'{m}-flat.ueec';back=b/f'{m}-back.ueec';flat2=b/f'{m}-flat2.ueec'
     for x,y,d in [(src,flat,'flat'),(flat,back,'compact'),(back,flat2,'flat')]:run([b/'convert_export',x,y,'--design',d]);parse(y)
     assert src.read_bytes()==back.read_bytes() and flat.read_bytes()==flat2.read_bytes()
     if j==i:
      for path in (src,flat,back,flat2):runloader('loader_convert_export',path,export=True)
print(f'[PASS] stage {a.stage:02d} {a.arch}: {n} subprocess checks + structural assertions')
