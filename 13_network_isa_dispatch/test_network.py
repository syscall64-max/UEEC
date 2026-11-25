#!/usr/bin/env python3
"""Real local TLS, target-generated CPU profiles, reconstruction and execution."""
import argparse
from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import queue
import re
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import threading

from architectures import ARCHES, native_arch
from generate import ROOT, build

NET=ROOT/"build/ueec-net"
REPORTS=ROOT/"build/network-results"

def command(args,ok=True,need=None):
    r=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=40)
    text=r.stdout+r.stderr
    if r.returncode<0 or (r.returncode==0)!=ok or (need and need not in text):
        raise AssertionError(f"{args}\nexit={r.returncode}\n{text}")
    if "ERROR: AddressSanitizer" in text or "runtime error:" in text:
        raise AssertionError(text)
    return text

def certificate(folder,label="localhost"):
    cert,key=folder/(label+".pem"),folder/(label+".key")
    names="DNS:localhost,IP:127.0.0.1" if label=="localhost" else "DNS:wrong.example"
    command(["openssl","req","-x509","-newkey","rsa:2048","-nodes","-days","1",
             "-subj","/CN="+label,"-addext","subjectAltName="+names,"-keyout",key,"-out",cert])
    return cert,key

@contextmanager
def server(source,cert,key,label):
    p=subprocess.Popen([str(NET),"server","--source",str(source),"--cert",str(cert),
                        "--key",str(key),"--port","0"],stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT,text=True,bufsize=1)
    lines=[];ready=queue.Queue()
    def reader():
        for line in p.stdout:
            lines.append(line)
            if "TLS server " in line: ready.put(line)
        ready.put(None)
    thread=threading.Thread(target=reader,daemon=True);thread.start()
    try:
        first=ready.get(timeout=15)
        if first is None: raise AssertionError("server startup failed:\n"+"".join(lines))
        port=int(re.search(r"port=(\d+)",first).group(1))
        yield port,lines
        if p.poll() is not None: raise AssertionError("server exited unexpectedly:\n"+"".join(lines))
    finally:
        if p.poll() is None:
            p.terminate()
            try: p.wait(timeout=10)
            except subprocess.TimeoutExpired: p.kill();p.wait()
        thread.join(timeout=5)
        REPORTS.mkdir(exist_ok=True)
        (REPORTS/(label+"-server.log")).write_text("".join(lines))

def read_request(data):
    assert len(data)==64 and data[:4]==b"UEC2"
    return dict(machine=data[4],system=data[5],endian=data[6],fallback=bool(data[7]),
                usable=struct.unpack_from(">Q",data,8)[0],technology=data[16:48].split(b"\0")[0].decode())

def inspect_one(path,spec,variant,granule):
    """Independent byte-level check; no project parser is used here."""
    b=path.read_bytes();assert b[:4]==b"_GM\1"
    assert struct.unpack_from("<HH",b,4)==(1,1)
    word,fmt=struct.unpack_from("<IH",b,8)
    assert fmt==0 and word>>27==2 and (word>>10)&255==spec.machine
    assert (word>>20)&127==2 and word&1==int(spec.machine in (3,4,7,8,11))
    assert struct.unpack_from("<HIHI",b,14)==(0,1,0,2)
    desc=[struct.unpack_from("<HHH",b,26+6*i) for i in range(3)]
    assert [d[0] for d in desc]==[0,1,13]
    assert sum(d[1] for d in desc)==len(b)
    assert all(d[1]%granule==0 and d[2]<=d[1] for d in desc)
    at=desc[0][1]
    code=b[at:at+desc[1][2]]
    assert code==(ROOT/"payloads"/next(k for k,v in ARCHES.items() if v is spec)/(variant.name+".bin")).read_bytes()
    at+=desc[1][1];md=b[at:at+desc[2][2]]
    assert len(md)==34 and struct.unpack_from("<HHH",md)==(0,1,2)
    assert struct.unpack_from("<HHH",md,6)==(0,spec.metadata_type,0)
    assert struct.unpack_from("<HHH",md,12)==(0,2,8)
    assert struct.unpack_from("<HHH",md,18)==(0,0,10)
    assert struct.unpack_from("<QH",md,24)==(variant.required,variant.priority)
    return len(b)

def receive(port,cert,request,path,mode="compact",ok=True,need=None,extra=()):
    text=command([NET,"client","--host","localhost","--port",port,"--ca",cert,
                  "--request-file",request,"--output",path,"--mode",mode,*extra],ok=ok,need=need)
    if not ok: assert not path.exists()
    return text

