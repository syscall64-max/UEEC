import itertools, pathlib, struct, subprocess, sys, shlex, hashlib
ids=dict(zip('arm32 aarch64 loongarch64 ppc32 ppc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64 s390x'.split(),range(12)))
big={3,4,7,8,11}; order=[8,2,10,0,11,4,1,9,6,3,7,5]
folder=pathlib.Path('build/fixtures') / sys.argv[1]; folder.mkdir(parents=True,exist_ok=True)
def fixture(be,sh,fmt,bad=None):
 p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v); w='HIQ'[fmt]; sw='HIQ'[sh]; sections=[(1,b'\x90',16),(4,b'abcdef',16)]; mappings=[]
 for m in order: sections.append((2,bytes(32),32))
 for j,m in enumerate(order):
  r=pack('HH'+w,fmt,0,2)
  def name(n):return pack('H',len(n))+n
  r+=name(b'libalpha')+pack(w,2)+pack('B'+w*2,0,2,0)+name(b'answer')+pack('B'+w*3,1,2,8,7)
  r+=name(b'libbeta')+pack(w,1)+pack('B'+w*2,0,2,16)+name(b'value')
  if bad=='format':r=pack('H',3)+r[2:]
  if bad=='design':r=r[:2]+pack('H',1)+r[4:]
  if bad=='count':r=r[:4]+pack(w,0)+r[4+struct.calcsize(w):]
  if bad=='truncated':r=r[:-1]
  if bad=='trailing':r+=b'X'
  if bad=='name':r=r.replace(b'libalpha',b'lib\0lpha')
  k=4+struct.calcsize(w)+2+8+struct.calcsize(w)
  if bad=='kind':r=r[:k]+b'\x02'+r[k+1:]
  if bad=='section':r=r[:k+1]+pack(w,31)+r[k+1+struct.calcsize(w):]
  if bad=='readonly':r=r[:k+1]+pack(w,1)+r[k+1+struct.calcsize(w):]
  if bad in ('offset','alignment'):i=k+1+struct.calcsize(w);r=r[:i]+pack(w,32 if bad=='offset' else 1)+r[i+struct.calcsize(w):]
  if bad=='duplicate':i=k+1+struct.calcsize(w);r=r[:i]+pack(w,8)+r[i+struct.calcsize(w):]
  sections.append((7,r,len(r)));mappings.append([1,2,3+j,15+j])
 n=12; hs=8+6*n+6*4*n+(2+2*struct.calcsize(sw))*(1+len(sections))+1+2*n+4
 h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)+b''.join(pack('IH',int(m in big)|12|(m<<10)|(2<<18)|(2<<20)|(4<<27),sh) for m in order)
 h+=b''.join(pack('HI',j,k) for j,ks in enumerate(mappings) for k in ks)
 h+=pack('H'+sw*2,0,hs,hs)+b''.join(pack('H'+sw*2,t,s,len(d)) for t,d,s in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
 return h+b''.join(d.ljust(s,b'\0') for t,d,s in sections)
def run(arch,exe,runner):
 cases=rejects=0
 def check(path,mode,rc):
  sha=hashlib.sha256(path.read_bytes()).digest();r=subprocess.run(shlex.split(runner)+[exe,mode,str(path)],capture_output=True,text=True,timeout=30)
  assert r.returncode==rc and ('VALID imports=3' in r.stdout if rc==0 else 'invalid import' in r.stderr) and 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr and sha==hashlib.sha256(path.read_bytes()).digest(),(path,mode,r.returncode,r.stdout,r.stderr)
  if rc==0:
   for line in ('IMPORT library=libalpha section=2 offset=0 name=answer','IMPORT library=libalpha section=2 offset=8 ordinal=7','IMPORT library=libbeta section=2 offset=16 name=value'):assert line in r.stdout
 for be,sh,fmt in itertools.product(range(2),range(3),range(3)):
  path=folder/f'{be}-{sh}-{fmt}.ueec';path.write_bytes(fixture(be,sh,fmt))
  for mode in ('auto','monolithic','scattered','hybrid'):check(path,mode,0);cases+=1
  for bad in ('format','design','count','truncated','trailing','name','kind','section','readonly','offset','alignment','duplicate'):
   path=folder/f'bad-{be}-{sh}-{fmt}-{bad}.ueec';path.write_bytes(fixture(be,sh,fmt,bad));check(path,'auto',1);rejects+=1
 print(f'[PASS] {arch}: {cases} valid import tables, {rejects} malformed tables rejected')
run(*sys.argv[1:])
