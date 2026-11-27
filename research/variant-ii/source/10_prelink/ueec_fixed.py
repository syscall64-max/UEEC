#!/usr/bin/env python3
"""Add experimental load-info metadata to an ungranulated combined image."""
import argparse
from pathlib import Path
from ueec_merge import parse, require
from ueec_align import transform

def prepare(path, base, page, fmt, granule):
    e, sh, nodes = parse(path)
    require(page>0 and page&(page-1)==0 and base>0 and base%page==0,'invalid base/page')
    require(fmt in (0,1,2),'invalid address format')
    require(sum(bool(v&8) for v,_,_ in nodes)==1,'one program required')
    nodes=[(v,n,list(ss)) for v,n,ss in nodes]
    idx=next(i for i,(v,_,_) in enumerate(nodes) if v&8)
    v,n,ss=nodes[idx]; require(len(ss)<31,'section count overflow')
    aw=2<<fmt; ss.append((13,bytes(6+len(nodes)*(2+aw))))
    nodes[idx]=((v&0x07ffffff)|(len(ss)<<27),n,ss)
    def u(x,w): return x.to_bytes(w,e)
    w=2<<sh; p=lambda x:u(x,2)
    sections=[s for _,_,ss in nodes for s in ss]
    names=bytes(len(n) for _,n,_ in nodes)+b''.join(p(i) for i in range(len(nodes)))+b''.join(n for _,n,_ in nodes)
    links=b''; i=1
    for j,(_,_,ss) in enumerate(nodes):
        for _ in ss: links+=p(j)+u(i,4); i+=1
    h=b'_GM'+bytes([2+128*(e=='big')])+p(sh)+p(len(nodes))*2+b''.join(u(v,4) for v,_,_ in nodes)+links
    size=len(h)+(len(sections)+1)*(2+2*w)+len(names)
    def advance(n):
        padded=(n+granule-1)//granule*granule
        return (padded+page-1)//page*page
    addr=base+advance(size); meta=p(1)+p(fmt)+p(len(nodes))
    for j,(_,_,ss) in enumerate(nodes):
        meta+=p(j)+u(addr,aw)
        addr+=sum(advance(len(b)) for _,b in ss)
    sections=[(t,meta if t==13 else b) for t,b in sections]
    raw=h+p(0)+u(size,w)*2+b''.join(p(t)+u(len(b),w)*2 for t,b in sections)+names+b''.join(b for _,b in sections)
    return transform(raw,granule)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('input',type=Path); p.add_argument('output',type=Path); p.add_argument('--base',type=lambda x:int(x,0),required=True); p.add_argument('--page',type=int,required=True); p.add_argument('--format',type=int,choices=(16,32,64),default=64); p.add_argument('--granularity',type=int,default=1); a=p.parse_args()
    try:
        result=prepare(a.input,a.base,a.page,{16:0,32:1,64:2}[a.format],a.granularity)
        with a.output.open('xb') as f: f.write(result)
    except (ValueError,OverflowError,OSError) as exc: p.exit(1,f'FIXED error: {exc}\n')
