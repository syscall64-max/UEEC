#!/usr/bin/env python3
"""Separate whole-process perf/strace passes; use generated benchmark fixtures."""
import argparse,ctypes,json,mmap,os,random,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('--input',type=Path,required=True); p.add_argument('--output',type=Path,required=True); p.add_argument('--rounds',type=int,default=5); p.add_argument('--cpu',type=int); a=p.parse_args()
if a.rounds<1: p.error('rounds must be positive')
if a.cpu is not None: os.sched_setaffinity(0,{a.cpu})
I=a.input.resolve(); O=a.output.resolve(); O.mkdir(parents=True,exist_ok=False)
page=os.sysconf('SC_PAGE_SIZE'); libc=ctypes.CDLL(None,use_errno=True); libc.mincore.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]; libc.mincore.restype=ctypes.c_int
modes={n:(n if n in ('scattered','hybrid','direct') else 'monolithic',I/('prelinked.ueec' if n in ('direct','prelinked') else 'fixed.ueec' if n=='fixed' else 'ordinary.ueec')) for n in ('scattered','hybrid','monolithic','fixed','prelinked','direct')}
def condition(path,state):
    with path.open('rb') as f:
        if state=='cold':
            os.fsync(f.fileno()); os.posix_fadvise(f.fileno(),0,0,os.POSIX_FADV_DONTNEED)
        else:
            while f.read(1024*1024): pass
        size=path.stat().st_size
        with mmap.mmap(f.fileno(),size,access=mmap.ACCESS_COPY) as m:
            anchor=ctypes.c_char.from_buffer(m); vec=(ctypes.c_ubyte*((size+page-1)//page))(); rc=libc.mincore(ctypes.addressof(anchor),size,vec); del anchor
            if rc: raise OSError(ctypes.get_errno(),'mincore')
            resident=sum(x&1 for x in vec)
        if resident != (0 if state=='cold' else len(vec)): raise RuntimeError(f'cache mismatch {resident}/{len(vec)}')
        return resident,len(vec)
keys=[(s,n) for s in ('cold','warm') for n in modes]; rng=random.Random(1098); status={}; records=[]
for tool in ('perf','strace'):
    if not shutil.which(tool): status[tool]={'status':'not installed','completed':0}; continue
    completed=0; failure=None
    for i in range(a.rounds):
        order=keys.copy(); rng.shuffle(order)
        for s,n in order:
            mode,path=modes[n]; resident,pages=condition(path,s); dest=O/f'{tool}-{i}-{s}-{n}.txt'
            prefix=['perf','stat','-x',',','-o',str(dest),'-e','cycles,instructions','--'] if tool=='perf' else ['strace','-c','-o',str(dest),'--']
            r=subprocess.run(prefix+[str(I/'loader'),'--load-mode',mode,str(path)],capture_output=True,text=True,timeout=120)
            if r.returncode or 'PROGRAM_RESULT=40\n' not in r.stdout or not dest.exists():
                failure=f'returncode={r.returncode}: {r.stderr.strip()}'; break
            if tool=='perf' and ('<not counted>' in dest.read_text() or '<not supported>' in dest.read_text()): failure='unsupported counter'; break
            records.append({'tool':tool,'round':i,'cache':s,'mode':n,'resident_pages':resident,'file_pages':pages,'file':dest.name}); completed+=1
        if failure: break
    status[tool]={'status':failure or 'completed','completed':completed,'expected':a.rounds*12}
(O/'status.json').write_text(json.dumps({'tools':status,'records':records,'affinity':sorted(os.sched_getaffinity(0)),'scope':'whole process; separate perf and strace executions'},indent=2)); print(json.dumps(status,indent=2))
