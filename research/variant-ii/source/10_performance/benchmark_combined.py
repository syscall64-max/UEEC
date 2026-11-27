#!/usr/bin/env python3
"""Native x86-64 warm-cache paired experiment; no QEMU."""
import argparse,csv,hashlib,os,platform,random,re,statistics,subprocess,sys,time,json
from pathlib import Path
P=Path(__file__).resolve().parent; S=P.parent/'10_direct_mapping'; B=S/'build/x86_64'; sys.path.insert(0,str(S))
from ueec_merge import merge
from ueec_fixed import prepare
from prelink_combined_bench import prelink
from ueec_align import transform
from test_prelink import fixture
p=argparse.ArgumentParser(); p.add_argument('--libraries',type=int,required=True); p.add_argument('--rounds',type=int,default=201); p.add_argument('--cpu',type=int); p.add_argument('--output',type=Path,default=P/'results'); a=p.parse_args()
assert 1<=a.libraries<=64
assert platform.machine()=='x86_64' and platform.system()=='Linux' and a.rounds>=2
if a.cpu is not None: os.sched_setaffinity(0,{a.cpu})
a.output.mkdir(parents=True,exist_ok=False); O=a.output.resolve()
subprocess.run(['make','build-x86_64'],cwd=S,check=True)
source=(S/'loader.c').read_text(); instrumented='#include <time.h>\nstatic void quiet(const char *fmt, ...);\n'+source.replace('64*1024*1024','128*1024*1024')
instrumented=instrumented.replace("if(im->nr!=2 ||", "if(im->nr!=3 || im->s[im->resident[2]].type!=UEEC_SECTION_CONSTANT_DATA ||")
# Suppress diagnostic output inside the measured interval only; preserve error reporting.
# Use a variadic no-op for every diagnostic, leaving final outputs intact.
for marker in ('FIXED_NODE=', 'BOUND ', 'IMPORT format=', 'PRELINKED=', 'DIRECT_FILE_MAP='):
    instrumented=instrumented.replace('if (0) printf("'+marker,'quiet("'+marker).replace('printf("'+marker,'quiet("'+marker)
instrumented=instrumented.replace('int main(int argc,char **argv) {','static void quiet(const char *fmt, ...) { (void)fmt; }\nstatic unsigned long long ns(void) { struct timespec t; if(clock_gettime(CLOCK_MONOTONIC_RAW,&t)) exit(2); return (unsigned long long)t.tv_sec*1000000000ULL+t.tv_nsec; }\nint main(int argc,char **argv) {')
instrumented=instrumented.replace('atexit(cleanup); image *im=','atexit(cleanup); unsigned long long start=ns(); image *im=')
instrumented=instrumented.replace('int result=entry(data->mem);','unsigned long long ready=ns(); int result=entry(data->mem); unsigned long long end=ns(); printf("BENCH=%llu,%llu,%zu,%zu,%zu,%zu,%zu,%zu\\n",ready-start,end-ready,mappings,inspection_maps,section_copy_bytes,relocations,lookups,bindings);')
instrumented=instrumented.replace("memcpy(s->mem,s->raw.p,s->data_size);", "memcpy(s->mem,s->raw.p,s->data_size); section_copy_bytes+=s->data_size;")

