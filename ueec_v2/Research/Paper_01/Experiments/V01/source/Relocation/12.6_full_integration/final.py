"""Part B: wide couples, cross-CODE execution, data targets, protection and stress."""
import hashlib,json,pathlib,struct,subprocess,shlex,sys,statistics,resource
from convert import sections,decode
from mixed import ids,order
F=pathlib.Path('build/fixtures')
def prepare():
 for be in (0,1):
  for fmt in range(3):
   for cw in (1,2):
    data=(F/f'{be}-{fmt}-42-0-0.ueec').read_bytes(); p,start,sf,sw,meta,bodies,he=sections(data)
    pack=lambda f,*v:struct.pack(p+f,*v)
    maps=[]; nodes=bytearray(data[8:80]); tail=data[start+sw*len(meta):he]
    # Extra code section per node contains the original function at offset 32.
    for j,m in enumerate(order):
     ptr=4 if m in {0,3,5,7,9} else 8; typ=int(ptr==8)
     old=bodies[j]; code=old[:meta[j+1][2]]; bodies[j]=old[:32].ljust(128,b'\0'); meta[j+1]=(1,128,32)
     extra=len(meta); meta.append((1,128,len(code))); bodies.append(code.ljust(128,b'\0'))
     maps.extend((j,k) for k in (j+1,13,16+j,15,extra,28+j))
     word=struct.unpack_from(p+'I',nodes,6*j)[0]; struct.pack_into(p+'I',nodes,6*j,word+(1<<27))
     k=27+j; couples=decode(bodies[k][:meta[k+1][2]],p)
     couples[-1]=(4<<5,couples[-1][1]); couples += [(1|(1<<5),[(8,typ)]),(3|(1<<5),[(8,typ)])]
     # Constant section has an initialized pointer at offset 8; BSS addend is zero.
     blob=pack('HH',0,0)
     for c,entries in couples:
      blob+=pack('H'+'HIQ'[cw],c|(cw<<10),len(entries))+b''.join(pack('H',off|(t<<12)) for off,t in entries)
     assert decode(blob,p)==couples
     bodies[k]=blob; meta[k+1]=(5,len(blob),len(blob))
    meta[13]=(4,16,16); bodies[12]=b'shared'+b'\0'*10
    maps=b''.join(pack('HI',j,k) for j,k in maps)
    hs=8+len(nodes)+len(maps)+sw*len(meta)+len(tail); meta[0]=(0,hs,hs)
    blob=data[:8]+nodes+maps+b''.join(pack(sf,*m) for m in meta)+tail+b''.join(bodies)
    (F/f'final-{be}-{fmt}-{cw}.ueec').write_bytes(blob)
def test(arch,exe,runner):
 resource.setrlimit(resource.RLIMIT_CORE,(0,0)); cmd=shlex.split(runner)+[exe]; n=0
 for path in sorted(F.glob('final-*.ueec')):
  sha=hashlib.sha256(path.read_bytes()).hexdigest()
  m=ids[arch]; v=(0,42,255)[(1+m)%3]; first=209+v; second=104+2*(first%256)+v
  expected=f'EXECUTED result={first} second={second} rw={second%256} bss={second%256}'
  r=subprocess.run(cmd+['--load-mode','monolithic','--probe-protection','--final-check',str(path)],capture_output=True,text=True,timeout=60)
  if r.returncode or r.stdout.count('PROTECTION_OK')!=3 or r.stdout.count('FINAL_DATA_OK')!=3 or r.stdout.count(expected)!=3 or 'REPEAT_OK 3' not in r.stdout or hashlib.sha256(path.read_bytes()).hexdigest()!=sha or 'runtime error:' in r.stderr or 'AddressSanitizer' in r.stderr: raise SystemExit(f'FAIL {arch} {path}: {r.returncode}\n{r.stdout}\n{r.stderr}')
  n+=1
 rejected=0
 for be in (0,1):
  for cw in (1,2):
   original=(F/f'final-{be}-0-{cw}.ueec').read_bytes(); p,_,_,_,meta,_,he=sections(original)
   k=28+order.index(ids[arch]); base=he+sum(x[1] for x in meta[1:k]); countfmt='HIQ'[cw]
   for label,off,change in [('wide-zero',6,struct.pack(p+countfmt,0)),('wide-huge',6,struct.pack(p+countfmt,(1<<(8*(2<<cw)))-1)),('bad-couple-format',4,struct.pack(p+'H',2|(1<<5)|(3<<10)))]:
    path=F/f'wide-bad-{arch}-{be}-{cw}-{label}.ueec'; buf=bytearray(original); buf[base+off:base+off+len(change)]=change; path.write_bytes(buf)
    r=subprocess.run(cmd+['--load-mode','monolithic',str(path)],capture_output=True,text=True,timeout=30)
    if r.returncode!=1 or 'invalid relocation' not in r.stderr or 'LOADED' in r.stdout or 'AddressSanitizer' in r.stderr or 'runtime error:' in r.stderr: raise SystemExit(f'FAIL {arch} {label}: {r.returncode}\n{r.stdout}\n{r.stderr}')
    rejected+=1
 r=subprocess.run(cmd+['--stress'],capture_output=True,text=True,timeout=120)
 if r.returncode or r.stderr: raise SystemExit(f'STRESS FAIL {arch}: {r.returncode}\n{r.stdout}\n{r.stderr}')
 samples={str(k):[] for k in (1000,10000,100000)}
 for line in r.stdout.splitlines():
  if line.startswith('STRESS '): _,k,trial,ns,_=line.split(); samples[k].append(int(ns))
 assert all(len(v)==5 for v in samples.values())
 result=dict(architecture=arch,cases=n,rejections=rejected,stress_samples_ns=samples,median_ns={k:statistics.median(v) for k,v in samples.items()})
 pathlib.Path(exe).with_name('final-report.json').write_text(json.dumps(result,indent=2)); print(f'[PASS] {arch}: {n} final integration cases, {rejected} rejections, 15 stress runs, protection checks')
if __name__=='__main__':
 if sys.argv[1]=='prepare': prepare()
 else: test(*sys.argv[1:])
