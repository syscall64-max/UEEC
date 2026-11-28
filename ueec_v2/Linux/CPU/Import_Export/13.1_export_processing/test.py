import itertools, pathlib, struct, subprocess, sys, shlex, hashlib
ids=dict(zip('arm32 aarch64 loongarch64 ppc32 ppc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64 s390x'.split(),range(12)))
big={3,4,7,8,11}; order=[8,2,10,0,11,4,1,9,6,3,7,5]
folder=pathlib.Path('build/fixtures') / sys.argv[1]; folder.mkdir(parents=True,exist_ok=True)
def fixture(be,sh,fmt,design,bad=None):
 p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v); w='HIQ'[fmt]; sw='HIQ'[sh]; sections=[(1,b'\x90',16),(4,b'abcdef',16)]; mappings=[]
 for m in order: sections.append((2,bytes([m+20, m+40]),16))
 for j,m in enumerate(order):
  entries=[(b'a',1,1,3),(b'bb',2,0,7),(b'cc',0,0,11)]
  if bad=='section': entries[0]=(b'a',31,1,3)
  if bad=='offset': entries[0]=(b'a',1,16,3)
  if bad=='order': entries[1],entries[2]=entries[2],entries[1]
  if bad=='duplicate': entries[2]=(b'bb',0,0,7)
  r=pack('HHH'+w,fmt,design,fmt,2 if design==0 else 3)
  if design==0:
   r+=pack('H'+w,1,1)+pack('H'+w,2,2)
   for group in (entries[:1],entries[1:]): r+=b''.join(pack('B'+w,s,o) for n,s,o,k in group)+b''.join(n for n,s,o,k in group)
  elif design==1:
   pos=0; blob=b''
   for n,s,o,k in entries: r+=pack('H'+w*2,s|(len(n)<<5),o,pos); pos+=len(n);blob+=n
   r+=blob
  else: r+=b''.join(pack('B'+w*2,s,o,k) for n,s,o,k in entries)
  if bad=='format': r=pack('H',3)+r[2:]
  if bad=='count': r=r[:6]+pack(w,(1<<(8*struct.calcsize(w)))-1)+r[6+struct.calcsize(w):]
  if bad=='truncated': r=r[:-1]
  sections.append((6,r,len(r)));mappings.append([1,2,3+j,15+j])
 n=12; hs=8+6*n+6*4*n+(2+2*struct.calcsize(sw))*(1+len(sections))+1+2*n+4
 h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)+b''.join(pack('IH',int(m in big)|12|(m<<10)|(2<<18)|(2<<20)|(4<<27),sh) for m in order)
 h+=b''.join(pack('HI',j,k) for j,ks in enumerate(mappings) for k in ks)
 h+=pack('H'+sw*2,0,hs,hs)+b''.join(pack('H'+sw*2,t,s,len(d)) for t,d,s in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
 return h+b''.join(d.ljust(s,b'\0') for t,d,s in sections)
def run(arch,exe,runner):
 m=ids[arch]; cases=rejects=0
 def check(path,mode,kind,q,rc,out):
  sha=hashlib.sha256(path.read_bytes()).digest(); r=subprocess.run(shlex.split(runner)+[exe,mode,str(path),kind,str(q)],capture_output=True,text=True,timeout=30)
  assert r.returncode==rc and out in (r.stdout if rc!=1 else r.stderr) and 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr and sha==hashlib.sha256(path.read_bytes()).digest(),(path,mode,q,r.returncode,r.stdout,r.stderr)
 for be,sh,fmt,design in itertools.product(range(2),range(3),range(3),range(3)):
  path=folder/f'{be}-{sh}-{fmt}-{design}.ueec'; path.write_bytes(fixture(be,sh,fmt,design)); kind='ordinal' if design==2 else 'name'
  for mode in ('auto','monolithic','scattered','hybrid'):
   for q,s,o,v in zip((3,7,11) if design==2 else ('a','bb','cc'),(1,2,0),(1,0,0),(98,m+20,144)):
    check(path,mode,kind,q,0,f'FOUND node={order.index(m)} section={s} offset={o} byte={v}\n');cases+=1
   check(path,mode,kind,99 if design==2 else 'absent',3,'NOT_FOUND');cases+=1
  for bad in ('section','offset','order','duplicate','format','count','truncated'):
   path=folder/f'bad-{be}-{sh}-{fmt}-{design}-{bad}.ueec';path.write_bytes(fixture(be,sh,fmt,design,bad));check(path,'auto',kind,3 if design==2 else 'a',1,'invalid export');rejects+=1
 print(f'[PASS] {arch}: {cases} lookup cases, {rejects} malformed tables rejected')
run(*sys.argv[1:])
