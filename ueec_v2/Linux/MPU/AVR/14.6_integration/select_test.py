import pathlib,struct,subprocess,sys,tempfile,shlex,hashlib
arch,exe,runner=sys.argv[1:]; good=bad=0
profiles=[('ATtiny85','1e930b',12,8192,512,512,64,15,4,0),('ATmega8','1e9307',12,8192,1024,512,64,19,5,0),('ATmega328P','1e950f',14,32768,2048,1024,128,26,7,0),('ATmega128','1e9702',16,131072,4096,4096,256,35,7,1),('ATmega2560','1e9801',17,262144,8192,4096,256,57,7,3)]
with tempfile.TemporaryDirectory() as tmp:
 path=pathlib.Path(tmp)/'test.ueec'
 for be in (0,1):
  for fmt,width in enumerate('HIQ'):
   pack=lambda f,*a:struct.pack(('>' if be else '<')+f,*a)
   def fixture(order,case='valid'):
    sections=[]; maps=[]; words=[]
    for n,i in enumerate(order):
     name,sig,pc,*a=profiles[i]; b=pack('HH3sB7I',0,0,bytes.fromhex(sig),pc,*a)
     if n==0 and case=='badmeta': b=b[:-1]
     if n==0 and case=='badfeature': b=b[:28]+pack('I',0x80000000)+b[32:]
     t=4 if n==0 and case=='missingmeta' else 13
     sections.extend([(1,b'\x00\x00'),(t,b)])
     maps.append((n,2*n+1));maps.append((n,2*n+1 if n==0 and case=='duplicate_map' else 2*n+2))
     w=8|(1<<7)|(2<<27)
     if n==0 and case=='wrongmachine':w|=1<<10
     if n==0 and case=='wrongtype':w^=1<<7
     if n==0 and case=='wrongsystem':w|=2<<18
     if n==0 and case=='nocode':sections[-2]=(4,b'xx')
     words.append(w)
    n=len(order); hs=8+6*n+6*len(maps)+(2+2*struct.calcsize(width))*(1+len(sections))+1+2*n+3
    sh=lambda t,s,d:pack('H'+width*2,t,s,d)
    h=b'_GM'+bytes([1+128*be])+pack('HH',n,1)+b''.join(pack('IH',w,fmt) for w in words)+b''.join(pack('HI',*m) for m in maps)+sh(0,hs,hs)+b''.join(sh(t,len(b),len(b)) for t,b in sections)+b'\x03'+pack('H'*n,*([0]*n))+b'avr'
    blob=h+b''.join(b for t,b in sections)
    if case=='truncated':blob=blob[:-1]
    if case=='magic':blob=b'BAD'+blob[3:]
    if case=='version':blob=blob[:3]+bytes([2+128*be])+blob[4:]
    return blob
   def run(blob,i,expected=None):
    global good,bad
    path.write_bytes(blob); before=hashlib.sha256(blob).digest(); sig=profiles[i][1] if i>=0 else 'ffffff'
    r=subprocess.run(shlex.split(runner)+[exe,sig,str(path)],capture_output=True,text=True,timeout=30)
    assert hashlib.sha256(path.read_bytes()).digest()==before
    assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
    if expected is None: assert r.returncode==1 and 'SELECTED' not in r.stdout,(r.returncode,r.stdout,r.stderr);bad+=1
    else: assert r.returncode==0 and r.stdout.strip()==f'SELECTED {expected} {profiles[i][0]}',(r.returncode,r.stdout,r.stderr);good+=1
   for shift in range(5):
    order=list(range(shift,5))+list(range(shift))
    for i in range(5):run(fixture(order),i,order.index(i))
   for i in range(5):
    run(fixture([i]),i,0)
    run(fixture([j for j in range(5) if j!=i]),i)
    run(fixture([i,i]),i)
    for case in ('badmeta','badfeature','missingmeta','duplicate_map','wrongmachine','wrongtype','wrongsystem','nocode','truncated','magic','version'):run(fixture([i],case),i)
   run(fixture(list(range(5))),-1)
print(f'[PASS] {arch}: {good} AVR node selections, {bad} rejections')
