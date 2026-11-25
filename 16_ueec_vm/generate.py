"""UEEC v1 fixture writer: existing _GM container, no legacy UEF wrapper.

Metadata endian and SH16/32/64 are independent of the VM ISA's LE bytecode.
"""
import argparse
from pathlib import Path
import struct
from asm_subset import assemble

ROOT=Path(__file__).resolve().parent
REG, IDATA, STACK = 0,1,2

def instruction(cmd, fmt, *values):
    if not (cmd==0 and fmt==0) and fmt not in (1,2,3):
        raise ValueError('Supported operand formats are 16/32/64; code 0 is reserved')
    width=1<<fmt
    b=struct.pack('<H',cmd|(fmt<<11))
    if cmd==0:
        assert len(values)==1
        return b+struct.pack('<H',values[0])
    for value in values:
        b+=(value&((1<<(8*width))-1)).to_bytes(width,'little')
    return b+b'\0'*(len(b)%2)

def index(space, value, fmt):
    if fmt not in (1,2,3): raise ValueError('Unsupported index format')
    assert 0<=value<(1<<((1<<fmt)*8-2))
    return (space<<((1<<fmt)*8-2))|value

def hello_code():
    # Read the shared seed (1234); write a message; exit(0).
    return b''.join([
        instruction(1,1,index(REG,4,1),index(IDATA,0,1)),
        instruction(2,1,index(REG,1,1),1),
        instruction(2,1,index(REG,2,1),len(b'Hello from UEEC VM!\n')),
        instruction(0,0,1),
        instruction(2,1,index(REG,1,1),0),
        instruction(0,0,0),
    ])

def shared_data():
    return struct.pack('<H',1234)+b''.join(struct.pack('<H',v) for v in b'Hello from UEEC VM!\n')

def legacy_parts(blob):
    # This helper is ONLY for importing the supplied old fixture into tests.
    assert blob[:2]==b'ue'
    header_size=struct.unpack_from('<H',blob,8)[0]>>4
    code_size=struct.unpack_from('<H',blob,10)[0]>>4
    assert header_size+code_size<=len(blob)
    return blob[header_size:header_size+code_size],blob[header_size+code_size:]

def container(nodes, bodies, fmt=0, endian='little', granule=1):
    """nodes=[(name, [physical section indexes], optional node word)].
    bodies excludes HEADER and contains (section_type, bytes) entries.
    """
    e='<' if endian=='little' else '>'
    width=(2,4,8)[fmt]; count=len(nodes)
    pack=lambda n,v: int(v).to_bytes(n,endian)
    names=[n[0].encode('utf-8') for n in nodes]
    assert 0<count<=65535 and all(0<len(n)<=255 for n in names)
    align=lambda n: ((n+granule-1)//granule)*granule
    headers=[]; maps=[]
    for i,node in enumerate(nodes):
        name,sections=node[:2]
        # VM=2, UEEC=0; portable hosted service ABI uses BARE_METAL/system=0.
        # Node encode=0 defines LE bytecode/cells, independent of file endian.
        word=((1<<2)|(1<<3)|(1<<5)|(2<<7)|(len(sections)<<27))
        if len(node)>2:word=node[2]
        headers.append(struct.pack(e+'IH',word,fmt))
        maps.extend(struct.pack(e+'HI',i,s) for s in sections)
    prefix=b'_GM'+bytes([1|(0 if endian=='little' else 128)])+struct.pack(e+'HH',count,count)
    prefix+=b''.join(headers)+b''.join(maps)
    suffix=bytes(map(len,names))+b''.join(pack(2,i) for i in range(count))+b''.join(names)
    meta_size=len(prefix)+(1+len(bodies))*(2+2*width)+len(suffix)
    entries=[(0,align(meta_size),meta_size)]+[(t,align(len(b)),len(b)) for t,b in bodies]
    table=b''.join(pack(2,t)+pack(width,stored)+pack(width,used) for t,stored,used in entries)
    header=prefix+table+suffix
    return header+b'\0'*(align(meta_size)-meta_size)+b''.join(b+b'\0'*(align(len(b))-len(b)) for _,b in bodies)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--format',choices=['sh16','sh32','sh64'],default='sh16')
    ap.add_argument('--endian',choices=['little','big'],default='little')
    ap.add_argument('--granule',type=int,default=1)
    ap.add_argument('--output',type=Path,default=ROOT/'build/programs.ueec')
    a=ap.parse_args()
    if a.granule<=0:ap.error('granule must be positive')
    stack=assemble(ROOT/'examples/stack16.asm')
    result=container([('hello',[1,3]),('stack',[2,3])],
                     [(1,hello_code()),(1,stack),(2,shared_data())],
                     ['sh16','sh32','sh64'].index(a.format),a.endian,a.granule)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_bytes(result)
    print(f'{a.output.name}: {len(result)} bytes, 2 VM program nodes, shared IDATA')

if __name__=='__main__':main()
