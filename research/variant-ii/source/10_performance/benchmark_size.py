#!/usr/bin/env python3
"""Native x86-64 warm-cache paired experiment; no QEMU."""
import argparse,csv,hashlib,os,platform,random,re,statistics,subprocess,sys,time,json
from pathlib import Path
P=Path(__file__).resolve().parent; S=P.parent/'10_direct_mapping'; B=S/'build/x86_64'; sys.path.insert(0,str(S))
from ueec_merge import merge
from ueec_fixed import prepare
from ueec_prelink import prelink
from ueec_align import transform
from test_prelink import fixture
p=argparse.ArgumentParser(); p.add_argument('--section-bytes',type=int,required=True); p.add_argument('--rounds',type=int,default=201); p.add_argument('--cpu',type=int); p.add_argument('--output',type=Path,default=P/'results'); a=p.parse_args()
assert 4096<=a.section_bytes<=64*1024*1024
assert platform.machine()=='x86_64' and platform.system()=='Linux' and a.rounds>=2
if a.cpu is not None: os.sched_setaffinity(0,{a.cpu})
a.output.mkdir(parents=True,exist_ok=False); O=a.output.resolve()
subprocess.run(['make','build-x86_64'],cwd=S,check=True)
source=(S/'loader.c').read_text(); instrumented='#include <time.h>\nstatic void quiet(const char *fmt, ...);\n'+source.replace('64*1024*1024','128*1024*1024')
# Suppress diagnostic output inside the measured interval only; preserve error reporting.
# Use a variadic no-op for every diagnostic, leaving final outputs intact.
for marker in ('FIXED_NODE=', 'BOUND ', 'IMPORT format=', 'PRELINKED=', 'DIRECT_FILE_MAP='):
    instrumented=instrumented.replace('if (0) printf("'+marker,'quiet("'+marker).replace('printf("'+marker,'quiet("'+marker)
