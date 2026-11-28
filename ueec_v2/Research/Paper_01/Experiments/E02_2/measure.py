import csv,struct
from pathlib import Path
rows=[]
for p in sorted(Path('fixtures').glob('*.elf')):
 b=p.read_bytes(); h=struct.unpack_from('<16sHHIQQQIHHHHHH',b)
 phoff,shoff=h[5:7]; eh,phsize,phnum,shsize,shnum=h[8:13]
 entry=h[4]; code=None
 for i in range(phnum):
  t,flags,off,va,pa,fs,ms,al=struct.unpack_from('<IIQQQQQQ',b,phoff+i*phsize)
  if t==1 and va<=entry<va+fs and flags&1: code=b[off+entry-va:off+entry-va+9]
 assert code==Path('fixtures/linux-code.bin').read_bytes()
 headers=eh+phsize*phnum+shsize*shnum
 rows.append([p.name,eh,phsize*phnum,shsize*shnum,0,headers,len(code),len(b)-headers-len(code),len(b)])
for p in sorted(Path('fixtures').glob('*.ueec')):
 b=p.read_bytes(); fmt=struct.unpack_from('<H',b,12)[0]; sh=2+2*(2<<fmt); headers=8+6+6+2*sh+7
 assert b[headers:]==Path('fixtures/linux-code.bin').read_bytes()
 rows.append([p.name,8,0,2*sh,19,headers,9,len(b)-headers-9,len(b)])
with open('results.csv','w') as f:
 w=csv.writer(f); w.writerow(['file','file_header','program_headers','section_headers','node_mapping_names','total_headers','code','remaining_bytes','file_size']); w.writerows(rows)
print(Path('results.csv').read_text(),end='')
