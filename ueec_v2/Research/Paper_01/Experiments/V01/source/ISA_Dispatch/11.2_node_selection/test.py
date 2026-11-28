import pathlib, struct, subprocess, sys, shlex
arch, exe, runner = sys.argv[1:]
ids = dict(zip('arm32 aarch64 loongarch64 ppc32 ppc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64 s390x'.split(),range(12)))
m = ids[arch]
big = {3,4,7,8,11}
def node(i, **kw):
    return (i, kw.get('kind',0),kw.get('endian',int(i in big)),kw.get('system',2),kw.get('user',1))
def fixture(nodes, be):
    p='>' if be else '<'
    pack=lambda fmt,*v:struct.pack(p+fmt,*v)
    count=len(nodes)
    # Shared logical name; implicit HEADER + one shared metadata section.
    header_size=8+6*count+6*count+12+1+2*count+4
    return (b'_GM'+bytes([1|be*128])+pack('HH',count,1)+b''.join(pack('IH',e|(u<<2)|(1<<3)|(k<<7)|(i<<10)|(2<<18)|(s<<20)|(1<<27),0) for i,k,e,s,u in nodes)+b''.join(pack('HI',j,1) for j in range(count))+pack('HHH',0,header_size,header_size)+pack('HHH',13,1,1)+b'\x04'+pack('H'*count,*([0]*count))+b'test'+b'\x00')
base=[node(i) for i in [8,2,10,0,11,4,1,9,6,3,7,5]]
folder=pathlib.Path(exe).parent
for be in (0,1):
    cases=[('all',base,0,f'SELECTED machine={m} node={next(j for j,n in enumerate(base) if n[0]==m)}'),('missing',[n for n in base if n[0]!=m],2,'NO_MATCH'),('duplicate',base+[node(m)],3,'AMBIGUOUS'),('wrong_kind',[node(m,kind=2)],2,'NO_MATCH'),('wrong_endian',[node(m,endian=int(m not in big))],2,'NO_MATCH'),('wrong_os',[node(m,system=4)],2,'NO_MATCH'),('wrong_mode',[node(m,user=0)],2,'NO_MATCH')]
    for name,nodes,rc,out in cases+[('truncated',base,4,'INVALID'),('bad_magic',base,4,'INVALID')]:
        data=fixture(nodes,be)
        if name=='truncated': data=data[:10]
        if name=='bad_magic': data=b'BAD'+data[3:]
        path=folder/f'{name}-{be}.ueec'; path.write_bytes(data)
        r=subprocess.run(shlex.split(runner)+[exe,str(path)],capture_output=True,text=True)
        if (r.returncode,r.stdout.strip())!=(rc,out): raise SystemExit(f'FAIL {arch} {name} {be}: {r.returncode} {r.stdout} {r.stderr}')
print(f'[PASS] {arch}: 18 cases (LE/BE metadata)')
