#!/usr/bin/env python3
import argparse, os, subprocess, shlex, tempfile
from pathlib import Path
from ueec_merge import merge
from ueec_fixed import prepare
p=argparse.ArgumentParser(); p.add_argument('build',type=Path); p.add_argument('--runner',default=''); a=p.parse_args(); b=a.build.resolve(); runner=shlex.split(a.runner)
# QEMU user mode normally exposes the host page size, but verify with loader.
page=int(subprocess.check_output(runner+[str(b/'loader'),'--page-size'],text=True)); base=0x30000000
pos=neg=0
with tempfile.TemporaryDirectory(dir=b) as tmp:
    src=Path(tmp)/'merged.ueec'; dst=Path(tmp)/'fixed.ueec'
    def run(data,mode='monolithic',ok=True,error=None,occupied=False):
        global pos,neg
        dst.write_bytes(data)
        r=subprocess.run(runner+[str(b/('loader_test' if occupied else 'loader')),'--load-mode',mode,str(dst)],capture_output=True,text=True,timeout=45,env={**os.environ,**({'UEEC_TEST_OCCUPIED':'1'} if occupied else {})})
        if ok:
            assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40\n' in r.stdout and 'LOAD_MODE=monolithic MAPPINGS=1\n' in r.stdout and r.stdout.count('BOUND ')==4 and r.stdout.count('FIXED_NODE=')==3,(r.stdout,r.stderr)
            # Decode expected addresses independently from the metadata section in the wire file.
            e='big' if data[3]&128 else 'little'; w=2<<int.from_bytes(data[4:6],e); n=int.from_bytes(data[6:8],e)
            links=sum(int.from_bytes(data[10+4*j:14+4*j],e)>>27 for j in range(n)); start=10+4*n+6*links; offset=0
            for j in range(links+1):
                h=start+j*(2+2*w); t=int.from_bytes(data[h:h+2],e); size=int.from_bytes(data[h+2:h+2+w],e)
                if t==13:
                    aw=2<<int.from_bytes(data[offset+2:offset+4],e)
                    for k in range(n):
                        q=offset+6+k*(2+aw); nd=int.from_bytes(data[q:q+2],e); addr=int.from_bytes(data[q+2:q+2+aw],e)
                        assert f'FIXED_NODE={nd} ADDRESS=0x{addr:x}\n' in r.stdout
                offset+=size
            assert dst.read_bytes()==data
            pos+=1
        else:
            assert r.returncode==1 and 'IMPORT error:' in r.stderr and 'PROGRAM_RESULT=' not in r.stdout and (error is None or error in r.stderr),(r.stdout,r.stderr)
            neg+=1
    for width in (16,32,64):
        for endian in ('le','be'):
            for swap in (False,True):
                d=b/f'f{width}-{endian}'; d=d/'swapped' if swap else d
                src.write_bytes(merge([d/x for x in ('program.ueec','libmath.ueec','libdata.ueec')]))
                for fmt in (1,2):
                    for g in (1,16,512,4096,8192,16384):
                        data=prepare(src,base,page,fmt,g); run(data)
                    data=prepare(src,base,page,fmt,1)
                    run(data,ok=False,error='OCCUPIED_UNCHANGED',occupied=True); run(data,'scattered',False); run(data,'hybrid',False); run(data[:-1],ok=False)
                    # Locate unique metadata record, then invalidate node id, count, address, format.
                    e='little' if endian=='le' else 'big'; aw=2<<fmt
                    u=lambda x,w:x.to_bytes(w,e)
                    marker=u(1,2)+u(fmt,2)+u(3,2)+u(0,2)+u(base+page,aw)
                    i=data.index(marker); assert data.count(marker)==1
                    for off,v,w in ((0,99,2),(2,3,2),(4,2,2),(6,65,2),(8,base+page+1,aw),(8,base+2*page,aw)):
                        bad=bytearray(data); bad[i+off:i+off+w]=u(v,w); run(bad,ok=False)
                try: prepare(src,base,page,0,1)
                except OverflowError: pass
                else: raise AssertionError('16-bit address overflow accepted')
                print(f'[PASS] {width}-{endian} swapped={swap}: fixed layouts and malformed metadata')
print(f'Fixed: positive={pos}; negative={neg}; passed={pos+neg}; failed=0; skipped=0')
