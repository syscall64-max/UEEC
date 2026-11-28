from pathlib import Path
import subprocess,csv,os,re
rows=[]; runs=[]; p=Path('fixtures'); c=len(Path('build/code.bin').read_bytes())
for n in (16,256,4096,32768):
 for label,expected in [('base',57),('control',58)]:
  for kind in ('single','split','sh16_physical','sh32_physical','sh64_physical','sh16_nobits','sh32_nobits','sh64_nobits'):
    ueec=kind.startswith('sh'); f=p/f'{n}_{label}_{kind}.{"ueec" if ueec else "elf"}'
    h=50+6*(2+2*(int(kind.split('_')[0][2:])//8)) if ueec else (120 if kind=='single' else 232)
    r=22 if ueec else 0; disk=n if kind.endswith('physical') else 0
    gap=8208-h-c-32 if kind=='split' else 0
    assert f.stat().st_size==h+c+32+r+disk+gap
    if label=='base': rows.append([n,kind,h,c,16,16,r,disk,gap,f.stat().st_size])
    for mode in ('monolithic','scattered','hybrid') if ueec else ('kernel',):
        env=os.environ.copy(); env.pop('UEEC_EXPERIMENT_NOBITS',None)
        if kind.endswith('nobits'): env['UEEC_EXPERIMENT_NOBITS']='1'
        cmd=['build/loader','--load-mode',mode,str(f)] if ueec else [str(f)]
        proc=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=5)
        regions={'monolithic':1,'scattered':6,'hybrid':2}.get(mode)
        ok=proc.returncode==expected and (not ueec or f'MATRIX_RELOC_OK runtime_header_addresses=6 regions={regions}' in proc.stdout and re.search(r'actual mode:\s+'+mode+r'\b',proc.stdout))
        print(f'{f} {mode} exit={proc.returncode} expected={expected} {"PASS" if ok else "FAIL"}'); runs.append([n,label,kind,mode,proc.returncode,ok]);
        if not ok: print(proc.stdout,proc.stderr); raise SystemExit(1)
for filename,head,data in [('results.csv',['memory_bytes','format','headers','code','constant_data','init_data','relocation','physical_unused_bytes','gap','file_size'],rows),('execution.csv',['memory_bytes','case','format','mode','exit','pass'],runs)]:
 with open(filename,'w') as f: w=csv.writer(f);w.writerow(head);w.writerows(data)
print(Path('results.csv').read_text(),end=''); print(f'{len(runs)}/{len(runs)} PASS')
