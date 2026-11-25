"""Behavioral/compatibility regression checks, including malformed guest input."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

from generate import ROOT, container, hello_code, shared_data, instruction, index, legacy_parts
from asm_subset import assemble

EXPECTED='r4=111 r5=222 r6=85 r7=333 r8=111 r9=122 r10=305419896 sp=0 bp=0 exit=0'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--binary',default='build/ueec-vm');a=ap.parse_args()
    binary=str(Path(a.binary).resolve()); info=str(ROOT/'build/ueec-info-original')
    records=[]
    def run(label,args,rc=0,contains=(),absent=()):
        p=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=8)
        text=p.stdout+p.stderr
        assert p.returncode==rc,(label,p.returncode,rc,text)
        for x in contains:assert x in text,(label,x,text)
        for x in absent:assert x not in text,(label,x,text)
        assert 'AddressSanitizer' not in text and 'runtime error:' not in text,(label,text)
        records.append({'check':label,'returncode':p.returncode})
        return text
    stack_blob=assemble(ROOT/'reference/final_stack.asm');legacy_stack,_=legacy_parts(stack_blob)
    stack=assemble(ROOT/'examples/stack16.asm')
    recursion_blob=assemble(ROOT/'reference/recursive_overflow.asm');recursive,_=legacy_parts(recursion_blob)
    with tempfile.TemporaryDirectory(prefix='ueec-vm-') as tmp:
        d=Path(tmp)
        # Original and converted programs must produce the same observable result.
        oldbin=d/'old-vm'
        run('compile original C interpreter',shlex.split(os.environ.get('CC','cc'))+
            ['-std=c11','-O2','-Wall','-Wextra',ROOT/'reference/uef_vm_original.c','-o',oldbin])
        (d/'old.uef').write_bytes(stack_blob)
        run('original stack fixture',[oldbin,d/'old.uef'],contains=(EXPECTED,))
        (d/'recursive.uef').write_bytes(recursion_blob)
        run('original recursion bound',[oldbin,d/'recursive.uef'],rc=1,contains=('depth=512 limit=512',))

        # Same two programs under all descriptor widths / metadata endian /
        # compact and page-granulated file layouts / allocation policies.
        for fmt in range(3):
            for endian in ('little','big'):
                for granule in (1,4096):
                    tag=f'SH{16<<fmt}/{endian}/g{granule}'
                    blob=container([('hello',[1,3]),('stack',[2,3])],
                        [(1,hello_code()),(1,stack),(2,shared_data())],fmt,endian,granule)
                    f=d/'matrix.ueec';f.write_bytes(blob)
                    run('original UEEC reader '+tag,[info,f],contains=('hello','stack'))
                    for node,expected in [('hello','r4=1234'),('stack',EXPECTED)]:
                        for mode in ('auto','monolithic','scattered','hybrid'):
                            run(f'{tag}/{node}/{mode}',[binary,'--node',node,'--load-mode',mode,f],
                                contains=(expected,'VM context released: frames=0',
                                    'mapped regions:      '+str({'auto':1,'monolithic':1,'scattered':3,'hybrid':2}[mode])),
                                absent=() if node=='hello' else ('Hello from UEEC VM!',))

        f=d/'good.ueec'
        good=container([('hello',[1,3]),('stack',[2,3])],[(1,hello_code()),(1,stack),(2,shared_data())])
        f.write_bytes(good)
        run('correct VM target labels',[ROOT/'build/ueec-info',f],contains=('VM/UEEC-VM','bare-metal/none'),absent=('VM/arm32','Android'))
        run('list both VM nodes',[binary,'--list',f],contains=(
            '0 hello machine_type=2 machine=0 compatible=yes',
            '1 stack machine_type=2 machine=0 compatible=yes'))
        for n,want in [(0,'r4=1234'),(1,EXPECTED)]:
            run('select index '+str(n),[binary,'--node',n,f],contains=(want,))
        run('default selects first compatible program',[binary,f],contains=('0 (hello)','Hello from UEEC VM!'))
        run('load only',[binary,'--node','stack','--no-run',f],absent=('RESULT','Hello from UEEC VM!'))
        run('trace original CALL/RET',[binary,'--node','stack','--trace',f],contains=('CALL relative=','RET cleanup=','physical_blocks=1'))
        run('unknown name',[binary,'--node','missing',f],rc=1)
        run('out of range node',[binary,'--node','2',f],rc=1)
        run('invalid step option',[binary,'--max-steps','-1',f],rc=2)

        def reject(label,blob,contains=(),options=()):
            p=d/'bad.ueec';p.write_bytes(blob)
            run(label,[binary,*options,p],rc=1,contains=contains)
        def guest(code,idata=b'\0\0',word=None):
            n=('bad',[1,2]) if word is None else ('bad',[1,2],word)
            return container([n],[(1,code),(2,idata)])
        exit0=instruction(0,0,0)
        word=(1<<2)|(1<<3)|(1<<5)|(2<<7)|(2<<27)
        reject('old UEF wrapper is rejected',stack_blob)
        reject('truncated file',good[:-1])
        reject('bad version',good[:3]+b'\x02'+good[4:])
        reject('wrong machine',guest(exit0,word=word|(1<<10)))
        reject('CPU node never run as bytecode',guest(exit0,word=word&~(7<<7)))
        reject('opposite bytecode endian',guest(exit0,word=word|1))
        reject('library is not a program',guest(exit0,word=word|(1<<4)))
        reject('unknown opcode',guest(struct.pack('<H',8)))
        reject('reserved 128-bit operand width',guest(struct.pack('<H',2|(4<<11))))
        reject('old FORMAT8 CODE rejected',guest(legacy_stack))
        for cmd in range(1,8):
            reject('format zero reserved for opcode '+str(cmd),guest(struct.pack('<H',cmd)+b'\0'*4+exit0))
        for fmt,value in [(1,0xBEEF),(2,0xDEADBEEF),(3,0xFEDCBA9876543210)]:
            code=b''.join([
                instruction(2,fmt,index(0,4,fmt),value),
                instruction(1,fmt,index(1,0,fmt),index(0,4,fmt)),
                instruction(1,fmt,index(0,5,fmt),index(1,0,fmt)),
                instruction(3,fmt,index(0,4,fmt)),
                instruction(5,fmt,index(0,6,fmt)),
                instruction(4,fmt,value),
                instruction(6,fmt,2), exit0,
                instruction(1,fmt,index(0,10,fmt),index(2,0,fmt)),
                instruction(7,fmt,(1<<fmt)//2),
            ])
            p=d/'width.ueec';p.write_bytes(guest(code,b'\0'*8))
            run('all operand instructions width '+str(8<<fmt),[binary,p],contains=(
                f'r4={value} r5={value} r6={value}',f'r10={value} sp=0 bp=0 exit=0'))
        reject('truncated operand',guest(struct.pack('<H',2|(3<<11))+b'\0\0'))
        reject('odd CODE length',guest(exit0+b'\0'))
        reject('odd IDATA length',guest(exit0,b'\0'))
        reject('invalid register',guest(instruction(2,1,index(0,16,1),1)+exit0))
        reject('huge IDATA index',guest(instruction(1,3,index(0,4,3),index(1,(1<<62)-1,3))+exit0))
        reject('huge STACK index',guest(instruction(1,3,index(0,4,3),index(2,(1<<62)-1,3))+exit0))
        reject('HEAP not implemented',guest(instruction(1,1,index(0,4,1),index(3,0,1))+exit0))
        reject('POP underflow',guest(instruction(5,1,index(0,4,1))+exit0))
        reject('RET without CALL',guest(instruction(7,1,0)+exit0))
        reject('CALL into operand',guest(instruction(6,1,1)+instruction(2,1,index(0,1,1),0)+exit0))
        reject('CALL signed overflow high',guest(instruction(6,3,(1<<63)-1)+exit0))
        reject('CALL signed overflow low',guest(instruction(6,3,-(1<<63))+exit0))
        reject('unknown syscall',guest(instruction(0,0,65535)))
        reject('IDATA output bounds',guest(instruction(2,1,index(0,2,1),2)+instruction(0,0,1)+exit0))
        reject('missing explicit exit',guest(instruction(2,1,index(0,4,1),12)),contains=('CODE ended without sys_exit',))
        reject('bounded recursion',guest(recursive),contains=('depth=512 limit=512','VM context released: frames=0'))
        reject('instruction budget',guest(instruction(6,1,-2)),contains=('instruction limit reached',),options=('--max-steps','20'))
        overflow=instruction(4,3,42)*513+exit0
        reject('data stack overflow',guest(overflow),contains=('PUSH immediate failed',))
        # CALL with no arguments followed by RET cleanup=1.
        reject('RET argument cleanup bound',guest(instruction(6,1,2)+exit0+instruction(7,1,1)))
        duplicate=bytearray(good)
        # Node 0's second mapping aliases its own CODE instead of shared IDATA.
        struct.pack_into('<I',duplicate,28,1)
        reject('duplicate selected mapping',bytes(duplicate))

        # File metadata retains one physical IDATA section referenced by both nodes.
        maps=[struct.unpack_from('<HI',good,20+6*i) for i in range(4)]
        assert maps==[(0,1),(0,3),(1,2),(1,3)],maps
        records.append({'check':'two nodes reference the same physical IDATA','returncode':0})

        # Optional independent NASM comparison when installed on the user's host.
        import shutil
        if shutil.which('nasm'):
            for name in ('final_stack','recursive_overflow'):
                target=d/(name+'.uef')
                run('NASM assemble '+name,['nasm','-f','bin',ROOT/'reference'/(name+'.asm'),'-o',target])
                assert target.read_bytes()==assemble(ROOT/'reference'/(name+'.asm'))
            target=d/'stack16.bin'
            run('NASM assemble CODE-only FORMAT16 fixture',['nasm','-f','bin',ROOT/'examples/stack16.asm','-o',target])
            assert target.read_bytes()==stack

    report={'host':os.uname().machine,'binary':Path(binary).name,'checks_passed':len(records),
        'asan_options':os.environ.get('ASAN_OPTIONS','not enabled'),
        'legacy_stack_fixture_bytes':len(stack_blob),'legacy_code_sha256':hashlib.sha256(legacy_stack).hexdigest(),
        'converted_code_bytes':len(stack),'converted_code_sha256':hashlib.sha256(stack).hexdigest(),
        'nasm_available':bool(shutil.which('nasm')),'checks':records}
    path=ROOT/'build'/('validation-sanitizers.json' if binary.endswith('-san') else 'validation.json')
    path.write_text(json.dumps(report,indent=2)+'\n')
    print(f'UEEC VM: {len(records)} checks PASS ({os.uname().machine}; {Path(binary).name})')

if __name__=='__main__':main()