def run_arch(arch,emulated,work,port,cert):
    spec=ARCHES[arch]
    prefix=[spec.qemu,"-cpu",spec.cpu] if emulated else []
    requester=ROOT/"build"/("ueec-request-"+arch)
    loader=ROOT/"build"/("ueec-loader-"+arch)
    folder=work/arch;folder.mkdir();checks=0;serial=0;executed=set()
    def profile(label,*options):
        dest=folder/(label+".request")
        command(prefix+[requester,"--output",dest,*options],need="REQUEST ")
        return dest,read_request(dest.read_bytes())
    def transfer(label,req,variant,mode="compact",execute=True):
        nonlocal checks,serial
        serial+=1;dest=folder/(f"{serial}-{label}.ueec")
        text=receive(port,cert,req,dest,mode,need="nodes=1")
        assert "technology="+variant.name in text and "output_node=0" in text
        inspect_one(dest,spec,variant,4096 if mode=="granulated" else 1);checks+=1
        if execute:
            command(prefix+[loader,"--technology",variant.name,dest],need="PASS: 256 alignment/length cases")
            executed.add(variant.name);checks+=1
        return dest
    req,profile_data=profile("auto")
    available=[v for v in spec.variants if not(v.required&~profile_data['usable'])]
    assert available
    best=max(available,key=lambda v:v.priority)
    for mode in ("compact","granulated"): transfer("auto-"+mode,req,best,mode)
    scalar_req,_=profile("scalar","--limit-isa","scalar")
    transfer("scalar",scalar_req,spec.variants[0])
    for variant in available:
        requested,_=profile("explicit-"+variant.name,"--technology",variant.name)
        transfer(variant.name,requested,variant)
    unavailable=spec.variants[-1].name
    strict,_=profile("strict","--limit-isa","scalar","--technology",unavailable)
    receive(port,cert,strict,folder/"strict.ueec",ok=False,need="technology-unavailable");checks+=1
    fallback,_=profile("fallback","--limit-isa","scalar","--technology",unavailable,"--fallback")
    transfer("fallback",fallback,spec.variants[0])
    absent,_=profile("absent","--technology","absent_technology")
    receive(port,cert,absent,folder/"absent.ueec",ok=False,need="technology-unavailable");checks+=1
    absent_fallback,_=profile("absent-fallback","--technology","absent_technology","--fallback")
    transfer("absent-fallback",absent_fallback,best)
    for name,index,value,reason in (("wrong-os",5,1,"no-compatible-node"),
                                     ("wrong-endian",6,profile_data['endian']^1,"no-compatible-node"),
                                     ("invalid-machine",4,255,"invalid request file"),
                                     ("reserved",63,1,"invalid request file")):
        raw=bytearray(req.read_bytes());raw[index]=value
        bad=folder/(name+".request");bad.write_bytes(raw)
        receive(port,cert,bad,folder/(name+".ueec"),ok=False,need=reason);checks+=1
    # A valid transfer must never overwrite an existing local file.
    existing=folder/"keep.ueec";existing.write_bytes(b"keep-original")
    command([NET,"client","--port",port,"--ca",cert,"--request-file",req,"--output",existing],
            ok=False,need="will not overwrite")
    assert existing.read_bytes()==b"keep-original";checks+=1
    record=dict(architecture=arch,profile_backend="QEMU" if emulated else "native",transport="native OpenSSL TLS",
                checks=checks,executed=sorted(executed),usable=hex(profile_data['usable']))
    REPORTS.mkdir(exist_ok=True);(REPORTS/(arch+("-qemu" if emulated else "-native")+".json")).write_text(json.dumps(record,indent=2)+"\n")
    print(f"{arch}: {checks} TLS selection/reconstruction/execution checks PASS; "
          f"CODE executed={','.join(sorted(executed))}; profile={record['profile_backend']}; transport=native-TLS",flush=True)
    return req,best,prefix,loader

