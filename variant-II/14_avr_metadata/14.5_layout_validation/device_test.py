import pathlib,struct,subprocess,sys,tempfile,shlex
arch,exe,runner=sys.argv[1:]; good=bad=0
profiles=[('ATtiny85','1e930b',12,8192,512,512,64,15,4,0),('ATmega8','1e9307',12,8192,1024,512,64,19,5,0),('ATmega328P','1e950f',14,32768,2048,1024,128,26,7,0),('ATmega128','1e9702',16,131072,4096,4096,256,35,7,1),('ATmega2560','1e9801',17,262144,8192,4096,256,57,7,3)]
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'metadata.bin'
 def run(blob,end,expected=None):
  global good,bad
  p.write_bytes(blob); r=subprocess.run(shlex.split(runner)+[exe,end,str(p)],capture_output=True,text=True)
  assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
  if expected is None: assert r.returncode==1,(r.returncode,r.stdout,r.stderr);bad+=1
  else: assert r.returncode==0 and r.stdout.strip()==expected,(r.returncode,r.stdout,r.stderr,expected);good+=1
 for end,o in [('le','<'),('be','>')]:
  for name,sig,pc,*a in profiles:
   for isa in range(8):
    for addr in range(4):
     vals=a[:5]+[isa,addr]; blob=struct.pack(o+'HH3sB7I',0,0,bytes.fromhex(sig),pc,*vals)
     run(blob,end,' '.join([name,sig,str(pc)]+list(map(str,vals))) if not (isa&~a[5] or addr&~a[6]) else None)
   base=struct.pack(o+'HH3sB7I',0,0,bytes.fromhex(sig),pc,*a)
   for off,fmt,val in [(4,'3s',b'\x1e\x99\x99'),(7,'B',pc+1)]+[(8+4*i,'I',v^1) for i,v in enumerate(a[:5])]+[(28,'I',0x80000000),(32,'I',0x80000000)]:
    b=bytearray(base);struct.pack_into(o+fmt,b,off,val);run(b,end)
   for n in range(36):run(base[:n],end)
   run(base+b'\0',end)
print(f'[PASS] {arch}: {good} device matches, {bad} device rejections')
