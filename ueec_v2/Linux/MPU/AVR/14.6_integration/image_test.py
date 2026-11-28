import pathlib,struct,subprocess,sys,tempfile,shlex,hashlib
arch,exe,runner=sys.argv[1:];good=bad=0
profiles=[('ATtiny85','1e930b',12,8192,512,512,64,15,4,0),('ATmega8','1e9307',12,8192,1024,512,64,19,5,0),('ATmega328P','1e950f',14,32768,2048,1024,128,26,7,0),('ATmega128','1e9702',16,131072,4096,4096,256,35,7,1),('ATmega2560','1e9801',17,262144,8192,4096,256,57,7,3)]
with tempfile.TemporaryDirectory() as tmp:
 path=pathlib.Path(tmp)/'layout.ueec'
 out=pathlib.Path(tmp)/'out';out.mkdir()
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
    def run(parts,expected=None,collision=False):
     global good,bad
     blob=fixture(parts);path.write_bytes(blob)
     if collision:(out/'sram.bin').write_bytes(b'keep')
     r=subprocess.run(shlex.split(runner)+[exe,sig,str(path),str(out)],capture_output=True,text=True,timeout=30)
     assert path.read_bytes()==blob
     assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
     if expected is None:
      assert r.returncode==(2 if collision else 1),(r.returncode,r.stdout,r.stderr)
      assert sorted(p.name for p in out.iterdir())==(['sram.bin'] if collision else [])
      if collision:assert (out/'sram.bin').read_bytes()==b'keep';(out/'sram.bin').unlink()
      bad+=1;return
     f,ram,entry=expected; base={'ATmega2560':512,'ATmega128':256,'ATmega328P':256}.get(name,96)
     assert r.returncode==0 and f'IMAGE flash={flash} sram={sram} base={base} sp={base+sram-1} entry={entry} used={len(f)}' in r.stdout,(r.returncode,r.stdout,r.stderr)
     assert (out/'flash.bin').read_bytes()==f+b'\xff'*(flash-len(f))
     assert (out/'sram.bin').read_bytes()==ram+bytes(sram-len(ram))
     decoded=bytearray();upper=0;eof=False
     for line in (out/'flash.hex').read_text().splitlines():
      assert not eof and line.startswith(':');raw=bytes.fromhex(line[1:]);assert len(raw)==raw[0]+5 and sum(raw)%256==0
      count,addr,kind=raw[0],int.from_bytes(raw[1:3],'big'),raw[3];payload=raw[4:-1]
      if kind==4:assert count==2 and addr==0;upper=int.from_bytes(payload,'big')<<16
      elif kind==0:assert upper+addr==len(decoded) and addr+count<=65536;decoded.extend(payload)
      elif kind==1:assert count==0 and addr==0;eof=True
      else:raise AssertionError(kind)
     assert eof and decoded==f
     for p in out.iterdir():p.unlink()
     good+=1
    code=(1,2,b'\x12\x34')
    run([code],(b'\x12\x34',b'',0))
    run([(4,3,b'abc'),code,(2,5,b'xy'),(3,7,b'')],(b'abc\xff\x12\x34xy',b'xy'+bytes(10),4))
    run([code,(10,64,b''),(2,sram-64,b'z')],(b'\x12\x34z',b'z',0))
    run([code,(3,8,b''),(2,4,b'\x81\x42')],(b'\x12\x34\x81\x42',bytes(8)+b'\x81\x42',0))
    chunk=min(flash,maxsize-1);chunk-=chunk%2
    parts=[];left=flash
    while left:
     sz=min(left,chunk);parts.append((1,sz,b'\xa5\x5a'*(sz//2)));left-=sz
    run(parts,(b'\xa5\x5a'*(flash//2),b'',0))
    run(parts+[(4,1,b'x')]);run([code,(3,sram-31,b'')]);run([(1,3,b'abc')]);run([code],collision=True)
print(f'[PASS] {arch}: {good} image cases, {bad} image rejections')