instrumented=instrumented.replace("if(m>4096)", "if(m>10000)")
# Experiment-only extension: one SECTION_BLOCK_N/32 couple, ABS64.
old=instrumented[instrumented.index('    if(get(&v,2)!=0 || get(&v,2)!=0) fail("unsupported relocation format/design");'):instrumented.index('    if(v.n) fail("trailing relocation bytes");')]
new=r'''    unsigned fmt=(unsigned)get(&v,2), design=(unsigned)get(&v,2);
    if(fmt!=1 || design!=2) fail("expected BLOCK_N/32");
    unsigned couple=(unsigned)get(&v,2), blocks=(unsigned)get(&v,2);
    unsigned t=couple&31, src=(couple>>5)&31;
    if(couple>>10 || !t || !src || t>=im->count || src>=im->count || blocks!=1) fail("invalid couple");
    section *dst=&im->s[t], *source=&im->s[src];
    if(dst->type!=UEEC_SECTION_INIT_DATA || !dst->mem || !source->mem) fail("invalid sections");
    size_t base=(size_t)get(&v,4), count=(size_t)get(&v,4);
    if(base!=0 || count<1 || count>100000 || count>v.n/4) fail("invalid block");
    for(size_t i=0;i<count;i++) {
        unsigned x=(unsigned)get(&v,4); size_t off=x&0x0fffffffU;
        if(x>>28!=1 || off!=(i?800096+8*(i-1):32) || off>dst->data_size || 8>dst->data_size-off) fail("invalid relocation");
        uintptr_t a; memcpy(&a,(uint8_t*)dst->mem+off,8);
        if(a!=40 || a>=source->data_size || a>UINTPTR_MAX-(uintptr_t)source->mem) fail("invalid addend");
        a+=(uintptr_t)source->mem; memcpy((uint8_t*)dst->mem+off,&a,8); relocations++;
    }
'''
instrumented=instrumented.replace(old,new)
# Check all patched pointers after timed entry, including direct mapping.
instrumented=instrumented.replace('unsigned long long end=ns(); printf',r'''unsigned long long end=ns();
    view check=im->reloc; (void)get(&check,2); (void)get(&check,2); (void)get(&check,2); (void)get(&check,2); (void)get(&check,4); size_t nc=(size_t)get(&check,4);
    for(size_t i=0;i<nc;i++) { uintptr_t ptr; size_t off=i?800096+8*(i-1):32; memcpy(&ptr,(uint8_t*)data->mem+off,8); if(ptr!=(uintptr_t)data->mem+40) fail("post-entry relocation mismatch"); }
    for(size_t j=4;j<10000;j++) { uintptr_t ptr; memcpy(&ptr,(uint8_t*)data->mem+96+8*(j-4),8); if(!ptr) fail("missing import pointer"); int value; memcpy(&value,(void*)ptr,4); if(value!=(int)j) fail("post-entry imported value mismatch"); }
    printf''')

# Keep relative headers resolving exactly as for original source.
(O/'loader_bench.c').write_text(instrumented)
subprocess.run(['gcc','-Wall','-Wextra','-Werror','-O2','-static','-I',str(S),str(O/'loader_bench.c'),'-o',str(O/'loader')],check=True)
page=os.sysconf('SC_PAGE_SIZE'); d=B/'f32-le'; src=O/'source.ueec'; src.write_bytes(merge([d/x for x in ('program.ueec','libmath.ueec','libdata.ueec')])); src.write_bytes(fixture(src,B,'x86_64'))
# Fixed data size for every count. Only relocation metadata length varies.
import struct
from ueec_merge import parse
_,fmt,nodes=parse(src); assert fmt==1
word,name,ss=nodes[0]; assert ss[1][0]==2 and ss[3][0]==5
N=10000; L=a.libraries; data=bytearray(1600096); data[:len(ss[1][1])]=ss[1][1]
r=struct.pack('<HHHHII',1,2,2|(2<<5),1,0,100000)+b''.join(struct.pack('<I',0x10000000+(32 if i==0 else 800096+8*(i-1))) for i in range(100000))
for i in range(99999): struct.pack_into('<Q',data,800096+8*i,40)
ss[1]=(2,bytes(data)); ss[3]=(5,r)
# The first library owns the four original symbols. Distribute the other 124
# contiguous data symbols over all libraries, ensuring every library is used.
q,rem=divmod(N-4,L)
libnames=[f'lib{i:02d}' for i in range(L)]
labels=['add','sub','value1','value2']+[f'x{i:05d}' for i in range(N-4)]
pool=bytearray(); offsets={}
for name in libnames+labels:
    offsets[name]=len(pool); pool+=name.encode()+b'\0'
