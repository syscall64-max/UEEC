#!/usr/bin/env python3
"""Extract self-contained executable sections; reject unresolved code relocations."""
from pathlib import Path
import re, shlex, subprocess, sys
b=Path(sys.argv[1]); cc=shlex.split(sys.argv[2])
def tool(name): return subprocess.check_output(cc+['-print-prog-name='+name],text=True).strip()
for name in ('add','sub','program'):
    sec='.text.'+name
    rel=subprocess.check_output([tool('objdump'),'-r','-j',sec,str(b/'payload.o')],text=True)
    if re.search(r'^\s*[0-9a-fA-F]+\s+\S+\s+',rel,re.M):
        raise SystemExit('unresolved code relocation: '+rel)
    path=b/(name+'.bin')
    subprocess.run([tool('objcopy'),'-O','binary','-j',sec,str(b/'payload.o'),str(path)],check=True)
    if not path.stat().st_size: raise SystemExit('empty code section: '+sec)
