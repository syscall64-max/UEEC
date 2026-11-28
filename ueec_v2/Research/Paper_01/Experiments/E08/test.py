import struct as s, csv, mmap, ctypes, platform
from pathlib import Path
assert platform.machine()=='x86_64' and platform.system()=='Linux'
counts=[1,16,64,256,1024,4096]
rows=[]; checks=dict(lookup=0,conversion=0,rejection=0,native=0)
def enc(entries,w,d,e):
    f={2:0,4:1,8:2}[w]; q={2:'H',4:'I',8:'Q'}[w]
    def p(fmt,*a):return s.pack(e+fmt,*a)
    if d==0:
        g={}
        for k,sec,o in sorted(entries):g.setdefault(len(k),[]).append((k,sec,o))
        return p('HHH'+q,f,d,f,len(g))+b''.join(p('H'+q,l,len(g[l])) for l in sorted(g))+b''.join(b''.join(p('B'+q,sec,o) for k,sec,o in g[l])+b''.join(k for k,sec,o in g[l]) for l in sorted(g))
    a=sorted(entries); h=p('HHH'+q,f,d,f,len(a))
    if d==2:return h+b''.join(p('B'+q+q,sec,o,k) for k,sec,o in a)
    b=b''; t=b''
    for k,sec,o in a:
        if not 0<len(k)<=2047:raise ValueError('name length')
        t+=p('H'+q+q,sec|(len(k)<<5),o,len(b)); b+=k
    return h+t+b
def dec(b,e):
    f,d,f2=s.unpack_from(e+'HHH',b); assert f==f2 and f in range(3) and d in range(3)
    w=2<<f;q={2:'H',4:'I',8:'Q'}[w]; pos=6
    def get(fmt):
        nonlocal pos
        a=s.unpack_from(e+fmt,b,pos);pos+=s.calcsize(e+fmt);return a
    n,=get(q); a=[]
    if d==0:
        groups=[get('H'+q) for _ in range(n)]
        for l,c in groups:
            infos=[get('B'+q) for _ in range(c)]
            for sec,o in infos:
                k=b[pos:pos+l];pos+=l;assert len(k)==l;a.append((k,sec,o))
    elif d==1:
        infos=[get('H'+q+q) for _ in range(n)]; start=pos
        for v,o,k in infos:
            l=v>>5; name=b[start+k:start+k+l];assert len(name)==l;a.append((name,v&31,o))
        pos=start+sum(len(k) for k,_,_ in a)
    else:
        for _ in range(n):
            sec,o,k=get('B'+q+q);a.append((k,sec,o))
    assert pos==len(b) and all(sec<32 for _,sec,_ in a) and len({k for k,_,_ in a})==len(a)
    return sorted(a)
for n in counts:
 for pattern in ['fixed','mixed']:
    named=[((f's{i:05d}'+('x'*(i%12) if pattern=='mixed' else '')).encode(),1,i*6) for i in range(n)]
    ordinal=[(i+1,1,i*6) for i in range(n)]
    code=b''.join(b'\xb8'+s.pack('<I',i+1)+b'\xc3' for i in range(n))
    mem=mmap.mmap(-1,len(code),prot=mmap.PROT_READ|mmap.PROT_WRITE|mmap.PROT_EXEC);mem.write(code);addr=ctypes.addressof(ctypes.c_char.from_buffer(mem))
    for e in ['<','>']:
     for w in [2,4,8]:
      for d in range(3):
        entries=ordinal if d==2 else named
        b=enc(entries,w,d,e); a=dec(b,e);assert a==sorted(entries)
        lookup={k:(sec,o) for k,sec,o in a}
        for k,sec,o in entries:
            assert lookup[k]==(sec,o);checks['lookup']+=1
        assert (0 if d==2 else b'absent') not in lookup;checks['lookup']+=1
        for i in sorted({0,n//2,n-1}):
            k=entries[i][0]; assert ctypes.CFUNCTYPE(ctypes.c_uint32)(addr+lookup[k][1])()==i+1;checks['native']+=1
        if d!=2:
            for target in [2,4,8]:
                other=enc(a,target,1-d,e);assert dec(other,e)==a;assert enc(dec(other,e),w,d,e)==b;checks['conversion']+=1
        for bad in [b[:-1],b+b'\0']:
            try:dec(bad,e)
            except (AssertionError,s.error,NameError):checks['rejection']+=1
            else:raise AssertionError('accepted malformed')
        if e=='<': rows.append([n,pattern,w*8,['COMPACT_GROUP','SIMPLE_FLAT','ORDINAL'][d],len(b),24*(n+1)+1+sum(len(k)+1 for k,_,_ in named),40+4*n+6*n+sum(len(k)+1 for k,_,_ in named)])
    mem.close()
with open('results.csv','w',newline='') as f:
    out=csv.writer(f);out.writerow(['symbols','pattern','width','design','ueec_bytes','elf_dynsym_dynstr_bytes','pe_named_directory_tables_strings_bytes']);out.writerows(rows)
for k,v in checks.items():print(f'{k}: {v} PASS')
print('total:',sum(checks.values()),'PASS')
for r in rows:
 if r[0]==4096 and r[2]==16:print(*r,sep=',')
