#!/usr/bin/env python3
"""Offline prelink of the stage 10.7.2 closed, fixed-layout experimental ABI."""
import argparse
from pathlib import Path
from ueec_merge import require
from ueec_align import transform


def prelink(data, page):
    require(page>0 and page&(page-1)==0,'invalid page')
    # Validate section sizes, padding, links and header without altering input.
    transform(data,1)
    e='big' if data[3]&128 else 'little'
    def num(b,o,w):
        require(o>=0 and o+w<=len(b),'truncated field')
        return int.from_bytes(b[o:o+w],e)
    def u(o,w): return num(data,o,w)
    w=2<<u(4,2); n=u(6,2); k=u(8,2)
    words=[u(10+4*i,4) for i in range(n)]
    links=sum(v>>27 for v in words); q=10+4*n; orders=[[] for _ in words]
    for i in range(links):
        nd,s=u(q+6*i,2),u(q+6*i+2,4); orders[nd].append(s)
    require(sorted(s for order in orders for s in order)==list(range(1,links+1)), 'shared sections unsupported')
    q+=6*links; sections=[]; pos=runtime=0
    for i in range(links+1):
        t,z,d=u(q,2),u(q+2,w),u(q+2+w,w); q+=2+2*w
        sections.append((t,pos,z,d,runtime)); pos+=z; runtime+=(z+page-1)//page*page
    lengths=data[q:q+k]; q+=k; indices=[u(q+2*i,2) for i in range(n)]; q+=2*n
    names=[]
    for length in lengths: names.append(data[q:q+length]); q+=length
    names=[names[i] for i in indices]; require(len(set(names))==n,'duplicate node names')
    def raw(i):
        _,p,_,d,_=sections[i]; return data[p:p+d]
    def one(order,t):
        a=[i for i in order if sections[i][0]==t]; require(len(a)==1,f'expected one section type {t}'); return a[0]
    progs=[i for i,v in enumerate(words) if v&8]; require(len(progs)==1,'one program required'); prog=progs[0]
    machine=(words[prog]>>10)&255; native='big' if words[prog]&1 else 'little'
    require(machine<12,'unknown machine'); ptr=4 if machine in (0,3,5,7,9) else 8
    require(all((v&0x07ffff87)==(words[prog]&0x07ffff87) for v in words),'incompatible node')
    metadata=one(range(1,links+1),13); meta=raw(metadata)
    require(num(meta,0,2)==1,'expected unprocessed LOAD_INFO'); af=num(meta,2,2); require(af<=2,'invalid address format'); aw=2<<af
    require(num(meta,4,2)==n and len(meta)==6+n*(2+aw),'invalid metadata count')
    bases=set(); seen=set()
    for j in range(n):
        nd=num(meta,6+j*(2+aw),2); addr=num(meta,8+j*(2+aw),aw)
        require(nd<n and nd not in seen and orders[nd] and addr%page==0,'invalid metadata address'); seen.add(nd)
        bases.add(addr-sections[orders[nd][0]][4])
    require(len(bases)==1,'inconsistent layout'); base=bases.pop(); require(base>0 and base%page==0 and base+runtime<=1<<(8*ptr),'address range overflow')
    order=orders[prog]; dst=one(order,2); reloc=one(order,5); imp=one(order,7)
    out=bytearray(data); written=set(); descriptor_count=0
    def put(i,off,val):
        _,p,_,d,_=sections[i]; require(off>=0 and off+ptr<=d and off%ptr==0,'invalid patch offset')
        require((i,off) not in written,'overlapping patch'); written.add((i,off)); out[p+off:p+off+ptr]=val.to_bytes(ptr,native)
    def address(i,off):
        t,_,_,d,r=sections[i]; require(t in (1,2,3,4) and 0<=off<d,'invalid address'); return base+r+off
    def resident(nd): return [i for i in orders[nd] if sections[i][0] in (1,2,3,4)]
    # Experiment-only BLOCK_N/32, one block, native ABS64.
    r=raw(reloc); require(ptr==8 and machine==10 and native=='little','x86-64 only')
    require(len(r)>=16 and num(r,0,2)==1 and num(r,2,2)==2,'expected BLOCK_N/32')
    c=num(r,4,2); blocks=num(r,6,2); t=c&31; s=(c>>5)&31
    require(c>>10==0 and t==s==2 and blocks==1 and num(r,8,4)==0,'invalid couple/block')
    count=num(r,12,4); require(1<=count<=100000 and len(r)==16+4*count,'invalid count')
    require(order[t-1]==dst,'invalid target')
    dst_raw=raw(dst)
    for i in range(count):
        x=num(r,16+4*i,4); off=x&0xfffffff
        require(x>>28==1 and off==(32 if i==0 else 96+8*(i-1)),'invalid patch')
        addend=int.from_bytes(dst_raw[off:off+ptr],native); require(addend==40,'invalid addend'); put(dst,off,address(order[s-1],addend))
    def symbol(nd,name):
        b=raw(one(orders[nd],6)); ef=num(b,0,2); design=num(b,2,2); cf=num(b,4,2)
        require(ef<=2 and cf<=2,'invalid export width'); ew=2<<ef; cw=2<<cf; count=num(b,6,cw); p=6+cw; hits=[]
        if design==0:
            require(count<4097,'export group limit'); groups=[(num(b,p+j*(2+cw),2),num(b,p+j*(2+cw)+2,cw)) for j in range(count)]; p+=count*(2+cw)
            for length,nn in groups:
                require(length>0 and nn<=4096 and p+nn*(1+ew+length)<=len(b),'invalid export group')
                start=p+nn*(1+ew)
                for j in range(nn):
                    if b[start+j*length:start+(j+1)*length]==name: hits.append((num(b,p+j*(1+ew),1)&31,num(b,p+j*(1+ew)+1,ew)))
                p=start+nn*length
            require(p==len(b),'trailing export data')
        elif design==1:
            require(count<=10000 and p+count*(2+2*ew)<=len(b),'invalid flat export'); start=p+count*(2+2*ew)
            lo,hi=0,count
            while lo<hi:
                j=(lo+hi)//2; x=p+j*(2+2*ew); bits=num(b,x,2); off=num(b,x+2+ew,ew); length=bits>>5
                require(length and start+off+length<=len(b),'invalid export name')
                key=b[start+off:start+off+length]
                if key==name: hits.append((bits&31,num(b,x+2,ew))); break
                if key<name: lo=j+1
                else: hi=j
        else: raise ValueError('unsupported export design')
        require(len(hits)==1,'missing or duplicate export'); sec,off=hits[0]; rs=resident(nd); require(sec<len(rs),'invalid export section'); return rs[sec],off
    b=raw(imp); f=num(b,2,2); require(num(b,0,2)==0 and f<=2,'invalid import'); iw=2<<f; nn=num(b,4,iw); require(nn==2,'experimental import library count'); p=4+iw
    counts=[num(b,p+(nn+j)*iw,iw) for j in range(nn)]; require(4<=sum(counts)<=10000,'experimental import count'); start=p+2*nn*iw; pool=b[start+3*sum(counts)*iw:]
    def name(off):
        require(off<len(pool) and (off==0 or pool[off-1]==0),'invalid name offset'); end=pool.find(b'\0',off); require(end>off,'invalid name'); return pool[off:end]
    obj=0
    for j in range(nn):
        v=num(b,p+j*iw,iw); require(v<1<<(8*iw-1),'external import unsupported'); lib=name(v); require(lib in names,'missing internal library'); nd=names.index(lib); require(words[nd]&16 and not any(sections[i][0] in (5,7) for i in orders[nd]),'unsupported library')
        for _ in range(counts[j]):
            q=start+obj*3*iw; sec,off,no=(num(b,q+i*iw,iw) for i in range(3)); rs=resident(prog)
            require(sec<len(rs) and rs[sec]==dst and off==(obj*ptr if obj<4 else 96+(obj-4)*ptr),'invalid import slot')
            target,delta=symbol(nd,name(no)); val=address(target,delta)
            if machine==4 and sections[target][0]==1: # PPC64 ELFv1 fixture ABI
                desc=(6+3*descriptor_count)*ptr; descriptor_count+=1
                put(dst,desc,val); put(dst,desc+ptr,0); put(dst,desc+2*ptr,0); val=address(dst,desc)
            put(dst,off,val); obj+=1
    require(all((dst,i*ptr) in written for i in range(5)),'missing entry slots')
    m=sections[metadata][1]; out[m:m+2]=(2).to_bytes(2,e)
    return bytes(out)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('input',type=Path); p.add_argument('output',type=Path); p.add_argument('--page',type=int,required=True); a=p.parse_args()
    try:
        result=prelink(a.input.read_bytes(),a.page)
        with a.output.open('xb') as f: f.write(result)
    except (ValueError,OverflowError,OSError) as exc: p.exit(1,f'PRELINK error: {exc}\n')
