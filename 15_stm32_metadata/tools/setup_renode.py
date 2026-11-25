#!/usr/bin/env python3
"""Fetch one pinned official portable runtime; no sudo or global installation."""
import hashlib
from pathlib import Path
import tarfile
import tempfile
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
VERSION='1.17.0'
SHA256='4ba7c68b59e2447f188ef4b4b112fcccd2802582c460beedceb63b84e5605a5f'
URL=f'https://github.com/renode/renode/releases/download/v{VERSION}/renode-{VERSION}.linux-portable.tar.gz'

def install():
    dest=ROOT/'build/deps';dest.mkdir(parents=True,exist_ok=True)
    executable=dest/'renode_1.17.0-portable/renode'
    if executable.is_file(): return executable
    print('Downloading the pinned Renode 1.17.0 Linux x86-64 portable runtime (about 61 MiB)...',flush=True)
    with tempfile.TemporaryDirectory(dir=dest) as temporary:
        archive=Path(temporary)/'renode.tar.gz'
        request=urllib.request.Request(URL,headers={'User-Agent':'UEEC-STM32-demo/1'})
        with urllib.request.urlopen(request,timeout=60) as source,archive.open('wb') as output:
            while chunk:=source.read(1024*1024): output.write(chunk)
        if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256:
            raise RuntimeError('Renode archive SHA-256 mismatch; not extracting')
        with tarfile.open(archive) as source: source.extractall(dest,filter='data')
    print('Renode archive checksum verified.',flush=True)
    return executable

if __name__=='__main__': print(install())
