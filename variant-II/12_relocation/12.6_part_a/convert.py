"""UEEC v1 relocation representation converter (Couple16, uniform SH format)."""
import argparse
import pathlib
import struct

FORMATS = {'flat': (0,0), 'b4k16': (1,0), 'b4k32': (1,1), 'b4k64': (1,2),
           'bn16': (2,0), 'bn32': (2,1), 'bn64': (2,2)}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def decode(data, p):
    require(len(data)>=4, 'truncated relocation header')
    rf, design = struct.unpack_from(p+'HH', data)
    require((design,rf) in FORMATS.values(), 'unsupported representation')
    w=2<<rf; code='HIQ'[rf]; shift=w*8-4; mask=(1<<shift)-1
    pos=4; couples=[]
    def get(fmt):
        nonlocal pos
        size=struct.calcsize(p+fmt)
        require(pos+size<=len(data), 'truncated relocation record')
        out=struct.unpack_from(p+fmt,data,pos); pos+=size
        return out
    while pos<len(data):
        c,count=get('HH'); require(c<1024 and count, 'invalid couple')
        entries=[]
        for _ in range(count if design else 1):
            base,n=get(code*2) if design else (0,count)
            require(n and n<=(len(data)-pos)//w, 'invalid block count')
            require(design!=1 or base%4096==0, 'unaligned block base')
            for _ in range(n):
                v,=get(code); delta=v&mask
                require(design!=1 or delta<4096, 'invalid block offset')
                require(base+delta<1<<64, 'offset overflow')
                entries.append((base+delta,v>>shift))
        couples.append((c,entries))
    require(couples, 'empty relocations')
    return couples


def encode(couples,p,design,rf,block_n=24):
    require((design,rf) in FORMATS.values(), 'unsupported target')
    require(block_n>0, 'invalid block size')
    w=2<<rf; shift=w*8-4; mask=(1<<shift)-1; limit=(1<<(w*8))-1; code='HIQ'[rf]
    pack=lambda f,*v:struct.pack(p+f,*v)
    out=bytearray(pack('HH',rf,design))
    for c,entries in couples:
        require(0<=c<1024 and entries, 'invalid couple')
        groups=[]
        for offset,typ in entries:
            require(0<=offset<1<<64 and 0<=typ<16, 'invalid record')
            page=4096 if design==1 else block_n
            base=offset//page*page if design else 0
            delta=offset-base
            require(base<=limit and delta<=mask, 'target offset not representable')
            if not groups or groups[-1][0]!=base: groups.append((base,[]))
            groups[-1][1].append(delta|(typ<<shift))
        count=len(groups) if design else len(entries)
        require(count<=65535, 'couple count overflow')
        out+=pack('HH',c,count)
        for base,values in groups:
            if design:
                require(len(values)<=limit, 'block count overflow')
                out+=pack(code*2,base,len(values))
            out+=b''.join(pack(code,v) for v in values)
    return bytes(out)


def sections(data):
    require(len(data)>=8 and data[:3]==b'_GM' and data[3]&127==1,'invalid UEEC header')
    p='>' if data[3]&128 else '<'; nodes,names=struct.unpack_from(p+'HH',data,4)
    require(0<names<=nodes and 8+6*nodes<=len(data),'invalid nodes')
    nh=[struct.unpack_from(p+'IH',data,8+6*i) for i in range(nodes)]
    fmt=nh[0][1]; require(fmt<3 and all(f==fmt for _,f in nh),'mixed/unsupported SH')
    count=sum(v>>27 for v,_ in nh); start=8+6*nodes+6*count
    require(start<=len(data),'truncated mappings')
    maps=[struct.unpack_from(p+'HI',data,8+6*nodes+6*i) for i in range(count)]
    require(maps and all(n<nodes and s>0 for n,s in maps),'invalid mappings')
    for i,(v,_) in enumerate(nh): require(sum(n==i for n,s in maps)==v>>27,'mapping count')
    total=1+max(s for n,s in maps); sf='H'+'HIQ'[fmt]*2; sw=struct.calcsize(p+sf)
    names_at=start+total*sw; require(names_at+names+nodes*2<=len(data),'truncated section headers')
    meta=[struct.unpack_from(p+sf,data,start+i*sw) for i in range(total)]
    lengths=data[names_at:names_at+names]; require(all(lengths),'empty name')
    indexes=struct.unpack_from(p+'H'*nodes,data,names_at+names); require(all(i<names for i in indexes),'invalid name index')
    header_end=names_at+names+nodes*2+sum(lengths)
    require(meta[0]==(0,header_end,header_end) and header_end<=len(data),'invalid header size')
    pos=header_end; bodies=[]
    for t,s,d in meta[1:]:
        require(d<=s and s<=len(data)-pos,'invalid section span')
        bodies.append(data[pos:pos+s]); pos+=s
    require(pos==len(data),'unexpected trailing bytes')
    return p,start,sf,sw,meta,bodies,header_end


def convert(data, design, rf, block_n=24):
    p,start,sf,sw,meta,bodies,header_end=sections(data)
    header=bytearray(data[:header_end]); sizes=[]
    for i,(t,s,d) in enumerate(meta[1:]):
        if t!=5: continue
        original=bodies[i][:d]; canonical=decode(original,p)
        encoded=encode(canonical,p,design,rf,block_n)
        require(decode(encoded,p)==canonical,'conversion mismatch')
        require(len(encoded)<1<<(8*((sw-2)//2)),'section size overflow')
        # Keep existing padding bytes verbatim, adjusting section size.
        body=encoded+bodies[i][d:]; require(len(body)<1<<(8*((sw-2)//2)),'section size overflow')
        struct.pack_into(p+sf,header,start+(i+1)*sw,t,len(body),len(encoded))
        bodies[i]=body; sizes.append((d,len(encoded)))
    require(sizes,'no relocation sections')
    return bytes(header)+b''.join(bodies),sizes


def main():
    ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('source',type=pathlib.Path); ap.add_argument('destination',type=pathlib.Path); ap.add_argument('--format',choices=FORMATS,required=True); ap.add_argument('--block-n',type=int,default=24)
    a=ap.parse_args()
    try:
        require(a.source.resolve()!=a.destination.resolve(),'source and destination must differ')
        result,sizes=convert(a.source.read_bytes(),*FORMATS[a.format],a.block_n)
        with a.destination.open('xb') as f: f.write(result)
        print(f'Converted {len(sizes)} relocation sections: {sum(x for x,y in sizes)} -> {sum(y for x,y in sizes)} bytes')
    except (ValueError,OSError,struct.error) as e: ap.exit(1,f'conversion rejected: {e}\n')

if __name__=='__main__': main()