counts=[q+(i<rem)+(4 if i==0 else 0) for i in range(L)]
imp=struct.pack('<HHI',0,1,L)+b''.join(struct.pack('<I',offsets[n]) for n in libnames)+b''.join(struct.pack('<I',n) for n in counts)
for i,name in enumerate(labels): imp+=struct.pack('<III',1,i*8 if i<4 else 96+8*(i-4),offsets[name])
ss[2]=(7,imp+pool)
mathword,_,mathss=nodes[1]; ss.append((4, bytes(range(256))*(64*1024*1024//256))); nodes=[((word&0x07ffffff)|(len(ss)<<27),nodes[0][1],ss)]; j=4
for i,c in enumerate(counts):
    values=bytearray(); symbols=[]; sections=[]
    if i==0:
        sections=mathss[:2]; symbols=[('add',0,0),('sub',1,0),('value1',2,0),('value2',2,4)]; values+=struct.pack('<ii',20,22)
    for _ in range(c-(4 if i==0 else 0)):
        symbols.append((labels[j],len(sections),len(values))); values+=struct.pack('<i',j); j+=1
    sections.append((2,bytes(values))); names=b''; entries=b''
    for name,sec,off in sorted(symbols):
        entries+=struct.pack('<HII',sec|(len(name)<<5),off,len(names)); names+=name.encode()
    sections.append((6,struct.pack('<HHHI',1,1,1,len(symbols))+entries+names))
    nodes.append(((mathword&0x07ffffff)|(len(sections)<<27),libnames[i].encode(),sections))
assert j==N and sum(counts)==N
# Rebuild ungranulated container, preserving node ordering and all other sections.
secs=[s for _,_,ls in nodes for s in ls]; names=bytes(len(n) for _,n,_ in nodes)+b''.join(struct.pack('<H',i) for i in range(len(nodes)))+b''.join(n for _,n,_ in nodes)
links=b''; idx=1
for j,(_,_,ls) in enumerate(nodes):
    for _ in ls: links+=struct.pack('<HI',j,idx); idx+=1
h=b'_GM\x02'+struct.pack('<HHH',fmt,len(nodes),len(nodes))+b''.join(struct.pack('<I',v) for v,_,_ in nodes)+links
size=len(h)+10*(len(secs)+1)+len(names)
src.write_bytes(h+struct.pack('<HII',0,size,size)+b''.join(struct.pack('<HII',t,len(d),len(d)) for t,d in secs)+names+b''.join(d for _,d in secs))
raw=O/'ordinary.ueec'; raw.write_bytes(transform(src.read_bytes(),page)); fixed=O/'fixed.ueec'; fixed.write_bytes(prepare(src,0x300000000000,page,2,page)); linked=O/'prelinked.ueec'; linked.write_bytes(prelink(fixed.read_bytes(),page))
modes={'scattered':('scattered',raw),'hybrid':('hybrid',raw),'monolithic':('monolithic',raw),'fixed':('monolithic',fixed),'prelinked':('monolithic',linked),'direct':('direct',linked)}
cols=['load_ns','entry_ns','mappings','inspection_maps','section_copy_bytes','relocations','lookups','bindings']
def run(name):
    mode,path=modes[name]; cmd=[str(O/'loader'),'--load-mode',mode,str(path)]; t=time.perf_counter_ns(); r=subprocess.run(cmd,capture_output=True,text=True,timeout=30); total=time.perf_counter_ns()-t
    assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40\n' in r.stdout,(name,r.stdout,r.stderr)
    v=list(map(int,re.search(r'^BENCH=([0-9,]+)$',r.stdout,re.M)[1].split(','))); assert len(v)==len(cols)
    assert v[-3:]==([0,0,0] if name in ('prelinked','direct') else [100000,10000,10000]),(name,v)
    if name=='direct': assert v[2:5]==[1,1,0],v
    return v+[total]
meta={'platform':platform.platform(),'cpuinfo':Path('/proc/cpuinfo').read_text(),'affinity':sorted(os.sched_getaffinity(0)),'page':page,'rounds':a.rounds,'section_bytes':1600096,'extra_constant_bytes':64*1024*1024,'library_data_bytes':39992,'library_count':a.libraries,'import_count':10000,'relocation_count':100000,'benchmark_file_limit':128*1024*1024,'fixed_base':'0x300000000000','workload':'100000 relocations; 10000 unique named imports; 64 MiB unused constant section distributed over internal libraries; all additional imported values checked after timed entry', 'instrumented_sha256':hashlib.sha256(instrumented.encode()).hexdigest(),'compiler':subprocess.check_output(['gcc','--version'],text=True),'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'files':{f.name:{'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in (raw,fixed,linked)}}
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
