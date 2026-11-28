import hashlib, pathlib, struct, subprocess, sys, shlex, json
ids = dict(zip('arm32 aarch64 loongarch64 ppc32 ppc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64 s390x'.split(),range(12)))
big={3,4,7,8,11}; order=[8,2,10,0,11,4,1,9,6,3,7,5]
folder=pathlib.Path('build/fixtures')
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def prepare():
 folder.mkdir(parents=True,exist_ok=True)
 manifest={}
 for be in (0,1):
  for fmt,width in enumerate('HIQ'):
   p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v)
   for value in (0,42,255):
    sections=[]; mappings=[]
    for j,i in enumerate(order):
     payload=(pathlib.Path('build/payloads') / next(a for a,k in ids.items() if k==i) / f'{(0,42,255)[((0,42,255).index(value)+i)%3]}.bin').read_bytes()
     sections.append((1,payload,128)); mappings.append([j+1,13,16+j,15,28+j])
    sections.extend([(4,b'shared',16),(2,b'unused',16),(3,b'',32)])
    for i in order:
     ptr=4 if i in {0,3,5,7,9} else 8
     sections.append((2,b'initial\0'+(1).to_bytes(ptr,'big' if i in big else 'little'),16))
    for i in order:
     ptr=4 if i in {0,3,5,7,9} else 8
     sections.append((5,pack('HHHHH',0,0,2|(1<<5),1,8|((ptr==8)<<12)),10))
    n=len(order); sh=lambda t,s,d:pack('H'+width*2,t,s,d)
    hs=8+6*n+6*5*n+(2+2*struct.calcsize(width))*(1+len(sections))+1+2*n+4
    h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)
    h+=b''.join(pack('IH',int(i in big)|12|(i<<10)|(2<<18)|(2<<20)|(5<<27),fmt) for i in order)
    h+=b''.join(pack('HI',j,k) for j,ks in enumerate(mappings) for k in ks)
    h+=sh(0,hs,hs)+b''.join(sh(t,s,len(d)) for t,d,s in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
    path=folder/f'{be}-{fmt}-{value}.ueec'; path.write_bytes(h+b''.join(d.ljust(s,b'\0') for t,d,s in sections)); manifest[path.name]=digest(path)
 (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print(f'Prepared {len(manifest)} common UEEC files')
def test(arch,exe,runner):
 m=ids[arch]; manifest=json.loads((folder/'manifest.json').read_text()); count=0
 for name,sha in manifest.items():
  path=folder/name; value=int(path.stem.split('-')[-1]); v=(0,42,255)[((0,42,255).index(value)+m)%3]
  first=104+105+v; second=104+2*(first%256)+v
  for mode in ('auto','monolithic','scattered','hybrid'):
   assert digest(path)==sha
   r=subprocess.run(shlex.split(runner)+[exe,'--load-mode',mode,str(path)],capture_output=True,text=True,timeout=30)
   expected=[f'node:                {order.index(m)} (test)', 'runtime sections:    5',f'regions: {dict(auto=1,monolithic=1,scattered=5,hybrid=2)[mode]}',f'global={order.index(m)+1} type=1 data=','global=13 type=4 data=6',f'global={16+order.index(m)} type=2 data=','global=15 type=3 data=0','RELOCATED ABS', 'LOADED',f'EXECUTED result={first} second={second} rw={second%256} bss={second%256}']
   if r.returncode or any(r.stdout.count(x+'\n' if x.startswith('EXECUTED') else x)!=3 for x in expected) or r.stdout.count('REPEAT_OK 3')!=1 or 'runtime error:' in r.stderr or 'AddressSanitizer' in r.stderr or digest(path)!=sha:
    raise SystemExit(f'FAIL {arch} {name} {mode}: {r.returncode}\n{r.stdout}\n{r.stderr}')
   count+=1
 report={'architecture':arch,'cases':count,'load_cycles':count*3,'native_calls':count*6,'sha256':manifest}
 pathlib.Path(exe).with_name('report.json').write_text(json.dumps(report,indent=2)+'\n')
 print(f'[PASS] {arch}: {count} cases, {count*3} load cycles, {count*6} native calls, unchanged SHA-256')
if sys.argv[1]=='prepare': prepare()
else: test(*sys.argv[1:])
