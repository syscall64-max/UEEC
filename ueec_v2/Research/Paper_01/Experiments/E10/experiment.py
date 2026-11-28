import csv, os, subprocess, re, platform, hashlib
from pathlib import Path
rows=[]; sections=[]; page=os.sysconf('SC_PAGE_SIZE'); env=os.environ.copy();env.pop('UEEC_EXPERIMENT_NOBITS',None)
for n in (2,4,8,16,24,32):
 for case in ('base','control'):
  expected=(42+(case=='control') if n==2 else (n-3)*(n-2)//2+(case=='control'))%256
  for width in (16,32,64):
   f=Path(f'fixtures/{n}_{case}_sh{width}.ueec'); before=hashlib.sha256(f.read_bytes()).digest(); baseline=None
   for mode,regions in [('monolithic',1),('scattered',n),('hybrid',2)]:
    vals=[]
    for run in (True,False):
     q=subprocess.run(['build/loader','--load-mode',mode]+([] if run else ['--no-run'])+[str(f)],capture_output=True,text=True,env=env,timeout=10)
     assert q.returncode==(expected if run else 0),(f,mode,run,q.stdout,q.stderr,q.returncode)
     m=re.search(r'E10_IMAGE (\d+) (\d+) (\d+) (\d+)',q.stdout); assert m,q.stdout
     a,b,c,d=map(int,m.groups()); assert (a,b,c)==(n,regions,n*page);assert 'actual mode:' in q.stdout and re.search(r'actual mode:\s+'+mode,q.stdout);assert ('E10_RELEASE_OK' in q.stdout)==(not run)
     ss=re.findall(r'E10_SECTION (\d+) (\d+) (\d+) (\d+) (\S+)',q.stdout);assert len(ss)==n
     vals.append((c,d,ss)); rows.append([n,case,width,mode,'execute' if run else 'release',b,c,d,c-d,q.returncode,'PASS'])
     if not run: sections.extend([[n,case,width,mode,*v] for v in ss])
    assert vals[0]==vals[1]
    if baseline is None:baseline=vals[0]
    else:assert baseline==vals[0]
   assert before==hashlib.sha256(f.read_bytes()).digest()
 print(f'sections={n}: 18 native + 18 release PASS; bytes={n*page}; regions=1/{n}/2')
for fn,head,data in [('results.csv',['sections','case','width','mode','test','allocation_regions','allocated_bytes','runtime_data_bytes','unused_capacity_bytes','exit','status'],rows),('sections.csv',['sections','case','width','mode','index','type','allocated_bytes','data_bytes','permissions'],sections)]:
 with open(fn,'w') as f:w=csv.writer(f);w.writerow(head);w.writerows(data)
print(f'Native executions: {sum(r[4]=="execute" for r in rows)} PASS');print(f'Cleanup checks: {sum(r[4]=="release" for r in rows)} PASS');print(f'Total: {len(rows)}/{len(rows)} PASS')
Path('environment.txt').write_text(platform.platform()+f'\npage_size={page}\n')
