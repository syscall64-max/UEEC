import pathlib,struct,subprocess,sys,tempfile,shlex
arch,exe,runner=sys.argv[1:]; good=bad=0
profiles=[('ATtiny85','1e930b',12,8192,512,512,64,15,4,0,96),('ATmega8','1e9307',12,8192,1024,512,64,19,5,0,96),('ATmega328P','1e950f',14,32768,2048,1024,128,26,7,0,256),('ATmega128','1e9702',16,131072,4096,4096,256,35,7,1,256),('ATmega2560','1e9801',17,262144,8192,4096,256,57,7,3,512)]
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'metadata.bin'
 def run(end,args,expected=None):
  global good,bad
  r=subprocess.run(shlex.split(runner)+[exe,end,str(p)]+list(map(str,args)),capture_output=True,text=True)
  assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
  if expected is None: assert r.returncode==1,(args,r.returncode,r.stdout,r.stderr);bad+=1
  else: assert r.returncode==0 and r.stdout.strip()==expected,(args,r.returncode,r.stdout,r.stderr,expected);good+=1
 for end,o in [('le','<'),('be','>')]:
  for name,sig,pc,f,s,e,page,irq,isa,addr,base in profiles:
   p.write_bytes(struct.pack(o+'HH3sB7I',0,0,bytes.fromhex(sig),pc,f,s,e,page,irq,isa,addr))
   run(end,['info'],f'{name} {base} {base+s} {f//2} {f//page} {e}')
   for space,lo,hi,scale in [('flash',0,f,1),('word',0,f//2,2),('page',0,f//page,page),('sram',base,base+s,1),('eeprom',0,e,1),('register',0,32,1),('io',32,96,1),('extended-io',96,base,1)]:
    if hi>lo:
     for a,n in [(lo,1),(hi-1,1),(lo,hi-lo)]:run(end,[space,a,n],f'{a*scale} {n*scale}')
    for a,n in [(hi,1),(lo,0),(lo,hi-lo+1),(2**64-1,1),(lo,2**64-1),(2**64,1),('-1',1)]:run(end,[space,a,n])
    if lo:run(end,[space,lo-1,1])
   p.write_bytes(b'bad');run(end,['info'])
print(f'[PASS] {arch}: {good} memory cases, {bad} memory rejections')
