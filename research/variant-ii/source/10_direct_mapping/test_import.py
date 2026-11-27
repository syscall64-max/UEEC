#!/usr/bin/env python3
from pathlib import Path
import argparse, shlex, subprocess
p=argparse.ArgumentParser(); p.add_argument('build',type=Path); p.add_argument('--runner',default=''); a=p.parse_args()
b=a.build.resolve(); runner=shlex.split(a.runner); pos=neg=0
errors={'bad-section':'invalid import target section','bad-offset':'invalid import target offset','bad-name-offset':'invalid name offset','missing-symbol':'missing symbol','missing-library':'missing library or program','truncated':'section bounds'}
def run(path):
    return subprocess.run(runner+[str(b/'loader'),str(path)],capture_output=True,text=True,timeout=45)
for width in (16,32,64):
    for endian in ('le','be'):
        d=b/f'f{width}-{endian}'
        r=run(d/'program.ueec')
        expected=[f'IMPORT format={width} libraries=2 objects=4','PROGRAM_RESULT=40','BOUND libmath.ueec:add','BOUND libmath.ueec:sub','BOUND libdata.ueec:value1','BOUND libdata.ueec:value2']
        assert r.returncode==0 and not r.stderr and all(x in r.stdout for x in expected), (d,r.returncode,r.stdout,r.stderr)
        pos+=1; print(f'[PASS] {d.name}: functions + data, result=40')
        assert (d/'program.ueec').read_bytes()==(d/'swapped/program.ueec').read_bytes()
        r=run(d/'swapped/program.ueec')
        assert r.returncode==0 and not r.stderr and all(x in r.stdout for x in expected), (d,'swapped',r.returncode,r.stdout,r.stderr)
        pos+=1; print(f'[PASS] {d.name}: swapped EXPORT designs and mixed widths, unchanged program, result=40')
        for case,message in errors.items():
            r=run(d/(case+'.ueec'))
            assert r.returncode==1 and 'IMPORT error: '+message in r.stderr and 'PROGRAM_RESULT=' not in r.stdout and 'Sanitizer' not in r.stderr and 'runtime error:' not in r.stderr,(d,case,r.returncode,r.stdout,r.stderr)
            neg+=1
        print(f'[PASS] {d.name}: {len(errors)} invalid inputs rejected')
        for variant in ('combined','swapped'):
            r=run(d/'internal'/(variant+'.ueec'))
            assert r.returncode==0 and not r.stderr and all(x in r.stdout for x in ['PROGRAM_RESULT=40',f'IMPORT format={width} libraries=2 objects=4','BOUND libmath:add','BOUND libmath:sub','BOUND libdata:value1','BOUND libdata:value2']), (d,variant,r.returncode,r.stdout,r.stderr)
            pos+=1; print(f'[PASS] {d.name}: internal {variant}, result=40')
        r=run(d/'internal/missing-node.ueec')
        assert r.returncode==1 and 'IMPORT error: missing compatible node' in r.stderr and 'PROGRAM_RESULT=' not in r.stdout,(d,r.returncode,r.stdout,r.stderr)
        neg+=1; print(f'[PASS] {d.name}: missing internal node rejected')
        for external_lib in ('libmath','libdata'):
            for swap in (False,True):
                variant='mixed-'+external_lib+('-swapped' if swap else '')
                folder=d/variant
                assert {x.name for x in folder.glob('*.ueec')}=={'program.ueec',external_lib+'.ueec'}
                r=run(folder/'program.ueec')
                math='libmath.ueec' if external_lib=='libmath' else 'libmath'
                data='libdata.ueec' if external_lib=='libdata' else 'libdata'
                expected=['PROGRAM_RESULT=40',f'IMPORT format={width} libraries=2 objects=4',f'BOUND {math}:add',f'BOUND {math}:sub',f'BOUND {data}:value1',f'BOUND {data}:value2']
                assert r.returncode==0 and not r.stderr and all(x in r.stdout for x in expected),(variant,r.returncode,r.stdout,r.stderr)
                pos+=1; print(f'[PASS] {d.name}: {variant}, result=40')
            folder=d/('mixed-missing-'+external_lib)
            assert {x.name for x in folder.glob('*.ueec')}=={'program.ueec'}
            r=run(folder/'program.ueec')
            assert r.returncode==1 and 'IMPORT error: missing library or program' in r.stderr and 'PROGRAM_RESULT=' not in r.stdout and 'Sanitizer' not in r.stderr and 'runtime error:' not in r.stderr,(folder,r.returncode,r.stdout,r.stderr)
            neg+=1; print(f'[PASS] {d.name}: missing mixed external {external_lib} rejected')
# Integrate the actual stage 09 converter (its scope is LE SH16).
from shutil import copyfile
src=b/'f16-le'; dst=src/'converted'; dst.mkdir(exist_ok=True)
copyfile(src/'program.ueec',dst/'program.ueec')
for lib, design in [('libmath.ueec','flat'),('libdata.ueec','compact')]:
    r=subprocess.run([str(b/'convert_export'),str(src/lib),str(dst/lib),'--design',design],capture_output=True,text=True,timeout=30)
    assert r.returncode==0,(lib,r.returncode,r.stdout,r.stderr)
    print(f'[PASS] existing stage 09 converter: {lib} -> {design}')
assert (src/'program.ueec').read_bytes()==(dst/'program.ueec').read_bytes()
r=run(dst/'program.ueec')
assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40' in r.stdout and 'IMPORT format=16 libraries=2 objects=4' in r.stdout,(r.returncode,r.stdout,r.stderr)
pos+=1; print('[PASS] converted libraries, unchanged program, result=40')
print(f'Positive: {pos}; negative: {neg}; host conversions: 2; passed: {pos+neg+2}; failed: 0; skipped: 0')
