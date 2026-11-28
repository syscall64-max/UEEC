from pathlib import Path
import subprocess, csv, struct
p=Path('fixtures'); code=Path('build/code.bin').read_bytes(); rows=[]; tests=[]
for label,expected in [('base',42),('control',43)]:
    for kind in ('single','split','ueec'):
        f=p/(f'{label}.ueec' if kind=='ueec' else f'{label}_{kind}.elf'); b=f.read_bytes()
        if kind=='ueec':
            h=62; reloc=10; data_at=h+len(code); c=b[h:data_at]; assert b[data_at+16:]==struct.pack('<5H',0,0,32,1,0x1002)
            assert c[:2]+c[10:]==code[:2]+code[10:]; assert c[2:10]==bytes(8)
            modes=('monolithic','scattered')
        else:
            eh=struct.unpack_from('<16sHHIQQQIHHHHHH',b); h=64+56*eh[10]; reloc=0; data_at=h+len(code) if kind=='single' else 4096
            c=b[h:h+len(code)]; assert c[:2]+c[10:]==code[:2]+code[10:]; assert struct.unpack_from('<Q',c,2)[0]==0x400000+data_at
            modes=('kernel',)
        assert sum(b[data_at:data_at+16])==expected
        gap=len(b)-h-len(code)-16-reloc; assert gap>=0
        if label=='base': rows.append([kind,h,len(code),16,reloc,gap,len(b)])
        for mode in modes:
            cmd=[str(f)] if mode=='kernel' else ['build/loader',f'--L={mode}',str(f)]
            r=subprocess.run(cmd,capture_output=True,text=True,timeout=5)
            ok=r.returncode==expected and (kind!='ueec' or ('ABS64' in r.stdout and 'unmapped after relocation' in r.stdout))
            print(f'{f} {mode}: exit={r.returncode} expected={expected} {"PASS" if ok else "FAIL"}',flush=True)
            tests.append(ok)
            if not ok: print(r.stdout,r.stderr); raise SystemExit(1)
with open('results.csv','w') as f:
    w=csv.writer(f); w.writerow(['format','headers','code','constant_data','relocation','gap','file_size']); w.writerows(rows)
print(Path('results.csv').read_text(),end=''); print(f'{sum(tests)}/{len(tests)} PASS')
