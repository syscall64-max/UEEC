import csv, os, sys, subprocess, statistics as st, math, time, platform, random, hashlib
from pathlib import Path
sys.path.insert(0,str(Path('symbols').resolve()))
from export_ref import enc,dec
from import_ref import encode,decode,bind
import struct
reps=int(sys.argv[1]) if len(sys.argv)>1 else 100
if not 10<=reps<=10000:raise SystemExit('repetitions: 10..10000')
launch_reps=max(10,reps//5); symbol_reps=max(10,reps//5)
# Capture CPU/kernel/compiler and affinity. No privilege changes, no cache dropping.
try: cpu=next(l.split(':',1)[1].strip() for l in Path('/proc/cpuinfo').read_text().splitlines() if l.startswith('model name'))
except StopIteration:cpu='unknown'
Path('environment.txt').write_text(f'{platform.platform()}\nCPU={cpu}\npage_size={os.sysconf("SC_PAGE_SIZE")}\naffinity={sorted(os.sched_getaffinity(0))}\npython={platform.python_version()}\ncompiler={subprocess.check_output([os.environ.get("CC","cc"),"--version"],text=True).splitlines()[0]}\nreps={reps}\nlaunch_reps={launch_reps}\nsymbol_reps={symbol_reps}\n')
raw=[]; summary=[]; memory=[]; validated=0
modes=['monolithic','scattered','hybrid']; rnd=random.Random(20261128)
clock=lambda:time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
fields=['parse','select','build','ready','allocate','runtime_header','copy','relocate','verify_header','protect','release']
def add(group,case,variant,mode,field,vals):
 assert vals and all(v>=0 for v in vals)
 raw.extend((group,case,variant,mode,field,i,v) for i,v in enumerate(vals))
 summary.append((group,case,variant,mode,field,len(vals),min(vals),st.median(vals),round(st.mean(vals),2),sorted(vals)[math.ceil(.95*len(vals))-1],max(vals)))
def run_image(group,path,case,variant,mode,expected):
 global validated
 q=subprocess.run([f'{group}/bench',str(path),mode,'-',str(reps),str(expected)],capture_output=True,text=True,check=True,timeout=120)
 lines=q.stdout.splitlines(); assert len(lines)==reps,(path,lines[:3]); data=[list(map(int,x.split(','))) for x in lines]; assert all(len(row)==14 and row[0]==i for i,row in enumerate(data));
 for j,f in enumerate(fields,1):add(group,case,variant,mode,f,[r[j] for r in data])
 line=next(l for l in q.stderr.splitlines() if l.startswith('VALIDATED,')); a,b,c,r0,r1=map(int,line.split(',')[1:]);assert all(row[-2:]==[a,c] for row in data);memory.append((group,case,variant,mode,a,b,c,r0,r1,r1-r0 if min(r0,r1)>=0 else ''));validated+=1
jobs=[]
for p in Path('loading/fixtures').glob('*.ueec'):
 n=int(p.name.split('_')[0]); width=p.stem.split('_')[-1]; expected=42 if n==2 else (n-3)*(n-2)//2%256
 for mode in modes:jobs.append(('loading',p,str(n),width,mode,expected))
with open('relocation/expected.csv') as f:
 for r in csv.DictReader(f):
  p=Path('relocation/fixtures')/r['file'];case,_,design,_=p.stem.split('_');
  for mode in modes:jobs.append(('relocation',p,case,design,mode,int(r['expected'])))
rnd.shuffle(jobs)
for job in jobs:run_image(*job)
print(f'Image configurations: {validated} PASS; timed cycles: {validated*reps}',flush=True)
# Node selection: first/last name vs last numerical index. Parse separately.
for n in (2,4,8,16,32,64):
 p=f'nodes/fixtures/{n}_16_sh16_shared.ueec'
 for label,selector,target in [('first_name','n00',0),('last_name',f'n{n-1:02d}',n-1),('last_index',str(n-1),n-1)]:
  q=subprocess.check_output(['nodes/bench',p,selector,str(target),str(reps)],text=True,timeout=30); rows=[list(map(int,x.split(','))) for x in q.splitlines()];assert len(rows)==reps
  for j,f in enumerate(['parse','select'],1):add('nodes',str(n),label,'none',f,[r[j] for r in rows])
print('Node selection configurations: 18 PASS',flush=True)
# Separate end-to-end subprocess wall time, including process startup, exec, loader diagnostics and wait. Same stdout/stderr sink.
launch=[]
for p in Path('elf/fixtures').glob('*.elf'):
 n=int(p.name.split('_')[0]);var=p.stem.split('_base_')[1];expected=42 if n==2 else (n-3)*(n-2)//2%256;p.chmod(0o755);launch.append((str(n),var,'kernel',[str(p.resolve())],expected))
for p in Path('loading/fixtures').glob('*.ueec'):
 n=int(p.name.split('_')[0]);var=p.stem.split('_')[-1];expected=42 if n==2 else (n-3)*(n-2)//2%256
 for mode in modes:launch.append((str(n),var,mode,['loading/launch','--load-mode',mode,str(p)],expected))
rnd.shuffle(launch)
for case,var,mode,cmd,expected in launch:
 vals=[]
 for i in range(-3,launch_reps):
  t=clock();q=subprocess.run(cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10);dt=clock()-t;assert q.returncode==expected,(cmd,q.returncode,expected)
  if i>=0:vals.append(dt)
 add('launch',case,var,mode,'process_wall',vals)
print(f'Process-launch configurations: {len(launch)} PASS; samples: {len(launch)*launch_reps}',flush=True)
# Exact E08/E09 Python reference functions: separate table decode, dict construction, lookups, binding. Synthetic nonexecuted addresses.
for n in (1,16,64,256,1024,4096):
 for design in (0,1,2):
  keys=[i+1 if design==2 else f's{i:05d}'.encode() for i in range(n)]; entries=[(k,0,i*6) for i,k in enumerate(keys)]; w=2;e='<';blob=enc(entries,w,design,e);kind=int(design==2);groups=[(b'libalpha',[(kind,0,i*8,k) for i,k in enumerate(keys)])];ib=encode(groups,w,e);vals={k:[] for k in ['export_decode','index_build','lookup_batch','import_decode','bind_batch']}
  for i in range(-3,symbol_reps):
   t=clock();a=dec(blob,e);b=clock();index={k:(sec,o) for k,sec,o in a};c=clock();found=[index[k] for k in keys];d=clock();g=decode(ib,e);f=clock();reg={b'libalpha':{(kind,k):0x100000+o for k,sec,o in a}};slots=bytearray(n*8);h=clock();bound=bind(g,reg,slots);j=clock()
   assert len(bound)==n and found==[(0,i*6) for i in range(n)] and all(struct.unpack_from('<Q',slots,i*8)[0]==0x100000+i*6 for i in range(n))
   if i>=0:
    for name,v in zip(vals,[b-t,c-b,d-c,f-d,j-h]):vals[name].append(v)
  for field,v in vals.items():add('symbols',str(n),str(design),'python',field,v)
print('Python export/import reference configurations: 18 PASS',flush=True)
for name,head,data in [('samples.csv',['group','case','variant','mode','phase','sample','ns'],raw),('summary.csv',['group','case','variant','mode','phase','samples','min_ns','median_ns','mean_ns','p95_ns','max_ns'],summary),('memory.csv',['group','case','variant','mode','allocated_bytes','resident_mincore_bytes','allocation_regions','rss_before_kib','rss_loaded_kib','rss_delta_kib'],memory)]:
 with open(name,'w',newline='') as f:w=csv.writer(f);w.writerow(head);w.writerows(data)
print(f'All configurations: {validated+18+len(launch)+18} PASS; timing observations: {len(raw)}')
for n in ('2','32'):
 for r in summary:
  if r[0]=='loading' and r[1]==n and r[2]=='sh16' and r[4]=='ready':print(f'sections={n} {r[3]} ready median={r[7]} ns p95={r[9]} ns')
