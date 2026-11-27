#!/usr/bin/env python3
"""Test trusted fixtures, assert output, and require rejection of invalid metadata."""
import argparse, subprocess, shlex, re, sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('stage',choices=['info','load']);p.add_argument('program');p.add_argument('fixtures',type=Path);p.add_argument('--runner',default='');a=p.parse_args()
base=shlex.split(a.runner)+[a.program]
count=0
def run(args, expected=True, check=None):
    global count
    r=subprocess.run(base+list(map(str,args)),capture_output=True,text=True,timeout=30)
    if (expected and r.returncode != 0) or (not expected and (r.returncode != 1 or 'error:' not in r.stderr)) or any(marker in r.stderr for marker in ('AddressSanitizer', 'UndefinedBehaviorSanitizer', 'runtime error:')) or (check and not check(r.stdout)):
        print('FAIL',base,args,r.returncode,r.stdout,r.stderr,file=sys.stderr);sys.exit(1)
    count+=1
for w in (16,32,64):
    for e in ('le','be'):
        path=a.fixtures/f'sh{w}-{e}.ueec'
        if a.stage=='info':
            def check(s):
                return all(re.search(q,s,re.M) for q in [r'^\s*version:\s*2$',r'^\s*nodes:\s*12$',r'^\s*global sections:\s*13$',rf'^\s*section format:\s*SH{w}\b',rf'^\s*metadata endian:\s*{"little" if e=="le" else "big"}',r'\[11\] name="exit" machine=CPU/s390x'])
            run([path],check=check)
            def check_shared(s):
                return check(s.replace('global sections:    14','global sections:    13')) and bool(re.search(r'^\s*node mappings:\s*24$',s,re.M)) and bool(re.search(r'^\s*\[13\] type=CONSTANT_DATA .* nodes=0,1,2,3,4,5,6,7,8,9,10,11$',s,re.M))
            run([a.fixtures/f'sh{w}-{e}-shared.ueec'],check=check_shared)
        else:
            for shared in (False, True):
                fixture = a.fixtures/f'sh{w}-{e}-shared.ueec' if shared else path
                for mode in ('auto','monolithic','scattered','hybrid'):
                    def check(s):
                        return bool(re.search(r'^  runtime sections:\s*'+str(3 if shared else 2)+r'$',s,re.M)) and bool(re.search(r'^  actual mode:\s*'+('(?:monolithic|scattered)' if mode=='auto' else mode)+r'$',s,re.M))
                    run(['--node','exit','--load-mode',mode,'--no-run',fixture],check=check)
                    run(['--node','exit','--load-mode',mode,fixture],check=check)
for k in ('bad-version','bad-format','truncated','bad-magic','truncated-data'):
    run(([ '--no-run'] if a.stage=='load' else [])+[a.fixtures/f'{k}.ueec'],expected=False)
print(f'[PASS] {a.stage}: {count} cases')
