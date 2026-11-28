import csv, hashlib, os, re, subprocess, platform
from pathlib import Path
rows=[]
env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
assert os.sysconf('SC_PAGE_SIZE')==4096, 'This fixture suite assumes 4096-byte pages'
def test(f,tag,node,n,w,expected):
 before=hashlib.sha256(f.read_bytes()).digest()
 for mode in ('monolithic','scattered','hybrid'):
  q=subprocess.run(['build/'+tag,'--node',str(node),'--load-mode',mode,str(f)],capture_output=True,text=True,timeout=10,env=env)
  m=re.search(r'E11_OK (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)',q.stdout)
  assert m and q.returncode==expected and re.search(r'actual mode:\s+'+mode+r'\b',q.stdout),(f,node,mode,q.returncode,q.stdout,q.stderr)
  idx,count,width,fh,rh,ao,ab,nl=map(int,m.groups())
  assert (idx,count,width)==(node,n,w)
  assert ao==(14+n*(2+2*(w//8))+7)//8*8 and ab==8*n and rh==ao+ab+3+nl
  assert before==hashlib.sha256(f.read_bytes()).digest()
  rows.append([f.name,tag,node,n,w,mode,fh,rh,ao,ab,nl,expected,'PASS'])
for n in (2,4,8,16,24,32):
 for case in ('base','control'):
  for w in (16,32,64):test(Path(f'fixtures/{n}_{case}_sh{w}.ueec'),'single',0,n,w,(42+(case=='control') if n==2 else (n-3)*(n-2)//2+(case=='control'))%256)
 print(f'single sections={n}: 18 PASS')
for n in (2,8,64):
 for w in (16,32,64):
  for i in range(n):test(Path(f'fixtures/multi_{n}_16_sh{w}_shared.ueec'),'multi',i,4,w,40+i)
 print(f'multi nodes={n}: {n*9} PASS')
with open('results.csv','w') as f:
 wr=csv.writer(f);wr.writerow(['file','fixture_family','node','sections','width','mode','file_metadata_bytes','runtime_header_bytes','address_offset','address_table_bytes','name_bytes','exit','status']);wr.writerows(rows)
Path('environment.txt').write_text(platform.platform()+'\npage_size=4096\n')
print(f'Runtime header + execution checks: {len(rows)}/{len(rows)} PASS')
