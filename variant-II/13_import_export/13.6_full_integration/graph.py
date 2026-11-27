import test as t
import resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
import itertools,pathlib,struct,subprocess,sys,shlex,hashlib
arch,exe,runner=sys.argv[1:]
names={v:k for k,v in t.ids.items()}
def fixture(be,sh,fmt,role,bad=None):
 p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v); w='HIQ'[fmt]; sw='HIQ'[sh]
 sections=[]; maps=[]
 def name(x):return pack('H',len(x))+x
 def imports(groups):
  r=pack('HH'+w,fmt,0,len(groups))
  for lib,items in groups:
   r+=name(lib)+pack(w,len(items))
   for kind,off,sym in items:r+=pack('B'+w*2,kind,2,off)+(pack(w,sym) if kind else name(sym))
  return r
 for m in t.order:
  code=(pathlib.Path('build/payloads')/names[m]/('DATA.bin' if role!='common' else 'ADD.bin')).read_bytes()
  ss=[(1,code,len(code)),(4,b'd',16),(2,b'\x14' if role=='common' else bytes(48),48)]
  if role=='common':ss.append((3,b'',16))
  # Export a marker from every module to force the diamond dependencies.
  entries=[(b'constant',1),(b'marker',1),(b'value',3 if role=='common' else 1)]
  r=pack('HHH'+w,fmt,1,fmt,len(entries));blob=b''
  for sym,sec in entries:r+=pack('H'+w*2,sec|(len(sym)<<5),0,len(blob));blob+=sym
  r+=blob;ss.append((6,r,len(r)))
  if role=='common':
   r=pack('HHH'+w,fmt,2,fmt,1)+pack('B'+w*2,2,0,7);ss.append((6,r,len(r)))
   if bad=='cycle':
    r=imports([(b'libalpha',[(0,8,b'marker')])]);ss.append((7,r,len(r)))
  else:
   g=[(b'libcommon' if bad!='missing' else b'notfound',[(0,0,b'constant'),(1,8,7),(0,16,b'value')])]
   if role=='app':g += [(b'libalpha',[(0,24,b'marker')]),(b'libbeta',[(0,32,b'marker')])]
   if bad=='duplicate':g[0][1].append((0,0,b'constant'))
   r=imports(g);ss.append((7,r,len(r)))
  if role=='app':
   typ=0 if m in {0,3,5,7,9} else 1
   r=pack('HHHHH',0,0,2|(1<<5),1,(typ<<12)|(0 if bad=='overlap' else 40))
   ss.append((5,r,len(r)))
  maps.append(list(range(len(sections)+1,len(sections)+len(ss)+1)));sections+=ss
 n=12;k=len(maps[0]);hs=8+6*n+6*k*n+(2+2*struct.calcsize(sw))*(1+len(sections))+1+2*n+4
 h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)+b''.join(pack('IH',int(m in t.big)|12|(m<<10)|(2<<18)|(2<<20)|(k<<27),sh) for m in t.order)
 h+=b''.join(pack('HI',j,i) for j,ks in enumerate(maps) for i in ks)
 h+=pack('H'+sw*2,0,hs,hs)+b''.join(pack('H'+sw*2,typ,z,len(d)) for typ,d,z in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
 return h+b''.join(d.ljust(z,b'\0') for typ,d,z in sections)
roles=['app','alpha','beta','common'];paths=[t.folder/('graph-'+r+'.ueec') for r in roles]
def check(mode,rc,protect=False):
 hashes=[hashlib.sha256(p.read_bytes()).digest() for p in paths]
 args=[exe,'--graph-protect' if protect else '--graph',mode,str(paths[0])]+[x for i in range(1,4) for x in ('lib'+roles[i],str(paths[i]))]
 r=subprocess.run(shlex.split(runner)+args,capture_output=True,text=True,timeout=30)
 assert r.returncode==rc and 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,(r.returncode,r.stdout,r.stderr)
 if protect:assert 'PROTECTION_OK' in r.stdout,r.stdout
 assert hashes==[hashlib.sha256(p.read_bytes()).digest() for p in paths]
 assert [l for l in r.stdout.splitlines() if l.startswith('SHARED result=')]==(['SHARED result=172','SHARED result=224','SHARED result=276'] if rc==0 else []),r.stdout
cases=rejects=0
for be,sh,fmt in itertools.product(range(2),range(3),range(3)):
 def write(bad=None):
  for i,role in enumerate(roles):paths[i].write_bytes(fixture(be^(i%2),sh,fmt,role,bad if role==('common' if bad=='cycle' else 'app' if bad=='overlap' else 'alpha') else None))
 write()
 for mode in ('auto','monolithic','scattered','hybrid'):check(mode,0);cases+=1
 for bad in ('cycle','missing','duplicate','overlap'):write(bad);check('auto',1);rejects+=1
write();check('auto',0,True)
print(f'[PASS] {arch}: {cases} shared dependency cases, {rejects} graph rejections, protection check')
