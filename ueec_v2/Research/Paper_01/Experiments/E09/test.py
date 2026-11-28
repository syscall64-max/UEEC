import struct as s, csv, mmap, ctypes as c, platform
from export_ref import enc,dec
assert platform.system()=='Linux' and platform.machine()=='x86_64'
Q={2:'H',4:'I',8:'Q'}
def encode(groups,w,e):
 p=lambda f,*a:s.pack(e+f,*a); q=Q[w]
 name=lambda k:p('H',len(k))+k
 return p('HH'+q,{2:0,4:1,8:2}[w],0,len(groups))+b''.join(name(lib)+p(q,len(entries))+b''.join(p('B'+q+q,kind,sec,off)+(p(q,key) if kind else name(key)) for kind,sec,off,key in entries) for lib,entries in groups)
def decode(b,e):
 pos=0
 def get(f):
  nonlocal pos
  a=s.unpack_from(e+f,b,pos);pos+=s.calcsize(e+f);return a
 def name():
  nonlocal pos
  n,=get('H');k=b[pos:pos+n];pos+=n
  if not n or len(k)!=n or b'\0' in k:raise ValueError('name')
  return k
 f,d=get('HH'); assert f<3 and d==0
 q=Q[2<<f];n,=get(q);assert 0<n<=len(b)
 groups=[]
 for _ in range(n):
  lib=name();k,=get(q);assert 0<k<=len(b);a=[]
  for _ in range(k):
   kind,sec,off=get('B'+q+q);assert kind in (0,1)
   key=get(q)[0] if kind else name();a.append((kind,sec,off,key))
  groups.append((lib,a))
 assert pos==len(b);return groups

def bind(groups,registry,slots):
 pending={}
 for lib,entries in groups:
  if lib not in registry:raise ValueError('library')
  for kind,sec,off,key in entries:
   if sec!=0 or off%8 or off+8>len(slots) or off in pending:raise ValueError('slot')
   if (kind,key) not in registry[lib]:raise ValueError('symbol')
   pending[off]=registry[lib][kind,key]
 for off,addr in pending.items():s.pack_into('<Q',slots,off,addr)
 return pending
counts=[1,16,64,256,1024,4096]; rows=[]; stats=dict(tables=0,bindings=0,native_calls=0,rejections=0)
for n in counts:
 code=b''.join(b'\xb8'+s.pack('<I',i+1)+b'\xc3' for i in range(n));mem=mmap.mmap(-1,len(code),prot=3);mem.write(code);addr=c.addressof(c.c_char.from_buffer(mem));libc=c.CDLL(None,use_errno=True);libc.mprotect.argtypes=[c.c_void_p,c.c_size_t,c.c_int];assert libc.mprotect(addr,len(code),5)==0
 for e in ['<','>']:
  for w in [2,4,8]:
   for design in [0,1,2]:
    kind=int(design==2); keys=[i+1 if kind else f's{i:05d}'.encode() for i in range(n)]
    exports=dec(enc([(k,0,i*6) for i,k in enumerate(keys)],w,design,e),e);reg={b'libalpha':{(kind,k):addr+o for k,sec,o in exports}}
    entries=[(kind,0,i*8,k) for i,k in enumerate(keys)];groups=[(b'libalpha',entries)];blob=encode(groups,w,e);assert decode(blob,e)==groups;stats['tables']+=1
    slots=bytearray(n*8);bind(decode(blob,e),reg,slots)
    for i in range(n):assert s.unpack_from('<Q',slots,i*8)[0]==addr+i*6;stats['bindings']+=1
    for i in sorted({0,n//2,n-1}):assert c.CFUNCTYPE(c.c_uint32)(s.unpack_from('<Q',slots,i*8)[0])()==i+1;stats['native_calls']+=1
    bad=[[(b'absent',entries)],[(b'libalpha',[(kind,0,0,0 if kind else b'absent')])],[(b'libalpha',[(kind,0,1,keys[0])])],[(b'libalpha',[(kind,0,n*8,keys[0])])],[(b'libalpha',[entries[0],entries[0]])],[(b'libalpha',[(kind,1,0,keys[0])])]]
    for g in bad:
     a=bytearray(n*8)
     try:bind(decode(encode(g,w,e),e),reg,a)
     except ValueError:assert not any(a);stats['rejections']+=1
     else:raise AssertionError('accepted invalid binding')
    for b in [blob[:-1],blob+b'\0']:
     try:decode(b,e)
     except (ValueError,AssertionError,s.error):stats['rejections']+=1
     else:raise AssertionError('accepted malformed')
    if e=='<':rows.append([n,w*8,design,len(blob),24*n+24*(n+1)+1+sum(len(k)+1 for k in keys) if not kind else '',20+20+8*(n+1)*2+len(b'libalpha')+1+sum(2+len(k)+1+(len(k)+3)%2 for k in keys) if not kind else ''])
 mem.close()
with open('results.csv','w',newline='') as f:
 out=csv.writer(f);out.writerow(['symbols','import_width','export_design','ueec_import_bytes','elf_rela_dynsym_dynstr_estimate','pe_import_estimate']);out.writerows(rows)
for k,v in stats.items():print(f'{k}: {v} PASS')
print('total:',sum(stats.values()),'PASS')
for r in rows:
 if r[0]==4096:print(*r,sep=',')
