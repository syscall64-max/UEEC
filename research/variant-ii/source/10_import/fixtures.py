#!/usr/bin/env python3
from pathlib import Path
import struct, sys
ARCHS=('arm32','aarch64','loongarch64','ppc32','ppc64','riscv32','riscv64','sparc32','sparc64','x86','x86_64','s390x')
BIG={'ppc32','ppc64','sparc32','sparc64','s390x'}
SMALL={'arm32','ppc32','riscv32','sparc32','x86'}
def generate(out,arch,fmt,be):
    out.mkdir(parents=True,exist_ok=True)
    e='>' if be else '<'; native='>' if arch in BIG else '<'; w=2<<fmt; ptr=4 if arch in SMALL else 8
    p=lambda f,*x:struct.pack(e+f,*x)
    u=lambda x:x.to_bytes(w,'big' if be else 'little')
    def image(name,sections,library=False,shfmt=fmt):
        sw=2<<shfmt
        su=lambda x:x.to_bytes(sw,'big' if be else 'little')
        n=len(sections); word=(n<<27)|(2<<20)|(2<<18)|(ARCHS.index(arch)<<10)|4|(16 if library else 8)|int(arch in BIG)
        h=b'_GM'+bytes([2+128*be])+p('HHH',shfmt,1,1)+p('I',word)
        h+=b''.join(p('HI',0,i+1) for i in range(n)); names=bytes([len(name)])+p('H',0)+name.encode()
        size=len(h)+(n+1)*(2+2*sw)+len(names)
        h+=p('H',0)+su(size)*2
        h+=b''.join(p('H',t)+su(len(b))*2 for t,b in sections)
        return h+names+b''.join(b for _,b in sections)
    build=out.parent
    add=(build/'add.bin').read_bytes(); sub=(build/'sub.bin').read_bytes(); prog=(build/'program.bin').read_bytes()
    # Existing EXPORT order is format, design. The EXPORT width is independent
    # of both the container's SH width and the IMPORT width.
    def exports(symbols, design, efmt):
        ew=2<<efmt; eu=lambda x:x.to_bytes(ew,'big' if be else 'little')
        h=p('HHH',efmt,design,efmt)
        if design==0:
            groups={}
            for name,sec,off in symbols: groups.setdefault(len(name),[]).append((name,sec,off))
            h+=eu(len(groups)); payload=b''
            for length,group in sorted(groups.items()):
                h+=p('H',length)+eu(len(group)); group.sort()
                payload+=b''.join(bytes([sec])+eu(off) for name,sec,off in group)
                payload+=b''.join(name.encode() for name,sec,off in group)
            return h+payload
        h+=eu(len(symbols)); names=b''
        for name,sec,off in sorted(symbols):
            h+=p('H',sec|(len(name)<<5))+eu(off)+eu(len(names)); names+=name.encode()
        return h+names
    math=[('add',0,0),('sub',1,0)]; data=[('value1',0,0),('value2',0,4)]
    def libraries(directory,swap=False):
        directory.mkdir(parents=True,exist_ok=True)
        ef1=(fmt+1)%3 if swap else fmt; ef2=(fmt+2)%3 if swap else fmt
        (directory/'libmath.ueec').write_bytes(image('libmath',[(1,add),(1,sub),(6,exports(math,int(swap),ef1))],True,ef2 if swap else fmt))
        (directory/'libdata.ueec').write_bytes(image('libdata',[(2,struct.pack(native+'ii',20,22)),(6,exports(data,int(not swap),ef2))],True,ef1 if swap else fmt))
    libraries(out)
    names=bytearray(); offsets={}
    for name in ('libmath.ueec','libdata.ueec','add','sub','value1','value2'):
        offsets[name]=len(names); names+=name.encode()+b'\0'
    ext=1<<(8*w-1)
    imp=p('HH',0,fmt)+u(2)+u(ext|offsets['libmath.ueec'])+u(ext|offsets['libdata.ueec'])+u(2)+u(2)
    for i,name in enumerate(('add','sub','value1','value2')): imp+=u(1)+u(i*ptr)+u(offsets[name])
    def program(payload): return image('program',[(1,prog),(2,bytes(4*ptr)),(7,payload)])
    original=program(imp+names)
    (out/'program.ueec').write_bytes(original)
    swapped=out/'swapped'; libraries(swapped,True)
    (swapped/'program.ueec').write_bytes(original)
    bad=bytearray(imp); start=4+5*w
    bad[start:start+w]=u(99); (out/'bad-section.ueec').write_bytes(program(bad+names))
    bad=bytearray(imp); bad[start+w:start+2*w]=u(4*ptr); (out/'bad-offset.ueec').write_bytes(program(bad+names))
    bad=bytearray(imp); bad[start+2*w:start+3*w]=u(len(names)); (out/'bad-name-offset.ueec').write_bytes(program(bad+names))
    (out/'missing-symbol.ueec').write_bytes(program(imp+names.replace(b'add\0',b'xyz\0')))
    (out/'missing-library.ueec').write_bytes(program(imp+names.replace(b'libmath.ueec',b'notmath.ueec')))
    (out/'truncated.ueec').write_bytes(program(imp+names)[:-1])
if __name__=='__main__':
    base=Path(sys.argv[1]); arch=sys.argv[2]
    for fmt in range(3):
        for be in (False,True): generate(base/f'f{16<<fmt}-{"be" if be else "le"}',arch,fmt,be)
