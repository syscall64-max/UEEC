import json,pathlib
arches='x86 x86_64 arm32 aarch64 riscv32 riscv64 ppc32 ppc64 sparc32 sparc64 loongarch64 s390x'.split()
reports=[json.loads(pathlib.Path(f'build/{a}/report.json').read_text()) for a in arches]
assert all(r['architecture']==a and r['cases']==504 and r['rejections']==84 and r['sha256']==reports[0]['sha256'] for a,r in zip(arches,reports))
summary={k:sum(r[k] for r in reports) for k in ('cases','load_cycles','native_calls','rejections')}
summary['architectures']=reports
pathlib.Path('build/integration-report.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS 12 architectures:', {k:v for k,v in summary.items() if k!='architectures'})
