import pathlib,struct,subprocess,sys,tempfile,shlex,hashlib
arch,exe,runner=sys.argv[1:];good=bad=0
profiles=[('ATtiny85','1e930b',12,8192,512,512,64,15,4,0),('ATmega8','1e9307',12,8192,1024,512,64,19,5,0),('ATmega328P','1e950f',14,32768,2048,1024,128,26,7,0),('ATmega128','1e9702',16,131072,4096,4096,256,35,7,1),('ATmega2560','1e9801',17,262144,8192,4096,256,57,7,3)]
with tempfile.TemporaryDirectory() as tmp:
 path=pathlib.Path(tmp)/'layout.ueec'
 for be in (0,1):
  for fmt,width in enumerate('HIQ'):
   pack=lambda f,*a:struct.pack(('>' if be else '<')+f,*a)
   maxsize=(1<<(8*struct.calcsize(width)))-1
   for name,sig,pc,*fields in profiles:
    flash,sram=fields[:2]
    def fixture(parts):
     meta=pack('HH3sB7I',0,0,bytes.fromhex(sig),pc,*fields)
     sections=[(13,len(meta),meta)]+parts
     n=len(sections);hs=8+6+6*n+(2+2*struct.calcsize(width))*(n+1)+1+2+3
     sh=lambda t,s,d:pack('H'+width*2,t,s,d)
     return b'_GM'+bytes([1+128*be])+pack('HHIH',1,1,8|(1<<7)|(n<<27),fmt)+b''.join(pack('HI',0,k+1) for k in range(n))+sh(0,hs,hs)+b''.join(sh(t,s,len(b)) for t,s,b in sections)+b'\x03'+pack('H',0)+b'avr'+b''.join(b+bytes(s-len(b)) for t,s,b in sections)
    def run(parts,expected=None):
     global good,bad
     blob=fixture(parts);path.write_bytes(blob);r=subprocess.run(shlex.split(runner)+[exe,sig,str(path)],capture_output=True,text=True,timeout=30)
     assert hashlib.sha256(path.read_bytes()).digest()==hashlib.sha256(blob).digest()
     assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
     if expected is None:
      assert r.returncode==1 and 'LAYOUT' not in r.stdout,(r.returncode,r.stdout,r.stderr);bad+=1
     else:
      f,s,t,e=expected;assert r.returncode==0 and f'LAYOUT {name} flash={f} sram={s} stack={t} entry={e}' in r.stdout,(r.returncode,r.stdout,r.stderr);good+=1
    code=(1,2,b'\0\0')
    run([code],(2,0,32,0))
    run([(4,3,b'abc'),code,(2,5,b'ab'),(3,7,b'')],(8,12,32,4))
    run([code,(3,sram-32,b'')],(2,sram-32,32,0))
    run([code,(10,64,b''),(2,sram-64,b'x')],(3,sram-64,64,0))
    chunk=min(flash,maxsize-1); chunk-=chunk%2
    parts=[(1,chunk,bytes(chunk))];left=flash-chunk
    while left:
     sz=min(left,maxsize-1);sz-=sz%2;parts.append((1,sz,bytes(sz)));left-=sz
    run(parts,(flash,0,32,0));run(parts+[(4,1,b'x')])
    for p in [[(1,3,b'abc')],[(1,4,b'ab')],[code,(3,1,b'x')],[code,(2,2,b'')],[code,(3,sram-31,b'')],[code,(10,sram+1,b'')],[code,(10,1,b'')],[code,(10,2,b'x')],[code,(10,2,b''),(10,2,b'')],[code,(12,1,b'x')],[code,(5,1,b'x')],[code,(4,2,b'x')]]:run(p)
print(f'[PASS] {arch}: {good} layout cases, {bad} layout rejections')
