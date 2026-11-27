#!/usr/bin/env python3
import argparse, subprocess, shlex, sys, tempfile
from pathlib import Path
from shutil import copyfile
from ueec_merge import parse, merge
p=argparse.ArgumentParser(); p.add_argument('build',type=Path); p.add_argument('--runner',default=''); a=p.parse_args(); b=a.build.resolve(); runner=shlex.split(a.runner)
util=Path(__file__).with_name('ueec_merge.py').resolve(); pos=neg=0
for width in (16,32,64):
    for endian in ('le','be'):
        d=b/f'f{width}-{endian}'
        for swapped in (False,True):
            src=d/'swapped' if swapped else d
            inputs=[src/x for x in ('program.ueec','libmath.ueec','libdata.ueec')]; before=[p.read_bytes() for p in inputs]
            for subset in ((1,),(2,),(1,2)):
                with tempfile.TemporaryDirectory(dir=b) as tmp:
                    out=Path(tmp)/'combined.ueec'; selected=[inputs[0]]+[inputs[i] for i in subset]
                    out.write_bytes(merge(selected))
                    for i in (1,2):
                        if i not in subset: copyfile(inputs[i],out.parent/inputs[i].name)
                    assert len(list(out.parent.glob('*.ueec')))==4-len(selected)
                    actual=parse(out)[2]; original=[node for f in selected for node in parse(f)[2]]
                    assert len(actual)==len(original)
                    for (w,n,s),(ow,on,os) in zip(actual,original):
                        assert (w,n)==(ow,on) and len(s)==len(os)
                        assert all(t==ot and (t==7 or payload==old) for (t,payload),(ot,old) in zip(s,os))
                    r=subprocess.run(runner+[str(b/'loader'),str(out)],capture_output=True,text=True,timeout=45)
                    assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40' in r.stdout,(r.stdout,r.stderr)
                    for i,lib,syms in ((1,'libmath',('add','sub')),(2,'libdata',('value1','value2'))):
                        name=lib if i in subset else lib+'.ueec'; assert all(f'BOUND {name}:{s}' in r.stdout for s in syms)
                    assert before==[p.read_bytes() for p in inputs]
                    pos+=1; print(f'[PASS] {d.name} swapped={swapped} merged={subset}: result=40; unchanged source/code/export')
            with tempfile.TemporaryDirectory(dir=b) as tmp:
                out=Path(tmp)/'bad.ueec'
                for bad in ([inputs[0],inputs[1],inputs[1]],[inputs[0],d/'truncated.ueec'],[inputs[1],inputs[2]]):
                    try: merge(bad)
                    except ValueError: neg+=1
                    else: raise AssertionError('invalid input accepted')
                    assert not out.exists()
print(f'Merge: positive={pos}; negative={neg}; passed={pos+neg}; failed=0; skipped=0')
