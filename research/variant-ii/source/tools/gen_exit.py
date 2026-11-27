#!/usr/bin/env python3
"""Build deterministic Variant II exit fixtures from common immutable raw code."""
import argparse, struct
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
ARCHS = ('arm32','aarch64','loongarch64','ppc32','ppc64','riscv32','riscv64','sparc32','sparc64','x86','x86_64','s390x')
BIG = {'ppc32','ppc64','sparc32','sparc64','s390x'}
def generate(width, endian, shared=False):
    e = '<' if endian == 'le' else '>'
    fmt = {16:0,32:1,64:2}[width]
    n = len(ARCHS)
    p = lambda f,*x: struct.pack(e+f,*x)
    codes = [(ROOT/'test_resources/bin'/f'{a}.bin').read_bytes() for a in ARCHS]
    head = b'_GM'+bytes([2 | (128 if endian=='be' else 0)])+p('HHH',fmt,n,1)
    for machine,a in enumerate(ARCHS):
        word = ((2 if shared else 1)<<27)|(2<<20)|(2<<18)|(machine<<10)|0x2c|(a in BIG)
        head += p('I',word)
    for i in range(n): head += p('HI',i,i+1)
    if shared:
        for i in range(n): head += p('HI',i,n+1)
    name_table = b'\x04'+p('H',0)*n+b'exit'
    sh = 'H'+{16:'H',32:'I',64:'Q'}[width]*2
    length = len(head)+(n+1+int(shared))*struct.calcsize(e+sh)+len(name_table)
    head += p(sh,0,length,length)
    for code in codes: head += p(sh,1,len(code),len(code))
    common = b'UEEC shared constant' if shared else b''
    if shared: head += p(sh,4,len(common),len(common))
    head += name_table
    assert len(head)==length
    return head+b''.join(codes)+common
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('out',type=Path); args=ap.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    for w in (16,32,64):
        for e in ('le','be'):
            (args.out/f'sh{w}-{e}.ueec').write_bytes(generate(w,e))
            (args.out/f'sh{w}-{e}-shared.ueec').write_bytes(generate(w,e,True))
    b=generate(16,'le'); cases={'bad-version':b[:3]+b'\x01'+b[4:],'bad-format':b[:4]+b'\xff\xff'+b[6:],'truncated':b[:9],'bad-magic':b'XYZ'+b[3:],'truncated-data':b[:-1]}
    for k,v in cases.items(): (args.out/f'{k}.ueec').write_bytes(v)
if __name__=='__main__': main()
