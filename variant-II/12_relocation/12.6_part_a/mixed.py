"""Mixed relocation sections and conversion microbenchmark."""
import pathlib, struct, json, hashlib, subprocess, shlex, sys, time, statistics
from convert import sections, decode, encode, FORMATS
F=pathlib.Path('build/fixtures'); order=[8,2,10,0,11,4,1,9,6,3,7,5]
ids=dict(zip('arm32 aarch64 loongarch64 ppc32 ppc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64 s390x'.split(),range(12)))
def mixed(data,block_n,overlap=False):
 p,start,sf,sw,meta,bodies,he=sections(data); pack=lambda f,*v:struct.pack(p+f,*v)
 n=12; mapping_start=8+6*n; oldmaps=data[mapping_start:start]; maps=bytearray(oldmaps); extra=[]
 for j in range(n):
  k=27+j; t,s,d=meta[k+1]; c=decode(bodies[k][:d],p)
  # Split five couples into three independent sections. Keep the same runtime indices.
  groups=[c[:2],c[2:4],c[4:]]
  if overlap: groups[1]=[c[0]]+groups[1]
  for idx,(group,design,rf) in enumerate(zip(groups,(1,2,0),(0,1,0))):
   blob=encode(group,p,design,rf,block_n)
   if idx==0: bodies[k]=blob; meta[k+1]=(5,len(blob),len(blob))
   else:
    maps+=pack('HI',j,len(meta)+len(extra)); extra.append(blob)
 for blob in extra: meta.append((5,len(blob),len(blob))); bodies.append(blob)
 nodes=bytearray(data[8:mapping_start]);
 for j in range(n):
  word,fmt=struct.unpack_from(p+'IH',nodes,6*j); struct.pack_into(p+'I',nodes,6*j,word+(2<<27))
 tail=data[start+sw*40:he]; hs=8+len(nodes)+len(maps)+sw*len(meta)+len(tail); meta[0]=(0,hs,hs)
 return data[:8]+nodes+maps+b''.join(pack(sf,*m) for m in meta)+tail+b''.join(bodies)
def prepare():
 manifest={}
 for be in (0,1):
  for fmt in range(3):
   for value in (0,42,255):
    original=(F/f'{be}-{fmt}-{value}-1-0.ueec').read_bytes()
    for bn in (16,24,128,4096):
     name=f'mixed-{be}-{fmt}-{value}-{bn}.ueec'; blob=mixed(original,bn); (F/name).write_bytes(blob); manifest[name]=hashlib.sha256(blob).hexdigest()
   (F/f'overlap-{be}-{fmt}.ueec').write_bytes(mixed(original,24,True))
 (F/'mixed-manifest.json').write_text(json.dumps(manifest,indent=2))
 print(f'Prepared {len(manifest)} mixed files and 6 overlap fixtures')
def test(arch,exe,runner):
 m=ids[arch]; manifest=json.loads((F/'mixed-manifest.json').read_text()); count=0
 for name,sha in manifest.items():
  value=int(name.split('-')[3]); v=(0,42,255)[((0,42,255).index(value)+m)%3]; first=209+v; second=104+2*(first%256)+v
  for mode in ('auto','monolithic','scattered','hybrid'):
   path=F/name; assert hashlib.sha256(path.read_bytes()).hexdigest()==sha
   r=subprocess.run(shlex.split(runner)+[exe,'--load-mode',mode,str(path)],capture_output=True,text=True,timeout=30)
   expected=f'EXECUTED result={first} second={second} rw={second%256} bss={second%256}'
   if r.returncode or r.stdout.count(expected)!=3 or r.stdout.count('RELOCATED')!=9 or 'REPEAT_OK 3' not in r.stdout or r.stderr or hashlib.sha256(path.read_bytes()).hexdigest()!=sha: raise SystemExit(f'FAIL {arch} {name} {mode}: {r.returncode}\n{r.stdout}\n{r.stderr}')
   count+=1
 for path in sorted(F.glob('overlap-*.ueec')):
  r=subprocess.run(shlex.split(runner)+[exe,'--load-mode','auto',str(path)],capture_output=True,text=True,timeout=30)
  if r.returncode!=1 or 'invalid relocation' not in r.stderr or 'LOADED' in r.stdout or 'Sanitizer' in r.stderr or 'runtime error:' in r.stderr: raise SystemExit(f'FAIL overlap {arch} {path}: {r.returncode}\n{r.stdout}\n{r.stderr}')
 result=dict(architecture=arch,cases=count,rejections=6,load_cycles=count*3,native_calls=count*6,sha256=manifest)
 pathlib.Path(exe).with_name('mixed-report.json').write_text(json.dumps(result,indent=2)); print(f'[PASS] {arch}: {count} mixed cases, 6 cross-section overlap rejections')
def bench():
 p,_,_,_,meta,bodies,_=sections((F/'0-0-42-0-0.ueec').read_bytes()); canonical=decode(bodies[27][:meta[28][2]],p); out=[]
 for name,(d,r) in FORMATS.items():
  blob=encode(canonical,p,d,r); samples=[]
  for _ in range(9):
   t=time.perf_counter_ns()
   for i in range(1000): assert decode(blob,p)==canonical
   samples.append((time.perf_counter_ns()-t)/1000)
  out.append(dict(format=name,bytes=len(blob),median_decode_ns=statistics.median(samples),samples_ns=samples))
 pathlib.Path('build/benchmark.json').write_text(json.dumps(out,indent=2)); print('Python decode benchmark: build/benchmark.json')
if __name__=='__main__':
 if sys.argv[1]=='prepare': prepare()
 elif sys.argv[1]=='benchmark': bench()
 else: test(*sys.argv[1:])
