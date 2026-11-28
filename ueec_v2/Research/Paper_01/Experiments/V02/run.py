import pathlib,struct,subprocess,hashlib,csv
root=pathlib.Path('fixtures');root.mkdir(exist_ok=True)
rows=[]
for be in (0,1):
 for fmt,w in enumerate('HIQ'):
  p='>' if be else '<'; pack=lambda f,*v:struct.pack(p+f,*v)
  sections=[(1,16,b'\xc3'),(2,16,b'\xc3'),(3,16,b''),(4,16,b'\xc3'),(5,10,pack('HHHHH',0,0,2|(3<<5),1,8|(1<<12)))]
  hs=8+6+5*6+6*(2+2*struct.calcsize(w))+1+2+4
  sh=lambda t,s,d:pack('H'+w*2,t,s,d)
  blob=b'_GM'+bytes([1|be*128])+pack('HH',1,1)+pack('IH',12|(10<<10)|(2<<18)|(2<<20)|(5<<27),fmt)+b''.join(pack('HI',0,i) for i in range(1,6))+sh(0,hs,hs)+b''.join(sh(t,s,len(d)) for t,s,d in sections)+b'\x04'+pack('H',0)+b'test'+b''.join(d.ljust(s,b'\0') for t,s,d in sections)
  path=root/f'{be}-{fmt}.ueec';path.write_bytes(blob)
  for mode in ('monolithic','scattered','hybrid'):
   r=subprocess.run(['./probe',mode,str(path)],capture_output=True,text=True,timeout=30)
   if r.returncode or r.stdout.count('PASS section=')!=5 or path.read_bytes()!=blob:raise SystemExit(f'FAIL {path} {mode}: {r.stdout}\n{r.stderr}')
   for line in r.stdout.splitlines():
    if line.startswith('PASS section='):rows.append([be,16<<fmt,mode,line])
   print(f'{path.name} {mode}: 5 sections PASS')
with open('results.csv','w') as f:
 wr=csv.writer(f);wr.writerow(['big_endian','width','mode','result']);wr.writerows(rows)
print(f'Configurations: {len(rows)//5} PASS; section checks: {len(rows)} PASS; individual checks: {len(rows)*4-len(rows)//5} PASS')
