#!/usr/bin/env python3
"""Bridge the native C UEEC selector and real Renode CPU/GPIO execution."""
import argparse
from dataclasses import asdict
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from models import MODELS,HALF_PERIOD_MS
ROOT=Path(__file__).resolve().parent

def find_renode():
    configured=os.environ.get('RENODE')
    if configured:
        executable=Path(configured).expanduser().resolve()
        if not executable.is_file(): raise RuntimeError('RENODE does not name an executable file')
        return str(executable)
    executable=shutil.which('renode')
    if executable: return executable
    if sys.platform!='linux' or os.uname().machine not in ('x86_64','amd64'):
        raise RuntimeError('Install Renode and set RENODE to its executable on this host')
    from tools.setup_renode import install
    return str(install())

def time_ns(text):
    hours,minutes,seconds=text.split(':')
    whole,_,fraction=seconds.partition('.')
    return ((int(hours)*60+int(minutes))*60+int(whole))*1_000_000_000+int((fraction+'0'*9)[:9])

def run(model,source,selector=None,edges=8,no_run=False,list_nodes=False,vcd=None,report_path=None,log_path=None):
    selector=Path(selector or ROOT/'build/ueec-stm32').resolve()
    executable=find_renode();source=Path(source).resolve()
    if not selector.is_file() or not source.is_file(): raise RuntimeError('Build the selector and UEEC file first: make all')
    if vcd and Path(vcd).exists(): raise RuntimeError('VCD exists; will not overwrite')
    with tempfile.TemporaryDirectory(prefix='ueec-stm32-') as directory:
        work=Path(directory)
        config=asdict(model)
        config.update(model=model.name,selector=str(selector),source=str(source),list_nodes=list_nodes,
                      no_run=no_run,edges=edges,device_profile=str(work/'device.bin'),
                      code=str(work/'selected.bin'),manifest=str(work/'selected.json'))
        driver=work/'driver.py'
        driver.write_text('import json\nCONFIG=json.loads('+repr(json.dumps(config,ensure_ascii=True))+')\n'+
                          (ROOT/'tools/renode_driver.py').read_text(),encoding='utf-8')
        # Runtime-owned paths below have no spaces; user paths travel as JSON
        # and ProcessStartInfo.ArgumentList, never through a shell.
        platform=work/'platform.repl';shutil.copyfile(ROOT/'platforms'/(model.name+'.repl'),platform)
        monitor_python='execfile('+repr(str(driver))+')'
        script=work/'run.resc'
        script.write_text('mach create "UEEC_STM32"\n'+f'machine LoadPlatformDescription @{platform}\n'+
                          'python '+json.dumps(monitor_python)+'\nquit\n')
        command=[executable,'--disable-xwt','--plain','--console','--config',str(work/'renode.config'),str(script)]
        environment=os.environ.copy()
        environment['TMPDIR']=str(work)
        result=subprocess.run(command,capture_output=True,text=True,timeout=45,env=environment)
        text=result.stdout+result.stderr
        if log_path: Path(log_path).write_text(text)
        failed=('UEEC_FAILURE' in text or 'There was an error' in text or '[ERROR]' in text or
                'ERROR: AddressSanitizer' in text or 'runtime error:' in text)
        marker='UEEC_SELECTION_PASS' if no_run or list_nodes else 'UEEC_EMULATION_PASS'
        if result.returncode!=0 or failed or marker not in text:
            raise RuntimeError(f'Renode/C selection failed (exit={result.returncode}):\n{text[-9000:]}')
        device_data=(work/'device.bin').read_bytes()
        if list_nodes:
            for line in text.splitlines():
                if line.startswith(('PROBE ','DEVICE_PROFILE ','node=')): print(line)
            return dict(model=model.name,device_profile_hex=device_data.hex(),selection_only=True)
        selected=json.loads((work/'selected.json').read_text())
        for line in text.splitlines():
            if line.startswith(('PROBE ','DEVICE_PROFILE ','SELECTED ','FLASH ')): print(line)
        record=dict(model=model.name,device_profile_hex=device_data.hex(),selected=selected,events=[])
        if not no_run:
            events=[dict(time_ns=time_ns(t),level=int(level)) for t,level in re.findall(r'^UEEC_GPIO time=([0-9:.]+) level=([01])',text,re.M)]
            if len(events)<edges or any(a['level']==b['level'] for a,b in zip(events,events[1:])):
                raise RuntimeError('missing or non-alternating real GPIO events')
            expected=selected['half_period_ms']*1_000_000
            if any(abs(b['time_ns']-a['time_ns']-expected)>expected//50+100_000 for a,b in zip(events,events[1:])):
                raise RuntimeError('observed GPIO timing does not match metadata')
            record['events']=events
            for i,event in enumerate(events[:edges],1):
                print(f"GPIO edge={i} port={chr(selected['port'])} pin={selected['pin']} level={event['level']} "
                      f"time_us={event['time_ns']//1000} LED={'ON' if event['level'] else 'OFF'}")
            if vcd:
                with Path(vcd).open('x') as output:
                    output.write('$timescale 1ns $end\n$scope module ueec_stm32 $end\n$var wire 1 ! LED $end\n'
                                 '$upscope $end\n$enddefinitions $end\n$dumpvars\nx!\n$end\n')
                    for event in events: output.write(f"#{event['time_ns']}\n{event['level']}!\n")
            print(f"PASS: Renode virtual Flash + Cortex-M execution + {len(events)} GPIO transitions; half_period_ms={selected['half_period_ms']}")
        if report_path: Path(report_path).write_text(json.dumps(record,indent=2)+'\n')
        return record

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('file',nargs='?',type=Path)
    p.add_argument('--mcu',choices=[m.name for m in MODELS],default='stm32f103rb',help='configure virtual target; UEEC node is selected automatically')
    p.add_argument('--list-models',action='store_true')
    p.add_argument('--list-nodes',action='store_true')
    p.add_argument('--no-run',action='store_true')
    p.add_argument('--vcd',type=Path)
    p.add_argument('--report',type=Path)
    p.add_argument('--log',type=Path)
    p.add_argument('--edges',type=int,default=8)
    a=p.parse_args()
    if a.list_models:
        for m in MODELS: print(f'{m.name}: {m.cpu}, Flash={m.flash_bytes}, boot RAM={m.ram_bytes}, GPIO=P{m.port}{m.pin}')
        return
    if not a.file or not 3<=a.edges<=32: p.error('provide FILE.ueec and 3 <= --edges <= 32')
    try:
        run(next(m for m in MODELS if m.name==a.mcu),a.file,edges=a.edges,no_run=a.no_run,
            list_nodes=a.list_nodes,vcd=a.vcd,report_path=a.report,log_path=a.log)
    except (OSError,RuntimeError,subprocess.TimeoutExpired) as error:
        print('ERROR: '+str(error),file=sys.stderr);sys.exit(1)

if __name__=='__main__': main()
