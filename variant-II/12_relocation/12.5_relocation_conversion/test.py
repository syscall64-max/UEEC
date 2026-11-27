import time
from convert import convert, sections as parse_sections, decode, FORMATS
import itertools, hashlib, pathlib, struct, subprocess, sys, shlex, json
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
   for value, (design,rf) in itertools.product((0,42,255), [(0,0),(1,0),(1,1),(1,2),(2,0),(2,1),(2,2)]):
    sections=[]; mappings=[]; large=design!=0
    for j,i in enumerate(order):
     payload=(pathlib.Path('build/payloads') / next(a for a,k in ids.items() if k==i) / f'{(0,42,255)[((0,42,255).index(value)+i)%3]}.bin').read_bytes()
     sections.append((1,payload,128)); mappings.append([j+1,13,16+j,15,28+j])
    sections.extend([(4,b'shared',16),(2,b'unused',16),(3,b'',32)])
    for i in order:
     ptr=4 if i in {0,3,5,7,9} else 8
     slots=b''.join(x.to_bytes(ptr,'big' if i in big else 'little').ljust(8,b'\0') for x in (1,2,3,0,1,0,1,0))
     data=b'initial\0'+slots
     if large: data=data.ljust(4096,b'\0')+slots; data=data.ljust(8200,b'\0')
     sections.append((2,data, len(data) if large else 80))
    for i in order:
     ptr=4 if i in {0,3,5,7,9} else 8
     r=pack('HH',rf,design)
     rw='HIQ'[rf]; shift=(2<<rf)*8-4
     def couple(c, offsets, typ):
      if not design: return pack('HH',c,len(offsets))+b''.join(pack('H',x|(typ<<12)) for x in offsets)
      page=4096 if design==1 else 24
      groups={}
      for x in offsets: groups.setdefault(x//page*page,[]).append(x)
      return pack('HH',c,len(groups))+b''.join(pack(rw*2,base,len(xs))+b''.join(pack(rw,(x-base)|(typ<<shift)) for x in xs) for base,xs in groups.items())
     for source,slots in ((1,(8,16,24)),(2,(32,40)),(3,(48,56)),(0,(64,))):
      r+=couple(2|(source<<5),slots+tuple(4096+x-8 for x in slots) if large else slots, int(ptr==8))
     r+=couple(0,(8,),2)
     sections.append((5,r,len(r)))
    n=len(order); sh=lambda t,s,d:pack('H'+width*2,t,s,d)
    hs=8+6*n+6*5*n+(2+2*struct.calcsize(width))*(1+len(sections))+1+2*n+4
    h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)
    h+=b''.join(pack('IH',int(i in big)|12|(i<<10)|(2<<18)|(2<<20)|(5<<27),fmt) for i in order)
    h+=b''.join(pack('HI',j,k) for j,ks in enumerate(mappings) for k in ks)
    h+=sh(0,hs,hs)+b''.join(sh(t,s,len(d)) for t,d,s in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
    path=folder/f'{be}-{fmt}-{value}-{design}-{rf}.ueec'; path.write_bytes(h+b''.join(d.ljust(s,b'\0') for t,d,s in sections)); manifest[path.name]=digest(path)
 # Normalize small FLAT fixtures to all seven source representations; large
 # fixtures already exist in all six block representations.
 converted={}; stats=[]; rejected=0
 def equivalent(a,b):
  pa,_,_,_,ma,ba,_=parse_sections(a); pb,_,_,_,mb,bb,_=parse_sections(b)
  assert pa==pb and len(ma)==len(mb)
  for (ta,sa,da),(tb,sb,db),x,y in zip(ma[1:],mb[1:],ba,bb):
   assert ta==tb
   if ta==5: assert decode(x[:da],pa)==decode(y[:db],pb) and x[da:]==y[db:]
   else: assert (sa,da,x)==(sb,db,y)
 for be,fmt in itertools.product((0,1),range(3)):
  original=(folder/f'{be}-{fmt}-42-0-0.ueec').read_bytes()
  for large in (0,1):
   for src,(sd,sf) in FORMATS.items():
    if large and not sd: continue
    if large: source=(folder/f'{be}-{fmt}-42-{sd}-{sf}.ueec').read_bytes()
    else: source,_=convert(original,sd,sf)
    for dst,(dd,df) in FORMATS.items():
     if src==dst: continue
     t=time.perf_counter_ns()
     if large and not dd:
      try: convert(source,dd,df)
      except ValueError as e: assert 'not representable' in str(e); rejected+=1
      else: raise AssertionError('lossy conversion accepted')
      continue
     result,sizes=convert(source,dd,df); elapsed=time.perf_counter_ns()-t
     equivalent(source,result)
     back,_=convert(result,sd,sf); equivalent(source,back)
     path=folder/f'{be}-{fmt}-42-{dd}-{df}-{large}-{src}-{dst}.ueec'
     path.write_bytes(result); converted[path.name]=digest(path)
     stats.append(dict(source=src,target=dst,large=large,section_bytes_before=sum(x for x,y in sizes),section_bytes_after=sum(y for x,y in sizes),conversion_ns=elapsed))
 assert len(converted)==432 and rejected==36
 (folder/'manifest.json').write_text(json.dumps(converted,indent=2)+'\n')
 (folder/'conversion-report.json').write_text(json.dumps(dict(conversions=len(converted),round_trips=len(converted),rejections=rejected,measurements=stats),indent=2)+'\n')
 print(f'Prepared {len(converted)} converted UEEC files; {rejected} unrepresentable conversions rejected')
def test(arch,exe,runner):
 m=ids[arch]; manifest=json.loads((folder/'manifest.json').read_text()); count=0
 for name,sha in manifest.items():
  path=folder/name; _,_,value,design,rf,large=map(int,path.stem.split('-')[:6]);  v=(0,42,255)[((0,42,255).index(value)+m)%3]
  first=104+105+v; second=104+2*(first%256)+v
  for mode in ('auto','monolithic','scattered','hybrid'):
   assert digest(path)==sha
   r=subprocess.run(shlex.split(runner)+[exe,'--load-mode',mode,str(path)],capture_output=True,text=True,timeout=30)
   expected=[f'node:                {order.index(m)} (test)', 'runtime sections:    5',f'regions: {dict(auto=1,monolithic=1,scattered=5,hybrid=2)[mode]}',f'global={order.index(m)+1} type=1 data=','global=13 type=4 data=6',f'global={16+order.index(m)} type=2 data=','global=15 type=3 data=0',f'RELOCATED ABS{32 if m in {0,3,5,7,9} else 64} couples=5 entries={17 if large else 9} design={design} format={rf}', 'LOADED',f'EXECUTED result={first} second={second} rw={second%256} bss={second%256}']
   if r.returncode or any(r.stdout.count(x+'\n' if x.startswith('EXECUTED') else x)!=3 for x in expected) or r.stdout.count('REPEAT_OK 3')!=1 or 'runtime error:' in r.stderr or 'AddressSanitizer' in r.stderr or digest(path)!=sha:
    raise SystemExit(f'FAIL {arch} {name} {mode}: {r.returncode}\n{r.stdout}\n{r.stderr}')
   count+=1
 rejected=0
 for be in (0,1):
  path=folder/f'{be}-0-0-0-0.ueec'; original=path.read_bytes(); j=order.index(m)
  base=len(original)-42*12+j*42; init=len(original)-42*12-80*12+j*80
  p='>' if be else '<'; typ=0 if m in {0,3,5,7,9} else 1; ptr=4 if typ==0 else 8
  for name,offset,data in [('format',base,struct.pack(p+'H',3)),('source',base+4,struct.pack(p+'H',2|(31<<5))),('target',base+4,struct.pack(p+'H',31|(1<<5))),('count',base+6,struct.pack(p+'H',65535)),('zero_count',base+6,struct.pack(p+'H',0)),('offset',base+8,struct.pack(p+'H',4095|(typ<<12))),('type',base+8,struct.pack(p+'H',8|(15<<12))),('overlap',base+10,struct.pack(p+'H',8|(typ<<12))),('negative_addend',init+8,b'\xff'*ptr),('branch_target',base+36,struct.pack(p+'H',2)),('branch_offset',base+40,struct.pack(p+'H',4095|(2<<12))),('branch_opcode',len(original)-(12*128+64+12*80+12*42)+j*128+8,b'\0'*4)]:
   bad=folder/f'bad-{arch}-{be}-{name}.ueec'; buf=bytearray(original); buf[offset:offset+len(data)]=data; bad.write_bytes(buf)
   r=subprocess.run(shlex.split(runner)+[exe,'--load-mode','auto',str(bad)],capture_output=True,text=True,timeout=30)
   if r.returncode!=1 or 'invalid relocation' not in r.stderr or 'LOADED' in r.stdout or 'AddressSanitizer' in r.stderr or 'runtime error:' in r.stderr:
    raise SystemExit(f'FAIL rejection {arch} {name}: {r.returncode}\n{r.stdout}\n{r.stderr}')
   rejected+=1
 for be,design,rf in itertools.product((0,1),(1,2),(0,1,2)):
  original=(folder/f'{be}-0-0-{design}-{rf}.ueec').read_bytes(); p='>' if be else '<'; w=2<<rf; rw='HIQ'[rf]; shift=w*8-4
  # All 36 section sizes follow the node/mapping arrays and the global header.
  sh=8+6*12+6*5*12; sizes=[struct.unpack_from(p+'H',original,sh+6*k+2)[0] for k in range(40)]
  base=sum(sizes[:28+order.index(m)]); j=order.index(m)
  changes=[('design',base+2,struct.pack(p+'H',3)),('empty_block',base+8+w,struct.pack(p+rw,0)),('huge_count',base+8+w,struct.pack(p+rw,(1<<(8*w))-1)),('bad_base',base+8,struct.pack(p+rw,(1<<(8*w))-1)),('bad_type',base+8+2*w,struct.pack(p+rw,15<<shift))]
  for name,offset,data in changes:
   bad=folder/f'bad-{arch}-{be}-{design}-{rf}-{name}.ueec'; buf=bytearray(original); buf[offset:offset+len(data)]=data; bad.write_bytes(buf)
   r=subprocess.run(shlex.split(runner)+[exe,'--load-mode','auto',str(bad)],capture_output=True,text=True,timeout=30)
   if r.returncode!=1 or 'invalid relocation' not in r.stderr or 'LOADED' in r.stdout or 'AddressSanitizer' in r.stderr or 'runtime error:' in r.stderr:
    raise SystemExit(f'FAIL block rejection {arch} {name}: {r.returncode}\n{r.stdout}\n{r.stderr}')
   rejected+=1
 report={'rejections':rejected,'architecture':arch,'cases':count,'load_cycles':count*3,'native_calls':count*6,'sha256':manifest}
 pathlib.Path(exe).with_name('report.json').write_text(json.dumps(report,indent=2)+'\n')
 print(f'[PASS] {arch}: {count} cases, {count*3} load cycles, {count*6} native calls, unchanged SHA-256, {rejected} rejections')
if sys.argv[1]=='prepare': prepare()
else: test(*sys.argv[1:])
