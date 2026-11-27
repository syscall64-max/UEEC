#!/usr/bin/env python3
import argparse, subprocess, shlex, tempfile
from pathlib import Path
from ueec_merge import merge,parse
from ueec_fixed import prepare
from ueec_prelink import prelink
from fixtures import ARCHS,BIG,SMALL

def fixture(path,build,arch):
    e,fmt,nodes=parse(path); ptr=4 if arch in SMALL else 8; native='big' if arch in BIG else 'little'; w=2<<fmt
    def u(x,z=2): return x.to_bytes(z,e)
    word,name,ss=nodes[0]; assert word&8
    data=bytearray(12*ptr); data[4*ptr:5*ptr]=(5*ptr).to_bytes(ptr,native); data[5*ptr:5*ptr+4]=(2).to_bytes(4,native)
    # local section indices include header: target=2, source=2.
    r=u(0)+u(0)+u(2|(2<<5))+u(1)+u((int(ptr==8)<<12)|4*ptr)
    ss=[(1,(build/'reloc_program.bin').read_bytes()),(2,bytes(data)),(7,ss[2][1]),(5,r)]
    nodes[0]=((word&0x07ffffff)|(len(ss)<<27),name,ss)
    sections=[s for _,_,ss in nodes for s in ss]; names=bytes(len(n) for _,n,_ in nodes)+b''.join(u(i) for i in range(len(nodes)))+b''.join(n for _,n,_ in nodes)
    links=b''; i=1
    for j,(_,_,ss) in enumerate(nodes):
        for _ in ss: links+=u(j)+u(i,4); i+=1
    h=b'_GM'+bytes([2+128*(e=='big')])+u(fmt)+u(len(nodes))*2+b''.join(u(v,4) for v,_,_ in nodes)+links
    size=len(h)+(len(sections)+1)*(2+2*w)+len(names)
    return h+u(0)+u(size,w)*2+b''.join(u(t)+u(len(b),w)*2 for t,b in sections)+names+b''.join(b for _,b in sections)

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('build',type=Path); p.add_argument('--runner',default=''); a=p.parse_args(); b=a.build.resolve(); runner=shlex.split(a.runner); arch=b.name; assert arch in ARCHS
    loader=runner+[str(b/'loader')]; page=int(subprocess.check_output(loader+['--page-size'],text=True)); pos=neg=0
    with tempfile.TemporaryDirectory(dir=b) as td:
        src=Path(td)/'src.ueec'; dst=Path(td)/'run.ueec'
        def run(data,prepared=False,ok=True):
            global pos,neg
            dst.write_bytes(data); r=subprocess.run(loader+['--load-mode','monolithic',str(dst)],capture_output=True,text=True,timeout=45)
            if ok:
                assert r.returncode==0 and not r.stderr and 'PROGRAM_RESULT=40\n' in r.stdout and 'LOAD_MODE=monolithic MAPPINGS=1\n' in r.stdout,(r.stdout,r.stderr)
                expected='PRELINKED=1 RELOCATIONS=0 LOOKUPS=0 BINDINGS=0' if prepared else 'PRELINKED=0 RELOCATIONS=1 LOOKUPS=4 BINDINGS=4'
                assert expected in r.stdout and r.stdout.count('BOUND ')==(0 if prepared else 4),(r.stdout,r.stderr)
                assert dst.read_bytes()==data; pos+=1
            else:
                assert r.returncode==1 and 'IMPORT error:' in r.stderr and 'PROGRAM_RESULT=' not in r.stdout,(r.stdout,r.stderr); neg+=1
        for width in (16,32,64):
            for endian in ('le','be'):
                for swap in (False,True):
                    d=b/f'f{width}-{endian}'; d=d/'swapped' if swap else d
                    src.write_bytes(merge([d/x for x in ('program.ueec','libmath.ueec','libdata.ueec')]))
                    src.write_bytes(fixture(src,b,arch))
                    for fmt in (1,2):
                        for g in (1,16,512,4096,8192,16384):
                            fixed=prepare(src,0x30000000,page,fmt,g); prepared=prelink(fixed,page); assert len(prepared)==len(fixed) and prepared!=fixed
                            run(fixed); run(prepared,True)
                        run(prepared[:-1],True,False)
                        for bad in (prepared,fixed[:-1]):
                            try: prelink(bad,page)
                            except ValueError: neg+=1
                            else: raise AssertionError('invalid prelink accepted')
                    print(f'[PASS] {width}-{endian} swapped={swap}: runtime and offline relocation/binding')
    print(f'Prelink: positive={pos}; negative={neg}; passed={pos+neg}; failed=0; skipped=0')