@contextmanager
def fake_server(cert,key,payload,mutate=None):
    """A trusted TLS peer returning a deliberately invalid application reply."""
    listener=socket.create_server(("127.0.0.1",0));port=listener.getsockname()[1]
    errors=[]
    def worker():
        try:
            ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(cert,key)
            listener.settimeout(15)
            raw,_=listener.accept()
            with raw,ctx.wrap_socket(raw,server_side=True) as stream:
                data=b""
                while len(data)<64:
                    chunk=stream.recv(64-len(data))
                    if not chunk: raise AssertionError("missing request")
                    data+=chunk
                head=bytearray(64);head[:4]=b"UEC2";struct.pack_into(">I",head,8,len(payload))
                # Read the canonical one-node metadata without project code.
                hs=struct.unpack_from("<H",payload,28)[0]
                cs=struct.unpack_from("<H",payload,34)[0]
                required,priority=struct.unpack_from("<QH",payload,hs+cs+24)
                struct.pack_into(">Q",head,16,required);struct.pack_into(">H",head,24,priority)
                head[32:]=hashlib.sha256(payload).digest()
                body=payload
                if mutate: head,body=mutate(head,body)
                stream.sendall(bytes(head)+body)
        except Exception as exc: errors.append(exc)
        finally: listener.close()
    thread=threading.Thread(target=worker,daemon=True);thread.start()
    try: yield port
    finally:
        thread.join(timeout=20)
        assert not thread.is_alive()
        if errors: raise errors[0]

def global_cases(work,cert,key,req,best,prefix,loader,arch):
    checks=0;spec=ARCHES[arch]
    # Server must normalize source endianness/MD width/design and reindex node 0.
    for endian in ("little","big"):
        for width in (2,4,8):
            for layout in ("shared","per-node"):
                label=f"{endian}-{width}-{layout}"
                source=work/(label+".ueec");source.write_bytes(build(endian=endian,width=width,metadata_layout=layout,reverse=True))
                with server(source,cert,key,label) as (port,_):
                    dest=work/(label+"-received.ueec")
                    receive(port,cert,req,dest,need="nodes=1")
                    inspect_one(dest,spec,best,1)
                    command(prefix+[loader,dest],need="PASS: 256 alignment/length cases");checks+=1
    # A source node index above 255 is reindexed to zero on the wire and in MD.
    from dataclasses import replace
    scalar=spec.variants[0];high=replace(scalar,priority=901)
    try:
        ARCHES[arch]=replace(spec,variants=(high,)+(scalar,)*256)
        large=build(architectures=[arch])
    finally:
        ARCHES[arch]=spec
    large_source=work/"node-256-source.ueec";large_source.write_bytes(large)
    with server(large_source,cert,key,"node-256") as (port,_):
        dest=work/"node-256-received.ueec"
        receive(port,cert,req,dest,need="source_node=256 output_node=0")
        inspect_one(dest,spec,high,1)
        command(prefix+[loader,dest],need="technology=scalar");checks+=1
    with server(ROOT/"build/sum.ueec",cert,key,"security") as (port,_):
        valid=work/"valid.ueec";receive(port,cert,req,valid)
        ctx=ssl.create_default_context(cafile=str(cert))
        with socket.create_connection(("127.0.0.1",port),timeout=10) as raw:
            with ctx.wrap_socket(raw,server_hostname="localhost") as stream:
                malformed=bytearray(req.read_bytes());malformed[7]=128;stream.sendall(malformed)
                answer=b""
                while len(answer)<64:
                    part=stream.recv(64-len(answer))
                    if not part: break
                    answer+=part
                assert len(answer)==64 and struct.unpack_from(">I",answer,4)[0]==1;checks+=1
        wrong_ca,_=certificate(work,"untrusted")
        receive(port,wrong_ca,req,work/"untrusted.ueec",ok=False,need="certificate/hostname");checks+=1
    # Wrong certificate identity is rejected even when its issuer is trusted.
    wrong_cert,wrong_key=certificate(work,"wrong-host")
    with server(ROOT/"build/sum.ueec",wrong_cert,wrong_key,"wrong-host") as (port,_):
        receive(port,wrong_cert,req,work/"wrong-host.ueec",ok=False,need="certificate/hostname");checks+=1
    payload=valid.read_bytes()
    def bad_hash(h,b): h[32]^=1;return h,b
    def bad_priority(h,b): h[25]^=1;return h,b
    def truncated(h,b): return h,b[:-1]
    def invalid_node(h,b):
        b=bytearray(b);md=struct.unpack_from("<H",b,28)[0]+struct.unpack_from("<H",b,34)[0]
        struct.pack_into("<H",b,md+6,1);h[32:]=hashlib.sha256(b).digest();return h,bytes(b)
    def invalid_isa(h,b):
        b=bytearray(b);md=struct.unpack_from("<H",b,28)[0]+struct.unpack_from("<H",b,34)[0]
        struct.pack_into("<Q",b,md+24,1<<63);struct.pack_into(">Q",h,16,1<<63)
        h[32:]=hashlib.sha256(b).digest();return h,bytes(b)
    def wrong_os(h,b):
        b=bytearray(b);word=struct.unpack_from("<I",b,8)[0]
        struct.pack_into("<I",b,8,(word&~(127<<20))|(1<<20))
        h[32:]=hashlib.sha256(b).digest();return h,bytes(b)
    def wrong_endian(h,b):
        b=bytearray(b);b[8]^=1;h[32:]=hashlib.sha256(b).digest();return h,bytes(b)
    def oversized(h,b): struct.pack_into(">I",h,8,64*1024*1024+1);return h,b""
    def reserved(h,b): h[26]=1;return h,b""
    for name,mutate,reason in (("hash",bad_hash,"SHA-256 mismatch"),("priority",bad_priority,"does not match"),
                               ("truncated",truncated,"incomplete TLS payload"),
                               ("node-index",invalid_node,"does not match"),("isa",invalid_isa,"does not match"),
                               ("wrong-os",wrong_os,"does not match"),("wrong-endian",wrong_endian,"does not match"),
                               ("oversized",oversized,"invalid response framing"),("reserved",reserved,"invalid response framing")):
        with fake_server(cert,key,payload,mutate) as port:
            receive(port,cert,req,work/("bad-"+name+".ueec"),ok=False,need=reason);checks+=1
    # Also exercise the actual native client's built-in CPU/OS detection path.
    if arch==native_arch() and not prefix:
        with server(ROOT/"build/sum.ueec",cert,key,"auto-client") as (port,_):
            output=work/"auto-client.ueec"
            command([NET,"client","--port",port,"--ca",cert,"--output",output],need="nodes=1")
            inspect_one(output,spec,best,1);checks+=1
    (REPORTS/"global-cases.json").write_text(json.dumps(dict(checks=checks,source_formats=12,
        source_node_256_reindexed=True,certificate_identity=True,hostile_responses=9),indent=2)+"\n")
    print(f"TLS global: {checks} source-format, identity, protocol and hostile-response checks PASS",flush=True)

