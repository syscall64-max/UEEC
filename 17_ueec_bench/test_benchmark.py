"""Checks benchmark plumbing/workload equivalence; no timing thresholds."""
import argparse,csv,io,json,subprocess,tempfile
import sys
from pathlib import Path
from benchmark import ROOT,fixture,parse_samples,PHASES
from environment import inventory

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--binary',type=Path,required=True);a=ap.parse_args()
    binary=str(a.binary.resolve());checks=0
    def call(args,rc=0):
        nonlocal checks
        p=subprocess.run([binary,*map(str,args)],capture_output=True,text=True,timeout=10)
        assert p.returncode==rc,(args,p.returncode,p.stdout,p.stderr)
        assert 'AddressSanitizer' not in p.stderr and 'runtime error:' not in p.stderr,p.stderr
        checks+=1;return p
    info=json.loads(call(['--info']).stdout);assert info['clock'].startswith('CLOCK_MONOTONIC')
    env=inventory(None,'test note')
    assert env['memory']['physical_DIMM_speed_MT_s'] is None
    assert set(env['virtualization']['detectors'])=={'vm','container'}
    assert env['user_supplied_environment_note']=='test note'
    json.dumps(env);checks+=1
    with tempfile.TemporaryDirectory(prefix='ueec-bench-check-') as temp:
        d=Path(temp)
        for nodes in [1,8]:
            for sharing in ['shared','duplicated']:
                for granule in [1,4096]:
                    f=d/'fixture.ueec';f.write_bytes(fixture(nodes,1024,sharing,granule))
                    for mode,regions in [('monolithic',1),('scattered',3),('hybrid',2)]:
                        rows=parse_samples(call([f,mode,3,2,nodes-1]).stdout,3)
                        assert all(r['mapped_regions']==regions and r['selected_payload_bytes']==1028 for r in rows)
                        assert len({r['mapped_bytes'] for r in rows})==1
        assert fixture(1,1024,'shared',1)==fixture(1,1024,'duplicated',1)
        checks+=1
        f=d/'fixture.ueec';f.write_bytes(fixture(1,1024,'shared',1))
        parse_samples(call([f,'monolithic',1,0]).stdout,1)
        for mode,repeats,warmups in [('auto',1,0),('bogus',1,0),('monolithic',0,0),('monolithic',-1,0),('monolithic',1,-1)]:
            call([f,mode,repeats,warmups],2)
        call([f,'monolithic',1,0,99],1)
        f.write_bytes(b'not UEEC')
        call([f,'monolithic',1,0],1)
        call([d/'missing.ueec','monolithic',1,0],1)
        # Check the Python runner, report provenance and its raw/aggregate link.
        if '-san' not in Path(binary).name:
            out=d/'results'
            p=subprocess.run([sys.executable,str(ROOT/'benchmark.py'),'--binary',binary,
                '--output',str(out),'--samples','2','--warmups','1','--rounds','2',
                '--sizes','1024','--nodes','1','--granules','1','--environment-note','test note'],
                capture_output=True,text=True,timeout=30)
            assert p.returncode==0,(p.stdout,p.stderr)
            report=json.loads((out/'summary.json').read_text())
            raw=list(csv.DictReader((out/'samples.csv').open()))
            assert len(raw)==report['total_measured_samples']==24
            assert len(report['summary'])==6
            assert report['environment']['user_supplied_environment_note']=='test note'
            assert json.loads((out/'environment.json').read_text())['environment']==report['environment']
            assert report['build']['build_command']
            for case in report['summary']:
                values=sorted(int(r['load_ns']) for r in raw if r['case']==case['case'])
                assert case['metrics']['load_ns']['median_ns']==(values[1]+values[2])/2
            checks+=1
    print(f'Benchmark harness: {checks} checks PASS; no speed threshold asserted')

if __name__=='__main__':main()
