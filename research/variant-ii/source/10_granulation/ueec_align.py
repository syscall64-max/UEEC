#!/usr/bin/env python3
"""Granulate Variant II sections, preserving payloads and header metadata."""
import argparse
from pathlib import Path


def transform(b, granule):
    def check(ok, msg):
        if not ok: raise ValueError(msg)
    check(1 <= granule <= 16384, 'invalid granule (1..16384)')
    check(len(b)>=10 and b[:3]==b'_GM' and b[3]&127==2,'bad header')
    e='big' if b[3]&128 else 'little'
    pos=4
    def get(w):
        nonlocal pos
        check(pos+w<=len(b),'truncated metadata')
        v=int.from_bytes(b[pos:pos+w],e); pos+=w; return v
    fmt,n,k=get(2),get(2),get(2)
    check(fmt<=2 and 0<k<=n<=65,'invalid format/count')
    w=2<<fmt; words=[get(4) for _ in range(n)]
    counts=[0]*n; refs=set()
    for _ in range(sum(v>>27 for v in words)):
        i,s=get(2),get(4)
        check(i<n and s>0,'invalid link'); counts[i]+=1; refs.add(s)
    check(counts==[v>>27 for v in words] and refs==set(range(1,len(refs)+1)),'invalid links')
    start=pos; hdr=[(get(2),get(w),get(w)) for _ in range(len(refs)+1)]
    lengths=[get(1) for _ in range(k)]; indexes=[get(2) for _ in range(n)]
    check(all(lengths) and all(i<k for i in indexes),'invalid names')
    pos+=sum(lengths)
    check(hdr[0][0]==0 and hdr[0][2]==pos,'invalid header size')
    off=0; payloads=[]; sizes=[]
    for t,s,d in hdr:
        check(d<=s and off+s<=len(b),'section bounds')
        check(not any(b[off+d:off+s]),'nonzero padding')
        size=((d+granule-1)//granule)*granule
        check(size < 1<<(8*w),'section size overflow')
        sizes.append(size); payloads.append(b[off:off+d]); off+=s
    check(off==len(b),'trailing bytes')
    h=bytearray(payloads[0])
    for i,size in enumerate(sizes):
        p=start+i*(2+2*w)+2; h[p:p+w]=size.to_bytes(w,e)
    payloads[0]=h
    return b''.join(p+bytes(s-len(p)) for p,s in zip(payloads,sizes))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path); p.add_argument('output',type=Path)
    p.add_argument('--granularity',type=int,required=True); a=p.parse_args()
    try:
        result=transform(a.input.read_bytes(),a.granularity)
        with a.output.open('xb') as f: f.write(result)
    except (ValueError,OSError,OverflowError) as exc: p.exit(1,f'ALIGN error: {exc}\n')

if __name__=='__main__': main()
