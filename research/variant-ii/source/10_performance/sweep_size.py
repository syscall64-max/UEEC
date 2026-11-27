#!/usr/bin/env python3
"""Stage 10.9.2: native warm-cache initialized-section size sweep."""
import argparse,csv,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__); p.add_argument('--rounds',type=int,default=101); p.add_argument('--cpu',type=int); p.add_argument('--output',type=Path,required=True); p.add_argument('--sizes',type=int,nargs='+',default=[4096,16384,65536,262144,1048576,4194304,16777216,67108864]); a=p.parse_args()
if a.rounds<2 or len(set(a.sizes))!=len(a.sizes) or any(not 4096<=s<=67108864 for s in a.sizes): p.error('invalid rounds/sizes')
a.output.mkdir(parents=True,exist_ok=False); out=a.output.resolve(); rows=[]
for size in a.sizes:
    folder=out/str(size); cmd=[sys.executable,str(Path(__file__).with_name('benchmark_size.py')),'--section-bytes',str(size),'--rounds',str(a.rounds),'--output',str(folder)]
    if a.cpu is not None: cmd+=['--cpu',str(a.cpu)]
    subprocess.run(cmd,check=True)
    with (folder/'summary.csv').open() as f: rows.extend({'section_bytes':size,**r} for r in csv.DictReader(f))
    # Retain metadata, source of instrumented loader, raw timings, summaries; remove regenerated large fixtures.
    for name in ('source.ueec','ordinary.ueec','fixed.ueec','prelinked.ueec','loader'): (folder/name).unlink()
    with (out/'summary.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
print(f'PASS {len(a.sizes)*6*a.rounds} measured executions; {out}')
