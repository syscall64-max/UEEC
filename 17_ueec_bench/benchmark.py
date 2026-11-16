"""Exploratory warm-cache measurements of selected-image loading, not VM execution."""
import argparse,csv,hashlib,io,json,math,os,random,statistics,subprocess,sys,time
from datetime import datetime,timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parent
VM=ROOT.parent/'16_ueec_vm'
sys.path.insert(0,str(VM))
from generate import container,instruction
from environment import inventory,observe

PHASES=('open_map_ns','parse_select_ns','load_ns','input_close_ns','release_ns','total_ns')

def fixture(nodes,size,sharing,granule):
    """One shared CODE; shared or per-node IDATA. Chosen node is last.
    Equal payload bytes and flags in every selected image. SH32, LE metadata.
    """
    data=(bytes(range(256))*((size+255)//256))[:size]
    code=instruction(0,0,0)
    bodies=[(1,code)]
    if sharing=='shared':
        bodies.append((2,data)); records=[(f'p{i}',[1,2]) for i in range(nodes)]
    else:
        bodies.extend((2,data) for _ in range(nodes))
        records=[(f'p{i}',[1,i+2]) for i in range(nodes)]
    return container(records,bodies,fmt=1,endian='little',granule=granule)

def parse_samples(text,expected):
    rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(io.StringIO(text))]
    if len(rows)!=expected:raise ValueError('Wrong sample count')
    for i,row in enumerate(rows):
        if row['sample']!=i or any(row[k]<0 for k in PHASES):raise ValueError('Invalid raw sample')
        if sum(row[k] for k in PHASES[:-1])!=row['total_ns']:raise ValueError('Phase sum mismatch')
    return rows

def stats(values):
    ordered=sorted(values)
    return {'n':len(values),'min_ns':ordered[0],
            'median_ns':statistics.median(ordered),
            'p95_ns':ordered[math.ceil(.95*len(ordered))-1],
            'max_ns':ordered[-1]}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--binary',type=Path,default=ROOT/'build/bench-load')
    ap.add_argument('--output',type=Path,default=ROOT/'build/results')
    ap.add_argument('--samples',type=int,default=200)
    ap.add_argument('--warmups',type=int,default=20)
    ap.add_argument('--rounds',type=int,default=3)
    ap.add_argument('--sizes',type=int,nargs='+',default=[1024,65536,1048576])
    ap.add_argument('--nodes',type=int,nargs='+',default=[1,8])
    ap.add_argument('--granules',type=int,nargs='+',default=[1,4096])
    ap.add_argument('--seed',type=int,default=20261116)
    ap.add_argument('--cpu',type=int)
    ap.add_argument('--environment-note',help='Optional known host/VM/RAM details; recorded as user-supplied, not detected')
    ap.add_argument('--quick',action='store_true')
    a=ap.parse_args()
    if a.quick:
        a.samples=20;a.warmups=5;a.rounds=1;a.sizes=[1024,1048576];a.nodes=[1,8];a.granules=[1]
    if not 1<=a.samples<=100000 or not 0<=a.warmups<=100000 or not 1<=a.rounds<=20:
        ap.error('Invalid repetition counts')
    if any(n<1 or n>64 for n in a.nodes) or any(s<2 or s%2 or s>67108864 for s in a.sizes):
        ap.error('Use 1..64 nodes and even payload sizes 2..67108864')
    if any(g<1 or g>1048576 for g in a.granules):ap.error('Invalid granule')
    if max(a.nodes)*(max(a.sizes)+max(a.granules))>134217728:
        ap.error('A fixture would exceed the 128 MiB conservative size limit')
    for field in ('nodes','sizes','granules'):
        if len(getattr(a,field))!=len(set(getattr(a,field))):ap.error('Duplicate matrix values')
    a.binary=a.binary.resolve();a.output=a.output.resolve()
    binary_bytes=a.binary.read_bytes()
    if b'__asan_init' in binary_bytes or b'__ubsan_handle_' in binary_bytes:
        ap.error('Use the normal optimized binary, not a sanitizer build, for timing')
    affinity=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None
    if a.cpu is not None:
        if affinity is None or a.cpu not in affinity:ap.error('Requested CPU is not in the allowed affinity set')
        os.sched_setaffinity(0,{a.cpu});affinity=[a.cpu]
    info=json.loads(subprocess.check_output([str(a.binary),'--info'],text=True))
    environment=inventory(affinity,a.environment_note)
    a.output.mkdir(parents=True,exist_ok=True);fixtures=a.output/'fixtures';fixtures.mkdir(exist_ok=True)
    cases=[]
    for size in a.sizes:
        for nodes in a.nodes:
            for sharing in ('shared','duplicated'):
                for granule in a.granules:
                    key=f'n{nodes}-d{size}-{sharing}-g{granule}'
                    path=fixtures/(key+'.ueec');blob=fixture(nodes,size,sharing,granule);path.write_bytes(blob)
                    for mode in ('monolithic','scattered','hybrid'):
                        cases.append({'case':key+'-'+mode,'nodes':nodes,'payload_bytes':size,
                            'sharing':sharing,'file_granule':granule,'mode':mode,
                            'file_bytes':len(blob),'fixture_sha256':hashlib.sha256(blob).hexdigest(),
                            'selected_node':nodes-1,'path':path})
    before=observe();started_utc=datetime.now(timezone.utc).isoformat()
    rng=random.Random(a.seed);all_rows=[];round_summaries=[];started=time.monotonic()
    for round_id in range(a.rounds):
        order=cases[:];rng.shuffle(order)
        for order_id,case in enumerate(order):
            command=[str(a.binary),str(case['path']),case['mode'],str(a.samples),str(a.warmups),str(case['selected_node'])]
            p=subprocess.run(command,capture_output=True,text=True,timeout=120,check=True)
            rows=parse_samples(p.stdout,a.samples)
            expected_regions={'monolithic':1,'scattered':3,'hybrid':2}[case['mode']]
            for row in rows:
                if row['selected_payload_bytes']!=case['payload_bytes']+4 or row['mapped_regions']!=expected_regions:
                    raise ValueError('Selected workload or allocation policy changed')
                all_rows.append({**{k:v for k,v in case.items() if k!='path'},
                                 'round':round_id,'order_in_round':order_id,**row})
            round_summaries.append({'case':case['case'],'round':round_id,
                                    'median_load_ns':statistics.median(r['load_ns'] for r in rows)})
    elapsed=time.monotonic()-started;after=observe()
    fields=list(all_rows[0])
    with (a.output/'samples.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(all_rows)
    summary=[]
    grouped={case['case']:[] for case in cases}
    for row in all_rows:grouped[row['case']].append(row)
    for case in cases:
        rows=grouped[case['case']]
        summary.append({**{k:v for k,v in case.items() if k!='path'},
                        'mapped_bytes':rows[0]['mapped_bytes'],'mapped_regions':rows[0]['mapped_regions'],
                        'metrics':{phase:stats([r[phase] for r in rows]) for phase in PHASES},
                        'round_median_load_ns':[r['median_load_ns'] for r in round_summaries if r['case']==case['case']]})
    git=subprocess.run(['git','-C',str(ROOT.parent),'rev-parse','HEAD'],capture_output=True,text=True)
    source_paths=[ROOT/'bench_load.c',ROOT/'benchmark.py',ROOT/'environment.py',ROOT/'Makefile',VM/'ueec_vm.c',VM/'ueec.h',VM/'vm_runtime.c',VM/'vm_runtime.h',VM/'generate.py']
    report={'schema_version':1,'started_utc':started_utc,'scope':'warm-cache selected-image loading only; no VM instructions executed',
            'samples_per_round':a.samples,'warmups_per_case_per_round':a.warmups,'rounds':a.rounds,
            'random_seed':a.seed,'cache_policy':'warm repeats; no cache eviction requested or claimed',
            'node_selection':'last node by explicit numeric index','clock':info,
            'environment':environment,'observations_before':before,'observations_after':after,
            'build':{
                'git_head':git.stdout.strip() if git.returncode==0 else None,
                'binary_sha256':hashlib.sha256(binary_bytes).hexdigest(),
                'build_command':(a.binary.parent/'build-command.txt').read_text().strip() if (a.binary.parent/'build-command.txt').exists() else None},
            'source_sha256':{str(p.relative_to(ROOT.parent)):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths},
            'total_measured_samples':len(all_rows),'wall_seconds_for_harness':elapsed,
            'interpretation':'Exploratory measurements on this host, not a general performance claim or comparison with ELF/PE/Mach-O.',
            'summary':summary}
    (a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    (a.output/'environment.json').write_text(json.dumps({'started_utc':started_utc,'environment':environment,'build':report['build'],
        'clock':info,'observations_before':before,'observations_after':report['observations_after']},indent=2)+'\n')
    columns=['case','file_bytes','mapped_bytes','mapped_regions']+[phase+'_median_us' for phase in PHASES]
    with (a.output/'summary.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=columns);w.writeheader()
        for row in summary:w.writerow({**{k:row[k] for k in columns[:4]},**{p+'_median_us':row['metrics'][p]['median_ns']/1000 for p in PHASES}})
    print(f'{len(cases)} configurations; {len(all_rows)} measured samples; warm cache')
    print('case | file bytes | mapped bytes | load median us | load p95 us')
    for row in summary:
        m=row['metrics']['load_ns']
        print(f"{row['case']} | {row['file_bytes']} | {row['mapped_bytes']} | {m['median_ns']/1000:.3f} | {m['p95_ns']/1000:.3f}")
    print('Saved samples.csv, summary.csv, summary.json and environment.json in '+str(a.output))

if __name__=='__main__':main()
