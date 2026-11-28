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
