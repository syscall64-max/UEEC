#!/usr/bin/env python3
import argparse, subprocess, shlex, tempfile
from pathlib import Path
from ueec_align import transform
from ueec_merge import merge
p=argparse.ArgumentParser(); p.add_argument('build',type=Path); p.add_argument('--runner',default=''); a=p.parse_args(); b=a.build.resolve(); runner=shlex.split(a.runner)
pos=neg=0
modes=('scattered','monolithic','hybrid')
def run(f,mode,containers,valid=True):
    global pos,neg
    r=subprocess.run(runner+[str(b/'loader'),'--load-mode',mode,str(f)],capture_output=True,text=True,timeout=45)
    if valid:
        maps=5 if mode=='scattered' else containers*(1 if mode=='monolithic' else 2)
        assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40' in r.stdout and f'LOAD_MODE={mode} MAPPINGS={maps}\n' in r.stdout,(f,mode,r.stdout,r.stderr)
        assert r.stdout.count('BOUND ')==4
        pos+=1
    else:
        assert r.returncode==1 and 'IMPORT error:' in r.stderr and 'PROGRAM_RESULT=' not in r.stdout,(f,mode,r.stdout,r.stderr)
        neg+=1
for width in (16,32,64):
    for endian in ('le','be'):
        for swapped in (False,True):
            d=b/f'f{width}-{endian}'; src=d/'swapped' if swapped else d
            merged=merge([src/x for x in ('program.ueec','libmath.ueec','libdata.ueec')])
            with tempfile.TemporaryDirectory(dir=b) as tmp:
                f=Path(tmp)/'combined.ueec'
                for g in (1,16,512,4096,8192,16384):
                    data=transform(merged,g); f.write_bytes(data)
                    for mode in modes: run(f,mode,1)
                    assert f.read_bytes()==data
                    print(f'[PASS] {width}-{endian} swapped={swapped} granule={g}: all three modes; result=40; mapping counts verified')
                f.write_bytes(merged[:-1])
                for mode in modes: run(f,mode,1,False)
            for mode in modes:
                run(src/'program.ueec',mode,3)
                for lib in ('libmath','libdata'):
                    run(d/('mixed-'+lib+('-swapped' if swapped else ''))/'program.ueec',mode,2)
                    run(d/('mixed-missing-'+lib)/'program.ueec',mode,2,False)
print(f'Unified: positive={pos}; negative={neg}; passed={pos+neg}; failed=0; skipped=0')
