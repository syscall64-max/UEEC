import struct as s, subprocess as sp, csv, statistics as st, sys, hashlib, pathlib, math
R=int(sys.argv[1]) if len(sys.argv)>1 else 20
assert R>0
F=pathlib.Path('fixtures');F.mkdir(exist_ok=True)
def fixture(parts,e):
 p=lambda f,*v:s.pack(e+f,*v); sh=1; w='I'; h=8+6+6*len(parts)+10*(1+len(parts))+4
 b=b'_GM'+bytes([1+(128 if e=='>' else 0)])+p('HHIH',1,1,12|(10<<10)|(2<<18)|(2<<20)|(len(parts)<<27),sh)
 b+=b''.join(p('HI',0,i+1) for i in range(len(parts)))+p('HII',0,h,h)+b''.join(p('HII',t,len(v),len(v)) for t,v in parts)+b'\x01\0\0x'
 assert len(b)==h;return b+b''.join(v for t,v in parts)
def tables(n,f,d,e):
 p=lambda fmt,*v:s.pack(e+fmt,*v);w='HIQ'[f];names=[f's{i:05d}'.encode() for i in range(n)]
 exp=p('HHH'+w,f,d,f,1 if d==0 else n)
 if d==0:exp+=p('H'+w,6,n)+b''.join(p('B'+w,1,i) for i in range(n))+b''.join(names)
 elif d==1:exp+=b''.join(p('H'+w*2,1|(6<<5),i,6*i) for i in range(n))+b''.join(names)
 else:exp+=b''.join(p('B'+w*2,1,i,i+1) for i in range(n))
 imp=p('HH'+w,f,0,1)+p('H',3)+b'lib'+p(w,n)
 for i,name in enumerate(names):imp+=p('B'+w*2,int(d==2),1,8*i)+(p(w,i+1) if d==2 else p('H',6)+name)
 return exp,imp
fields=['symbols','width','design','endian','mode','operation','sample','ns'];groups={};count=0
with open('samples.csv','w') as out:
 cw=csv.writer(out);cw.writerow(fields)
 for n in (1,16,64,256,1024,4096):
  for f in range(3):
   for d in range(3):
    for e in ('<','>'):
     exp,imp=tables(n,f,d,e);a=F/'app.ueec';b=F/'lib.ueec';a.write_bytes(fixture([(1,b'\xc3'),(2,bytes(n*8)),(7,imp)],e));b.write_bytes(fixture([(1,b'\xc3'),(4,bytes([42])*n),(6,exp)],e)); hashes=[hashlib.sha256(x.read_bytes()).digest() for x in (a,b)]
     for mode in ('monolithic','scattered','hybrid'):
      r=sp.run(['./bench',str(a),str(b),str(n),str(d),str(R),mode],stdout=sp.DEVNULL,stderr=sp.PIPE,text=True,timeout=180)
      if r.returncode:raise RuntimeError((n,f,d,e,mode,r.returncode,r.stderr[-2000:]))
      assert hashes==[hashlib.sha256(x.read_bytes()).digest() for x in (a,b)]
      for line in r.stderr.splitlines():
       op,i,ns=line.split(',');key=(n,16<<f,d,e,mode,op);groups.setdefault(key,[]).append(int(ns));cw.writerow([*key,i,ns])
      count+=1
  print(f'symbols={n}: 54 configurations PASS',flush=True)
with open('summary.csv','w') as out:
 cw=csv.writer(out);cw.writerow(fields[:6]+['samples','min_ns','median_ns','mean_ns','p95_ns','max_ns'])
 for key,v in groups.items():
  assert len(v)==R;cw.writerow([*key,len(v),min(v),st.median(v),st.mean(v),sorted(v)[math.ceil(.95*len(v))-1],max(v)])
print(f'Configurations: {count} PASS; timing observations: {count*5*R}')
