#!/usr/bin/env python3
"""Direct file mapping versus copied prelinked image; same fixture and result."""
import argparse, os, shlex, subprocess, tempfile
from pathlib import Path
from fixtures import ARCHS
from ueec_merge import merge
from ueec_fixed import prepare
from ueec_prelink import prelink
from test_prelink import fixture

p=argparse.ArgumentParser(); p.add_argument('build',type=Path); p.add_argument('--runner',default=''); a=p.parse_args()
b=a.build.resolve(); arch=b.name; assert arch in ARCHS; runner=shlex.split(a.runner)
page=int(subprocess.check_output(runner+[str(b/'loader'),'--page-size'],text=True)); pos=neg=0
with tempfile.TemporaryDirectory(dir=b) as td:
    src=Path(td)/'source.ueec'; dst=Path(td)/'run.ueec'
    def run(data,mode='direct',error=None,occupied=False):
        global pos,neg
        dst.write_bytes(data)
        env=dict(os.environ); env.pop('UEEC_TEST_OCCUPIED',None)
        if occupied: env['UEEC_TEST_OCCUPIED']='1'
        r=subprocess.run(runner+[str(b/('loader_test' if occupied else 'loader')),'--load-mode',mode,str(dst)],capture_output=True,text=True,timeout=45,env=env)
        assert dst.read_bytes()==data, 'source modified'
        if error:
            assert r.returncode==1 and error in r.stderr and 'PROGRAM_RESULT=' not in r.stdout,(r.stdout,r.stderr); neg+=1; return
        assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40\n' in r.stdout and f'LOAD_MODE={mode} MAPPINGS=1\n' in r.stdout,(r.stdout,r.stderr)
        assert 'PRELINKED=1 RELOCATIONS=0 LOOKUPS=0 BINDINGS=0' in r.stdout and 'BOUND ' not in r.stdout and r.stdout.count('FIXED_NODE=')==3,r.stdout
        if mode=='direct': assert 'DIRECT_FILE_MAP=1 INSPECTION_MAPS=1 SECTION_COPY_BYTES=0' in r.stdout,r.stdout
        pos+=1
        return [l for l in r.stdout.splitlines() if l.startswith('FIXED_NODE=')]
    for width in (16,32,64):
        for endian in ('le','be'):
            for swap in (False,True):
                d=b/f'f{width}-{endian}'; d=d/'swapped' if swap else d
                src.write_bytes(merge([d/x for x in ('program.ueec','libmath.ueec','libdata.ueec')]))
                src.write_bytes(fixture(src,b,arch))
                for fmt in (1,2):
                    for g in (page,2*page):
                        fixed=prepare(src,0x30000000,page,fmt,g); ready=prelink(fixed,page)
                        assert run(ready)==run(ready,'monolithic')
                    run(fixed,error='direct requires prelinked fixed container')
                    run(ready[:-1],error='section bounds')
                    run(ready,error='OCCUPIED_UNCHANGED',occupied=True)
                    run(prelink(prepare(src,0x30000000,page,fmt,1),page),error='direct requires page-aligned sections')
                    bad=bytearray(ready); e='little' if endian=='le' else 'big'; w=2<<int.from_bytes(bad[4:6],e)
                    nodes=int.from_bytes(bad[6:8],e); links=sum(int.from_bytes(bad[10+4*j:14+4*j],e)>>27 for j in range(nodes)); h=10+4*nodes+6*links
                    raw=int.from_bytes(bad[h+2+w:h+2+2*w],e); bad[raw]=1
                    run(bad,error='nonzero direct padding')
                    run(src.read_bytes(),error='direct requires prelinked fixed container')
                print(f'[PASS] {width}-{endian} swapped={swap}: file-backed vs copied prelink, rejection checks')
print(f'Direct: positive={pos}; negative={neg}; passed={pos+neg}; failed=0; skipped=0')
