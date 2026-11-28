import pathlib, struct, subprocess, sys, shlex
arch, exe, runner = sys.argv[1:]
ids = dict(zip('arm32 aarch64 loongarch64 ppc32 ppc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64 s390x'.split(),range(12)))
m = ids[arch]; big={3,4,7,8,11}; order=[8,2,10,0,11,4,1,9,6,3,7,5]
folder=pathlib.Path(exe).parent
count=0
for be in (0,1):
 for fmt, width in enumerate('HIQ'):
    p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v)
    def fixture(case,value):
        nodes=order.copy(); mappings=[]; sections=[]
        for j,i in enumerate(nodes):
            payload=(pathlib.Path("build/payloads") / next(a for a,k in ids.items() if k==i) / f"{(0,42,255)[((0,42,255).index(value)+i)%3]}.bin").read_bytes()
            sections.append((1,payload,32)); mappings.append([j+1,13,14,15])
        sections.extend([(4,b'shared',16),(2,b'initial',16),(3,b'',32)])
        selected=nodes.index(m)
        if case=='missing': nodes[selected]=(m+1)%12
        if case=='ambiguous': nodes[(selected+1)%12]=m
        if case=='duplicate_mapping': mappings[selected][1]=mappings[selected][0]
        if case=='no_code': sections[selected]=(4,b'not code',16)
        if case=='empty_code': sections[selected]=(1,b'',16)
        n=len(nodes); sh=lambda t,s,d:pack('H'+width*2,t,s,d)
        hs=8+6*n+6*4*n+(2+2*struct.calcsize(width))*(1+len(sections))+1+2*n+4
        h=b'_GM'+bytes([1|be*128])+pack('HH',n,1)
        h+=b''.join(pack('IH',int(i in big)|12|(i<<10)|(2<<18)|(2<<20)|(4<<27),fmt) for i in nodes)
        h+=b''.join(pack('HI',j,k) for j,ks in enumerate(mappings) for k in ks)
        h+=sh(0,hs,hs)+b''.join(sh(t,s,len(d)) for t,d,s in sections)+b'\x04'+pack('H'*n,*([0]*n))+b'test'
        data=h+b''.join(d.ljust(s,b'\0') for t,d,s in sections)
        if case=='truncated': data=data[:-1]
        if case=='bad_magic': data=b'BAD'+data[3:]
        if case=='bad_version': data=data[:3]+bytes([2|be*128])+data[4:]
        return data
    def run(case,mode,ok,value=42,no_run=False):
        global count
        path=folder/f'{case}-{be}-{fmt}-{value}.ueec'; path.write_bytes(fixture(case,value))
        r=subprocess.run(shlex.split(runner)+[exe,'--load-mode',mode]+(['--no-run'] if no_run else [])+[str(path)],capture_output=True,text=True,timeout=30)
        if ok:
            expected=[f'node:                {order.index(m)} (test)','runtime sections:    5',f'regions: {dict(auto=1,monolithic=1,scattered=5,hybrid=2)[mode]}',f'global={order.index(m)+1} type=1 data=','global=13 type=4 data=6','global=14 type=2 data=7','global=15 type=3 data=0','LOADED']
            good=r.returncode==0 and all(x in r.stdout for x in expected) and ('EXECUTED' not in r.stdout if no_run else f'EXECUTED result={(0,42,255)[((0,42,255).index(value)+m)%3]}\n' in r.stdout)
        else: good=r.returncode==1 and 'LOADED' not in r.stdout and 'EXECUTED' not in r.stdout
        if not good or 'runtime error:' in r.stderr or 'AddressSanitizer' in r.stderr: raise SystemExit(f'FAIL {arch} {case} {mode} {be} {fmt}: {r.returncode}\n{r.stdout}\n{r.stderr}')
        count+=1
    for mode in ('auto','monolithic','scattered','hybrid'):
        for value in (0,42,255): run('valid',mode,True,value)
    run('valid','scattered',True,no_run=True)
    for case in ('missing','ambiguous','duplicate_mapping','no_code','empty_code','truncated','bad_magic','bad_version'): run(case,'scattered',False)
print(f'[PASS] {arch}: {count} cases (72 execution, 6 load-only, 48 rejection)')