instrumented=instrumented.replace('int main(int argc,char **argv) {','static void quiet(const char *fmt, ...) { (void)fmt; }\nstatic unsigned long long ns(void) { struct timespec t; if(clock_gettime(CLOCK_MONOTONIC_RAW,&t)) exit(2); return (unsigned long long)t.tv_sec*1000000000ULL+t.tv_nsec; }\nint main(int argc,char **argv) {')
instrumented=instrumented.replace('atexit(cleanup); image *im=','atexit(cleanup); unsigned long long start=ns(); image *im=')
instrumented=instrumented.replace('int result=entry(data->mem);','unsigned long long ready=ns(); int result=entry(data->mem); unsigned long long end=ns(); printf("BENCH=%llu,%llu,%zu,%zu,%zu,%zu,%zu,%zu\\n",ready-start,end-ready,mappings,inspection_maps,section_copy_bytes,relocations,lookups,bindings);')
instrumented=instrumented.replace("memcpy(s->mem,s->raw.p,s->data_size);", "memcpy(s->mem,s->raw.p,s->data_size); section_copy_bytes+=s->data_size;")
# Keep relative headers resolving exactly as for original source.
(O/'loader_bench.c').write_text(instrumented)
subprocess.run(['gcc','-Wall','-Wextra','-Werror','-O2','-static','-I',str(S),str(O/'loader_bench.c'),'-o',str(O/'loader')],check=True)
page=os.sysconf('SC_PAGE_SIZE'); d=B/'f32-le'; src=O/'source.ueec'; src.write_bytes(merge([d/x for x in ('program.ueec','libmath.ueec','libdata.ueec')])); src.write_bytes(fixture(src,B,'x86_64'))
# Extend existing program INIT_DATA, preserving all referenced offsets.
b=src.read_bytes(); n=int.from_bytes(b[6:8],'little'); k=int.from_bytes(b[8:10],'little'); assert b[4:6]==b'\x01\x00'
start=10+4*n+6*sum(int.from_bytes(b[10+4*i:14+4*i],'little')>>27 for i in range(n))
hdr=[(int.from_bytes(b[j:j+2],'little'),int.from_bytes(b[j+2:j+6],'little')) for j in range(start,start+10* (1+sum(int.from_bytes(b[10+4*i:14+4*i],'little')>>27 for i in range(n))),10)]
assert hdr[2][0]==2 and hdr[2][1]<=a.section_bytes
pos=sum(z for _,z in hdr[:3]); extra=a.section_bytes-hdr[2][1]; h=bytearray(b[:pos]); h[start+22:start+30]=a.section_bytes.to_bytes(4,'little')*2
# Nonzero initialized bytes, not sparse zero padding. Existing payload never reads extension.
pattern=bytes(range(256)); src.write_bytes(h+(pattern*((extra+255)//256))[:extra]+b[pos:])
raw=O/'ordinary.ueec'; raw.write_bytes(transform(src.read_bytes(),page)); fixed=O/'fixed.ueec'; fixed.write_bytes(prepare(src,0x300000000000,page,2,page)); linked=O/'prelinked.ueec'; linked.write_bytes(prelink(fixed.read_bytes(),page))
modes={'scattered':('scattered',raw),'hybrid':('hybrid',raw),'monolithic':('monolithic',raw),'fixed':('monolithic',fixed),'prelinked':('monolithic',linked),'direct':('direct',linked)}
cols=['load_ns','entry_ns','mappings','inspection_maps','section_copy_bytes','relocations','lookups','bindings']
def run(name):
    mode,path=modes[name]; cmd=[str(O/'loader'),'--load-mode',mode,str(path)]; t=time.perf_counter_ns(); r=subprocess.run(cmd,capture_output=True,text=True,timeout=30); total=time.perf_counter_ns()-t
    assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40\n' in r.stdout,(name,r.stdout,r.stderr)
    v=list(map(int,re.search(r'^BENCH=([0-9,]+)$',r.stdout,re.M)[1].split(','))); assert len(v)==len(cols)
    assert v[-3:]==([0,0,0] if name in ('prelinked','direct') else [1,4,4]),(name,v)
    if name=='direct': assert v[2:5]==[1,1,0],v
    return v+[total]
meta={'platform':platform.platform(),'cpuinfo':Path('/proc/cpuinfo').read_text(),'affinity':sorted(os.sched_getaffinity(0)),'page':page,'rounds':a.rounds,'section_bytes':a.section_bytes,'benchmark_file_limit':128*1024*1024,'fixed_base':'0x300000000000','workload':'initialized data extension not accessed by entry', 'instrumented_sha256':hashlib.sha256(instrumented.encode()).hexdigest(),'compiler':subprocess.check_output(['gcc','--version'],text=True),'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'files':{f.name:{'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in (raw,fixed,linked)}}
(O/'environment.json').write_text(json.dumps(meta,indent=2)); rng=random.Random(109); rows={n:[] for n in modes}
with (O/'raw.csv').open('w',newline='') as f:
    w=csv.writer(f); w.writerow(['round','mode',*cols,'subprocess_ns'])
    for i in range(-10,a.rounds):
        order=list(modes); rng.shuffle(order)
        for n in order:
            v=run(n)
            if i>=0: w.writerow([i,n,*v]); rows[n].append(v)
        f.flush()
with (O/'summary.csv').open('w',newline='') as f:
    w=csv.writer(f); w.writerow(['mode','n','median_load_ns','q1_load_ns','q3_load_ns','median_entry_ns','median_load_plus_entry_ns','median_subprocess_ns','median_paired_speedup_vs_monolithic',*cols[2:]])
    for n,vs in rows.items():
        z=list(zip(*vs)); q=statistics.quantiles(z[0],n=4,method='inclusive'); ratio=statistics.median(x[0]/y[0] for x,y in zip(rows['monolithic'],vs)); assert all(len(set(c))==1 for c in z[2:8]); row=[n,len(vs),statistics.median(z[0]),q[0],q[2],statistics.median(z[1]),statistics.median(x[0]+x[1] for x in vs),statistics.median(z[8]),ratio,*[c[0] for c in z[2:8]]]; w.writerow(row); print(*row)
for path in (raw,fixed,linked):
    assert hashlib.sha256(path.read_bytes()).hexdigest()==meta['files'][path.name]['sha256'],path
print('PASS',a.rounds*len(modes),'measured executions; results:',O)
