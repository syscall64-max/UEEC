from pathlib import Path
import subprocess,csv,struct,re
p=Path('fixtures'); code=Path('build/code.bin').read_bytes(); rows=[]; results=[]
for label,expected in [('base',42),('control',43)]:
    for kind in ('single','split','sh16','sh32','sh64'):
        ueec=kind.startswith('sh'); f=p/f'{label}_{kind}.{ "ueec" if ueec else "elf"}'; b=f.read_bytes()
        if ueec:
            w=int(kind[2:])//8; h=8+6+18+4*(2+2*w)+6; reloc=10; data_at=h+len(code); c=b[h:data_at]
            assert b[data_at+16:]==struct.pack('<5H',0,0,32,1,0x1002) and c[2:10]==bytes(8)
            modes=('monolithic','scattered','hybrid')
        else:
            eh=struct.unpack_from('<16sHHIQQQIHHHHHH',b); h=64+56*eh[10]; reloc=0; data_at=h+len(code) if kind=='single' else 4096; c=b[h:h+len(code)]
            assert struct.unpack_from('<Q',c,2)[0]==0x400000+data_at; modes=('kernel',)
        assert c[:2]+c[10:]==code[:2]+code[10:] and sum(b[data_at:data_at+16])==expected
        gap=len(b)-h-len(code)-16-reloc; assert gap>=0
        if label=='base': rows.append([kind,h,len(code),16,reloc,gap,len(b)])
        for mode in modes:
            cmd=['build/loader','--load-mode',mode,str(f)] if ueec else [str(f)]
            r=subprocess.run(cmd,capture_output=True,text=True,timeout=5)
            regions={'monolithic':1,'scattered':4,'hybrid':2}.get(mode)
            ok=r.returncode==expected and (not ueec or (f'MATRIX_RELOC_OK runtime_header_addresses=4 regions={regions}' in r.stdout and re.search(r'actual mode:\s+'+mode+r'\b',r.stdout)))
            print(f'{f} {mode}: exit={r.returncode} expected={expected} {"PASS" if ok else "FAIL"}',flush=True)
            results.append([str(f),mode,expected,r.returncode,regions or '',bool(ok)])
            if not ok: print(r.stdout,r.stderr); raise SystemExit(1)
with open('results.csv','w') as f:
    w=csv.writer(f); w.writerow(['format','headers','code','constant_data','relocation','gap','file_size']); w.writerows(rows)
with open('execution.csv','w') as f:
    w=csv.writer(f); w.writerow(['file','mode','expected','actual','runtime_regions','pass']); w.writerows(results)
print(Path('results.csv').read_text(),end='');print(f'{len(results)}/{len(results)} PASS')
