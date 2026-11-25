#!/usr/bin/env python3
"""Execute every compatible CODE and test selection, metadata and rejection."""
import argparse
import json
from pathlib import Path
import re
import resource
import signal
import shlex
import subprocess
import sys
import tempfile

from architectures import ARCHES, native_arch
from generate import ROOT, build

NEGATIVE_CPUS = {
    "arm32": ("cortex-a9,neon=off,vfp-d32=off", "scalar", "neon"),
    "arm64": ("cortex-a53", "neon", "sve"),
    "loongarch64": ("max,lasx=off", "lsx", "lasx"),
    "powerpc32": ("750", "scalar", "altivec"),
    "riscv32": ("rv32,v=false", "scalar", "rvv"),
    "riscv64": ("rv64,v=false", "scalar", "rvv"),
    "x86": ("pentium3", "scalar", "sse2"),
    "x86_64": ("qemu64", "sse2", "avx2"),
    "s390x": ("max,vx=off", "scalar", "vx"),
}

# CPU-model cases run only if an independent libc/ABI probe can execute on
# that model. The three reduced-mask cases always run, including when the
# toolchain's libc requires an extension that the model deliberately removes.
# PowerPC64 is deliberately not in this table: this demo's supported runtime
# profile is POWER8. A small ABI probe cannot certify that every path through
# a complete statically linked loader is safe on POWER5+. PowerPC64 still runs
# scalar/AltiVec plus selection, rejection and fallback tests on POWER8.

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--arch",choices=tuple(ARCHES),default=native_arch())
    p.add_argument("--qemu",action="store_true",help="force QEMU even for the native architecture")
    p.add_argument("--cpu",help="QEMU model override")
    p.add_argument("--loader",type=Path,help="override the loader binary, e.g. a sanitizer build")
    args=p.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    arch,spec=args.arch,ARCHES[args.arch]
    emulated=args.qemu or arch!=native_arch()
    loader=args.loader.resolve() if args.loader else ROOT/"build"/("ueec-loader-"+arch)
    prefix=[spec.qemu,"-cpu",args.cpu or spec.cpu] if emulated else []
    base=prefix+[str(loader)]
    checks=0
    executed=set()
    negative_mode="capability-mask"
    abi_preflight=None

    def call(options=(),file=None,ok=True,need=None,execute=False,command=None):
        nonlocal checks
        cmd=(command or base)+list(options)+([str(file)] if file else [])
        r=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
        text=r.stdout+r.stderr
        if "ERROR: AddressSanitizer" in text or "runtime error:" in text:
            raise AssertionError("sanitizer failure: "+text)
        if r.returncode<0:
            raise AssertionError(f"loader crashed: {shlex.join(cmd)}\nexit={r.returncode}\n{text}")
        if (r.returncode==0)!=ok or (need and not re.search(need,text)):
            raise AssertionError(f"{shlex.join(cmd)}\nexit={r.returncode}\n{text}")
        if execute and "PASS: 256 alignment/length cases, 32 guard-page cases" not in text:
            raise AssertionError("CODE did not complete its execution checks: "+text)
        if execute and "EXECUTE resident_sections=1 metadata_resident=no" not in text:
            raise AssertionError("metadata residency check missing: "+text)
        checks+=1
        return text

    with tempfile.TemporaryDirectory(prefix="ueec-dispatch-",dir=ROOT/"build") as temporary:
        work=Path(temporary)
        universal=ROOT/"build/sum.ueec"
        call(["--groups"],need="OS usable:")
        listing=call(["--list-technologies"],universal)
        rows=[dict(re.findall(r"(\w+)=([^\s]+)",line)) for line in listing.splitlines() if line.startswith("node=")]
        assert len(rows)==sum(len(a.variants) for a in ARCHES.values())
        assert len({r["architecture"] for r in rows})==12
        assert {int(r["METADATA"]) for r in rows}=={len(rows)+1}
        assert all(r["compatible"]=="no" for r in rows if r["architecture"]!=arch)
        host=[r for r in rows if r["architecture"]==arch]
        available=[r for r in host if r["compatible"]=="yes"]
        assert len(host)==len(spec.variants) and available
        best=max(available,key=lambda r:int(r["priority"]))["technology"]
        call([],universal,need="technology="+best,execute=True)
        selected=call(["--no-run"],universal,need="SELECTED")
        assert "PASS:" not in selected and "sum(1..64)" not in selected
        assert "SELECTED" not in listing and "PASS:" not in listing
        host_listing=call(["--list-technologies","--host-only"],universal)
        assert sum(x.startswith("node=") for x in host_listing.splitlines())==len(host)
        for r in host:
            tech=r["technology"]
            if r["compatible"]=="yes":
                call(["--technology",tech],universal,need="technology="+tech,execute=True)
                executed.add(tech)
            else:
                call(["--technology",tech],universal,ok=False,need="cannot run")
        non_scalar=spec.variants[-1].name
        call(["--limit-isa","scalar"],universal,need="technology=scalar",execute=True)
        call(["--limit-isa","scalar","--technology",non_scalar],universal,ok=False,need="cannot run")
        call(["--limit-isa","scalar","--technology",non_scalar,"--fallback"],universal,need="FALLBACK",execute=True)
        call(["--technology","absent-technology"],universal,ok=False,need="not present")
        call(["--technology","absent-technology","--fallback"],universal,need="technology="+best,execute=True)
        call(["--fallback"],universal,ok=False,need="Usage:")
        call(["--unknown"],universal,ok=False,need="Usage:")

        def fixture(label,**kw):
            dest=work/(label+".ueec")
            dest.write_bytes(build(**kw))
            return dest

        def shared_location(blob,endian="little"):
            import struct
            e="<" if endian=="little" else ">"
            n=struct.unpack_from(e+"H",blob,4)[0]
            table=8+n*18
            return sum(struct.unpack_from(e+"H",blob,table+6*i+2)[0] for i in range(n+1))
        # One container contains both code endians. Outer structure and
        # METADATA endianness are independently varied for all numeric widths.
        for endian in ("little","big"):
            for width in (2,4,8):
                f=fixture(f"{endian}-{width}",width=width,endian=endian,reverse=True)
                call([],f,need="technology="+best,execute=True)
                # Huge counts must be bounded before addition/multiplication
                # at every width, including uint64, in both byte orders.
                malformed=bytearray(f.read_bytes());md=shared_location(malformed,endian)
                malformed[md+4:md+4+width]=b"\xff"*width
                bad=work/f"count-overflow-{endian}-{width}.ueec";bad.write_bytes(malformed)
                call([],bad,ok=False,need="malformed")
                old=fixture(f"legacy-{endian}-{width}",width=width,endian=endian,
                            metadata_layout="per-node",reverse=True)
                call([],old,need="technology="+best,execute=True)
        # A real record for node 256 proves that node_index is not truncated
        # to eight bits. All code is scalar; only the last node has priority 901.
        from dataclasses import replace
        scalar=spec.variants[0]
        try:
            # generate.build emits variants in reverse priority-list order.
            ARCHES[arch]=replace(spec,variants=(replace(scalar,priority=901),)+(scalar,)*256)
            wide_node=fixture("node-256",architectures=[arch])
        finally:
            ARCHES[arch]=spec
        call([],wide_node,need=r"technology=scalar node=256 CODE=257 priority=901",execute=True)
        call([],fixture("priority",overrides={(arch,"scalar"):(0,900)}),
             need="technology=scalar",execute=True)
        unknown={(arch,v.name):(v.required|(1<<63),v.priority) for v in spec.variants if v.name!="scalar"}
        call([],fixture("unknown-bit",overrides=unknown),need="technology=scalar",execute=True)
        unknown[(arch,"scalar")]=(1<<63,100)
        call([],fixture("no-compatible",overrides=unknown),ok=False,need="no compatible node")
        wrong_arch=next(a for a in ARCHES if a!=arch)
        call([],fixture("foreign-only",architectures=[wrong_arch]),ok=False,need="no compatible node")

        good=build(architectures=[arch])
        for length in (0,3,7,20,len(good)-1):
            bad=work/("truncated-"+str(length)+".ueec")
            bad.write_bytes(good[:length]);call([],bad,ok=False)
        # Independently locate the single trailing shared MD section. Its
        # mappings all refer to one section; every record has a uint16 node.
        import struct
        nodes=len(spec.variants)
        sh_at=8+nodes*6+nodes*12
        descriptors=[struct.unpack_from("<HHH",good,sh_at+6*i) for i in range(nodes+2)]
        assert [d[0] for d in descriptors]==[0]+[1]*nodes+[13]
        for node in range(nodes):
            assert struct.unpack_from("<HIHI",good,8+nodes*6+node*12)==(node,node+1,node,nodes+1)
        md_at=sum(d[1] for d in descriptors[:-1])
        assert md_at+descriptors[-1][1]==len(good)
        assert struct.unpack_from("<HHH",good,md_at)==(0,1,2*nodes)
        sentinel=md_at+6+2*nodes*6
        assert struct.unpack_from("<HHH",good,sentinel)==(0,0,10*nodes)
        for node in range(nodes):
            assert struct.unpack_from("<HHH",good,md_at+6+12*node)==(node,spec.metadata_type,10*node)
            assert struct.unpack_from("<HHH",good,md_at+12+12*node)==(node,2,10*node+8)
        checks+=1
        for label,offset,replacement in (
            ("bad-design",md_at+2,b"\x02\x00"),
            ("bad-family",md_at+8,b"\xff\x7f"),
            ("bad-sentinel",sentinel+2,b"\x01\x00"),
            ("bad-sentinel-node",sentinel,b"\x01\x00"),
            ("bad-offset",md_at+10,b"\xff\xff"),
            ("bad-node",md_at+6,struct.pack("<H",nodes)),
            ("bad-node-65535",md_at+6,b"\xff\xff"),
            ("bad-duplicate-priority",md_at+24,b"\x00\x00"),
            ("bad-empty-index",md_at+4,b"\x00\x00"),
            ("bad-index-count",md_at+4,b"\xff\xff"),
        ):
            malformed=bytearray(good);malformed[offset:offset+len(replacement)]=replacement
            f=work/(label+".ueec");f.write_bytes(malformed)
            call([],f,ok=False,need="malformed")
        # A foreign node's corrupt metadata must fail even without --list.
        all_nodes=sum(len(a.variants) for a in ARCHES.values())
        foreign_index=sum(len(a.variants) for a in list(ARCHES.values())[:list(ARCHES).index(wrong_arch)])
        foreign_blob=bytearray(build())
        table=8+all_nodes*18
        shared_at=sum(struct.unpack_from("<H",foreign_blob,table+6*i+2)[0] for i in range(all_nodes+1))
        struct.pack_into("<H",foreign_blob,shared_at+6+12*foreign_index+2,0x7fff)
        f=work/"foreign-corrupt.ueec";f.write_bytes(foreign_blob)
        call([],f,ok=False,need="malformed")
        # Distinguish an unsupported libc/CPU baseline from a UEEC failure.
        # A model that cannot run an independent C program cannot be used to
        # test loader dispatch. Never treat a failed UEEC call as a baseline
        # limitation: when the ABI probe passes, all three calls must pass.
        if emulated and arch in NEGATIVE_CPUS and args.cpu is None:
            cpu,expected,rejected=NEGATIVE_CPUS[arch]
            probe=ROOT/"build"/("abi-probe-"+arch)
            if not probe.is_file():
                raise RuntimeError("Missing ABI baseline probe; rebuild with make loaders")
            result=subprocess.run([spec.qemu,"-cpu",cpu,str(probe)],
                                  capture_output=True,text=True,timeout=30)
            output=result.stdout+result.stderr
            abi_preflight=dict(cpu=cpu,returncode=result.returncode,output=output)
            if result.returncode==0 and "UEEC ABI baseline probe PASS" in output:
                negative_mode="cpu-model+capability-mask"
                command=[spec.qemu,"-cpu",cpu,str(loader)]
                call([],universal,need="technology="+expected,execute=True,command=command)
                call(["--technology",rejected],universal,ok=False,need="cannot run",command=command)
                call(["--technology",rejected,"--fallback"],universal,need="technology="+expected,execute=True,command=command)
                abi_preflight["status"]="supported"
            elif (result.returncode in (-signal.SIGILL,-signal.SIGSEGV,-signal.SIGABRT,
                                        -signal.SIGFPE,-signal.SIGBUS)
                  or "Fatal glibc error: CPU does not support" in output):
                abi_preflight["status"]="unavailable-libc-cpu-baseline"
                print(f"{arch}: CPU-model check unavailable: independent libc probe fails on {cpu} "
                      f"(exit={result.returncode}); negative dispatch checked with --limit-isa scalar",flush=True)
            else:
                raise AssertionError(f"ABI probe could not be verified on {cpu}; exit={result.returncode}\n{output}")

    record=dict(architecture=arch,backend="QEMU" if emulated else "native",
                cpu=args.cpu or (spec.cpu if emulated else "host"),checks=checks,
                executed=sorted(executed),negative_test_mode=negative_mode,
                abi_preflight=abi_preflight,loader=loader.name)
    report=ROOT/"build/results";report.mkdir(exist_ok=True)
    report_name=arch+("-qemu" if emulated else "-native")+("-custom" if args.loader else "")
    (report/(report_name+".json")).write_text(json.dumps(record,indent=2)+"\n")
    print(f"{arch}: {checks} dispatch/format checks PASS; CODE executed={','.join(sorted(executed))}; backend={record['backend']}; negative={negative_mode}")

if __name__=="__main__": main()
