import test as t
import itertools, pathlib, struct, subprocess, sys, shlex, hashlib
arch,exe,runner=sys.argv[1:]; folder=t.folder
names={v:k for k,v in t.ids.items()}
def fixture(be,sh,fmt,design,kind,bad=None):
 p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v); w='HIQ'[fmt]; sw='HIQ'[sh]
 sections=[]; mappings=[]
 for m in t.order:
  payload=pathlib.Path('build/payloads')/names[m]/(kind+'.bin')
  code=payload.read_bytes()
  sections.append((1,code,len(code)))
  sections.append((4,b'd' if kind!='DATA' else b'abcdef',16));sections.append((2,b'\x14'+bytes(31) if kind!='DATA' else bytes(32),32))
  if kind=='DATA':
   def name(n):return pack('H',len(n))+n
   r=pack('HH'+w,fmt,0,2)+name(b'libalpha')+pack(w,2)+pack('B'+w*2,0,2,0)+name(b'answer')+pack('B'+w*3,1,2,8,7)+name(b'libbeta')+pack(w,1)+pack('B'+w*2,0,2,16)+name(b'value')
   if bad=='missing':r=r.replace(b'answer',b'absent')
   if bad=='slot':
    k=4+struct.calcsize(w)+2+8+struct.calcsize(w)+1+struct.calcsize(w);r=r[:k]+pack(w,24)+r[k+struct.calcsize(w):]
   sections.append((7,r,len(r)));mappings.append(list(range(len(sections)-3,len(sections)+1)))
  else:
   sections.append((3,b'',16))
   entries=[(b'answer',0 if bad=='data' else 1,0),(b'value',3,0)]
   r=pack('HHH'+w,fmt,design,fmt,2)
   if design==0:
    r+=pack('H'+w,5,1)+pack('H'+w,6,1)
    for n,sec,off in reversed(entries):r+=pack('B'+w,sec,off)+n
   else:
    pos=0;blob=b''
    for n,sec,off in entries:r+=pack('H'+w*2,sec|(len(n)<<5),off,pos);pos+=len(n);blob+=n
    r+=blob
   o=pack('HHH'+w,fmt,2,fmt,1)+pack('B'+w*2,2,0,7)
   sections.extend([(6,r,len(r)),(6,o,len(o))]);mappings.append(list(range(len(sections)-5,len(sections)+1)))
 n=12; k=len(mappings[0]);hs=8+6*n+6*k*n+(2+2*struct.calcsize(sw))*(1+len(sections))+1+2*n+4
 h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)+b''.join(pack('IH',int(m in t.big)|12|(m<<10)|(2<<18)|(2<<20)|(k<<27),sh) for m in t.order)
 h+=b''.join(pack('HI',j,i) for j,ks in enumerate(mappings) for i in ks)
 h+=pack('H'+sw*2,0,hs,hs)+b''.join(pack('H'+sw*2,typ,z,len(d)) for typ,d,z in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
 return h+b''.join(d.ljust(z,b'\0') for typ,d,z in sections)
paths=[folder/(n+'.ueec') for n in ('data-app','data-alpha','data-beta')]
def check(mode,rc):
 hashes=[hashlib.sha256(p.read_bytes()).digest() for p in paths]
 r=subprocess.run(shlex.split(runner)+[exe,'--data',mode,str(paths[0]),'libalpha',str(paths[1]),'libbeta',str(paths[2])],capture_output=True,text=True,timeout=30)
 assert r.returncode==rc and 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,(r.returncode,r.stdout,r.stderr)
 assert hashes==[hashlib.sha256(p.read_bytes()).digest() for p in paths]
 assert [line for line in r.stdout.splitlines() if line.startswith('DATA result=')]==(['DATA result=172','DATA result=224','DATA result=276'] if rc==0 else []),r.stdout
cases=rejects=0
for be,sh,fmt,design in itertools.product(range(2),range(3),range(3),range(2)):
 def write(bad=None):
  for i,kind in enumerate(('DATA','ADD','MUL')):paths[i].write_bytes(fixture(be^(i==1),sh,fmt,design if i!=2 else 1-design,kind,bad if (i==0 and bad!='data') or (i==1 and bad=='data') else None))
 write()
 for mode in ('auto','monolithic','scattered','hybrid'):check(mode,0);cases+=1
 for bad in ('missing','slot','data'):write(bad);check('auto',1);rejects+=1
print(f'[PASS] {arch}: {cases} external-data cases, {cases*3} native invocations, {rejects} rejections')