def synthetic_transfers(work,port,cert):
    """Exercise all server families without claiming target detection/execution."""
    records=[]
    for arch,spec in ARCHES.items():
        for variant in spec.variants:
            profile=bytearray(64);profile[:4]=b"UEC2";profile[4]=spec.machine;profile[5]=2
            profile[6]=int(spec.machine in (3,4,7,8,11));struct.pack_into(">Q",profile,8,variant.required)
            profile[16:16+len(variant.name)]=variant.name.encode()
            req=work/(arch+"-"+variant.name+".request");req.write_bytes(profile)
            for mode in ("compact","granulated"):
                output=work/(arch+"-"+variant.name+"-"+mode+".ueec")
                receive(port,cert,req,output,mode,need="nodes=1")
                size=inspect_one(output,spec,variant,4096 if mode=="granulated" else 1)
                records.append(dict(architecture=arch,technology=variant.name,mode=mode,output_bytes=size))
    (REPORTS/"synthetic-transfers.json").write_text(json.dumps(records,indent=2)+"\n")
    print(f"TLS transfer-only: {len(records)} checks PASS; 12 architectures, 29 CODE variants x 2 modes; "
          "profiles=synthetic; target execution=not-run",flush=True)

def main():
    global REPORTS,NET
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--arch",choices=tuple(ARCHES),default=native_arch())
    p.add_argument("--all",action="store_true")
    p.add_argument("--qemu",action="store_true")
    p.add_argument("--synthetic",action="store_true",help="transfer all 29 variants using synthetic profiles; no target execution")
    p.add_argument("--net",type=Path,help="override the native TLS binary, e.g. a sanitizer build")
    args=p.parse_args()
    if args.net: NET=args.net.resolve()
    REPORTS=REPORTS/(("synthetic" if args.synthetic else "all" if args.all else args.arch)+
                    ("-qemu" if args.qemu else "-native")+("-custom" if args.net else ""))
    REPORTS.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ueec-tls-",dir=ROOT/"build") as directory:
        work=Path(directory);cert,key=certificate(work)
        with server(ROOT/"build/sum.ueec",cert,key,"main") as (port,_):
            if args.synthetic:
                synthetic_transfers(work,port,cert)
                return
            first=None
            for arch in ARCHES if args.all else (args.arch,):
                emulated=args.qemu or arch!=native_arch()
                result=run_arch(arch,emulated,work,port,cert)
                if first is None: first=(arch,result)
        arch,(req,best,prefix,loader)=first
        global_cases(work,cert,key,req,best,prefix,loader,arch)
    print("UEEC network ISA selection PASS (no skipped targets)")

if __name__=="__main__": main()
