import itertools, pathlib, struct, subprocess, sys, shlex, hashlib
ids=dict(zip('arm32 aarch64 loongarch64 ppc32 ppc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64 s390x'.split(),range(12)))
big={3,4,7,8,11}; order=[8,2,10,0,11,4,1,9,6,3,7,5]
folder=pathlib.Path('build/fixtures') / sys.argv[1]; folder.mkdir(parents=True,exist_ok=True)
def app_fixture(be,sh,fmt,bad=None):
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
def lib_fixture(be,sh,fmt,design,bad=None):
 p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v); w='HIQ'[fmt]; sw='HIQ'[sh]; sections=[(1,b'\x90',16),(4,b'abcdef',16)]; mappings=[]
 for m in order: sections.append((2,bytes([m+20,m+40]),16))
 for j,m in enumerate(order):
  entries=[(b'answer',1,1),(b'value',2,0)]
  r=pack('HHH'+w,fmt,design,fmt,2)
  if design==0:
   r+=pack('H'+w,5,1)+pack('H'+w,6,1)
   for n,sec,off in reversed(entries):r+=pack('B'+w,sec,off)+n
  else:
   pos=0;blob=b''
   for n,sec,off in entries:r+=pack('H'+w*2,sec|(len(n)<<5),off,pos);pos+=len(n);blob+=n
   r+=blob
  o=pack('HHH'+w,fmt,2,fmt,1)+pack('B'+w*2,2,1,7)
  if bad:r=r[:-1]
  sections.extend([(6,r,len(r)),(6,o,len(o))]);mappings.append([1,2,3+j,15+2*j,16+2*j])
 n=12;hs=8+6*n+6*5*n+(2+2*struct.calcsize(sw))*(1+len(sections))+1+2*n+4
 h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)+b''.join(pack('IH',int(m in big)|12|(m<<10)|(2<<18)|(2<<20)|(5<<27),sh) for m in order)
 h+=b''.join(pack('HI',j,k) for j,ks in enumerate(mappings) for k in ks)
 h+=pack('H'+sw*2,0,hs,hs)+b''.join(pack('H'+sw*2,t,z,len(d)) for t,d,z in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
 return h+b''.join(d.ljust(z,b'\0') for t,d,z in sections)
def run(arch,exe,runner):
 cases=rejects=0;m=ids[arch];app=folder/'app.ueec';alpha=folder/'alpha.ueec';beta=folder/'beta.ueec'
 def check(mode,rc,extra=None):
  paths=[app,alpha,beta];hashes=[hashlib.sha256(p.read_bytes()).digest() for p in paths];args=['libalpha',str(alpha),'libbeta',str(beta)] if extra is None else extra
  r=subprocess.run(shlex.split(runner)+[exe,mode,str(app)]+args,capture_output=True,text=True,timeout=30)
  assert r.returncode==rc and 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,(r.returncode,r.stdout,r.stderr)
  assert hashes==[hashlib.sha256(p.read_bytes()).digest() for p in paths]
  if rc==0:
   assert 'RESOLVED imports=3' in r.stdout and all(f'BOUND section=2 offset={o} byte={v}' in r.stdout for o,v in [(0,98),(8,m+40),(16,m+20)]),r.stdout
 for be,sh,fmt,design in itertools.product(range(2),range(3),range(3),range(2)):
  app.write_bytes(app_fixture(be,sh,fmt));alpha.write_bytes(lib_fixture(1-be,sh,fmt,design));beta.write_bytes(lib_fixture(be,sh,fmt,1-design))
  for mode in ('auto','monolithic','scattered','hybrid'):check(mode,0);cases+=1
  for bad in ('duplicate','section','readonly','truncated'):
   app.write_bytes(app_fixture(be,sh,fmt,bad));check('auto',1);rejects+=1
  app.write_bytes(app_fixture(be,sh,fmt));check('auto',1,['libalpha',str(alpha)]);rejects+=1
  check('auto',1,['libalpha',str(alpha),'libalpha',str(beta)]);rejects+=1
  alpha.write_bytes(lib_fixture(be,sh,fmt,design,True));check('auto',1);rejects+=1
 print(f'[PASS] {arch}: {cases} resolution cases, {rejects} rejections')
if __name__ == "__main__": run(*sys.argv[1:])
