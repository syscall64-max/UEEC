#!/usr/bin/env python3
"""Stage 10.4: merge compatible, ungranulated Variant II containers."""
import argparse
from pathlib import Path
import struct


def require(ok, message):
    if not ok:
        raise ValueError(message)


def parse(path):
    b = Path(path).read_bytes()
    require(len(b)>=10 and b[:3]==b'_GM' and b[3]&127==2,'bad header')
    endian = 'big' if b[3]&128 else 'little'
    pos=4
    def get(w):
        nonlocal pos
        require(pos+w<=len(b),'truncated field')
        v=int.from_bytes(b[pos:pos+w],endian); pos+=w; return v
    fmt,n,k=get(2),get(2),get(2)
    require(fmt<=2 and 0<k<=n<=65,'invalid format or count')
    w=2<<fmt
    words=[get(4) for _ in range(n)]
    links=[[] for _ in words]
    for _ in range(sum(x>>27 for x in words)):
        i,s=get(2),get(4)
        require(i<n and s>0,'bad link'); links[i].append(s)
    require(all(len(l)==v>>27 for l,v in zip(links,words)),'bad node links')
    flat=[s for l in links for s in l]
    require(sorted(flat)==list(range(1,len(flat)+1)),'shared or sparse sections unsupported')
    headers=[(get(2),get(w),get(w)) for _ in range(len(flat)+1)]
    lengths=[get(1) for _ in range(k)]; indexes=[get(2) for _ in words]
    names=[]
    for l in lengths:
        require(l and pos+l<=len(b),'bad name'); names.append(b[pos:pos+l]); pos+=l
    require(all(i<k for i in indexes),'bad name index')
    require(headers[0]==(0,pos,pos),'granulated or invalid header')
    sections=[None]
    for t,s,d in headers[1:]:
        require(s==d and pos+s<=len(b),'granulated or truncated section')
        require(t in (1,2,3,4,6,7),'unsupported section')
        sections.append((t,b[pos:pos+s])); pos+=s
    require(pos==len(b),'trailing bytes')
    return endian,fmt,[(word,names[idx],[sections[s] for s in l]) for word,idx,l in zip(words,indexes,links)]


def rewrite(data,endian,aliases):
    def read(p,w):
        require(p+w<=len(data),'truncated import'); return int.from_bytes(data[p:p+w],endian)
    require(read(0,2)==0 and read(2,2)<=2,'unsupported import')
    w=2<<read(2,2); n=read(4,w); start=4+w
    require(n<=64,'library limit')
    m=sum(read(start+(n+i)*w,w) for i in range(n)); require(m<=4096,'object limit')
    base=start+(2*n+3*m)*w; require(base<=len(data),'truncated import arrays')
    flag=1<<(8*w-1); names=data[base:]; out=bytearray(data[:base]); pool=bytearray(); offsets={}
    def name(off):
        require(off<len(names) and (not off or names[off-1]==0),'bad name offset')
        end=names.find(b'\0',off); require(end>off,'bad name'); return names[off:end]
    def intern(s):
        if s not in offsets:
            offsets[s]=len(pool); pool.extend(s+b'\0')
        return offsets[s]
    for i in range(n):
        p=start+i*w; v=read(p,w); s=name(v&~flag); external=v&flag
        if external and s in aliases: s=aliases[s]; external=0
        off=intern(s); require(off<flag,'library offset overflow'); out[p:p+w]=(off|external).to_bytes(w,endian)
    for i in range(m):
        p=start+(2*n+3*i+2)*w; out[p:p+w]=intern(name(read(p,w))).to_bytes(w,endian)
    return bytes(out+pool)


def merge(paths):
    parsed=[parse(p) for p in paths]; endian=parsed[0][0]
    require(all(e==endian for e,_,_ in parsed),'mixed container endian unsupported')
    nodes=[node for _,_,ns in parsed for node in ns]
    require(0<len(nodes)<=65,'node limit')
    require(sum(bool(word&8) for word,_,_ in nodes)==1,'exactly one program required')
    require(len({name for _,name,_ in nodes})==len(nodes),'duplicate node names')
    aliases={}
    for path,(_,_,ns) in zip(paths,parsed):
        if len(ns)==1 and ns[0][0]&16:
            key=Path(path).name.encode(); require(key not in aliases,'duplicate library filename'); aliases[key]=ns[0][1]
    nodes=[(word,name,[(t,rewrite(b,endian,aliases) if t==7 else b) for t,b in secs]) for word,name,secs in nodes]
    fmt=max(f for _,f,_ in parsed); secs=[s for _,_,ss in nodes for s in ss]
    require(len(secs)<=2015,'section limit')
    def num(v,w): return v.to_bytes(w,endian)
    p=lambda v:num(v,2)
    names=bytes(len(n) for _,n,_ in nodes)+b''.join(p(i) for i in range(len(nodes)))+b''.join(n for _,n,_ in nodes)
    links=b''; i=1
    for j,(_,_,ss) in enumerate(nodes):
        for _ in ss: links+=p(j)+num(i,4); i+=1
    while True:
        w=2<<fmt; h=b'_GM'+bytes([2+(128 if endian=='big' else 0)])+p(fmt)+p(len(nodes))*2+b''.join(num(v,4) for v,_,_ in nodes)+links
        size=len(h)+(len(secs)+1)*(2+2*w)+len(names)
        if max([size]+[len(b) for _,b in secs])<1<<(8*w): break
        fmt+=1; require(fmt<=2,'section width overflow')
    return h+p(0)+num(size,w)*2+b''.join(p(t)+num(len(b),w)*2 for t,b in secs)+names+b''.join(b for _,b in secs)


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('output',type=Path); ap.add_argument('inputs',nargs='+',type=Path); a=ap.parse_args()
    try:
        require(len(a.inputs)>=2,'at least two inputs required')
        require(not a.output.exists(),'output already exists')
        result=merge(a.inputs)
        with a.output.open('xb') as f: f.write(result)
    except (ValueError, OSError, OverflowError) as e: ap.exit(1,f'MERGE error: {e}\n')
