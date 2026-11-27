#!/usr/bin/env python3
import argparse, subprocess, shlex, tempfile
from pathlib import Path
from ueec_align import transform
from ueec_merge import merge
p=argparse.ArgumentParser(); p.add_argument('build',type=Path); p.add_argument('--runner',default=''); a=p.parse_args(); b=a.build.resolve(); runner=shlex.split(a.runner)
pos=neg=0
for width in (16,32,64):
    for endian in ('le','be'):
        for swapped in (False,True):
            d=b/f'f{width}-{endian}'; d=d/'swapped' if swapped else d
            paths=[d/x for x in ('program.ueec','libmath.ueec','libdata.ueec')]
            before=[p.read_bytes() for p in paths]; merged=merge(paths)
            for g in (1,16,512,4096,8192,16384):
                data=transform(merged,g)
                assert len(data)%g==0
                assert transform(data,1)==merged
                assert transform(data,g)==data
                # Independently inspect section sizes and padding.
                e='big' if data[3]&128 else 'little'; w=2<<int.from_bytes(data[4:6],e)
                n=int.from_bytes(data[6:8],e); links=sum(int.from_bytes(data[10+i*4:14+i*4],e)>>27 for i in range(n))
                start=10+4*n+6*links; off=0
                for i in range(links+1):
                    p0=start+i*(2+2*w)+2; s=int.from_bytes(data[p0:p0+w],e); ds=int.from_bytes(data[p0+w:p0+2*w],e)
                    assert s==((ds+g-1)//g)*g and not any(data[off+ds:off+s]); off+=s
                assert off==len(data)
                with tempfile.TemporaryDirectory(dir=b) as tmp:
                    f=Path(tmp)/'combined.ueec'; f.write_bytes(data)
                    r=subprocess.run(runner+[str(b/'loader'),str(f)],capture_output=True,text=True,timeout=45)
                    assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40' in r.stdout,(r.stdout,r.stderr)
                assert before==[p.read_bytes() for p in paths]
                pos+=1; print(f'[PASS] {width}-{endian} swapped={swapped} granule={g}: result=40; byte-identical roundtrip')
            for raw,g in ((merged,0),(merged,-1),(merged,16385),(merged[:-1],16),(merged+b'x',16)):
                try: transform(raw,g)
                except ValueError: neg+=1
                else: raise AssertionError('invalid input accepted')
print(f'Granulation: positive={pos}; negative={neg}; passed={pos+neg}; failed=0; skipped=0')
