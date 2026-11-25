#!/usr/bin/env python3
"""Format/selection fixtures plus mandatory real Renode CPU/GPIO execution."""
import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

from generate import build
from models import MODELS,FLASH_BASE,RAM_BASE
from run_stm32 import run

ROOT=Path(__file__).resolve().parent

def profile(m):
    # Synthetic profiles are ONLY unit-test fixtures. End-to-end tests below
    # use CPUID, memory sizes and timer frequency observed inside Renode.
    return struct.pack('<4sHHIIIIIIQIBBH',b'STP1',m.model_id,0,0x410f0000|(m.part<<4),
                       FLASH_BASE,m.flash_bytes,RAM_BASE,m.ram_bytes,m.clock_hz,
                       m.capabilities,m.gpio_base,ord(m.port),m.pin,0)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loader',type=Path,default=ROOT/'build/ueec-stm32')
    args=parser.parse_args();loader=args.loader.resolve()
    # Also inherited by selector subprocesses launched inside Renode/.NET.
    os.environ.setdefault('ASAN_OPTIONS','detect_leaks=0')
    output=ROOT/'build/results';output.mkdir(parents=True,exist_ok=True)
    checks=0
    with tempfile.TemporaryDirectory(prefix='ueec-stm32-tests-') as directory:
        work=Path(directory)
        def call(data,m=MODELS[0],valid=True,need=None,device=None,extra=()):
            nonlocal checks
            source=work/'test.ueec';source.write_bytes(data)
            p=work/'device.bin';p.write_bytes(profile(m) if device is None else device)
            env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
            result=subprocess.run([str(loader),'--select','--device-profile',str(p),*extra,str(source)],
                                  capture_output=True,text=True,timeout=10,env=env)
            text=result.stdout+result.stderr
            if result.returncode<0 or 'AddressSanitizer' in text or 'runtime error:' in text:
                raise AssertionError('selector crashed: '+text)
            if (result.returncode==0)!=valid or (need and need not in text):
                raise AssertionError(f'expected valid={valid}, need={need}; exit={result.returncode}\n{text}')
            checks+=1
            return text

        # All six endian/metadata-width combinations select each model.
        for width in (2,4,8):
            for endian in ('little','big'):
                for m in MODELS:
                    call(build(width,endian),m,need='SELECTED model='+m.name)
        for i,m in enumerate(MODELS):
            call(build(reverse=True),m,need=f'node={2-i} ')
            call(build(models=[m.name]),m,need='node=0 ')
            call(build(models=[MODELS[(i+1)%3].name]),m,valid=False,need='no compatible')
        text=call(build(),extra=('--list-nodes',))
        assert text.count('compatible=yes')==1 and text.count('compatible=no')==2
        # Fixed uint16_t node indices: select index 256, not truncated index 0.
        many=build(model_sequence=[MODELS[0]]*256+[MODELS[1]])
        call(many,MODELS[1],need='node=256 ')
        raw=work/'only-code.bin';manifest=work/'manifest.json'
        call(build(),extra=('--extract',str(raw),'--manifest',str(manifest)))
        assert raw.read_bytes()==(ROOT/'payloads/stm32f103rb.bin').read_bytes()
        assert json.loads(manifest.read_text())['code_bytes']==len(raw.read_bytes())
        call(build(),valid=False,extra=('--extract',str(raw)))
        assert raw.read_bytes()==(ROOT/'payloads/stm32f103rb.bin').read_bytes()
        call(build(),valid=False,extra=('--manifest',str(manifest)))

        # A valid container can still have no compatible target.
        for field,value in [('model_id',77),('core_part',0xc24),('flash_bytes',262144),
                            ('ram_bytes',32768),('clock_hz',16000000),('gpio_base',0x40010c00),
                            ('port','B'),('pin',6),('required',1<<63)]:
            call(build(overrides={MODELS[0].name:{field:value}}),valid=False)
        for field,value in [('ram_bytes',0),('flash_bytes',0),('clock_hz',0),('port','Z'),
                            ('pin',16),('half_period_ms',0),('half_period_ms',1001)]:
            call(build(overrides={MODELS[0].name:{field:value}}),valid=False)

        baseline=build();header=struct.unpack_from('<H',baseline,8+18+36+2)[0]
        # Header section begins after 8-byte header, 3 node headers and 6 maps.
        assert header==104
        md_at=header+sum(len((ROOT/'payloads'/(m.name+'.bin')).read_bytes()) for m in MODELS)
        md_head=md_at+6;data_at=md_at+6+13*6
        def mutated(offset,fmt,value):
            data=bytearray(baseline);struct.pack_into(fmt,data,offset,value);return bytes(data)
        # Structural rejection includes foreign-node metadata and sentinel.
        for offset,fmt,value in [
            (0,'<B',0),(4,'<H',0),(md_at,'<H',3),(md_at+2,'<H',0),
            (md_at+4,'<H',65535),(md_head,'<H',3),(md_head+2,'<H',0xffff),
            (md_head+4,'<H',1),(md_head+6+2,'<H',0x210),
            (md_head+6+4,'<H',65535),(md_head+12*6,'<H',1),
            (md_head+12*6+2,'<H',1),(md_head+12*6+4,'<H',0),
            (data_at+42*2+24+8+4,'<B',ord('Z')),
            (header,'<I',RAM_BASE),(header,'<I',RAM_BASE+20484),
            (header,'<I',RAM_BASE+20479),(header+4,'<I',FLASH_BASE+64),
            (header+4,'<I',FLASH_BASE+1),(header+4,'<I',FLASH_BASE+0x10001)]:
            call(mutated(offset,fmt,value),valid=False)
        for size in (0,7,header-1,len(baseline)-1): call(baseline[:size],valid=False)
        for offset,fmt,value in [(0,'<B',0),(4,'<H',99),(6,'<H',1),(8,'<I',0),
                                 (12,'<I',0),(16,'<I',1),(20,'<I',0),(24,'<I',1),
                                 (28,'<I',1),(32,'<Q',0),(40,'<I',0),
                                 (44,'<B',ord('B')),(45,'<B',6),(46,'<H',1)]:
            data=bytearray(profile(MODELS[0]));struct.pack_into(fmt,data,offset,value)
            call(baseline,device=bytes(data),valid=False)
        call(baseline,device=profile(MODELS[0])[:-1],valid=False)
        unit_count=checks
        print(f'C selector: {unit_count} format/compatibility/extraction checks PASS',flush=True)

        # Every target MUST really execute; missing Renode or API errors fail.
        reports=[]
        for m in MODELS:
            for suffix in ('vcd','json','log'):
                (output/(m.name+'.'+suffix)).unlink(missing_ok=True)
            report=run(m,ROOT/'build/blink.ueec',selector=loader,vcd=output/(m.name+'.vcd'),
                       report_path=output/(m.name+'.json'),log_path=output/(m.name+'.log'))
            assert len(report['events'])==8
            reports.append(report);checks+=1
        # Byte order/64-bit shared-index offsets also survive actual loading.
        big=work/'big reversed.ueec';big.write_bytes(build(8,'big',reverse=True))
        with contextlib.redirect_stdout(io.StringIO()):
            report=run(MODELS[1],big,selector=loader)
        assert report['selected']['node']==1 and len(report['events'])==8
        checks+=1
        # No-run and listing observe the virtual target but do not write Flash.
        for flags in ({'no_run':True},{'list_nodes':True}):
            log=work/'inspect.log'
            with contextlib.redirect_stdout(io.StringIO()):
                report=run(MODELS[0],ROOT/'build/blink.ueec',selector=loader,log_path=log,**flags)
            assert 'FLASH bytes=' not in log.read_text() and not report.get('events')
            checks+=1
        # End-to-end incompatible file must be rejected before any Flash write.
        wrong=work/'wrong.ueec';wrong.write_bytes(build(models=[MODELS[1].name]))
        log=work/'wrong.log'
        try:
            with contextlib.redirect_stdout(io.StringIO()): run(MODELS[0],wrong,selector=loader,log_path=log)
        except RuntimeError as error:
            assert 'no compatible' in str(error) and 'FLASH bytes=' not in log.read_text()
        else: raise AssertionError('incompatible firmware accepted')
        checks+=1
        # A metadata promise is not enough: wrong blink timing MUST fail.
        wrong.write_bytes(build(overrides={MODELS[0].name:{'half_period_ms':200}}))
        try:
            with contextlib.redirect_stdout(io.StringIO()): run(MODELS[0],wrong,selector=loader)
        except RuntimeError: pass
        else: raise AssertionError('incorrect GPIO timing accepted')
        checks+=1
        summary=dict(checks=checks,selector_checks=unit_count,renode_checks=checks-unit_count,
                     models=3,primary_gpio_transitions=sum(len(r['events']) for r in reports),
                     extra_big_endian_gpio_transitions=8,loader=loader.name,no_skips=True)
        (output/('summary-sanitize.json' if 'sanitize' in loader.name else 'summary.json')).write_text(json.dumps(summary,indent=2)+'\n')
        print(f'UEEC STM32: 3 Cortex-M targets, 24 primary + 8 endian-test GPIO transitions; {checks} checks PASS (no skips)')

if __name__=='__main__': main()
